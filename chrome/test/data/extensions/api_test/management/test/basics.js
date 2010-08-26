// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function checkIcon(item, size, path) {
  var icons = item.icons;
  for (var i = 0; i < icons.length; i++) {
    var icon = icons[i];
    if (icon.size == size) {
      var expected_url = "chrome-extension://" + item.id + "/" + path;
      assertEq(expected_url, icon.url);
      return;
    }
  }
  fail("didn't find icon of size " + size + " at path " + path);
}

var tests = [
  function simple() {
    chrome.management.getAll(function(items) {
      assertNoLastError();
      chrome.test.assertEq(5, items.length);

      checkItem(items, "Extension Management API Test", true, false);

      checkItem(items, "enabled_app", true, true,
                {"appLaunchUrl": "http://www.google.com/"});
      checkItem(items, "disabled_app", false, true);
      checkItem(items, "enabled_extension", true, false);
      checkItem(items, "disabled_extension", false, false,
                {"optionsUrl": "chrome-extension://<ID>/pages/options.html"});

      // Check that we got the icons correctly
      var extension = getItemNamed(items, "enabled_extension");
      assertEq(3, extension.icons.length);
      checkIcon(extension, 128, "icon_128.png");
      checkIcon(extension, 48, "icon_48.png");
      checkIcon(extension, 16, "icon_16.png");

      succeed();
    });
  },

  // Disables an enabled app.
  function disable() {
    chrome.management.getAll(function(items) {
      assertNoLastError();
      checkItem(items, "enabled_app", true, true);
      var enabled_app = getItemNamed(items, "enabled_app");
      chrome.management.setEnabled(enabled_app.id, false, function() {
        assertNoLastError();
        chrome.management.getAll(function(items2) {
          assertNoLastError();
          chrome.test.log("re-checking enabled_app");
          checkItem(items2, "enabled_app", false, true);
          succeed();
        });
      });
    });
  },

  // Enables a disabled extension.
  function enable() {
    chrome.management.getAll(function(items) {
      assertNoLastError();
      checkItem(items, "disabled_extension", false, false);
      var disabled = getItemNamed(items, "disabled_extension");
      chrome.management.setEnabled(disabled.id, true, function() {
        assertNoLastError();
        chrome.management.getAll(function(items2) {
          assertNoLastError();
          checkItem(items2, "disabled_extension", true, false);
          succeed();
        });
      });
    });
  }
];

chrome.test.runTests(tests);
