/* August 24, 1998
 * Copyright (C) 1998 Juergen Mueller And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained.
 * Juergen Mueller And Sundry Contributors are not responsible for
 * the consequences of using this software.
 *
 * minor updates: Martin W. Guy, Dr. Thomas Tensi, 2025-01
 */

/*
 * Chorus effect.
 *
 * See the flow diagram in the usage message at the end of the file.
 *
 * In reality, it only has one delay buffer, whose size is the longest version
 * of (delay + depth) and each of the apparent delays reads from that,
 *
 * The delay i is controlled by a sine or triangle modulation i ( 1 <= i <= n).
 *
 * Usage:
 *   chorus gain-in gain-out delay-1 decay-1 speed-1 depth-1 -s1|t1 [
 *       delay-2 decay-2 speed-2 depth-2 -s2|-t2 ... ]
 *
 * Where:
 *   gain-in, decay-1 ... decay-n :  0.0 ... 1.0      volume
 *   gain-out :  0.0 ...      volume
 *   delay-1 ... delay-n :  20.0 ... 100.0 msec
 *   speed-1 ... speed-n :  0.1 ... 5.0 Hz       modulation 1 ... n
 *   depth-1 ... depth-n :  0.0 ... 10.0 msec    modulated delay 1 ... n
 *   -s1 ... -sn : modulation by sine 1 ... n
 *   -t1 ... -tn : modulation by triangle 1 ... n
 *
 * Note:
 *   when decay is close to 1.0, the samples can begin clipping and the output
 *   can saturate!
 *
 * Hint:
 *   1 / out-gain < gain-in ( 1 + decay-1 + ... + decay-n )
 *
 */

/*
 * libSoX chorus effect
 */

#include "sox_i.h"

/** the maximum number of stages in a chorus */
#define MAX_STAGE_COUNT  7

/** the number of parameter for a chorus stage */
#define PARAM_COUNT_PER_STAGE 5

/** the number of global chorus parameters */
#define FIXED_PARAM_COUNT    2

/** the downscaling factor for the samples in the delay line (to
 * prevent overflow); must be a power of 2 */
#define SCALING_FACTOR 256

/** the function for checking for a clipped sample in the resolution
 * after downscaling */
#define CLIP_COUNT_PROC SOX_24BIT_CLIP_COUNT
 
/** the allowed options for the modulation kind mapped onto a wave
 * type */
static lsx_enum_item modulation_kind_map[] ={
    { "-s", SOX_WAVE_SINE },
    { "-t", SOX_WAVE_TRIANGLE },
    { NULL, 0 }
};

/** the type used in the delay lines */
typedef sox_sample_t chorus_delay_sample_t;

/** an auxiliary macro for doing a modular increment */
#define MODULAR_INCREMENT(a, b)     a = ((a) + 1) % (b)

/*--------------------*/

/** a single stage in the chorus effect */
typedef struct {
        /* command line parameters */
        float                  delay;       /* in seconds */
        float                  decay;
        float                  speed;       /* in Hz */
        float                  depth;       /* in seconds */
        lsx_wave_t             wave_type;
        /* sample tables, table counts and indices */
        sox_uint64_t           delay_line_index;
        sox_uint64_t           delay_line_length;
        chorus_delay_sample_t  *delay_line;
        sox_uint64_t           depth_sample_count;
        sox_uint64_t           wave_index;
        sox_uint64_t           wave_length;
        int                    *wave_table;
} chorus_stage_t;

/*--------------------*/

/** the chorus effect */
typedef struct {
        /* command line parameters */
        float           gain_in;
        float           gain_out;

        /* all stages of that effect */
        sox_uint64_t    stage_count;
        chorus_stage_t  stage[MAX_STAGE_COUNT];

        /* remaining samples for drain phase */
        sox_uint64_t    remaining_samples;
} chorus_priv_t;

/*--------------------*/

/**
 * Process command line options for this effect <C>effp</C> given by
 * <C>argc</C> and <C>argv</C>.
 *
 * @param effp  chorus effect affected by parameters
 * @param argc  count of parameters (including effect name)
 * @param argv  list of string parameters
 * @return  information whether parameter collection has been
 *          successful
 */
