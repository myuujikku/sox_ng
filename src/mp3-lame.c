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


/* libmp3lame support for SoX */

#include "sox_i.h"
#include "mp3.h"
#include "mp3-lame.h"

#if HAVE_LAME

/* For encoding */
#define MAXFRAMESIZE 2880
#define ID3PADDING 128
/* LAME takes float values as input. */
#define MP3_LAME_PRECISION   24

/* For decoding */
/* The size of the char buffer used to pass MPEG data to the decoder */
#define HIP_MPEG_BUFSIZ 4096
/* The decoder returns up to 1152 samples per channel */
#define HIP_PCM_BUFSIZ 1152

/* Note: sox_precision() returns 0 for SOX_ENCODING_MP2 and _MP3, as it varies
 * according to whether you're encoding or decoding but sox_ng.c knows
 * about this and has a special case and reports 24 as the Writes: precision */

/* Names for the mp3data.mode field */
static char *mode_string[] = { /* The meanings of mp3data.mode */
  "STEREO", "JOINT STEREO", "DUAL CHANNEL", "MONO"
};

static void write_comments(sox_format_t * ft)
{
  priv_t *p = (priv_t *) ft->priv;
  const char* comment;

  p->id3tag_init(p->gfp);
  p->id3tag_set_pad(p->gfp, (size_t)ID3PADDING);

  /* Note: id3tag_set_fieldvalue is not present in LAME 3.97, so we're using
     the 3.97-compatible methods for all of the tags that 3.97 supported. */
  /* FIXME: This is no more necessary, since support for LAME 3.97 has ended. */
  if ((comment = sox_find_comment(ft->oob.comments, "Title")))
    p->id3tag_set_title(p->gfp, comment);
  if ((comment = sox_find_comment(ft->oob.comments, "Artist")))
    p->id3tag_set_artist(p->gfp, comment);
  if ((comment = sox_find_comment(ft->oob.comments, "Album")))
    p->id3tag_set_album(p->gfp, comment);
  if ((comment = sox_find_comment(ft->oob.comments, "Tracknumber")))
    p->id3tag_set_track(p->gfp, comment);
  if ((comment = sox_find_comment(ft->oob.comments, "Year")))
    p->id3tag_set_year(p->gfp, comment);
  if ((comment = sox_find_comment(ft->oob.comments, "Comment")))
    p->id3tag_set_comment(p->gfp, comment);
  if ((comment = sox_find_comment(ft->oob.comments, "Genre")))
  {
    if (p->id3tag_set_genre(p->gfp, comment))
      lsx_warn("\"%s\" is not a recognized ID3v1 genre.", comment);
  }

  if ((comment = sox_find_comment(ft->oob.comments, "Discnumber")))
  {
    char* id3tag_buf = lsx_malloc(strlen(comment) + 6);
    if (id3tag_buf)
    {
      sprintf(id3tag_buf, "TPOS=%s", comment);
      p->id3tag_set_fieldvalue(p->gfp, id3tag_buf);
      free(id3tag_buf);
    }
  }
}

/*
 * Adapters for lame and hip message callbacks.
 */

/* THe fmt string has a trailing newline which we don't want,
 * but we can't modify the const string we're passed so take a copy
 * and modify that.
 */
static char *unnewline(char const *in)
{
  char *out = lsx_strdup(in);
  char *newline = strrchr(out, '\n');

  if (newline) *newline = '\0';

  return out;
}

static void errorf(const char* fmt, va_list va)
{
  /* What they call "errors" are actually warnings
   * and they carry on all the same, hence 2, not 1.
   */
  char *fmt2 = unnewline(fmt);
  sox_globals.subsystem=__FILE__;
  if (sox_globals.output_message_handler)
    (*sox_globals.output_message_handler)(2,sox_globals.subsystem,fmt2,va);
  free(fmt2);
  return;
}

