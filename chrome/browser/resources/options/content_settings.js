// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {

  var OptionsPage = options.OptionsPage;

  //////////////////////////////////////////////////////////////////////////////
  // ContentSettings class:

  /**
   * Encapsulated handling of content settings page.
   * @constructor
   */
  function ContentSettings() {
    this.activeNavTab = null;
    OptionsPage.call(this, 'content', templateData.contentSettingsPage,
                     'contentSettingsPage');
  }

  cr.addSingletonGetter(ContentSettings);

  ContentSettings.prototype = {
    __proto__: OptionsPage.prototype,

    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      chrome.send('getContentFilterSettings');
      this.showTab($('cookies-nav-tab'));

      var self = this;
      $('content-settings-nav-tabs').onclick = function(event) {
        self.showTab(event.srcElement);
      };

      // Cookies filter page ---------------------------------------------------
      $('cookies-exceptions-button').onclick = function(event) {
        // TODO(estade): show exceptions page.
      };

      $('block-third-party-cookies').onclick = function(event) {
        chrome.send('setAllowThirdPartyCookies',
                    [String($('block-third-party-cookies').checked)]);
      };

      $('show-cookies-button').onclick = function(event) {
        // TODO(estade): show cookies and other site data page.
      };

      // Images filter page ----------------------------------------------------
      $('images-exceptions-button').onclick = function(event) {
        // TODO(estade): show a dialog.
        // TODO(estade): remove this hack.
        imagesExceptionsList.redraw();
      };

      options.contentSettings.ExceptionsArea.decorate(
          $('imagesExceptionsArea'));
    },

    /**
     * Shows the tab contents for the given navigation tab.
     * @param {!Element} tab The tab that the user clicked.
     */
    showTab: function(tab) {
      if (!tab.classList.contains('inactive-tab'))
        return;

      if (this.activeNavTab != null) {
        this.activeNavTab.classList.remove('active-tab');
        $(this.activeNavTab.getAttribute('tab-contents')).classList.
            remove('active-tab-contents');
      }

      tab.classList.add('active-tab');
      $(tab.getAttribute('tab-contents')).classList.add('active-tab-contents');
      this.activeNavTab = tab;
    }
  };

  /**
   * Sets the initial values for all the content settings radios.
   * @param {Object} dict A mapping from radio groups to the checked value for
   *     that group.
   */
  ContentSettings.setInitialContentFilterSettingsValue = function(dict) {
    for (var group in dict) {
      document.querySelector('input[type=radio][name=' + group +
                             '][value=' + dict[group] + ']').checked = true;
    }
  };

  /**
   * Initializes the image exceptions list.
   * @param {Array} list An array of pairs, where the first element of each pair
   *     is the filter string, and the second is the setting (allow/block).
   */
  ContentSettings.setImagesExceptions = function(list) {
    imagesExceptionsList.clear();
    for (var i = 0; i < list.length; ++i) {
      imagesExceptionsList.addException(list[i]);
    }
  };

  /**
   * Sets the initial value for the Third Party Cookies checkbox.
   * @param {boolean=} block True if we are blocking third party cookies.
   */
  ContentSettings.setBlockThirdPartyCookies = function(block) {
    $('block-third-party-cookies').checked = block;
  };

  // Export
  return {
    ContentSettings: ContentSettings
  };

});

