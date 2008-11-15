// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "net/base/filter.h"
#include "net/base/sdch_filter.h"
#include "net/url_request/url_request_http_job.cc"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/zlib.h"

//------------------------------------------------------------------------------
// Provide sample data and compression results with a sample VCDIFF dictionary.
// Note an SDCH dictionary has extra meta-data before the VCDIFF dictionary.
static const char kTestVcdiffDictionary[] = "DictionaryFor"
    "SdchCompression1SdchCompression2SdchCompression3SdchCompression\n";
// Pre-compression test data.
static const char kTestData[] = "TestData "
    "SdchCompression1SdchCompression2SdchCompression3SdchCompression\n";
// Note SDCH compressed data will include a reference to the SDCH dictionary.
static const char kCompressedTestData[] =
    "\326\303\304\0\0\001M\0\022I\0\t\003\001TestData \n\023\100\r";

//------------------------------------------------------------------------------

class SdchFilterTest : public testing::Test {
 protected:
  SdchFilterTest()
    : test_vcdiff_dictionary_(kTestVcdiffDictionary,
                              sizeof(kTestVcdiffDictionary) - 1),
      vcdiff_compressed_data_(kCompressedTestData,
                              sizeof(kCompressedTestData) - 1),
      expanded_(kTestData, sizeof(kTestData) - 1),
      sdch_manager_(new SdchManager) {
    sdch_manager_->EnableSdchSupport("");
  }

  std::string NewSdchCompressedData(const std::string dictionary);

  const std::string test_vcdiff_dictionary_;
  const std::string vcdiff_compressed_data_;
  const std::string expanded_;  // Desired final, decompressed data.

  scoped_ptr<SdchManager> sdch_manager_;  // A singleton database.
};

std::string SdchFilterTest::NewSdchCompressedData(
    const std::string dictionary) {
  std::string client_hash;
  std::string server_hash;
  SdchManager::GenerateHash(dictionary, &client_hash, &server_hash);

  // Build compressed data that refers to our dictionary.
  std::string compressed(server_hash);
  compressed.append("\0", 1);
  compressed.append(vcdiff_compressed_data_);
  return compressed;
}

//------------------------------------------------------------------------------


TEST_F(SdchFilterTest, Hashing) {
  std::string client_hash, server_hash;
  std::string dictionary("test contents");
  SdchManager::GenerateHash(dictionary, &client_hash, &server_hash);

  EXPECT_EQ(client_hash, "lMQBjS3P");
  EXPECT_EQ(server_hash, "MyciMVll");
}


//------------------------------------------------------------------------------
// Provide a generic helper function for trying to filter data.
// This function repeatedly calls the filter to process data, until the entire
// source is consumed.  The return value from the filter is appended to output.
// This allows us to vary input and output block sizes in order to test for edge
// effects (boundary effects?) during the filtering process.
// This function provides data to the filter in blocks of no-more-than the
// specified input_block_length.  It allows the filter to fill no more than
// output_buffer_length in any one call to proccess (a.k.a., Read) data, and
// concatenates all these little output blocks into the singular output string.
static bool FilterTestData(const std::string& source,
                           size_t input_block_length,
                           const size_t output_buffer_length,
                           Filter* filter, std::string* output) {
  CHECK(input_block_length > 0);
  Filter::FilterStatus status(Filter::FILTER_NEED_MORE_DATA);
  size_t source_index = 0;
  scoped_array<char> output_buffer(new char[output_buffer_length]);
  size_t input_amount = std::min(input_block_length,
      static_cast<size_t>(filter->stream_buffer_size()));

  do {
    int copy_amount = std::min(input_amount, source.size() - source_index);
    if (copy_amount > 0 && status == Filter::FILTER_NEED_MORE_DATA) {
      memcpy(filter->stream_buffer(), source.data() + source_index,
             copy_amount);
      filter->FlushStreamBuffer(copy_amount);
      source_index += copy_amount;
    }
    int buffer_length = output_buffer_length;
    status = filter->ReadData(output_buffer.get(), &buffer_length);
    output->append(output_buffer.get(), buffer_length);
    if (status == Filter::FILTER_ERROR)
      return false;
    if (copy_amount == 0 && buffer_length == 0)
      return true;
  } while (1);
}
//------------------------------------------------------------------------------
static std::string NewSdchDictionary(const std::string& domain) {
  std::string dictionary;
  if (!domain.empty()) {
    dictionary.append("Domain: ");
    dictionary.append(domain);
    dictionary.append("\n");
  }
  dictionary.append("\n");
  dictionary.append(kTestVcdiffDictionary, sizeof(kTestVcdiffDictionary) - 1);
  return dictionary;
}

