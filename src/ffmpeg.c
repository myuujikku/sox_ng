/* libSoX ffmpeg file formats
 *
 * Copyright (c) 2024 Martin Guy <sox_ng@fastmail.com>
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or (at
 * your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "sox_i.h"

#if USING_FFMPEG
/* "configure" checks that we HAVE_POPEN */

#include <ctype.h>

extern sox_format_handler_t const * lsx_au_format_fn(void);

/*
 * Open file with ffmpeg
 */
static int startread(sox_format_t * ft)
{
  char *quoted_filename;
  char *p, *q;
  char const * const command_fmt = "ffmpeg -loglevel quiet -strict -2 -i \"%s\" -f au -";
  char *command;

  /* Quote special characters in the filename */
  quoted_filename = lsx_malloc(strlen(ft->filename) * 2 + 1);
  for (p=ft->filename, q=quoted_filename; *p; p++, q++) {
    switch (*p) {
    case '"':
    case '`':
    case '\\':
    case '$':
    case '\n':
      *q++ = '\\';
      break;
    }
    *q = *p;
  }
  *q = '\0';

  command = malloc(strlen(quoted_filename) + strlen(command_fmt) + 1);
  sprintf(command, command_fmt, quoted_filename);

  ft->fp = popen(command, "r");
  free(command);
  free(quoted_filename);

  return lsx_au_format_fn()->startread(ft);
}

LSX_FORMAT_HANDLER(ffmpeg)
{
  static char const * const names[] = {
    "ffmpeg", /* Special type to force use of ffmpeg */
    NULL
  };
  static sox_format_handler_t handler;

  handler = *lsx_au_format_fn();
  handler.description = "Pseudo format to use ffmpeg";
  handler.names = names;
  handler.startread = startread;
  handler.write_formats = NULL;
  handler.startwrite = NULL;
  handler.write = NULL;
  handler.stopwrite = NULL;
  handler.seek = NULL;

  return &handler;
}

/* All the formats ffmpeg handles that sox doesn't otherwise,
 * created with yet more macros because there are too many! */

/* For example, the three "3gp" macros expand to: */
#if 0
LSX_FORMAT_HANDLER(3gp)
{
  static char const * const names[] = { "3gp", "3gpp", NULL };
  static sox_format_handler_t handler;

  handler = *lsx_ffmpeg_format_fn();
  handler.description = "Third Generation Partnership Project"
  handler.names = names;
  return &handler;
}
#endif

#define FFMPEG_FORMAT(name) \
LSX_FORMAT_HANDLER(name) \
{ static char const * const names[] = {

#define FFMPEG_DESCRIPTION , NULL}; \
  static sox_format_handler_t handler; \
  handler = *lsx_ffmpeg_format_fn(); \
  handler.description =

#define FFMPEG_ENDFORMAT ; \
  handler.names = names; \
  return &handler; \
}

FFMPEG_FORMAT(3gp) "3gp", "3gpp"
FFMPEG_DESCRIPTION "Third Generation Partnership Project"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(aac) "aac"
FFMPEG_DESCRIPTION "Advanced Audio Coding"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(ac3) "ac3"
FFMPEG_DESCRIPTION "Audio Codec 3 (Dolby Digital)"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(adx) "adx"
FFMPEG_DESCRIPTION "CRI ADX"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(ape) "ape"
FFMPEG_DESCRIPTION "Monkey's Audio"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(apm) "apm"
FFMPEG_DESCRIPTION "Ubisoft Rayman 2 APM"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(aptx) "aptx", "aptxhd"
FFMPEG_DESCRIPTION "Audio Processing Technology for Bluetooth"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(argo_asf) "argo_asf"
FFMPEG_DESCRIPTION "Argonaut Games ASF"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(asf) "asf"
FFMPEG_DESCRIPTION "Advanced / Active Streaming Format"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(ast) "ast"
FFMPEG_DESCRIPTION "AST (Audio Stream)"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(avi) "avi"
FFMPEG_DESCRIPTION "Audio Video Interleaved"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(dfpwm) "dfpwm"
FFMPEG_DESCRIPTION "DFPWM1a"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(dts) "dts"
FFMPEG_DESCRIPTION "Digital Theatre Systems"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(eac3) "eac3"
FFMPEG_DESCRIPTION "Enhanced AC-3 Audio"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(flv) "flv"
FFMPEG_DESCRIPTION "Macromedia Flash Video"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(gxf) "gxf"
FFMPEG_DESCRIPTION "General eXchange Format"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(kvag) "kvag"
FFMPEG_DESCRIPTION "Simon & Schuster Interactive VAG"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(loas) "loas"
FFMPEG_DESCRIPTION "LOAS AudioSyncStream"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(m4a) "m4a"
FFMPEG_DESCRIPTION "MPEG-4 Audio"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(m4v) "m4v", "mp4"
FFMPEG_DESCRIPTION "MPEG-4 Video"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(mlp) "mlp"
FFMPEG_DESCRIPTION "Meridian Lossless Packing"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(mpeg) "mpg", "mpeg"
FFMPEG_DESCRIPTION "MPEG-1 Systems / MPEG program stream"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(mpegts) "mpegts"
FFMPEG_DESCRIPTION "MPEG-TS (MPEG-2 Transport Stream)"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(nut) "nut"
FFMPEG_DESCRIPTION "NUT"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(oga) "oga"
FFMPEG_DESCRIPTION "Ogg Audio"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(ra)  "ra"
FFMPEG_DESCRIPTION "RealAudio"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(rm)  "rm"
FFMPEG_DESCRIPTION "RealMedia"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(rso) "rso"
FFMPEG_DESCRIPTION "Lego Mindstorms RSO"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(sbc) "sbc"
FFMPEG_DESCRIPTION "SBC"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(smjpeg) "smjpeg"
FFMPEG_DESCRIPTION "Loki SDL MJPEG"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(spdif) "spdif"
FFMPEG_DESCRIPTION "IEC 61937 S/PDIF"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(spx) "spx", "speex"
FFMPEG_DESCRIPTION "Ogg Speex"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(tta) "tta"
FFMPEG_DESCRIPTION "True Audio"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(vag) "vag"
FFMPEG_DESCRIPTION "Sony PS2 VAG"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(wma) "wma"
FFMPEG_DESCRIPTION "Windows Media Audio"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(wsaud) "wsaud"
FFMPEG_DESCRIPTION "Westwood Studios audio"
FFMPEG_ENDFORMAT

FFMPEG_FORMAT(wtv) "wtv"
FFMPEG_DESCRIPTION "Windows Television"
FFMPEG_ENDFORMAT

#endif
