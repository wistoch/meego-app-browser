// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "webkit/tools/test_shell/image_decoder_unittest.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/md5.h"
#include "base/path_service.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/time.h"

using base::Time;

namespace {

// Determine if we should test with file specified by |path| based
// on |file_selection| and the |threshold| for the file size.
bool ShouldSkipFile(const FilePath& path,
                    ImageDecoderTestFileSelection file_selection,
                    const int64 threshold) {
  if (file_selection == TEST_ALL)
    return false;

  int64 image_size = 0;
  file_util::GetFileSize(path, &image_size);
  return (file_selection == TEST_SMALLER) == (image_size > threshold);
}

}  // anonymous namespace

void ReadFileToVector(const FilePath& path, Vector<char>* contents) {
  std::string contents_str;
  file_util::ReadFileToString(path, &contents_str);
  contents->resize(contents_str.size());
  memcpy(&contents->first(), contents_str.data(), contents_str.size());
}

FilePath GetMD5SumPath(const FilePath& path) {
  static const FilePath::StringType kDecodedDataExtension(
      FILE_PATH_LITERAL(".md5sum"));
  return FilePath(path.value() + kDecodedDataExtension);
}

#ifdef CALCULATE_MD5_SUMS
void SaveMD5Sum(const std::wstring& path, WebCore::RGBA32Buffer* buffer) {
  // Calculate MD5 sum.
  MD5Digest digest;
  scoped_ptr<NativeImageSkia> image_data(buffer->asNewNativeImage());
  {
    SkAutoLockPixels bmp_lock(*image_data);
    MD5Sum(image_data->getPixels(),
           image_data->width() * image_data->height() * sizeof(uint32_t),
           &digest);
  }

  // Write sum to disk.
  int bytes_written = file_util::WriteFile(path,
      reinterpret_cast<const char*>(&digest), sizeof digest);
  ASSERT_EQ(sizeof digest, bytes_written);
}
#else
void VerifyImage(WebCore::ImageDecoder* decoder,
                 const FilePath& path,
                 const FilePath& md5_sum_path,
                 size_t frame_index) {
  // Make sure decoding can complete successfully.
  EXPECT_TRUE(decoder->isSizeAvailable()) << path.value();
  EXPECT_GE(decoder->frameCount(), frame_index) << path.value();
  WebCore::RGBA32Buffer* image_buffer =
      decoder->frameBufferAtIndex(frame_index);
  ASSERT_NE(static_cast<WebCore::RGBA32Buffer*>(NULL), image_buffer) <<
      path.value();
  // EXPECT_EQ(WebCore::RGBA32Buffer::FrameComplete, image_buffer->status()) <<
  //     path.value();
  EXPECT_FALSE(decoder->failed()) << path.value();

  // Calculate MD5 sum.
  MD5Digest actual_digest;
  scoped_ptr<NativeImageSkia> image_data(image_buffer->asNewNativeImage());
  {
    SkAutoLockPixels bmp_lock(*image_data);
    MD5Sum(image_data->getPixels(),
           image_data->width() * image_data->height() * sizeof(uint32_t),
           &actual_digest);
  }

  // Read the MD5 sum off disk.
  std::string file_bytes;
  file_util::ReadFileToString(md5_sum_path, &file_bytes);
  MD5Digest expected_digest;
  ASSERT_EQ(sizeof expected_digest, file_bytes.size()) << path.value();
  memcpy(&expected_digest, file_bytes.data(), sizeof expected_digest);

  // Verify that the sums are the same.
  // EXPECT_EQ(0, memcmp(&expected_digest, &actual_digest, sizeof(MD5Digest))) <<
  //     path.value();
}
#endif

void ImageDecoderTest::SetUp() {
  FilePath data_dir;
  ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &data_dir));
  data_dir_ = data_dir.AppendASCII("webkit").
                       AppendASCII("data").
                       AppendASCII(format_ + "_decoder");
  ASSERT_TRUE(file_util::PathExists(data_dir_)) << data_dir_.value();
}

std::vector<FilePath> ImageDecoderTest::GetImageFiles() const {
  std::string pattern = "*." + format_;

  file_util::FileEnumerator enumerator(data_dir_,
                                       false,
                                       file_util::FileEnumerator::FILES);

  std::vector<FilePath> image_files;
  FilePath next_file_name;
  while (!(next_file_name = enumerator.Next()).empty()) {
    FilePath base_name = next_file_name.BaseName();
#if defined(OS_WIN)
    std::string base_name_ascii = WideToASCII(base_name.value());
#else
    std::string base_name_ascii = base_name.value();
#endif
    if (!MatchPatternASCII(base_name_ascii, pattern))
      continue;
    image_files.push_back(next_file_name);
  }

  return image_files;
}

