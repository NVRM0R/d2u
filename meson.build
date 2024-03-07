project('d2u', 'cpp',
  version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++23'])


deps = [dependency('sdbusplus')]

executable('d2u',
           'main.cpp',
           install : true,
           dependencies : deps
            )