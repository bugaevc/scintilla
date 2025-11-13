#!/usr/bin/env python3
# EnumGen.py - generate ScintillaEnums.h from Scintilla.iface

import sys, os.path

def join_normpath(*paths):
    return os.path.normpath(os.path.join(*paths))

scintilla_path = join_normpath(__file__, '..', '..')

try:
    import FileGenerator
    import Face
except ImportError:
    # Add ../scripts to sys.path, and try again
    scripts_path = join_normpath(scintilla_path, 'scripts')
    sys.path.insert(0, scripts_path)
    import FileGenerator
    import Face

f = Face.Face()
f.ReadFromFile(join_normpath(scintilla_path, 'include/Scintilla.iface'))

dead_values = [
    'INDIC_CONTAINER',
    'INDIC_IME',
    'INDIC_IME_MAX',
    'INDIC_MAX',
]

treat_as_flags = [
    'FindOption',
    'ChangeHistoryOption',
    'WrapVisualLocation',
    'DocumentOption',
    'VisiblePolicy',
    'CaretPolicy',
    'VirtualSpace',
    'Update',
    'KeyMod',
    'CaretStyle',
    'AutomaticFold',
]

def pascal_case_to_snake_case(s):
    """Convert PascalCase into snake_case

    >>> pascal_case_to_upper_snake_case('Hello')
    'hello'
    >>> pascal_case_to_upper_snake_case('ThisIsSomeName')
    'this_is_some_name'
    >> pascal_case_to_upper_snake_case('GetURLPath')
    'get_url_path'
    """

    res = ''
    for i, c in enumerate(s):
        if c.isupper():
            if res and (not s[i-1].isupper() or (i+1 < len(s) and not s[i+1].isupper())):
                res += '_'
        res += c.lower()
    return res

out = []
for enum_name in f.order:
    if enum_name == 'Lexer':
        continue
    enum_features = f.features[enum_name]
    if enum_features['Category'] == 'Deprecated':
        continue
    if enum_features['FeatureType'] != 'enu':
        continue

    enum_options = []
    if enum_name.endswith('Flags') or enum_name.endswith('Flag') or enum_name in treat_as_flags:
        enum_options.append('flags')
    enum_name_trimmed = (enum_name
        .removesuffix('Flags')
        .removesuffix('Flag')
        .removesuffix('Option')
        .removesuffix('Options')
    )
    snake_case_name = pascal_case_to_snake_case(enum_name)
    snake_case_name_trimmed = pascal_case_to_snake_case(enum_name_trimmed)
    enum_options.append('underscore_name=scintilla_' + snake_case_name)
    gen_enum_prefix = 'SCINTILLA_' + snake_case_name_trimmed.upper()
    enum_options.append('prefix=' + gen_enum_prefix)
    if out:
        out.append('')
    out.append('typedef enum /*< {} >*/'.format(','.join(enum_options)))
    out.append('{')

    prefixes = enum_features['Value'].split()
    for value_name in f.order:
        prefix_matched = None
        for p in prefixes:
            if value_name.startswith(p) and value_name not in dead_values:
                prefix_matched = p
        if prefix_matched is None:
            continue
        value_features = f.features[value_name]
        # Trim the prefix from value_name
        if value_name in f.aliases:
            trimmed_value_name = f.aliases[value_name]
        elif value_name == prefix_matched:
            trimmed_value_name = value_name
        else:
            trimmed_value_name = value_name[len(prefix_matched):]
        if trimmed_value_name.startswith('SC_'):
            trimmed_value_name = trimmed_value_name[3:]

        full_value_name = gen_enum_prefix + '_' + trimmed_value_name
        out.append('\t' + full_value_name + ' = ' + value_features['Value'] + ',')

    out.append('} Scintilla' + enum_name + ';')
    out.append('')
    out.append('SCINTILLA_AVAILABLE_IN_ALL')
    out.append('GType scintilla_' + snake_case_name + '_get_type(void);')
    out.append('')
    out.append('#define SCINTILLA_TYPE_' + snake_case_name.upper() + ' scintilla_' + snake_case_name + '_get_type()')

inpath = join_normpath(scintilla_path, 'gtk4', 'ScintillaEnums.h.template')
outpath = join_normpath(scintilla_path, 'gtk4', 'ScintillaEnums.h')

FileGenerator.GenerateFile(inpath, outpath, '/* ', False, out)
