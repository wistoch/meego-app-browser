// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
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

#include <windows.h>

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "chrome/browser/history/thumbnail_database.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/jpeg_codec.h"
#include "chrome/common/thumbnail_score.h"
#include "chrome/tools/profiles/thumbnail-inl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "SkBitmap.h"

namespace history {

namespace {

class ThumbnailDatabaseTest : public testing::Test {
 public:
  ThumbnailDatabaseTest() {
  }
  ~ThumbnailDatabaseTest() {
  }

 protected:
  // testing::Test
  virtual void SetUp() {
    // get an empty file for the test DB
    PathService::Get(chrome::DIR_TEST_DATA, &file_name_);
    file_name_.push_back(file_util::kPathSeparator);
    file_name_.append(L"TestThumbnails.db");
    DeleteFile(file_name_.c_str());

    google_bitmap_.reset(
        JPEGCodec::Decode(kGoogleThumbnail, sizeof(kGoogleThumbnail)));
  }

  virtual void TearDown() {
    DeleteFile(file_name_.c_str());
  }

  scoped_ptr<SkBitmap> google_bitmap_;

  std::wstring file_name_;
};

// data we'll put into the thumbnail database
static const unsigned char blob1[] =
    "12346102356120394751634516591348710478123649165419234519234512349134";
static const unsigned char blob2[] =
    "goiwuegrqrcomizqyzkjalitbahxfjytrqvpqeroicxmnlkhlzunacxaneviawrtxcywhgef";
static const unsigned char blob3[] =
    "3716871354098370776510470746794707624107647054607467847164027";
const double kBoringness = 0.25;
const double kWorseBoringness = 0.50;
const double kBetterBoringness = 0.10;
const double kTotallyBoring = 1.0;

const __int64 kPage1 = 1234;

// converts out constant data above to a vector for SetPageThumbnail
static std::vector<unsigned char> StringToVector(const unsigned char* str) {
  size_t len = strlen(reinterpret_cast<const char*>(str));
  std::vector<unsigned char> vect;
  vect.resize(len);

  memcpy(&vect[0], str, len);
  return vect;
}

}  // namespace


TEST_F(ThumbnailDatabaseTest, AddDelete) {
  ThumbnailDatabase db;
  ASSERT_TRUE(db.Init(file_name_) == INIT_OK);

  // Add one page & verify it got added.
  ThumbnailScore boring(kBoringness, true, true);
  db.SetPageThumbnail(kPage1, *google_bitmap_, boring);
  ThumbnailScore score_output;
  ASSERT_TRUE(db.ThumbnailScoreForId(kPage1, &score_output));
  ASSERT_TRUE(boring.Equals(score_output));

  // Verify a random page is not found.
  __int64 page2 = 5678;
  std::vector<unsigned char> jpeg_data;
  EXPECT_FALSE(db.GetPageThumbnail(page2, &jpeg_data));
  EXPECT_FALSE(db.ThumbnailScoreForId(page2, &score_output));

  // Add another page with a better boringness & verify it got added.
  ThumbnailScore better_boringness(kBetterBoringness, true, true);
  db.SetPageThumbnail(page2, *google_bitmap_, better_boringness);
  ASSERT_TRUE(db.ThumbnailScoreForId(page2, &score_output));
  ASSERT_TRUE(better_boringness.Equals(score_output));

  // Delete the thumbnail for the second page.
  ThumbnailScore worse_boringness(kWorseBoringness, true, true);
  db.SetPageThumbnail(page2, SkBitmap(), worse_boringness);
  ASSERT_FALSE(db.GetPageThumbnail(page2, &jpeg_data));
  ASSERT_FALSE(db.ThumbnailScoreForId(page2, &score_output));

  // Delete the first thumbnail using the explicit delete API.
  ASSERT_TRUE(db.DeleteThumbnail(kPage1));

  // Make sure it is gone
  ASSERT_FALSE(db.ThumbnailScoreForId(kPage1, &score_output));
  ASSERT_FALSE(db.GetPageThumbnail(kPage1, &jpeg_data));
  ASSERT_FALSE(db.ThumbnailScoreForId(page2, &score_output));
  ASSERT_FALSE(db.GetPageThumbnail(page2, &jpeg_data));
}

TEST_F(ThumbnailDatabaseTest, UseLessBoringThumbnails) {
  ThumbnailDatabase db;
  Time now = Time::Now();
  ASSERT_TRUE(db.Init(file_name_) == INIT_OK);

  // Add one page & verify it got added.
  ThumbnailScore boring(kBoringness, true, true);
  db.SetPageThumbnail(kPage1, *google_bitmap_, boring);
  std::vector<unsigned char> jpeg_data;
  ThumbnailScore score_out;
  ASSERT_TRUE(db.GetPageThumbnail(kPage1, &jpeg_data));
  ASSERT_TRUE(db.ThumbnailScoreForId(kPage1, &score_out));
  ASSERT_TRUE(boring.Equals(score_out));

  // Attempt to update the first page entry with a thumbnail that
  // is more boring and verify that it doesn't change.
  ThumbnailScore more_boring(kWorseBoringness, true, true);
  db.SetPageThumbnail(kPage1, *google_bitmap_, more_boring);
  ASSERT_TRUE(db.GetPageThumbnail(kPage1, &jpeg_data));
  ASSERT_TRUE(db.ThumbnailScoreForId(kPage1, &score_out));
  ASSERT_TRUE(boring.Equals(score_out));

  // Attempt to update the first page entry with a thumbnail that
  // is less boring and verify that we update it.
  ThumbnailScore less_boring(kBetterBoringness, true, true);
  db.SetPageThumbnail(kPage1, *google_bitmap_, less_boring);
  ASSERT_TRUE(db.GetPageThumbnail(kPage1, &jpeg_data));
  ASSERT_TRUE(db.ThumbnailScoreForId(kPage1, &score_out));
  ASSERT_TRUE(less_boring.Equals(score_out));
}

TEST_F(ThumbnailDatabaseTest, UseAtTopThumbnails) {
  ThumbnailDatabase db;
  Time now = Time::Now();
  ASSERT_TRUE(db.Init(file_name_) == INIT_OK);

  // Add one page & verify it got added. Note that it doesn't have
  // |good_clipping| and isn't |at_top|.
  ThumbnailScore boring_and_bad(kBoringness, false, false);
  db.SetPageThumbnail(kPage1, *google_bitmap_, boring_and_bad);
  std::vector<unsigned char> jpeg_data;
  ThumbnailScore score_out;
  ASSERT_TRUE(db.GetPageThumbnail(kPage1, &jpeg_data));
  ASSERT_TRUE(db.ThumbnailScoreForId(kPage1, &score_out));
  ASSERT_TRUE(boring_and_bad.Equals(score_out));

  // A thumbnail that's at the top of the page should replace
  // thumbnails that are in the middle, for the same boringness.
  ThumbnailScore boring_but_better(kBoringness, false, true);
  db.SetPageThumbnail(kPage1, *google_bitmap_, boring_but_better);
  ASSERT_TRUE(db.GetPageThumbnail(kPage1, &jpeg_data));
  ASSERT_TRUE(db.ThumbnailScoreForId(kPage1, &score_out));
  ASSERT_TRUE(boring_but_better.Equals(score_out));

  // The only case where we should replace a thumbnail at the top with
  // a thumbnail in the middle/bottom is when the current thumbnail is
  // weirdly stretched and the incoming thumbnail isn't.
  ThumbnailScore better_boring_bad_framing(kBetterBoringness, false, false);
  db.SetPageThumbnail(kPage1, *google_bitmap_, better_boring_bad_framing);
  ASSERT_TRUE(db.GetPageThumbnail(kPage1, &jpeg_data));
  ASSERT_TRUE(db.ThumbnailScoreForId(kPage1, &score_out));
  ASSERT_TRUE(boring_but_better.Equals(score_out));

  ThumbnailScore boring_good_clipping(kBoringness, true, false);
  db.SetPageThumbnail(kPage1, *google_bitmap_, boring_good_clipping);
  ASSERT_TRUE(db.GetPageThumbnail(kPage1, &jpeg_data));
  ASSERT_TRUE(db.ThumbnailScoreForId(kPage1, &score_out));
  ASSERT_TRUE(boring_good_clipping.Equals(score_out));

  // Now that we have a non-stretched, middle of the page thumbnail,
  // we shouldn't be able to replace it with:

  // 1) A stretched thumbnail in the middle of the page
  db.SetPageThumbnail(kPage1, *google_bitmap_,
                      ThumbnailScore(kBetterBoringness, false, false, now));
  ASSERT_TRUE(db.GetPageThumbnail(kPage1, &jpeg_data));
  ASSERT_TRUE(db.ThumbnailScoreForId(kPage1, &score_out));
  ASSERT_TRUE(boring_good_clipping.Equals(score_out));

  // 2) A stretched thumbnail at the top of the page
  db.SetPageThumbnail(kPage1, *google_bitmap_,
                      ThumbnailScore(kBetterBoringness, false, true, now));
  ASSERT_TRUE(db.GetPageThumbnail(kPage1, &jpeg_data));
  ASSERT_TRUE(db.ThumbnailScoreForId(kPage1, &score_out));
  ASSERT_TRUE(boring_good_clipping.Equals(score_out));

  // But it should be replaced by a thumbnail that's clipped properly
  // and is at the top
  ThumbnailScore best_score(kBetterBoringness, true, true);
  db.SetPageThumbnail(kPage1, *google_bitmap_, best_score);
  ASSERT_TRUE(db.GetPageThumbnail(kPage1, &jpeg_data));
  ASSERT_TRUE(db.ThumbnailScoreForId(kPage1, &score_out));
  ASSERT_TRUE(best_score.Equals(score_out));
}

TEST_F(ThumbnailDatabaseTest, ThumbnailTimeDegradation) {
  ThumbnailDatabase db;
  const Time kNow = Time::Now();
  const Time kThreeHoursAgo = kNow - TimeDelta::FromHours(4);
  const Time kFiveHoursAgo = kNow - TimeDelta::FromHours(6);
  const double kBaseBoringness = 0.305;
  const double kWorseBoringness = 0.345;

  ASSERT_TRUE(db.Init(file_name_) == INIT_OK);

  // add one page & verify it got added.
  ThumbnailScore base_boringness(kBaseBoringness, true, true, kFiveHoursAgo);
  db.SetPageThumbnail(kPage1, *google_bitmap_, base_boringness);
  std::vector<unsigned char> jpeg_data;
  ThumbnailScore score_out;
  ASSERT_TRUE(db.GetPageThumbnail(kPage1, &jpeg_data));
  ASSERT_TRUE(db.ThumbnailScoreForId(kPage1, &score_out));
  ASSERT_TRUE(base_boringness.Equals(score_out));

  // Try to add a different thumbnail with a worse score an hour later
  // (but not enough to trip the boringness degradation threshold).
  ThumbnailScore hour_later(kWorseBoringness, true, true, kThreeHoursAgo);
  db.SetPageThumbnail(kPage1, *google_bitmap_, hour_later);
  ASSERT_TRUE(db.GetPageThumbnail(kPage1, &jpeg_data));
  ASSERT_TRUE(db.ThumbnailScoreForId(kPage1, &score_out));
  ASSERT_TRUE(base_boringness.Equals(score_out));

  // After a full five hours, things should have degraded enough
  // that we'll allow the same thumbnail with the same (worse)
  // boringness that we previous rejected.
  ThumbnailScore five_hours_later(kWorseBoringness, true, true, kNow);
  db.SetPageThumbnail(kPage1, *google_bitmap_, five_hours_later);
  ASSERT_TRUE(db.GetPageThumbnail(kPage1, &jpeg_data));
  ASSERT_TRUE(db.ThumbnailScoreForId(kPage1, &score_out));
  ASSERT_TRUE(five_hours_later.Equals(score_out));
}

TEST_F(ThumbnailDatabaseTest, NeverAcceptTotallyBoringThumbnail) {
  // We enforce a maximum boringness score: even in cases where we
  // should replace a thumbnail with another because of reasons other
  // than straight up boringness score, still reject because the
  // thumbnail is totally boring.
  ThumbnailDatabase db;
  Time now = Time::Now();
  ASSERT_TRUE(db.Init(file_name_) == INIT_OK);

  std::vector<unsigned char> jpeg_data;
  ThumbnailScore score_out;
  const double kBaseBoringness = 0.50;
  const Time kNow = Time::Now();
  const int kSizeOfTable = 4;
  struct {
    bool good_scaling;
    bool at_top;
  } const heiarchy_table[] = {
    {false, false},
    {false, true},
    {true, false},
    {true, true}
  };

  // Test that for each entry type, all entry types that are better
  // than it still will reject thumbnails which are totally boring.
  for (int i = 0; i < kSizeOfTable; ++i) {
    ThumbnailScore base(kBaseBoringness,
                        heiarchy_table[i].good_scaling,
                        heiarchy_table[i].at_top,
                        kNow);

    db.SetPageThumbnail(kPage1, *google_bitmap_, base);
    ASSERT_TRUE(db.GetPageThumbnail(kPage1, &jpeg_data));
    ASSERT_TRUE(db.ThumbnailScoreForId(kPage1, &score_out));
    ASSERT_TRUE(base.Equals(score_out));

    for (int j = i; j < kSizeOfTable; ++j) {
      ThumbnailScore shouldnt_replace(
          kTotallyBoring, heiarchy_table[j].good_scaling,
          heiarchy_table[j].at_top, kNow);

      db.SetPageThumbnail(kPage1, *google_bitmap_, shouldnt_replace);
      ASSERT_TRUE(db.GetPageThumbnail(kPage1, &jpeg_data));
      ASSERT_TRUE(db.ThumbnailScoreForId(kPage1, &score_out));
      ASSERT_TRUE(base.Equals(score_out));
    }

    // Clean up for the next iteration
    ASSERT_TRUE(db.DeleteThumbnail(kPage1));
    ASSERT_FALSE(db.GetPageThumbnail(kPage1, &jpeg_data));
    ASSERT_FALSE(db.ThumbnailScoreForId(kPage1, &score_out));
  }

  // We should never accept a totally boring thumbnail no matter how
  // much old the current thumbnail is.
  ThumbnailScore base_boring(kBaseBoringness, true, true, kNow);
  db.SetPageThumbnail(kPage1, *google_bitmap_, base_boring);
  ASSERT_TRUE(db.GetPageThumbnail(kPage1, &jpeg_data));
  ASSERT_TRUE(db.ThumbnailScoreForId(kPage1, &score_out));
  ASSERT_TRUE(base_boring.Equals(score_out));

  ThumbnailScore totally_boring_in_the_future(
      kTotallyBoring, true, true, kNow + TimeDelta::FromDays(365));
  db.SetPageThumbnail(kPage1, *google_bitmap_, totally_boring_in_the_future);
  ASSERT_TRUE(db.GetPageThumbnail(kPage1, &jpeg_data));
  ASSERT_TRUE(db.ThumbnailScoreForId(kPage1, &score_out));
  ASSERT_TRUE(base_boring.Equals(score_out));
}

}  // namespace history
