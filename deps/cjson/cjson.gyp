{
  'targets': [
    {
      'target_name': 'cjson',
      'type': 'static_library',
      'direct_dependent_settings': {
        'include_dirs': [
          './'
        ]
      },
      'include_dirs': [
        './'
      ],
      'sources': [
        'cJSON.c',
        'cJSON.h',
      ],
    },
  ],
}
