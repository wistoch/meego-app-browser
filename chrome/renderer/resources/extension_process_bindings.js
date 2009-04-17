// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// -----------------------------------------------------------------------------
// NOTE: If you change this file you need to touch renderer_resources.grd to
// have your change take effect.
// -----------------------------------------------------------------------------

var chromium;
(function() {
  native function GetNextCallbackId();
  native function GetWindows();
  native function GetTabsForWindow();
  native function GetTab();
  native function CreateTab();
  native function UpdateTab();
  native function MoveTab();
  native function RemoveTab();

  if (!chromium)
    chromium = {};

  // Validate arguments.
  function validate(inst, schemas) {
    if (inst.length > schemas.length)
      throw new Error("Too many arguments.");

    for (var i = 0; i < schemas.length; i++) {
      if (inst[i]) {
        var validator = new chromium.JSONSchemaValidator();
        validator.validate(inst[i], schemas[i]);
        if (validator.errors.length == 0)
          continue;
        
        var message = "Invalid value for argument " + i + ". ";
        for (var i = 0, err; err = validator.errors[i]; i++) {
          if (err.path) {
            message += "Property '" + err.path + "': ";
          }
          message += err.message;
          message = message.substring(0, message.length - 1);
          message += ", ";
        }
        message = message.substring(0, message.length - 2);
        message += ".";

        throw new Error(message);
      } else if (!schemas[i].optional) {
        throw new Error("Argument " + i + " is required.");
      }
    }
  }

  // callback handling
  var callbacks = [];
  chromium._dispatchCallback = function(callbackId, str) {
    try {
      if (str) {
        callbacks[callbackId](goog.json.parse(str));
      } else {
        callbacks[callbackId]();
      }
    } finally {
      delete callbacks[callbackId];
    }
  };

  // Send an API request and optionally register a callback.
  function sendRequest(request, args, callback) {
    var sargs = goog.json.serialize(args);
    var callbackId = -1;
    if (callback) {
      callbackId = GetNextCallbackId();
      callbacks[callbackId] = callback;
    }
    request(sargs, callbackId);
  }

  //----------------------------------------------------------------------------

  // Tabs
  chromium.tabs = {};

  chromium.tabs.getWindows = function(windowQuery, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(GetWindows, windowQuery, callback);
  };

  chromium.tabs.getWindows.params = [
    {
      type: "object",
      properties: {
        ids: {
          type: "array",
          items: chromium.types.pInt,
          minItems: 1
        }
      },
      optional: true,
      additionalProperties: false
    },
    chromium.types.optFun
  ];

  // TODO(aa): This should eventually take an optional windowId param.
  chromium.tabs.getTabsForWindow = function(callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(GetTabsForWindow, null, callback);
  };

  chromium.tabs.getTabsForWindow.params = [
    chromium.types.optFun
  ];

  chromium.tabs.getTab = function(tabId, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(GetTab, tabId, callback);
  };

  chromium.tabs.getTab.params = [
    chromium.types.pInt,
    chromium.types.optFun
  ];

  chromium.tabs.createTab = function(tab, callback) {  
    validate(arguments, arguments.callee.params);
    sendRequest(CreateTab, tab, callback);
  };

  chromium.tabs.createTab.params = [
    {
      type: "object",
      properties: {
        windowId: chromium.types.optPInt,
        url: chromium.types.optStr,
        selected: chromium.types.optBool
      },
      additionalProperties: false
    },
    chromium.types.optFun
  ];

  chromium.tabs.updateTab = function(tab) {
    validate(arguments, arguments.callee.params);
    sendRequest(UpdateTab, tab);
  };

  chromium.tabs.updateTab.params = [
    {
      type: "object",
      properties: {
        id: chromium.types.pInt,
        windowId: chromium.types.optPInt,
        url: chromium.types.optStr,
        selected: chromium.types.optBool
      },
      additionalProperties: false
    }
  ];

  chromium.tabs.moveTab = function(tab) {
    validate(arguments, arguments.callee.params);
    sendRequest(MoveTab, tab);
  };

  chromium.tabs.moveTab.params = [
    {
      type: "object",
      properties: {
        id: chromium.types.pInt,
        windowId: chromium.types.optPInt,
        index: chromium.types.pInt
      },
      additionalProperties: false
    }
  ];
  
  chromium.tabs.removeTab = function(tabId) {
    validate(arguments, arguments.callee.params);
    sendRequest(RemoveTab, tabId);
  };

  chromium.tabs.removeTab.params = [
    chromium.types.pInt
  ];

  // onTabMoved sends ({tabId, windowId, fromIndex, toIndex}) as named
  // arguments.
  chromium.tabs.onTabMoved = new chromium.Event("tab-moved");

  //----------------------------------------------------------------------------

  // Bookmarks
  chromium.bookmarks = {};

  chromium.bookmarks.get = function(ids, callback) {
    native function GetBookmarks();
    sendRequest(GetBookmarks, ids, callback);
  };

  chromium.bookmarks.get.params = [
    {
      type: "array",
      items: {
        type: chromium.types.pInt
      },
      minItems: 1,
      optional: true,
      additionalProperties: false
    },
    chromium.types.optFun
  ];

  chromium.bookmarks.search = function(query, callback) {
    native function SearchBookmarks();
    sendRequest(SearchBookmarks, query, callback);
  };

  chromium.bookmarks.search.params = [
    chromium.types.string,
    chromium.types.optFun
  ];

  chromium.bookmarks.remove = function(bookmark, callback) {
    native function RemoveBookmark();
    sendRequest(RemoveBookmark, bookmark, callback);
  };

  chromium.bookmarks.remove.params = [
    {
      type: "object",
      properties: {
        id: chromium.types.pInt,
        recursive: chromium.types.bool
      },
      additionalProperties: false
    },
    chromium.types.optFun
  ];

  chromium.bookmarks.create = function(bookmark, callback) {
    native function CreateBookmark();
    sendRequest(CreateBookmark, bookmark, callback);
  };

  chromium.bookmarks.create.params = [
    {
      type: "object",
      properties: {
        parentId: chromium.types.optPInt,
        index: chromium.types.optPInt,
        title: chromium.types.optString,
        url: chromium.types.optString,
      },
      additionalProperties: false
    },
    chromium.types.optFun
  ];

  chromium.bookmarks.move = function(obj, callback) {
    native function MoveBookmark();
    sendRequest(MoveBookmark, obj, callback);
  };

  chromium.bookmarks.move.params = [
    {
      type: "object",
      properties: {
        id: chromium.types.pInt,
        parentId: chromium.types.optPInt,
        index: chromium.types.optPInt
      },
      additionalProperties: false
    },
    chromium.types.optFun
  ];

  chromium.bookmarks.setTitle = function(bookmark, callback) {
    native function SetBookmarkTitle();
    sendRequest(SetBookmarkTitle, bookmark, callback);
  };

  chromium.bookmarks.setTitle.params = [
    {
      type: "object",
      properties: {
        id: chromium.types.pInt,
        title: chromium.types.optString
      },
      additionalProperties: false
    },
    chromium.types.optFun
  ];
  
  //----------------------------------------------------------------------------

  // Self
  chromium.self = {};
  chromium.self.onConnect = new chromium.Event("channel-connect");
})();

