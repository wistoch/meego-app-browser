#!/usr/bin/env python

import os.path
import sys

output_filename = 'samples_gen.gyp'
try:
  output_file = open(output_filename, "w+")
except IOError:
  sys.stderr.write('Unable to write to generated gyp file %s\n',
                   output_filename)

x_up = '1,0,0'
y_up = '0,1,0'
z_up = '0,0,1'

names = {
  x_up : 'x_up',
  y_up : 'y_up',
  z_up : 'z_up',
}

assets = [
 {'path': 'beachdemo/convert_assets/beachdemo.zip', 'up': z_up},
 {'path': 'beachdemo/convert_assets/beach-low-poly.dae', 'up': z_up},
 {'path': 'GoogleIO-2009/convert_assets/background.zip', 'up': y_up},
 {'path': 'GoogleIO-2009/convert_assets/character.zip', 'up': y_up},
 {'path': 'home-configurators/convert_cbassets/House_Roofless.kmz', 'up': z_up},
 {'path': 'home-configurators/convert_cbassets/Agra_Rug.kmz', 'up': z_up},
 {'path': 'home-configurators/convert_cbassets/Asimi_Rug.kmz', 'up': z_up},
 {'path': 'home-configurators/convert_cbassets/Camden_Chair.kmz', 'up': z_up},
 {'path': 'home-configurators/convert_cbassets/Elements_Bookshelf.kmz', 'up': z_up},
 {'path': 'home-configurators/convert_cbassets/Ferrara_Rug.kmz', 'up': z_up},
 {'path': 'home-configurators/convert_cbassets/Lounge_Chair.kmz', 'up': z_up},
 {'path': 'home-configurators/convert_cbassets/Lounge_Chaise.kmz', 'up': z_up},
 {'path': 'home-configurators/convert_cbassets/Lounge_Sofa.kmz', 'up': z_up},
 {'path': 'home-configurators/convert_cbassets/Lounge_Storage_Ottoman.kmz', 'up': z_up},
 {'path': 'home-configurators/convert_cbassets/Madison_Dining_Table.kmz', 'up': z_up},
 {'path': 'home-configurators/convert_cbassets/Miles_Side_Chair.kmz', 'up': z_up},
 {'path': 'home-configurators/convert_cbassets/Pullman_Bar_Stool.kmz', 'up': z_up},
 {'path': 'home-configurators/convert_cbassets/Puzzle_TV_Stand.kmz', 'up': z_up},
 {'path': 'home-configurators/convert_cbassets/Stow_Leather_Ottoman.kmz', 'up': z_up},
 {'path': 'home-configurators/convert_cbassets/Tivoli_Dining_Table.kmz', 'up': z_up},
 {'path': 'home-configurators/convert_cbassets/Tivoli_Miles_Dining_Set.kmz', 'up': z_up},
 {'path': 'home-configurators/convert_cbassets/Troy_Chair.kmz', 'up': z_up},
 {'path': 'home-configurators/convert_cbassets/Troy_Ottoman.kmz', 'up': z_up},
 {'path': 'home-configurators/convert_cbassets/Troy_Sofa.kmz', 'up': z_up},
 {'path': 'home-configurators/convert_cbassets/Troy_Storage_Ottoman.kmz', 'up': z_up},
 {'path': 'home-configurators/convert_cbassets/Troy_Twin_Sleeper.kmz', 'up': z_up},

 {'path': 'io/convert_levels/all_actors.kmz', 'up': y_up},
 {'path': 'io/convert_levels/map1.kmz', 'up': y_up},
 {'path': 'simpleviewer/convert_assets/cube.zip', 'up': y_up},
 {'path': 'convert_assets/dome1.zip', 'up': y_up},
 {'path': 'convert_assets/dome2.zip', 'up': y_up},
 {'path': 'convert_assets/dome3.zip', 'up': y_up},
 {'path': 'convert_assets/dome4.zip', 'up': y_up},
 {'path': 'convert_assets/kitty_151_idle_stand05_cff1.zip', 'up': y_up},
 {'path': 'convert_assets/part1.zip', 'up': y_up},
 {'path': 'convert_assets/part2.zip', 'up': y_up},
 {'path': 'convert_assets/part3.zip', 'up': y_up},
 {'path': 'convert_assets/seven_shapes.zip', 'up': y_up},
 {'path': 'convert_assets/stencil_frame.zip', 'up': y_up},
 {'path': 'convert_assets/teapot.zip', 'up': y_up},
 {'path': 'convert_assets/yard.zip', 'up': y_up},
 {'path': 'waterdemo/convert_assets/bamboo.zip', 'up': y_up},
 {'path': 'waterdemo/convert_assets/coconuts.zip', 'up': y_up},
 {'path': 'waterdemo/convert_assets/driftwood.zip', 'up': y_up},
 {'path': 'waterdemo/convert_assets/island.zip', 'up': y_up},
 {'path': 'waterdemo/convert_assets/lazy_bridge.zip', 'up': y_up},
 {'path': 'waterdemo/convert_assets/palm_leaves.zip', 'up': y_up},
 {'path': 'waterdemo/convert_assets/palm_trees.zip', 'up': y_up},
 {'path': 'waterdemo/convert_assets/rocks.9.zip', 'up': y_up},
 {'path': 'waterdemo/convert_assets/rocks.zip', 'up': y_up},
]

output_file.write("""# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    '../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'build_samples',
      'type': 'none',
      'dependencies': [
        '../converter/converter.gyp:o3dConverter',
      ],
      'actions': [\n""")
for asset in assets:
  filename = os.path.splitext(os.path.basename(asset['path']))[0]
  filename = filename.replace('.','_')
  filename = filename.replace('-','_')
  filename = filename.lower()
  name = "convert_" + filename
  output = asset['path'].replace('convert_', '')
  output = os.path.splitext(output)[0] + ".o3dtgz"
  output_dir = os.path.dirname(output)
  output_file.write("        {\n")
  output_file.write("          'action_name': '%s',\n" % name)
  output_file.write("          'inputs': [\n")
  output_file.write("            '../o3d_assets/samples/%s',\n" % asset['path'])
  output_file.write("          ],\n")
  output_file.write("          'outputs': [\n")
  output_file.write("            '../samples/%s',\n" % output)
  output_file.write("          ],\n")
  output_file.write("          'action': [\n")
  output_file.write("            '<(PRODUCT_DIR)/o3dConverter',\n")
  output_file.write("            '--no-condition',\n")
  output_file.write("            '--up-axis=%s',\n" % asset['up'])
  output_file.write("            '<(_inputs)',\n")
  output_file.write("            '<(_outputs)',\n")
  output_file.write("          ],\n")
  output_file.write("        },\n")

output_file.write("      ],\n")

# coalesce copies.
copies = {}
for asset in assets:
  output = asset['path'].replace('convert_', '')
  output = os.path.splitext(output)[0] + ".o3dtgz"
  output_dir = os.path.dirname(output)
  if output_dir in copies:
    copies[output_dir] += [output]
  else:
    copies[output_dir] = [output]

output_file.write("      'copies': [\n")
for (dir, paths) in copies.items():
  output_file.write("        {\n")
  output_file.write("          'destination': " \
                    "'<(PRODUCT_DIR)/samples/%s',\n" % dir)
  output_file.write("          'files': [\n")
  for path in paths:
    output_file.write("            '../samples/%s',\n" % path)
  output_file.write("          ],\n")
  output_file.write("        },\n")

output_file.write("      ],\n")
output_file.write("    },\n")
output_file.write("  ],\n")
output_file.write("}\n")

print output_filename
sys.exit(0)