static int sox_chorus_getopts (sox_effect_t *effp,
                               int argc,
                               char **argv)
{
        chorus_priv_t *chorus = (chorus_priv_t *) effp->priv;
        sox_uint64_t i;
        float total_volume;

        /* skip over effect name */
        argc--;
        argv++;

        /* there must be at least one stage and at most
         * MAX_STAGE_COUNT and all stages must have parameters */
        if ((argc < FIXED_PARAM_COUNT + PARAM_COUNT_PER_STAGE)
            || ((argc - FIXED_PARAM_COUNT) % PARAM_COUNT_PER_STAGE != 0)
            || (argc > (MAX_STAGE_COUNT * PARAM_COUNT_PER_STAGE
                        + FIXED_PARAM_COUNT))) {
            return lsx_usage(effp);
        }

        /* read the global parameters gain_in and gain_out */
        do {
            chorus_priv_t* p = chorus;
            NUMERIC_PARAMETER(gain_in, 0.0, 1.0);
            NUMERIC_PARAMETER(gain_out, 0.0, 1.0);
        } while (0);

        /* read all stages */
        chorus->stage_count = 0;

        do {
            chorus_stage_t *p = &chorus->stage[chorus->stage_count];
            NUMERIC_PARAMETER(delay, 20.0, 100.0);
            NUMERIC_PARAMETER(decay,  0.0,   1.0);
            NUMERIC_PARAMETER(speed,  0.1,   5.0);
            NUMERIC_PARAMETER(depth,  0.0,  10.0);
            TEXTUAL_PARAMETER(wave_type, modulation_kind_map);
            /* normalize time parameters to seconds */
            p->delay /= 1000.0;
            p->depth /= 1000.0;
            chorus->stage_count++;
        } while (chorus->stage_count < MAX_STAGE_COUNT);

        /* issue warning about possible clipping when parameters are
         * above some threshold */
        total_volume = 1.0;

        for (i = 0;  i < chorus->stage_count;  i++) {
            total_volume += chorus->stage[i].decay;
        }

        if (chorus->gain_in * total_volume > 1.0 / chorus->gain_out) {
            lsx_warn("gain-out can cause saturation or clipping of output");
        }

        return (SOX_SUCCESS);
}

/*--------------------*/

/**
 * Prepare effect <C>effp</C> for processing.
 *
 * @param effp  chorus effect about to be started
 * @return  information whether start was successful
  */
static int sox_chorus_start (sox_effect_t *effp)
{
        chorus_priv_t *chorus = (chorus_priv_t *) effp->priv;
        sox_uint64_t i;

        chorus->remaining_samples = 0;

        for (i = 0;  i < chorus->stage_count;  i++) {
                chorus_stage_t *stage = &chorus->stage[i];
                stage->depth_sample_count =
                    stage->depth * effp->in_signal.rate;

                /* delay line */
                stage->delay_line_index = 0;
                stage->delay_line_length =
                    (stage->delay + stage->depth) * effp->in_signal.rate;
                stage->delay_line =
                    lsx_calloc(stage->delay_line_length,
                               sizeof(chorus_delay_sample_t));

                /* modulation wave table */
                stage->wave_index = 0;
                stage->wave_length = effp->in_signal.rate / stage->speed;
                stage->wave_table =
                    lsx_malloc(stage->wave_length * sizeof(int));
                lsx_generate_wave_table(stage->wave_type, SOX_INT,
                                        stage->wave_table,
                                        stage->wave_length,
                                        0., stage->depth_sample_count,
                                        M_PI_2);
 
                /* find maximum delay line length across all stages */
                chorus->remaining_samples =
                        max(chorus->remaining_samples,
                             stage->delay_line_length);
        }

        effp->out_signal.length = SOX_UNKNOWN_LEN;
        /* TODO: calculate actual length */

        return (SOX_SUCCESS);
}

/*--------------------*/

/**
 * Apply effect <C>effp</C> on samples from input sample buffer
 * <C>ibuf</C> to output sample buffer <C>obuf</C> with sample counts
 * given by <C>isamp</C> and <C>osamp</C>.  When <C>ibuf</C> is NULL,
 * the drain phase must be processed and input is assumed to be 0.
 *
 * @param effp   chorus effect processing the samples
 * @param ibuf   sample buffer containing input samples (may be NULL)
 * @param obuf   target sample buffer for the output samples
 * @param isamp  count of samples in the input buffer or, when ibuf is
 *               NULL, the count of samples to be drained (to be updated)
 * @param osamp  count of sample slots in the output buffer (to be
 *               updated)
 * @return  information whether process has been successful
  */
static int sox_chorus_flow_or_drain (sox_effect_t *effp,
                                     const sox_sample_t *ibuf,
                                     sox_sample_t *obuf,
                                     size_t *isamp,
                                     size_t *osamp)
{
        chorus_priv_t *chorus = (chorus_priv_t *) effp->priv;
        const sox_bool is_drain = (ibuf == NULL);
        size_t len = min(*isamp, *osamp);
        int result = (!is_drain
                      ? SOX_SUCCESS
                      : (*isamp > *osamp ? SOX_SUCCESS : SOX_EOF));

        *isamp = *osamp = len;

        while (len--) {
                sox_uint64_t i;
                sox_sample_t output_sample;

                /* Scale samples down to prevent arithmetic overflow
                 * when adding up many delay lines */
                const chorus_delay_sample_t d_in =
                    (is_drain
                     ? 0.0
                     : (chorus_delay_sample_t) *ibuf++ / SCALING_FACTOR);

                /* Compute output */
                chorus_delay_sample_t d_out = (float) d_in * chorus->gain_in;

                for (i = 0; i < chorus->stage_count; i++) {
                    chorus_stage_t *stage = &chorus->stage[i];
                    sox_uint64_t wave_index = stage->wave_index;
                    sox_uint64_t offset = stage->wave_table[wave_index];
                    sox_uint64_t delay_line_index =
                        ((stage->delay_line_index + offset)
                         % stage->delay_line_length);
                    chorus_delay_sample_t sample =
                        stage->delay_line[delay_line_index];
                    d_out += sample * stage->decay;
                    stage->delay_line[stage->delay_line_index] = d_in;
                    MODULAR_INCREMENT(stage->delay_line_index,
                                      stage->delay_line_length);
                    MODULAR_INCREMENT(stage->wave_index, stage->wave_length);
                }

                /* Adjust the output volume by gain_out, check for
                 * clipping and scale output up again */
                d_out = d_out * chorus->gain_out;
                output_sample = CLIP_COUNT_PROC((sox_sample_t) d_out,
                                                effp->clips);
                *obuf++ = output_sample * SCALING_FACTOR;
        }

        return result;
}

