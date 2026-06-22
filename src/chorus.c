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
 * In reality, it only has one delay buffer, whose size is the longest
 * version of (delay + depth) and each of the apparent delays reads
 * from that,
 *
 * The delay i is controlled by a sine or triangle modulation i ( 1 <=
 * i <= n).
 */

/*
 * libSoX chorus effect
 */

/*============================================================*/

#include "sox_i.h"
#include "eff_parameters.h"

/*============================================================*/

/** the downscaling factor for the samples in the delay line
 * to prevent overflow; best if it's a power of 2 */
#define SCALING_FACTOR 256

/** the maximum number of stages in a chorus, because of
 * SCALING_FACTOR */
#define MAX_STAGE_COUNT SCALING_FACTOR

/** the allowed options for the modulation kind mapped onto a wave
 * type */
static lsx_enum_item modulation_kind_map[] ={
    { "-sine",     SOX_WAVE_SINE },
    { "-triangle", SOX_WAVE_TRIANGLE },
    { NULL, 0 }
};

/** the allowed interpolation types, mirroring those of flanger */
typedef enum {INTERP_NONE, INTERP_LINEAR, INTERP_QUADRATIC} interp_t;

/** the type used in the delay lines */
typedef sox_sample_t chorus_delay_sample_t;

/** an auxiliary macro for doing a modular increment */
#define MODULAR_INCREMENT(a, b)     a = ((a) + 1) % (b)

/** an auxiliary macro for accessing delay line at position i */
#define DELAY_LINE_AT(i)  delay_line[((i) + stage->delay_line_length) \
                                     % stage->delay_line_length]

/*--------------------*/

/** channel specific data for a single stage */
typedef struct {
    chorus_delay_sample_t  *delay_line;
} chorus_stage_channel_t;

/*--------------------*/

/** the data for a single stage in the chorus effect */
typedef struct {
    /* command line parameters */
    float                    delay;       /* in seconds */
    float                    decay;
    float                    speed;       /* in Hz */
    float                    depth;       /* in seconds */
    lsx_wave_t               wave_type;

    /* sample tables, table counts and indices */
    sox_uint32_t             depth_sample_count;
    sox_uint32_t             wave_length;
    sox_uint32_t             wave_index;
    sox_uint32_t*            wave_table_i;
    float*                   wave_table_f;
    sox_uint32_t             delay_line_length;
    sox_uint32_t             delay_line_index;
    chorus_stage_channel_t*  channel_data;
} chorus_stage_t;

/*--------------------*/

/** the chorus effect */
typedef struct {
    /* command line parameters */
    interp_t        interpolation;
    float           gain_in;
    float           gain_out;
    sox_bool        has_integer_wave_tables;

    /* all stages of that effect */
    sox_uint32_t    stage_count;
    chorus_stage_t  *stage_list;

    /* remaining samples for drain phase */
    sox_uint32_t    remaining_samples;
} chorus_priv_t;

/*====================*/

/**
 * Rounds <C>r</C> to <C>digitCount</C> fractional digits precision.
 *
 * @param[in] r           real value to be rounded
 * @param[in] digitCount  number of fractional digits to be kept
 * @return real value rounded to <C>digitCount</C> fractional digits
 */
static inline float roundToPrecision (float r,
                                      size_t digitCount)
{
    float factor = pow(10.0, digitCount);
    return copysign(floor(abs(r) * factor + 0.5) / factor, r);
}

/*--------------------*/

/**
 * Process command line options for this effect <C>effp</C> given by
 * <C>parameter_list</C>.
 *
 * @param[inout]  effp            chorus effect affected by parameters
 * @param[inout]  parameter_list  list of parameters (including effect
 *                                name)
 * @return  information whether parameter collection has been
 *          successful
 */
