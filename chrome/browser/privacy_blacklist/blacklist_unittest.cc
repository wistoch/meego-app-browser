// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_blacklist/blacklist.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/browser/privacy_blacklist/blacklist_io.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(BlacklistTest, Generic) {
  // Get path relative to test data dir.
  FilePath input;
  PathService::Get(chrome::DIR_TEST_DATA, &input);
  input = input.AppendASCII("blacklist_small.pbr");

  Blacklist blacklist;
  ASSERT_TRUE(BlacklistIO::ReadBinary(&blacklist, input));

  Blacklist::EntryList entries(blacklist.entries_begin(),
                               blacklist.entries_end());

  ASSERT_EQ(9U, entries.size());

  EXPECT_EQ(Blacklist::kBlockByType|Blacklist::kDontPersistCookies,
            entries[0]->attributes());
  EXPECT_TRUE(entries[0]->MatchesType("application/x-shockwave-flash"));
  EXPECT_FALSE(entries[0]->MatchesType("image/jpeg"));
  EXPECT_FALSE(entries[0]->is_exception());
  EXPECT_EQ("@", entries[0]->pattern());

  // All entries include global attributes.
  // NOTE: Silly bitwise-or with zero to workaround a Mac compiler bug.
  EXPECT_EQ(Blacklist::kBlockUnsecure|0, entries[1]->attributes());
  EXPECT_FALSE(entries[1]->MatchesType("application/x-shockwave-flash"));
  EXPECT_FALSE(entries[1]->MatchesType("image/jpeg"));
  EXPECT_FALSE(entries[1]->is_exception());
  EXPECT_EQ("@poor-security-site.com", entries[1]->pattern());

  EXPECT_EQ(Blacklist::kDontSendCookies|Blacklist::kDontStoreCookies,
            entries[2]->attributes());
  EXPECT_FALSE(entries[2]->MatchesType("application/x-shockwave-flash"));
  EXPECT_FALSE(entries[2]->MatchesType("image/jpeg"));
  EXPECT_FALSE(entries[2]->is_exception());
  EXPECT_EQ("@.ad-serving-place.com", entries[2]->pattern());

  EXPECT_EQ(Blacklist::kDontSendUserAgent|Blacklist::kDontSendReferrer,
            entries[3]->attributes());
  EXPECT_FALSE(entries[3]->MatchesType("application/x-shockwave-flash"));
  EXPECT_FALSE(entries[3]->MatchesType("image/jpeg"));
  EXPECT_FALSE(entries[3]->is_exception());
  EXPECT_EQ("www.site.com/anonymous/folder/@", entries[3]->pattern());

  // NOTE: Silly bitwise-or with zero to workaround a Mac compiler bug.
  EXPECT_EQ(Blacklist::kBlockAll|0, entries[4]->attributes());
  EXPECT_FALSE(entries[4]->MatchesType("application/x-shockwave-flash"));
  EXPECT_FALSE(entries[4]->MatchesType("image/jpeg"));
  EXPECT_FALSE(entries[4]->is_exception());
  EXPECT_EQ("www.site.com/bad/url", entries[4]->pattern());

  // NOTE: Silly bitwise-or with zero to workaround a Mac compiler bug.
  EXPECT_EQ(Blacklist::kBlockAll|0, entries[5]->attributes());
  EXPECT_FALSE(entries[5]->MatchesType("application/x-shockwave-flash"));
  EXPECT_FALSE(entries[5]->MatchesType("image/jpeg"));
  EXPECT_FALSE(entries[5]->is_exception());
  EXPECT_EQ("@/script?@", entries[5]->pattern());

  // NOTE: Silly bitwise-or with zero to workaround a Mac compiler bug.
  EXPECT_EQ(Blacklist::kBlockAll|0, entries[6]->attributes());
  EXPECT_FALSE(entries[6]->MatchesType("application/x-shockwave-flash"));
  EXPECT_FALSE(entries[6]->MatchesType("image/jpeg"));
  EXPECT_FALSE(entries[6]->is_exception());
  EXPECT_EQ("@?badparam@", entries[6]->pattern());

  // NOTE: Silly bitwise-or with zero to workaround a Mac compiler bug.
  EXPECT_EQ(Blacklist::kBlockAll|0, entries[7]->attributes());
  EXPECT_FALSE(entries[7]->MatchesType("application/x-shockwave-flash"));
  EXPECT_FALSE(entries[7]->MatchesType("image/jpeg"));
  EXPECT_TRUE(entries[7]->is_exception());
  EXPECT_EQ("www.site.com/bad/url/good", entries[7]->pattern());

  // NOTE: Silly bitwise-or with zero to workaround a Mac compiler bug.
  EXPECT_EQ(Blacklist::kBlockByType|Blacklist::kDontPersistCookies,
            entries[8]->attributes());
  EXPECT_TRUE(entries[8]->MatchesType("application/x-shockwave-flash"));
  EXPECT_FALSE(entries[8]->MatchesType("image/jpeg"));
  EXPECT_TRUE(entries[8]->is_exception());
  EXPECT_EQ("www.good.com", entries[8]->pattern());

  Blacklist::ProviderList providers(blacklist.providers_begin(),
                                    blacklist.providers_end());

  ASSERT_EQ(1U, providers.size());
  EXPECT_EQ("Sample", providers[0]->name());
  EXPECT_EQ("http://www.google.com", providers[0]->url());

  // No match for chrome, about or empty URLs.
  EXPECT_FALSE(blacklist.FindMatch(GURL()));
  EXPECT_FALSE(blacklist.FindMatch(GURL("chrome://new-tab")));
  EXPECT_FALSE(blacklist.FindMatch(GURL("about:blank")));

  // Expected rule matches.
  Blacklist::Match* match;
  match = blacklist.FindMatch(GURL("http://www.google.com"));
  EXPECT_TRUE(match);
  if (match) {
    EXPECT_EQ(Blacklist::kBlockByType|Blacklist::kDontPersistCookies,
              match->attributes());
    EXPECT_EQ(1U, match->entries().size());
    EXPECT_TRUE(match->MatchType("application/x-shockwave-flash"));
    delete match;
  }

  match = blacklist.FindMatch(GURL("http://www.site.com/bad/url"));
  EXPECT_TRUE(match);
  if (match) {
    EXPECT_EQ(Blacklist::kBlockAll|
              Blacklist::kBlockByType|Blacklist::kDontPersistCookies,
              match->attributes());
    EXPECT_EQ(2U, match->entries().size());
    delete match;
  }

  match = blacklist.FindMatch(GURL("http://www.site.com/anonymous"));
  EXPECT_TRUE(match);
  if (match) {
    EXPECT_EQ(Blacklist::kBlockByType|Blacklist::kDontPersistCookies,
              match->attributes());
    EXPECT_EQ(1U, match->entries().size());
    delete match;
  }

  match = blacklist.FindMatch(GURL("http://www.site.com/anonymous/folder"));
  EXPECT_TRUE(match);
  if (match) {
    EXPECT_EQ(Blacklist::kBlockByType|Blacklist::kDontPersistCookies,
      match->attributes());
    EXPECT_EQ(1U, match->entries().size());
    delete match;
  }

  match = blacklist.FindMatch(
      GURL("http://www.site.com/anonymous/folder/subfolder"));
  EXPECT_TRUE(match);
  if (match) {
    EXPECT_EQ(Blacklist::kDontSendUserAgent|Blacklist::kDontSendReferrer|
              Blacklist::kBlockByType|Blacklist::kDontPersistCookies,
              match->attributes());
    EXPECT_EQ(2U, match->entries().size());
    delete match;
  }

  // No matches for URLs without query string
  match = blacklist.FindMatch(GURL("http://badparam.com/"));
  EXPECT_TRUE(match);
  if (match) {
    EXPECT_EQ(1U, match->entries().size());
    EXPECT_EQ(Blacklist::kBlockByType|Blacklist::kDontPersistCookies,
              match->attributes());
    delete match;
  }

  match = blacklist.FindMatch(GURL("http://script.bad.org/"));
  EXPECT_TRUE(match);
  if (match) {
    EXPECT_EQ(1U, match->entries().size());
    EXPECT_EQ(Blacklist::kBlockByType|Blacklist::kDontPersistCookies,
              match->attributes());
    delete match;
  }

  // Expected rule matches.
  match = blacklist.FindMatch(GURL("http://host.com/script?q=x"));
  EXPECT_TRUE(match);
  if (match) {
    EXPECT_EQ(2U, match->entries().size());
    EXPECT_EQ(Blacklist::kBlockByType|Blacklist::kDontPersistCookies|
              Blacklist::kBlockAll,
              match->attributes());
    delete match;
  }

  match = blacklist.FindMatch(GURL("http://host.com/img?badparam=x"));
  EXPECT_TRUE(match);
  if (match) {
    EXPECT_EQ(2U, match->entries().size());
    EXPECT_EQ(Blacklist::kBlockByType|Blacklist::kDontPersistCookies|
              Blacklist::kBlockAll,
              match->attributes());
    delete match;
  }

  // Whitelisting tests.
  match = blacklist.FindMatch(GURL("http://www.site.com/bad/url/good"));
  EXPECT_TRUE(match);
  if (match) {
    EXPECT_EQ(2U, match->entries().size());
    EXPECT_EQ(Blacklist::kBlockByType|Blacklist::kDontPersistCookies,
              match->attributes());
    delete match;
  }

  match = blacklist.FindMatch(GURL("http://www.good.com"));
  EXPECT_TRUE(match);
  if (match) {
    EXPECT_EQ(1U, match->entries().size());
    EXPECT_EQ(0U, match->attributes());
    EXPECT_FALSE(match->MatchType("application/x-shockwave-flash"));
    delete match;
  }

  // StripCookieExpiry Tests
  std::string cookie1(
      "PREF=ID=14a549990453e42a:TM=1245183232:LM=1245183232:S=Occ7khRVIEE36Ao5;"
      " expires=Thu, 16-Jun-2011 20:13:52 GMT; path=/; domain=.google.com");
  std::string cookie2(
      "PREF=ID=14a549990453e42a:TM=1245183232:LM=1245183232:S=Occ7khRVIEE36Ao5;"
      " path=/; domain=.google.com");
  std::string cookie3(
    "PREF=ID=14a549990453e42a:TM=1245183232:LM=1245183232:S=Occ7khRVIEE36Ao5;"
    " expires=Thu, 17-Jun-2011 02:13:52 GMT; path=/; domain=.google.com");
  std::string cookie4("E=MC^2; path=relative;  expires=never;");
  std::string cookie5("E=MC^2; path=relative;");

  // No expiry, should be equal to itself after stripping.
  EXPECT_EQ(cookie2, Blacklist::StripCookieExpiry(cookie2));
  EXPECT_EQ(cookie5, Blacklist::StripCookieExpiry(cookie5));

  // Expiry, should be equal to non-expiry version after stripping.
  EXPECT_EQ(cookie2, Blacklist::StripCookieExpiry(cookie1));
  EXPECT_EQ(cookie5, Blacklist::StripCookieExpiry(cookie4));

  // Same cookie other than expiry should be same after stripping.
  EXPECT_EQ(Blacklist::StripCookieExpiry(cookie2),
            Blacklist::StripCookieExpiry(cookie3));

  // Edge cases.
  std::string invalid("#$%^&*()_+");
  EXPECT_EQ(invalid, Blacklist::StripCookieExpiry(invalid));
  EXPECT_EQ(std::string(), Blacklist::StripCookieExpiry(std::string()));

  // StripCookies Test. Note that "\r\n" line terminators are used
  // because the underlying net util uniformizes those when stripping
  // headers.
  std::string header1("Host: www.example.com\r\n");
  std::string header2("Upgrade: TLS/1.0, HTTP/1.1\r\n"
                      "Connection: Upgrade\r\n");
  std::string header3("Date: Mon, 12 Mar 2001 19:20:33 GMT\r\n"
                      "Expires: Mon, 12 Mar 2001 19:20:33 GMT\r\n"
                      "Content-Type: text/html\r\n"
                      "Set-Cookie: B=460soc0taq8c1&b=2; "
                      "expires=Thu, 15 Apr 2010 20:00:00 GMT; path=/;\r\n");
  std::string header4("Date: Mon, 12 Mar 2001 19:20:33 GMT\r\n"
                      "Expires: Mon, 12 Mar 2001 19:20:33 GMT\r\n"
                      "Content-Type: text/html\r\n");

  EXPECT_TRUE(header1 == Blacklist::StripCookies(header1));
  EXPECT_TRUE(header2 == Blacklist::StripCookies(header2));
  EXPECT_TRUE(header4 == Blacklist::StripCookies(header3));

  // GetURLAsLookupString Test.
  std::string url_spec1("example.com/some/path");
  std::string url_spec2("example.com/script?param=1");

  EXPECT_TRUE(url_spec1 == Blacklist::GetURLAsLookupString(
              GURL("http://example.com/some/path")));
  EXPECT_TRUE(url_spec1 == Blacklist::GetURLAsLookupString(
              GURL("ftp://example.com/some/path")));
  EXPECT_TRUE(url_spec1 == Blacklist::GetURLAsLookupString(
              GURL("http://example.com:8080/some/path")));
  EXPECT_TRUE(url_spec1 == Blacklist::GetURLAsLookupString(
              GURL("http://user:login@example.com/some/path")));
  EXPECT_TRUE(url_spec2 == Blacklist::GetURLAsLookupString(
              GURL("http://example.com/script?param=1")));
}

