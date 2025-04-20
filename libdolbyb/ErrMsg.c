/*
 * ErrMsg.c - a library of routines to display an error message.
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

#include "ErrMsg.h"

#include <stdio.h>
#include <string.h>

/* Modify to allow messages to be split between 2 or more lines */

#define ErrMsgMaxLines  2
#define ErrMsgMaxLinLen  60

#define ErrMsgMaxMsgLen  (ErrMsgMaxLinLen - 12)

#define ErrMsgMaxWrn    9000000000000000000.0


int ErrMsgFlg = 0;
static int64_t ErrMsgWCt = 0;


/* A routine from FreePascal.
 * Store in "ret" the substring of length "len" starting from "pos" (1-based).
 * Store a shorter or null string if out-of-range.  Return "ret".
 */
static char *strsub(char *ret, char *s, int pos, int len)
{
    char *s2;

    if (--pos < 0 || len <= 0) {
        *ret = 0;
        return ret;
    }
    while (pos > 0) {
        if (!*s++) {
            *ret = 0;
            return ret;
        }
        pos--;
    }
    s2 = ret;
    while (--len >= 0) {
        if (!(*s2++ = *s++))
            return ret;
    }
    *s2 = 0;
    return ret;
}


static uint32_t ErrMgsLastNextLength(char *ChkStr)
{
  /* Return the longest length of ChkStr allowed */
  /* up to just before the last space character */
  uint32_t StrLen, OutLen;

  StrLen = strlen(ChkStr);
  if (StrLen <= ErrMsgMaxMsgLen) {
    OutLen = StrLen;
    return OutLen;
  }
  OutLen = ErrMsgMaxMsgLen;
  while (ChkStr[OutLen-1] != ' ')
    OutLen--;
  while (ChkStr[OutLen-1] == ' ')
    OutLen--;
  return OutLen;
}


static void ErrMsgDisplayStarLine(uint16_t LenVal)
{
  /* Display a line of star characters, to help highlight a message */
  uint16_t LenCnt = 0;

  while (LenCnt < LenVal) {
    LenCnt++;
    putchar('*');
  }
  putchar('\n');
}



static void ErrMsgSendStr(char *OutStr)
{
  uint32_t StrCnt, TotLen;
  uint32_t NxtPos = 1;
  uint32_t NxtLen;
  uint32_t MaxLen = 0;
  uint32_t StrPos[ErrMsgMaxLines], StrLen[ErrMsgMaxLines];
  char STR1[256];

  /* Work out start and length of strings */
  TotLen = strlen(OutStr);
  for (StrCnt = 0; StrCnt < ErrMsgMaxLines; StrCnt++) {
    if (NxtPos > TotLen) {
      StrPos[StrCnt] = 0;
      StrLen[StrCnt] = 0;
    } else {
      NxtLen = TotLen - NxtPos + 1;
      NxtLen = ErrMgsLastNextLength(strsub(STR1, OutStr, NxtPos, NxtLen));
      if (NxtLen > MaxLen)
	MaxLen = NxtLen;
      StrPos[StrCnt] = NxtPos;
      StrLen[StrCnt] = NxtLen;
      NxtPos += NxtLen + 1;
    }
  }
  /* Now send strings */
  ErrMsgDisplayStarLine(MaxLen + 12);
  for (StrCnt = 0; StrCnt < ErrMsgMaxLines; StrCnt++) {
    if (StrLen[StrCnt] > 0)
      fputs(strsub(STR1, OutStr, StrPos[StrCnt], StrLen[StrCnt]), stdout);
  }
  ErrMsgDisplayStarLine(MaxLen + 12);
}


void ErrMsgFlagError(char *ErrMsg)
{
  char STR1[256];

  if (ErrMsgFlg)
    return;
  ErrMsgFlg = 1;
  putchar('\n');
  sprintf(STR1, "Error. %s.", ErrMsg);
  ErrMsgSendStr(STR1);
}


void ErrMsgWarning(char *WrnMsg)
{
  char STR1[256];

  putchar('\n');
  sprintf(STR1, "Warning. %s.", WrnMsg);
  ErrMsgSendStr(STR1);
  if (ErrMsgWCt < ErrMsgMaxWrn)
    ErrMsgWCt++;
}


char *ErrMsgNumToStr(char *Result, int64_t InVal)
{
  int64_t ValLft, BytVal;
  char OutStr[256];

  *OutStr = '\0';
  ValLft = InVal;
  while (ValLft > 0) {
    BytVal = ValLft % 10;
    ValLft /= 10;
    sprintf(OutStr + strlen(OutStr), "%c", (char)(BytVal + 48));
  }
  return strcpy(Result, OutStr);
}
