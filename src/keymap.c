/* keymap.c: SoX keypress-to-effect-parameter-change functions
 *
 * Copyright (c) 2025 Martin Guy <martinwguy@gmail.com>
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 2
 * as published by the Free Software Foundation.
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

#include "sox_i.h"

/* Routines to remember and forget keymaps.
 *
 * The string values are mallocked memory which we are responsible for freeing.
 * The effect name may be "synth2" meaning "only tweak the second synth effect
 * in the effects chain".
 */
void
sox_keymap_add(char *key, char *effect, char *field, char operator, double step)
{
  sox_keymap_t *keymaps;
  unsigned keymap_count = sox_globals.keymap_count;

  lsx_revalloc(sox_globals.keymaps, sox_globals.keymap_count + 1);
  keymaps = sox_globals.keymaps;
  keymaps[keymap_count].key = key;
  keymaps[keymap_count].effect = effect;
  keymaps[keymap_count].field = field;
  keymaps[keymap_count].operator = operator;
  keymaps[keymap_count].step = step;
  sox_globals.keymap_count++;
}

void
keymap_free(void)
{
  sox_keymap_t *keymaps = sox_globals.keymaps;
  unsigned i;

  for (i=0; i < sox_globals.keymap_count; i++) {
    free(keymaps[i].key);
    free(keymaps[i].effect);
    free(keymaps[i].field);
  }
  free(keymaps);
  sox_globals.keymaps = NULL;
  sox_globals.keymap_count = 0;
}
