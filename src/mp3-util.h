/* libSoX MP3 utilities  Copyright (c) 2007-9 SoX contributors
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

#include <sys/stat.h>

#if defined(HAVE_LAME_ID3TAG)

extern char const * lsx_id3tagmap[][2];

#if defined _WIN32

#include <wchar.h>
#include <windows.h>

#define UTF16_ID3 1

static unsigned short * utf8_to_utf16 (const char * utf8)
{
  int len;
  int wlength;
  LPWSTR wstr;

  len = strlen(utf8);
  wlength = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, len, 0, 0);
  if (wlength == 0) return NULL;
  wstr = (LPWSTR)calloc((size_t)(wlength+2), sizeof(wchar_t));
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, len, wstr+1, wlength);
  wstr[0] = 0xFEFF; /* add BOM, it is required by LAME */
  return (unsigned short *) wstr;
}

#elif defined(HAVE_ICONV)

#include <iconv.h>

#define UTF16_ID3 1

static unsigned short * utf8_to_utf16 (const char * utf8) {
    size_t inSize, outSize, cstrSize;
    char *in, *out, *cstr;
    iconv_t conv;
    size_t  status;

    inSize  = strlen(utf8);
    outSize = inSize * 2 + 2; /* worst case, UTF-16 can take max twice as much space as UTF-8 */
    in      = (char *) utf8;
    out     = lsx_malloc(outSize);
    cstr    = out;
    
    conv   = iconv_open("UTF-16", "UTF-8");
    status = iconv(conv, &in, &inSize, &out, &outSize);
    iconv_close(conv);

    if (status == ((size_t) -1)) {
      free(cstr);
      return NULL;
    }

    cstrSize = out - cstr;
    cstr[cstrSize] = 0;                /* add null-terminator, first byte */
    cstr[cstrSize+1] = 0;              /* second byte */
    return (unsigned short *) cstr;
}

#endif /* HAVE_ICONV */



static void set_id3_field (priv_t * p, const char * field, const char * value)
{
  char* buf = lsx_malloc(strlen(field) + strlen(value) + 2);
#if defined(UTF16_ID3)
  unsigned short * utf16;
#endif

  if (!buf) return;
  sprintf(buf, "%s=%s", field, value);

#if defined(UTF16_ID3)
  utf16 = utf8_to_utf16(buf);
  if (utf16) {
    p->id3tag_set_fieldvalue_utf16(p->gfp, utf16);
    free(utf16);
  } else {
    /* the value wasn't valid utf8, fall back to latin1 */
    p->id3tag_set_fieldvalue(p->gfp, buf);
  }
#else
  p->id3tag_set_fieldvalue(p->gfp, buf);
#endif

  free(buf);
}


static void write_comments(sox_format_t * ft)
{
  priv_t *p = (priv_t *) ft->priv;
  const char* comment;
  size_t i;

  p->id3tag_init(p->gfp);
  p->id3tag_set_pad(p->gfp, (size_t)ID3PADDING);

  for (i = 0; lsx_id3tagmap[i][0]; ++i)
    if ((comment = sox_find_comment(ft->oob.comments, lsx_id3tagmap[i][1])))
      set_id3_field(p, lsx_id3tagmap[i][0], comment);
}

#endif /* HAVE_LAME */

#ifdef HAVE_MAD_H

static unsigned long xing_frames(priv_t * p, struct mad_bitptr ptr, unsigned bitlen)
{
  #define XING_MAGIC ( ('X' << 24) | ('i' << 16) | ('n' << 8) | 'g' )
  if (bitlen >= 96 && p->mad_bit_read(&ptr, 32) == XING_MAGIC &&
      (p->mad_bit_read(&ptr, 32) & 1 )) /* XING_FRAMES */
    return p->mad_bit_read(&ptr, 32);
  return 0;
}

static void mad_timer_mult(mad_timer_t * t, double d)
{
  t->seconds = (signed long)(d *= (t->seconds + t->fraction * (1. / MAD_TIMER_RESOLUTION)));
  t->fraction = (unsigned long)((d - t->seconds) * MAD_TIMER_RESOLUTION + .5);
}

