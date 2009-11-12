/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


/**
 * @fileoverview This file contains classes that implement several
 * forms of 2D and 3D manipulation.
 */

o3djs.provide('o3djs.manipulators');

o3djs.require('o3djs.material');

o3djs.require('o3djs.math');

o3djs.require('o3djs.picking');

o3djs.require('o3djs.primitives');

o3djs.require('o3djs.quaternions');

/**
 * A module implementing several forms of 2D and 3D manipulation.
 * @namespace
 */
o3djs.manipulators = o3djs.manipulators || {};

/**
 * Creates a new manipulator manager, which maintains multiple
 * manipulators in the same scene. The manager is implicitly
 * associated with a particular O3D client via the Pack which is
 * passed in, although multiple managers can be created for a given
 * client. The manipulators are positioned in world coordinates and
 * are placed in the scene graph underneath the parent transform which
 * is passed in.
 * @param {!o3d.Pack} pack Pack in which manipulators' geometry and
 *     materials will be created.
 * @param {!o3d.DrawList} drawList The draw list against which
 *     internal materials are created.
 * @param {!o3d.Transform} parentTransform The parent transform under
 *     which the manipulators' geometry should be parented.
 * @param {!o3d.RenderNode} parentRenderNode The parent render node
 *     under which the manipulators' draw elements should be placed.
 * @param {number} renderNodePriority The priority that the
 *     manipulators' geometry should use for rendering.
 * @return {!o3djs.manipulators.Manager} The created manipulator
 *     manager.
 */
o3djs.manipulators.createManager = function(pack,
                                            drawList,
                                            parentTransform,
                                            parentRenderNode,
                                            renderNodePriority) {
  return new o3djs.manipulators.Manager(pack,
                                        drawList,
                                        parentTransform,
                                        parentRenderNode,
                                        renderNodePriority);
}

//
// Some linear algebra classes.
// TODO(kbr): find a better home for these.
//

/**
 * Creates a new Line object, which implements projection and
 * closest-point operations.
 * @constructor
 * @private
 * @param {!o3djs.math.Vector3} opt_direction The direction of the
 *     line. Does not need to be normalized but must not be the zero
 *     vector. Defaults to [1, 0, 0] if not specified.
 * @param {!o3djs.math.Vector3} opt_point A point through which the
 *     line goes. Defaults to [0, 0, 0] if not specified.
 */
o3djs.manipulators.Line_ = function(opt_direction,
                                    opt_point) {
  /**
   * The direction of the line.
   * @private
   * @type {!o3djs.math.Vector3}
   */
  this.direction_ = o3djs.math.copyVector(opt_direction || [1, 0, 0]);
  /**
   * A point through which the line goes.
   * @private
   * @type {!o3djs.math.Vector3}
   */
  this.point_ = o3djs.math.copyVector(opt_point || [0, 0, 0]);
  this.recalc_();
}

/**
 * Sets the direction of this line.
 * @private
 * @param {!o3djs.math.Vector3} direction The new direction of the
 *     line. Does not need to be normalized but must not be the zero
 *     vector.
 */
o3djs.manipulators.Line_.prototype.setDirection = function(direction) {
  this.direction_ = o3djs.math.copyVector(direction);
  this.recalc_();
}

/**
 * Gets the direction of this line.
 * @private
 * @return {!o3djs.math.Vector3} The direction of the line.
 */
o3djs.manipulators.Line_.prototype.getDirection = function() {
  return this.direction_;
}

/**
 * Sets one point through which this line travels.
 * @private
 * @param {!o3djs.math.Vector3} point A point which through the line
 *     will travel.
 */
o3djs.manipulators.Line_.prototype.setPoint = function(point) {
  this.point_ = o3djs.math.copyVector(point);
  this.recalc_();
}

/**
 * Gets one point through which this line travels.
 * @private
 * @return {!o3djs.math.Vector3} A point which through the line
 *     travels.
 */
o3djs.manipulators.Line_.prototype.getPoint = function() {
  return this.point_;
}

/**
 * Projects a point onto the line.
 * @private
 * @param {!o3djs.math.Vector3} point Point to be projected.
 * @return {!o3djs.math.Vector3} Point on the line closest to the
 *     passed point.
 */
o3djs.manipulators.Line_.prototype.projectPoint = function(point) {
  var dotp = o3djs.math.dot(this.direction_, point);
  return o3djs.math.addVector(this.alongVec_,
                              o3djs.math.mulScalarVector(dotp,
                                                         this.direction_));
}

/**
 * A threshold / error tolerance for determining if a number should be
 * considered zero.
 * @type {!number}
 */
o3djs.manipulators.EPSILON = 0.00001;

/**
 * A unit vector pointing along the positive X-axis.
 * @type {!o3djs.math.Vector3}
 */
o3djs.manipulators.X_AXIS = [1, 0, 0];

/**
 * A unit vector pointing along the positive Z-axis.
 * @type {!o3djs.math.Vector3}
 */
o3djs.manipulators.Z_AXIS = [0, 0, 1];


/**
 * Returns the closest point on this line to the given ray, which is
 * specified by start and end points. If the ray is parallel to the
 * line, returns null.
 * @private
 * @param {!o3djs.math.Vector3} startPoint Start point of ray.
 * @param {!o3djs.math.Vector3} endPoint End point of ray.
 * @return {o3djs.math.Vector3} The closest point on the line to the
 *     ray, or null if the ray is parallel to the line.
 */
