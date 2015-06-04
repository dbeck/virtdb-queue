{
  'variables': {
    'queue_sources' :  [
                         'src/queue/queue.cc',         'src/queue/queue.hh',
                         'src/queue/is_folder.cc',     'src/queue/is_folder.hh',
                         'src/queue/owner_only.cc',    'src/queue/owner_only.hh',
                         'src/queue/publisher.cc',     'src/queue/publisher.hh',
                         'src/queue/subscriber.cc',    'src/queue/subscriber.hh',
                         'src/queue/sysinfo.cc',       'src/queue/sysinfo.hh',
                         'src/queue/exception.hh',     'src/queue/constants.hh',
                         'src/queue/shared_mem.cc',    'src/queue/shared_mem.hh',
                       ],
  },
  'target_defaults': {
    'default_configuration': 'Debug',
    'configurations': {
      'Debug': {
        'defines':  [ 'DEBUG', '_DEBUG', ],
        'cflags':   [ '-O0', '-g3', ],
        'ldflags':  [ '-g3', ],
        'xcode_settings': {
          'OTHER_CFLAGS':  [ '-O0', '-g3', ],
          'OTHER_LDFLAGS': [ '-g3', ],
        },
      },
      'Release': {
        'defines':  [ 'NDEBUG', 'RELEASE', ],
        'cflags':   [ '-O3', ],
        'xcode_settings': {
        },
      },
    },
    'include_dirs': [
                      './src/',
                      '/usr/local/include/',
                      '/usr/include/',
                    ],
    'cflags': [
                '-Wall',
                '-fPIC',
                '-std=c++11',
              ],
    'defines':  [
                  'PIC',
                  'STD_CXX_11',
                  '_THREAD_SAFE',
                ],
  },
  'conditions': [
    ['OS=="mac"', {
     'defines':            [ 'QUEUE_MAC_BUILD', 'NO_IPV6_SUPPORT', ],
     'xcode_settings':  {
       'GCC_ENABLE_CPP_EXCEPTIONS':    'YES',
       'OTHER_CFLAGS':               [ '-std=c++11', ],
     },
     }, ],
    ['OS=="linux"', {
     'defines':            [ 'QUEUE_LINUX_BUILD', ],
     'link_settings': {
       'ldflags':   [ '-Wl,--no-as-needed', ],
       'libraries': [ '-lrt', ],
      },
     },
    ],
  ],
  'targets' : [
    {
      'conditions': [
        ['OS=="mac"', {
          'variables':  { 'queue_root':  '<!(pwd)/', },
          'xcode_settings':  {
            'GCC_ENABLE_CPP_EXCEPTIONS':    'YES',
            'OTHER_CFLAGS':               [ '-std=c++11', ],
          },
          'direct_dependent_settings': {
            'include_dirs': [ '<(queue_root)/', ],
          },},],
        ['OS=="linux"', {
          'direct_dependent_settings': {
            'include_dirs':       [ '.', ],
          },},],
      ],
      'target_name':                   'queue',
      'type':                          'static_library',
      'defines':                     [ 'USING_QUEUE_LIB',  ],
      'sources':                     [ '<@(queue_sources)', ],
    },
    {
      'target_name':       'queue_test',
      'type':              'executable',
      'dependencies':  [ 'queue', 'deps_/gtest/gyp/gtest.gyp:gtest_lib', ],
      'include_dirs':  [ './deps_/gtest/include/', ],
      'sources':       [ 'test/queue_test.cc', ],
    },
  ],
}
