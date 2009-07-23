// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METADATA_PARSER_JPEG_H_
#define CHROME_BROWSER_METADATA_PARSER_JPEG_H_

#include "chrome/browser/parsers/metadata_parser_filebase.h"

class JpegMetadataParser : public FileMetadataParser {
 public:
  JpegMetadataParser(const FilePath& path);
  // Implementation of MetadataParser
  virtual bool Parse();

 private:
  DISALLOW_COPY_AND_ASSIGN(JpegMetadataParser);
};

#endif  // CHROME_BROWSER_METADATA_PARSER_JPEG_H_
