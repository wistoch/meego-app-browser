// Copyright (c) 2009, Google Inc.
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// 
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "config.h"

#include "v8_binding.h"
#include "v8_custom.h"
#include "v8_events.h"
#include "v8_proxy.h"

#include "CanvasPixelArray.h"

namespace WebCore {

// Get the specified value from the pixel buffer and return it wrapped as a
// JavaScript Number object to V8. Accesses outside the valid pixel buffer
// range return "undefined".
INDEXED_PROPERTY_GETTER(CanvasPixelArray) {
    INC_STATS("DOM.CanvasPixelArray.IndexedPropertyGetter");
    CanvasPixelArray* pixelBuffer =
        V8Proxy::ToNativeObject<CanvasPixelArray>(
            V8ClassIndex::CANVASPIXELARRAY,
            info.Holder());
  
    if ((index < 0) || (index >= pixelBuffer->length())) {
        return v8::Undefined();
    }
    return v8::Number::New(pixelBuffer->get(index));
}


// Set the specified value in the pixel buffer. Accesses outside the valid pixel
// buffer range are silently ignored.
INDEXED_PROPERTY_SETTER(CanvasPixelArray) {
    INC_STATS("DOM.CanvasPixelArray.IndexedPropertySetter");
    CanvasPixelArray* pixelBuffer =
        V8Proxy::ToNativeObject<CanvasPixelArray>(
            V8ClassIndex::CANVASPIXELARRAY,
            info.Holder());
  
    if ((index >= 0) && (index < pixelBuffer->length())) {
        pixelBuffer->set(index, value->NumberValue());
    }
    return value;
}


}  // namespace WebCore
