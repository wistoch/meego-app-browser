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

      var exceptionsLists = this.pageDiv.querySelectorAll('list');
      for (var i = 0; i < exceptionsLists.length; i++) {
        options.contentSettings.ExceptionsList.decorate(exceptionsLists[i]);
      }
      ContentSettings.hideOTRLists();

      this.addEventListener('visibleChange', function(event) {
        for (var i = 0; i < exceptionsLists.length; i++) {
          exceptionsLists[i].redraw();
        }
      });

      // Cookies filter page ---------------------------------------------------
      $('block-third-party-cookies').onclick = function(event) {
        chrome.send('setAllowThirdPartyCookies',
                    [String($('block-third-party-cookies').checked)]);
      };

      $('show-cookies-button').onclick = function(event) {
        chrome.send('coreOptionsUserMetricsAction', ['Options_ShowCookies']);
        OptionsPage.showPageByName('cookiesView');
      };

      $('plugins-tab').onclick = function(event) {
        chrome.send('openPluginsTab');
      };

      if (!templateData.enable_click_to_play)
        $('click_to_play').style.display = 'none';
    },

    /**
     * Handles a hash value in the URL (such as bar in
     * chrome://options/foo#bar). Overrides the default action of showing an
     * overlay by instead navigating to a particular subtab.
     * @param {string} hash The hash value.
     */
    handleHash: function(hash) {
      // TODO(estade): show subpage for hash.
    },
  };

  /**
   * Sets the values for all the content settings radios.
   * @param {Object} dict A mapping from radio groups to the checked value for
   *     that group.
   */
  ContentSettings.setContentFilterSettingsValue = function(dict) {
    for (var group in dict) {
      document.querySelector('input[type=radio][name=' + group +
                             '][value=' + dict[group] + ']').checked = true;
    }
  };

  /**
   * Initializes an exceptions list.
   * @param {string} type The content type that we are setting exceptions for.
   * @param {Array} list An array of pairs, where the first element of each pair
   *     is the filter string, and the second is the setting (allow/block).
   */
  ContentSettings.setExceptions = function(type, list) {
    var exceptionsList =
        document.querySelector('div[contentType=' + type + ']' +
                               ' list[mode=normal]');

    exceptionsList.reset();
    for (var i = 0; i < list.length; i++) {
      exceptionsList.addException(list[i]);
    }
    exceptionsList.redraw();
  };

  ContentSettings.setOTRExceptions = function(type, list) {
    var exceptionsList =
        document.querySelector('div[contentType=' + type + ']' +
                               ' div list[mode=normal]');

    exceptionsList.parentNode.classList.remove('hidden');

    exceptionsList.reset();
    for (var i = 0; i < list.length; i++) {
      exceptionsList.addException(list[i]);
    }
    exceptionsList.redraw();
  };

  /**
   * Called when the last incognito window is closed.
   */
  ContentSettings.OTRProfileDestroyed = function() {
    this.hideOTRLists();
  };

  /**
   * Clears and hides the incognito exceptions lists.
   */
  ContentSettings.hideOTRLists = function() {
    var otrLists = document.querySelectorAll('list[mode=otr]');

    for (var i = 0; i < otrLists.length; i++) {
      otrLists[i].reset();
      otrLists[i].parentNode.classList.add('hidden');
    }
  };

  /**
   * Sets the initial value for the Third Party Cookies checkbox.
   * @param {boolean=} block True if we are blocking third party cookies.
   */
  ContentSettings.setBlockThirdPartyCookies = function(block) {
    $('block-third-party-cookies').checked = block;
  };

  /**
   * The browser's response to a request to check the validity of a given URL
   * pattern.
   * @param {string} type The content type.
   * @param {string} mode The browser mode.
   * @param {string} pattern The pattern.
   * @param {bool} valid Whether said pattern is valid in the context of
   *     a content exception setting.
   */
  ContentSettings.patternValidityCheckComplete =
      function(type, mode, pattern, valid) {
    var exceptionsList =
        document.querySelector('div[contentType=' + type + '] ' +
                               'list[mode=' + mode + ']');
    exceptionsList.patternValidityCheckComplete(pattern, valid);
  };

  // Export
  return {
    ContentSettings: ContentSettings
  };

});