o3djs.manipulators.Line_.prototype.closestPointToRay = function(startPoint,
                                                                endPoint) {
  // Consider a two-sided line and a one-sided ray, both in in 3D
  // space, and assume they are not parallel. Their parametric
  // formulation is:
  //
  //   p1 = point + t * dir
  //   p2 = raystart + u * raydir
  //
  // Here t and u are scalar parameter values, and the other values
  // are three-dimensional vectors. p1 and p2 are arbitrary points on
  // the line and ray, respectively.
  //
  // At the points cp1 and cp2 on these two lines where the line and
  // the ray are closest together, the line segment between cp1 and
  // cp2 is perpendicular to both of the lines.
  //
  // We can therefore write the following equations:
  //
  //   dot(   dir, (cp2 - cp1)) = 0
  //   dot(raydir, (cp2 - cp1)) = 0
  //
  // Define t' and u' as the parameter values for cp1 and cp2,
  // respectively. Expanding, these equations become
  //
  //   dot(   dir, ((raystart + u' * raydir) - (point + t' * dir))) = 0
  //   dot(raydir, ((raystart + u' * raydir) - (point + t' * dir))) = 0
  //
  // With some reshuffling, these can be expressed in vector/matrix
  // form:
  //
  //   [ dot(   dir, raystart) - dot(   dir, point) ]
  //   [ dot(raydir, raystart) - dot(raydir, point) ] +  (continued)
  //
  //       [ -dot(   dir, dir)   dot(   dir, raydir) ]   [ t' ]   [0]
  //       [ -dot(raydir, dir)   dot(raydir, raydir) ] * [ u' ] = [0]
  //
  // u' is the parameter for the world space ray being cast into the
  // screen. We can deduce whether the starting point of the ray is
  // actually the closest point to the infinite 3D line by whether the
  // value of u' is less than zero.
  var rayDirection = o3djs.math.subVector(endPoint, startPoint);
  var ddrd = o3djs.math.dot(this.direction_, rayDirection);
  var A = [[-o3djs.math.lengthSquared(this.direction_), ddrd],
           [ddrd, -o3djs.math.lengthSquared(rayDirection)]];
  var det = o3djs.math.det2(A);
  if (Math.abs(det) < o3djs.manipulators.EPSILON) {
    return null;
  }
  var Ainv = o3djs.math.inverse2(A);
  var b = [o3djs.math.dot(this.point_, this.direction_) -
           o3djs.math.dot(startPoint, this.direction_),
           o3djs.math.dot(startPoint, rayDirection) -
           o3djs.math.dot(this.point_, rayDirection)];
  var x = o3djs.math.mulMatrixVector(Ainv, b);
  if (x[1] < 0) {
    // Means that start point is closest point to this line
    return startPoint;
  } else {
    return o3djs.math.addVector(this.point_,
                                o3djs.math.mulScalarVector(
                                    x[0],
                                    this.direction_));
  }
}

/**
 * Performs internal recalculations when the parameters of the line change.
 * @private
 */
o3djs.manipulators.Line_.prototype.recalc_ = function() {
  var denom = o3djs.math.lengthSquared(this.direction_);
  if (denom == 0.0) {
    throw 'Line_.recalc_: ERROR: direction was the zero vector (not allowed)';
  }

  /**
   * Helper (internal cache) for computing projections along the line.
   * @private
   * @type {!o3djs.math.Vector3}
   */
  this.alongVec_ =
      o3djs.math.subVector(this.point_,
                           o3djs.math.mulScalarVector(
                               o3djs.math.dot(this.point_,
                                              this.direction_),
                               this.direction_));
}

/**
 * A vector with 4 entries, the R,G,B, and A components of the default color
 * for manipulators (used when not highlighted).
 * @type {!o3djs.math.Vector4}
 */
o3djs.manipulators.DEFAULT_COLOR = [0.8, 0.8, 0.8, 1.0];

/**
 * A vector with 4 entries, the R,G,B, and A components of the color used
 * for manipulators when they are highlighted.
 * @type {!o3djs.math.Vector4}
 */
o3djs.manipulators.HIGHLIGHTED_COLOR = [0.9, 0.9, 0.0, 1.0];


/**
 * Creates a new Plane object.
 * @constructor
 * @private
 * @param {!o3djs.math.Vector3} opt_normal The normal of the
 *     plane. Does not need to be unit length, but must not be the zero
 *     vector. Defaults to [0, 1, 0] if not specified.
 * @param {!o3djs.math.Vector3} opt_point A point through which the
 *     plane passes. Defaults to [0, 0, 0] if not specified.
 */
o3djs.manipulators.Plane_ = function(opt_normal,
                                     opt_point) {
  /**
   * A point through which the plane passes.
   * @private
   * @type {!o3djs.math.Vector3}
   */
  this.point_ = o3djs.math.copyVector(opt_point || [0, 0, 0]);
  this.setNormal(opt_normal || [0, 1, 0]);
}

/**
 * Sets the normal of this plane.
 * @private
 * @param {!o3djs.math.Vector3} normal The new normal of the
 *     plane. Does not need to be unit length, but must not be the zero
 *     vector.
 */
o3djs.manipulators.Plane_.prototype.setNormal = function(normal) {
  // Make sure the normal isn't zero.
  var denom = o3djs.math.lengthSquared(normal);
  if (denom == 0.0) {
    throw 'Plane_.setNormal: ERROR: normal was the zero vector (not allowed)';
  }

  /**
   * The normal to the plane. Normalized, cannot be zero.
   * @private
   * @type {!o3djs.math.Vector3}
   */
  this.normal_ = o3djs.math.normalize(normal); // Makes copy.
  this.recalc_();
}

/**
 * Gets the normal of this plane, as a unit vector.
 * @private
 * @return {!o3djs.math.Vector3} The (normalized) normal of the plane.
 */
o3djs.manipulators.Plane_.prototype.getNormal = function() {
  return this.normal_;
}