/*--------------------*/

/**
 * Apply effect <C>effp</C> on samples from input sample buffer
 * <C>ibuf</C> to output sample buffer <C>obuf</C> with sample counts
 * given by <C>isamp</C> and <C>osamp</C>.
 *
 * @param effp   chorus effect processing the samples
 * @param ibuf   sample buffer containing input samples
 * @param obuf   target sample buffer for the output samples
 * @param isamp  count of samples in the input buffer (to be updated)
 * @param osamp  count of sample slots in the output buffer (to be
 *               updated)
 * @return number of samples processed
 */
static int sox_chorus_flow (sox_effect_t *effp,
                            const sox_sample_t *ibuf,
                            sox_sample_t *obuf,
                            size_t *isamp,
                            size_t *osamp)
{
    return sox_chorus_flow_or_drain(effp, ibuf, obuf, isamp, osamp);
}

/*--------------------*/

/**
 * Drain out effect delay lines for effect <C>effp</C> to output
 * sample buffer <C>obuf</C> with sample count given by <C>osamp</C>.
 *
 * @param effp   chorus effect processing the samples
 * @param obuf   target sample buffer for the output samples
 * @param osamp  count of sample slots in the output buffer (to be
 *               updated)
 * @return number of samples processed
 */
static int sox_chorus_drain (sox_effect_t * effp,
                             sox_sample_t *obuf,
                             size_t *osamp)
{
    chorus_priv_t *chorus = (chorus_priv_t *) effp->priv;
    int result = SOX_EOF;

    if (chorus->remaining_samples > 0) {
        size_t dummy = chorus->remaining_samples;
        result = sox_chorus_flow_or_drain(effp, NULL, obuf,
                                          &dummy, osamp);
        chorus->remaining_samples -= dummy;
    }

    return result;
}

/*--------------------*/

/**
 * Clean up chorus effect <C>effp</C>.
 *
 * @param effp   chorus effect to be cleaned up
 */
static int sox_chorus_stop (sox_effect_t * effp)
{
        chorus_priv_t * chorus = (chorus_priv_t *) effp->priv;
        sox_uint64_t i;

        for (i = 0;  i < chorus->stage_count;  i++) {
                chorus_stage_t *stage = &chorus->stage[i];
                free(stage->wave_table);
                free(stage->delay_line);
         }

        memset(chorus, 0, sizeof(*chorus));

        return (SOX_SUCCESS);
}

const sox_effect_handler_t *lsx_chorus_effect_fn(void)
{
  static char const usage[] =
"gain-in gain-out <delay decay speed depth -s|-t>";
  static char const * const extra_usage[] = {
"                                              ___",
"In---+-------------------------------------->|   |",
"     |     _________              * gain-in  |   |",
"     |    |         |                        |   |",
"     +--->| delay 1 |----------------------->|   |",
"     |    |_________|             * decay 1  |   |",
"     |         ^                             |   |",
"     :         | * depth 1                   |   |",
"     : +---------------+                     | + |------------>Out",
"     : | sine/triangle |<--speed 1           |   | * gain-out",
"     : +---------------+                     |   |",
"     |     _________                         |   |",
"     |    |         |                        |   |",
"     +--->| delay n |----------------------->|   |",
"          |_________|              * decay n |   |",
"               ^                             |___|",
"               | * depth n",
"       +---------------+",
"       | sine/triangle |<--speed n",
"       +---------------+",
"",
"         RANGE   DESCRIPTION",
"gain-in   0-1    Proportion of input delivered clean to adder",
"gain-out  0-     Final volume adjustment",
"delay    20-100  Fixed delay in milliseconds",
"decay     0-1    Proportion of delay's output delivered to adder",
"speed   0.1-5    Modulation frequency in Hz",
"depth     0-10   Additional variable delay in milliseconds",
"-s               Modulate sinusoidally",
"-t               Modulate triangularly",
          NULL
	};

        static sox_effect_handler_t sox_chorus_effect = {
                "chorus",
                usage, extra_usage,
                SOX_EFF_LENGTH | SOX_EFF_GAIN,
                sox_chorus_getopts,
                sox_chorus_start,
                sox_chorus_flow,
                sox_chorus_drain,
                sox_chorus_stop,
                NULL,
                sizeof(chorus_priv_t)
        };

        return &sox_chorus_effect;
}