static int read_parameters (sox_effect_t *effp,
                            sox_effparameter_list_t *parameter_list)
{
    chorus_priv_t *chorus = (chorus_priv_t *) effp->priv;
    lsx_wave_t default_wave = SOX_WAVE_SINE;
    sox_effparameter_scan_result scan_result;
    size_t temp_enum;

    /* skip over effect name */
    effparameter_list_advance(parameter_list);

    while (!effparameter_list_is_empty(parameter_list)
           && effparameter_is_flag(parameter_list)) {
        switch (effparameter_flag_name(parameter_list)) {
            case 'n':
                chorus->interpolation = INTERP_NONE;
                break;
            case 'l':
                chorus->interpolation = INTERP_LINEAR;
                break;
            case 'q':
                chorus->interpolation = INTERP_QUADRATIC;
                break;
            case 's':
                default_wave = SOX_WAVE_SINE;
                break;
            case 't':
                default_wave = SOX_WAVE_TRIANGLE;
                break;
            default:
                lsx_fail("invalid option  '%s'",
                         effparameter_current(parameter_list));
                return lsx_usage(effp);
        }

        effparameter_list_advance(parameter_list);
    }
    

    chorus->has_integer_wave_tables =
        (chorus->interpolation == INTERP_NONE);

    /* read the global parameters gain_in and gain_out */
    chorus->gain_in = 0.5;
    chorus->gain_out = 1;

    do {
        scan_result =
            effparameter_scan_number_f(parameter_list,
                                       &chorus->gain_in, "gain_in",
                                       -1.0, 1.0);
        HANDLE_SCAN_RESULT(scan_result);
        scan_result =
            effparameter_scan_number_f(parameter_list,
                                       &chorus->gain_out, "gain_out",
                                       -1.0, 1.0);
        HANDLE_SCAN_RESULT(scan_result);
    } while (0);

    /* read all stages */
    chorus->stage_count = 0;
    chorus->stage_list  = NULL;

    do {
        chorus_stage_t *stage;

        lsx_revalloc(chorus->stage_list, chorus->stage_count + 1);
        stage = &chorus->stage_list[chorus->stage_count];
        memset(stage, 0, sizeof(*stage));

        stage->delay     = 50;
        stage->decay     = 0.5;
        stage->speed     = 0.25;
        stage->depth     = 2;
        stage->wave_type = default_wave;

        do {
            scan_result =
                effparameter_scan_number_f(parameter_list,
                                           &stage->delay, "delay",
                                           0, 86400000);
            HANDLE_SCAN_RESULT(scan_result);
            scan_result =
                effparameter_scan_number_f(parameter_list,
                                        &stage->decay, "decay",
                                        -1, 1);
            HANDLE_SCAN_RESULT(scan_result);
            scan_result =
                effparameter_scan_number_f(parameter_list,
                                           &stage->speed, "speed",
                                           0, 192000);
            HANDLE_SCAN_RESULT(scan_result);
            scan_result =
                effparameter_scan_number_f(parameter_list,
                                           &stage->depth, "depth",
                                           0, 86400000);
            HANDLE_SCAN_RESULT(scan_result);
            scan_result =
                effparameter_scan_enum(parameter_list,
                                       &temp_enum, "wave_type",
                                       modulation_kind_map);
            HANDLE_SCAN_RESULT(scan_result);
            stage->wave_type = (lsx_wave_t) temp_enum;
        } while (0);

        /* normalize time parameters to seconds */
        stage->delay /= 1000.0;
        stage->depth /= 1000.0;

        chorus->stage_count++;
    } while (!effparameter_list_is_empty(parameter_list)
             && chorus->stage_count < MAX_STAGE_COUNT);

    return SOX_SUCCESS;
}

/*--------------------*/

/**
 * Postprocesses handler part of effect <C>effp</C> after the
 * parameters have been read.
 *
 * @param[inout]  effp  chorus effect
 * @return  information whether derived initialization has been
 *          successful
 */
