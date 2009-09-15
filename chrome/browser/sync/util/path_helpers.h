// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_UTIL_PATH_HELPERS_H_
#define CHROME_BROWSER_SYNC_UTIL_PATH_HELPERS_H_

#include <algorithm>
#include <iterator>
#include <string>

#include "chrome/browser/sync/util/compat_file.h"
#include "chrome/browser/sync/util/sync_types.h"

template <typename StringType>
class PathSegmentIterator : public std::iterator<std::forward_iterator_tag,
                                                 StringType> {
 public:
  explicit PathSegmentIterator(const StringType& path) :
  path_(path), segment_begin_(0), segment_end_(0) {
    ++(*this);
  }

  PathSegmentIterator() : segment_begin_(0), segment_end_(0) { }

  // Default copy constructors, constructors, etc. will all do the right thing.
  PathSegmentIterator& operator ++() {
    segment_begin_ =
        std::min(path_.size(),
                 path_.find_first_not_of(kPathSeparator, segment_end_));
    segment_end_ =
        std::min(path_.size(),
                 path_.find_first_of(kPathSeparator, segment_begin_));
    value_.assign(path_, segment_begin_, segment_end_ - segment_begin_);
    return *this;
  }

  PathSegmentIterator operator ++(int) {
    PathSegmentIterator i(*this);
    return ++i;
  }

  const StringType& operator * () const {
    return value_;
  }
  const StringType* operator -> () const {
    return &value_;
  }

  // If the current value and remaining path are equal, then we
  // call the iterators equal.
  bool operator == (const PathSegmentIterator& i) const {
    return 0 == path_.compare(segment_begin_,
      path_.size() - segment_begin_,
      i.path_, i.segment_begin_, i.path_.size() - i.segment_begin_);
  }

  bool operator != (const PathSegmentIterator& i) const {
    return !(*this == i);
  }

 protected:
  StringType path_;
  typename StringType::size_type segment_begin_;
  typename StringType::size_type segment_end_;
  StringType value_;
};

// NOTE: The functions (Strip)LastPathSegment always return values without a
// trailing slash.
PathString LastPathSegment(const PathString& path);
std::string LastPathSegment(const std::string& path);
PathString AppendSlash(const PathString& path);
PathString GetFullPath(const PathString& path);
PathString LowercasePath(const PathString& path);
PathString ExpandTilde(const PathString& path);

inline bool HasSuffixPathString(const PathString& str,
                                const PathString& suffix) {
  return str.find(suffix, str.size() - suffix.size()) != PathString::npos;
}

inline PathString StripSuffixPathString(const PathString& str,
                                        const PathString& suffix) {
  PathString ret(str);
  if (HasSuffixPathString(str, suffix)) {
    ret.resize(str.size() - suffix.size());
  }
  return ret;
}

// Returns a string with length or fewer elements, careful to not truncate a
// string mid-surrogate pair.
PathString TruncatePathString(const PathString& original, int length);

// Makes a path component legal for your OS, but doesn't handle collisions
// with other files in the same directory. it can do this by removing
// illegal characters and adding ~1 before the first '.' in the filename.
// returns PSTR("") if the name is fine as-is
// on mac/linux we let names stay unicode normalization form C in the system
// and convert to another normal form in fuse handlers. but, if a '/' is in
// a filename, we handle it here.
PathString MakePathComponentOSLegal(const PathString& component);

#endif  // CHROME_BROWSER_SYNC_UTIL_PATH_HELPERS_H_
