/*
 * Calibrate.c - a library of routines to automatically
 * figure out the calibration values for the program.
 * Some initialization will be needed by the main program.
 *
 * Copyright (C) 2025 Martin Guy <martinwguy@gmail.com>
 * based on dolbybcsoftwaredecode by Richard Evans 2018.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation. See COPYING for details.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
 * Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "dolbyb.h"

#include "Calibrate.h"
#include "Mixers.h"
#include "Param.h"
#include "SidePath.h"

#include <stdlib.h>	/* for malloc() etc */

#define CalibrateTestAmp  17.5   /* 17.5 mv * root 2 */
#define CalibrateTstFrq  5000
#define CalibrateNumFltTyp  4

/* We want to make a table of sin values to be able to look them up
 * instead of calculating them all, but one cycle isn't enough because
 * at 44100 samples per second, one cycle of a 5kHz sine wave takes
 * 44100/5000 samples, which is 8.82. If we had 100 cycles, it would
 * fit exactly (into 882 samples). 50 cycles would be 441 samples and
 * that's the minimum because they have no common divisor.
 * The highest common divisor of 44100 and 5000 is 100 so
 * the number of cycles is freq/hcd and the number of samples is
 * SmpFrq/hcd.
 * In the worst case, when the sample rate is 47999 and there is
 * no common divisor, it still only uses 2.5MB of memory.
 */
static int64_t hcd(int64_t a, int64_t b)
{
   while (a != b)
     if (a > b) a = a - b;
     else b = b - a;
   return a;
}

static int CalibrateMakeSinTab(dolbyb_t *Param)
{
  double dt = 2.0 * M_PI * CalibrateTstFrq / Param->CFrq;
  double SinArg = 0.0;
  int64_t SmpCnt;

  Param->CalibrateSinTabMax = Param->CFrq / hcd(Param->CFrq, CalibrateTstFrq);
  Param->CalibrateSinTab = malloc(Param->CalibrateSinTabMax * sizeof(*Param->CalibrateSinTab));
  if (!Param->CalibrateSinTab) return 1;
  for (SmpCnt = 0; SmpCnt < Param->CalibrateSinTabMax; SmpCnt++) {
    Param->CalibrateSinTab[SmpCnt] = round(sin(SinArg) * Param->CalibrateSmpMux);
    SinArg += dt;
  }
  return 0;
}

static int CalibrateInit(dolbyb_t *Param, int64_t WarmUp, int64_t TstLen)
{
  Param->CalibrateSmpDiv = ((double)Param->CFrq / CalibrateTstFrq) / (2 * M_PI);
  Param->CalibrateSmpMux = CalibrateTestAmp * sqrt(2.0) * ParamVltMux / 1000.0;
  Param->CalibrateWrmSam = WarmUp * Param->CFrq;
  Param->CalibrateEndSam = (TstLen + WarmUp) * Param->CFrq;
  return CalibrateMakeSinTab(Param);
}

/****  Routines to run calibration tests  ****/

static int64_t CalibrateSmpCnt;

static int64_t CalibrateNextTestToneSamp(dolbyb_t *Param)
{
  return Param->CalibrateSinTab[CalibrateSmpCnt++ % Param->CalibrateSinTabMax];
}

static int64_t CalibrateTestEncode(dolbyb_t *Param, int64_t SmpVal)
{
  return MixersEncode(SmpVal, SidePath(Param, SmpVal, 1));
}

static int64_t CalibrateRunNoNRTest(dolbyb_t *Param)
{
  int64_t SmpVal, SmpTot = 0;

  CalibrateInit(Param, 0, 5);

  CalibrateSmpCnt = 0;
  while (CalibrateSmpCnt < Param->CalibrateEndSam) {
    /* Apply mixer as if encoding */
    SmpVal = MixersEncode(CalibrateNextTestToneSamp(Param), 0);
    SmpTot += SmpVal < 0 ? -SmpVal : SmpVal;
  }
  /* Return the result */
  return SmpTot / Param->CalibrateEndSam;
}

static int64_t CalibrateRunEncodeTest(dolbyb_t *Param)
{
  /* Initialise calibration test */
  int64_t SmpVal, SmpTot = 0;

  /* Warm up */
  CalibrateSmpCnt = 0;
  while (CalibrateSmpCnt < Param->CalibrateWrmSam)
    CalibrateTestEncode(Param, CalibrateNextTestToneSamp(Param));

  while (CalibrateSmpCnt < Param->CalibrateEndSam) {
    SmpVal = CalibrateTestEncode(Param, CalibrateNextTestToneSamp(Param));
    SmpTot += SmpVal < 0 ? -SmpVal : SmpVal;
  }
  /* Return the result */
  return SmpTot / (Param->CalibrateEndSam - Param->CalibrateWrmSam);
}

/****  Routines to find best value for side Amp  ****/

static int64_t CalibrateTrySideAmp(dolbyb_t *Param, double SidAmp)
{
  Param->SidAmp = SidAmp;
  SidePathInit(Param);
  return CalibrateRunEncodeTest(Param);
}