static int setup_derived_data (sox_effect_t *effp)
{
    chorus_priv_t *chorus = (chorus_priv_t *) effp->priv;
    const size_t channel_count = effp->in_signal.channels;
    const sox_bool has_integer_wave_tables =
        chorus->has_integer_wave_tables;
    const sox_data_t wave_table_kind =
        (has_integer_wave_tables ? SOX_INT : SOX_FLOAT);
    const size_t wave_table_element_size =
        (has_integer_wave_tables ? sizeof(int) : sizeof(float));
    size_t i;

    /* traverse all stages and initialize the wave tables and the
     * placeholder for the channel data, but do not yet allocate the
     * channel specific data */
    for (i = 0;  i < chorus->stage_count;  i++) {
        chorus_stage_t *stage = &chorus->stage_list[i];
        float wave_table_length;
        double dll;

        stage->depth_sample_count = stage->depth * effp->in_signal.rate;
        wave_table_length =
            (stage->delay + stage->depth) * effp->in_signal.rate;

        /* get rid of any floating point noise before calculating the
           delay line dimensions */
        dll = ceil(roundToPrecision(wave_table_length, 4));

        if (dll > SOX_UINT_MAX(32)) {
            lsx_fail("delay + depth can't be more than"
                     " %.0f ms at sample rate %.0fHz",
                     SOX_UINT_MAX(32) / effp->in_signal.rate * 1000,
                     effp->in_signal.rate);
            lsx_fail("lower sr to increase the maximum depth of the delay");
            return SOX_EOF;
        } else if (dll < 1) {
            lsx_fail("delay can't be less than %g milliseconds",
                     1000 / effp->in_signal.rate);
            return SOX_EOF;
        }

        /* add some slack to ensure that for maximum modulation the
         * delay line does not read bad data */
        stage->delay_line_length = dll + (double) chorus->interpolation;

        /* setup modulation wave table */
        stage->wave_length = effp->in_signal.rate / stage->speed + 0.5;

        if (stage->wave_length < 1) {
            lsx_fail("speed can't be more than the sample rate");
            return SOX_EOF;
        }

        void* wave_table =
            lsx_calloc(stage->wave_length, wave_table_element_size);

        if (has_integer_wave_tables) {
            stage->wave_table_i = (int *) wave_table;
            stage->wave_table_f = NULL;
        } else {
            stage->wave_table_f = (float *) wave_table;
            stage->wave_table_i = NULL;
        }

        const double maxWaveValue = stage->depth_sample_count;
        const double minWaveValue = -maxWaveValue;

        lsx_generate_wave_table(stage->wave_type, wave_table_kind,
                                wave_table,
                                stage->wave_length,
                                minWaveValue, maxWaveValue, 3 * M_PI_2);

        /* create channel data array, but do not yet fill it */
        lsx_valloc(stage->channel_data, channel_count);

        /* find maximum delay line length across all stages */
        chorus->remaining_samples =
            max(chorus->remaining_samples, stage->delay_line_length);
    }

    effp->out_signal.length = effp->in_signal.length;
    if (effp->out_signal.length != SOX_UNKNOWN_LEN)
        effp->out_signal.length += chorus->remaining_samples;

    return SOX_SUCCESS;
}

/*--------------------*/

/**
 * Process command line options for this effect <C>effp</C> given by
 * <C>argc</C> and <C>argv</C> and initializes effect accordingly.
 *
 * @param effp  chorus effect affected by parameters
 * @param argc  count of parameters (including effect name)
 * @param argv  list of string parameters
 * @return      information whether parameter collection and
 *              initialization has been successful
 */
static int chorus_getopts (sox_effect_t* effp,
                           int argc,
                           char** argv)
{
    chorus_priv_t *chorus = (chorus_priv_t *) effp->priv;
    sox_uint32_t i;
    float total_volume;
    int result;
    sox_effparameter_list_t parameter_list;

    effparameter_list_init(&parameter_list, argc, argv);
    result = read_parameters(effp, &parameter_list);

    if (result != SOX_SUCCESS) {
        return result;
    }

    if (!effparameter_list_is_empty(&parameter_list)) {
        if (chorus->stage_count == MAX_STAGE_COUNT)
            lsx_fail("there is a maximum of %d stages", MAX_STAGE_COUNT);
        else
            lsx_fail("invalid argument `%s'", *argv);

        return SOX_EOF;
    }

    /* issue warning about possible clipping when parameters are
     * above some threshold */
    total_volume = chorus->gain_in;

    for (i = 0; i < chorus->stage_count; i++) {
        total_volume += chorus->stage_list[i].decay;
    }

    if (total_volume * chorus->gain_out > 1.0) {
        lsx_warn("the output may saturate; a safe gain-out is %g",
                 fabsf(1.0f / total_volume));
    }

    return SOX_SUCCESS;
}

