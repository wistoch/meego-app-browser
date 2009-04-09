var chromium;
(function() {
  if (!chromium)
    chromium = {};

  // callback handling
  var callbacks = [];
  chromium._dispatchCallback = function(callbackId, str) {
    callbacks[callbackId](goog.json.parse(str));
    delete callbacks[callbackId];
  };

  // Send an API request and optionally register a callback.
  function sendRequest(request, args, callback) {
    var sargs = goog.json.serialize(args);
    var callbackId = -1;
    if (callback) {
      native function GetNextCallbackId();
      callbackId = GetNextCallbackId();
      callbacks[callbackId] = callback;
    }
    request(sargs, callbackId);
  }

  // Tabs
  chromium.tabs = {};
  // TODO(aa): This should eventually take an optional windowId param.
  chromium.tabs.getTabsForWindow = function(callback) {
    native function GetTabsForWindow();
    sendRequest(GetTabsForWindow, null, callback);
  };
  chromium.tabs.createTab = function(tab, callback) {
    native function CreateTab();
    sendRequest(CreateTab, tab, callback);
  };
})();
