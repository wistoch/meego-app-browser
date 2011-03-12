// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Get basic type definitions.
#define IPC_MESSAGE_IMPL
#include "chrome/common/common_message_generator.h"
#include "chrome/common/common_param_traits.h"

// Generate constructors.
#include "ipc/struct_constructor_macros.h"
#include "chrome/common/common_message_generator.h"

// Generate destructors.
#include "ipc/struct_destructor_macros.h"
#include "chrome/common/common_message_generator.h"

// Generate param traits write methods.
#include "ipc/param_traits_write_macros.h"
namespace IPC {
#include "chrome/common/common_message_generator.h"
}  // namespace IPC

// Generate param traits read methods.
#include "ipc/param_traits_read_macros.h"
namespace IPC {
#include "chrome/common/common_message_generator.h"
}  // namespace IPC

// Generate param traits log methods.
#include "ipc/param_traits_log_macros.h"
namespace IPC {
#include "chrome/common/common_message_generator.h"
}  // namespace IPC

