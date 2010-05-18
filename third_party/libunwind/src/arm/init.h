/* libunwind - a platform-independent unwind library
   Copyright (C) 2008 CodeSourcery

This file is part of libunwind.

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.  */

#include "unwind_i.h"

static inline int
common_init (struct cursor *c)
{
  int ret, i;

  c->dwarf.loc[R0] = DWARF_REG_LOC (&c->dwarf, UNW_ARM_R0);
  c->dwarf.loc[R1] = DWARF_REG_LOC (&c->dwarf, UNW_ARM_R1);
  c->dwarf.loc[R2] = DWARF_REG_LOC (&c->dwarf, UNW_ARM_R2);
  c->dwarf.loc[R3] = DWARF_REG_LOC (&c->dwarf, UNW_ARM_R3);
  c->dwarf.loc[R4] = DWARF_REG_LOC (&c->dwarf, UNW_ARM_R4);
  c->dwarf.loc[R5] = DWARF_REG_LOC (&c->dwarf, UNW_ARM_R5);
  c->dwarf.loc[R6] = DWARF_REG_LOC (&c->dwarf, UNW_ARM_R6);
  c->dwarf.loc[R7] = DWARF_REG_LOC (&c->dwarf, UNW_ARM_R7);
  c->dwarf.loc[R8] = DWARF_REG_LOC (&c->dwarf, UNW_ARM_R8);
  c->dwarf.loc[R9] = DWARF_REG_LOC (&c->dwarf, UNW_ARM_R9);
  c->dwarf.loc[R10] = DWARF_REG_LOC (&c->dwarf, UNW_ARM_R10);
  c->dwarf.loc[R11] = DWARF_REG_LOC (&c->dwarf, UNW_ARM_R11);
  c->dwarf.loc[R12] = DWARF_REG_LOC (&c->dwarf, UNW_ARM_R12);
  c->dwarf.loc[R13] = DWARF_REG_LOC (&c->dwarf, UNW_ARM_R13);
  c->dwarf.loc[R14] = DWARF_REG_LOC (&c->dwarf, UNW_ARM_R14);
  c->dwarf.loc[R15] = DWARF_REG_LOC (&c->dwarf, UNW_ARM_R15);
  for (i = R15 + 1; i < DWARF_NUM_PRESERVED_REGS; ++i)
    c->dwarf.loc[i] = DWARF_NULL_LOC;

  ret = dwarf_get (&c->dwarf, c->dwarf.loc[R15], &c->dwarf.ip);
  if (ret < 0)
    return ret;

  /* FIXME: correct for ARM?  */
  ret = dwarf_get (&c->dwarf, DWARF_REG_LOC (&c->dwarf, UNW_ARM_R13),
		   &c->dwarf.cfa);
  if (ret < 0)
    return ret;

  /* FIXME: Initialisation for other registers.  */

  c->dwarf.args_size = 0;
  c->dwarf.ret_addr_column = 0;
  c->dwarf.pi_valid = 0;
  c->dwarf.pi_is_dynamic = 0;
  c->dwarf.hint = 0;
  c->dwarf.prev_rs = 0;

  return 0;
}
