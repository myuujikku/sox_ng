/* libSoX effect: Saturation   (c) 2025 michael@briarproject.org
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

#include <assert.h>
#include <math.h>
#include "sox_i.h"

/* Ensure no clipping due to rounding errors in output gain compensation */
#define SAFETY_FACTOR 0.9999

typedef enum {SAT_TANH, SAT_SQRT, SAT_DIODE} sat_t;

typedef struct {
  /* Parameters */
  sat_t  sat_type;
  double blend;
  double offset;
  union {
    double drive; /* tanh */
    double color; /* sqrt */
    double threshold; /* diode */
  };

  /* Recenter the output so zero in -> zero out */
  double offset_out;
  /* Keep the output within range */
  double gain_out;
} priv_t;



static lsx_enum_item const sat_enum[] = {
  LSX_ENUM_ITEM(SAT_,TANH)
  LSX_ENUM_ITEM(SAT_,SQRT)
  LSX_ENUM_ITEM(SAT_,DIODE)
  {0, 0}};



static double sat_tanh(priv_t *p, double d) {
  d += p->offset;
  return tanh(p->drive * d) - p->offset_out;
}



static double sat_sqrt(priv_t *p, double d) {
  d += p->offset;
  double root_d = sqrt(fabs(d));
  /* TODO: Can we use copysign (C99) to avoid a branch? */
  double sign_d_root_d = d < 0 ? -root_d : root_d;
  double d_root_d = d * root_d;
  return sign_d_root_d * p->color + d_root_d * (1 - p->color) - p->offset_out;
}



static double sat_diode(priv_t *p, double d) {
  d += p->offset;
  d = d > p->threshold ? p->threshold : d;
  return (d < -p->threshold ? -p->threshold : d) - p->offset_out;
}



static int getopts(sox_effect_t * effp, int argc, char *argv[])
{
  priv_t * p = (priv_t *) effp->priv;
  --argc, ++argv;

  /* Set defaults */
  p->sat_type = SAT_TANH;
  p->blend = 1;
  p->offset = 0;

  do {
    TEXTUAL_PARAMETER(sat_type, sat_enum)
    NUMERIC_PARAMETER(blend, 0, 1)
    NUMERIC_PARAMETER(offset, 0, 1)
  } while (0);

  switch (p->sat_type) {
    case SAT_TANH:
      p->drive = 1;
      do {
        NUMERIC_PARAMETER(drive, 1, INFINITY)
      } while (0);
      break;
    case SAT_SQRT:
      p->color = 0.5;
      do {
        NUMERIC_PARAMETER(color, 0, 1)
      } while (0);
      break;
    case SAT_DIODE:
      p->threshold = 0.5;
      do {
        NUMERIC_PARAMETER(threshold, 0, 1)
      } while (0);
      break;
    default:
      assert(sox_false);
  }

  if (argc != 0)
    return lsx_usage(effp);

  return SOX_SUCCESS;
}



static int start(sox_effect_t * effp)
{
  priv_t * p = (priv_t *) effp->priv;

  /* Initialise to 0 and use saturation function to calculate the right value */
  p->offset_out = 0;

  switch (p->sat_type) {
    case SAT_TANH:
      p->offset_out = sat_tanh(p, 0);
      p->gain_out = SAFETY_FACTOR / fmax(sat_tanh(p, 1), fabs(sat_tanh(p, -1)));
      break;
    case SAT_SQRT:
      p->offset_out = sat_sqrt(p, 0);
      p->gain_out = SAFETY_FACTOR / fmax(sat_sqrt(p, 1), fabs(sat_sqrt(p, -1)));
      break;
    case SAT_DIODE:
      p->offset_out = sat_diode(p, 0);
      p->gain_out = SAFETY_FACTOR / fmax(sat_diode(p, 1), fabs(sat_diode(p, -1)));
      break;
    default:
      assert(sox_false);
  }

  return SOX_SUCCESS;
}



static int flow(sox_effect_t * effp, sox_sample_t const * ibuf,
    sox_sample_t * obuf, size_t * isamp, size_t * osamp)
{
  SOX_SAMPLE_LOCALS;
  priv_t * p = (priv_t *) effp->priv;
  size_t len = *isamp > *osamp ? *osamp : *isamp;
  *isamp = *osamp = len;
  double in, fx;

  switch (p->sat_type) {
    case SAT_TANH:
      while (len--) {
        in = SOX_SAMPLE_TO_FLOAT_64BIT(*ibuf++, effp->clips);
        fx = sat_tanh(p, in);
        fx = fx * p->gain_out * p->blend + in * (1 - p->blend);
        *obuf++ = SOX_FLOAT_64BIT_TO_SAMPLE(fx, effp->clips);
      }
      break;
    case SAT_SQRT:
      while (len--) {
        in = SOX_SAMPLE_TO_FLOAT_64BIT(*ibuf++, effp->clips);
        fx = sat_sqrt(p, in);
        fx = fx * p->gain_out * p->blend + in * (1 - p->blend);
        *obuf++ = SOX_FLOAT_64BIT_TO_SAMPLE(fx, effp->clips);
      }
      break;
    case SAT_DIODE:
      while (len--) {
        in = SOX_SAMPLE_TO_FLOAT_64BIT(*ibuf++, effp->clips);
        fx = sat_diode(p, in);
        fx = fx * p->gain_out * p->blend + in * (1 - p->blend);
        *obuf++ = SOX_FLOAT_64BIT_TO_SAMPLE(fx, effp->clips);
      }
      break;
    default:
      assert(sox_false);
  }

  return SOX_SUCCESS;
}



sox_effect_handler_t const * lsx_saturation_effect_fn(void)
{
  static const char usage[] =
"[type [blend [offset [drive|color|threshold]]]]";
  static char const * const extra_usage[] = {
"",
"           RANGE            DEFAULT  DESCRIPTION",
"type       tanh|sqrt|diode  tanh     saturation type",
"blend      0-1              1        mixture of dry and wet signals",
"offset     0-1              0        DC offset for asymmetric distortion",
"drive      1-inf            1        input gain (tanh)",
"color      0-1              0.5      mixture of two saturation colors (sqrt)",
"threshold  0-1              0.5      level at which clipping starts (diode)",
    NULL
  };

  static sox_effect_handler_t handler = {
    "saturation", usage, extra_usage, SOX_EFF_GAIN,
    getopts, start, flow, NULL, NULL, NULL, sizeof(priv_t)};

  return &handler;
}
