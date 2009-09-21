// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tools is a main class that wires all components of the
 * DevTools frontend together. It is also responsible for overriding existing
 * WebInspector functionality while it is getting upstreamed into WebCore.
 */
goog.provide('devtools.Tools');

goog.require('devtools.DebuggerAgent');


/**
 * Dispatches raw message from the host.
 * @param {string} remoteName
 * @prama {string} methodName
 * @param {string} param1, param2, param3 Arguments to dispatch.
 */
devtools$$dispatch = function(remoteName, methodName, param1, param2, param3) {
  remoteName = 'Remote' + remoteName.substring(0, remoteName.length - 8);
  var agent = window[remoteName];
  if (!agent) {
    debugPrint('No remote agent "' + remoteName + '" found.');
    return;
  }
  var method = agent[methodName];
  if (!method) {
    debugPrint('No method "' + remoteName + '.' + methodName + '" found.');
    return;
  }
  method.call(this, param1, param2, param3);
};


devtools.ToolsAgent = function() {
  RemoteToolsAgent.DidDispatchOn =
      devtools.Callback.processCallback;
  RemoteToolsAgent.FrameNavigate =
      goog.bind(this.frameNavigate_, this);
  RemoteToolsAgent.DispatchOnClient =
      goog.bind(this.dispatchOnClient_, this);
  this.debuggerAgent_ = new devtools.DebuggerAgent();
};


/**
 * Resets tools agent to its initial state.
 */
devtools.ToolsAgent.prototype.reset = function() {
  DevToolsHost.reset();
  this.debuggerAgent_.reset();
};


/**
 * @param {string} script Script exression to be evaluated in the context of the
 *     inspected page.
 * @param {function(Object|string, boolean):undefined} opt_callback Function to
 *     call with the result.
 */
devtools.ToolsAgent.prototype.evaluateJavaScript = function(script,
    opt_callback) {
  InspectorController.evaluate(script, opt_callback || function() {});
};


/**
 * @return {devtools.DebuggerAgent} Debugger agent instance.
 */
devtools.ToolsAgent.prototype.getDebuggerAgent = function() {
  return this.debuggerAgent_;
};


/**
 * @param {string} url Url frame navigated to.
 * @see tools_agent.h
 * @private
 */
devtools.ToolsAgent.prototype.frameNavigate_ = function(url) {
  this.reset();
  // Do not reset Profiles panel.
  var profiles = null;
  if ('profiles' in WebInspector.panels) {
    profiles = WebInspector.panels['profiles'];
    delete WebInspector.panels['profiles'];
  }
  WebInspector.reset();
  if (profiles != null) {
    WebInspector.panels['profiles'] = profiles;
  }
};


/**
 * @param {string} message Serialized call to be dispatched on WebInspector.
 * @private
 */
devtools.ToolsAgent.prototype.dispatchOnClient_ = function(message) {
  WebInspector.dispatch.apply(WebInspector, JSON.parse(message));
};


/**
 * Evaluates js expression.
 * @param {string} expr
 */
devtools.ToolsAgent.prototype.evaluate = function(expr) {
  RemoteToolsAgent.evaluate(expr);
};


/**
 * Enables / disables resources panel in the ui.
 * @param {boolean} enabled New panel status.
 */
WebInspector.setResourcesPanelEnabled = function(enabled) {
  InspectorController.resourceTrackingEnabled_ = enabled;
  WebInspector.panels.resources.reset();
};


/**
 * Prints string  to the inspector console or shows alert if the console doesn't
 * exist.
 * @param {string} text
 */
function debugPrint(text) {
  var console = WebInspector.console;
  if (console) {
    console.addMessage(new WebInspector.ConsoleMessage(
        WebInspector.ConsoleMessage.MessageSource.JS,
        WebInspector.ConsoleMessage.MessageType.Log,
        WebInspector.ConsoleMessage.MessageLevel.Log,
        1, 'chrome://devtools/<internal>', undefined, -1, text));
  } else {
    alert(text);
  }
}


/**
 * Global instance of the tools agent.
 * @type {devtools.ToolsAgent}
 */
devtools.tools = null;


var context = {};  // Used by WebCore's inspector routines.

///////////////////////////////////////////////////////////////////////////////
// Here and below are overrides to existing WebInspector methods only.
// TODO(pfeldman): Patch WebCore and upstream changes.
var oldLoaded = WebInspector.loaded;
WebInspector.loaded = function() {
  devtools.tools = new devtools.ToolsAgent();
  devtools.tools.reset();

  Preferences.ignoreWhitespace = false;
  Preferences.samplingCPUProfiler = true;
  oldLoaded.call(this);

  // Hide dock button on Mac OS.
  // TODO(pfeldman): remove once Mac OS docking is implemented.
  if (InspectorController.platform().indexOf('mac') == 0) {
    document.getElementById('dock-status-bar-item').addStyleClass('hidden');
  }

  // Mute refresh action.
  document.addEventListener("keydown", function(event) {
    if (event.keyIdentifier == 'F5') {
      event.preventDefault();
    } else if (event.keyIdentifier == 'U+0052' /* 'R' */ &&
        (event.ctrlKey || event.metaKey)) {
      event.preventDefault();
    }
  }, true);

  DevToolsHost.loaded();
};


/**
 * This override is necessary for adding script source asynchronously.
 * @override
 */
WebInspector.ScriptView.prototype.setupSourceFrameIfNeeded = function() {
  if (!this._frameNeedsSetup) {
    return;
  }

  this.attach();

  if (this.script.source) {
    this.didResolveScriptSource_();
  } else {
    var self = this;
    devtools.tools.getDebuggerAgent().resolveScriptSource(
        this.script.sourceID,
        function(source) {
          self.script.source = source ||
              WebInspector.UIString('<source is not available>');
          self.didResolveScriptSource_();
        });
  }
};