static size_t mp3_duration_ms(sox_format_t * ft)
{
  priv_t              * p = (priv_t *) ft->priv;
  struct mad_stream   mad_stream;
  struct mad_header   mad_header;
  struct mad_frame    mad_frame;
  mad_timer_t         time = mad_timer_zero;
  size_t              initial_bitrate = 0; /* Initialised to prevent warning */
  size_t              tagsize = 0, consumed = 0, frames = 0;
  sox_bool            vbr = sox_false, depadded = sox_false;

  p->mad_stream_init(&mad_stream);
  p->mad_header_init(&mad_header);
  p->mad_frame_init(&mad_frame);

  do {  /* Read data from the MP3 file */
    int read, padding = 0;
    size_t leftover = mad_stream.bufend - mad_stream.next_frame;

    memcpy(p->mp3_buffer, mad_stream.this_frame, leftover);
    read = lsx_readbuf(ft, p->mp3_buffer + leftover, p->mp3_buffer_size - leftover);
    if (read <= 0) {
      lsx_debug("got exact duration by scan to EOF (frames=%" PRIuPTR " leftover=%" PRIuPTR ")", frames, leftover);
      break;
    }
    for (; !depadded && padding < read && !p->mp3_buffer[padding]; ++padding);
    depadded = sox_true;
    p->mad_stream_buffer(&mad_stream, p->mp3_buffer + padding, leftover + read - padding);

    while (sox_true) {  /* Decode frame headers */
      mad_stream.error = MAD_ERROR_NONE;
      if (p->mad_header_decode(&mad_header, &mad_stream) == -1) {
        if (mad_stream.error == MAD_ERROR_BUFLEN)
          break;  /* Normal behaviour; get some more data from the file */
        if (!MAD_RECOVERABLE(mad_stream.error)) {
          lsx_warn("unrecoverable MAD error");
          break;
        }
        if (mad_stream.error == MAD_ERROR_LOSTSYNC) {
          unsigned available = (mad_stream.bufend - mad_stream.this_frame);
          tagsize = tagtype(mad_stream.this_frame, (size_t) available);
          if (tagsize) {   /* It's some ID3 tags, so just skip */
            if (tagsize >= available) {
              lsx_seeki(ft, (off_t)(tagsize - available), SEEK_CUR);
              depadded = sox_false;
            }
            p->mad_stream_skip(&mad_stream, min(tagsize, available));
          }
          else lsx_warn("MAD lost sync");
        }
        else lsx_warn("recoverable MAD error");
        continue; /* Not an audio frame */
      }

      p->mad_timer_add(&time, mad_header.duration);
      consumed += mad_stream.next_frame - mad_stream.this_frame;

      lsx_debug_more("bitrate=%lu", mad_header.bitrate);
      if (!frames) {
        initial_bitrate = mad_header.bitrate;

        /* Get the precise frame count from the XING header if present */
        mad_frame.header = mad_header;
        if (p->mad_frame_decode(&mad_frame, &mad_stream) == -1)
          if (!MAD_RECOVERABLE(mad_stream.error)) {
            lsx_warn("unrecoverable MAD error");
            break;
          }
        if ((frames = xing_frames(p, mad_stream.anc_ptr, mad_stream.anc_bitlen))) {
          p->mad_timer_multiply(&time, (signed long)frames);
          lsx_debug("got exact duration from XING frame count (%" PRIuPTR ")", frames);
          break;
        }
      }
      else vbr |= mad_header.bitrate != initial_bitrate;

      /* If not VBR, we can time just a few frames then extrapolate */
      if (++frames == 25 && !vbr) {
        mad_timer_mult(&time, (double)(lsx_filelength(ft) - tagsize) / consumed);
        lsx_debug("got approx. duration by CBR extrapolation");
        break;
      }
    }
  } while (mad_stream.error == MAD_ERROR_BUFLEN);

  p->mad_frame_finish(&mad_frame);
  mad_header_finish(&mad_header);
  p->mad_stream_finish(&mad_stream);
  lsx_rewind(ft);
  return p->mad_timer_count(time, MAD_UNITS_MILLISECONDS);
}

#endif /* HAVE_MAD_H */
