"""SCons.Tool.gs

Tool-specific initialization for Ghostscript.

There normally shouldn't be any need to import this module directly.
It will usually be imported through the generic SCons.Tool.Tool()
selection method.

"""

#
# Copyright (c) 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008 The SCons Foundation
#
# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
# KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
# WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
# LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
# OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
# WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#

__revision__ = "src/engine/SCons/Tool/gs.py 3424 2008/09/15 11:22:20 scons"

import SCons.Action
import SCons.Platform
import SCons.Util

# Ghostscript goes by different names on different platforms...
platform = SCons.Platform.platform_default()

if platform == 'os2':
    gs = 'gsos2'
elif platform == 'win32':
    gs = 'gswin32c'
else:
    gs = 'gs'

GhostscriptAction = None

def generate(env):
    """Add Builders and construction variables for Ghostscript to an
    Environment."""

    global GhostscriptAction
    if GhostscriptAction is None:
        GhostscriptAction = SCons.Action.Action('$GSCOM', '$GSCOMSTR')

    import pdf
    pdf.generate(env)

    bld = env['BUILDERS']['PDF']
    bld.add_action('.ps', GhostscriptAction)

    env['GS']      = gs
    env['GSFLAGS'] = SCons.Util.CLVar('-dNOPAUSE -dBATCH -sDEVICE=pdfwrite')
    env['GSCOM']   = '$GS $GSFLAGS -sOutputFile=$TARGET $SOURCES'


def exists(env):
    if env.has_key('PS2PDF'):
        return env.Detect(env['PS2PDF'])
    else:
        return env.Detect(gs) or SCons.Util.WhereIs(gs)
