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

#ifdef HAVE_POPEN

#include <stdio.h>
#include <string.h>
#include <ctype.h>

extern sox_format_handler_t const * lsx_sox_format_fn(void); /* used by ffmpeg */

/*
 * Open file with ffmpeg
 */
static int startread(sox_format_t * ft)
{
  char *quoted_filename;
  char *p, *q;
  char const * const command_fmt = "ffmpeg -loglevel quiet -i \"%s\" -f sox -";
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

  return lsx_sox_format_fn()->startread(ft);
}

LSX_FORMAT_HANDLER(ffmpeg)
{
  static char const * const names[] = {
    "ffmpeg", /* Special type to force use of ffmpeg */
    NULL
  };
  static sox_format_handler_t handler;

  handler = *lsx_sox_format_fn();
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

/* and all the formats it handles */

LSX_FORMAT_HANDLER(3gp)
{
  static char const * const names[] = {"3gp", "3gpp", NULL};
  static sox_format_handler_t handler;

  handler = *lsx_ffmpeg_format_fn();
  handler.description = "Third Generation Partnership Project Format";
  handler.names = names;
  return &handler;
}

LSX_FORMAT_HANDLER(aac)
{
  static char const * const names[] = {"aac", NULL};
  static sox_format_handler_t handler;

  handler = *lsx_ffmpeg_format_fn();
  handler.description = "Advanced Audio Coding Format";
  handler.names = names;
  return &handler;
}

LSX_FORMAT_HANDLER(ac3)
{
  static char const * const names[] = {"ac3", NULL};
  static sox_format_handler_t handler;

  handler = *lsx_ffmpeg_format_fn();
  handler.description = "Audio Codec 3 (Dolby Digital) Format";
  handler.names = names;
  return &handler;
}

LSX_FORMAT_HANDLER(dts)
{
  static char const * const names[] = {"dts", NULL};
  static sox_format_handler_t handler;

  handler = *lsx_ffmpeg_format_fn();
  handler.description = "Digital Theatre Systems Format";
  handler.names = names;
  return &handler;
}

LSX_FORMAT_HANDLER(m4a)
{
  static char const * const names[] = {"m4a", NULL};
  static sox_format_handler_t handler;

  handler = *lsx_ffmpeg_format_fn();
  handler.description = "MPEG-4 Audio Format";
  handler.names = names;
  return &handler;
}

LSX_FORMAT_HANDLER(m4v)
{
  static char const * const names[] = {"m4v", NULL};
  static sox_format_handler_t handler;
  handler = *lsx_ffmpeg_format_fn();
  handler.description = "MPEG-4 Video Format";
  handler.names = names;
  return &handler;
}

LSX_FORMAT_HANDLER(oga)
{
  static char const * const names[] = {"oga", NULL};
  static sox_format_handler_t handler;
  handler = *lsx_ffmpeg_format_fn();
  handler.description = "Ogg Vorbis Audio Format";
  handler.names = names;
  return &handler;
}

LSX_FORMAT_HANDLER(ra)
{
  static char const * const names[] = {"ra", NULL};
  static sox_format_handler_t handler;
  handler = *lsx_ffmpeg_format_fn();
  handler.description = "RealAudio Format";
  handler.names = names;
  return &handler;
}

LSX_FORMAT_HANDLER(spx)
{
  static char const * const names[] = {"spx", "speex", NULL};
  static sox_format_handler_t handler;
  handler = *lsx_ffmpeg_format_fn();
  handler.description = "Speex Format";
  handler.names = names;
  return &handler;
}

LSX_FORMAT_HANDLER(tta)
{
  static char const * const names[] = {"tta", NULL};
  static sox_format_handler_t handler;
  handler = *lsx_ffmpeg_format_fn();
  handler.description = "True Audio Format";
  handler.names = names;
  return &handler;
}

LSX_FORMAT_HANDLER(wma)
{
  static char const * const names[] = {"wma", NULL};
  static sox_format_handler_t handler;

  handler = *lsx_ffmpeg_format_fn();
  handler.description = "Windows Media Audio Format";
  handler.names = names;
  return &handler;
}

#endif