//------------------------------------------------------------------------------

TEST_F(SdchFilterTest, BasicBadDictionary) {
  std::vector<Filter::FilterType> filter_types;
  filter_types.push_back(Filter::FILTER_TYPE_SDCH);
  const int kInputBufferSize(30);
  char output_buffer[20];
  scoped_ptr<Filter> filter(Filter::Factory(filter_types, kInputBufferSize));
  std::string url_string("http://ignore.com");
  filter->SetURL(GURL(url_string));


  // With no input data, try to read output.
  int output_bytes_or_buffer_size = sizeof(output_buffer);
  Filter::FilterStatus status = filter->ReadData(output_buffer,
                                                 &output_bytes_or_buffer_size);

  EXPECT_EQ(0, output_bytes_or_buffer_size);
  EXPECT_EQ(Filter::FILTER_NEED_MORE_DATA, status);

  // Supply bogus data (which doesnt't yet specify a full dictionary hash).
  // Dictionary hash is 8 characters followed by a null.
  std::string dictionary_hash_prefix("123");

  char* input_buffer = filter->stream_buffer();
  int input_buffer_size = filter->stream_buffer_size();
  EXPECT_EQ(kInputBufferSize, input_buffer_size);

  EXPECT_LT(static_cast<int>(dictionary_hash_prefix.size()),
            input_buffer_size);
  memcpy(input_buffer, dictionary_hash_prefix.data(),
         dictionary_hash_prefix.size());
  filter->FlushStreamBuffer(dictionary_hash_prefix.size());

  // With less than a dictionary specifier, try to read output.
  output_bytes_or_buffer_size = sizeof(output_buffer);
  status = filter->ReadData(output_buffer, &output_bytes_or_buffer_size);

  EXPECT_EQ(0, output_bytes_or_buffer_size);
  EXPECT_EQ(Filter::FILTER_NEED_MORE_DATA, status);

  // Provide enough data to complete *a* hash, but it is bogus, and not in our
  // list of dictionaries, so the filter should error out immediately.
  std::string dictionary_hash_postfix("4abcd\0", 6);

  CHECK(dictionary_hash_postfix.size() <
        static_cast<size_t>(input_buffer_size));
  memcpy(input_buffer, dictionary_hash_postfix.data(),
         dictionary_hash_postfix.size());
  filter->FlushStreamBuffer(dictionary_hash_postfix.size());

  // With a non-existant dictionary specifier, try to read output.
  output_bytes_or_buffer_size = sizeof(output_buffer);
  status = filter->ReadData(output_buffer, &output_bytes_or_buffer_size);

  EXPECT_EQ(0, output_bytes_or_buffer_size);
  EXPECT_EQ(Filter::FILTER_ERROR, status);

  EXPECT_FALSE(SdchManager::Global()->IsInSupportedDomain(GURL(url_string)));
  SdchManager::ClearBlacklistings();
  EXPECT_TRUE(SdchManager::Global()->IsInSupportedDomain(GURL(url_string)));
}

