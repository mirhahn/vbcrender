#!/usr/bin/env python

#
# vbcrender - Command line tool to render videos from VBC files.
# Copyright (C) 2019 Mirko Hahn
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
#

import os
import os.path
from collections import namedtuple

NodeType = namedtuple('NodeType', ['color', 'font_color', 'has_number', 'is_filled', 'is_circle', 'name'])

def split_fields(value):
    fields = []
    start_hyphen = value.find('-')
    start_underscore = value.find('_')
    if start_hyphen < 0:
        start_hyphen = len(value)
    if start_underscore < 0:
        start_underscore = len(value)
    start = min(start_hyphen, start_underscore)
    while start < len(value):
        while (value[start] == '-' or value[start] == '_') and start < len(value):
            start += 1
        start_hyphen = value.find('-', start)
        start_underscore = value.find('_', start)
        if start_hyphen < 0:
            start_hyphen = len(value)
        if start_underscore < 0:
            start_underscore = len(value)
        end = min(start_hyphen, start_underscore)
        fields.append(value[start:end])
        start = end
    return fields

# Find script directory and VbcTool code
resourcedir = os.path.join(os.getcwd(), 'vbctool', 'vbctool', 'GRAPHResource')

# Determine paths of standard resource and palette file
palette_path = os.path.join(resourcedir, 'GRAPHrgb.txt')
resource_path = os.path.join(resourcedir, 'GRAPHStandardResource.rsc')

# Load palette
palette = dict()
with open(palette_path, 'r') as f:
    line = f.readline()
    while len(line) > 0:
        components = line.split()
        if len(components) == 4:
            palette[components[3]] = (
                int(components[0]),
                int(components[1]),
                int(components[2])
            )
        line = f.readline()
    f.close()

# Load resource descriptions
background_color = (255, 255, 255)
tree_level_sep = 4.0
tree_subtree_sep = 6.0
tree_sibling_sep = 6.0
tree_node_radius = 20.0
node_types = list()
edge_types = list()
with open(resource_path, 'r') as f:
    while True:
        # Read next line
        line = f.readline()
        if len(line) == 0:
            break

        # Recognize comments
        line = line.lstrip()
        if line.startswith('#'):
            continue

        # Split into variable name and value
        fields = line.split(':', 1)
        if len(fields) != 2:
            continue
        name, value = fields
        name.strip()

        # Process field
        if name == 'DrawAreaBackgroundColor':
            background_color = palette[value.strip()]
        elif name == 'TreeLevelSeparationValue':
            tree_level_sep = float(value.strip())
        elif name == 'TreeSubtreeSeparationValue':
            tree_subtree_sep = float(value.strip())
        elif name == 'TreeSiblingSeparationValue':
            tree_sibling_sep = float(value.strip())
        elif name == 'TreeNodeRadiusValue':
            tree_node_radius = float(value.strip())
        elif name.startswith('Nodes'):
            fields = name.split()
            if len(fields) != 2:
                continue
            type_idx = int(fields[1])

            fields = split_fields(value)
            if len(fields) < 4:
                continue

            node_color = palette[fields[0].strip()]
            font_color = palette[fields[1].strip()]
            flags = fields[3].strip()
            has_number = len(flags) > 0 and (flags[0] == 'y' or flags[0] == 'Y')
            is_filled = len(flags) > 1 and (flags[1] == 'y' or flags[1] == 'Y')
            is_circle = len(flags) > 2 and (flags[2] == 'y' or flags[2] == 'Y')
            custom_name = '' if len(fields) < 5 else fields[4].strip()

            while len(node_types) < type_idx:
                node_types.append(NodeType((0, 0, 0), (255, 255, 255), False, True, True, 'Undefined Node Type ' + str(len(node_types))))
            node_types.append(NodeType(node_color, font_color, has_number, is_filled, is_circle, custom_name))
        elif name.startswith('Edges'):
            fields = name.split()
            if len(fields) != 2:
                continue
            type_idx = int(fields[1])

            fields = split_fields(value)
            edge_color = palette[fields[0].strip()]

            while len(edge_types) < type_idx:
                edge_types.append((0, 0, 0))
            edge_types.append(edge_color)
    f.close()

# Write header
print('''// WARNING: THIS CODE IS AUTOMATICALLY GENERATED. DO NOT ALTER IT!

#include "Styles.hpp"


SkScalar tree_level_sep = SkScalar({});
SkScalar tree_subtree_sep = SkScalar({});
SkScalar tree_sibling_sep = SkScalar({});
SkScalar tree_node_radius = SkScalar({});
SkColor background_color = SkColorSetRGB({}, {}, {});

std::vector<NodeStyle> node_style_table {{'''.format(
    tree_level_sep, tree_subtree_sep, tree_sibling_sep, tree_node_radius,
    background_color[0], background_color[1], background_color[2]
))

bool_lit = ('false', 'true')
for style in node_types:
    print('    {{ SkColorSetRGB({}, {}, {}), SkColorSetRGB({}, {}, {}), {}, {}, {}, "{}" }},'.format(
        style.color[0], style.color[1], style.color[2],
        style.font_color[0], style.font_color[1], style.font_color[2],
        bool_lit[int(style.has_number)],
        bool_lit[int(style.is_filled)],
        bool_lit[int(style.is_circle)],
        style.name
    ))

print('''};

std::vector<EdgeStyle> edge_style_table {''')

for style in edge_types:
    print('    {{ SkColorSetRGB({}, {}, {}) }},'.format(
        style[0], style[1], style[2]
    ))

print('};')
