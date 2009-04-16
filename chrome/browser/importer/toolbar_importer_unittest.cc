// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include <string>
#include <vector>

#include "chrome/browser/importer/importer.h"
#include "chrome/browser/importer/toolbar_importer.h"
#include "chrome/common/libxml_utils.h"
#include "googleurl/src/gurl.h"


namespace toolbar_importer_unittest {
static const wchar_t* kTitle = L"MyTitle";
static const char* kUrl = "http://www.google.com/";
static const wchar_t* kFolder = L"Google";
static const wchar_t* kFolder2 = L"Homepage";
static const wchar_t* kFolderArray[3] = {L"Google", L"Search", L"Page"};

static const wchar_t* kOtherTitle = L"MyOtherTitle";
static const char* kOtherUrl = "http://www.google.com/mail";
static const wchar_t* kOtherFolder = L"Mail";

// Since the following is very dense to read I enumerate the test cases here.
// 1. Correct bookmark structure with one label.
// 2. Correct bookmark structure with no labels.
// 3. Correct bookmark structure with two labels.
// 4. Correct bookmark structure with a folder->label translation by toolbar.
// 5. Correct bookmark structure with no favicon.
// 6. Two correct bookmarks.
// The following are error cases by removing sections from the xml:
// 7. Empty string passed as xml.
// 8. No <bookmarks> section in the xml.
// 9. No <bookmark> section below the <bookmarks> section.
// 10. No <title> in a <bookmark> section.
// 11. No <url> in a <bookmark> section.
// 12. No <timestamp> in a <bookmark> section.
// 13. No <labels> in a <bookmark> section.
static const char* kGoodBookmark =
    "<?xml version=\"1.0\" ?> <xml_api_reply version=\"1\"> <bookmarks>"
    " <bookmark> "
    "<title>MyTitle</title> "
    "<url>http://www.google.com/</url> "
    "<timestamp>1153328691085181</timestamp> "
    "<id>N123nasdf239</id> <notebook_id>Bxxxxxxx</notebook_id> "
    "<section_id>Sxxxxxx</section_id> <has_highlight>0</has_highlight>"
    "<labels> <label>Google</label> </labels> "
    "<attributes> "
    "<attribute> "
    "<name>favicon_url</name> <value>http://www.google.com/favicon.ico</value> "
    "</attribute> "
    "<attribute> "
    "<name>favicon_timestamp</name> <value>1153328653</value> "
    "</attribute> "
    "<attribute> <name>notebook_name</name> <value>My notebook 0</value> "
    "</attribute> "
    "<attribute> <name>section_name</name> <value>My section 0 "
    "</value> </attribute> </attributes> "
    "</bookmark> </bookmarks>";
static const char* kGoodBookmarkNoLabel =
    "<?xml version=\"1.0\" ?> <xml_api_reply version=\"1\"> <bookmarks>"
    " <bookmark> "
    "<title>MyTitle</title> "
    "<url>http://www.google.com/</url> "
    "<timestamp>1153328691085181</timestamp> "
    "<id>N123nasdf239</id> <notebook_id>Bxxxxxxx</notebook_id> "
    "<section_id>Sxxxxxx</section_id> <has_highlight>0</has_highlight>"
    "<labels> </labels> "
    "<attributes> "
    "<attribute> "
    "<name>favicon_url</name> <value>http://www.google.com/favicon.ico</value> "
    "</attribute> "
    "<attribute> "
    "<name>favicon_timestamp</name> <value>1153328653</value> "
    "</attribute> "
    "<attribute> <name>notebook_name</name> <value>My notebook 0</value> "
    "</attribute> "
    "<attribute> <name>section_name</name> <value>My section 0 "
    "</value> </attribute> </attributes> "
    "</bookmark> </bookmarks>";
static const char* kGoodBookmarkTwoLabels =
    "<?xml version=\"1.0\" ?> <xml_api_reply version=\"1\"> <bookmarks>"
    " <bookmark> "
    "<title>MyTitle</title> "
    "<url>http://www.google.com/</url> "
    "<timestamp>1153328691085181</timestamp> "
    "<id>N123nasdf239</id> <notebook_id>Bxxxxxxx</notebook_id> "
    "<section_id>Sxxxxxx</section_id> <has_highlight>0</has_highlight>"
    "<labels> <label>Google</label> <label>Homepage</label> </labels> "
    "<attributes> "
    "<attribute> "
    "<name>favicon_url</name> <value>http://www.google.com/favicon.ico</value> "
    "</attribute> "
    "<attribute> "
    "<name>favicon_timestamp</name> <value>1153328653</value> "
    "</attribute> "
    "<attribute> <name>notebook_name</name> <value>My notebook 0</value> "
    "</attribute> "
    "<attribute> <name>section_name</name> <value>My section 0 "
    "</value> </attribute> </attributes> "
    "</bookmark> </bookmarks>";
static const char* kGoodBookmarkFolderLabel =
    "<?xml version=\"1.0\" ?> <xml_api_reply version=\"1\"> <bookmarks>"
    " <bookmark> "
    "<title>MyTitle</title> "
    "<url>http://www.google.com/</url> "
    "<timestamp>1153328691085181</timestamp> "
    "<id>N123nasdf239</id> <notebook_id>Bxxxxxxx</notebook_id> "
    "<section_id>Sxxxxxx</section_id> <has_highlight>0</has_highlight>"
    "<labels> <label>Google:Search:Page</label> </labels> "
    "<attributes> "
    "<attribute> "
    "<name>favicon_url</name> <value>http://www.google.com/favicon.ico</value> "
    "</attribute> "
    "<attribute> "
    "<name>favicon_timestamp</name> <value>1153328653</value> "
    "</attribute> "
    "<attribute> <name>notebook_name</name> <value>My notebook 0</value> "
    "</attribute> "
    "<attribute> <name>section_name</name> <value>My section 0 "
    "</value> </attribute> </attributes> "
    "</bookmark> </bookmarks>";
static const char* kGoodBookmarkNoFavicon =
    "<?xml version=\"1.0\" ?> <xml_api_reply version=\"1\"> <bookmarks>"
    " <bookmark> "
    "<title>MyTitle</title> "
    "<url>http://www.google.com/</url> "
    "<timestamp>1153328691085181</timestamp> "
    "<id>N123nasdf239</id> <notebook_id>Bxxxxxxx</notebook_id> "
    "<section_id>Sxxxxxx</section_id> <has_highlight>0</has_highlight>"
    "<labels> <label>Google</label> </labels> "
    "<attributes> "
    "<attribute> "
    "<name>favicon_timestamp</name> <value>1153328653</value> "
    "</attribute> "
    "<attribute> <name>notebook_name</name> <value>My notebook 0</value> "
    "</attribute> "
    "<attribute> <name>section_name</name> <value>My section 0 "
    "</value> </attribute> </attributes> "
    "</bookmark> </bookmarks>";
static const char* kGoodBookmark2Items =
    "<?xml version=\"1.0\" ?> <xml_api_reply version=\"1\"> <bookmarks>"
    " <bookmark> "
    "<title>MyTitle</title> "
    "<url>http://www.google.com/</url> "
    "<timestamp>1153328691085181</timestamp> "
    "<id>N123nasdf239</id> <notebook_id>Bxxxxxxx</notebook_id> "
    "<section_id>Sxxxxxx</section_id> <has_highlight>0</has_highlight>"
    "<labels> <label>Google</label> </labels> "
    "<attributes> "
    "<attribute> "
    "<name>favicon_url</name> <value>http://www.google.com/favicon.ico</value> "
    "</attribute> "
    "<attribute> "
    "<name>favicon_timestamp</name> <value>1153328653</value> "
    "</attribute> "
    "<attribute> <name>notebook_name</name> <value>My notebook 0</value> "
    "</attribute> "
    "<attribute> <name>section_name</name> <value>My section 0 "
    "</value> </attribute> </attributes> "
    "</bookmark>"
    " <bookmark> "
    "<title>MyOtherTitle</title> "
    "<url>http://www.google.com/mail</url> "
    "<timestamp>1153328691085181</timestamp> "
    "<id>N123nasdf239</id> <notebook_id>Bxxxxxxx</notebook_id> "
    "<section_id>Sxxxxxx</section_id> <has_highlight>0</has_highlight>"
    "<labels> <label>Mail</label> </labels> "
    "<attributes> "
    "<attribute> "
    "<name>favicon_url</name>"
    "<value>http://www.google.com/mail/favicon.ico</value> "
    "</attribute> "
    "<attribute> "
    "<name>favicon_timestamp</name> <value>1253328653</value> "
    "</attribute> "
    "<attribute> <name>notebook_name</name> <value>My notebook 0</value> "
    "</attribute> "
    "<attribute> <name>section_name</name> <value>My section 0 "
    "</value> </attribute> </attributes> "
    "</bookmark>"
    "</bookmarks>";
static const char* kEmptyString = "";
static const char* kBadBookmarkNoBookmarks =
    " <bookmark> "
    "<title>MyTitle</title> "
    "<url>http://www.google.com/</url> "
    "<timestamp>1153328691085181</timestamp> "
    "<id>N123nasdf239</id> <notebook_id>Bxxxxxxx</notebook_id> "
    "<section_id>Sxxxxxx</section_id> <has_highlight>0</has_highlight>"
    "<labels> <label>Google</label> </labels> "
    "<attributes> "
    "<attribute> "
    "<name>favicon_url</name> <value>http://www.google.com/favicon.ico</value> "
    "</attribute> "
    "<attribute> "
    "<name>favicon_timestamp</name> <value>1153328653</value> "
    "</attribute> "
    "<attribute> <name>notebook_name</name> <value>My notebook 0</value> "
    "</attribute> "
    "<attribute> <name>section_name</name> <value>My section 0 "
    "</value> </attribute> </attributes> "
    "</bookmark> </bookmarks>";
static const char* kBadBookmarkNoBookmark =
    "<?xml version=\"1.0\" ?> <xml_api_reply version=\"1\"> <bookmarks>"
    "<title>MyTitle</title> "
    "<url>http://www.google.com/</url> "
    "<timestamp>1153328691085181</timestamp> "
    "<id>N123nasdf239</id> <notebook_id>Bxxxxxxx</notebook_id> "
    "<section_id>Sxxxxxx</section_id> <has_highlight>0</has_highlight>"
    "<labels> <label>Google</label> </labels> "
    "<attributes> "
    "<attribute> "
    "<name>favicon_url</name> <value>http://www.google.com/favicon.ico</value> "
    "</attribute> "
    "<attribute> "
    "<name>favicon_timestamp</name> <value>1153328653</value> "
    "</attribute> "
    "<attribute> <name>notebook_name</name> <value>My notebook 0</value> "
    "</attribute> "
    "<attribute> <name>section_name</name> <value>My section 0 "
    "</value> </attribute> </attributes> "
    "</bookmark> </bookmarks>";
static const char* kBadBookmarkNoTitle =
    "<?xml version=\"1.0\" ?> <xml_api_reply version=\"1\"> <bookmarks>"
    " <bookmark> "
    "<url>http://www.google.com/</url> "
    "<timestamp>1153328691085181</timestamp> "
    "<id>N123nasdf239</id> <notebook_id>Bxxxxxxx</notebook_id> "
    "<section_id>Sxxxxxx</section_id> <has_highlight>0</has_highlight>"
    "<labels> <label>Google</label> </labels> "
    "<attributes> "
    "<attribute> "
    "<name>favicon_url</name> <value>http://www.google.com/favicon.ico</value> "
    "</attribute> "
    "<attribute> "
    "<name>favicon_timestamp</name> <value>1153328653</value> "
    "</attribute> "
    "<attribute> <name>notebook_name</name> <value>My notebook 0</value> "
    "</attribute> "
    "<attribute> <name>section_name</name> <value>My section 0 "
    "</value> </attribute> </attributes> "
    "</bookmark> </bookmarks>";
static const char* kBadBookmarkNoUrl =
    "<?xml version=\"1.0\" ?> <xml_api_reply version=\"1\"> <bookmarks>"
    " <bookmark> "
    "<title>MyTitle</title> "
    "<timestamp>1153328691085181</timestamp> "
    "<id>N123nasdf239</id> <notebook_id>Bxxxxxxx</notebook_id> "
    "<section_id>Sxxxxxx</section_id> <has_highlight>0</has_highlight>"
    "<labels> <label>Google</label> </labels> "
    "<attributes> "
    "<attribute> "
    "<name>favicon_url</name> <value>http://www.google.com/favicon.ico</value> "
    "</attribute> "
    "<attribute> "
    "<name>favicon_timestamp</name> <value>1153328653</value> "
    "</attribute> "
    "<attribute> <name>notebook_name</name> <value>My notebook 0</value> "
    "</attribute> "
    "<attribute> <name>section_name</name> <value>My section 0 "
    "</value> </attribute> </attributes> "
    "</bookmark> </bookmarks>";
static const char* kBadBookmarkNoTimestamp =
    "<?xml version=\"1.0\" ?> <xml_api_reply version=\"1\"> <bookmarks>"
    " <bookmark> "
    "<title>MyTitle</title> "
    "<url>http://www.google.com/</url> "
    "<id>N123nasdf239</id> <notebook_id>Bxxxxxxx</notebook_id> "
    "<section_id>Sxxxxxx</section_id> <has_highlight>0</has_highlight>"
    "<labels> <label>Google</label> </labels> "
    "<attributes> "
    "<attribute> "
    "<name>favicon_url</name> <value>http://www.google.com/favicon.ico</value> "
    "</attribute> "
    "<attribute> "
    "<name>favicon_timestamp</name> <value>1153328653</value> "
    "</attribute> "
    "<attribute> <name>notebook_name</name> <value>My notebook 0</value> "
    "</attribute> "
    "<attribute> <name>section_name</name> <value>My section 0 "
    "</value> </attribute> </attributes> "
    "</bookmark> </bookmarks>";
static const char* kBadBookmarkNoLabels =
    "<?xml version=\"1.0\" ?> <xml_api_reply version=\"1\"> <bookmarks>"
    " <bookmark> "
    "<title>MyTitle</title> "
    "<url>http://www.google.com/</url> "
    "<timestamp>1153328691085181</timestamp> "
    "<id>N123nasdf239</id> <notebook_id>Bxxxxxxx</notebook_id> "
    "<section_id>Sxxxxxx</section_id> <has_highlight>0</has_highlight>"
    "<attributes> "
    "<attribute> "
    "<name>favicon_url</name> <value>http://www.google.com/favicon.ico</value> "
    "</attribute> "
    "<attribute> "
    "<name>favicon_timestamp</name> <value>1153328653</value> "
    "</attribute> "
    "<attribute> <name>notebook_name</name> <value>My notebook 0</value> "
    "</attribute> "
    "<attribute> <name>section_name</name> <value>My section 0 "
    "</value> </attribute> </attributes> "
    "</bookmark> </bookmarks>";
}  // namespace toolbar_importer_unittest

