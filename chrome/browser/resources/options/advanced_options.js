// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {

var OptionsPage = options.OptionsPage;

  //
  // AdvancedOptions class
  // Encapsulated handling of advanced options page.
  //
  function AdvancedOptions() {
    OptionsPage.call(this, 'advanced', templateData.advancedPage,
                     'advancedPage');
  }

  cr.addSingletonGetter(AdvancedOptions);

  AdvancedOptions.prototype = {
    // Inherit AdvancedOptions from OptionsPage.
    __proto__: options.OptionsPage.prototype,

    // Initialize AdvancedOptions page.
    initializePage: function() {
      // Call base class implementation to starts preference initialization.
      OptionsPage.prototype.initializePage.call(this);

      // Setup click handlers for buttons.
      $('privacyContentSettingsButton').onclick = function(event) {
        OptionsPage.showPageByName('content');
        OptionsPage.showTab($('cookies-nav-tab'));
      };
      $('privacyClearDataButton').onclick = function(event) {
        OptionsPage.showOverlay('clearBrowserDataOverlay');
      };
      $('autoOpenFileTypesResetToDefault').onclick = function(event) {
        chrome.send('autoOpenFileTypesAction');
      };
      $('fontSettingsConfigureFontsOnlyButton').onclick = function(event) {
        OptionsPage.showOverlay('fontSettingsOverlay');
      };

      if (!cr.isChromeOS) {
        $('proxiesConfigureButton').onclick = function(event) {
          chrome.send('showNetworkProxySettings');
        };
        $('certificatesManageButton').onclick = function(event) {
          chrome.send('showManageSSLCertificates');
        };
        $('downloadLocationBrowseButton').onclick = function(event) {
          chrome.send('selectDownloadLocation');
        };

        // Remove Windows-style accelerators from the Browse button label.
        // TODO(csilv): Remove this after the accelerator has been removed from
        // the localized strings file, pending removal of old options window.
        $('downloadLocationBrowseButton').textContent =
            localStrings.getStringWithoutAccelerator(
                'downloadLocationBrowseButton');
      } else {
        $('proxiesConfigureButton').onclick = function(event) {
          OptionsPage.showPageByName('proxy');
        };
      }

      if (cr.isWindows) {
        $('sslCheckRevocation').onclick = function(event) {
          chrome.send('checkRevocationCheckboxAction',
              [String($('sslCheckRevocation').checked)]);
        };
        $('sslUseSSL2').onclick = function(event) {
          chrome.send('useSSL2CheckboxAction',
              [String($('sslUseSSL2').checked)]);
        };
      }
    }
  };

  //
  // Chrome callbacks
  //

  // Set the download path.
  AdvancedOptions.SetDownloadLocationPath = function (path) {
    if (!cr.isChromeOS)
      $('downloadLocationPath').value = path;
  };

  // Set the enabled state for the autoOpenFileTypesResetToDefault button.
  AdvancedOptions.SetAutoOpenFileTypesDisabledAttribute = function (disabled) {
    $('autoOpenFileTypesResetToDefault').disabled = disabled;
  };

  // Set the enabled state for the proxy settings button.
  AdvancedOptions.SetProxySettingsDisabledAttribute = function (disabled) {
    $('proxiesConfigureButton').disabled = disabled;
  };

  // Set the checked state for the sslCheckRevocation checkbox.
  AdvancedOptions.SetCheckRevocationCheckboxState = function(checked) {
    $('sslCheckRevocation').checked = checked;
  };

  // Set the checked state for the sslUseSSL2 checkbox.
  AdvancedOptions.SetUseSSL2CheckboxState = function(checked) {
    $('sslUseSSL2').checked = checked;
  };

  // Export
  return {
    AdvancedOptions: AdvancedOptions
  };

});
