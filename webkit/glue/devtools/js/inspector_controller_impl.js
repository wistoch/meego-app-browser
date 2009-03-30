// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview DevTools' implementation of the InspectorController API.
 */
goog.require('devtools.InspectorController');

goog.provide('devtools.InspectorControllerImpl');

devtools.InspectorControllerImpl = function() {
  devtools.InspectorController.call(this);
  this.frame_element_id_ = 1;

  this.window_ = {
      get document() {
        return devtools.tools.getDomAgent().getDocument();
      },
      get Node() {
        return devtools.DomNode;
      },
      get Element() {
        return devtools.DomNode;
      }
  };
};
goog.inherits(devtools.InspectorControllerImpl,
    devtools.InspectorController);


/**
 * {@inheritDoc}.
 */
devtools.InspectorControllerImpl.prototype.hiddenPanels = function() {
  return "profiles,databases";
};


/**
 * {@inheritDoc}.
 */
devtools.InspectorControllerImpl.prototype.addSourceToFrame =
    function(mimeType, source, element) {
  if (!element.id) {
    element.id = "f" + this.frame_element_id_++; 
  }
  DevToolsHost.addSourceToFrame(mimeType, source, element.id);
  return true;
};


/**
 * {@inheritDoc}.
 */
devtools.InspectorControllerImpl.prototype.addResourceSourceToFrame =
    function(identifier, element) {
  var self = this;
  tools.getNetAgent().getResourceContentAsync(identifier, function(source) {
    var resource = netAgent.getResource(identifier);
    self.addSourceToFrame(resource.mimeType, source, element);
  });
  return false;
};


/**
 * {@inheritDoc}.
 */
devtools.InspectorControllerImpl.prototype.hideDOMNodeHighlight = function() {
  RemoteToolsAgent.HideDOMNodeHighlight();
};


/**
 * {@inheritDoc}.
 */
devtools.InspectorControllerImpl.prototype.highlightDOMNode =
    function(hoveredNode) {
  RemoteToolsAgent.HighlightDOMNode(hoveredNode.id_);
};


/**
 * {@inheritDoc}.
 */
devtools.InspectorControllerImpl.prototype.inspectedWindow = function() {
  return this.window_;
};


/**
 * @override
 */
devtools.InspectorControllerImpl.prototype.debuggerEnabled = function() {
  return true;
};


devtools.InspectorControllerImpl.prototype.currentCallFrame = function() {
  // TODO(yurys);
  return null;
};


devtools.InspectorControllerImpl.prototype.removeBreakpoint = function(
    sourceID, line) {
};


devtools.InspectorControllerImpl.prototype.resumeDebugger = function() {
};


devtools.InspectorControllerImpl.prototype.stepIntoStatementInDebugger =
    function() {
};


devtools.InspectorControllerImpl.prototype.stepOutOfFunctionInDebugger =
    function() {
};


devtools.InspectorControllerImpl.prototype.stepOverStatementInDebugger =
    function() {
};


var InspectorController = new devtools.InspectorControllerImpl();