TEST_F(SdchFilterTest, DictionaryAddOnce) {
  // Construct a valid SDCH dictionary from a VCDIFF dictionary.
  const std::string kSampleDomain = "sdchtest.com";
  std::string dictionary(NewSdchDictionary(kSampleDomain));

  std::string url_string = "http://" + kSampleDomain;
  GURL url(url_string);
  EXPECT_TRUE(sdch_manager_->AddSdchDictionary(dictionary, url));

  // Check we can't add it twice.
  EXPECT_FALSE(sdch_manager_->AddSdchDictionary(dictionary, url));

  const std::string kSampleDomain2 = "sdchtest2.com";

  // Construct a second SDCH dictionary from a VCDIFF dictionary.
  std::string dictionary2(NewSdchDictionary(kSampleDomain2));

  std::string url_string2 = "http://" + kSampleDomain2;
  GURL url2(url_string2);
  EXPECT_TRUE(sdch_manager_->AddSdchDictionary(dictionary2, url2));
}

TEST_F(SdchFilterTest, BasicDictionary) {
  // Construct a valid SDCH dictionary from a VCDIFF dictionary.
  const std::string kSampleDomain = "sdchtest.com";
  std::string dictionary(NewSdchDictionary(kSampleDomain));

  std::string url_string = "http://" + kSampleDomain;

  GURL url(url_string);
  EXPECT_TRUE(sdch_manager_->AddSdchDictionary(dictionary, url));

  std::string compressed(NewSdchCompressedData(dictionary));

  std::vector<Filter::FilterType> filter_types;
  filter_types.push_back(Filter::FILTER_TYPE_SDCH);

  // Decode with a large buffer (larger than test input, or compressed data).
  const int kInputBufferSize(100);
  scoped_ptr<Filter> filter(Filter::Factory(filter_types, kInputBufferSize));
  filter->SetURL(url);

  size_t feed_block_size = 100;
  size_t output_block_size = 100;
  std::string output;
  EXPECT_TRUE(FilterTestData(compressed, feed_block_size, output_block_size,
                             filter.get(), &output));
  EXPECT_EQ(output, expanded_);

  // Decode with really small buffers (size 1) to check for edge effects.
  filter.reset((Filter::Factory(filter_types, kInputBufferSize)));
  filter->SetURL(url);

  feed_block_size = 1;
  output_block_size = 1;
  output.clear();
  EXPECT_TRUE(FilterTestData(compressed, feed_block_size, output_block_size,
                             filter.get(), &output));
  EXPECT_EQ(output, expanded_);
}

TEST_F(SdchFilterTest, CrossDomainDictionaryUse) {
  // Construct a valid SDCH dictionary from a VCDIFF dictionary.
  const std::string kSampleDomain = "sdchtest.com";
  std::string dictionary(NewSdchDictionary(kSampleDomain));

  std::string url_string = "http://" + kSampleDomain;

  GURL url(url_string);
  EXPECT_TRUE(sdch_manager_->AddSdchDictionary(dictionary, url));

  std::string compressed(NewSdchCompressedData(dictionary));

  std::vector<Filter::FilterType> filter_types;
  filter_types.push_back(Filter::FILTER_TYPE_SDCH);
  const int kInputBufferSize(100);

  // Decode with content arriving from the "wrong" domain.
  // This tests CanSet() in the sdch_manager_->
  scoped_ptr<Filter> filter((Filter::Factory(filter_types,  kInputBufferSize)));
  GURL wrong_domain_url("http://www.wrongdomain.com");
  filter->SetURL(wrong_domain_url);

  size_t feed_block_size = 100;
  size_t output_block_size = 100;
  std::string output;
  EXPECT_FALSE(FilterTestData(compressed, feed_block_size, output_block_size,
                              filter.get(), &output));
  EXPECT_EQ(output.size(), 0u);  // No output written.

  EXPECT_TRUE(SdchManager::Global()->IsInSupportedDomain(GURL(url_string)));
  EXPECT_FALSE(SdchManager::Global()->IsInSupportedDomain(wrong_domain_url));
  SdchManager::ClearBlacklistings();
  EXPECT_TRUE(SdchManager::Global()->IsInSupportedDomain(wrong_domain_url));
}

