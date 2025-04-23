/*
 * dolbyb.c - the main program to drive libdolbyb as a stand-alone program.
 *
 * Copyright (C) 2025 Martin Guy <martinwguy@gmail.com>
 * based on dolbybcsoftwaredecode/DolbyBi64.PP by Richard Evans 2018.
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

#include <stdlib.h>
#include <stdio.h>
#include <sndfile.h>
#include <sndfile.h>
#if DEBUG
#include "src/Param.h"
#endif

static void
usage(void)
{
  fprintf(stderr, "Usage: dolbyb [options] in.wav out.wav\n");
  fprintf(stderr, "-d   Decode a Dolby B-encoded recording (this is the default action)\n");
  fprintf(stderr, "-e   Encode instead of decoding\n");
  fprintf(stderr, "-uN  Upsample the low-pass filter N times\n");
  fprintf(stderr, "     (defaults to the right value for 200kHz)\n");
  fprintf(stderr, "-h   Process absolutely everything at the upsampled rate\n");
  fprintf(stderr, "-tN  Set the threshold gain to N (float, default 1.0)\n");
  fprintf(stderr, "-aN  Set the decode accuracy in dB (float, default -5)\n");
  fprintf(stderr, "-fN  Use filter type N (1-4, default 4)\n");
}

#define HANDFUL 1024

int
main(int argc, char *argv[])
{
  dolbyb_t dolbyb;
  SNDFILE *sf_in, *sf_out;
  SF_INFO sfinfo;
  short *buf_in, *buf_out;
  sf_count_t n_read;
  int Encode = 0;

  dolbyb_init(&dolbyb);

  /* Process commandline arguments */
  argv++; argc--; /* Skip program name */
  while (argc > 0 && argv[0][0] == '-') {
    switch (argv[0][1]) {
    case 'd': Encode        = 0; break;
    case 'e': Encode        = 1; break;
    case 'h': dolbyb.AllHig = 1; break;
    case 'f': dolbyb.FltTyp = argv[0][1] - '0'; break;
    case 'u': dolbyb.UpSamp = atol(&argv[0][2]); break;
    case 't': dolbyb.ThGndB = atof(&argv[0][2]); break;
    case 'a': dolbyb.DecAdB = atof(&argv[0][1]); break;
    default: usage(); exit(1);
    }
    argv++; argc--;
  }
  if (argc != 2) { usage(); exit(1); }

  sf_in = sf_open(argv[0], SFM_READ, &sfinfo);
  if (sf_in == NULL) exit(1);
  sf_out = sf_open(argv[1], SFM_WRITE, &sfinfo);
  if (sf_out == NULL) exit(1);

  buf_in = calloc(sfinfo.channels * sizeof(short), HANDFUL);
  buf_out = calloc(sfinfo.channels * sizeof(short), HANDFUL);
  if (buf_in == NULL || buf_out == NULL) exit(1);

  dolbyb.SmpSec = sfinfo.samplerate;
  dolbyb.NumChn = sfinfo.channels;
  dolbyb.BDepth = 16;
  dolbyb_start(&dolbyb);

  while ((n_read = sf_readf_short(sf_in, buf_in, (sf_count_t)HANDFUL)) > 0) {
    if (Encode) dolbyb_encode(&dolbyb, buf_in, buf_out, (size_t)n_read);
    else        dolbyb_decode(&dolbyb, buf_in, buf_out, (size_t)n_read);
    if (sf_writef_short(sf_out, buf_out, n_read) != n_read) {
      fprintf(stderr, "Write error on the output file.\n");
      break;
    }
  }
#if DEBUG
  dolbyb_dumpparam(&dolbyb);
#endif
  dolbyb_free(&dolbyb);
  sf_close(sf_out);
  sf_close(sf_in);

  return 0;
}
