// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_NATIVE_METAFILE_H_
#define PRINTING_NATIVE_METAFILE_H_

#include "build/build_config.h"

// Define a metafile format for the current platform.  We use this platform
// independent define so we can define interfaces in platform agnostic manner.
// It is still an outstanding design issue whether we create classes on all
// platforms that have the same interface as Emf or if we change Emf to support
// multiple platforms (and rename to NativeMetafile).


#if defined(OS_WIN)

#include "printing/emf_win.h"

namespace printing {

typedef Emf NativeMetafile;

}  // namespace printing

#elif defined(OS_MACOSX)

// TODO(port): Printing using CG/PDF?
// The mock class is here so we can compile.
class NativeMetafile {
 public:
  int GetDataSize() const { return 0; }
 private:
  DISALLOW_COPY_AND_ASSIGN(NativeMetafile);
};

#elif defined(OS_LINUX)

#include "printing/pdf_ps_metafile_linux.h"

namespace printing {

typedef PdfPsMetafile NativeMetafile;

}  // namespace printing

#endif


#endif  // PRINTING_NATIVE_METAFILE_H_
