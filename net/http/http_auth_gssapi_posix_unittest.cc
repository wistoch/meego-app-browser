// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_gssapi_posix.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/native_library.h"
#include "base/scoped_ptr.h"
#include "net/http/mock_gssapi_library_posix.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

// gss_buffer_t helpers.
void ClearBuffer(gss_buffer_t dest) {
  if (!dest)
    return;
  dest->length = 0;
  delete [] reinterpret_cast<char*>(dest->value);
  dest->value = NULL;
}

void SetBuffer(gss_buffer_t dest, const void* src, size_t length) {
  if (!dest)
    return;
  ClearBuffer(dest);
  if (!src)
    return;
  dest->length = length;
  if (length) {
    dest->value = new char[length];
    memcpy(dest->value, src, length);
  }
}

void CopyBuffer(gss_buffer_t dest, const gss_buffer_t src) {
  if (!dest)
    return;
  ClearBuffer(dest);
  if (!src)
    return;
  SetBuffer(dest, src->value, src->length);
}

}  // namespace

TEST(HttpAuthGSSAPIPOSIXTest, GSSAPIStartup) {
  // TODO(ahendrickson): Manipulate the libraries and paths to test each of the
  // libraries we expect, and also whether or not they have the interface
  // functions we want.
  GSSAPILibrary* gssapi = GSSAPILibrary::GetDefault();
  DCHECK(gssapi);
  gssapi->Init();
}

TEST(HttpAuthGSSAPIPOSIXTest, GSSAPICycle) {
  scoped_ptr<test::MockGSSAPILibrary> mock_library(new test::MockGSSAPILibrary);
  DCHECK(mock_library.get());
  mock_library->Init();
  const char kAuthResponse[] = "Mary had a little lamb";
  test::GssContextMockImpl context1(
      "localhost",                    // Source name
      "example.com",                  // Target name
      23,                             // Lifetime
      *GSS_C_NT_HOSTBASED_SERVICE,    // Mechanism
      0,                              // Context flags
      1,                              // Locally initiated
      0);                             // Open
  test::GssContextMockImpl context2(
      "localhost",                    // Source name
      "example.com",                  // Target name
      23,                             // Lifetime
      *GSS_C_NT_HOSTBASED_SERVICE,    // Mechanism
      0,                              // Context flags
      1,                              // Locally initiated
      1);                             // Open
  test::MockGSSAPILibrary::SecurityContextQuery queries[] = {
    { "Negotiate",                    // Package name
      GSS_S_CONTINUE_NEEDED,          // Major response code
      0,                              // Minor response code
      context1,                       // Context
      { 0, NULL },                           // Expected input token
      { arraysize(kAuthResponse),
        const_cast<char*>(kAuthResponse) }   // Output token
    },
    { "Negotiate",                    // Package name
      GSS_S_COMPLETE,                 // Major response code
      0,                              // Minor response code
      context2,                       // Context
      { arraysize(kAuthResponse),
        const_cast<char*>(kAuthResponse) },  // Expected input token
      { arraysize(kAuthResponse),
        const_cast<char*>(kAuthResponse) }   // Output token
    },
  };

  for (size_t i = 0; i < arraysize(queries); ++i) {
    mock_library->ExpectSecurityContext(queries[i].expected_package,
                                        queries[i].response_code,
                                        queries[i].minor_response_code,
                                        queries[i].context_info,
                                        queries[i].expected_input_token,
                                        queries[i].output_token);
  }

  OM_uint32 major_status = 0;
  OM_uint32 minor_status = 0;
  gss_cred_id_t initiator_cred_handle = NULL;
  gss_ctx_id_t context_handle = NULL;
  gss_name_t target_name = NULL;
  gss_OID mech_type = NULL;
  OM_uint32 req_flags = 0;
  OM_uint32 time_req = 25;
  gss_channel_bindings_t input_chan_bindings = NULL;
  gss_buffer_desc input_token = { 0, NULL };
  gss_OID actual_mech_type= NULL;
  gss_buffer_desc output_token = { 0, NULL };
  OM_uint32 ret_flags = 0;
  OM_uint32 time_rec = 0;
  for (size_t i = 0; i < arraysize(queries); ++i) {
    major_status = mock_library->init_sec_context(&minor_status,
                                                  initiator_cred_handle,
                                                  &context_handle,
                                                  target_name,
                                                  mech_type,
                                                  req_flags,
                                                  time_req,
                                                  input_chan_bindings,
                                                  &input_token,
                                                  &actual_mech_type,
                                                  &output_token,
                                                  &ret_flags,
                                                  &time_rec);
    CopyBuffer(&input_token, &output_token);
    ClearBuffer(&output_token);
  }
  ClearBuffer(&input_token);
  major_status = mock_library->delete_sec_context(&minor_status,
                                                  &context_handle,
                                                  GSS_C_NO_BUFFER);
}

}  // namespace net