TEST(BlacklistTest, PatternMatch) {
  // @ matches all but empty strings.
  EXPECT_TRUE(Blacklist::Matches("@", "foo.com"));
  EXPECT_TRUE(Blacklist::Matches("@", "path"));
  EXPECT_TRUE(Blacklist::Matches("@", "foo.com/path"));
  EXPECT_TRUE(Blacklist::Matches("@", "x"));
  EXPECT_FALSE(Blacklist::Matches("@", ""));
  EXPECT_FALSE(Blacklist::Matches("@", std::string()));

  // Prefix match.
  EXPECT_TRUE(Blacklist::Matches("prefix@", "prefix.com"));
  EXPECT_TRUE(Blacklist::Matches("prefix@", "prefix.com/path"));
  EXPECT_TRUE(Blacklist::Matches("prefix@", "prefix/path"));
  EXPECT_TRUE(Blacklist::Matches("prefix@", "prefix/prefix"));
  EXPECT_FALSE(Blacklist::Matches("prefix@", "prefix"));
  EXPECT_FALSE(Blacklist::Matches("prefix@", "Xprefix"));
  EXPECT_FALSE(Blacklist::Matches("prefix@", "Y.Xprefix"));
  EXPECT_FALSE(Blacklist::Matches("prefix@", "Y/Xprefix"));

  // Postfix match.
  EXPECT_TRUE(Blacklist::Matches("@postfix", "something.postfix"));
  EXPECT_TRUE(Blacklist::Matches("@postfix", "something/postfix"));
  EXPECT_TRUE(Blacklist::Matches("@postfix", "foo.com/something/postfix"));
  EXPECT_FALSE(Blacklist::Matches("@postfix", "postfix"));
  EXPECT_FALSE(Blacklist::Matches("@postfix", "postfixZ"));
  EXPECT_FALSE(Blacklist::Matches("@postfix", "postfixZ.Y"));

  // Infix matches.
  EXPECT_TRUE(Blacklist::Matches("@evil@", "www.evil.com"));
  EXPECT_TRUE(Blacklist::Matches("@evil@", "www.evil.com/whatever"));
  EXPECT_TRUE(Blacklist::Matches("@evil@", "www.whatever.com/evilpath"));
  EXPECT_TRUE(Blacklist::Matches("@evil@", "www.evil.whatever.com"));
  EXPECT_FALSE(Blacklist::Matches("@evil@", "evil"));
  EXPECT_FALSE(Blacklist::Matches("@evil@", "evil/"));
  EXPECT_FALSE(Blacklist::Matches("@evil@", "/evil"));

  // Outfix matches.
  EXPECT_TRUE(Blacklist::Matches("really@bad", "really/bad"));
  EXPECT_TRUE(Blacklist::Matches("really@bad", "really.com/bad"));
  EXPECT_TRUE(Blacklist::Matches("really@bad", "really.com/path/bad"));
  EXPECT_TRUE(Blacklist::Matches("really@bad", "really.evil.com/path/bad"));
  EXPECT_FALSE(Blacklist::Matches("really@bad", "really.bad.com"));
  EXPECT_FALSE(Blacklist::Matches("really@bad", "reallybad"));
  EXPECT_FALSE(Blacklist::Matches("really@bad", ".reallybad"));
  EXPECT_FALSE(Blacklist::Matches("really@bad", "reallybad."));
  EXPECT_FALSE(Blacklist::Matches("really@bad", "really.bad."));
  EXPECT_FALSE(Blacklist::Matches("really@bad", ".really.bad"));
}
