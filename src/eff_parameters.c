/**
 * SoX effect parameter list (implementation).
 *
 * Parameters in SoX are passed as command-line parameters in C via an
 * <C>argc</C> and <C>argv</C> data pair, where <C>argc</C> gives the
 * count of parameters and <C>argv</C> the array of strings with the
 * actual parameter values.  This module implements some support
 * functions and macros for handling those parameter lists in the SoX
 * effects.
 *
 * author: Dr. Thomas Tensi
 * initial version: 2026-04
 */

/*====================*/

#include "eff_parameters.h"

/*====================*/

void effparameter_list_init (sox_effparameter_list_t* self,
                             int argc,
                             char** argv)
{
    self->argc = argc;
    self->argv = argv;
}

/*--------------------*/

void effparameter_list_advance (sox_effparameter_list_t* self)
{
    self->argc--;
    self->argv++;
}

/*--------------------*/

sox_bool effparameter_list_is_empty (sox_effparameter_list_t* self)
{
    return self->argc == 0;
}

/*--------------------*/

char* effparameter_current (sox_effparameter_list_t* self)
{
    return self->argv[0];
}

/*--------------------*/

sox_bool effparameter_is_flag (sox_effparameter_list_t* self)
{
    return self->argv[0][0] == '-';
}

/*--------------------*/

char effparameter_flag_name (sox_effparameter_list_t* self)
{
    return self->argv[0][1];
}

/*--------------------*/

sox_effparameter_scan_result
effparameter_scan_number_f (sox_effparameter_list_t* self,
                            float *variable,
                            const char *variable_name,
                            const float low_bound,
                            const float high_bound)
{
    sox_effparameter_scan_result result = scan_exhausted;

    if (self->argc > 0) {
        char *end_ptr;
        const char *current_argument = *self->argv;
        float f = strtod(current_argument, &end_ptr);

        if (end_ptr == current_argument) {
            /* no valid number was found */
        } else {
            result = scan_erroneous;

            if (*end_ptr != '\0') {
                /* this is a number with trailing garbage */
                lsx_fail("%s `%s' is not a number",
                         variable_name, current_argument);
            } else if (f < low_bound || f > high_bound) {
                lsx_fail("%s `%s' must be from %g to %g",
                         variable_name, current_argument,
                         low_bound, high_bound); 
            } else {
                *variable = f;
                result = scan_okay;
                effparameter_list_advance(self);
            }
        }
    }

    return result;
}

/*--------------------*/

sox_effparameter_scan_result
effparameter_scan_enum (sox_effparameter_list_t* self,
                        size_t *variable,
                        const char *variable_name,
                        const lsx_enum_item *item_table)
{
    sox_effparameter_scan_result result = scan_exhausted;

    if (self->argc > 0) {
        lsx_enum_item const * e;                \
        const char *current_argument = *self->argv;
        e = lsx_find_enum_text(current_argument, item_table, 0);

        if (e != NULL) {
            result = scan_okay;
            *variable = e->value;
            effparameter_list_advance(self);
        }
    }

    return result;
}