/**
 * Sets one point through which this plane passes.
 * @private
 * @param {!o3djs.math.Vector3} point A point through which the plane passes.
 */
o3djs.manipulators.Plane_.prototype.setPoint = function(point) {
  this.point_ = o3djs.math.copyVector(point);
  this.recalc_();
}

/**
 * Gets one point through which this plane passes.
 * @private
 * @return {!o3djs.math.Vector3} A point which through the plane passes.
 */
o3djs.manipulators.Plane_.prototype.getPoint = function() {
  return this.point_;
}

/**
 * Projects a point onto the plane.
 * @private
 * @param {!o3djs.math.Vector3} point Point to be projected.
 * @return {!o3djs.math.Vector3} Point on the plane closest to the
 *     passed point.
 */
o3djs.manipulators.Plane_.prototype.projectPoint = function(point) {
  var distFromPlane =
      o3djs.math.dot(this.normal_, point) - this.normalDotPoint_;
  return o3djs.math.subVector(point,
                              o3djs.math.mulScalarVector(distFromPlane,
                                                         this.normal_));
}

/**
 * Intersects a ray with the plane. Returns the point of intersection.
 * This is a two-sided ray cast. If the ray is parallel to the plane,
 * returns null.
 * @private
 * @param {!o3djs.math.Vector3} rayStart Start point of ray.
 * @param {!o3djs.math.Vector3} rayDirection Direction vector of ray.
 *     Does not need to be normalized, but must not be the zero vector.
 * @return {o3djs.math.Vector3} The point of intersection of the ray
 *     with the plane, or null if the ray is parallel to the plane.
 */
o3djs.manipulators.Plane_.prototype.intersectRay = function(rayStart,
                                                            rayDirection) {
  var distFromPlane =
      this.normalDotPoint_ - o3djs.math.dot(this.normal_, rayStart);
  var denom = o3djs.math.dot(this.normal_, rayDirection);
  if (denom == 0) {
    return null;
  }
  var t = distFromPlane / denom;
  return o3djs.math.addVector(rayStart,
                              o3djs.math.mulScalarVector(t, rayDirection));
}

/**
 * Performs internal recalculations when the parameters of the plane change.
 * @private
 */
o3djs.manipulators.Plane_.prototype.recalc_ = function() {
  /**
   * Helper (internal cache) for computing projections into the plane.
   * The dot product between normal_ and point_.
   * @private
   * @type {!number}
   */
  this.normalDotPoint_ = o3djs.math.dot(this.normal_, this.point_);
}



/**
 * Constructs a new manipulator manager. Do not call this directly;
 * use o3djs.manipulators.createManager instead.
 * @constructor
 * @param {!o3d.Pack} pack Pack in which manipulators' geometry and
 *     materials will be created.
 * @param {!o3d.DrawList} drawList The draw list against which
 *     internal materials are created.
 * @param {!o3d.Transform} parentTransform The parent transform under
 *     which the manipulators' geometry should be parented.
 * @param {!o3d.RenderNode} parentRenderNode The parent render node
 *     under which the manipulators' draw elements should be placed.
 * @param {number} renderNodePriority The priority that the
 *     manipulators' geometry should use for rendering.
 */
o3djs.manipulators.Manager = function(pack,
                                      drawList,
                                      parentTransform,
                                      parentRenderNode,
                                      renderNodePriority) {
  /**
   * Pack in which manipulators' geometry and materials are created.
   * @type {!o3d.Pack}
   */
  this.pack = pack;
  /**
   * The draw list against which internal materials are created.
   * @type {!o3d.DrawList}
   */
  this.drawList = drawList;
  /**
   * The parent transform under which the manipulators' geometry
   * shall be parented.
   * @type {!o3d.Transform}
   */
  this.parentTransform = parentTransform;
  /**
   * The parent render node under which the manipulators' draw elements
   * shall be placed.
   * @type {!o3d.RenderNode}
   */
  this.parentRenderNode = parentRenderNode;
  /**
   * The priority that the manipulators' geometry should use for rendering.
   * @type {number}
   */
  this.renderNodePriority = renderNodePriority;

  /**
   * The light position used by the manipulators' shader.
   * @type {!o3djs.math.Vector3}
   */
  this.lightPosition = [10, 10, 10];

  /**
   * The default material for manipulators (used when not highlighted).
   * @type {!o3d.Material}
   */
  this.defaultMaterial =
      this.createPhongMaterial_(o3djs.manipulators.DEFAULT_COLOR);
  /**
   * The material used for manipulators when they are highlighted.
   * (TODO(simonrad): This is not currently used; only defaultMaterial is used. Remove this?)
   * @type {!o3d.Material}
   */
  this.highlightedMaterial =
      this.createPhongMaterial_(o3djs.manipulators.HIGHLIGHTED_COLOR);

  /**
   * A map from the manip's parent Transform clientId to the manip.
   * @type {!Array.<!o3djs.manipulators.Manip>}
   */
  this.manipsByClientId = [];

  /**
   * Presumably we need a TransformInfo for the parentTransform.
   * @type {!o3djs.picking.TransformInfo}
   */
  this.transformInfo =
      o3djs.picking.createTransformInfo(this.parentTransform, null);

  /**
   * The currently-highlighted manipulator.
   * @type {o3djs.manipulators.Manip}
   */
  this.highlightedManip = null;

  /**
   * The manipulator currently being dragged.
   * @private
   * @type {o3djs.manipulators.Manip}
   */
  this.draggedManip_ = null;
}

/**
 * Creates a phong material based on the given single color.
 * @private
 * @param {!o3djs.math.Vector4} baseColor A vector with 4 entries, the
 *     R,G,B, and A components of a color.
 * @return {!o3d.Material} A phong material whose overall pigment is baseColor.
 */
