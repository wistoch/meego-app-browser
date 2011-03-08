{
  'variables': {
    'chromium_code': 1,
    'chrome_grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/chrome',
    'webkit_grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/webkit',

    'app_grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/app',
    'app_strings_out_dir': '<(app_grit_out_dir)/app_strings',
    'app_locale_out_dir': '<(app_grit_out_dir)/app_locale_settings',

    # TODO(sgk): eliminate this; see comment in build/common.gypi
    'msvs_debug_link_incremental': '1',

    # Decide which .rc file to use for strings based on branding
    'conditions': [
      ['branding=="Chrome"', {
        'strings_rc_name': 'strings_google_chrome',
      }, {
        'strings_rc_name': 'strings',
      }],
    ],
  },
  'target_defaults': {
    'type': 'loadable_module',
    'dependencies': [
      '../../chrome.gyp:chrome_strings',
      '../../chrome.gyp:platform_locale_settings',
      '../../../ui/base/strings/ui_strings.gyp:ui_strings',
      '../../../webkit/support/webkit_support.gyp:webkit_strings',
    ],
    'msvs_settings': {
      'VCLinkerTool': {
        'BaseAddress': '0x3CF00000',
        'OutputFile': '$(OutDir)\\locales\\$(ProjectName).dll',
        'LinkIncremental': '1',  # 1 == No
        'LinkTimeCodeGeneration': '0',
        'ResourceOnlyDLL': 'true',
      },
    },
    'defines': [
      '_USRDLL',
      'GENERATED_RESOURCES_DLL_EXPORTS',
    ],
    'include_dirs': [
      '<(chrome_grit_out_dir)',
    ],
  },
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'am',
          'msvs_guid': 'A59E9C5D-5140-4D8C-A1B5-58044D577AAF',
          'sources': [
            '<(chrome_grit_out_dir)/platform_locale_settings_am.rc',
            '<(chrome_grit_out_dir)/generated_resources_am.rc',
            '<(chrome_grit_out_dir)/locale_settings_am.rc',
            '<(chrome_grit_out_dir)/<(strings_rc_name)_am.rc',
            '<(webkit_grit_out_dir)/webkit_strings_am.rc',
            '<(app_strings_out_dir)/app_strings_am.rc',
            '<(app_locale_out_dir)/app_locale_settings_am.rc',
          ],
        },
        {
          'target_name': 'ar',
          'msvs_guid': '3AB90E6A-56FF-4C9D-B918-AB76DDBF8BE8',
          'sources': [
            '<(chrome_grit_out_dir)/platform_locale_settings_ar.rc',
            '<(chrome_grit_out_dir)/generated_resources_ar.rc',
            '<(chrome_grit_out_dir)/locale_settings_ar.rc',
            '<(chrome_grit_out_dir)/<(strings_rc_name)_ar.rc',
            '<(webkit_grit_out_dir)/webkit_strings_ar.rc',
            '<(app_strings_out_dir)/app_strings_ar.rc',
            '<(app_locale_out_dir)/app_locale_settings_ar.rc',
          ],
        },
        {
          'target_name': 'bg',
          'msvs_guid': '5BDB7EE1-A1FD-410C-9598-21519A1B7952',
          'sources': [
            '<(chrome_grit_out_dir)/platform_locale_settings_bg.rc',
            '<(chrome_grit_out_dir)/generated_resources_bg.rc',
            '<(chrome_grit_out_dir)/locale_settings_bg.rc',
            '<(chrome_grit_out_dir)/<(strings_rc_name)_bg.rc',
            '<(webkit_grit_out_dir)/webkit_strings_bg.rc',
            '<(app_strings_out_dir)/app_strings_bg.rc',
            '<(app_locale_out_dir)/app_locale_settings_bg.rc',
          ],
        },
        {
          'target_name': 'bn',
          'msvs_guid': '4B60E8B8-416F-40B2-8A54-F75970A21992',
          'sources': [
            '<(chrome_grit_out_dir)/platform_locale_settings_bn.rc',
            '<(chrome_grit_out_dir)/generated_resources_bn.rc',
            '<(chrome_grit_out_dir)/locale_settings_bn.rc',
            '<(chrome_grit_out_dir)/<(strings_rc_name)_bn.rc',
            '<(webkit_grit_out_dir)/webkit_strings_bn.rc',
            '<(app_strings_out_dir)/app_strings_bn.rc',
            '<(app_locale_out_dir)/app_locale_settings_bn.rc',
          ],
        },
        {
          'target_name': 'ca',
          'msvs_guid': 'F7790A54-4078-4E4A-8231-818BE9FB1F94',
          'sources': [
            '<(chrome_grit_out_dir)/platform_locale_settings_ca.rc',
            '<(chrome_grit_out_dir)/generated_resources_ca.rc',
            '<(chrome_grit_out_dir)/locale_settings_ca.rc',
            '<(chrome_grit_out_dir)/<(strings_rc_name)_ca.rc',
            '<(webkit_grit_out_dir)/webkit_strings_ca.rc',
            '<(app_strings_out_dir)/app_strings_ca.rc',
            '<(app_locale_out_dir)/app_locale_settings_ca.rc',
          ],
        },
        {
          'target_name': 'cs',
          'msvs_guid': '7EA8C4AB-F9C6-4FA1-8B0A-74F5650430B2',
          'sources': [
            '<(chrome_grit_out_dir)/platform_locale_settings_cs.rc',
            '<(chrome_grit_out_dir)/generated_resources_cs.rc',
            '<(chrome_grit_out_dir)/locale_settings_cs.rc',
            '<(chrome_grit_out_dir)/<(strings_rc_name)_cs.rc',
            '<(webkit_grit_out_dir)/webkit_strings_cs.rc',
            '<(app_strings_out_dir)/app_strings_cs.rc',
            '<(app_locale_out_dir)/app_locale_settings_cs.rc',
          ],
        },
        {
          'target_name': 'da',
          'msvs_guid': 'A493331B-3180-49FE-8D0E-D121645E63AD',
          'sources': [
            '<(chrome_grit_out_dir)/platform_locale_settings_da.rc',
            '<(chrome_grit_out_dir)/generated_resources_da.rc',
            '<(chrome_grit_out_dir)/locale_settings_da.rc',
            '<(chrome_grit_out_dir)/<(strings_rc_name)_da.rc',
            '<(webkit_grit_out_dir)/webkit_strings_da.rc',
            '<(app_strings_out_dir)/app_strings_da.rc',
            '<(app_locale_out_dir)/app_locale_settings_da.rc',
          ],
        },
        {
          'target_name': 'de',
          'msvs_guid': 'BA62FF5E-08A6-4102-9675-C12E8C9D4CC0',
          'sources': [
            '<(chrome_grit_out_dir)/platform_locale_settings_de.rc',
            '<(chrome_grit_out_dir)/generated_resources_de.rc',
            '<(chrome_grit_out_dir)/locale_settings_de.rc',
            '<(chrome_grit_out_dir)/<(strings_rc_name)_de.rc',
            '<(webkit_grit_out_dir)/webkit_strings_de.rc',
            '<(app_strings_out_dir)/app_strings_de.rc',
            '<(app_locale_out_dir)/app_locale_settings_de.rc',
          ],
        },
        {
          'target_name': 'el',
          'msvs_guid': 'D314F1B3-9299-4866-8362-08BF811B0FA3',
          'sources': [
            '<(chrome_grit_out_dir)/platform_locale_settings_el.rc',
            '<(chrome_grit_out_dir)/generated_resources_el.rc',
            '<(chrome_grit_out_dir)/locale_settings_el.rc',
            '<(chrome_grit_out_dir)/<(strings_rc_name)_el.rc',
            '<(webkit_grit_out_dir)/webkit_strings_el.rc',
            '<(app_strings_out_dir)/app_strings_el.rc',
            '<(app_locale_out_dir)/app_locale_settings_el.rc',
          ],
        },
        {
          'target_name': 'en-GB',
          'msvs_guid': '34231B28-C51C-4C1C-AF07-C763668B1404',
          'sources': [
            '<(chrome_grit_out_dir)/platform_locale_settings_en-GB.rc',
            '<(chrome_grit_out_dir)/generated_resources_en-GB.rc',
            '<(chrome_grit_out_dir)/locale_settings_en-GB.rc',
            '<(chrome_grit_out_dir)/<(strings_rc_name)_en-GB.rc',
            '<(webkit_grit_out_dir)/webkit_strings_en-GB.rc',
            '<(app_strings_out_dir)/app_strings_en-GB.rc',
            '<(app_locale_out_dir)/app_locale_settings_en-GB.rc',
          ],
        },
        {
          'target_name': 'en-US',
          'msvs_guid': 'CAE2D1E6-3F19-492F-A35C-68AA7ACAD6D3',
          'sources': [
            '<(chrome_grit_out_dir)/platform_locale_settings_en-US.rc',
            '<(chrome_grit_out_dir)/generated_resources_en-US.rc',
            '<(chrome_grit_out_dir)/locale_settings_en-US.rc',
            '<(chrome_grit_out_dir)/<(strings_rc_name)_en-US.rc',
            '<(webkit_grit_out_dir)/webkit_strings_en-US.rc',
            '<(app_strings_out_dir)/app_strings_en-US.rc',
            '<(app_locale_out_dir)/app_locale_settings_en-US.rc',
          ],
        },
        {
          'target_name': 'es-419',
          'msvs_guid': 'FA660037-EB40-4A43-AA9D-9653C57F2789',
          'sources': [
            '<(chrome_grit_out_dir)/platform_locale_settings_es-419.rc',
            '<(chrome_grit_out_dir)/generated_resources_es-419.rc',
            '<(chrome_grit_out_dir)/locale_settings_es-419.rc',
            '<(chrome_grit_out_dir)/<(strings_rc_name)_es-419.rc',
            '<(webkit_grit_out_dir)/webkit_strings_es-419.rc',
            '<(app_strings_out_dir)/app_strings_es-419.rc',
            '<(app_locale_out_dir)/app_locale_settings_es-419.rc',
          ],
        },
        {
          'target_name': 'es',
          'msvs_guid': '5AEA4BF6-27CD-47FC-9370-D87771CFA196',
          'sources': [
            '<(chrome_grit_out_dir)/platform_locale_settings_es.rc',
            '<(chrome_grit_out_dir)/generated_resources_es.rc',
            '<(chrome_grit_out_dir)/locale_settings_es.rc',
            '<(chrome_grit_out_dir)/<(strings_rc_name)_es.rc',
            '<(webkit_grit_out_dir)/webkit_strings_es.rc',
            '<(app_strings_out_dir)/app_strings_es.rc',
            '<(app_locale_out_dir)/app_locale_settings_es.rc',
          ],
        },
        {
          'target_name': 'et',
          'msvs_guid': '0557BC3C-DE87-4127-BDAA-9BD9BDB13FB4',
          'sources': [
            '<(chrome_grit_out_dir)/platform_locale_settings_et.rc',
            '<(chrome_grit_out_dir)/generated_resources_et.rc',
            '<(chrome_grit_out_dir)/locale_settings_et.rc',
            '<(chrome_grit_out_dir)/<(strings_rc_name)_et.rc',
            '<(webkit_grit_out_dir)/webkit_strings_et.rc',
            '<(app_strings_out_dir)/app_strings_et.rc',
            '<(app_locale_out_dir)/app_locale_settings_et.rc',
          ],
        },
        {
          'target_name': 'fa',
          'msvs_guid': '347C5804-9391-4B91-A301-9D30B5E089BA',
          'sources': [
            '<(chrome_grit_out_dir)/platform_locale_settings_fa.rc',
            '<(chrome_grit_out_dir)/generated_resources_fa.rc',
            '<(chrome_grit_out_dir)/locale_settings_fa.rc',
            '<(chrome_grit_out_dir)/<(strings_rc_name)_fa.rc',
            '<(webkit_grit_out_dir)/webkit_strings_fa.rc',
            '<(app_strings_out_dir)/app_strings_fa.rc',
            '<(app_locale_out_dir)/app_locale_settings_fa.rc',
          ],
        },
        {
          'target_name': 'fi',
          'msvs_guid': '64D81334-DE73-457D-8FC1-9492508A2663',
          'sources': [
            '<(chrome_grit_out_dir)/platform_locale_settings_fi.rc',
            '<(chrome_grit_out_dir)/generated_resources_fi.rc',
            '<(chrome_grit_out_dir)/locale_settings_fi.rc',
            '<(chrome_grit_out_dir)/<(strings_rc_name)_fi.rc',
            '<(webkit_grit_out_dir)/webkit_strings_fi.rc',
            '<(app_strings_out_dir)/app_strings_fi.rc',
            '<(app_locale_out_dir)/app_locale_settings_fi.rc',
          ],
        },
        {
          'target_name': 'fil',
          'msvs_guid': '3A932C39-AFA9-4BDC-B775-F71A426D04BF',
          'sources': [
            '<(chrome_grit_out_dir)/platform_locale_settings_fil.rc',
            '<(chrome_grit_out_dir)/generated_resources_fil.rc',
            '<(chrome_grit_out_dir)/locale_settings_fil.rc',
            '<(chrome_grit_out_dir)/<(strings_rc_name)_fil.rc',
            '<(webkit_grit_out_dir)/webkit_strings_fil.rc',
            '<(app_strings_out_dir)/app_strings_fil.rc',
            '<(app_locale_out_dir)/app_locale_settings_fil.rc',
          ],
        },
        {
          'target_name': 'fr',
          'msvs_guid': '0D54A5C4-B78B-41A2-BF8A-5DA48AC90495',
          'sources': [
            '<(chrome_grit_out_dir)/platform_locale_settings_fr.rc',
            '<(chrome_grit_out_dir)/generated_resources_fr.rc',
            '<(chrome_grit_out_dir)/locale_settings_fr.rc',
            '<(chrome_grit_out_dir)/<(strings_rc_name)_fr.rc',
            '<(webkit_grit_out_dir)/webkit_strings_fr.rc',
            '<(app_strings_out_dir)/app_strings_fr.rc',
            '<(app_locale_out_dir)/app_locale_settings_fr.rc',
          ],
        },
        {
          'target_name': 'gu',
          'msvs_guid': '256DECCE-9886-4C21-96A5-EE47DF5E07E9',
          'sources': [
            '<(chrome_grit_out_dir)/platform_locale_settings_gu.rc',
            '<(chrome_grit_out_dir)/generated_resources_gu.rc',
            '<(chrome_grit_out_dir)/locale_settings_gu.rc',
            '<(chrome_grit_out_dir)/<(strings_rc_name)_gu.rc',
            '<(webkit_grit_out_dir)/webkit_strings_gu.rc',
            '<(app_strings_out_dir)/app_strings_gu.rc',
            '<(app_locale_out_dir)/app_locale_settings_gu.rc',
          ],
        },
        {
          'target_name': 'he',
          'msvs_guid': 'A28310B8-7BD0-4CDF-A7D8-59CAB42AA1C4',
          'sources': [
            '<(chrome_grit_out_dir)/platform_locale_settings_he.rc',
            '<(chrome_grit_out_dir)/generated_resources_he.rc',
            '<(chrome_grit_out_dir)/locale_settings_he.rc',
            '<(chrome_grit_out_dir)/<(strings_rc_name)_he.rc',
            '<(webkit_grit_out_dir)/webkit_strings_he.rc',
            '<(app_strings_out_dir)/app_strings_he.rc',
            '<(app_locale_out_dir)/app_locale_settings_he.rc',
          ],
        },
        {
          'target_name': 'hi',
          'msvs_guid': '228DD844-9926-420E-B193-6973BF2A4D0B',
          'sources': [
            '<(chrome_grit_out_dir)/platform_locale_settings_hi.rc',
            '<(chrome_grit_out_dir)/generated_resources_hi.rc',
            '<(chrome_grit_out_dir)/locale_settings_hi.rc',
            '<(chrome_grit_out_dir)/<(strings_rc_name)_hi.rc',
            '<(webkit_grit_out_dir)/webkit_strings_hi.rc',
            '<(app_strings_out_dir)/app_strings_hi.rc',
            '<(app_locale_out_dir)/app_locale_settings_hi.rc',
          ],
        },
        {
          'target_name': 'hr',
          'msvs_guid': 'CE1426F6-7D2B-4574-9929-58387BF7B05F',
          'sources': [
            '<(chrome_grit_out_dir)/platform_locale_settings_hr.rc',
            '<(chrome_grit_out_dir)/generated_resources_hr.rc',
            '<(chrome_grit_out_dir)/locale_settings_hr.rc',
            '<(chrome_grit_out_dir)/<(strings_rc_name)_hr.rc',
            '<(webkit_grit_out_dir)/webkit_strings_hr.rc',
            '<(app_strings_out_dir)/app_strings_hr.rc',
            '<(app_locale_out_dir)/app_locale_settings_hr.rc',
          ],
        },
        {
          'target_name': 'hu',
          'msvs_guid': 'AFF332BF-AF3D-4D35-86FC-42A727F01D36',
          'sources': [
            '<(chrome_grit_out_dir)/platform_locale_settings_hu.rc',
            '<(chrome_grit_out_dir)/generated_resources_hu.rc',
            '<(chrome_grit_out_dir)/locale_settings_hu.rc',
            '<(chrome_grit_out_dir)/<(strings_rc_name)_hu.rc',
            '<(webkit_grit_out_dir)/webkit_strings_hu.rc',
            '<(app_strings_out_dir)/app_strings_hu.rc',
            '<(app_locale_out_dir)/app_locale_settings_hu.rc',
          ],
        },
        {
          'target_name': 'id',
          'msvs_guid': 'E3DF045F-2174-4685-9CF7-0630A79F324B',
          'sources': [
            '<(chrome_grit_out_dir)/platform_locale_settings_id.rc',
            '<(chrome_grit_out_dir)/generated_resources_id.rc',
            '<(chrome_grit_out_dir)/locale_settings_id.rc',
            '<(chrome_grit_out_dir)/<(strings_rc_name)_id.rc',
            '<(webkit_grit_out_dir)/webkit_strings_id.rc',
            '<(app_strings_out_dir)/app_strings_id.rc',
            '<(app_locale_out_dir)/app_locale_settings_id.rc',
          ],
        },
        {
          'target_name': 'it',
          'msvs_guid': '275F2993-EE9B-4E00-9C85-10A182FD423A',
          'sources': [
            '<(chrome_grit_out_dir)/platform_locale_settings_it.rc',
            '<(chrome_grit_out_dir)/generated_resources_it.rc',
            '<(chrome_grit_out_dir)/locale_settings_it.rc',
            '<(chrome_grit_out_dir)/<(strings_rc_name)_it.rc',
            '<(webkit_grit_out_dir)/webkit_strings_it.rc',
            '<(app_strings_out_dir)/app_strings_it.rc',
            '<(app_locale_out_dir)/app_locale_settings_it.rc',
          ],
        },
        {
          'target_name': 'ja',
          'msvs_guid': 'B2D715CE-4CBB-415A-A032-E700C90ADF91',
          'sources': [
            '<(chrome_grit_out_dir)/platform_locale_settings_ja.rc',
            '<(chrome_grit_out_dir)/generated_resources_ja.rc',
            '<(chrome_grit_out_dir)/locale_settings_ja.rc',
            '<(chrome_grit_out_dir)/<(strings_rc_name)_ja.rc',
            '<(webkit_grit_out_dir)/webkit_strings_ja.rc',
            '<(app_strings_out_dir)/app_strings_ja.rc',
            '<(app_locale_out_dir)/app_locale_settings_ja.rc',
          ],
        },
        {
          'target_name': 'kn',
          'msvs_guid': '3E6B24F6-9FA9-4066-859E-BF747FA3080A',
          'sources': [
            '<(chrome_grit_out_dir)/platform_locale_settings_kn.rc',
            '<(chrome_grit_out_dir)/generated_resources_kn.rc',
            '<(chrome_grit_out_dir)/locale_settings_kn.rc',
            '<(chrome_grit_out_dir)/<(strings_rc_name)_kn.rc',
            '<(webkit_grit_out_dir)/webkit_strings_kn.rc',
            '<(app_strings_out_dir)/app_strings_kn.rc',
            '<(app_locale_out_dir)/app_locale_settings_kn.rc',
          ],
        },
        {
          'target_name': 'ko',
          'msvs_guid': '32167995-4014-4E4C-983B-F7E17C24EB25',
          'sources': [
            '<(chrome_grit_out_dir)/platform_locale_settings_ko.rc',
            '<(chrome_grit_out_dir)/generated_resources_ko.rc',
            '<(chrome_grit_out_dir)/locale_settings_ko.rc',
            '<(chrome_grit_out_dir)/<(strings_rc_name)_ko.rc',
            '<(webkit_grit_out_dir)/webkit_strings_ko.rc',
            '<(app_strings_out_dir)/app_strings_ko.rc',
            '<(app_locale_out_dir)/app_locale_settings_ko.rc',
          ],
        },
        {
          'target_name': 'lt',
          'msvs_guid': '80E37CB5-059D-4F4B-AEF6-08265468D368',
          'sources': [
            '<(chrome_grit_out_dir)/platform_locale_settings_lt.rc',
            '<(chrome_grit_out_dir)/generated_resources_lt.rc',
            '<(chrome_grit_out_dir)/locale_settings_lt.rc',
            '<(chrome_grit_out_dir)/<(strings_rc_name)_lt.rc',
            '<(webkit_grit_out_dir)/webkit_strings_lt.rc',
            '<(app_strings_out_dir)/app_strings_lt.rc',
            '<(app_locale_out_dir)/app_locale_settings_lt.rc',
          ],
        },
        {
          'target_name': 'lv',
          'msvs_guid': 'A5C5D801-4026-49F2-BBF1-250941855306',
          'sources': [
            '<(chrome_grit_out_dir)/platform_locale_settings_lv.rc',
            '<(chrome_grit_out_dir)/generated_resources_lv.rc',
            '<(chrome_grit_out_dir)/locale_settings_lv.rc',
            '<(chrome_grit_out_dir)/<(strings_rc_name)_lv.rc',
            '<(webkit_grit_out_dir)/webkit_strings_lv.rc',
            '<(app_strings_out_dir)/app_strings_lv.rc',
            '<(app_locale_out_dir)/app_locale_settings_lv.rc',
          ],
        },
        {
          'target_name': 'ml',
          'msvs_guid': 'CAB69303-0F02-4C68-A12E-FFE55DB52526',
          'sources': [
            '<(chrome_grit_out_dir)/platform_locale_settings_ml.rc',
            '<(chrome_grit_out_dir)/generated_resources_ml.rc',
            '<(chrome_grit_out_dir)/locale_settings_ml.rc',
            '<(chrome_grit_out_dir)/<(strings_rc_name)_ml.rc',
            '<(webkit_grit_out_dir)/webkit_strings_ml.rc',
            '<(app_strings_out_dir)/app_strings_ml.rc',
            '<(app_locale_out_dir)/app_locale_settings_ml.rc',
          ],
        },
        {
          'target_name': 'mr',
          'msvs_guid': 'A464166F-8507-49B4-9B02-5CB77C498B25',
          'sources': [
            '<(chrome_grit_out_dir)/platform_locale_settings_mr.rc',
            '<(chrome_grit_out_dir)/generated_resources_mr.rc',
            '<(chrome_grit_out_dir)/locale_settings_mr.rc',
            '<(chrome_grit_out_dir)/<(strings_rc_name)_mr.rc',
            '<(webkit_grit_out_dir)/webkit_strings_mr.rc',
            '<(app_strings_out_dir)/app_strings_mr.rc',
            '<(app_locale_out_dir)/app_locale_settings_mr.rc',
          ],
        },
        {
          'target_name': 'nb',
          'msvs_guid': 'B30B0E1F-1CE9-4DEF-A752-7498FD709C1F',
          'sources': [
            '<(chrome_grit_out_dir)/platform_locale_settings_nb.rc',
            '<(chrome_grit_out_dir)/generated_resources_nb.rc',
            '<(chrome_grit_out_dir)/locale_settings_nb.rc',
            '<(chrome_grit_out_dir)/<(strings_rc_name)_nb.rc',
            '<(webkit_grit_out_dir)/webkit_strings_nb.rc',
            '<(app_strings_out_dir)/app_strings_nb.rc',
            '<(app_locale_out_dir)/app_locale_settings_nb.rc',
          ],
        },
        {
          'target_name': 'nl',
          'msvs_guid': '63011A7B-CE4D-4DF1-B5DA-1B133C14A2E8',
          'sources': [
            '<(chrome_grit_out_dir)/platform_locale_settings_nl.rc',
            '<(chrome_grit_out_dir)/generated_resources_nl.rc',
            '<(chrome_grit_out_dir)/locale_settings_nl.rc',
            '<(chrome_grit_out_dir)/<(strings_rc_name)_nl.rc',
            '<(webkit_grit_out_dir)/webkit_strings_nl.rc',
            '<(app_strings_out_dir)/app_strings_nl.rc',
            '<(app_locale_out_dir)/app_locale_settings_nl.rc',
          ],
        },
        {
          'target_name': 'pl',
          'msvs_guid': '9F53807E-9382-47BD-8371-E5D04F517E9C',
          'sources': [
            '<(chrome_grit_out_dir)/platform_locale_settings_pl.rc',
            '<(chrome_grit_out_dir)/generated_resources_pl.rc',
            '<(chrome_grit_out_dir)/locale_settings_pl.rc',
            '<(chrome_grit_out_dir)/<(strings_rc_name)_pl.rc',
            '<(webkit_grit_out_dir)/webkit_strings_pl.rc',
            '<(app_strings_out_dir)/app_strings_pl.rc',
            '<(app_locale_out_dir)/app_locale_settings_pl.rc',
          ],
        },
        {
          'target_name': 'pt-BR',
          'msvs_guid': '2F914112-2657-49EC-8EA6-3BA63340DE27',
          'sources': [
            '<(chrome_grit_out_dir)/platform_locale_settings_pt-BR.rc',
            '<(chrome_grit_out_dir)/generated_resources_pt-BR.rc',
            '<(chrome_grit_out_dir)/locale_settings_pt-BR.rc',
            '<(chrome_grit_out_dir)/<(strings_rc_name)_pt-BR.rc',
            '<(webkit_grit_out_dir)/webkit_strings_pt-BR.rc',
            '<(app_strings_out_dir)/app_strings_pt-BR.rc',
            '<(app_locale_out_dir)/app_locale_settings_pt-BR.rc',
          ],
        },
        {
          'target_name': 'pt-PT',
          'msvs_guid': '0A13F602-B497-4BC1-ABD8-03CA8E95B2AF',
          'sources': [
            '<(chrome_grit_out_dir)/platform_locale_settings_pt-PT.rc',
            '<(chrome_grit_out_dir)/generated_resources_pt-PT.rc',
            '<(chrome_grit_out_dir)/locale_settings_pt-PT.rc',
            '<(chrome_grit_out_dir)/<(strings_rc_name)_pt-PT.rc',
            '<(webkit_grit_out_dir)/webkit_strings_pt-PT.rc',
            '<(app_strings_out_dir)/app_strings_pt-PT.rc',
            '<(app_locale_out_dir)/app_locale_settings_pt-PT.rc',
          ],
        },
        {
          'target_name': 'ro',
          'msvs_guid': 'C70D3509-57C4-4326-90C1-2EC0AE34848D',
          'sources': [
            '<(chrome_grit_out_dir)/platform_locale_settings_ro.rc',
            '<(chrome_grit_out_dir)/generated_resources_ro.rc',
            '<(chrome_grit_out_dir)/locale_settings_ro.rc',
            '<(chrome_grit_out_dir)/<(strings_rc_name)_ro.rc',
            '<(webkit_grit_out_dir)/webkit_strings_ro.rc',
            '<(app_strings_out_dir)/app_strings_ro.rc',
            '<(app_locale_out_dir)/app_locale_settings_ro.rc',
          ],
        },
        {
          'target_name': 'ru',
          'msvs_guid': '7D456640-3619-4D23-A56D-E0084400CCBF',
          'sources': [
            '<(chrome_grit_out_dir)/platform_locale_settings_ru.rc',
            '<(chrome_grit_out_dir)/generated_resources_ru.rc',
            '<(chrome_grit_out_dir)/locale_settings_ru.rc',
            '<(chrome_grit_out_dir)/<(strings_rc_name)_ru.rc',
            '<(webkit_grit_out_dir)/webkit_strings_ru.rc',
            '<(app_strings_out_dir)/app_strings_ru.rc',
            '<(app_locale_out_dir)/app_locale_settings_ru.rc',
          ],
        },
        {
          'target_name': 'sk',
          'msvs_guid': '82F5BFE5-FDCE-47D4-8B38-BEEBED561681',
          'sources': [
            '<(chrome_grit_out_dir)/platform_locale_settings_sk.rc',
            '<(chrome_grit_out_dir)/generated_resources_sk.rc',
            '<(chrome_grit_out_dir)/locale_settings_sk.rc',
            '<(chrome_grit_out_dir)/<(strings_rc_name)_sk.rc',
            '<(webkit_grit_out_dir)/webkit_strings_sk.rc',
            '<(app_strings_out_dir)/app_strings_sk.rc',
            '<(app_locale_out_dir)/app_locale_settings_sk.rc',
          ],
        },
        {
          'target_name': 'sl',
          'msvs_guid': 'C2A444C2-9D74-4AD7-AE7C-04F5EDA17060',
          'sources': [
            '<(chrome_grit_out_dir)/platform_locale_settings_sl.rc',
            '<(chrome_grit_out_dir)/generated_resources_sl.rc',
            '<(chrome_grit_out_dir)/locale_settings_sl.rc',
            '<(chrome_grit_out_dir)/<(strings_rc_name)_sl.rc',
            '<(webkit_grit_out_dir)/webkit_strings_sl.rc',
            '<(app_strings_out_dir)/app_strings_sl.rc',
            '<(app_locale_out_dir)/app_locale_settings_sl.rc',
          ],
        },
        {
          'target_name': 'sr',
          'msvs_guid': '300C6A09-663E-48B6-8E07-A0D50CAF8F25',
          'sources': [
            '<(chrome_grit_out_dir)/platform_locale_settings_sr.rc',
            '<(chrome_grit_out_dir)/generated_resources_sr.rc',
            '<(chrome_grit_out_dir)/locale_settings_sr.rc',
            '<(chrome_grit_out_dir)/<(strings_rc_name)_sr.rc',
            '<(webkit_grit_out_dir)/webkit_strings_sr.rc',
            '<(app_strings_out_dir)/app_strings_sr.rc',
            '<(app_locale_out_dir)/app_locale_settings_sr.rc',
          ],
        },
        {
          'target_name': 'sv',
          'msvs_guid': 'B0D5BD91-6153-4CA6-BC2F-4E3BD43E5DB7',
          'sources': [
            '<(chrome_grit_out_dir)/platform_locale_settings_sv.rc',
            '<(chrome_grit_out_dir)/generated_resources_sv.rc',
            '<(chrome_grit_out_dir)/locale_settings_sv.rc',
            '<(chrome_grit_out_dir)/<(strings_rc_name)_sv.rc',
            '<(webkit_grit_out_dir)/webkit_strings_sv.rc',
            '<(app_strings_out_dir)/app_strings_sv.rc',
            '<(app_locale_out_dir)/app_locale_settings_sv.rc',
          ],
        },
        {
          'target_name': 'sw',
          'msvs_guid': 'CBB54535-5590-464D-BB3A-631DAD11EBB5',
          'sources': [
            '<(chrome_grit_out_dir)/platform_locale_settings_sw.rc',
            '<(chrome_grit_out_dir)/generated_resources_sw.rc',
            '<(chrome_grit_out_dir)/locale_settings_sw.rc',
            '<(chrome_grit_out_dir)/<(strings_rc_name)_sw.rc',
            '<(webkit_grit_out_dir)/webkit_strings_sw.rc',
            '<(app_strings_out_dir)/app_strings_sw.rc',
            '<(app_locale_out_dir)/app_locale_settings_sw.rc',
          ],
        },
        {
          'target_name': 'ta',
          'msvs_guid': '7A0BA0C5-0D90-49AE-919A-4BE096F69E4F',
          'sources': [
            '<(chrome_grit_out_dir)/platform_locale_settings_ta.rc',
            '<(chrome_grit_out_dir)/generated_resources_ta.rc',
            '<(chrome_grit_out_dir)/locale_settings_ta.rc',
            '<(chrome_grit_out_dir)/<(strings_rc_name)_ta.rc',
            '<(webkit_grit_out_dir)/webkit_strings_ta.rc',
            '<(app_strings_out_dir)/app_strings_ta.rc',
            '<(app_locale_out_dir)/app_locale_settings_ta.rc',
          ],
        },
        {
          'target_name': 'te',
          'msvs_guid': '9D13D9B8-6C28-42A7-935C-B769EBC55BAA',
          'sources': [
            '<(chrome_grit_out_dir)/platform_locale_settings_te.rc',
            '<(chrome_grit_out_dir)/generated_resources_te.rc',
            '<(chrome_grit_out_dir)/locale_settings_te.rc',
            '<(chrome_grit_out_dir)/<(strings_rc_name)_te.rc',
            '<(webkit_grit_out_dir)/webkit_strings_te.rc',
            '<(app_strings_out_dir)/app_strings_te.rc',
            '<(app_locale_out_dir)/app_locale_settings_te.rc',
          ],
        },
        {
          'target_name': 'th',
          'msvs_guid': '226B3533-1FF3-42F6-A8E3-C4DDBC955290',
          'sources': [
            '<(chrome_grit_out_dir)/platform_locale_settings_th.rc',
            '<(chrome_grit_out_dir)/generated_resources_th.rc',
            '<(chrome_grit_out_dir)/locale_settings_th.rc',
            '<(chrome_grit_out_dir)/<(strings_rc_name)_th.rc',
            '<(webkit_grit_out_dir)/webkit_strings_th.rc',
            '<(app_strings_out_dir)/app_strings_th.rc',
            '<(app_locale_out_dir)/app_locale_settings_th.rc',
          ],
        },
        {
          'target_name': 'tr',
          'msvs_guid': '65C78BBB-8FCB-48E4-94C8-1F0F981929AF',
          'sources': [
            '<(chrome_grit_out_dir)/platform_locale_settings_tr.rc',
            '<(chrome_grit_out_dir)/generated_resources_tr.rc',
            '<(chrome_grit_out_dir)/locale_settings_tr.rc',
            '<(chrome_grit_out_dir)/<(strings_rc_name)_tr.rc',
            '<(webkit_grit_out_dir)/webkit_strings_tr.rc',
            '<(app_strings_out_dir)/app_strings_tr.rc',
            '<(app_locale_out_dir)/app_locale_settings_tr.rc',
          ],
        },
        {
          'target_name': 'uk',
          'msvs_guid': '182D578D-2DAC-4BB7-AFEC-9A2855E56F94',
          'sources': [
            '<(chrome_grit_out_dir)/platform_locale_settings_uk.rc',
            '<(chrome_grit_out_dir)/generated_resources_uk.rc',
            '<(chrome_grit_out_dir)/locale_settings_uk.rc',
            '<(chrome_grit_out_dir)/<(strings_rc_name)_uk.rc',
            '<(webkit_grit_out_dir)/webkit_strings_uk.rc',
            '<(app_strings_out_dir)/app_strings_uk.rc',
            '<(app_locale_out_dir)/app_locale_settings_uk.rc',
          ],
        },
        {
          'target_name': 'vi',
          'msvs_guid': 'DA5C6FCB-FCFD-49B8-8DDA-8351638096DB',
          'sources': [
            '<(chrome_grit_out_dir)/platform_locale_settings_vi.rc',
            '<(chrome_grit_out_dir)/generated_resources_vi.rc',
            '<(chrome_grit_out_dir)/locale_settings_vi.rc',
            '<(chrome_grit_out_dir)/<(strings_rc_name)_vi.rc',
            '<(webkit_grit_out_dir)/webkit_strings_vi.rc',
            '<(app_strings_out_dir)/app_strings_vi.rc',
            '<(app_locale_out_dir)/app_locale_settings_vi.rc',
          ],
        },
        {
          'target_name': 'zh-CN',
          'msvs_guid': 'C0C7DA58-C90D-4BDE-AE44-588997339F5D',
          'sources': [
            '<(chrome_grit_out_dir)/platform_locale_settings_zh-CN.rc',
            '<(chrome_grit_out_dir)/generated_resources_zh-CN.rc',
            '<(chrome_grit_out_dir)/locale_settings_zh-CN.rc',
            '<(chrome_grit_out_dir)/<(strings_rc_name)_zh-CN.rc',
            '<(webkit_grit_out_dir)/webkit_strings_zh-CN.rc',
            '<(app_strings_out_dir)/app_strings_zh-CN.rc',
            '<(app_locale_out_dir)/app_locale_settings_zh-CN.rc',
          ],
        },
        {
          'target_name': 'zh-TW',
          'msvs_guid': 'E7B11CF0-FE40-4A69-AE20-1B882F4D7585',
          'sources': [
            '<(chrome_grit_out_dir)/platform_locale_settings_zh-TW.rc',
            '<(chrome_grit_out_dir)/generated_resources_zh-TW.rc',
            '<(chrome_grit_out_dir)/locale_settings_zh-TW.rc',
            '<(chrome_grit_out_dir)/<(strings_rc_name)_zh-TW.rc',
            '<(webkit_grit_out_dir)/webkit_strings_zh-TW.rc',
            '<(app_strings_out_dir)/app_strings_zh-TW.rc',
            '<(app_locale_out_dir)/app_locale_settings_zh-TW.rc',
          ],
        },
      ],
    }],
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
