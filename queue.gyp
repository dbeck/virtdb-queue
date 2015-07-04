{
  'variables': {
    'queue_sources' :  [
                         'src/queue/simple_queue.cc',        'src/queue/simple_queue.hh',
                         'src/queue/multi_queue.cc',         'src/queue/multi_queue.hh',
                         'src/queue/sync_object.cc',         'src/queue/sync_object.hh',
                         'src/queue/mmapped_file.cc',        'src/queue/mmapped_file.hh',
                         # header only:
                         'src/queue/on_return.hh',
                         'src/queue/exception.hh',
                         'src/queue/params.hh',
                         'src/queue/varint.hh',
                       ],
  },
  'conditions': [
    ['OS=="mac"', {
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
              'OTHER_CFLAGS':  [ '-O3', ],
            },
          },
        },
        'include_dirs': [ './src/', '/usr/local/include/', '/usr/include/', ],
        'cflags':   [ '-Wall', '-fPIC', '-std=c++11', ],
        'defines':  [ 'PIC', 'STD_CXX_11', '_THREAD_SAFE', 'QUEUE_MAC_BUILD', 'NO_IPV6_SUPPORT', ],
        'xcode_settings': {
          'OTHER_CFLAGS':  [ '-std=c++11', ],
          'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',
        },
      },
    },],
    ['OS=="linux"', {
      'target_defaults': {
        'default_configuration': 'Debug',
        'configurations': {
          'Debug': {
            'defines':  [ 'DEBUG', '_DEBUG', ],
            'cflags':   [ '-O0', '-g3', ],
            'ldflags':  [ '-g3', ],
          },
          'Release': {
            'defines':  [ 'NDEBUG', 'RELEASE', ],
            'cflags':   [ '-O3', ],
          },
        },
        'include_dirs': [ './src/', '/usr/local/include/', '/usr/include/', ],
        'cflags':   [ '-Wall', '-fPIC', '-std=c++11', ],
        'defines':  [ 'PIC', 'STD_CXX_11', '_THREAD_SAFE', 'QUEUE_LINUX_BUILD', ],
        'link_settings': {
          'ldflags':   [ '-Wl,--no-as-needed', ],
          'libraries': [ '-lrt', ],
        },
      },
    },],
  ],
  'targets' : [
    {
      'target_name':     'queue',
      'type':            'static_library',
      'defines':       [ 'USING_QUEUE_LIB',  ],
      'sources':       [ '<@(queue_sources)', ],
    },
    {
      'target_name':     'queue_test',
      'type':            'executable',
      'dependencies':  [ 'queue', 'deps_/gtest/gyp/gtest.gyp:gtest_lib', ],
      'include_dirs':  [ './deps_/gtest/include/', ],
      'sources':       [ 'test/queue_test.cc', ],
    },
    {
      'target_name':     'sync_server_test',
      'type':            'executable',
      'dependencies':  [ 'queue', ],
      'sources':       [ 'test/sync_server_test.cc', ],
    },
    {
      'target_name':     'sync_client_test',
      'type':            'executable',
      'dependencies':  [ 'queue', ],
      'sources':       [ 'test/sync_client_test.cc', ],
    },
    {
      'target_name':     'simple_q_server_test',
      'type':            'executable',
      'dependencies':  [ 'queue', ],
      'sources':       [ 'test/simple_q_server_test.cc', ],
    },
    {
      'target_name':     'simple_q_client_test',
      'type':            'executable',
      'dependencies':  [ 'queue', ],
      'sources':       [ 'test/simple_q_client_test.cc', ],
    },
  ],
}