TEST_F(SdchFilterTest, DictionaryPathValidation) {
  // Construct a valid SDCH dictionary from a VCDIFF dictionary.
  const std::string kSampleDomain = "sdchtest.com";
  std::string dictionary(NewSdchDictionary(kSampleDomain));

  std::string url_string = "http://" + kSampleDomain;

  GURL url(url_string);
  EXPECT_TRUE(sdch_manager_->AddSdchDictionary(dictionary, url));

  // Create a dictionary with a path restriction, by prefixing dictionary.
  const std::string path("/special_path/bin");
  std::string dictionary_with_path("Path: " + path + "\n");
  dictionary_with_path.append(dictionary);
  EXPECT_TRUE(sdch_manager_->AddSdchDictionary(dictionary_with_path, url));

  std::string compressed_for_path(NewSdchCompressedData(dictionary_with_path));

  std::vector<Filter::FilterType> filter_types;
  filter_types.push_back(Filter::FILTER_TYPE_SDCH);
  const int kInputBufferSize(100);

  // Test decode the path data, arriving from a valid path.
  scoped_ptr<Filter> filter((Filter::Factory(filter_types, kInputBufferSize)));
  filter->SetURL(GURL(url_string + path));

  size_t feed_block_size = 100;
  size_t output_block_size = 100;
  std::string output;

  EXPECT_TRUE(FilterTestData(compressed_for_path, feed_block_size,
                             output_block_size, filter.get(), &output));
  EXPECT_EQ(output, expanded_);

  // Test decode the path data, arriving from a invalid path.
  filter.reset((Filter::Factory(filter_types, kInputBufferSize)));
  filter->SetURL(GURL(url_string));

  feed_block_size = 100;
  output_block_size = 100;
  output.clear();
  EXPECT_FALSE(FilterTestData(compressed_for_path, feed_block_size,
                              output_block_size, filter.get(), &output));
  EXPECT_EQ(output.size(), 0u);  // No output written.

  EXPECT_FALSE(SdchManager::Global()->IsInSupportedDomain(GURL(url_string)));
  SdchManager::ClearBlacklistings();
  EXPECT_TRUE(SdchManager::Global()->IsInSupportedDomain(GURL(url_string)));
}

TEST_F(SdchFilterTest, DictionaryPortValidation) {
  // Construct a valid SDCH dictionary from a VCDIFF dictionary.
  const std::string kSampleDomain = "sdchtest.com";
  std::string dictionary(NewSdchDictionary(kSampleDomain));

  std::string url_string = "http://" + kSampleDomain;

  GURL url(url_string);
  EXPECT_TRUE(sdch_manager_->AddSdchDictionary(dictionary, url));


  // Create a dictionary with a port restriction, by prefixing old dictionary.
  const std::string port("502");
  std::string dictionary_with_port("Port: " + port + "\n");
  dictionary_with_port.append("Port: 80\n");  // Add default port.
  dictionary_with_port.append(dictionary);
  EXPECT_TRUE(sdch_manager_->AddSdchDictionary(dictionary_with_port,
                                            GURL(url_string + ":" + port)));

  std::string compressed_for_port(NewSdchCompressedData(dictionary_with_port));

  std::vector<Filter::FilterType> filter_types;
  filter_types.push_back(Filter::FILTER_TYPE_SDCH);
  const int kInputBufferSize(100);

  // Test decode the port data, arriving from a valid port.
  scoped_ptr<Filter> filter((Filter::Factory(filter_types, kInputBufferSize)));
  filter->SetURL(GURL(url_string + ":" + port));

  size_t feed_block_size = 100;
  size_t output_block_size = 100;
  std::string output;
  EXPECT_TRUE(FilterTestData(compressed_for_port, feed_block_size,
                             output_block_size, filter.get(), &output));
  EXPECT_EQ(output, expanded_);

  // Test decode the port data, arriving from a valid (default) port.
  filter.reset((Filter::Factory(filter_types, kInputBufferSize)));
  filter->SetURL(GURL(url_string));  // Default port.

  feed_block_size = 100;
  output_block_size = 100;
  output.clear();
  EXPECT_TRUE(FilterTestData(compressed_for_port, feed_block_size,
                             output_block_size, filter.get(), &output));
  EXPECT_EQ(output, expanded_);

  // Test decode the port data, arriving from a invalid port.
  filter.reset((Filter::Factory(filter_types, kInputBufferSize)));
  filter->SetURL(GURL(url_string + ":" + port + "1"));

  feed_block_size = 100;
  output_block_size = 100;
  output.clear();
  EXPECT_FALSE(FilterTestData(compressed_for_port, feed_block_size,
                              output_block_size, filter.get(), &output));
  EXPECT_EQ(output.size(), 0u);  // No output written.

  EXPECT_FALSE(SdchManager::Global()->IsInSupportedDomain(GURL(url_string)));
  SdchManager::ClearBlacklistings();
  EXPECT_TRUE(SdchManager::Global()->IsInSupportedDomain(GURL(url_string)));
}

