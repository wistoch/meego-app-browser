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


// This file contains the RendererCB::StateManager class, managing states for
// the command-buffer renderer.

#ifndef O3D_CORE_CROSS_COMMAND_BUFFER_STATES_CB_H_
#define O3D_CORE_CROSS_COMMAND_BUFFER_STATES_CB_H_

#include "core/cross/command_buffer/renderer_cb.h"
#include "command_buffer/common/cross/cmd_buffer_format.h"
#include "command_buffer/client/cross/cmd_buffer_helper.h"

namespace o3d {

// This class manages the states for the command-buffer renderer. It takes care
// of the registration of the state handlers, as well as the sending of
// commands to commit modified states.
class RendererCB::StateManager {
 public:
  StateManager() { }
  ~StateManager() { }

  // Sends commands to commit all the changed states.
  void ValidateStates(command_buffer::CommandBufferHelper *helper);

  // Adds the state handlers for all the states.
  void AddStateHandlers(RendererCB *renderer);
 private:
  // A template helper. This wraps a command sent to set a set of states. It
  // keeps all the arguments of a single command, that get modified by the
  // various handles, as well as a dirty bit.
  template <typename CommandType>
  class StateHelper {
   public:
    StateHelper() : dirty_(false) {
      // NOTE: This code assumes the state commands only need their
      // header set and that the rest will be set by the state handlers.
      memset(&command_, 0, sizeof(command_));
      command_.SetHeader();
    }

    // Sends the command if it is marked as dirty.
    void Validate(command_buffer::CommandBufferHelper *helper) {
      if (!dirty_) return;
      helper->AddTypedCmdData(command_);
      dirty_ = false;
    }

    CommandType& command() {
      return command_;
    }

    bool *dirty_ptr() { return &dirty_; }
   private:
    bool dirty_;
    CommandType command_;
    DISALLOW_COPY_AND_ASSIGN(StateHelper);
  };

  StateHelper<command_buffer::cmd::SetPointLineRaster> point_line_helper_;
  StateHelper<command_buffer::cmd::SetPolygonOffset> poly_offset_helper_;
  StateHelper<command_buffer::cmd::SetPolygonRaster> poly_raster_helper_;
  StateHelper<command_buffer::cmd::SetAlphaTest> alpha_test_helper_;
  StateHelper<command_buffer::cmd::SetDepthTest> depth_test_helper_;
  StateHelper<command_buffer::cmd::SetStencilTest> stencil_test_helper_;
  StateHelper<command_buffer::cmd::SetColorWrite> color_write_helper_;
  StateHelper<command_buffer::cmd::SetBlending> blending_helper_;
  StateHelper<command_buffer::cmd::SetBlendingColor> blending_color_helper_;
  DISALLOW_COPY_AND_ASSIGN(StateManager);
};

}  // namespace o3d

#endif  // O3D_CORE_CROSS_COMMAND_BUFFER_STATES_CB_H_
