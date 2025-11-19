/* Copyright (C) 2002 Fabrizio Gennari <fabrizio.ge@tiscali.it>
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

/* The decoding part is based on the decoder-tutorial program madlld
 * which is Copyright (c) 2001-2004 Bertrand Petit <madlld@phoe.fmug.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 * 
 * 3. Neither the name of the author nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* MP3 support for SoX
 *
 * Uses libmad for MP3 decoding
 * libmp3lame for MP3 encoding
 * and libtwolame for MP2 encoding
 */

#include "sox_i.h"
#include "mp3.h"
#include "mp3-mad.h"

#if defined(HAVE_MAD) || defined(HAVE_LAME) || defined(HAVE_TWOLAME)

#define MAXFRAMESIZE 2880
#define ID3PADDING 128

/* Twolame takes float values as input. */
#define MP2_TWOLAME_PRECISION   24

/* LAME takes float values as input. */
#define MP3_LAME_PRECISION   24

/* Note: sox_precision() returns 0 for SOX_ENCODING_MP2 and _MP3, as it varies
 * according to whether you're encoding or decoding but sox_ng.c knows
 * about this and has a special case and reports 24 as the Writes: precision */

#include "mp3-util.h"

#if !HAVE_MAD

#define startread NULL
#define sox_mp3read NULL
#define stopread NULL
#define sox_mp3seek NULL

#endif /* !HAVE_MAD */

#if HAVE_LAME

/* Adapters for lame message callbacks: */

static void errorf(const char* fmt, va_list va)
{
  sox_globals.subsystem=__FILE__;
  if (sox_globals.output_message_handler)
    (*sox_globals.output_message_handler)(1,sox_globals.subsystem,fmt,va);
  return;
}

static void debugf(const char* fmt, va_list va)
{
  sox_globals.subsystem=__FILE__;
  if (sox_globals.output_message_handler)
    (*sox_globals.output_message_handler)(4,sox_globals.subsystem,fmt,va);
  return;
}

static void msgf(const char* fmt, va_list va)
{
  sox_globals.subsystem=__FILE__;
  if (sox_globals.output_message_handler)
    (*sox_globals.output_message_handler)(3,sox_globals.subsystem,fmt,va);
  return;
}

/* These functions are considered optional. If they aren't present in the
   library, the stub versions defined here will be used instead. */

UNUSED static void id3tag_init_stub(lame_global_flags * gfp UNUSED)
  { return; }
UNUSED static void id3tag_set_title_stub(lame_global_flags * gfp UNUSED, const char* title UNUSED)
  { return; }
UNUSED static void id3tag_set_artist_stub(lame_global_flags * gfp UNUSED, const char* artist UNUSED)
  { return; }
UNUSED static void id3tag_set_album_stub(lame_global_flags * gfp UNUSED, const char* album UNUSED)
  { return; }
UNUSED static void id3tag_set_year_stub(lame_global_flags * gfp UNUSED, const char* year UNUSED)
  { return; }
UNUSED static void id3tag_set_comment_stub(lame_global_flags * gfp UNUSED, const char* comment UNUSED)
  { return; }
UNUSED static void id3tag_set_track_stub(lame_global_flags * gfp UNUSED, const char* track UNUSED)
  { return; }
UNUSED static int id3tag_set_genre_stub(lame_global_flags * gfp UNUSED, const char* genre UNUSED)
  { return 0; }
UNUSED static size_t id3tag_set_pad_stub(lame_global_flags * gfp UNUSED, size_t n UNUSED)
  { return 0; }
UNUSED static size_t lame_get_id3v2_tag_stub(lame_global_flags * gfp UNUSED, unsigned char * buffer UNUSED, size_t size UNUSED)
  { return 0; }
UNUSED static int id3tag_set_fieldvalue_stub(lame_global_flags * gfp UNUSED, const char *fieldvalue UNUSED)
  { return 0; }

