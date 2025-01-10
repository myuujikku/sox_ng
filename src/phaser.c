/* SoX flanger effect
 * Copyright (C) 24 August 1998, Juergen Mueller And Sundry Contributors
 *
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained.
 * Juergen Mueller And Sundry Contributors are not responsible for
 * the consequences of using this software.
 */

#include "sox_i.h"

typedef struct {
  double     in_gain, out_gain, delay_ms, decay, mod_speed;
  lsx_wave_t mod_type;

  int        * mod_buf;
  size_t     mod_buf_len;
  int        mod_pos;
            
  double     * delay_buf;
  size_t     delay_buf_len;
  int        delay_pos;
} priv_t;

static int getopts(sox_effect_t * effp, int argc, char * * argv)
{
  priv_t * p = (priv_t *) effp->priv;
  char chars[2];

  /* Set non-zero defaults: */
  p->in_gain   = .4;
  p->out_gain  = .74;
  p->delay_ms  = 3.;
  p->decay     = .4;
  p->mod_speed = .5;

  --argc, ++argv;
  do { /* break-able block */
    NUMERIC_PARAMETER(in_gain  , .0, 1)
    NUMERIC_PARAMETER(out_gain , .0, 1e9)
    NUMERIC_PARAMETER(delay_ms , .0, 5)
    NUMERIC_PARAMETER(decay    , .0, .99)
    NUMERIC_PARAMETER(mod_speed, .1, 2)
  } while (0);

  if (argc && sscanf(*argv, "-%1[st]%c", chars, chars + 1) == 1) {
    p->mod_type = *chars == 's'? SOX_WAVE_SINE : SOX_WAVE_TRIANGLE;
    --argc, ++argv;
  }

  if (p->in_gain > (1 - p->decay * p->decay))
    lsx_warn("warning: gain-in might cause clipping");
  if (p->in_gain / (1 - p->decay) > 1 / p->out_gain)
    lsx_warn("warning: gain-out might cause clipping");

  return argc? lsx_usage(effp) : SOX_SUCCESS;
}

static int start(sox_effect_t * effp)
{
  priv_t * p = (priv_t *) effp->priv;

  p->delay_buf_len = p->delay_ms * .001 * effp->in_signal.rate + .5;
  p->delay_buf = lsx_calloc(p->delay_buf_len, sizeof(*p->delay_buf));

  p->mod_buf_len = effp->in_signal.rate / p->mod_speed + .5;
  p->mod_buf = lsx_malloc(p->mod_buf_len * sizeof(*p->mod_buf));
  lsx_generate_wave_table(p->mod_type, SOX_INT, p->mod_buf, p->mod_buf_len,
      1., (double)p->delay_buf_len, M_PI_2);

  p->delay_pos = p->mod_pos = 0;

  effp->out_signal.length = SOX_UNKNOWN_LEN; /* TODO: calculate actual length */
  return SOX_SUCCESS;
}

static int flow(sox_effect_t * effp, const sox_sample_t *ibuf,
    sox_sample_t *obuf, size_t *isamp, size_t *osamp)
{
  priv_t * p = (priv_t *) effp->priv;
  size_t len = *isamp = *osamp = min(*isamp, *osamp);

  while (len--) {
    double d = *ibuf++ * p->in_gain + p->delay_buf[
      (p->delay_pos + p->mod_buf[p->mod_pos]) % p->delay_buf_len] * p->decay;
    p->mod_pos = (p->mod_pos + 1) % p->mod_buf_len;
    
    p->delay_pos = (p->delay_pos + 1) % p->delay_buf_len;
    p->delay_buf[p->delay_pos] = d;

    *obuf++ = SOX_ROUND_CLIP_COUNT(d * p->out_gain, effp->clips);
  }
  return SOX_SUCCESS;
}

static int stop(sox_effect_t * effp)
{
  priv_t * p = (priv_t *) effp->priv;

  free(p->delay_buf);
  free(p->mod_buf);
  return SOX_SUCCESS;
}

sox_effect_handler_t const * lsx_phaser_effect_fn(void)
{
  static char usage[] = "\
gain-in gain-out delay decay speed -s|-t\n\
\n\
     * gain-in  ___  * gain-out\n\
  In---------->|   |-----------> Out\n\
               | + | * decay\n\
               |___|<-----------+\n\
                 |    _______   |\n\
                 |   |       |  |\n\
                 +-->| delay |--+\n\
                     |_______|\n\
                         ^\n\
                         |\n\
                 +---------------+\n\
         speed-->| sine/triangle |\n\
                 +---------------+\n\
\n\
           RANGE  DEFAULT  DESCRIPTION\n\
  gain-in   0-1     0.4    Proportion of input delivered to output and delay\n\
  decay     0-1     0.4    Proportion of delay that is fed back\n\
  gain-out  0-      0.74   Final output volume adjustment\n\
  delay     0-5     3.0    Delay in milliseconds\n\
  speed   0.1-2     0.5    Modulation speed in Hz\n\
  -s                       Sinusoidal modulation\n\
  -t                       Triangular modulation\n\
\n\
Hint:  in-gain < (1 - decay * decay)\n\
      out-gain < (1 - decay) / gain-in";

  static sox_effect_handler_t handler = {
    "phaser", usage, SOX_EFF_LENGTH | SOX_EFF_GAIN, getopts,
    start, flow, NULL, stop, NULL, sizeof(priv_t)
  };

  return &handler;
}