o3djs.manipulators.Manager.prototype.createPhongMaterial_ =
    function(baseColor) {
  // Create a new, empty Material object.
  var material = this.pack.createObject('Material');

  o3djs.effect.attachStandardShader(
      this.pack, material, this.lightPosition, 'phong');

  material.drawList = this.drawList;

  // Assign parameters to the phong material.
  material.getParam('emissive').value = [0, 0, 0, 1];
  material.getParam('ambient').value =
    o3djs.math.mulScalarVector(0.1, baseColor);
  material.getParam('diffuse').value = baseColor;
  material.getParam('specular').value = [.2, .2, .2, 1];
  material.getParam('shininess').value = 20;

  return material;
}

/**
 * Creates a new Translate1 manipulator. A Translate1 moves along the
 * X axis in its local coordinate system.
 * @return {!o3djs.manipulators.Translate1} A new Translate1 manipulator.
 */
o3djs.manipulators.Manager.prototype.createTranslate1 = function() {
  var manip = new o3djs.manipulators.Translate1(this);
  this.add_(manip);
  return manip;
}

/**
 * Creates a new Translate2 manipulator. A Translate2 moves around the
 * XY plane in its local coordinate system.
 * @return {!o3djs.manipulators.Translate2} A new Translate2 manipulator.
 */
o3djs.manipulators.Manager.prototype.createTranslate2 = function() {
  var manip = new o3djs.manipulators.Translate2(this);
  this.add_(manip);
  return manip;
}

/**
 * Adds a manipulator to this manager's set.
 * @private
 * @param {!o3djs.manipulators.Manip} manip The manipulator to add.
 */
o3djs.manipulators.Manager.prototype.add_ = function(manip) {
  // Generate draw elements for the manipulator's transform
  manip.getTransform().createDrawElements(this.pack, null);
  // Add the manipulator's transform to the parent transform
  manip.getBaseTransform_().parent = this.parentTransform;
  // Add the manipulator into our managed list
  this.manipsByClientId[manip.getTransform().clientId] = manip;
}

/**
 * Event handler for multiple kinds of mouse events.
 * @private
 * @param {number} x The x coordinate of the mouse event.
 * @param {number} y The y coordinate of the mouse event.
 * @param {!o3djs.math.Matrix4} view The current view matrix.
 * @param {!o3djs.math.Matrix4} projection The current projection matrix.
 * @param {number} width The width of the viewport.
 * @param {number} height The height of the viewport.
 * @param {!function(!o3djs.manipulators.Manager,
 *     o3djs.picking.PickInfo, o3djs.manipulators.Manip): void} func
 *     Callback function. Always receives the manager as argument; if
 *     a manipulator was picked, receives non-null PickInfo and Manip
 *     arguments, otherwise receives null for both of these arguments.
 */
o3djs.manipulators.Manager.prototype.handleMouse_ = function(x,
                                                             y,
                                                             view,
                                                             projection,
                                                             width,
                                                             height,
                                                             func) {
  this.transformInfo.update();

  // Create the world ray
  var worldRay =
    o3djs.picking.clientPositionToWorldRayEx(x, y,
                                             view, projection,
                                             width, height);

  // Pick against all of the manipulators' geometry
  var pickResult = this.transformInfo.pick(worldRay);
  if (pickResult != null) {
    // Find which manipulator we picked.
    // NOTE this assumes some things about the transform graph
    // structure of the manipulators.
    var manip =
      this.manipsByClientId[pickResult.shapeInfo.parent.transform.clientId];
    func(this, pickResult, manip);
  } else {
    func(this, null, null);
  }
}

/**
 * Callback handling the mouse-down event on a manipulator.
 * @private
 * @param {!o3djs.manipulators.Manager} manager The manipulator
 *     manager owning the given manipulator.
 * @param {o3djs.picking.PickInfo} pickResult The picking information
 *     associated with the mouse-down event.
 * @param {o3djs.manipulators.Manip} manip The manipulator to be
 *     selected.
 */
o3djs.manipulators.mouseDownCallback_ = function(manager,
                                                 pickResult,
                                                 manip) {
  if (manip != null) {
    manager.draggedManip_ = manip;
    manip.makeActive(pickResult);
  }
}

/**
 * Callback handling the mouse-over event on a manipulator.
 * @private
 * @param {!o3djs.manipulators.Manager} manager The manipulator
 *     manager owning the given manipulator.
 * @param {o3djs.picking.PickInfo} pickResult The picking information
 *     associated with the mouse-over event.
 * @param {o3djs.manipulators.Manip} manip The manipulator to be
 *     highlighted.
 */
o3djs.manipulators.hoverCallback_ = function(manager,
                                             pickResult,
                                             manip) {
  if (manager.highlightedManip != null &&
      manager.highlightedManip != manip) {
    // Un-highlight the previously highlighted manipulator
    manager.highlightedManip.clearHighlight();
    manager.highlightedManip = null;
  }

  if (manip != null) {
    manip.highlight(pickResult);
    manager.highlightedManip = manip;
  }
}

/**
 * Method which should be called by end user code upon receiving a
 * mouse-down event.
 * @param {number} x The x coordinate of the mouse event.
 * @param {number} y The y coordinate of the mouse event.
 * @param {!o3djs.math.Matrix4} view The current view matrix.
 * @param {!o3djs.math.Matrix4} projection The current projection matrix.
 * @param {number} width The width of the viewport.
 * @param {number} height The height of the viewport.
 */
o3djs.manipulators.Manager.prototype.mousedown = function(x,
                                                          y,
                                                          view,
                                                          projection,
                                                          width,
                                                          height) {
  this.handleMouse_(x, y, view, projection, width, height,
                    o3djs.manipulators.mouseDownCallback_);
}

