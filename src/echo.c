/* libSoX Echo effect             August 24, 1998
 *
 * Copyright (C) 1998 Juergen Mueller And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained.
 * Juergen Mueller And Sundry Contributors are not responsible for
 * the consequences of using this software.
 */

#include "sox_i.h"


/* This is rubbish because it uses doubles internally */
#define MAX_ECHOS 255   /* 24 bit x ( 1 + MAX_ECHOS ) = */
                        /* 24 bit x 8 = 32 bit !!!      */

/* Private data */
typedef struct {
        int     counter;
        int     num_delays;
        double  *delay_buf;
        float   in_gain, out_gain;
        float   *delay, *decay;
        ptrdiff_t *samples, maxsamples;
        size_t fade_out;
} priv_t;

/*
 * Process options
 */
static int sox_echo_getopts(sox_effect_t * effp, int argc, char **argv)
{
        priv_t * echo = (priv_t *) effp->priv;
        int i;

        echo->num_delays = 0;
        echo->delay = echo->decay = NULL;

        --argc, ++argv;
        if ((argc < 4) || (argc % 2))
          return lsx_usage(effp);

        i = 0;
        if (sscanf(argv[i], "%f", &echo->in_gain) != 1) {
                lsx_fail("gain-in `%s` is not a number", argv[i]);
		return (SOX_EOF);
	}
	i++;
        if (sscanf(argv[i], "%f", &echo->out_gain) != 1) {
                lsx_fail("gain-out `%s` is not a number", argv[i]);
		return (SOX_EOF);
	}
	i++;
        while (i < argc - 1) {
		float delay, decay;

                if (sscanf(argv[i], "%f", &delay) != 1) {
			lsx_fail("delay `%s` is not a number", argv[i]);
			return (SOX_EOF);
		}
		i++;
                if (sscanf(argv[i], "%f", &decay) != 1) {
			lsx_fail("decay `%s` is not a number", argv[i]);
			return (SOX_EOF);
		}
		i++;

                echo->num_delays++;
		echo->delay = lsx_realloc_array(echo->delay, echo->num_delays,
                                                sizeof(*echo->delay));
		echo->decay = lsx_realloc_array(echo->decay, echo->num_delays,
                                                sizeof(*echo->decay));
		echo->delay[echo->num_delays - 1] = delay;
		echo->decay[echo->num_delays - 1] = decay;
        }
	/* This is not true because it uses doubles internally
	if (echo->num_delays >= MAX_ECHOS) {
		lsx_warn("more than %d echos may cause an integer overflow",
			MAX_ECHOS);
	}
	*/
        return (SOX_SUCCESS);
}

/*
 * Prepare for processing.
 */
static int sox_echo_start(sox_effect_t * effp)
{
        priv_t * echo = (priv_t *) effp->priv;
        int i;
        float sum_in_volume;

        echo->maxsamples = 0;
        if ( echo->in_gain < 0.0 )
        {
                lsx_fail("gain-in must be positive!");
                return (SOX_EOF);
        }
        if ( echo->in_gain > 1.0 )
        {
                lsx_fail("gain-in must be less than 1.0!");
                return (SOX_EOF);
        }
        if ( echo->out_gain < 0.0 )
        {
                lsx_fail("gain-out must be positive!");
                return (SOX_EOF);
        }
	echo->samples = lsx_calloc(echo->num_delays, sizeof(*echo->samples));
        for ( i = 0; i < echo->num_delays; i++ ) {
                echo->samples[i] = echo->delay[i] * effp->in_signal.rate / 1000.0;
                if ( echo->samples[i] < 1 )
                {
                    lsx_fail("delay must be positive!");
                    return (SOX_EOF);
                }
                if ( echo->decay[i] < 0.0 )
                {
                    lsx_fail("decay must be positive!" );
                    return (SOX_EOF);
                }
                if ( echo->decay[i] > 1.0 )
                {
                    lsx_fail("decay must be less than 1.0!" );
                    return (SOX_EOF);
                }
                if ( echo->samples[i] > echo->maxsamples )
                        echo->maxsamples = echo->samples[i];
        }
        echo->delay_buf = lsx_calloc(echo->maxsamples,
                                     sizeof(*(echo->delay_buf)));
	/* calloc() sets the memory to zero */
        sum_in_volume = echo->in_gain;
        for ( i = 0; i < echo->num_delays; i++ )
                sum_in_volume += echo->decay[i];
        if ( sum_in_volume * echo->out_gain > 1.0 )
                lsx_warn("the output may saturate; a safe gain-out is %g",
		         1.0 / sum_in_volume);
        echo->counter = 0;
        echo->fade_out = echo->maxsamples;

  effp->out_signal.length = SOX_UNKNOWN_LEN; /* TODO: calculate actual length */

        return (SOX_SUCCESS);
}