static double CalibrateFindSideAmp(dolbyb_t *Param, int64_t Target)
{
  int64_t OldGVt;
  double HigAmp, LowAmp, PrvAmp, TryAmp;
  int64_t TryRes, PrvRes;
  int64_t HigRes, LowRes;

  OldGVt = Param->FETGVt;   /* Save Value to restore afterwards */
  CalibrateInit(Param, 0, 5);
  /* Set up some test values */
  Param->FETClp = 1;
  Param->FETGVt = 0;

  /* Find High and low values */

  /* Lowest  result is 2.354113326 for filter type 4 at 384000Hz
   * Highest result is 4.225473657 for filter type 1 at 8000Hz
   * Use a few millionths larger in case of future algorithmic changes
   * which so far have always got the same result within a millionth part
   */
  LowAmp = 2.35411;
  HigAmp = 4.22548;
  LowRes = CalibrateTrySideAmp(Param, LowAmp);
  HigRes = CalibrateTrySideAmp(Param, HigAmp);

  /* Invent the loop variables so that the loop starts */
  PrvAmp = LowAmp; PrvRes = LowRes;
  TryAmp = HigAmp; TryRes = HigRes;

  /* Now use successive linear approximations until we find
   * two close values that give the same result
   *
   * The linear approximation was derived thus:
   *                                                     ,,--
   *  HigRes|________________________________________--''____
   *  Target|-------------------|------------,,|-''|---------
   *        |                   |      ,,--''  |   |
   *  LowRes|___________________|,,--''________|___|_________
   *        |            _..--''|              |   |
   *        |      ,,--''       |              |   |
   *        |,,--''             |              |   |
   *        |                   |              |   |
   *        |                   |              |   |
   *        |___________________|______________|___|_________
   *                         LowAmp            X  HigAmp
   *
   * where X is the new estimate of TryAmp whose result is Target.
   *   By similar triangles:
   * (X - LowAmp) / (Target - LowRes) == (HigAmp - LowAmp) / (HigRes - LowRes)
   *   to get X, multiply both sides by (Target - LowRes)
   * X - LowAmp == (HigAmp - LowAmp) / (HigRes - LowRes) * (Target - LowRes)
   *   add LowAmp to both sides and you get
   * X == LowAmp + ((HigAmp - LowAmp) / (HigRes - LowRes)) * (Target - LowRes)
   */
  while (TryRes != PrvRes) {
    PrvAmp = TryAmp; PrvRes = TryRes;
    TryAmp = LowAmp + (HigAmp - LowAmp) * (Target - LowRes) / (HigRes - LowRes);
    TryRes = CalibrateTrySideAmp(Param, TryAmp);
    if (TryRes > Target) {
      HigAmp = TryAmp; HigRes = TryRes;
    } else if (TryRes < Target) {
      LowAmp = TryAmp; LowRes = TryRes;
    }
  }
  /* We have two Amplitudes that give the same result
   * so out best estimate is half way between them */
  TryAmp = (TryAmp + PrvAmp) / 2;

  /* Restore parameters and store the result */
  Param->FETGVt = OldGVt;
  Param->FETClp = 0;
  return TryAmp;
}

/****  Routines to find best value for SVlt  ****/

static int64_t CalibrateTrySVlt(dolbyb_t *Param, int64_t SVlt)
{
  Param->FETSVt = SVlt;
  SidePathInit(Param);
  return CalibrateRunEncodeTest(Param);
}

static int64_t CalibrateFindSVlt(dolbyb_t *Param, int64_t Target)
{
  int64_t HigS, LowS, PrvS, TryS;
  int64_t TryRes, PrvRes;
  int64_t HigRes, LowRes;

  CalibrateInit (Param, 2, 5);

  /* Set up some test values */
  Param->FETClp = 0;

  /* Set initial high and low values */

  /* The Lowest  result is 11365539381 for filter type 2 at 8000Hz
   * The Highest result is 11497842079 for filter type 3 at 384000Hz
   * Use a few millionths larger in case of future algorithmic changes
   * which so far have always got the same result within a millionth part
   */
  /* gcc-2.95 warns "integer constant out of range" and Ansi C disallows
   * long long constants (11365500000LL) so here's a halfway house */
  LowS = (int64_t)113655*100000;
  HigS = (int64_t)114979*100000;
  LowRes = CalibrateTrySVlt(Param, LowS);
  HigRes = CalibrateTrySVlt(Param, HigS);

  /* Invent the loop variables so that the loop starts */
  PrvS = LowS; PrvRes = LowRes;
  TryS = HigS; TryRes = HigRes;

  /* Now use successive linear approximations until we find */
  /* two close values that give the same result */
  while (TryRes != PrvRes) {
    PrvS = TryS; PrvRes = TryRes;
    /* See the diagram above for the derivation of this formula */
    TryS = LowS + (HigS - LowS) * (Target - LowRes) / (HigRes - LowRes);
    if (TryS != PrvS) {
      TryRes = CalibrateTrySVlt(Param, TryS);
      if (TryRes > Target) {
	  HigS = TryS; HigRes = TryRes;
      } else if (TryRes < Target) {
	  LowS = TryS; LowRes = TryRes;
      }
    }
  }
  /* We have two Amplitudes that give the same result
   * so out best estimate is half way between them */
  TryS = (TryS + PrvS + 1) / 2;

  return TryS;
}

/******************************************/
/****  Routines to do the calibration  ****/
/******************************************/

void Calibrate(dolbyb_t *Param)
{
  int64_t OffRes;   /* Result with Noise Resuction off */
  int64_t GanTgt;   /* Target for gain adjustement */
  int64_t SvtTgt;   /* Target for SVlt adjustement */
  
  /* Try with noise reduction turned off, to get reference level */
  OffRes = CalibrateRunNoNRTest(Param);

  /* Calculate other targets */
  GanTgt = round(OffRes * ParamConvertDb(10.0));
  SvtTgt = round(OffRes * ParamConvertDb(8.0));

  /* Store the result */
  Param->SidAmp = CalibrateFindSideAmp(Param, GanTgt);
  Param->FETSVt = CalibrateFindSVlt(Param, SvtTgt);

  SidePathInit(Param);
}