/**
 * Method which should be called by end user code upon receiving a
 * mouse motion event.
 * @param {number} x The x coordinate of the mouse event.
 * @param {number} y The y coordinate of the mouse event.
 * @param {!o3djs.math.Matrix4} view The current view matrix.
 * @param {!o3djs.math.Matrix4} projection The current projection matrix.
 * @param {number} width The width of the viewport.
 * @param {number} height The height of the viewport.
 */
o3djs.manipulators.Manager.prototype.mousemove = function(x,
                                                          y,
                                                          view,
                                                          projection,
                                                          width,
                                                          height) {
  if (this.draggedManip_ != null) {
    var worldRay =
      o3djs.picking.clientPositionToWorldRayEx(x, y,
                                               view, projection,
                                               width, height);
    this.draggedManip_.drag(worldRay.near, worldRay.far);
  } else {
    this.handleMouse_(x, y, view, projection, width, height,
                      o3djs.manipulators.hoverCallback_);
  }
}

/**
 * Method which should be called by end user code upon receiving a
 * mouse-up event.
 */
o3djs.manipulators.Manager.prototype.mouseup = function() {
  if (this.draggedManip_ != null) {
    this.draggedManip_.makeInactive();
    this.draggedManip_ = null;
  }
}

/**
 * Method which should be called by end user code, typically in
 * response to mouse move events, to update the transforms of
 * manipulators which might have been moved either because of
 * manipulators further up the hierarchy, or programmatic changes to
 * transforms.
 */
o3djs.manipulators.Manager.prototype.updateInactiveManipulators = function() {
  for (var ii in this.manipsByClientId) {
    var manip = this.manipsByClientId[ii];
    if (!manip.isActive()) {
      manip.updateBaseTransformFromAttachedTransform_();
    }
  }
}

/**
 * Base class for all manipulators.
 * @constructor
 * @param {!o3djs.manipulators.Manager} manager The manager of this
 *     manipulator.
 */
o3djs.manipulators.Manip = function(manager) {
  /**
   * The manager of this manipulator.
   * @private
   * @type {!o3djs.manipulators.Manager}
   */
  this.manager_ = manager;
  var pack = manager.pack;

  /**
   * This transform holds the local transformation of the manipulator,
   * which is either applied to the transform to which it is attached,
   * or (see below) consumed by the user in the manipulator's
   * callbacks. After each interaction, if there is an attached
   * transform, this local transform is added in to it and reset to
   * the identity.
   * TODO(kbr): add support for user callbacks on manipulators.
   * @private
   * @type {!o3d.Transform}
   */
  this.localTransform_ = pack.createObject('Transform');

  /**
   * This transform provides an offset, if desired, between the
   * manipulator's geometry and the transform (and, implicitly, the
   * shape) to which it is attached. This allows the manipulator to be
   * easily placed below an object, for example.
   * @private
   * @type {!o3d.Transform}
   */
  this.offsetTransform_ = pack.createObject('Transform');

  /**
   * This transform is the one which is actually parented to the
   * manager's parentTransform. It is used to place the manipulator in
   * world space, regardless of the world space location of the
   * parentTransform supplied to the manager. If this manipulator is
   * attached to a given transform, then upon completion of a
   * particular drag interaction, this transform is adjusted to take
   * into account the attached transform's new value.
   * @private
   * @type {!o3d.Transform}
   */
  this.baseTransform_ = pack.createObject('Transform');

  // Hook up these transforms
  this.localTransform_.parent = this.offsetTransform_;
  this.offsetTransform_.parent = this.baseTransform_;

  /**
   * This is the transform in the scene graph to which this
   * manipulator is conceptually "attached", and whose local transform
   * we are modifying.
   * @private
   * @type {o3d.Transform}
   */
  this.attachedTransform_ = null;

  /**
   * Whether this manipulator is active (ie being dragged).
   * @private
   * @type {boolean}
   */
  this.active_ = false;
}

/**
 * Adds shapes to the internal transform of this manipulator.
 * @private
 * @param {!Array.<!o3d.Shape>} shapes Array of shapes to add.
 */
o3djs.manipulators.Manip.prototype.addShapes_ = function(shapes) {
  for (var ii = 0; ii < shapes.length; ii++) {
    this.localTransform_.addShape(shapes[ii]);
  }
}

/**
 * Returns the "base" transform of this manipulator, which places the
 * origin of the manipulator at the local origin of the attached
 * transform.
 * @private
 * @return {!o3d.Transform} The base transform of this manipulator.
 */
o3djs.manipulators.Manip.prototype.getBaseTransform_ = function() {
  return this.baseTransform_;
}

/**
 * Returns the "offset" transform of this manipulator, which allows
 * the manipulator's geometry to be moved or rotated with respect to
 * the local origin of the attached transform.
 * @return {!o3d.Transform} The offset transform of this manipulator.
 */
o3djs.manipulators.Manip.prototype.getOffsetTransform = function() {
  return this.offsetTransform_;
}

/**
 * Returns the local transform of this manipulator, which contains the
 * changes that have been made in response to the current drag
 * operation. Upon completion of the drag, this transform's effects
 * are composed in to the attached transform, and this transform is
 * reset to the identity.
 * @return {!o3d.Transform} The local transform of this manipulator.
 */
o3djs.manipulators.Manip.prototype.getTransform = function() {
  return this.localTransform_;
}

/**
 * Sets the translation component of the offset transform. This is
 * useful for moving the manipulator's geometry with respect to the
 * local origin of the attached transform.
 * @param {!o3djs.math.Vector3} translation The offset translation for
 *     this manipulator.
 */