//------------------------------------------------------------------------------
// Helper function to perform gzip compression of data.

static std::string gzip_compress(const std::string &input) {
  z_stream zlib_stream;
  memset(&zlib_stream, 0, sizeof(zlib_stream));
  int code;

  // Initialize zlib
  code = deflateInit2(&zlib_stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                      -MAX_WBITS,
                      8,  // DEF_MEM_LEVEL
                      Z_DEFAULT_STRATEGY);

  CHECK(code == Z_OK);

  // Fill in zlib control block
  zlib_stream.next_in = bit_cast<Bytef*>(input.data());
  zlib_stream.avail_in = input.size();

  // Assume we can compress into similar buffer (add 100 bytes to be sure).
  size_t gzip_compressed_length = zlib_stream.avail_in + 100;
  scoped_array<char> gzip_compressed(new char[gzip_compressed_length]);
  zlib_stream.next_out = bit_cast<Bytef*>(gzip_compressed.get());
  zlib_stream.avail_out = gzip_compressed_length;

  // The GZIP header (see RFC 1952):
  //   +---+---+---+---+---+---+---+---+---+---+
  //   |ID1|ID2|CM |FLG|     MTIME     |XFL|OS |
  //   +---+---+---+---+---+---+---+---+---+---+
  //     ID1     \037
  //     ID2     \213
  //     CM      \010 (compression method == DEFLATE)
  //     FLG     \000 (special flags that we do not support)
  //     MTIME   Unix format modification time (0 means not available)
  //     XFL     2-4? DEFLATE flags
  //     OS      ???? Operating system indicator (255 means unknown)
  //
  // Header value we generate:
  const char kGZipHeader[] = { '\037', '\213', '\010', '\000', '\000',
                               '\000', '\000', '\000', '\002', '\377' };
  CHECK(zlib_stream.avail_out > sizeof(kGZipHeader));
  memcpy(zlib_stream.next_out, kGZipHeader, sizeof(kGZipHeader));
  zlib_stream.next_out += sizeof(kGZipHeader);
  zlib_stream.avail_out -= sizeof(kGZipHeader);

  // Do deflate
  code = MOZ_Z_deflate(&zlib_stream, Z_FINISH);
  gzip_compressed_length -= zlib_stream.avail_out;
  std::string compressed(gzip_compressed.get(), gzip_compressed_length);
  MOZ_Z_deflateEnd(&zlib_stream);
  return compressed;
}

//------------------------------------------------------------------------------