/*--------------------*/

static char * chorus_get (sox_effect_t *effp,
                          char *name)
{
    chorus_priv_t *chorus = (chorus_priv_t *)effp->priv;
    char *s = NULL;

    if (!strcmp(name, "gain_in")) {
        s = lsx_malloc(32);
        sprintf(s, "%g", chorus->gain_in);
    }
    if (!strcmp(name, "gain_out")) {
        s = lsx_malloc(32);
        sprintf(s, "%g", chorus->gain_out);
    }

    return s;
}

/*--------------------*/

static char * chorus_set (sox_effect_t *effp,
                          char *name,
                          char *value)
{
    chorus_priv_t *chorus = (chorus_priv_t *)effp->priv;
    char *s = NULL;
    char *endptr = value;

    if (!strcmp(name, "gain-in")) {
        double gain = lsx_strtod(value, &endptr);
        if (endptr == value || *endptr != '\0') return NULL;
        chorus->gain_in = gain;
        s = malloc(32);
        sprintf(s, "%g", gain);
    }

    if (!strcmp(name, "gain-out")) {
        double gain = lsx_strtod(value, &endptr);
        if (endptr == value || *endptr != '\0') return NULL;
        chorus->gain_out = gain;
        s = malloc(32);
        sprintf(s, "%g", gain);
    }

    return s;
}

/*--------------------*/

/**
 * Prepare current channel of effect <C>effp</C> for processing.
 *
 * @param effp  chorus effect about to be started
 * @return  information whether start was successful
  */
static int chorus_start (sox_effect_t *effp)
{
    chorus_priv_t *chorus = (chorus_priv_t *) effp->priv;
    const int channel = effp->flow;
    sox_uint32_t i;

    if (channel == 0) {
        int result = setup_derived_data(effp);

        if (result != SOX_SUCCESS) {
            return result;
        }
    }
    
    for (i = 0;  i < chorus->stage_count;  i++) {
        /* allocate and set up channel data in stage for current
         * channel */
        chorus_stage_t *stage = &chorus->stage_list[i];
        chorus_stage_channel_t* channel_data = &stage->channel_data[channel];
        lsx_vcalloc(channel_data->delay_line, stage->delay_line_length);
    }

    return SOX_SUCCESS;
}

/*--------------------*/

/**
 * Handles the channel specific leadin phase for chorus effect with an
 * input gain of <C>gain_in</C> on <C>channel</C> with samples from
 * input sample buffer <C>ibuf</C> with sample count given by
 * <C>isamp</C>.  When <C>is_drain</C> isset, the drain phase is
 * processed and the input is assumed to be 0. Sets <C>d_in</C> and
 * initializes <C>d_out</C> accordingly.
 *
 * @param[in]     is_drain      tells whether this is the drain phase
 * @param[in]     gain_in       in gain of the chorus effect
 * @param[in]     channel       processed channel
 * @param[in]     sample_index  index of the current input sample in
 *                              the input buffer
 * @param[inout]  ibuf          sample buffer containing input samples
 *                              (may be NULL)
 * @param[out]    d_in          value of current input sample
 * @param[out]    d_out         initial value of current output sample
 */
