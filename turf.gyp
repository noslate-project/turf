{
  'variables': {
    'git_version': '<!(git describe --abbrev=6 --dirty --always --tags)',
    'cli_sources': [
      'src/misc.c',
      'src/stat.c',
      'src/realm.c',
      'src/turf.c',
      'src/sock.c',
      'src/daemon.c',
      'src/shell.c',
      'src/spec.json',
      'src/cli.c',
      'src/oci.c',
      'src/crc32.c',
      'src/ipc.c',
      'src/warmfork.c',
    ],
    'preload_sources': [
      'src/misc.c',
      'src/stat.c',
      'src/realm.c',
      'src/sock.c',
      'src/shell.c',
      'src/crc32.c',
      'src/ipc.c',
      'src/warmfork.c',
      'src/preload.c',
    ],
    'test_sources': [
      'test/test_bin.c',
      'test/test_cli.c',
      'test/test_ipc.c',
      'test/test_misc.c',
      'test/test_oci.c',
      'test/test_realm.c',
      'test/test_realm_linux_fallback.c',
      'test/test_realm_linux_sysadmin.c',
      'test/test_shell.c',
      'test/test_stat.c',
      'test/test_state.c',
      'test/test_turf.c',
      'test/test_warm_fork.c',
    ]
  },
  'target_defaults': {
    'cflags': [
      '-Wall', '-fvisibility=hidden', '-fPIC',
      '-ffunction-sections', '-fdata-sections',
    ],
    'conditions': [
      ['OS=="linux"', {
        'ldflags': [
          '-Wl,--gc-sections', '-Wl,--as-needed',
        ],
      }],
      ['OS=="darwin"', {
        'ldflags': [
          '-Wl,-dead_strip',
        ],
      }]
    ]
  },
  'targets': [
    {
      'target_name': 'turf',
      'type': 'executable',
      'dependencies': [
        'deps/cjson/cjson.gyp:cjson',
      ],
      'include_dirs': [
        'src',
        'deps',
        'include',
      ],
      'defines': [
        'GIT_VERSION="<(git_version)"'
      ],
      'sources': [
        '<@(cli_sources)',
        '<(INTERMEDIATE_DIR)/src_spec_json.c'
      ],
      'actions': [
        {
          'action_name': 'src_spec_json',
          'inputs': [
            'src/spec.json'
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/src_spec_json.c'
          ],
          'action': ['xxd', '-i', '<@(_inputs)', '<@(_outputs)'],
        },
      ],
    },
    {
      'target_name': 'libturf',
      'type': 'shared_library',
      'include_dirs': [
        'src',
        'deps',
        'include',
      ],
      'defines': [
        'GIT_VERSION="<(git_version)"'
      ],
      'sources': [
        '<@(preload_sources)',
      ],
      'libraries': [
        '-ldl',
      ],
      'direct_dependent_settings': {
        'xcode_settings': {
          'OTHER_LDFLAGS': [
            '-Wl,--version-script=preload.script',
          ],
        },
        'ldflags': [
          '-Wl,--version-script=preload.script',
        ],
      },
    },
    {
      'target_name': 'turf_test',
      'type': 'executable',
      'dependencies': [
        'deps/cjson/cjson.gyp:cjson',
      ],
      'include_dirs': [
        'src',
        'deps',
        'include',
      ],
      'defines': [
        'GIT_VERSION="<(git_version)"'
      ],
      'sources': [
        '<@(cli_sources)',
        '<@(test_sources)',
        '<(INTERMEDIATE_DIR)/src_spec_json.c'
      ],
      'actions': [
        {
          'action_name': 'src_spec_json',
          'inputs': [
            'src/spec.json'
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/src_spec_json.c'
          ],
          'action': ['xxd', '-i', '<@(_inputs)', '<@(_outputs)'],
        },
      ],
    },
  ],
}
