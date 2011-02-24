// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// proxy api test
// browser_tests.exe --gtest_filter=ExtensionApiTest.ProxyAutoSettings

function expect(expected, message) {
  return chrome.test.callbackPass(function(value) {
    chrome.test.assertEq(expected, value, message);
  });
}

chrome.test.runTests([
  function setAutoSettings() {
    var config = {
      mode: "direct",
    };
    chrome.experimental.proxy.settings.set(
        {'value': config},
        chrome.test.callbackPass());
    chrome.experimental.proxy.settings.get(
        {'incognito': false},
        expect(config, "invalid proxy settings"));
    chrome.experimental.proxy.settings.get(
        {'incognito': true},
        expect(config, "invalid proxy settings"));
  }
]);
