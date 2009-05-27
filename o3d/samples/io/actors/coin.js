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
 * @fileoverview  This file defines the Coin class.
 */

/**
 * A Coin to be picked up
 */
function Coin(initObj) {
  this.absorbNamedValues(initObj);
  this.hasBeenPickedUp = false;
  this.width = 14;
  this.height = 14;
  this.isHidden = false;
}
Coin.prototype = new Actor;

Coin.prototype.onTick = function(timeElapsed) {
  if (this.isHidden == true) {
    return;
  } else if (this.hasBeenPickedUp == true) {
    this.z = (this.z*6 + eyeZ+40)/7;
    this.x = (this.x*3 + eyeX)/4;
    this.y = (this.y*3 + eyeY)/4;
    if (Math.abs(this.z - eyeZ+40) < 1) {
      this.isHidden = true;
      this.z = -10000;
    }
  } else {
    if (this.collidesWith(avatar)) {
      soundPlayer.play('sound/coin_3.mp3', 100, 0, true);
      this.hasBeenPickedUp = true;
    }
  }
  this.rotZ += .4 * 20 * timeElapsed;
  updateActor(this);
}