o3djs.manipulators.Manip.prototype.setOffsetTranslation =
    function(translation) {
  this.getOffsetTransform().localMatrix =
    o3djs.math.matrix4.setTranslation(this.getOffsetTransform().localMatrix,
                                      translation);
}

/**
 * Sets the rotation component of the offset transform. This is useful
 * for orienting the manipulator's geometry with respect to the local
 * origin of the attached transform.
 * @param {!o3djs.quaternions.Quaternion} quaternion The offset
 *     rotation for this manipulator.
 */
o3djs.manipulators.Manip.prototype.setOffsetRotation = function(quaternion) {
  var rot = o3djs.quaternions.quaternionToRotation(quaternion);
  this.getOffsetTransform().localMatrix =
    o3djs.math.matrix4.setUpper3x3(this.getOffsetTransform().localMatrix,
                                   rot);
}

/**
 * Explicitly sets the local translation of this manipulator.
 * (TODO(kbr): it is not clear that this capability should be in the
 * API.)
 * @param {!o3djs.math.Vector3} translation The local translation for
 *     this manipulator.
 */
o3djs.manipulators.Manip.prototype.setTranslation = function(translation) {
  this.getTransform().localMatrix =
    o3djs.math.matrix4.setTranslation(this.getTransform().localMatrix,
                                      translation);
}

/**
 * Explicitly sets the local rotation of this manipulator. (TODO(kbr):
 * it is not clear that this capability should be in the API.)
 * @param {!o3djs.quaternions.Quaternion} quaternion The local
 *     rotation for this manipulator.
 */
o3djs.manipulators.Manip.prototype.setRotation = function(quaternion) {
  var rot = o3djs.quaternions.quaternionToRotation(quaternion);
  this.getTransform().localMatrix =
    o3djs.math.matrix4.setUpper3x3(this.getTransform().localMatrix,
                                   rot);
}

/**
 * Attaches this manipulator to the given transform. Interactions with
 * the manipulator will cause this transform's local matrix to be
 * modified appropriately.
 * @param {!o3d.Transform} transform The transform to which this
 *     manipulator should be attached.
 */
o3djs.manipulators.Manip.prototype.attachTo = function(transform) {
  this.attachedTransform_ = transform;
  // Update our base transform to place the manipulator at exactly the
  // location of the attached transform.
  this.updateBaseTransformFromAttachedTransform_();
}

/**
 * Highlights this manipulator according to the given pick result.
 * @param {o3djs.picking.PickInfo} pickResult The pick result which
 *     caused this manipulator to become highlighted.
 */
o3djs.manipulators.Manip.prototype.highlight = function(pickResult) {
}

/**
 * Clears any highlight for this manipulator.
 */
o3djs.manipulators.Manip.prototype.clearHighlight = function() {
}

/**
 * Activates this manipulator according to the given pick result. In
 * complex manipulators, picking different portions of the manipulator
 * may result in different forms of interaction.
 * @param {o3djs.picking.PickInfo} pickResult The pick result which
 *     caused this manipulator to become active.
 */
o3djs.manipulators.Manip.prototype.makeActive = function(pickResult) {
  this.active_ = true;
}

/**
 * Deactivates this manipulator.
 */
o3djs.manipulators.Manip.prototype.makeInactive = function() {
  this.active_ = false;
}

/**
 * Drags this manipulator according to the world-space ray specified
 * by startPoint and endPoint. makeActive must already have been
 * called with the initial pick result causing this manipulator to
 * become active.
 * @param {!o3djs.math.Vector3} startPoint Start point of the
 *     world-space ray through the current mouse position.
 * @param {!o3djs.math.Vector3} endPoint End point of the world-space
 *     ray through the current mouse position.
 */
o3djs.manipulators.Manip.prototype.drag = function(startPoint,
                                                   endPoint) {
}

/**
 * Indicates whether this manipulator is active.
 * @return {boolean} Whether this manipulator is active.
 */
o3djs.manipulators.Manip.prototype.isActive = function() {
  return this.active_;
}

/**
 * Updates the base transform of this manipulator from the state of
 * its attached transform, resetting the local transform of this
 * manipulator to the identity.
 * @private
 */
o3djs.manipulators.Manip.prototype.updateBaseTransformFromAttachedTransform_ =
    function() {
  if (this.attachedTransform_ != null) {
    var attWorld = this.attachedTransform_.worldMatrix;
    var parWorld = this.manager_.parentTransform.worldMatrix;
    var parWorldInv = o3djs.math.matrix4.inverse(parWorld);
    this.baseTransform_.localMatrix =
        o3djs.math.matrix4.mul(attWorld, parWorldInv);
    // Reset the manipulator's local matrix to the identity.
    this.localTransform_.localMatrix = o3djs.math.matrix4.identity();
  }
}

/**
 * Updates this manipulator's attached transform based on the values
 * in the local transform.
 * @private
 */
o3djs.manipulators.Manip.prototype.updateAttachedTransformFromLocalTransform_ =
    function() {
  if (this.attachedTransform_ != null) {
    // Compute the composition of the base and local transforms.
    // The offset transform is skipped except for transforming the
    // local matrix's translation by the rotation component of the
    // offset transform.
    var base = this.baseTransform_.worldMatrix;
    var local = this.localTransform_.localMatrix;
    var xlate = o3djs.math.matrix4.getTranslation(local);
    var transformedXlate =
      o3djs.math.matrix4.transformDirection(
          this.offsetTransform_.localMatrix,
          o3djs.math.matrix4.getTranslation(local));
    o3djs.math.matrix4.setTranslation(local, transformedXlate);
    var totalMat = o3djs.math.matrix4.mul(base, local);

    // Set this into the attached transform, taking into account its
    // parent's transform, if any.
    // Note that we can not query the parent's transform directly, so
    // we compute it using a little trick.
    var attWorld = this.attachedTransform_.worldMatrix;
    var attLocal = this.attachedTransform_.localMatrix;
    var attParentMat =
      o3djs.math.matrix4.mul(attWorld,
                             o3djs.math.matrix4.inverse(attLocal));
    // Now we can take the inverse of this matrix
    var attParentMatInv = o3djs.math.matrix4.inverse(attParentMat);
    totalMat = o3djs.math.matrix4.mul(attParentMatInv, totalMat);
    this.attachedTransform_.localMatrix = totalMat;
  }
}