// Test that filters can be cascaded (chained) so that the output of one filter
// is processed by the next one.  This is most critical for SDCH, which is
// routinely followed by gzip (during encoding).  The filter we'll test for will
// do the gzip decoding first, and then decode the SDCH content.
TEST_F(SdchFilterTest, FilterChaining) {
  // Construct a valid SDCH dictionary from a VCDIFF dictionary.
  const std::string kSampleDomain = "sdchtest.com";
  std::string dictionary(NewSdchDictionary(kSampleDomain));

  std::string url_string = "http://" + kSampleDomain;

  GURL url(url_string);
  EXPECT_TRUE(sdch_manager_->AddSdchDictionary(dictionary, url));

  std::string sdch_compressed(NewSdchCompressedData(dictionary));

  // Use Gzip to compress the sdch sdch_compressed data.
  std::string gzip_compressed_sdch = gzip_compress(sdch_compressed);

  // Construct a chained filter.
  std::vector<Filter::FilterType> filter_types;
  filter_types.push_back(Filter::FILTER_TYPE_SDCH);
  filter_types.push_back(Filter::FILTER_TYPE_GZIP);

  // First try with a large buffer (larger than test input, or compressed data).
  const int kInputBufferSize(100);
  scoped_ptr<Filter> filter(Filter::Factory(filter_types, kInputBufferSize));
  filter->SetURL(url);

  // Verify that chained filter is waiting for data.
  char tiny_output_buffer[10];
  int tiny_output_size = sizeof(tiny_output_buffer);
  EXPECT_EQ(Filter::FILTER_NEED_MORE_DATA,
            filter->ReadData(tiny_output_buffer, &tiny_output_size));

  size_t feed_block_size = 100;
  size_t output_block_size = 100;
  std::string output;
  EXPECT_TRUE(FilterTestData(gzip_compressed_sdch, feed_block_size,
                             output_block_size, filter.get(), &output));
  EXPECT_EQ(output, expanded_);

  // Next try with a tiny buffer to cover edge effects.
  filter.reset(Filter::Factory(filter_types, kInputBufferSize));
  filter->SetURL(url);

  feed_block_size = 1;
  output_block_size = 1;
  output.clear();
  EXPECT_TRUE(FilterTestData(gzip_compressed_sdch, feed_block_size,
                             output_block_size, filter.get(), &output));
  EXPECT_EQ(output, expanded_);
}

TEST_F(SdchFilterTest, DefaultGzipIfSdch) {
  // Construct a valid SDCH dictionary from a VCDIFF dictionary.
  const std::string kSampleDomain = "sdchtest.com";
  std::string dictionary(NewSdchDictionary(kSampleDomain));

  std::string url_string = "http://" + kSampleDomain;

  GURL url(url_string);
  EXPECT_TRUE(sdch_manager_->AddSdchDictionary(dictionary, url));

  std::string sdch_compressed(NewSdchCompressedData(dictionary));

  // Use Gzip to compress the sdch sdch_compressed data.
  std::string gzip_compressed_sdch = gzip_compress(sdch_compressed);

  // Only claim to have sdch content, but really use the gzipped sdch content.
  // System should automatically add the missing (optional) gzip.
  std::vector<Filter::FilterType> filter_types;
  filter_types.push_back(Filter::FILTER_TYPE_SDCH);
  Filter::FixupEncodingTypes(true, "anything/mime", &filter_types);

  // First try with a large buffer (larger than test input, or compressed data).
  const int kInputBufferSize(100);
  scoped_ptr<Filter> filter(Filter::Factory(filter_types, kInputBufferSize));
  filter->SetURL(url);

  // Verify that chained filter is waiting for data.
  char tiny_output_buffer[10];
  int tiny_output_size = sizeof(tiny_output_buffer);
  EXPECT_EQ(Filter::FILTER_NEED_MORE_DATA,
            filter->ReadData(tiny_output_buffer, &tiny_output_size));

  size_t feed_block_size = 100;
  size_t output_block_size = 100;
  std::string output;
  EXPECT_TRUE(FilterTestData(gzip_compressed_sdch, feed_block_size,
                             output_block_size, filter.get(), &output));
  EXPECT_EQ(output, expanded_);

  // Next try with a tiny buffer to cover edge effects.
  filter.reset(Filter::Factory(filter_types, kInputBufferSize));
  filter->SetURL(url);

  feed_block_size = 1;
  output_block_size = 1;
  output.clear();
  EXPECT_TRUE(FilterTestData(gzip_compressed_sdch, feed_block_size,
                             output_block_size, filter.get(), &output));
  EXPECT_EQ(output, expanded_);
}