bool ImageDecoderTest::ShouldImageFail(const FilePath& path) const {
  static const FilePath::StringType kBadSuffix(FILE_PATH_LITERAL(".bad."));
  return (path.value().length() > (kBadSuffix.length() + format_.length()) &&
          !path.value().compare(path.value().length() - format_.length() -
                                    kBadSuffix.length(),
                                kBadSuffix.length(), kBadSuffix));
}

WebCore::ImageDecoder* ImageDecoderTest::SetupDecoder(
    const FilePath& path,
    bool split_at_random) const {
  Vector<char> image_contents;
  ReadFileToVector(path, &image_contents);

  WebCore::ImageDecoder* decoder = CreateDecoder();
  RefPtr<WebCore::SharedBuffer> shared_contents(
      WebCore::SharedBuffer::create());

  if (split_at_random) {
    // Split the file at an arbitrary point.
    const int partial_size = static_cast<int>(
        (static_cast<double>(rand()) / RAND_MAX) * image_contents.size());
    shared_contents->append(image_contents.data(), partial_size);

    // Make sure the image decoder doesn't fail when we ask for the frame buffer
    // for this partial image.
    decoder->setData(shared_contents.get(), false);
    EXPECT_FALSE(decoder->failed()) << path.value();
    // NOTE: We can't check that frame 0 is non-NULL, because if this is an ICO
    // and we haven't yet supplied enough data to read the directory, there is
    // no framecount and thus no first frame.

    // Make sure passing the complete image results in successful decoding.
    shared_contents->append(
        &image_contents.data()[partial_size],
        static_cast<int>(image_contents.size() - partial_size));
  } else {
    shared_contents->append(image_contents.data(),
                            static_cast<int>(image_contents.size()));
  }

  decoder->setData(shared_contents.get(), true);
  return decoder;
}

void ImageDecoderTest::TestDecoding(
    ImageDecoderTestFileSelection file_selection,
    const int64 threshold) const {
  const std::vector<FilePath> image_files(GetImageFiles());
  for (std::vector<FilePath>::const_iterator i = image_files.begin();
       i != image_files.end(); ++i) {
    if (ShouldSkipFile(*i, file_selection, threshold))
      continue;

    scoped_ptr<WebCore::ImageDecoder> decoder(SetupDecoder(*i, false));
    if (ShouldImageFail(*i)) {
      // We may get a non-NULL frame buffer, but it should be incomplete, and
      // the decoder should have failed.
      WebCore::RGBA32Buffer* const image_buffer =
          decoder->frameBufferAtIndex(0);
      if (image_buffer) {
        EXPECT_NE(image_buffer->status(),
                  WebCore::RGBA32Buffer::FrameComplete) << i->value();
      }
      EXPECT_TRUE(decoder->failed()) << i->value();
      continue;
    }

#ifdef CALCULATE_MD5_SUMS
    SaveMD5Sum(GetMD5SumPath(*i), decoder->frameBufferAtIndex(0));
#else
    VerifyImage(decoder.get(), *i, GetMD5SumPath(*i), 0);
#endif
  }
}

#ifndef CALCULATE_MD5_SUMS
void ImageDecoderTest::TestChunkedDecoding(
    ImageDecoderTestFileSelection file_selection,
    const int64 threshold) const {
  // Init random number generator with current day, so a failing case will fail
  // consistently over the course of a whole day.
  const Time today = Time::Now().LocalMidnight();
  srand(static_cast<unsigned int>(today.ToInternalValue()));

  const std::vector<FilePath> image_files(GetImageFiles());
  for (std::vector<FilePath>::const_iterator i = image_files.begin();
       i != image_files.end(); ++i) {
    if (ShouldSkipFile(*i, file_selection, threshold))
      continue;

    if (ShouldImageFail(*i))
      continue;

    scoped_ptr<WebCore::ImageDecoder> decoder(SetupDecoder(*i, true));
    VerifyImage(decoder.get(), *i, GetMD5SumPath(*i), 0);
  }
}
#endif