static inline
void chorus_flow_leadin_ch (const sox_bool is_drain,
                            float gain_in,
                            size_t channel,
                            size_t sample_index,
                            const sox_sample_t **ibuf,
                            chorus_delay_sample_t *d_in,
                            chorus_delay_sample_t *d_out)
{
    /* Scale samples down to prevent arithmetic overflow when adding
     * up many delay lines.
     *
     * Dividing by scale_factor rounds the positive and negative
     * halves of the wave towards zero, thereby crushing the wave
     * towards zero by an average of half a sample value at the scaled
     * resolution.  The correction is *ibuf + (*ibuf < 0 ? -128 :
     * +128) but this can overflow, and the scaled resolution is
     * 24-bit so the error is negligable.
     */

    *d_in = (is_drain
             ? 0
             : (chorus_delay_sample_t) **ibuf / SCALING_FACTOR);
    (*ibuf)++;

    /* initialize output */
    *d_out = *d_in * gain_in;
}

/*--------------------*/

/**
 * Handles the stage specific leadin phase for chorus effect
 * <C>chorus</C> on <C>channel</C> and stage with index
 * <C>stage_index</C> . Sets <C>stage</C>, <C>delay_line</C> and the
 * integer offset <C>offset_i</C> as well as the real offset
 * <C>offset_r</C> and the modulated index <C>modulated_index</C>
 * accordingly.
 *
 * @param[in]  chorus             chorus effect
 * @param[in]  channel            processed channel
 * @param[in]  stage_index        index of the stage to be processed
 * @param[in]  offset_is_integer  tells whether offset is integer
 * @param[out] stage              stage object
 * @param[out] delay_line         delay line for this stage and channel
 * @param[out] offset_i           integer offset into delay line
 * @param[out] frac               fractional offset into delay line
 * @param[out] modulated_index    index into delay line taking
 *                                modulation into account
 */
static inline
void chorus_flow_leadin_stg (chorus_priv_t *chorus,
                             size_t channel,
                             size_t stage_index,
                             sox_bool offset_is_integer,
                             chorus_stage_t **stage,
                             chorus_delay_sample_t **delay_line,
                             int *offset_i,
                             double *frac,
                             sox_uint32_t *modulated_index)
{
    chorus_stage_t* chorus_stage = &chorus->stage_list[stage_index];
    *stage = chorus_stage;
    const chorus_stage_channel_t* channel_data =
        &chorus_stage->channel_data[channel];
    *delay_line = channel_data->delay_line;
    sox_uint32_t wave_index = chorus_stage->wave_index;

    if (offset_is_integer) {
        *offset_i = chorus_stage->wave_table_i[wave_index];
        *frac = 0.0;
    } else {
        float offset_f = chorus_stage->wave_table_f[wave_index];
        *offset_i = offset_f;
        *frac = offset_f - *offset_i;
    }

    *modulated_index = ((chorus_stage->delay_line_index
                         + chorus_stage->depth_sample_count
                         + *offset_i)
                        % chorus_stage->delay_line_length);
}

/*--------------------*/

/**
 * Handles the channel leadout phase for effect <C>effp</C> by
 * adjusting the output from <C>scaled_sample</C> into buffer given by
 * <C>buffer</C>, scaling output up again and checking for clipping
 *
 * @param[inout]  effp           chorus effect
 * @param[in]     scaled_sample  delayed sample
 * @param[out]    buffer         buffer for output samples
 */
static inline
void chorus_flow_leadout_ch (sox_effect_t *effp,
                             chorus_delay_sample_t scaled_sample,
                             sox_sample_t **buffer)
{
    **buffer = SOX_ROUND_CLIP_COUNT(scaled_sample * SCALING_FACTOR,
                                    effp->clips);
    (*buffer)++;
}

/*--------------------*/

/**
 * Handles the stage specific leadout phase for <C>stage</C> using
 * <C>sample</C> to update the cumulated output sample <C>d_out</C>
 * accordingly.
 *
 * @param[in]  stage        stage object
 * @param[in]  delay_line   current stage and channel delay line
 * @param[in]  d_in         current input sample
 * @param[in]  sample       result sample of current stage
 * @param[out] d_out        cumulative output sample
 */
