// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_THEME_PACK_H_
#define CHROME_BROWSER_BROWSER_THEME_PACK_H_

#include <map>
#include <string>

#include "app/gfx/color_utils.h"
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/ref_counted.h"
#include "chrome/common/extensions/extension.h"

class BrowserThemeProviderTest;
namespace base {
class DataPack;
}
class DictionaryValue;
class FilePath;
class RefCountedMemory;

// An optimized representation of a theme, backed by a mmapped DataPack.
//
// The idea is to pre-process all images (tinting, compositing, etc) at theme
// install time, save all the PNG-ified data into an mmappable file so we don't
// suffer multiple file system access times, therefore solving two of the
// problems with the previous implementation.
//
// A note on const-ness. All public, non-static methods are const.  We do this
// because once we've constructed a BrowserThemePack through the
// BuildFromExtension() interface, we WriteToDisk() on a thread other than the
// UI thread that consumes a BrowserThemePack.
class BrowserThemePack : public base::RefCountedThreadSafe<BrowserThemePack> {
 public:
  ~BrowserThemePack();

  // Builds the theme pack from all data from |extension|. This is often done
  // on a separate thread as it takes so long.
  static BrowserThemePack* BuildFromExtension(Extension* extension);

  // Builds the theme pack from a previously WriteToDisk(). This operation
  // should be relatively fast, as it should be an mmap() and some pointer
  // swizzling. Returns NULL on any error attempting to read |path|.
  static scoped_refptr<BrowserThemePack> BuildFromDataPack(
      FilePath path, const std::string& expected_id);

  // Builds a data pack on disk at |path| for future quick loading by
  // BuildFromDataPack(). Often (but not always) called from the file thread;
  // implementation should be threadsafe because neither thread will write to
  // |image_memory_| and the worker thread will keep a reference to prevent
  // destruction.
  bool WriteToDisk(FilePath path) const;

  // Returns data from the pack, or the default value if |id| doesn't
  // exist. These methods should only be called from the UI thread. (But this
  // isn't enforced because of unit tests).
  bool GetTint(int id, color_utils::HSL* hsl) const;
  bool GetColor(int id, SkColor* color) const;
  bool GetDisplayProperty(int id, int* result) const;
  SkBitmap* GetBitmapNamed(int id) const;
  RefCountedMemory* GetRawData(int id) const;

  // Whether this theme provides an image for |id|.
  bool HasCustomImage(int id) const;

 private:
  friend class BrowserThemePackTest;

  // Cached images. We cache all retrieved and generated bitmaps and keep
  // track of the pointers. We own these and will delete them when we're done
  // using them.
  typedef std::map<int, SkBitmap*> ImageCache;

  // The raw PNG memory associated with a certain id.
  typedef std::map<int, scoped_refptr<RefCountedMemory> > RawImages;

  // Default. Everything is empty.
  BrowserThemePack();

  // Builds a header ready to write to disk.
  void BuildHeader(Extension* extension);

  // Transforms the JSON tint values into their final versions in the |tints_|
  // array.
  void BuildTintsFromJSON(DictionaryValue* tints_value);

  // Transforms the JSON color values into their final versions in the
  // |colors_| array and also fills in unspecified colors based on tint values.
  void BuildColorsFromJSON(DictionaryValue* color_value);

  // Implementation details of BuildColorsFromJSON().
  void ReadColorsFromJSON(DictionaryValue* colors_value,
                          std::map<int, SkColor>* temp_colors);
  void GenerateMissingColors(std::map<int, SkColor>* temp_colors);

  // Transforms the JSON display properties into |display_properties_|.
  void BuildDisplayPropertiesFromJSON(DictionaryValue* display_value);

  // Parses the image names out of an extension.
  void ParseImageNamesFromJSON(DictionaryValue* images_value,
                               FilePath images_path,
                               std::map<int, FilePath>* file_paths) const;

  // Loads the unmodified bitmaps packed in the extension to SkBitmaps.
  void LoadRawBitmapsTo(const std::map<int, FilePath>& file_paths,
                        ImageCache* raw_bitmaps);

  // Creates tinted and composited frame images. Source and destination is
  // |bitmaps|.
  void GenerateFrameImages(ImageCache* bitmaps) const;

  // Generates button images tinted with |button_tint| and places them in
  // processed_bitmaps.
  void GenerateTintedButtons(color_utils::HSL button_tint,
                             ImageCache* processed_bitmaps) const;

  // Generates the semi-transparent tab background images, putting the results
  // in |bitmaps|. Must be called after GenerateFrameImages().
  void GenerateTabBackgroundImages(ImageCache* bitmaps) const;

  // Takes all the SkBitmaps in |image_cache_|, encodes them as PNGs and places
  // them in |image_memory_|.
  void RepackImageCacheToImageMemory();

  // Takes all images in |source| and puts them in |destination|, freeing any
  // image already in |destination| that |source| would overwrite.
  void MergeImageCaches(const ImageCache& source,
                        ImageCache* destination) const;

  // Retrieves the tint OR the default tint. Unlike the public interface, we
  // always need to return a reasonable tint here, instead of partially
  // querying if the tint exists.
  color_utils::HSL GetTintInternal(int id) const;

  // Data pack, if we have one.
  scoped_ptr<base::DataPack> data_pack_;

  // All structs written to disk need to be packed; no alignment tricks here,
  // please.
#pragma pack(push,1)
  // Header that is written to disk.
  struct BrowserThemePackHeader {
    // Numeric version to make sure we're compatible in the future.
    int32 version;

    // 1 if little_endian. 0 if big_endian. On mismatch, abort load.
    int32 little_endian;

    // theme_id without NULL terminator.
    uint8 theme_id[16];
  } *header_;

  // The remaining structs represent individual entries in an array. For the
  // following three structs, BrowserThemePack will either allocate an array or
  // will point directly to mmapped data.
  struct TintEntry {
    int32 id;
    double h;
    double s;
    double l;
  } *tints_;

  struct ColorPair {
    int32 id;
    SkColor color;
  } *colors_;

  struct DisplayPropertyPair {
    int32 id;
    int32 property;
  } *display_properties_;
#pragma pack(pop)

  // References to raw PNG data. This map isn't touched when |data_pack_| is
  // non-NULL; |image_memory_| is only filled during BuildFromExtension(). Any
  // image data that needs to be written to the DataPack during WriteToDisk()
  // needs to be in |image_memory_|.
  RawImages image_memory_;

  // Tinted (or otherwise prepared) images for passing out. BrowserThemePack
  // owns all these images. This cache isn't touched when we write our data to
  // a DataPack.
  mutable ImageCache image_cache_;

  DISALLOW_COPY_AND_ASSIGN(BrowserThemePack);
};

#endif  // CHROME_BROWSER_BROWSER_THEME_PACK_H_
