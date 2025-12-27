/* soxcopy: copy one audio file to another... a simple format converter
 *
 * We know that SOX_SUCCESS == 0, don't bother freeing anything
 * and assume that sox_find_effect("input") or ("output") can't fail.
 */

#include "sox_ng.h"
#include <stdlib.h> /* for main() */
#include <stdio.h>  /* for fprintf(stderr) */

/* Only show failure messages */
static void show_error(unsigned level, const char *filename,
                       const char *format, va_list ap)
{ if (level == 1) { vfprintf(stderr, format, ap); putc('\n', stderr); } }

int main(int argc, char * argv[])
{
  char *infile, *outfile;
  sox_format_t *in, *out;
  sox_effects_chain_t *chain;
  sox_effect_t * effect;

  if (argc != 3) {
    fprintf(stderr, "usage: soxcopy input output\n");
    exit(1);
  }
  infile = argv[1]; outfile = argv[2];

  if (sox_init()) exit(1);
  sox_globals.output_message_handler = show_error;
  sox_format_init();
  chain = sox_create_effects_chain(&in->encoding, &out->encoding);

  in = sox_open_read(infile, NULL, NULL, NULL);
  if (!in) exit(1);
  effect = sox_create_effect(sox_find_effect("input"));
  sox_effect_options(effect, 1, (char **)&in);
  sox_add_effect(chain, effect, &in->signal, &in->signal);

  out = sox_open_write(outfile, &in->signal, NULL, NULL, NULL, NULL);
  if (!out) exit(1);
  effect = sox_create_effect(sox_find_effect("output"));
  sox_add_effect(chain, effect, &in->signal, &in->signal);
  sox_effect_options(effect, 1, (char **)&out);

  sox_flow_effects(chain, NULL, NULL);
  sox_close(out);
  sox_quit();
  return 0;
}
