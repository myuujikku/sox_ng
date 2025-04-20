/*
 * libdolbyb.c - API to libdolbyb
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
#include "Param.h"
#include "Mixers.h"
#include "Calibrate.h"
#include "SidePath.h"
#include "FindOutSmp.h"

#include <stdlib.h>  /* for free() */
#include <string.h>  /* for memset() */
#include <stdio.h>   /* we shouldn't but it's only for error messages */

/* Set default values */
void dolbyb_init(dolbyb_t *Param)
{
  memset((void *)Param, 0, sizeof(*Param));

  /* Set default values */
  Param->NumChn = 1;
  Param->BDepth = 16;
  Param->FltTyp = 4;
  Param->DecAdB = -5.0;
  Param->ThGndB = 0.0;

  /* Set initial values */
  Param->FETGVt = 7500000000;
}

static int SecondInit(dolbyb_t *Param)
{
  /* More initalising, after the input wave header has been read,  */
  /* so the sample rate and number of samples is now known.        */

  Param->DecAMX = ParamConvertDb(Param->DecAdB);
  Param->ThGain = ParamConvertDb(Param->ThGndB);

  /* Set up sample rates */
  if (Param->UpSamp == 0) {
    while (Param->UpSamp * Param->SmpSec < 200000L)
      Param->UpSamp++;
  }
  if (Param->AllHig)
    Param->InUS = Param->UpSamp;
  else
    Param->InUS = 1;

  Param->CFrq = Param->SmpSec;

  /* Initialise side path routines */
  if (SidePathInit(Param)) return 1;
  FindOutSmpInit(Param);
  return 0;
}

/* Do initialisation that depends on parameter changes */
int dolbyb_start(dolbyb_t *Param)
{
  /* Check validity of parameters */

  if (Param->SmpSec <= 0) {
    fprintf(stderr, "Did you forget to set dolbyb.SmpSec before calling dolbyb_start()?\n");
    return 1;
  }

  if (Param->NumChn < 1 || Param->NumChn > 2) {
    fprintf(stderr, "libdolbyb can only process mono and stereo audio\n");
    return 1;
  }

  /* Check filter type is valid */
  if (Param->FltTyp < 1 || Param->FltTyp > 4) {
    fprintf(stderr, "dolbyb.FltTyp must be from 1 to 4.\n");
    return 1;
  }

  switch (Param->FltTyp) {
  case 1: Param->FETSVt = ParamFETSVt1; Param->SidAmp = ParamSidAmp1; break;
  case 2: Param->FETSVt = ParamFETSVt2; Param->SidAmp = ParamSidAmp2; break;
  case 3: Param->FETSVt = ParamFETSVt3; Param->SidAmp = ParamSidAmp3; break;
  case 4: Param->FETSVt = ParamFETSVt4; Param->SidAmp = ParamSidAmp4; break;
  }

  /* Set multiplier for wave input and divider for wave output */
  switch (Param->BDepth) {
  case 8:  Param->SmpMux = ParamSMux08; break;
  case 16: Param->SmpMux = ParamSMux16; break;
  case 24: Param->SmpMux = ParamSMux24; break;
  default:
    fprintf(stderr, "dolbyb.BDepth must be 8, 16 or 24.\n");
    return 1;
  }

  if (SecondInit(Param)) return 1;
  Calibrate(Param);
  if (SecondInit(Param)) return 1;
  return 0;
}

void dolbyb_encode(dolbyb_t *Param, void *in, void *out, size_t nframes)
{
  unsigned char *inp = in;
  unsigned char *outp = out;
  uint16_t Chn, UpCnt;
  int64_t SidSmp, TotSmp, OutSmp;
  int64_t SmpCnt;
  int64_t MaxSamp, SubSamp;
  uint16_t NumByt, BytCnt;
  int32_t MaxVal, MinVal, AddVal;
  int32_t PrvVal, NxtVal, BytVal;

  switch (Param->BDepth) {
  case 8:  NumByt = 1; MaxSamp = -1; SubSamp = 128; 
           MaxVal = 127; MinVal = -128; AddVal = 128;
           break;
  case 16: NumByt = 2; MaxSamp = 32767; SubSamp = 65536L;
           MaxVal = 32767; MinVal = -32768L; AddVal = 65536L;
           break;
  case 24: NumByt = 3; MaxSamp = 8388607L; SubSamp = 16777216L;
           MaxVal = 8388607L; MinVal = -8388608L; AddVal = 16777216L;
           break;
  default:
           fprintf(stderr, "BDepth is not 8/16/24. Did you set it before calling dolbyb_start()?\n");
	   /* Shut the compiler warnings up */
           NumByt = MaxSamp = SubSamp = 0;
           MaxVal = MinVal = AddVal = 0;
	   exit(1);
  }

  for (SmpCnt = 0; SmpCnt < nframes; SmpCnt++) {
    for (Chn = 1; Chn <= Param->NumChn; Chn++) {
      int i; int Mux; uint32_t SmpVal;

      /* Get input */
      for (i=1, Mux = 1, SmpVal = 0; i <= NumByt; i++) {
	SmpVal += Mux * *inp++;
	Mux *= 256;
      }
      if (SmpVal > MaxSamp) SmpVal -= SubSamp;
      SmpVal *= Param->SmpMux;

      /* If upsampling at the start then the same sample */
      /* is processed several times */
      TotSmp = 0;
      for (UpCnt = 1; UpCnt <= Param->InUS; UpCnt++) {
	/* Add audio from side path */
	SidSmp = SidePath(Param, SmpVal, Chn);
	SmpVal = MixersEncode(SmpVal, SidSmp);
	TotSmp += SmpVal;
      }
      OutSmp = (TotSmp / Param->InUS) / Param->SmpMux;

      /* Check for sample value out of range */
      if (OutSmp > MaxVal)
	PrvVal = MaxVal;
      else if (OutSmp < MinVal)
	PrvVal = MinVal;
      else
	PrvVal = OutSmp;
      /* Convert negative values */
      if (PrvVal < 0 || Param->BDepth == 8)
	PrvVal += AddVal;

      /* Send as output bytes */
      for (BytCnt = 1; BytCnt <= NumByt; BytCnt++) {
	NxtVal = PrvVal / 256;
	BytVal = PrvVal - NxtVal * 256;
	PrvVal = NxtVal;
	*outp++ = BytVal;
      }
    }
  }
}