static inline
void chorus_flow_leadout_stg (chorus_stage_t *stage,
                              chorus_delay_sample_t *delay_line,
                              chorus_delay_sample_t d_in,
                              chorus_delay_sample_t sample,
                              chorus_delay_sample_t *d_out)
{
    delay_line[stage->delay_line_index] = d_in;
    *d_out += sample * stage->decay;
    MODULAR_INCREMENT(stage->delay_line_index,
                      stage->delay_line_length);
    MODULAR_INCREMENT(stage->wave_index, stage->wave_length);
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
static int chorus_flow_or_drain (sox_effect_t *effp,
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

    /* variable reused for all the interpolation variants */
    const size_t channel = effp->flow;
    size_t sample_index;
    sox_uint32_t stage_index;
    chorus_delay_sample_t d_in;
    chorus_delay_sample_t d_out;

    chorus_stage_t *stage;
    chorus_delay_sample_t *delay_line;
    int offset_i;
    double frac;
    sox_uint32_t modulated_index;
    chorus_delay_sample_t sample;
    chorus_delay_sample_t delayed_0, delayed_1, delayed_2;

    switch (chorus->interpolation) {
        case INTERP_NONE:
            while (len--) {
                sample_index = *isamp - len - 1;
                chorus_flow_leadin_ch(is_drain, chorus->gain_in,
                                      channel, sample_index,
                                      &ibuf, &d_in, &d_out);
                    
                for (stage_index = 0; stage_index < chorus->stage_count;
                     stage_index++) {
                    chorus_flow_leadin_stg(chorus,
                                           channel, stage_index, sox_true,
                                           &stage, &delay_line,
                                           &offset_i, &frac,
                                           &modulated_index);
                        
                    sample = DELAY_LINE_AT(modulated_index);

                    chorus_flow_leadout_stg(stage, delay_line,
                                            d_in, sample, &d_out);
                }

                chorus_flow_leadout_ch(effp, d_out * chorus->gain_out,
                                       &obuf);
            }
            break;
         case INTERP_LINEAR:
            while (len--) {
                sample_index = *isamp - len - 1;
                chorus_flow_leadin_ch(is_drain, chorus->gain_in,
                                      channel, sample_index,
                                      &ibuf, &d_in, &d_out);

                for (stage_index = 0; stage_index < chorus->stage_count;
                     stage_index++) {
                    chorus_flow_leadin_stg(chorus,
                                           channel, stage_index, sox_false,
                                           &stage, &delay_line,
                                           &offset_i, &frac,
                                           &modulated_index);

                    delayed_0 = DELAY_LINE_AT(modulated_index);
                    delayed_1 = DELAY_LINE_AT(modulated_index - 1);
                    sample = delayed_0 * (1 - frac) + delayed_1 * frac;

                    chorus_flow_leadout_stg(stage, delay_line,
                                            d_in, sample, &d_out);
                }

                chorus_flow_leadout_ch(effp, d_out * chorus->gain_out,
                                       &obuf);
            }
            break;
         case INTERP_QUADRATIC:
            while (len--) {
                sample_index = *isamp - len - 1;
                chorus_flow_leadin_ch(is_drain, chorus->gain_in,
                                      channel, sample_index,
                                      &ibuf, &d_in, &d_out);

                for (stage_index = 0; stage_index < chorus->stage_count;
                     stage_index++) {
                    chorus_flow_leadin_stg(chorus,
                                           channel, stage_index, sox_false,
                                           &stage, &delay_line,
                                           &offset_i, &frac,
                                           &modulated_index);

                    delayed_0 = DELAY_LINE_AT(modulated_index);
                    delayed_1 = DELAY_LINE_AT(modulated_index - 1);
                    delayed_2 = DELAY_LINE_AT(modulated_index - 2);

                    double a, b;
                    delayed_2 -= delayed_0;
                    delayed_1 -= delayed_0;
                    a = delayed_2 *.5 - delayed_1;
                    b = delayed_1 * 2 - delayed_2 *.5;
                    sample = delayed_0 + (a * frac + b) * frac;

                    chorus_flow_leadout_stg(stage, delay_line,
                                            d_in, sample, &d_out);
                }

                chorus_flow_leadout_ch(effp, d_out * chorus->gain_out,
                                       &obuf);
            }
            break;
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
static int chorus_flow (sox_effect_t *effp,
                            const sox_sample_t *ibuf,
                            sox_sample_t *obuf,
                            size_t *isamp,
                            size_t *osamp)
{
    return chorus_flow_or_drain(effp, ibuf, obuf, isamp, osamp);
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
static int chorus_drain (sox_effect_t * effp,
                             sox_sample_t *obuf,
                             size_t *osamp)
{
    chorus_priv_t *chorus = (chorus_priv_t *) effp->priv;
    int result = SOX_EOF;

    if (chorus->remaining_samples > 0) {
        size_t dummy = chorus->remaining_samples;
        result = chorus_flow_or_drain(effp, NULL, obuf,
                                          &dummy, osamp);
        chorus->remaining_samples -= dummy;
    }

    return result;
}

/*--------------------*/

/**
 * Clean up single channel in chorus effect <C>effp</C>.
 *
 * @param effp   chorus effect to be cleaned up
 */
static int chorus_stop (sox_effect_t * effp)
{
    chorus_priv_t * chorus = (chorus_priv_t *) effp->priv;
    size_t channel = effp->flow;
    sox_uint32_t i;

    for (i = 0;  i < chorus->stage_count;  i++) {
        chorus_stage_t *stage = &chorus->stage_list[i];
        chorus_stage_channel_t* channel_data = &stage->channel_data[channel];
        free(channel_data->delay_line);
    }

    return SOX_SUCCESS;
}

/*--------------------*/

/**
 * Clean up chorus effect <C>effp</C>.
 *
 * @param effp   chorus effect to be finally cleaned up
 */
static int chorus_kill (sox_effect_t * effp)
{
    chorus_priv_t * chorus = (chorus_priv_t *) effp->priv;
    sox_uint32_t i;

    for (i = 0;  i < chorus->stage_count;  i++) {
        chorus_stage_t *stage = &chorus->stage_list[i];

        if (chorus->has_integer_wave_tables) {
            free(stage->wave_table_i);
        } else {
            free(stage->wave_table_f);
        }
    }

    free(chorus->stage_list);
    return SOX_SUCCESS;
}

/*--------------------*/

const sox_effect_handler_t *lsx_chorus_effect_fn (void)
{
    static char const chorus_usage[] =
        "[-n|-l|-q] [-s|-t] [gain-in [gain-out {delay [decay"
        " [speed [depth [-s|-t]]]]}]]";

    static char const * const chorus_extra_usage[] = {
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
        "OPTION   RANGE DEFAULT DESCRIPTION",
        "interp -n|-l|-q  -n    Interpolation type: none, linear or"
                              " quadratic",
        "gain-in  -1-1    0.5   Proportion of input delivered clean"
                              " to the adder",
        "gain-out -1-1     1    Final volume adjustment",
        "delay   0-1000   50    Fixed delay in milliseconds",
        "decay    -1-1    0.5   Proportion of delay's output delivered"
                              " to the adder",
        "speed   0-192k   0.25  Modulation frequency (no more than the"
                              " sample rate)",
        "depth   0-1000    2    Additional variable delay in"
                              " milliseconds",
        "wave     -s|-t   -s    Modulate with a sinusoidal or a"
                              " triangular wave",
        "Hint: gain-out <= 1 / ( gain-in + decay 1 + ... + decay n )",
        "Keymaps: chorus.(gain-in|gain-out)",
        NULL
    };

    static sox_effect_handler_t chorus_effect = {
        "chorus",
        chorus_usage,
        SOX_EFF_LENGTH | SOX_EFF_GAIN,
        chorus_getopts,
        chorus_start,
        chorus_flow,
        chorus_drain,
        chorus_stop,
        chorus_kill,
        sizeof(chorus_priv_t),
        chorus_extra_usage,
        chorus_get,
        chorus_set,
    };

    return &chorus_effect;
}