/**
 * Sets the material of the given shape's draw elements.
 * TODO(simonrad): This function is not used, remove it?
 * @private
 * @param {!o3d.Shape} shape Shape to modify the material of.
 * @param {!o3d.Material} material Material to set.
 */
o3djs.manipulators.Manip.prototype.setMaterial_ = function(shape, material) {
  var elements = shape.elements;
  for (var ii = 0; ii < elements.length; ii++) {
    var drawElements = elements[ii].drawElements;
    for (var jj = 0; jj < drawElements.length; jj++) {
      drawElements[jj].material = material;
    }
  }
}

/**
 * Sets the materials of the given shapes' draw elements.
 * TODO(simonrad): This function is not used, remove it?
 * @private
 * @param {!Array.<!o3d.Shape>} shapes Array of shapes to modify the materials of.
 * @param {!o3d.Material} material Material to set.
 */
o3djs.manipulators.Manip.prototype.setMaterials_ = function(shapes, material) {
  for (var ii = 0; ii < shapes.length; ii++) {
    this.setMaterial_(shapes[ii], material);
  }
}


/**
 * Create the geometry for a double-ended arrow going from
 * (0, -1, 0) to (0, 1, 0), transformed by the given matrix.
 * @private
 * @param {!o3djs.math.Matrix4} matrix A matrix by which to multiply
 *     all the vertices.
 * @return {!o3djs.primitives.VertexInfo} The created vertices.
 */
o3djs.manipulators.createArrowVertices_ = function(matrix) {
  var matrix4 = o3djs.math.matrix4;

  var verts = o3djs.primitives.createTruncatedConeVertices(
      0.15,    // Bottom radius.
      0.0,     // Top radius.
      0.3,     // Height.
      4,       // Number of radial subdivisions.
      1,       // Number of vertical subdivisions.
      matrix4.mul(matrix4.translation([0, 0.85, 0]), matrix));

  verts.append(o3djs.primitives.createCylinderVertices(
      0.06,    // Radius.
      1.4,     // Height.
      4,       // Number of radial subdivisions.
      1,       // Number of vertical subdivisions.
      matrix));

  verts.append(o3djs.primitives.createTruncatedConeVertices(
      0.0,     // Bottom radius.
      0.15,    // Top radius.
      0.3,     // Height.
      4,       // Number of radial subdivisions.
      1,       // Number of vertical subdivisions.
      matrix4.mul(matrix4.translation([0, -0.85, 0]), matrix)));

  return verts;
}


/**
 * A manipulator allowing an object to be dragged along a line.
 * @constructor
 * @extends {o3djs.manipulators.Manip}
 * @param {!o3djs.manipulators.Manager} manager The manager for the
 *     new Translate1 manipulator.
 */
o3djs.manipulators.Translate1 = function(manager) {
  o3djs.manipulators.Manip.call(this, manager);

  var pack = manager.pack;
  var material = manager.defaultMaterial;

  var shape = manager.translate1Shape_;
  if (!shape) {
    // Create the geometry for the manipulator, which looks like a
    // two-way arrow going from (-1, 0, 0) to (1, 0, 0).
    var verts = o3djs.manipulators.createArrowVertices_(
        o3djs.math.matrix4.rotationZ(Math.PI / 2));
    shape = verts.createShape(pack, material);
    manager.translate1Shape_ = shape;
  }

  this.addShapes_([ shape ]);

  this.transformInfo = o3djs.picking.createTransformInfo(this.getTransform(),
                                                         manager.transformInfo);

  /**
   * A parameter added to our transform to be able to change the
   * material's color for highlighting.
   * @private
   * @type {!o3d.ParamFloat4}
   */
  this.colorParam_ = this.getTransform().createParam('diffuse', 'ParamFloat4');
  this.clearHighlight();

  /**
   * Line along which we are dragging.
   * @private
   * @type {!o3djs.manipulators.Line_}
   */
  this.dragLine_ = new o3djs.manipulators.Line_();
}

o3djs.base.inherit(o3djs.manipulators.Translate1, o3djs.manipulators.Manip);

o3djs.manipulators.Translate1.prototype.highlight = function(pickResult) {
  // We can use instanced geometry for the entire Translate1 since its
  // entire color changes during highlighting.
  // TODO(kbr): support custom user geometry and associated callbacks.
  this.colorParam_.value = o3djs.manipulators.HIGHLIGHTED_COLOR;
}

o3djs.manipulators.Translate1.prototype.clearHighlight = function() {
  this.colorParam_.value = o3djs.manipulators.DEFAULT_COLOR;
}

o3djs.manipulators.Translate1.prototype.makeActive = function(pickResult) {
  o3djs.manipulators.Manip.prototype.makeActive.call(this, pickResult);
  this.highlight(pickResult);
  var worldMatrix = this.getTransform().worldMatrix;
  this.dragLine_.setDirection(
    o3djs.math.matrix4.transformDirection(worldMatrix,
                                          o3djs.manipulators.X_AXIS));
  this.dragLine_.setPoint(pickResult.worldIntersectionPosition);
}