static int get_id3v2_tag_size(sox_format_t * ft)
{
  size_t bytes_read;
  int id3v2_size;
  unsigned char id3v2_header[10];

  if (lsx_seeki(ft, (off_t)0, SEEK_SET) != 0) {
    lsx_warn("cannot update id3 tag - failed to seek to beginning");
    return SOX_EOF;
  }

  /* read 10 bytes in case there's an ID3 version 2 header here */
  bytes_read = lsx_readbuf(ft, id3v2_header, sizeof(id3v2_header));
  if (bytes_read != sizeof(id3v2_header)) {
    lsx_warn("cannot update id3 tag - failed to read id3 header");
    return SOX_EOF;      /* not readable, maybe opened Write-Only */
  }

  /* does the stream begin with the ID3 version 2 file identifier? */
  if (!strncmp((char *) id3v2_header, "ID3", (size_t)3)) {
    /* the tag size (minus the 10-byte header) is encoded into four
     * bytes where the most significant bit is clear in each byte */
    id3v2_size = (((id3v2_header[6] & 0x7f) << 21)
                    | ((id3v2_header[7] & 0x7f) << 14)
                    | ((id3v2_header[8] & 0x7f) << 7)
                    | (id3v2_header[9] & 0x7f))
        + sizeof(id3v2_header);
  } else {
    /* no ID3 version 2 tag in this stream */
    id3v2_size = 0;
  }
  return id3v2_size;
}

static void rewrite_id3v2_tag(sox_format_t * ft, size_t id3v2_size, uint64_t num_samples)
{
  priv_t *p = (priv_t *)ft->priv;
  size_t new_size;
  unsigned char * buffer;

  if (LSX_DLFUNC_IS_STUB(p, lame_get_id3v2_tag))
  {
    if (p->num_samples) {
      lsx_warn("this version of LAME does not support tag update;");
      lsx_warn("the track length will be incorrect");
    } else {
      lsx_report("this version of LAME does not support tag update;");
      lsx_report("the track length will be unspecified");
    }
    return;
  }

  buffer = lsx_malloc(id3v2_size);

  if (num_samples > ULONG_MAX)
  {
    lsx_warn("cannot accurately update track length info - file is too long");
    num_samples = 0;
  }
  p->lame_set_num_samples(p->gfp, (unsigned long)num_samples);
  lsx_debug("updated MP3 TLEN to %lu samples", (unsigned long)num_samples);

  new_size = p->lame_get_id3v2_tag(p->gfp, buffer, id3v2_size);

  if (new_size != id3v2_size && new_size-ID3PADDING <= id3v2_size) {
    p->id3tag_set_pad(p->gfp, ID3PADDING + id3v2_size - new_size);
    new_size = p->lame_get_id3v2_tag(p->gfp, buffer, id3v2_size);
  }

  if (new_size != id3v2_size) {
    if (LSX_DLFUNC_IS_STUB(p, id3tag_set_pad))
    {
      if (p->num_samples) {
        lsx_report("this version of LAME does not support tag size adjustment;");
        lsx_report("the track length will be invalid");
      } else {
        lsx_report("this version of LAME does not support tag size adjustment;");
        lsx_report("the track length will be unspecified");
      }
    }
    else
      lsx_warn("cannot update track length info - failed to adjust tag size");
  } else {
    if (lsx_seeki(ft, (off_t)0, SEEK_SET))
      lsx_warn("cannot rewrite Id3v2 tag");
      
    /* Overwrite the Id3v2 tag (this time TLEN should be accurate) */
    if (lsx_writebuf(ft, buffer, id3v2_size) != 1) {
      lsx_debug("Rewrote Id3v2 tag (%" PRIuPTR " bytes)", id3v2_size);
    }
  }

  free(buffer);
}