/**
 * Performs source frame setup when script source is aready resolved.
 */
WebInspector.ScriptView.prototype.didResolveScriptSource_ = function() {
  if (!InspectorController.addSourceToFrame(
      "text/javascript", this.script.source, this.sourceFrame.element)) {
    return;
  }

  delete this._frameNeedsSetup;

  this.sourceFrame.addEventListener(
      "syntax highlighting complete", this._syntaxHighlightingComplete, this);
  this.sourceFrame.syntaxHighlightJavascript();
};


/**
 * @param {string} type Type of the the property value('object' or 'function').
 * @param {string} className Class name of the property value.
 * @constructor
 */
WebInspector.UnresolvedPropertyValue = function(type, className) {
  this.type = type;
  this.className = className;
};


/**
 * This function overrides standard searchableViews getters to perform search
 * only in the current view (other views are loaded asynchronously, no way to
 * search them yet).
 */
WebInspector.searchableViews_ = function() {
  var views = [];
  const visibleView = this.visibleView;
  if (visibleView && visibleView.performSearch) {
    views.push(visibleView);
  }
  return views;
};


/**
 * @override
 */
WebInspector.ResourcesPanel.prototype.__defineGetter__(
    'searchableViews',
    WebInspector.searchableViews_);


/**
 * @override
 */
WebInspector.ScriptsPanel.prototype.__defineGetter__(
    'searchableViews',
    WebInspector.searchableViews_);


(function() {
  var oldShow = WebInspector.ScriptsPanel.prototype.show;
  WebInspector.ScriptsPanel.prototype.show =  function() {
    devtools.tools.getDebuggerAgent().initUI();
    this.enableToggleButton.visible = false;
    oldShow.call(this);
  };
})();


(function InterceptProfilesPanelEvents() {
  var oldShow = WebInspector.ProfilesPanel.prototype.show;
  WebInspector.ProfilesPanel.prototype.show = function() {
    devtools.tools.getDebuggerAgent().initializeProfiling();
    this.enableToggleButton.visible = false;
    oldShow.call(this);
    // Show is called on every show event of a panel, so
    // we only need to intercept it once.
    WebInspector.ProfilesPanel.prototype.show = oldShow;
  };
})();


/*
 * @override
 * TODO(mnaganov): Restore l10n when it will be agreed that it is needed.
 */
WebInspector.UIString = function(string) {
  return String.vsprintf(string, Array.prototype.slice.call(arguments, 1));
};


// There is no clear way of setting frame title yet. So sniffing main resource
// load.
(function OverrideUpdateResource() {
  var originalUpdateResource = WebInspector.updateResource;
  WebInspector.updateResource = function(identifier, payload) {
    originalUpdateResource.call(this, identifier, payload);
    var resource = this.resources[identifier];
    if (resource && resource.mainResource && resource.finished) {
      document.title =
          WebInspector.UIString('Developer Tools - %s', resource.url);
    }
  };
})();


// Highlight extension content scripts in the scripts list.
(function () {
  var original = WebInspector.ScriptsPanel.prototype._addScriptToFilesMenu;
  WebInspector.ScriptsPanel.prototype._addScriptToFilesMenu = function(script) {
    var result = original.apply(this, arguments);
    var debuggerAgent = devtools.tools.getDebuggerAgent();
    var type = debuggerAgent.getScriptContextType(script.sourceID);
    var option = script.filesSelectOption;
    if (type == 'injected' && option) {
      option.addStyleClass('injected');
    }
    return result;
  };
})();


/** Pending WebKit upstream by apavlov). Fixes iframe vs drag problem. */
(function() {
  var originalDragStart = WebInspector.elementDragStart;
  WebInspector.elementDragStart = function(element) {
    var glassPane = document.createElement("div");
    glassPane.style.cssText =
        'position:absolute;width:100%;height:100%;opacity:0;z-index:1';
    glassPane.id = 'glass-pane-for-drag';
    element.parentElement.appendChild(glassPane);

    originalDragStart.apply(this, arguments);
  };

  var originalDragEnd = WebInspector.elementDragEnd;
  WebInspector.elementDragEnd = function() {
    originalDragEnd.apply(this, arguments);

    var glassPane = document.getElementById('glass-pane-for-drag');
    glassPane.parentElement.removeChild(glassPane);
  };
})();


(function() {
  var originalCreatePanels = WebInspector._createPanels;
  WebInspector._createPanels = function() {
    originalCreatePanels.apply(this, arguments);
    this.panels.heap = new WebInspector.HeapProfilerPanel();
  };
})();


(function () {
var orig = InjectedScriptAccess.getProperties;
InjectedScriptAccess.getProperties = function(
    objectProxy, ignoreHasOwnProperty, callback) {
  if (objectProxy.isScope) {
    devtools.tools.getDebuggerAgent().resolveScope(objectProxy.objectId,
        callback);
  } else if (objectProxy.isV8Ref) {
    devtools.tools.getDebuggerAgent().resolveChildren(objectProxy.objectId,
        callback, true);
  } else {
    orig.apply(this, arguments);
  }
};
})()


WebInspector.resourceTrackingWasEnabled = function()
{
    InspectorController.resourceTrackingEnabled_ = true;
    this.panels.resources.resourceTrackingWasEnabled();
};

WebInspector.resourceTrackingWasDisabled = function()
{
    InspectorController.resourceTrackingEnabled_ = false;
    this.panels.resources.resourceTrackingWasDisabled();
};