o3djs.manipulators.Translate1.prototype.makeInactive = function() {
  o3djs.manipulators.Manip.prototype.makeInactive.call(this);
  this.clearHighlight();
  this.updateAttachedTransformFromLocalTransform_();
  this.updateBaseTransformFromAttachedTransform_();
}

o3djs.manipulators.Translate1.prototype.drag = function(startPoint,
                                                        endPoint) {
  // Algorithm: Find closest point of ray to dragLine_. Subtract this
  // point from the line's point to find difference vector; transform
  // from world to local coordinates to find new local offset of
  // manipulator.
  var closestPoint = this.dragLine_.closestPointToRay(startPoint, endPoint);
  if (closestPoint == null) {
    // Drag axis is parallel to ray. Punt.
    return;
  }
  // Need to do a world-to-local transformation on the difference vector.
  // Note that we also incorporate the translation portion of the matrix.
  var diffVector =
      o3djs.math.subVector(closestPoint, this.dragLine_.getPoint());
  var worldToLocal =
      o3djs.math.matrix4.inverse(this.getTransform().worldMatrix);
  this.getTransform().localMatrix =
      o3djs.math.matrix4.setTranslation(
          this.getTransform().localMatrix,
          o3djs.math.matrix4.transformDirection(worldToLocal,
                                                diffVector));
  this.updateAttachedTransformFromLocalTransform_();
}


/**
 * A manipulator allowing an object to be dragged around a plane.
 * @constructor
 * @extends {o3djs.manipulators.Manip}
 * @param {!o3djs.manipulators.Manager} manager The manager for the
 *     new Translate2 manipulator.
 */
o3djs.manipulators.Translate2 = function(manager) {
  o3djs.manipulators.Manip.call(this, manager);

  var pack = manager.pack;
  var material = manager.defaultMaterial;

  var shape = manager.Translate2Shape_;
  if (!shape) {
    // Create the geometry for the manipulator, which looks like
    // a two-way arrow going from (-1, 0, 0) to (1, 0, 0),
    // and another one going from (0, -1, 0) to (0, 1, 0).
    var verts = o3djs.manipulators.createArrowVertices_(
        o3djs.math.matrix4.rotationZ(Math.PI / 2));
    verts.append(o3djs.manipulators.createArrowVertices_(
        o3djs.math.matrix4.rotationZ(0)));
    shape = verts.createShape(pack, material);
    manager.Translate2Shape_ = shape;
  }

  this.addShapes_([ shape ]);

  this.transformInfo = o3djs.picking.createTransformInfo(this.getTransform(),
                                                         manager.transformInfo);

  /**
   * A parameter added to our transform to be able to change the
   * material's color for highlighting.
   * @private
   * @type {!o3d.ParamFloat4}
   */
  this.colorParam_ = this.getTransform().createParam('diffuse', 'ParamFloat4');
  this.clearHighlight();

  /**
   * Plane through which we are dragging.
   * @private
   * @type {!o3djs.manipulators.Plane_}
   */
  this.dragPlane_ = new o3djs.manipulators.Plane_();
}

o3djs.base.inherit(o3djs.manipulators.Translate2, o3djs.manipulators.Manip);

o3djs.manipulators.Translate2.prototype.highlight = function(pickResult) {
  // We can use instanced geometry for the entire Translate2 since its
  // entire color changes during highlighting.
  // TODO(kbr): support custom user geometry and associated callbacks.
  this.colorParam_.value = o3djs.manipulators.HIGHLIGHTED_COLOR;
}

o3djs.manipulators.Translate2.prototype.clearHighlight = function() {
  this.colorParam_.value = o3djs.manipulators.DEFAULT_COLOR;
}

o3djs.manipulators.Translate2.prototype.makeActive = function(pickResult) {
  o3djs.manipulators.Manip.prototype.makeActive.call(this, pickResult);
  this.highlight(pickResult);
  var worldMatrix = this.getTransform().worldMatrix;
  this.dragPlane_.setNormal(
    o3djs.math.matrix4.transformDirection(worldMatrix,
                                          o3djs.manipulators.Z_AXIS));
  this.dragPlane_.setPoint(pickResult.worldIntersectionPosition);
}

o3djs.manipulators.Translate2.prototype.makeInactive = function() {
  o3djs.manipulators.Manip.prototype.makeInactive.call(this);
  this.clearHighlight();
  this.updateAttachedTransformFromLocalTransform_();
  this.updateBaseTransformFromAttachedTransform_();
}

o3djs.manipulators.Translate2.prototype.drag = function(startPoint,
                                                        endPoint) {
  // Algorithm: Find intersection of ray with dragPlane_. Subtract this
  // point from the plane's point to find difference vector; transform
  // from world to local coordinates to find new local offset of
  // manipulator.
  var intersectPoint = this.dragPlane_.intersectRay(startPoint,
      o3djs.math.subVector(endPoint, startPoint));
  if (intersectPoint == null) {
    // Drag plane is parallel to ray. Punt.
    return;
  }
  // Need to do a world-to-local transformation on the difference vector.
  // Note that we also incorporate the translation portion of the matrix.
  var diffVector =
      o3djs.math.subVector(intersectPoint, this.dragPlane_.getPoint());
  var worldToLocal =
      o3djs.math.matrix4.inverse(this.getTransform().worldMatrix);
  this.getTransform().localMatrix =
      o3djs.math.matrix4.setTranslation(
          this.getTransform().localMatrix,
          o3djs.math.matrix4.transformDirection(worldToLocal,
                                                diffVector));
  this.updateAttachedTransformFromLocalTransform_();
}