static void msgf(const char* fmt, va_list va)
{
  char *fmt2 = unnewline(fmt);
  sox_globals.subsystem=__FILE__;
  if (sox_globals.output_message_handler)
    (*sox_globals.output_message_handler)(3,sox_globals.subsystem,fmt2,va);
  free(fmt2);
  return;
}

static void debugf(const char* fmt, va_list va)
{
  char *fmt2 = unnewline(fmt);
  sox_globals.subsystem=__FILE__;
  if (sox_globals.output_message_handler)
    (*sox_globals.output_message_handler)(4,sox_globals.subsystem,fmt2,va);
  free(fmt2);
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

#define LAME_BUFFER_SIZE(num_samples) (((num_samples) + 3) / 4 * 5 + 7200)

/* MP1/2/3 decoding using LAME's built-in mpglib */

/* The way you use mpglib (hip_decode*()) is:
 * - Read in 4096 bytes of MPEG data
 * - Call hip_decode1_headersB() on it. It returns 0.
 * - Read in another 4096 bytes of data and pass them to hip_decode1_headersB()
 * - Repeat until it returns you a non-zero result.
 *   - if it's -1, there was a decoding error
 *   - if it's positive there are samples to be taken from the PCM buffers.
 *     - At this point, enc_delay and enc_padding should have been filled in
 *       (but I'm getting 0 and 0 always) and the contents of mp3data
 *       are almost certainly valid. You can check by seeing if
 *       mp3data.header_parsed is true.
 *     - Copy those samples and call hip_decode1_headers() with a size of 0
 *     - It gives you more samples
 *     - Keep doing that until it returns you zero.
 *     - Read more MPEG data, give it to hip_decode1_headers() and repeat the
 *       above dance until it returns 0 and you don't have any more MPEG data
 *       to give it. At this point your output is complete
 *
 * This gives you a WAV file that, if you trim off the first 529 silent samples,
 * is bitwise identical to the audio data in the output of lame --decode.
 *
 * "lame --decode" itself is more complex: it parses the header itself before
 * calling hip_decode1_headers(), to find out if it's MP1, MP2 or MP3,
 * and is able to decode some files where hip alone fails to parse the header.
 */

/* Start decoding MPEG data. */
int startread_lame(sox_format_t * ft)
{
  priv_t *p = (priv_t *) ft->priv;
  sox_bool ignore_length = ft->signal.length == SOX_IGNORE_LENGTH;
  int openlibrary_result;
  int enc_delay, enc_padding;  /* The values hip_decode1_headersB() fills in */
  size_t skip_start, LSX_UNUSED skip_end; /* What those mean in sample frames */
  int nout;                    /* What hip_decode1_headers() returned */

  LSX_DLLIBRARY_OPEN(
      p,
      lame_dl,
      LAME_FUNC_ENTRIES,
      "LAME decoder library",
      lame_library_names,
      openlibrary_result);
  if (openlibrary_result)
    return SOX_EOF;

  if ((p->gfp = p->lame_init()) == NULL){
    lsx_fail_errno(ft,SOX_EOF,"initialization of LAME library failed");
    return SOX_EOF;
  }
  p->lame_set_errorf(p->gfp,errorf);
  p->lame_set_msgf  (p->gfp,msgf);
  p->lame_set_debugf(p->gfp,debugf);
  lame_set_decode_only(p->gfp, 1);
  if(lame_init_params(p->gfp) == -1) {
    lsx_fail("parameters failed to initialize");
    return SOX_EOF;
  }

  p->hip = p->hip_decode_init();
  p->hip_set_errorf(p->hip, &errorf);
  p->hip_set_msgf  (p->hip, &msgf);
  p->hip_set_debugf(p->hip, &debugf);

  lsx_valloc(p->hip_buffer, HIP_MPEG_BUFSIZ);
  lsx_valloc(p->hip_pcm_l,  HIP_PCM_BUFSIZ);
  lsx_valloc(p->hip_pcm_r,  HIP_PCM_BUFSIZ);
  p->hip_mp3data.nsamp = ULONG_MAX;

  /* Read the file until hip has filled in the mp3data from the headers */
  do {
    size_t nread = lsx_readbuf(ft, p->hip_buffer, HIP_MPEG_BUFSIZ);
    if (nread == 0) {
      /* EOF before decoding the header */
      lsx_fail("file ends before MPEG headers are decoded");
      return SOX_EOF;
    }
    nout = p->hip_decode1_headersB(p->hip, p->hip_buffer, nread,
                                   p->hip_pcm_l, p->hip_pcm_r,
                                   &(p->hip_mp3data),
                                   &enc_delay, &enc_padding);
    if (nout < 0) {
      lsx_fail("decoding error in file header");
      return SOX_EOF;
    }
  } while (nout == 0);
  p->hip_pending = nout;

  if (!p->hip_mp3data.header_parsed) {
    lsx_fail("hip was unable to parse the MPEG header");
    return SOX_EOF;
  }
  lsx_report("decoding %s with samplerate %d",
             mode_string[p->hip_mp3data.mode],
             p->hip_mp3data.samplerate);

  /* LAME finds out if it's MP1, MP2 or MP3 by scanning the header itself
   * before calling hip_*() and this information seems not to be available
   * when using just the hip interface. */
  if (ft->encoding.encoding == SOX_ENCODING_UNKNOWN)
    ft->encoding.encoding = SOX_ENCODING_MP3;
  /*
   * Calculation of enc_delay and enc_padding from
   * lame/frontend/get_audio.c:setSkipStartAndEnd()
   */
  skip_start = 0;
  switch (ft->encoding.encoding) {
  case SOX_ENCODING_MP3:
    if (enc_delay > -1)
      skip_start = enc_delay + 528 + 1;
    if (enc_padding > 528 + 1)
      skip_end = enc_padding - (528 + 1);
    break;
  case SOX_ENCODING_MP2:
  case SOX_ENCODING_MP1:
    skip_start = 240 + 1;
    break;
  default:
    lsx_fail("MPEG file is said to have encoding %u", ft->encoding.encoding);
    return SOX_EOF;
  }

  /* We've never seen enc_padding being significant and enc_delay is always
   * less than the first batch of samples, so just dump them here. */
  if (skip_start > p->hip_pending) {
    lsx_warn("the first %zu silent samples are bogus",
             skip_start - p->hip_pending);
    p->hip_pending = 0;
  } else if (skip_start) {
    size_t n = skip_start;           /* How many samples to discard */
    size_t remaining = p->hip_pending - n;  /* How many to keep */
    size_t i;

    lsx_report("stripping %zd samples from the start", n);

    switch (ft->signal.channels) {
    case 1:
      for (i=0; i<remaining; i++)
        p->hip_pcm_l[i] = p->hip_pcm_l[i + n];
      break;
    case 2:
      for (i=0; i<remaining; i++) {
        p->hip_pcm_l[i] = p->hip_pcm_l[i + n];
        p->hip_pcm_r[i] = p->hip_pcm_r[i + n];
      }
      break;
    }
    p->hip_pending -= n;
  }

  switch (p->hip_mp3data.stereo) {
  case 1: case 2:
    ft->signal.channels = p->hip_mp3data.stereo;
    break;
  default:
    lsx_fail("MPEG file seems to have %d channels", p->hip_mp3data.stereo);
    return SOX_EOF;
  }
  ft->signal.precision = 16; /* LAME decoder always returns short ints */
  ft->signal.rate = p->hip_mp3data.samplerate;

  ft->signal.length = SOX_UNSPEC;
  if (!ignore_length) {
    if (p->hip_mp3data.totalframes > 0) {
      ft->signal.length = p->hip_mp3data.totalframes * ft->signal.channels;
      lsx_report("the file length from totalframes is %g seconds",
                 (double)(ft->signal.length / ft->signal.channels)
                 / ft->signal.rate);
    } else if (p->hip_mp3data.nsamp != ULONG_MAX) {
      ft->signal.length = p->hip_mp3data.nsamp * ft->signal.channels;
      lsx_report("the file length from nsamp is %g seconds",
                 (double)(ft->signal.length / ft->signal.channels)
                 / ft->signal.rate);
    } else if (ft->seekable) {
      /* This seems to get it wrong by one sample */
#if 0
      /* try to figure out num_samples from file size
       * assuming 2 bytes per sample
       */
      double flen = lsx_filelength(ft);
      if (flen >= 0 && p->hip_mp3data.bitrate > 0) {
        double totalseconds =
           (flen * 8.0 / (1000.0 * p->hip_mp3data.bitrate));
        unsigned long tmp_num_samples =
            totalseconds * lame_get_in_samplerate(p->gfp) + .5;

        (void) p->lame_set_num_samples(p->gfp, tmp_num_samples);
        ft->signal.length = tmp_num_samples;
        lsx_warn("Guessed the duration as %g from the file length",
                 (double)(ft->signal.length / ft->signal.channels)
                 / ft->signal.rate);
      }
#endif
    }
  }

  return SOX_SUCCESS;
}

size_t read_lame(sox_format_t *ft, sox_sample_t *buf, size_t samp)
{
  priv_t *p = (priv_t *) ft->priv;
  sox_sample_t *bufp = buf;
  size_t nwritten = 0; /* Number of samples already written into buf */
  SOX_SAMPLE_LOCALS;

  while (nwritten < samp) {
    /* If we still have some decoded data, return that first */
    if (p->hip_pending) {
      /* Number of sample frames to copy */
      size_t n = min(p->hip_pending, (samp - nwritten) / ft->signal.channels);
      size_t i;

      /* If the number of channels changed in mid-stream, compensate */
      if (ft->signal.channels == 1 && p->hip_mp3data.stereo == 2) {
        /* Mix stereo back to mono */
        for (i=0; i < n; i++)
          *bufp++ = SOX_SIGNED_16BIT_TO_SAMPLE(
                     (p->hip_pcm_l[i] + p->hip_pcm_r[i]) / 2, clips);
        nwritten += n;
      } else if (ft->signal.channels == 2 && p->hip_mp3data.stereo == 1) {
        /* Duplicate mono samples to stereo */
        for (i=0; i < n; i++) {
          sox_sample_t s = SOX_SIGNED_16BIT_TO_SAMPLE(p->hip_pcm_l[i], clips);
          *bufp++ = s; *bufp++ = s;
        }
        nwritten += 2 * n;
      } else switch (ft->signal.channels) {
      case 1:
        for (i=0; i < n; i++) {
          *bufp++ = SOX_SIGNED_16BIT_TO_SAMPLE(p->hip_pcm_l[i], clips);
        }
        nwritten += n;
        break;
      case 2:
        for (i=0; i < n; i++) {
          *bufp++ = SOX_SIGNED_16BIT_TO_SAMPLE(p->hip_pcm_l[i], clips);
          *bufp++ = SOX_SIGNED_16BIT_TO_SAMPLE(p->hip_pcm_r[i], clips);
        }
        nwritten += 2 * n;
        break;
      }
      /* If there were more pending samples than requested,
       * move the unused ones to the start of the buffer
       * and adjust the pending count.
       * At this point, hip_pending is the number of samples that were left
       * and n the number we took.
       * We should do this with memmove but I can't seem to get it right.
       */
      if (n < p->hip_pending) {
        size_t remaining = p->hip_pending - n;
        switch (p->hip_mp3data.stereo) {
          size_t i;
        case 1:
          for (i=0; i<remaining; i++)
            p->hip_pcm_l[i] = p->hip_pcm_l[i + n];
          break;
        case 2:
          for (i=0; i<remaining; i++) {
            p->hip_pcm_l[i] = p->hip_pcm_l[i + n];
            p->hip_pcm_r[i] = p->hip_pcm_r[i + n];
          }
          break;
        }
        p->hip_pending = remaining;

        /* Our output buffer is already full */
        return nwritten;
      } else {
        p->hip_pending = 0;
      }
    }

    /* No pending samples; get some more */
    if (nwritten < samp) {
      mp3data_struct mp3data; /* The format of this frame */
      int nout;               /* The value returned by hip_decode1_headers() */

      /* See if it's got any more data for us */
      nout = p->hip_decode1_headers(p->hip, p->hip_buffer, 0,
                                    p->hip_pcm_l, p->hip_pcm_r, &mp3data);

      if (nout < 0) {
        lsx_fail("decoding error");
        return nwritten;
      }
      while (nout == 0) {
        size_t nread = lsx_readbuf(ft, p->hip_buffer, HIP_MPEG_BUFSIZ);
        if (nread == 0) {
          /* No pending samples from hip, no more MPEG data - decoding ends */
          return nwritten;
        }
        nout = p->hip_decode1_headers(p->hip, p->hip_buffer, nread,
                                      p->hip_pcm_l, p->hip_pcm_r, &mp3data);
        if (nout < 0) {
          lsx_fail("decoding error in read_lame");
          return nwritten;
        }

        /* It put samples in pcm_l and pcm_r and filled the header
         * with the parameters for this frame. If they've changed,
         * (some stereo MP3 files contain mono frames) warn.
         */
        /* Changing between DUAL_MONO, STEREO and JOINT_STEREO is not
         * uncommon but harmless */
        if (mp3data.mode != p->hip_mp3data.mode) {
          lsx_warn("MPEG mode changed from %s to %s",
                   mode_string[p->hip_mp3data.mode],
                   mode_string[mp3data.mode]);
          p->hip_mp3data.mode = mp3data.mode;
        }
        if (mp3data.stereo != p->hip_mp3data.stereo) {
          /* Warn when a change happens */
          lsx_warn("MPEG channels changed from %d to %d%s",
                   p->hip_mp3data.stereo, mp3data.stereo,
                   ((unsigned)mp3data.stereo != ft->signal.channels)
                     ? "; compensating..." : "");
          /* This very rarely happens. Rarer instead is when it reports
           * the number of channels changing to 0
           */
          p->hip_mp3data.stereo = mp3data.stereo;
          /* The difference is noticed and stereoified or monized when
           * ft->signal.channels != p->hip_mp3data.stereo
           */
        }
        if (mp3data.samplerate != p->hip_mp3data.samplerate) {
          lsx_warn("MPEG samplerate changed from %d to %d",
                   p->hip_mp3data.samplerate, mp3data.samplerate);
          p->hip_mp3data.samplerate = mp3data.samplerate;
        }
      }
      p->hip_pending = nout;
    }
  }
  return nwritten;
}

int stopread_lame(sox_format_t * ft)
{
  priv_t *p = (priv_t *) ft->priv;

  hip_decode_exit(p->hip);
  free(p->hip_buffer); p->hip_buffer = NULL;
  free(p->hip_pcm_l);  p->hip_pcm_l = NULL;
  free(p->hip_pcm_r);  p->hip_pcm_r = NULL;
  lame_close(p->gfp);
  LSX_DLLIBRARY_CLOSE(p, lame_dl);
  return SOX_SUCCESS;
}

int startwrite_lame(sox_format_t * ft)
{
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

  if (ft->encoding.encoding != SOX_ENCODING_MP3) {
    if(ft->encoding.encoding != SOX_ENCODING_UNKNOWN)
      lsx_report("encoding forced to MP3");
    ft->encoding.encoding = SOX_ENCODING_MP3;
  }

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

  p->mp3_buffer_size = LAME_BUFFER_SIZE(sox_globals.bufsiz / max(ft->signal.channels, 1));
  p->mp3_buffer = lsx_malloc(p->mp3_buffer_size);

  p->pcm_buffer_size = sox_globals.bufsiz * sizeof(float);
  p->pcm_buffer = lsx_malloc(p->pcm_buffer_size);

  return(SOX_SUCCESS);
}

#define MP3_SAMPLE_TO_FLOAT(d,clips) ((float)(32768*SOX_SAMPLE_TO_FLOAT_32BIT(d,clips)))

size_t write_lame(sox_format_t *ft, const sox_sample_t *buf, size_t samp)
{
  priv_t *p = (priv_t *)ft->priv;
  size_t new_buffer_size;
  float *buffer_l, *buffer_r = NULL;
  int nsamples = samp/ft->signal.channels;
  int i, j;
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

  j=0;
  if (ft->signal.channels == 2)
  {
    /* lame doesn't support interleaved samples for floats so we must break
     * them out into seperate buffers.
     */
    buffer_r = p->pcm_buffer + nsamples;
    for (i = 0; i < nsamples; i++)
    {
      buffer_l[i] = MP3_SAMPLE_TO_FLOAT(buf[j++], clips);
      buffer_r[i] = MP3_SAMPLE_TO_FLOAT(buf[j++], clips);
    }
  } else {
    for (i = 0; i < nsamples; i++) {
      buffer_l[i] = MP3_SAMPLE_TO_FLOAT(buf[j++], clips);
    }
  }

  new_buffer_size = LAME_BUFFER_SIZE(nsamples);
  if (p->mp3_buffer_size < new_buffer_size) {
    unsigned char *new_buffer = lsx_realloc(p->mp3_buffer, new_buffer_size);
    p->mp3_buffer_size = new_buffer_size;
    p->mp3_buffer = new_buffer;
  }

  written = p->lame_encode_buffer_float(p->gfp, buffer_l, buffer_r,
            nsamples, p->mp3_buffer, (int)p->mp3_buffer_size);
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

int stopwrite_lame(sox_format_t * ft)
{
  priv_t *p = (priv_t *) ft->priv;
  uint64_t num_samples = ft->olength == SOX_IGNORE_LENGTH ? 0 : ft->olength / max(ft->signal.channels, 1);
  int written = 0;

  written = p->lame_encode_flush(p->gfp, p->mp3_buffer, (int)p->mp3_buffer_size);
  if (written < 0)
    lsx_fail_errno(ft, SOX_EOF, "encoding failed");
  else if (lsx_writebuf(ft, p->mp3_buffer, (size_t)written) < (size_t)written)
    lsx_fail_errno(ft, SOX_EOF, "file write failed");
  else if (!p->mp2) {
    if (ft->seekable && (num_samples != p->num_samples || p->vbr_tag))
      rewrite_tags(ft, num_samples);
  }

  free(p->mp3_buffer);
  free(p->pcm_buffer);

  p->lame_close(p->gfp);
  LSX_DLLIBRARY_CLOSE(p, lame_dl);
  return SOX_SUCCESS;
}

LSX_FORMAT_HANDLER(lame)
{
  static char const * const names[] = {"lame", "mp3", NULL};
  static unsigned const write_encodings[] = {
    SOX_ENCODING_MP3, 0, 0};
  static sox_rate_t const write_rates[] = {
    8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000, 0};
  static sox_format_handler_t const handler = {SOX_LIB_VERSION_CODE,
    "MPEG-1 Layer 3 lossy audio compression", names, 0,
    startread_lame, read_lame, stopread_lame,
    startwrite_lame, write_lame, stopwrite_lame,
    NULL, write_encodings, write_rates, sizeof(priv_t)
  };
  return &handler;
}

#endif /* HAVE_LAME */
