// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#include "base/file_util.h"
#include "base/format_macros.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/string_util.h"
#include "chrome/tools/convert_dict/aff_reader.h"
#include "chrome/tools/convert_dict/dic_reader.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/hunspell/google/bdict_reader.h"
#include "third_party/hunspell/google/bdict_writer.h"

namespace {

// Compares the given word list with the serialized trie to make sure they
// are the same.
// (This function is copied from "chrome/tools/convert_dict/convert_dict.cc").
bool VerifyWords(const convert_dict::DicReader::WordList& org_words,
                 const std::string& serialized) {
  hunspell::BDictReader reader;
  EXPECT_TRUE(
      reader.Init(reinterpret_cast<const unsigned char*>(serialized.data()),
      serialized.size()));

  hunspell::WordIterator iter = reader.GetAllWordIterator();

  int affix_ids[hunspell::BDict::MAX_AFFIXES_PER_WORD];

  static const int kBufSize = 128;
  char buf[kBufSize];
  for (size_t i = 0; i < org_words.size(); i++) {
    SCOPED_TRACE(StringPrintf("org_words[%" PRIuS "]: %s",
                              i, org_words[i].first.c_str()));

    int affix_matches = iter.Advance(buf, kBufSize, affix_ids);
    EXPECT_NE(0, affix_matches);
    EXPECT_EQ(org_words[i].first, std::string(buf));
    EXPECT_EQ(affix_matches, static_cast<int>(org_words[i].second.size()));

    // Check the individual affix indices.
    for (size_t affix_index = 0; affix_index < org_words[i].second.size();
         affix_index++) {
      EXPECT_EQ(affix_ids[affix_index], org_words[i].second[affix_index]);
    }
  }

  return true;
}

// Implements the test process used by ConvertDictTest.
// This function encapsulates all complicated operations used by
// ConvertDictTest so we can conceal them from the tests themselves.
// This function consists of the following parts:
// * Creates a dummy affix file and a dictionary file.
// * Reads the dummy files.
// * Creates bdict data.
// * Verify the bdict data.
void RunDictionaryTest(const char* codepage,
                       const std::map<std::wstring, bool>& word_list) {
  // Create an affix data and a dictionary data.
  std::string aff_data(StringPrintf("SET %s\n", codepage));

  std::string dic_data(StringPrintf("%" PRIuS "\n", word_list.size()));
  for (std::map<std::wstring, bool>::const_iterator it = word_list.begin();
       it != word_list.end(); ++it) {
    std::string encoded_word;
    EXPECT_TRUE(WideToCodepage(it->first,
                               codepage,
                               base::OnStringConversionError::FAIL,
                               &encoded_word));
    dic_data += encoded_word;
    dic_data += "\n";
  }

  // Create a temporary affix file and a dictionary file from the test data.
  FilePath aff_file;
  file_util::CreateTemporaryFile(&aff_file);
  file_util::WriteFile(aff_file, aff_data.c_str(), aff_data.length());

  FilePath dic_file;
  file_util::CreateTemporaryFile(&dic_file);
  file_util::WriteFile(dic_file, dic_data.c_str(), dic_data.length());

  {
    // Read the above affix file with AffReader and read the dictionary file
    // with DicReader, respectively.
#if defined(OS_WIN)
    std::string aff_path = WideToUTF8(aff_file.value());
    std::string dic_path = WideToUTF8(dic_file.value());
#else
    std::string aff_path = aff_file.value();
    std::string dic_path = dic_file.value();
#endif
    convert_dict::AffReader aff_reader(aff_path);
    EXPECT_TRUE(aff_reader.Read());

    convert_dict::DicReader dic_reader(dic_path);
    EXPECT_TRUE(dic_reader.Read(&aff_reader));

    // Verify this DicReader includes all the input words.
    EXPECT_EQ(word_list.size(), dic_reader.words().size());
    for (size_t i = 0; i < dic_reader.words().size(); ++i) {
      SCOPED_TRACE(StringPrintf("dic_reader.words()[%" PRIuS "]: %s",
                                i, dic_reader.words()[i].first.c_str()));
      std::wstring word(UTF8ToWide(dic_reader.words()[i].first));
      EXPECT_TRUE(word_list.find(word) != word_list.end());
    }

    // Create BDICT data and verify it.
    hunspell::BDictWriter writer;
    writer.SetComment(aff_reader.comments());
    writer.SetAffixRules(aff_reader.affix_rules());
    writer.SetAffixGroups(aff_reader.GetAffixGroups());
    writer.SetReplacements(aff_reader.replacements());
    writer.SetOtherCommands(aff_reader.other_commands());
    writer.SetWords(dic_reader.words());

    VerifyWords(dic_reader.words(), writer.GetBDict());
  }

  // Deletes the temporary files.
  // We need to delete them after the above AffReader and DicReader are deleted
  // since they close the input files in their destructors.
  file_util::Delete(aff_file, false);
  file_util::Delete(dic_file, false);
}

}  // namespace

// Tests whether or not our DicReader can read all the input English words
TEST(ConvertDictTest, English) {
  const char kCodepage[] = "UTF-8";
  const wchar_t* kWords[] = {
    L"I",
    L"he",
    L"she",
    L"it",
    L"we",
    L"you",
    L"they",
  };

  std::map<std::wstring, bool> word_list;
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kWords); ++i)
    word_list.insert(std::make_pair<std::wstring, bool>(kWords[i], true));

  RunDictionaryTest(kCodepage, word_list);
}

// Tests whether or not our DicReader can read all the input Russian words.
TEST(ConvertDictTest, Russian) {
  const char kCodepage[] = "KOI8-R";
  const wchar_t* kWords[] = {
    L"\x044f",
    L"\x0442\x044b",
    L"\x043e\x043d",
    L"\x043e\x043d\x0430",
    L"\x043e\x043d\x043e",
    L"\x043c\x044b",
    L"\x0432\x044b",
    L"\x043e\x043d\x0438",
  };

  std::map<std::wstring, bool> word_list;
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kWords); ++i)
    word_list.insert(std::make_pair<std::wstring, bool>(kWords[i], true));

  RunDictionaryTest(kCodepage, word_list);
}

// Tests whether or not our DicReader can read all the input Hungarian words.
TEST(ConvertDictTest, Hungarian) {
  const char kCodepage[] = "ISO8859-2";
  const wchar_t* kWords[] = {
    L"\x00e9\x006e",
    L"\x0074\x0065",
    L"\x0151",
    L"\x00f6\x006e",
    L"\x006d\x0061\x0067\x0061",
    L"\x006d\x0069",
    L"\x0074\x0069",
    L"\x0151\x006b",
    L"\x00f6\x006e\x00f6\x006b",
    L"\x006d\x0061\x0067\x0075\x006b",
  };

  std::map<std::wstring, bool> word_list;
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kWords); ++i)
    word_list.insert(std::make_pair<std::wstring, bool>(kWords[i], true));

  RunDictionaryTest(kCodepage, word_list);
}
