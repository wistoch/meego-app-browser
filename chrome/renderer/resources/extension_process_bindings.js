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
  native function CreateWindow();
  native function RemoveWindow();
  native function GetWindows();
  native function GetTabsForWindow();
  native function GetTab();
  native function CreateTab();
  native function UpdateTab();
  native function MoveTab();
  native function RemoveTab();
  native function EnablePageAction();
  native function GetBookmarks();
  native function GetBookmarkChildren();
  native function GetBookmarkTree();
  native function SearchBookmarks();
  native function RemoveBookmark();
  native function CreateBookmark();
  native function MoveBookmark();
  native function SetBookmarkTitle();

  if (!chromium)
    chromium = {};

  // Validate arguments.
  function validate(args, schemas) {
    if (args.length > schemas.length)
      throw new Error("Too many arguments.");

    for (var i = 0; i < schemas.length; i++) {
      if (i in args && args[i] !== null && args[i] !== undefined) {
        var validator = new chromium.JSONSchemaValidator();
        validator.validate(args[i], schemas[i]);
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
        throw new Error("Parameter " + i + " is required.");
      }
    }
  }

  // Callback handling.
  // TODO(aa): This function should not be publicly exposed. Pass it into V8
  // instead and hold one per-context. See the way event_bindings.js works.
  var callbacks = [];
  chromium.dispatchCallback_ = function(callbackId, str) {
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

  // Windows.
  chromium.windows = {};

  chromium.windows.getWindows = function(windowQuery, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(GetWindows, windowQuery, callback);
  };

  chromium.windows.getWindows.params = [
    {
      type: "object",
      properties: {
        ids: {
          type: "array",
          items: chromium.types.pInt,
          minItems: 1
        }
      },
      optional: true
    },
    chromium.types.optFun
  ];
  
  chromium.windows.createWindow = function(createData, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(CreateWindow, createData, callback);
  };
  chromium.windows.createWindow.params = [
    {
      type: "object",
      properties: {
        url: chromium.types.optStr,
        left: chromium.types.optInt,
        top: chromium.types.optInt,
        width: chromium.types.optPInt,
        height: chromium.types.optPInt
      },
      optional: true
    },
    chromium.types.optFun
  ];
  
  chromium.windows.removeWindow = function(windowId, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(RemoveWindow, windowId, callback);
  };

  chromium.windows.removeWindow.params = [
    chromium.types.pInt,
    chromium.types.optFun
  ];
  
  // sends (windowId).
  // *WILL* be followed by tab-attached AND then tab-selection-changed.
  chromium.windows.onWindowCreated = new chromium.Event("window-created");

  // sends (windowId).
  // *WILL* be preceded by sequences of tab-removed AND then
  // tab-selection-changed -- one for each tab that was contained in the window
  // that closed
  chromium.windows.onWindowRemoved = new chromium.Event("window-removed");

  //----------------------------------------------------------------------------

  // Tabs
  chromium.tabs = {};

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
      }
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
      }
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
      }
    }
  ];
  
  chromium.tabs.removeTab = function(tabId) {
    validate(arguments, arguments.callee.params);
    sendRequest(RemoveTab, tabId);
  };

  chromium.tabs.removeTab.params = [
    chromium.types.pInt
  ];

  // Sends ({tabId, windowId, index}).
  // Will *NOT* be followed by tab-attached - it is implied.
  // *MAY* be followed by tab-selection-changed.
  chromium.tabs.onTabCreated = new chromium.Event("tab-created");
  
  // Wends ({tabId, windowId, fromIndex, toIndex}).
  // Tabs can only "move" within a window.
  chromium.tabs.onTabMoved = new chromium.Event("tab-moved");
 
  // Sends ({tabId, windowId, index}).
  chromium.tabs.onTabSelectionChanged = 
       new chromium.Event("tab-selection-changed");
   
  // Sends ({tabId, windowId, index}).
  // *MAY* be followed by tab-selection-changed.
  chromium.tabs.onTabAttached = new chromium.Event("tab-attached");
  
  // Sends ({tabId, windowId, index}).
  // *WILL* be followed by tab-selection-changed.
  chromium.tabs.onTabDetached = new chromium.Event("tab-detached");
  
  // Sends (tabId).
  // *WILL* be followed by tab-selection-changed.
  // Will *NOT* be followed or preceded by tab-detached.
  chromium.tabs.onTabRemoved = new chromium.Event("tab-removed");

  //----------------------------------------------------------------------------

  // PageActions.
  chromium.pageActions = {};

  chromium.pageActions.enableForTab = function(pageActionId, action) {
    validate(arguments, arguments.callee.params);
    sendRequest(EnablePageAction, [pageActionId, action]);
  }

  chromium.pageActions.enableForTab.params = [  
    chromium.types.str,
    {
      type: "object",
      properties: {
        tabId: chromium.types.pInt,
        url: chromium.types.str
      },
      optional: false
    }
  ];

  // Sends ({pageActionId, tabId, tabUrl}).
  chromium.pageActions.onExecute =
       new chromium.Event("page-action-executed");

  //----------------------------------------------------------------------------
  // Bookmarks
  chromium.bookmarks = {};

  chromium.bookmarks.get = function(ids, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(GetBookmarks, ids, callback);
  };

  chromium.bookmarks.get.params = [
    {
      type: "array",
      items: chromium.types.pInt,
      optional: true
    },
    chromium.types.fun
  ];
  
  chromium.bookmarks.getChildren = function(id, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(GetBookmarkChildren, id, callback);
  };

  chromium.bookmarks.getChildren.params = [
    chromium.types.pInt,
    chromium.types.fun
  ];
  
  chromium.bookmarks.getTree = function(callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(GetBookmarkTree, null, callback);
  };
  
  // TODO(erikkay): allow it to take an optional id as a starting point
  chromium.bookmarks.getTree.params = [
    chromium.types.fun
  ];

  chromium.bookmarks.search = function(query, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(SearchBookmarks, query, callback);
  };

  chromium.bookmarks.search.params = [
    chromium.types.str,
    chromium.types.fun
  ];

  chromium.bookmarks.remove = function(bookmark, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(RemoveBookmark, bookmark, callback);
  };

  chromium.bookmarks.remove.params = [
    {
      type: "object",
      properties: {
        id: chromium.types.pInt,
        recursive: chromium.types.optBool
      }
    },
    chromium.types.optFun
  ];

  chromium.bookmarks.create = function(bookmark, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(CreateBookmark, bookmark, callback);
  };

  chromium.bookmarks.create.params = [
    {
      type: "object",
      properties: {
        parentId: chromium.types.optPInt,
        index: chromium.types.optPInt,
        title: chromium.types.optStr,
        url: chromium.types.optStr,
      }
    },
    chromium.types.optFun
  ];

  chromium.bookmarks.move = function(obj, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(MoveBookmark, obj, callback);
  };

  chromium.bookmarks.move.params = [
    {
      type: "object",
      properties: {
        id: chromium.types.pInt,
        parentId: chromium.types.optPInt,
        index: chromium.types.optPInt
      }
    },
    chromium.types.optFun
  ];

  chromium.bookmarks.setTitle = function(bookmark, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest(SetBookmarkTitle, bookmark, callback);
  };

  chromium.bookmarks.setTitle.params = [
    {
      type: "object",
      properties: {
        id: chromium.types.pInt,
        title: chromium.types.optStr
      }
    },
    chromium.types.optFun
  ];

  // bookmark events

  // Sends ({id, title, url, parentId, index})
  chromium.bookmarks.onBookmarkAdded = new chromium.Event("bookmark-added");

  // Sends ({parentId, index})
  chromium.bookmarks.onBookmarkRemoved = new chromium.Event("bookmark-removed");

  // Sends (id, object) where object has list of properties that have changed.
  // Currently, this only ever includes 'title'.
  chromium.bookmarks.onBookmarkChanged = new chromium.Event("bookmark-changed");

  // Sends ({id, parentId, index, oldParentId, oldIndex})
  chromium.bookmarks.onBookmarkMoved = new chromium.Event("bookmark-moved");
  
  // Sends (id, [childrenIds])
  chromium.bookmarks.onBookmarkChildrenReordered =
      new chromium.Event("bookmark-children-reordered");


  //----------------------------------------------------------------------------

  // Self.
  chromium.self = {};
  chromium.self.onConnect = new chromium.Event("channel-connect");
})();