void dolbyb_decode(dolbyb_t *Param, void *in, void *out, size_t nframes)
{
  unsigned char *inp = in;
  unsigned char *outp = out;
  uint16_t NumChn, UpCnt, NumByt;
  int64_t TotSmp;
  int64_t MaxSamp, SubSamp, MinVal, MaxVal, AddVal;
  int64_t SmpCnt;
  int32_t PrvVal, NxtVal, BytVal;

  switch (Param->BDepth) {
  case 8:  NumByt = 1; MaxSamp = -1; SubSamp = 128; 
           MaxVal = 127; MinVal = -128; AddVal = 128;
           break;
  case 16: NumByt = 2; MaxSamp = 32767; SubSamp = 65536L;
           MaxVal = 32767; MinVal = -32768L; AddVal = 65536L;
           break;
  case 24: NumByt = 3; MaxSamp = 8388607L; SubSamp = 16777216L;
           MaxVal = 8388607L; MinVal = -8388608L; AddVal = 16777216L;
           break;
  default:
           fprintf(stderr, "BDepth is not 8/16/24. Did you set it before calling dolbyb_start()?\n");
	   /* Shut the compiler warnings up */
           NumByt = MaxSamp = SubSamp = 0;
           MaxVal = MinVal = AddVal = 0;
	   exit(1);
  }

  /* Process samples */
  for (SmpCnt = 0; SmpCnt < nframes; SmpCnt++) {
    for (NumChn = 1; NumChn <= Param->NumChn; NumChn++) {
      int i, Mux, BytCnt; int64_t SmpVal, OutSmp;

      /* Get input */
      for (i=1, Mux = 1, SmpVal = 0; i <= NumByt; i++) {
	SmpVal += Mux * *inp++;
	Mux *= 256;
      }
      if (SmpVal > MaxSamp) SmpVal -= SubSamp;
      SmpVal *= Param->SmpMux;

      /* If upsampling at the start then the same sample */
      /* is processed several times */
      TotSmp = 0;
      for (UpCnt = 1; UpCnt <= Param->InUS; UpCnt++) {
	/* Set and process output */
	(void) FindOutSmp(Param, SmpVal, NumChn);
	TotSmp += MixersDecode(SmpVal, SidePathUpdate(Param, NumChn));
      }
      OutSmp = (TotSmp / Param->InUS) / Param->SmpMux;

      /* Check for sample value out of range */
      if (OutSmp > MaxVal)
	PrvVal = MaxVal;
      else if (OutSmp < MinVal)
	PrvVal = MinVal;
      else
	PrvVal = OutSmp;
      /* Convert negative values */
      if (PrvVal < 0 || Param->BDepth == 8)
	PrvVal += AddVal;

      /* Send as output bytes */
      for (BytCnt = 1; BytCnt <= NumByt; BytCnt++) {
	NxtVal = PrvVal / 256;
	BytVal = PrvVal - NxtVal * 256;
	PrvVal = NxtVal;
	*outp++ = BytVal;
      }
    }
  }
}

void dolbyb_free(dolbyb_t *Param)
{
  if (Param->HPF2SetValsPotTab) {
    free(Param->HPF2SetValsPotTab); Param->HPF2SetValsPotTab = NULL;
  }
  if (Param->HPF2SetValsAlpTab) {
    free(Param->HPF2SetValsAlpTab); Param->HPF2SetValsAlpTab = NULL;
  }
  if (Param->HPF2SetValsFLTATb) {
    free(Param->HPF2SetValsFLTATb); Param->HPF2SetValsFLTATb = NULL;
  }
  if (Param->SetGateAttTab) {
    free(Param->SetGateAttTab); Param->SetGateAttTab = NULL;
  }
  if (Param->SetGateAlpTab1) {
    free(Param->SetGateAlpTab1); Param->SetGateAlpTab1 = NULL;
  }
  if (Param->SetGateAlpTab2) {
    free(Param->SetGateAlpTab2); Param->SetGateAlpTab2 = NULL;
  }
  if (Param->CalibrateSinTab) {
    free(Param->CalibrateSinTab); Param->CalibrateSinTab = NULL;
  }
}
