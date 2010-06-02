// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/gfx/gl/gl_context.h"
#include "gpu/command_buffer/service/gpu_processor.h"

using ::base::SharedMemory;

namespace gpu {

bool GPUProcessor::Initialize(gfx::PluginWindowHandle window,
                              const gfx::Size& size,
                              GPUProcessor* parent,
                              uint32 parent_texture_id) {
  // Cannot reinitialize.
  if (context_.get())
    return false;

  // Get the parent decoder and the GLContext to share IDs with, if any.
  gles2::GLES2Decoder* parent_decoder = NULL;
  gfx::GLContext* parent_context = NULL;
  void* parent_handle = NULL;
  if (parent) {
    parent_decoder = parent->decoder_.get();
    DCHECK(parent_decoder);

    parent_context = parent_decoder->GetGLContext();
    DCHECK(parent_context);

    parent_handle = parent_context->GetHandle();
    DCHECK(parent_handle);
  }

  context_.reset(gfx::GLContext::CreateOffscreenGLContext(parent_handle));
  if (!context_.get())
    return false;

  // On Mac OS X since we can not render on-screen we don't even
  // attempt to create a view based GLContext. The only difference
  // between "on-screen" and "off-screen" rendering on this platform
  // is whether we allocate an AcceleratedSurface, which transmits the
  // rendering results back to the browser.
  if (window) {
#if !defined(UNIT_TEST)
    surface_.reset(new AcceleratedSurface());
    // TODO(apatrick): AcceleratedSurface will not work with an OSMesa context.
    if (!surface_->Initialize(
        static_cast<CGLContextObj>(context_->GetHandle()), false)) {
      Destroy();
      return false;
    }
#endif
  }

  return InitializeCommon(size, parent_decoder, parent_texture_id);

  return true;
}

void GPUProcessor::Destroy() {
#if !defined(UNIT_TEST)
  if (surface_.get()) {
    surface_->Destroy();
  }
  surface_.reset();
#endif
  DestroyCommon();
}

uint64 GPUProcessor::SetWindowSizeForIOSurface(const gfx::Size& size) {
#if !defined(UNIT_TEST)
  ResizeOffscreenFrameBuffer(size);
  decoder_->UpdateOffscreenFrameBufferSize();
  return surface_->SetSurfaceSize(size);
#else
  return 0;
#endif
}

TransportDIB::Handle GPUProcessor::SetWindowSizeForTransportDIB(
    const gfx::Size& size) {
#if !defined(UNIT_TEST)
  ResizeOffscreenFrameBuffer(size);
  decoder_->UpdateOffscreenFrameBufferSize();
  return surface_->SetTransportDIBSize(size);
#else
  return TransportDIB::DefaultHandleValue();
#endif
}

void GPUProcessor::SetTransportDIBAllocAndFree(
      Callback2<size_t, TransportDIB::Handle*>::Type* allocator,
      Callback1<TransportDIB::Id>::Type* deallocator) {
#if !defined(UNIT_TEST)
  surface_->SetTransportDIBAllocAndFree(allocator, deallocator);
#endif
}

void GPUProcessor::WillSwapBuffers() {
  DCHECK(context_->IsCurrent());
#if !defined(UNIT_TEST)
  if (surface_.get()) {
    surface_->SwapBuffers();
  }
#endif

  if (wrapped_swap_buffers_callback_.get()) {
    wrapped_swap_buffers_callback_->Run();
  }
}

}  // namespace gpu

