/**
 * SoX effect parameter list (specification).
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



#ifndef EFF_PARAMETERS_INCLUDED
#define EFF_PARAMETERS_INCLUDED

/*============================================================*/

#include "sox_i.h"

/*--------------------*/
/*--------------------*/

/**
 * a single type combining argc and argv into one structure
 */
typedef struct {
    int  argc;
    char **argv;
} sox_effparameter_list_t;


/**
 * the enumeration type of return codes when scanning the next
 * parameter
 */
typedef enum {
    scan_okay,
    scan_exhausted,
    scan_erroneous
} sox_effparameter_scan_result;

/*--------------------*/
/*--------------------*/

/**
 * macro that breaks the enclosing loop when the parameter list is
 * exhausted and returns SOX_EOF when the parameter has a bad format
 */
#define HANDLE_SCAN_RESULT(r) \
    { \
        if ((r) == scan_exhausted) break; \
        else if ((r) == scan_erroneous) return SOX_EOF;    \
    }

/*--------------------*/

/**
 * Initializes parameter list <C>self</C> from count of arguments
 * <C>argc</C> and string argument array <C>argv</C>.
 *
 * @param self  parameter list to be initialized
 * @param argc  count of parameters
 * @param argc  parameter string array
 */
void effparameter_list_init (sox_effparameter_list_t* self,
                             int argc,
                             char** argv);

/*--------------------*/

/**
 * Advances parameter list <C>self</C>
 *
 * @param self  parameter list to be advanced
 */
void effparameter_list_advance (sox_effparameter_list_t* self);

/*--------------------*/

/**
 * Tells whether parameter list <C>self</C> is empty
 *
 * @param self  parameter list to be checked
 * @return  information whether parameter list is empty
 */
sox_bool effparameter_list_is_empty (sox_effparameter_list_t* self);

/*--------------------*/

/**
 * Returns current parameter in parameter list <C>self</C>
 *
 * @param self  parameter list to be checked
 * @return  current parameter as string
 */
char* effparameter_current (sox_effparameter_list_t* self);

/*--------------------*/

/**
 * Tells whether current parameter in list <C>self</C> is a flag.
 *
 * @param self  parameter list to be checked
 * @return  information whether current parameter is a flag
 */
sox_bool effparameter_is_flag (sox_effparameter_list_t* self);

/*--------------------*/

/**
 * Returns option character in current parameter in list <C>self</C>
 * (assuming it is a flag).
 *
 * @param self  parameter list to be queried for option name
 * @return  option character for current parameter
 */
char effparameter_flag_name (sox_effparameter_list_t* self);

/*--------------------*/

/**
 * Checks current parameter in list <C>self</C> for being a float
 * number in the range <C>low_bound</C> to <C>high_bound</C> and
 * assigns it to <C>variable</C> if successful. Returns information
 * whether assignment was okay or otherwise the parameter list is
 * exhausted or the current parameter is not a valid number.
 *
 * @param self           parameter list to be queried for option name
 * @param variable       target float variable
 * @param variable_name  name of target variable
 * @param low_bound      minimum acceptable value for variable
 * @param high_bound     maximum acceptable value for variable
 * @return  information whether assignment has succeeded
 */
sox_effparameter_scan_result
effparameter_scan_number_f (sox_effparameter_list_t* self,
                            float *variable,
                            const char *variable_name,
                            const float low_bound,
                            const float high_bound);

/*--------------------*/

/**
 * Checks current parameter in list <C>self</C> for being an
 * enumeration item as given by <C>item_table</C> and assigns it to
 * <C>variable</C> if successful. Returns information whether
 * assignment was okay or otherwise the parameter list is exhausted or
 * the current parameter is not valid.
 *
 * @param self           parameter list to be queried for option name
 * @param variable       target float variable
 * @param variable_name  name of target variable
 * @param item_table     list of valid strings
 * @return  information whether assignment has succeeded
 */
sox_effparameter_scan_result
effparameter_scan_enum (sox_effparameter_list_t* self,
                        size_t *variable,
                        const char *variable_name,
                        const lsx_enum_item *item_table);

/*============================================================*/

#endif /* EFF_PARAMETERS_INCLUDED */