static void rewrite_tags(sox_format_t * ft, uint64_t num_samples)
{
  priv_t *p = (priv_t *)ft->priv;

  off_t file_size;
  size_t id3v2_size;

  if (lsx_seeki(ft, (off_t)0, SEEK_END)) {
    lsx_warn("cannot update tags - seek to end failed");
    return;
  }

  /* Get file size */
  file_size = lsx_tell(ft);

  if (file_size == 0) {
    lsx_warn("cannot update tags - file size is 0");
    return;
  }

  id3v2_size = get_id3v2_tag_size(ft);
  if (id3v2_size > 0 && num_samples != p->num_samples) {
    rewrite_id3v2_tag(ft, id3v2_size, num_samples);
  }

  if (p->vbr_tag) {
    size_t lametag_size;
    uint8_t buffer[MAXFRAMESIZE];

    if (lsx_seeki(ft, (off_t)id3v2_size, SEEK_SET)) {
      lsx_warn("cannot write VBR tag - seek to tag block failed");
      return;
    }

    lametag_size = p->lame_get_lametag_frame(p->gfp, buffer, sizeof(buffer));
    if (lametag_size > sizeof(buffer)) {
      lsx_warn("cannot write VBR tag - VBR tag too large for buffer");
      return;
    }

    if (lametag_size < 1) {
      return;
    }

    if (lsx_writebuf(ft, buffer, lametag_size) != lametag_size) {
      lsx_warn("cannot write VBR tag - VBR tag write failed");
    } else {
      lsx_debug("rewrote VBR tag (%" PRIuPTR " bytes)", lametag_size);
    }
  }
}

#endif /* HAVE_LAME */

#if defined(HAVE_LAME) || defined(HAVE_TWOLAME)

#define LAME_BUFFER_SIZE(num_samples) (((num_samples) + 3) / 4 * 5 + 7200)

static int startwrite_mp2(sox_format_t * ft)
{
#if !HAVE_TWOLAME
  lsx_fail_errno(ft,SOX_EOF,"SoX was compiled without MP2 encoding support");
  return SOX_EOF;
#else
  priv_t *p = (priv_t *) ft->priv;
  int openlibrary_result;

  LSX_DLLIBRARY_OPEN(
      p,
      twolame_dl,
      TWOLAME_FUNC_ENTRIES,
      "Twolame encoder library",
      twolame_library_names,
      openlibrary_result);
  if (openlibrary_result)
    return SOX_EOF;

  if ((p->opt = p->twolame_init()) == NULL){
    lsx_fail_errno(ft,SOX_EOF,"initialization of Twolame library failed");
    return SOX_EOF;
  }
  (void) p->twolame_set_verbosity(p->opt, 0);

  ft->signal.precision = MP2_TWOLAME_PRECISION;

  if (ft->signal.channels != SOX_ENCODING_UNKNOWN) {
    if (p->twolame_set_num_channels(p->opt,(int)ft->signal.channels) != 0) {
      lsx_fail_errno(ft,SOX_EOF,"unsupported number of channels");
      return(SOX_EOF);
    }
  } else {
    ft->signal.channels = p->twolame_get_num_channels(p->opt); /* Twolame default */
  }

  p->twolame_set_in_samplerate(p->opt,(int)ft->signal.rate);
  p->twolame_set_out_samplerate(p->opt,(int)ft->signal.rate);

  lsx_debug("-C option is %f", ft->encoding.compression);

  if (ft->encoding.compression == HUGE_VAL) {
    /* Do nothing, use defaults: */
    lsx_report("using MP2 encoding defaults");
  } else {
    /* Twolame's CBR rates are exactly 32,48,64... and VBR rates a float
     * from -50 to +50 with "useful" values from -10 to +10 so we count
     * any floating point value from -50 to 50 as VBR unless it is exactly
     * 32 or 48, and anything else as CBR.
     */
    if (ft->encoding.compression >=-50 && ft->encoding.compression <= 50 &&
        ft->encoding.compression != 32 && ft->encoding.compression != 48) {
        if (p->twolame_set_VBR(p->opt, 1) != 0) {
          lsx_fail_errno(ft,SOX_EOF,"failed to enable MP2 VBR");
          return(SOX_EOF);
        }
        if (p->twolame_set_VBR_level(p->opt, ft->encoding.compression) != 0) {
          lsx_fail_errno(ft,SOX_EOF,"failed to set MP2 VBR level");
          return(SOX_EOF);
        }
    } else {
      if (p->twolame_set_brate(p->opt, (int)ft->encoding.compression) != 0) {
        lsx_fail_errno(ft, SOX_EOF, "invalid MP2 bitrate");
        return(SOX_EOF);
      }
      lsx_report("twolame set bitrate(%d)", (int)ft->encoding.compression);
    }
  }

  if (p->twolame_init_params(p->opt) != 0) {
    lsx_fail_errno(ft,SOX_EOF,"Twolame initialization failed");
    return(SOX_EOF);
  }

  return(SOX_SUCCESS);
#endif
}