TEST_F(SdchFilterTest, DomainSupported) {
  GURL test_url("http://www.test.com");
  GURL google_url("http://www.google.com");

  EXPECT_TRUE(SdchManager::sdch_enabled());
  EXPECT_TRUE(SdchManager::Global()->IsInSupportedDomain(test_url));
  sdch_manager_->EnableSdchSupport(".google.com");
  EXPECT_FALSE(SdchManager::Global()->IsInSupportedDomain(test_url));
  EXPECT_TRUE(SdchManager::Global()->IsInSupportedDomain(google_url));
}

TEST_F(SdchFilterTest, DomainBlacklisting) {
  GURL test_url("http://www.test.com");
  GURL google_url("http://www.google.com");

  SdchManager::BlacklistDomain(test_url);
  EXPECT_FALSE(SdchManager::Global()->IsInSupportedDomain(test_url));
  EXPECT_TRUE(SdchManager::Global()->IsInSupportedDomain(google_url));

  SdchManager::BlacklistDomain(google_url);
  EXPECT_FALSE(SdchManager::Global()->IsInSupportedDomain(test_url));
  EXPECT_FALSE(SdchManager::Global()->IsInSupportedDomain(google_url));
}

TEST_F(SdchFilterTest, CanSetExactMatchDictionary) {
  std::string dictionary_domain("x.y.z.google.com");
  std::string dictionary_text(NewSdchDictionary(dictionary_domain));

  // Perfect match should work.
  EXPECT_TRUE(sdch_manager_->AddSdchDictionary(dictionary_text,
              GURL("http://" + dictionary_domain)));
}

TEST_F(SdchFilterTest, FailToSetDomainMismatchDictionary) {
  std::string dictionary_domain("x.y.z.google.com");
  std::string dictionary_text(NewSdchDictionary(dictionary_domain));

  // Fail the "domain match" requirement.
  EXPECT_FALSE(sdch_manager_->AddSdchDictionary(dictionary_text,
               GURL("http://y.z.google.com")));
}

TEST_F(SdchFilterTest, FailToSetDotHostPrefixDomainDictionary) {
  std::string dictionary_domain("x.y.z.google.com");
  std::string dictionary_text(NewSdchDictionary(dictionary_domain));

  // Fail the HD with D being the domain and H having a dot requirement.
  EXPECT_FALSE(sdch_manager_->AddSdchDictionary(dictionary_text,
               GURL("http://w.x.y.z.google.com")));
}

TEST_F(SdchFilterTest, FailToSetRepeatPrefixWithDotDictionary) {
  // Make sure that a prefix that matches the domain postfix won't confuse
  // the validation checks.
  std::string dictionary_domain("www.google.com");
  std::string dictionary_text(NewSdchDictionary(dictionary_domain));

  // Fail the HD with D being the domain and H having a dot requirement.
  EXPECT_FALSE(sdch_manager_->AddSdchDictionary(dictionary_text,
               GURL("http://www.google.com.www.google.com")));
}

TEST_F(SdchFilterTest, CanSetLeadingDotDomainDictionary) {
  // Make sure that a prefix that matches the domain postfix won't confuse
  // the validation checks.
  std::string dictionary_domain(".google.com");
  std::string dictionary_text(NewSdchDictionary(dictionary_domain));

  // Fail the HD with D being the domain and H having a dot requirement.
  EXPECT_TRUE(sdch_manager_->AddSdchDictionary(dictionary_text,
               GURL("http://www.google.com")));
}

// Make sure the order of the tests is not helping us or confusing things.
// See test CanSetExactMatchDictionary above for first try.
TEST_F(SdchFilterTest, CanStillSetExactMatchDictionary) {
  std::string dictionary_domain("x.y.z.google.com");
  std::string dictionary_text(NewSdchDictionary(dictionary_domain));

  // Perfect match should *STILL* work.
  EXPECT_TRUE(sdch_manager_->AddSdchDictionary(dictionary_text,
              GURL("http://" + dictionary_domain)));
}