/*
 * Processed signed long samples from ibuf to obuf.
 * Return number of samples processed.
 */
static int sox_echo_flow(sox_effect_t * effp, const sox_sample_t *ibuf, sox_sample_t *obuf,
                 size_t *isamp, size_t *osamp)
{
        priv_t * echo = (priv_t *) effp->priv;
        int j;
        double d_in, d_out;
        sox_sample_t out;
        size_t len = min(*isamp, *osamp);
        *isamp = *osamp = len;

        while (len--) {
                /* Store delays as 24-bit signed longs */
                d_in = (double) *ibuf++ / 256;
                /* Compute output first */
                d_out = d_in * echo->in_gain;
                for ( j = 0; j < echo->num_delays; j++ ) {
                        d_out += echo->delay_buf[
(echo->counter + echo->maxsamples - echo->samples[j]) % echo->maxsamples]
                        * echo->decay[j];
                }
                /* Adjust the output volume and size to 24 bit */
                d_out = d_out * echo->out_gain;
                out = SOX_24BIT_CLIP_COUNT((sox_sample_t) d_out, effp->clips);
                *obuf++ = out * 256;
                /* Store input in delay buffer */
                echo->delay_buf[echo->counter] = d_in;
                /* Adjust the counter */
                echo->counter = ( echo->counter + 1 ) % echo->maxsamples;
        }
        /* processed all samples */
        return (SOX_SUCCESS);
}

/*
 * Drain out reverb lines.
 */
static int sox_echo_drain(sox_effect_t * effp, sox_sample_t *obuf, size_t *osamp)
{
        priv_t * echo = (priv_t *) effp->priv;
        double d_in, d_out;
        sox_sample_t out;
        int j;
        size_t done;

        done = 0;
        /* drain out delay samples */
        while ( ( done < *osamp ) && ( done < echo->fade_out ) ) {
                d_in = 0;
                d_out = 0;
                for ( j = 0; j < echo->num_delays; j++ ) {
                        d_out += echo->delay_buf[
(echo->counter + echo->maxsamples - echo->samples[j]) % echo->maxsamples]
                        * echo->decay[j];
                }
                /* Adjust the output volume and size to 24 bit */
                d_out = d_out * echo->out_gain;
                out = SOX_24BIT_CLIP_COUNT((sox_sample_t) d_out, effp->clips);
                *obuf++ = out * 256;
                /* Store input in delay buffer */
                echo->delay_buf[echo->counter] = d_in;
                /* Adjust the counters */
                echo->counter = ( echo->counter + 1 ) % echo->maxsamples;
                done++;
                echo->fade_out--;
        };
        /* samples played, it remains */
        *osamp = done;
        if (echo->fade_out == 0)
            return SOX_EOF;
        else
            return SOX_SUCCESS;
}

static int sox_echo_stop(sox_effect_t * effp)
{
        priv_t * echo = (priv_t *) effp->priv;

        free(echo->delay);
        free(echo->decay);
        free(echo->samples);
        free(echo->delay_buf);
        echo->delay_buf = NULL;
        return (SOX_SUCCESS);
}

const sox_effect_handler_t *lsx_echo_effect_fn(void)
{
  static char usage[] = "gain-in gain-out <delay decay>\n\
                                 ___\n\
  In--+------------------------>|   |\n\
      |    _________  * gain-in |   |\n\
      |   |         |           |   |\n\
      +-->| delay 1 |---------->|   |\n\
      |   |_________| * decay 1 |   |\n\
      |    _________            | + |------------>Out\n\
      |   |         |           |   | * gain-out\n\
      +-->| delay 2 |---------->|   |\n\
      |   |_________| * decay 2 |   |\n\
      :    _________            |   |\n\
      |   |         |           |   |\n\
      +-->| delay n |---------->|___|\n\
          |_________| * decay n\n\
\n\
         RANGE  DESCRIPTION\n\
gain-in   0-1   Proportion of input signal delivered clean to adder\n\
gain-out  0-1   Final volume adjustment\n\
delay     0-    Delay in milliseconds\n\
decay     0-1   Proportion of delayed signal delivered to adder";

  static sox_effect_handler_t handler = {
    "echo", usage, SOX_EFF_LENGTH | SOX_EFF_GAIN,
    sox_echo_getopts,
    sox_echo_start,
    sox_echo_flow,
    sox_echo_drain,
    sox_echo_stop,
    NULL, sizeof(priv_t)
  };

  return &handler;
}