static int startwrite_mp3(sox_format_t * ft)
{
#if !HAVE_LAME
  lsx_fail_errno(ft,SOX_EOF,"SoX was compiled without MP3 encoding support");
  return SOX_EOF;
#else
  priv_t *p = (priv_t *) ft->priv;
  int openlibrary_result;

  LSX_DLLIBRARY_OPEN(
      p,
      lame_dl,
      LAME_FUNC_ENTRIES,
      "LAME encoder library",
      lame_library_names,
      openlibrary_result);
  if (openlibrary_result)
    return SOX_EOF;

  if ((p->gfp = p->lame_init()) == NULL){
    lsx_fail_errno(ft,SOX_EOF,"initialization of LAME library failed");
    return SOX_EOF;
  }

  /* First set message callbacks so we don't miss any messages: */
  p->lame_set_errorf(p->gfp,errorf);
  p->lame_set_debugf(p->gfp,debugf);
  p->lame_set_msgf  (p->gfp,msgf);

  p->num_samples = ft->signal.length == SOX_IGNORE_LENGTH ? 0 : ft->signal.length / max(ft->signal.channels, 1);
  p->lame_set_num_samples(p->gfp, p->num_samples > ULONG_MAX ? 0 : (unsigned long)p->num_samples);

  ft->signal.precision = MP3_LAME_PRECISION;

  if (ft->signal.channels != SOX_ENCODING_UNKNOWN) {
    if (p->lame_set_num_channels(p->gfp,(int)ft->signal.channels) < 0) {
      lsx_fail_errno(ft,SOX_EOF,"unsupported number of channels");
      return(SOX_EOF);
    }
  } else {
    ft->signal.channels = p->lame_get_num_channels(p->gfp); /* LAME default */
  }

  p->lame_set_in_samplerate(p->gfp,(int)ft->signal.rate);
  p->lame_set_out_samplerate(p->gfp,(int)ft->signal.rate);

  if (!LSX_DLFUNC_IS_STUB(p, id3tag_init))
    write_comments(ft);

  /* The primary parameter to the LAME encoder is the bit rate. If the
   * value of encoding.compression is a positive integer, it's taken as
   * the bitrate in kbps (that is if you specify 128, it use 128 kbps).
   *
   * The second most important parameter is probably "quality" (really
   * performance), which allows balancing encoding speed vs. quality.
   * In LAME, 0 specifies highest quality but is very slow, while
   * 9 selects poor quality, but is fast. (5 is the default and 2 is
   * recommended as a good trade-off for high quality encodes.)
   *
   * Because encoding.compression is a float, the fractional part is used
   * to select quality. 128.2 selects 128 kbps encoding with a quality
   * of 2. There is one problem with this approach. We need 128 to specify
   * 128 kbps encoding with default quality, so .0 means use default. Instead
   * of .0 you have to use .01 to specify the highest quality (128.01).
   *
   * LAME uses bitrate to specify a constant bitrate, but higher quality
   * can be achieved using Variable Bit Rate (VBR). VBR quality (really
   * size) is selected using a number from 0 to 9. Use a value of 0 for high
   * quality, larger files, and 9 for smaller files of lower quality. 4 is
   * the default.
   *
   * In order to squeeze the selection of VBR into the encoding.compression
   * float we use negative numbers to select VRR. -4.2 would select default
   * VBR encoding (size) with high quality (speed). One special case is 0,
   * which is a valid VBR encoding parameter but not a valid bitrate.
   * Compression value of 0 is always treated as a high quality vbr, as a
   * result both -0.2 and 0.2 are treated as highest quality VBR (size) and
   * high quality (speed).
   *
   * Note: It would have been nice to simply use low values, 0-9, to trigger
   * VBR mode, but 8 kbps is a valid bit rate, so negative values were
   * used instead.
   */

  lsx_debug("-C option is %f", ft->encoding.compression);

  if (ft->encoding.compression == HUGE_VAL) {
    /* Do nothing, use defaults: */
    lsx_report("using MP3 encoding defaults");
  } else {
    double abs_compression = fabs(ft->encoding.compression);
    double floor_compression = floor(abs_compression);
    double fraction_compression = abs_compression - floor_compression;
    int bitrate_q = (int)floor_compression;
    int encoder_q =
        fraction_compression == 0.0
        ? -1
        : (int)(fraction_compression * 10.0 + 0.5);

    if (ft->encoding.compression < 0.5) {
      if (p->lame_get_VBR(p->gfp) == vbr_off)
        p->lame_set_VBR(p->gfp, vbr_default);

      if (ft->seekable) {
        p->vbr_tag = 1;
      } else {
        lsx_warn("unable to write VBR tag because we can't seek");
      }

      if (p->lame_set_VBR_q(p->gfp, bitrate_q) < 0)
      {
        lsx_fail_errno(ft, SOX_EOF,
          "lame_set_VBR_q(%d) failed (should be between 0 and 9)",
          bitrate_q);
        return(SOX_EOF);
      }
      lsx_report("lame_set_VBR_q(%d)", bitrate_q);
    } else {
      if (p->lame_set_brate(p->gfp, bitrate_q) < 0) {
        lsx_fail_errno(ft, SOX_EOF,
          "lame_set_brate(%d) failed", bitrate_q);
        return(SOX_EOF);
      }
      lsx_report("lame_set_brate(%d)", bitrate_q);
    }

    /* Set Quality */

    if (encoder_q < 0) {
      /* use default quality value */
      lsx_report("using MP3 default quality");
    } else {
      if (p->lame_set_quality(p->gfp, encoder_q) < 0) {
        lsx_fail_errno(ft, SOX_EOF,
          "lame_set_quality(%d) failed", encoder_q);
        return(SOX_EOF);
      }
      lsx_report("lame_set_quality(%d)", encoder_q);
    }
  }

  p->lame_set_bWriteVbrTag(p->gfp, p->vbr_tag);

  if (p->lame_init_params(p->gfp) < 0) {
    lsx_fail_errno(ft,SOX_EOF,"LAME initialization failed");
    return(SOX_EOF);
  }

  return(SOX_SUCCESS);
#endif
}

