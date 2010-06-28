// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PDF_PS_METAFILE_CAIRO_H_
#define PRINTING_PDF_PS_METAFILE_CAIRO_H_

#include <string>

#include "base/basictypes.h"

typedef struct _cairo_surface cairo_surface_t;
typedef struct _cairo cairo_t;

namespace base {
struct FileDescriptor;
}

class FilePath;

namespace printing {

// This class uses Cairo graphics library to generate PostScript/PDF stream
// and stores rendering results in a string buffer.
class PdfPsMetafile {
 public:
  enum FileFormat {
    PDF,
    PS,
  };

  // In the renderer process, callers should also call Init(void) to see if the
  // metafile can obtain all necessary rendering resources.
  // In the browser process, callers should also call Init(const void*, uint32)
  // to initialize the buffer |data_| to use SaveTo().
  explicit PdfPsMetafile(const FileFormat& format);

  ~PdfPsMetafile();

  // Initializes to a fresh new metafile. Returns true on success.
  // Note: Only call in the renderer to allocate rendering resources.
  bool Init();

  // Initializes a copy of metafile from PDF/PS stream data.
  // Returns true on success.
  // |src_buffer| should point to the shared memory which stores PDF/PS
  // contents generated in the renderer.
  // Note: Only call in the browser to initialize |data_|.
  bool Init(const void* src_buffer, uint32 src_buffer_size);

  FileFormat GetFileFormat() const { return format_; }

  // Prepares a new cairo surface/context for rendering a new page.
  // The unit is in point (=1/72 in).
  // Returns NULL when failed.
  cairo_t* StartPage(double width, double height);

  // Destroys the surface and the context used in rendering current page.
  // The results of current page will be appended into buffer |data_|.
  // Returns true on success.
  bool FinishPage();

  // Closes resulting PDF/PS file. No further rendering is allowed.
  void Close();

  // Returns size of PDF/PS contents stored in buffer |data_|.
  // This function should ONLY be called after PDF/PS file is closed.
  uint32 GetDataSize() const;

  // Copies PDF/PS contents stored in buffer |data_| into |dst_buffer|.
  // This function should ONLY be called after PDF/PS file is closed.
  // Returns true only when success.
  bool GetData(void* dst_buffer, uint32 dst_buffer_size) const;

  // Saves PDF/PS contents stored in buffer |data_| into the file
  // associated with |fd|.
  // This function should ONLY be called after PDF/PS file is closed.
  bool SaveTo(const base::FileDescriptor& fd) const;

  // The hardcoded margins, in points. These values are based on 72 dpi,
  // with 0.25 margins on top, left, and right, and 0.56 on bottom.
  static const double kTopMargin;
  static const double kRightMargin;
  static const double kBottomMargin;
  static const double kLeftMargin;

 private:
  // Cleans up all resources.
  void CleanUpAll();

  FileFormat format_;

  // Cairo surface and context for entire PDF/PS file.
  cairo_surface_t* surface_;
  cairo_t* context_;

  // Buffer stores PDF/PS contents for entire PDF/PS file.
  std::string data_;

  DISALLOW_COPY_AND_ASSIGN(PdfPsMetafile);
};

}  // namespace printing

#endif  // PRINTING_PDF_PS_METAFILE_CAIRO_H_