// The parsing tests for Toolbar5Importer use the string above.  For a
// description of all the tests run please see the comments immediately before
// the string constants above.
TEST(Toolbar5ImporterTest, BookmarkParse) {
  XmlReader reader;
  std::string bookmark_xml;
  std::vector<ProfileWriter::BookmarkEntry> bookmarks;

  const GURL url(toolbar_importer_unittest::kUrl);
  const GURL other_url(toolbar_importer_unittest::kOtherUrl);

  // Test case 1 is parsing a basic bookmark with a single label.
  bookmark_xml = toolbar_importer_unittest::kGoodBookmark;
  bookmarks.clear();
  EXPECT_TRUE(reader.Load(bookmark_xml));
  EXPECT_TRUE(Toolbar5Importer::ParseBookmarksFromReader(&reader, &bookmarks));

  EXPECT_EQ(bookmarks.size(), (size_t) 1);
  EXPECT_FALSE(bookmarks[0].in_toolbar);
  EXPECT_EQ(toolbar_importer_unittest::kTitle, bookmarks[0].title);
  EXPECT_EQ(url, bookmarks[0].url);
  EXPECT_EQ(bookmarks[0].path.size(), (size_t) 2);
  EXPECT_EQ(toolbar_importer_unittest::kFolder, bookmarks[0].path[1]);

  // Test case 2 is parsing a single bookmark with no label.
  bookmark_xml = toolbar_importer_unittest::kGoodBookmarkNoLabel;
  bookmarks.clear();
  EXPECT_TRUE(reader.Load(bookmark_xml));
  EXPECT_TRUE(Toolbar5Importer::ParseBookmarksFromReader(&reader, &bookmarks));

  EXPECT_EQ(bookmarks.size(), (size_t) 1);
  EXPECT_FALSE(bookmarks[0].in_toolbar);
  EXPECT_EQ(toolbar_importer_unittest::kTitle, bookmarks[0].title);
  EXPECT_EQ(bookmarks[0].url, url);
  EXPECT_EQ(bookmarks[0].path.size(), (size_t) 1);

  // Test case 3 is parsing a single bookmark with two labels.
  bookmark_xml = toolbar_importer_unittest::kGoodBookmarkTwoLabels;
  bookmarks.clear();
  EXPECT_TRUE(reader.Load(bookmark_xml));
  EXPECT_TRUE(Toolbar5Importer::ParseBookmarksFromReader(&reader, &bookmarks));

  EXPECT_EQ(bookmarks.size(), (size_t) 2);
  EXPECT_FALSE(bookmarks[0].in_toolbar);
  EXPECT_FALSE(bookmarks[1].in_toolbar);
  EXPECT_EQ(toolbar_importer_unittest::kTitle, bookmarks[0].title);
  EXPECT_EQ(toolbar_importer_unittest::kTitle, bookmarks[1].title);
  EXPECT_EQ(bookmarks[0].url, url);
  EXPECT_EQ(bookmarks[1].url, url);
  EXPECT_EQ(toolbar_importer_unittest::kFolder, bookmarks[0].path[1]);
  EXPECT_EQ(toolbar_importer_unittest::kFolder2, bookmarks[1].path[1]);

  // Test case 4 is parsing a single bookmark which has a label with a colon,
  // this test file name translation between Toolbar and Chrome.
  bookmark_xml = toolbar_importer_unittest::kGoodBookmarkFolderLabel;
  bookmarks.clear();
  EXPECT_TRUE(reader.Load(bookmark_xml));
  EXPECT_TRUE(Toolbar5Importer::ParseBookmarksFromReader(&reader, &bookmarks));

  EXPECT_EQ(bookmarks.size(), (size_t) 1);
  EXPECT_FALSE(bookmarks[0].in_toolbar);
  EXPECT_EQ(toolbar_importer_unittest::kTitle, bookmarks[0].title);
  EXPECT_EQ(bookmarks[0].url, url);
  EXPECT_EQ(bookmarks[0].path.size(), (size_t) 4);
  EXPECT_TRUE(toolbar_importer_unittest::kFolderArray[0] ==
      bookmarks[0].path[1]);
  EXPECT_TRUE(toolbar_importer_unittest::kFolderArray[1] ==
      bookmarks[0].path[2]);
  EXPECT_TRUE(toolbar_importer_unittest::kFolderArray[2] ==
      bookmarks[0].path[3]);

  // Test case 5 is parsing a single bookmark without a favicon URL.
  bookmark_xml = toolbar_importer_unittest::kGoodBookmarkNoFavicon;
  bookmarks.clear();
  EXPECT_TRUE(reader.Load(bookmark_xml));
  EXPECT_TRUE(Toolbar5Importer::ParseBookmarksFromReader(&reader, &bookmarks));

  EXPECT_EQ(bookmarks.size(), (size_t) 1);
  EXPECT_FALSE(bookmarks[0].in_toolbar);
  EXPECT_EQ(toolbar_importer_unittest::kTitle, bookmarks[0].title);
  EXPECT_EQ(bookmarks[0].url, url);
  EXPECT_EQ(bookmarks[0].path.size(), (size_t) 2);
  EXPECT_EQ(toolbar_importer_unittest::kFolder, bookmarks[0].path[1]);

  // Test case 6 is parsing two bookmarks.
  bookmark_xml = toolbar_importer_unittest::kGoodBookmark2Items;
  bookmarks.clear();
  EXPECT_TRUE(reader.Load(bookmark_xml));
  EXPECT_TRUE(Toolbar5Importer::ParseBookmarksFromReader(&reader, &bookmarks));

  EXPECT_EQ(bookmarks.size(), (size_t) 2);
  EXPECT_FALSE(bookmarks[0].in_toolbar);
  EXPECT_FALSE(bookmarks[1].in_toolbar);
  EXPECT_EQ(toolbar_importer_unittest::kTitle, bookmarks[0].title);
  EXPECT_EQ(toolbar_importer_unittest::kOtherTitle, bookmarks[1].title);
  EXPECT_EQ(bookmarks[0].url, url);
  EXPECT_EQ(bookmarks[1].url, other_url);
  EXPECT_EQ(bookmarks[0].path.size(), (size_t) 2);
  EXPECT_EQ(toolbar_importer_unittest::kFolder, bookmarks[0].path[1]);
  EXPECT_EQ(bookmarks[0].path.size(), (size_t) 2);
  EXPECT_EQ(toolbar_importer_unittest::kOtherFolder, bookmarks[1].path[1]);

  // Test case 7 is parsing an empty string for bookmarks.
  bookmark_xml = toolbar_importer_unittest::kEmptyString;
  bookmarks.clear();
  EXPECT_FALSE(reader.Load(bookmark_xml));

  // Test case 8 is testing the error when no <bookmarks> section is present.
  bookmark_xml = toolbar_importer_unittest::kBadBookmarkNoBookmarks;
  bookmarks.clear();
  EXPECT_TRUE(reader.Load(bookmark_xml));
  EXPECT_FALSE(Toolbar5Importer::ParseBookmarksFromReader(&reader, &bookmarks));

  // Test case 9 tests when no <bookmark> section is present.
  bookmark_xml = toolbar_importer_unittest::kBadBookmarkNoBookmark;
  bookmarks.clear();
  EXPECT_TRUE(reader.Load(bookmark_xml));
  EXPECT_FALSE(Toolbar5Importer::ParseBookmarksFromReader(&reader, &bookmarks));


  // Test case 10 tests when a bookmark has no <title> section.
  bookmark_xml = toolbar_importer_unittest::kBadBookmarkNoTitle;
  bookmarks.clear();
  EXPECT_TRUE(reader.Load(bookmark_xml));
  EXPECT_FALSE(Toolbar5Importer::ParseBookmarksFromReader(&reader, &bookmarks));


  // Test case 11 tests when a bookmark has no <url> section.
  bookmark_xml = toolbar_importer_unittest::kBadBookmarkNoUrl;
  bookmarks.clear();
  EXPECT_TRUE(reader.Load(bookmark_xml));
  EXPECT_FALSE(Toolbar5Importer::ParseBookmarksFromReader(&reader, &bookmarks));

  // Test case 12 tests when a bookmark has no <timestamp> section.
  bookmark_xml = toolbar_importer_unittest::kBadBookmarkNoTimestamp;
  bookmarks.clear();
  EXPECT_TRUE(reader.Load(bookmark_xml));
  EXPECT_FALSE(Toolbar5Importer::ParseBookmarksFromReader(&reader, &bookmarks));

  // Test case 13 tests when a bookmark has no <labels> section.
  bookmark_xml = toolbar_importer_unittest::kBadBookmarkNoLabels;
  bookmarks.clear();
  EXPECT_TRUE(reader.Load(bookmark_xml));
  EXPECT_FALSE(Toolbar5Importer::ParseBookmarksFromReader(&reader, &bookmarks));
}