static int startwrite(sox_format_t * ft)
{
  priv_t *p = (priv_t *) ft->priv;

  p->mp2 = !!strchr(ft->filetype, '2');

  if (ft->encoding.encoding != SOX_ENCODING_MP2 &&
      ft->encoding.encoding != SOX_ENCODING_MP3) {
    if(ft->encoding.encoding != SOX_ENCODING_UNKNOWN)
      lsx_report("encoding forced to MP%d", p->mp2 ? 2 : 3);
    ft->encoding.encoding = p->mp2 ? SOX_ENCODING_MP2 : SOX_ENCODING_MP3;
  }

  p->mp3_buffer_size = LAME_BUFFER_SIZE(sox_globals.bufsiz / max(ft->signal.channels, 1));
  p->mp3_buffer = lsx_malloc(p->mp3_buffer_size);

  p->pcm_buffer_size = sox_globals.bufsiz * sizeof(float);
  p->pcm_buffer = lsx_malloc(p->pcm_buffer_size);


  return p->mp2 ? startwrite_mp2(ft) : startwrite_mp3(ft);
}

#define MP3_SAMPLE_TO_FLOAT(d,clips) ((float)(32768*SOX_SAMPLE_TO_FLOAT_32BIT(d,clips)))

static size_t sox_mp3write(sox_format_t * ft, const sox_sample_t *buf, size_t samp)
{
    priv_t *p = (priv_t *)ft->priv;
    size_t new_buffer_size;
    float *buffer_l, *buffer_r = NULL;
    int nsamples = samp/ft->signal.channels;
    int i,j;
    int written = 0;
    int clips = 0;
    SOX_SAMPLE_LOCALS;

    new_buffer_size = samp * sizeof(float);
    if (p->pcm_buffer_size < new_buffer_size) {
      float *new_buffer = lsx_realloc(p->pcm_buffer, new_buffer_size);
      p->pcm_buffer_size = new_buffer_size;
      p->pcm_buffer = new_buffer;
    }

    buffer_l = p->pcm_buffer;

    if (p->mp2)
    {
        size_t s;
        for(s = 0; s < samp; s++)
            buffer_l[s] = SOX_SAMPLE_TO_FLOAT_32BIT(buf[s], clips);
    }
    else
    {
        if (ft->signal.channels == 2)
        {
            /* lame doesn't support interleaved samples for floats so we must break
             * them out into seperate buffers.
             */
            buffer_r = p->pcm_buffer + nsamples;
            j=0;
            for (i = 0; i < nsamples; i++)
            {
                buffer_l[i] = MP3_SAMPLE_TO_FLOAT(buf[j++], clips);
                buffer_r[i] = MP3_SAMPLE_TO_FLOAT(buf[j++], clips);
            }
        }
        else
        {
            j=0;
            for (i = 0; i < nsamples; i++) {
                buffer_l[i] = MP3_SAMPLE_TO_FLOAT(buf[j++], clips);
            }
        }
    }

    new_buffer_size = LAME_BUFFER_SIZE(nsamples);
    if (p->mp3_buffer_size < new_buffer_size) {
      unsigned char *new_buffer = lsx_realloc(p->mp3_buffer, new_buffer_size);
      p->mp3_buffer_size = new_buffer_size;
      p->mp3_buffer = new_buffer;
    }

    if(p->mp2) {
#ifdef HAVE_TWOLAME
        written = p->twolame_encode_buffer_float32_interleaved(p->opt, buffer_l,
                  nsamples, p->mp3_buffer, (int)p->mp3_buffer_size);
#endif
    } else {
#ifdef HAVE_LAME
        written = p->lame_encode_buffer_float(p->gfp, buffer_l, buffer_r,
                  nsamples, p->mp3_buffer, (int)p->mp3_buffer_size);
#endif
    }
    if (written < 0) {
        lsx_fail_errno(ft,SOX_EOF,"encoding failed");
        return 0;
    }

    if (lsx_writebuf(ft, p->mp3_buffer, (size_t)written) < (size_t)written)
    {
        lsx_fail_errno(ft,SOX_EOF,"file write failed");
        return 0;
    }

    return samp;
}

