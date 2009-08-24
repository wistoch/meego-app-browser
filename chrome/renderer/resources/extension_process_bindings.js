// Copyright (c) 2009 The chrome Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// -----------------------------------------------------------------------------
// NOTE: If you change this file you need to touch renderer_resources.grd to
// have your change take effect.
// -----------------------------------------------------------------------------

// This script contains privileged chrome extension related javascript APIs.
// It is loaded by pages whose URL has the chrome-extension protocol.

var chrome = chrome || {};
(function() {
  native function GetExtensionAPIDefinition();
  native function StartRequest();
  native function GetCurrentPageActions(extensionId);
  native function GetExtensionViews();
  native function GetChromeHidden();
  native function GetNextRequestId();
  native function OpenChannelToTab();

  if (!chrome)
    chrome = {};

  var chromeHidden = GetChromeHidden();

  // Validate arguments.
  chromeHidden.validationTypes = [];
  chromeHidden.validate = function(args, schemas) {
    if (args.length > schemas.length)
      throw new Error("Too many arguments.");

    for (var i = 0; i < schemas.length; i++) {
      if (i in args && args[i] !== null && args[i] !== undefined) {
        var validator = new chrome.JSONSchemaValidator();
        validator.addTypes(chromeHidden.validationTypes);
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
  var requests = [];
  chromeHidden.handleResponse = function(requestId, name,
                                         success, response, error) {
    try {
      var request = requests[requestId];
      if (success) {
        delete chrome.extension.lastError;
      } else {
        if (!error) {
          error = "Unknown error."
        }
        console.error("Error during " + name + ": " + error);
        chrome.extension.lastError = {
          "message": error
        };
      }
      
      if (request.callback) {
        // Callbacks currently only support one callback argument.
        var callbackArgs = response ? [JSON.parse(response)] : [];

        // Validate callback in debug only -- and only when the
        // caller has provided a callback. Implementations of api
        // calls my not return data if they observe the caller
        // has not provided a callback.
        if (chromeHidden.validateCallbacks && !error) {
          try {
            if (!request.callbackSchema.parameters) {
              throw "No callback schemas defined";
            }

            if (request.callbackSchema.parameters.length > 1) {
              throw "Callbacks may only define one parameter";
            }

            chromeHidden.validate(callbackArgs,
                request.callbackSchema.parameters);
          } catch (exception) {
            return "Callback validation error during " + name + " -- " +
                   exception;
          }
        }

        if (response) {
          request.callback(callbackArgs[0]);
        } else {
          request.callback();
        }
      }
    } finally {
      delete requests[requestId];
      delete chrome.extension.lastError;
    }
  };

  function prepareRequest(args, argSchemas) {
    var request = {};
    var argCount = args.length;
 
    // Look for callback param.
    if (argSchemas.length > 0 &&
        args.length == argSchemas.length &&
        argSchemas[argSchemas.length - 1].type == "function") {
      request.callback = args[argSchemas.length - 1];
      request.callbackSchema = argSchemas[argSchemas.length - 1];
      --argCount;
    }

    // Calls with one argument expect singular argument. Calls with multiple
    // expect a list.
    if (argCount == 1) {
      request.args = args[0];
    }
    if (argCount > 1) {
      request.args = [];
      for (var k = 0; k < argCount; k++) {
        request.args[k] = args[k];
      }
    }
    
    return request;
  }

  // Send an API request and optionally register a callback.
  function sendRequest(functionName, args, argSchemas) {
    var request = prepareRequest(args, argSchemas);
    // JSON.stringify doesn't support a root object which is undefined.
    if (request.args === undefined)
      request.args = null;
    var sargs = JSON.stringify(request.args);
    var requestId = GetNextRequestId();
    requests[requestId] = request;
    return StartRequest(functionName, sargs, requestId,
                        request.callback ? true : false);
  }
  
  // Using forEach for convenience, and to bind |module|s & |apiDefs|s via
  // closures.
  function forEach(a, f) {
    for (var i = 0; i < a.length; i++) {
      f(a[i], i);
    }
  }

  function bind(obj, func) {
    return function() {
      return func.apply(obj, arguments);
    };
  }

  // --- Setup additional api's not currently handled in common/extensions/api

  // Page action events send (pageActionId, {tabId, tabUrl}).
  function setupPageActionEvents(extensionId) {
    var pageActions = GetCurrentPageActions(extensionId);
    var eventName = "";
    for (var i = 0; i < pageActions.length; ++i) {
      eventName = extensionId + "/" + pageActions[i];
      // Setup events for each extension_id/page_action_id string we find.
      chrome.pageActions[pageActions[i]] = new chrome.Event(eventName);
    }
  }

  chromeHidden.onLoad.addListener(function (extensionId) {
    chrome.extension = new chrome.Extension(extensionId);

    // TODO(mpcomplete): chrome.self is deprecated.  Remove it at 1.0.
    // http://code.google.com/p/chromium/issues/detail?id=16356
    chrome.self = chrome.extension;

    // |apiFunctions| is a hash of name -> object that stores the 
    // name & definition of the apiFunction. Custom handling of api functions
    // is implemented by adding a "handleRequest" function to the object.
    var apiFunctions = {};

    // Read api definitions and setup api functions in the chrome namespace.
    // TODO(rafaelw): Consider defining a json schema for an api definition
    //   and validating either here, in a unit_test or both.
    // TODO(rafaelw): Handle synchronous functions.
    // TOOD(rafaelw): Consider providing some convenient override points
    //   for api functions that wish to insert themselves into the call. 
    var apiDefinitions = JSON.parse(GetExtensionAPIDefinition());

    forEach(apiDefinitions, function(apiDef) {
      chrome[apiDef.namespace] = chrome[apiDef.namespace] || {};
      var module = chrome[apiDef.namespace];

      // Add types to global validationTypes
      if (apiDef.types) {
        forEach(apiDef.types, function(t) {
          chromeHidden.validationTypes.push(t);
        });
      }

      // Setup Functions.
      if (apiDef.functions) {
        forEach(apiDef.functions, function(functionDef) {
          // Module functions may have been defined earlier by hand. Don't
          // clobber them.
          if (module[functionDef.name])
            return;

          var apiFunction = {};      
          apiFunction.definition = functionDef;
          apiFunction.name = apiDef.namespace + "." + functionDef.name;;
          apiFunctions[apiFunction.name] = apiFunction;
          
          module[functionDef.name] = bind(apiFunction, function() {
            chromeHidden.validate(arguments, this.definition.parameters);
      
            if (this.handleRequest)
              return this.handleRequest.apply(this, arguments);
            else 
              return sendRequest(this.name, arguments,
                  this.definition.parameters);        
          });
        });
      }

      // Setup Events
      if (apiDef.events) {
        forEach(apiDef.events, function(eventDef) {
          // Module events may have been defined earlier by hand. Don't clobber
          // them.
          if (module[eventDef.name])
            return;

          var eventName = apiDef.namespace + "." + eventDef.name;
          module[eventDef.name] = new chrome.Event(eventName,
              eventDef.parameters);
        });
      }
    });

    apiFunctions["tabs.connect"].handleRequest = function(tabId, opt_name) {
      var portId = OpenChannelToTab(
          tabId, chrome.extension.id_, opt_name || "");
      return chromeHidden.Port.createPort(portId, opt_name);
    }

    apiFunctions["extension.getViews"].handleRequest = function() {
      return GetExtensionViews(-1, "ALL");
    }

    apiFunctions["extension.getBackgroundPage"].handleRequest = function() {
      return GetExtensionViews(-1, "BACKGROUND")[0] || null;
    }

    apiFunctions["extension.getToolstrips"].handleRequest =
        function(windowId) {
      if (typeof(windowId) == "undefined")
        windowId = -1;
      return GetExtensionViews(windowId, "TOOLSTRIP");
    }

    apiFunctions["extension.getTabContentses"].handleRequest =
        function(windowId) {
      if (typeof(windowId) == "undefined")
        windowId = -1;
      return GetExtensionViews(windowId, "TAB");
    }

    apiFunctions["devtools.getTabEvents"].handleRequest = function(tabId) {
      var tabIdProxy = {};
      forEach(["onPageEvent", "onTabUrlChange", "onTabClose"],
              function(name) {
        // Event disambiguation is handled by name munging.  See
        // chrome/browser/extensions/extension_devtools_events.h for the C++
        // equivalent of this logic.
        tabIdProxy[name] = new chrome.Event("devtools." + tabId + "." + name);
      });
      return tabIdProxy;
    }

    setupPageActionEvents(extensionId);
  });
})();
