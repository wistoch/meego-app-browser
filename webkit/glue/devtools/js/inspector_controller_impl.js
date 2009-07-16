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
};
goog.inherits(devtools.InspectorControllerImpl,
    devtools.InspectorController);


/**
 * {@inheritDoc}.
 */
devtools.InspectorControllerImpl.prototype.platform = function() {
  return DevToolsHost.getPlatform();
};


/**
 * {@inheritDoc}.
 */
devtools.InspectorControllerImpl.prototype.closeWindow = function() {
  DevToolsHost.closeWindow();
};


/**
 * {@inheritDoc}.
 */
devtools.InspectorControllerImpl.prototype.attach = function() {
  DevToolsHost.dockWindow();
};


/**
 * {@inheritDoc}.
 */
devtools.InspectorControllerImpl.prototype.detach = function() {
  DevToolsHost.undockWindow();
};


/**
 * {@inheritDoc}.
 */
devtools.InspectorControllerImpl.prototype.storeLastActivePanel = function(panel) {
  RemoteToolsAgent.ExecuteUtilityFunction(
      devtools.Callback.wrap(undefined),
      'InspectorController', JSON.stringify(['storeLastActivePanel', panel]));
};


/**
 * {@inheritDoc}.
 */
devtools.InspectorControllerImpl.prototype.clearMessages = function() {
  RemoteToolsAgent.ClearConsoleMessages();
};


/**
 * {@inheritDoc}.
 */
devtools.InspectorControllerImpl.prototype.hiddenPanels = function() {
  return 'databases';
};


/**
 * {@inheritDoc}.
 */
devtools.InspectorControllerImpl.prototype.search = function(sourceRow, query) {
  return DevToolsHost.search(sourceRow, query);
};


/**
 * {@inheritDoc}.
 */
devtools.InspectorControllerImpl.prototype.toggleNodeSearch = function() {
  devtools.InspectorController.prototype.toggleNodeSearch.call(this);
  DevToolsHost.toggleInspectElementMode(this.searchingForNode());
};


/**
 * {@inheritDoc}.
 */
devtools.InspectorControllerImpl.prototype.addSourceToFrame =
    function(mimeType, source, element) {
  return DevToolsHost.addSourceToFrame(mimeType, source, element);
};


/**
 * {@inheritDoc}.
 */
devtools.InspectorControllerImpl.prototype.addResourceSourceToFrame =
    function(identifier, element) {
  var resource = WebInspector.resources[identifier];
  if (!resource) {
    return;
  }
  DevToolsHost.addResourceSourceToFrame(identifier, resource.mimeType, element);
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
  return devtools.tools.getDomAgent().getWindow();
};


/**
 * {@inheritDoc}.
 */
devtools.InspectorControllerImpl.prototype.enableResourceTracking =
    function(always) {
  devtools.tools.setResourceTrackingEnabled(true, always);
}


/**
 * {@inheritDoc}.
 */
devtools.InspectorControllerImpl.prototype.disableResourceTracking =
    function(always) {
  devtools.tools.setResourceTrackingEnabled(false, always);
};


/**
 * @override
 */
devtools.InspectorControllerImpl.prototype.debuggerEnabled = function() {
  return true;
};


devtools.InspectorControllerImpl.prototype.currentCallFrame = function() {
  return devtools.tools.getDebuggerAgent().getCurrentCallFrame();
};


devtools.InspectorControllerImpl.prototype.addBreakpoint = function(
    sourceID, line) {
  devtools.tools.getDebuggerAgent().addBreakpoint(sourceID, line);
};


devtools.InspectorControllerImpl.prototype.removeBreakpoint = function(
    sourceID, line) {
  devtools.tools.getDebuggerAgent().removeBreakpoint(sourceID, line);
};


devtools.InspectorControllerImpl.prototype.pauseInDebugger = function() {
  devtools.tools.getDebuggerAgent().pauseExecution();
};


devtools.InspectorControllerImpl.prototype.resumeDebugger = function() {
  devtools.tools.getDebuggerAgent().resumeExecution();
};


devtools.InspectorControllerImpl.prototype.stepIntoStatementInDebugger =
    function() {
  devtools.tools.getDebuggerAgent().stepIntoStatement();
};


devtools.InspectorControllerImpl.prototype.stepOutOfFunctionInDebugger =
    function() {
  devtools.tools.getDebuggerAgent().stepOutOfFunction();
};


devtools.InspectorControllerImpl.prototype.stepOverStatementInDebugger =
    function() {
  devtools.tools.getDebuggerAgent().stepOverStatement();
};


/**
 * @override
 */
devtools.InspectorControllerImpl.prototype.pauseOnExceptions = function() {
  return devtools.tools.getDebuggerAgent().pauseOnExceptions();
};


/**
 * @override
 */
devtools.InspectorControllerImpl.prototype.setPauseOnExceptions = function(
    value) {
  return devtools.tools.getDebuggerAgent().setPauseOnExceptions(value);
};


/**
 * @override
 */
devtools.InspectorControllerImpl.prototype.startProfiling = function() {
  devtools.tools.getDebuggerAgent().startProfiling();
};


/**
 * @override
 */
devtools.InspectorControllerImpl.prototype.stopProfiling = function() {
  devtools.tools.getDebuggerAgent().stopProfiling();
};


var InspectorController = new devtools.InspectorControllerImpl();