static int stopwrite(sox_format_t * ft)
{
  priv_t *p = (priv_t *) ft->priv;
#ifdef HAVE_LAME
  uint64_t num_samples = ft->olength == SOX_IGNORE_LENGTH ? 0 : ft->olength / max(ft->signal.channels, 1);
#endif
  int written = 0;

  if (p->mp2) {
#ifdef HAVE_TWOLAME
    written = p->twolame_encode_flush(p->opt, p->mp3_buffer, (int)p->mp3_buffer_size);
#endif
  } else {
#ifdef HAVE_LAME
    written = p->lame_encode_flush(p->gfp, p->mp3_buffer, (int)p->mp3_buffer_size);
#endif
  }
  if (written < 0)
    lsx_fail_errno(ft, SOX_EOF, "encoding failed");
  else if (lsx_writebuf(ft, p->mp3_buffer, (size_t)written) < (size_t)written)
    lsx_fail_errno(ft, SOX_EOF, "file write failed");
  else if (!p->mp2) {
#ifdef HAVE_LAME
    if (ft->seekable && (num_samples != p->num_samples || p->vbr_tag))
      rewrite_tags(ft, num_samples);
#endif
  }

  free(p->mp3_buffer);
  free(p->pcm_buffer);

  if(p->mp2) {
#ifdef HAVE_TWOLAME
    p->twolame_close(&p->opt);
    LSX_DLLIBRARY_CLOSE(p, twolame_dl);
#endif
  } else {
#ifdef HAVE_LAME
    p->lame_close(p->gfp);
    LSX_DLLIBRARY_CLOSE(p, lame_dl);
#endif
  }
  return SOX_SUCCESS;
}

#else /* !(HAVE_LAME || HAVE_TWOLAME) */

#define startwrite NULL
#define sox_mp3write NULL
#define stopwrite NULL

#endif /* HAVE_LAME || HAVE_TWOLAME */

/* MAD can tell the difference between Layer 1 and Layer 2 but decodes them both
 * with the same decoder (Layer 1 is a subset of Layer 2) but soxi might
 * tell people if they happen across an MP1 file */
LSX_FORMAT_HANDLER(mp1)
{
  static char const * const names[] = {"mp1", "m1a", NULL};
  static sox_format_handler_t const handler = {SOX_LIB_VERSION_CODE,
    "MPEG-1 Layer 1 lossy audio compression", names, 0,
    startread_mad, read_mad, stopread_mad,
    NULL, NULL, NULL,
    seek_mad, NULL, NULL, sizeof(priv_t)
  };
  return &handler;
}

LSX_FORMAT_HANDLER(mp2)
{
  static char const * const names[] = {"mp2", NULL};
  static unsigned const write_encodings[] = {
    SOX_ENCODING_MP2, 0, 0};
  static sox_rate_t const write_rates[] = {
    16000, 22050, 24000, 32000, 44100, 48000, 0};
  static sox_format_handler_t const handler = {SOX_LIB_VERSION_CODE,
    "MPEG-1 Layer 2 lossy audio compression", names, 0,
    startread_mad, read_mad, stopread_mad,
    startwrite, sox_mp3write, stopwrite,
    seek_mad, write_encodings, write_rates, sizeof(priv_t)
  };
  return &handler;
}

LSX_FORMAT_HANDLER(mp3)
{
  /* "mp3" is both the name of a dynamic format handler plugin and
   * a filename extension and to load the above two handlers --with-mp3=dyn
   * they must be listed here because init_format() relies on that to guess
   * the symbol names of lsx_mp[12]_format_fn() and "mp3", the one that
   * corresponds to the symbol name of this handler, must come first.
   */
  static char const * const names[] = {"mp3", "mp2", "mp1", NULL};
  static unsigned const write_encodings[] = {
    SOX_ENCODING_MP3, 0, 0};
  static sox_rate_t const write_rates[] = {
    8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000, 0};
  static sox_format_handler_t const handler = {SOX_LIB_VERSION_CODE,
    "MPEG-1 Layer 3 lossy audio compression", names, 0,
    startread_mad, read_mad, stopread_mad,
    startwrite, sox_mp3write, stopwrite,
    seek_mad, write_encodings, write_rates, sizeof(priv_t)
  };
  return &handler;
}
#endif /* defined(HAVE_MAD) || defined(HAVE_LAME) || defined(HAVE_TWOLAME) */
