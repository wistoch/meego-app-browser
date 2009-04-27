// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_codec.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

class BookmarkCodecTest : public testing::Test {
 protected:
  // Helpers to create bookmark models with different data.
  // Callers own the returned instances.
  BookmarkModel* CreateModelWithOneUrl() {
    scoped_ptr<BookmarkModel> model(new BookmarkModel(NULL));
    BookmarkNode* bookmark_bar = model->GetBookmarkBarNode();
    model->AddURL(bookmark_bar, 0, L"foo", GURL(L"http://www.foo.com"));
    return model.release();
  }
  BookmarkModel* CreateModelWithTwoUrls() {
    scoped_ptr<BookmarkModel> model(new BookmarkModel(NULL));
    BookmarkNode* bookmark_bar = model->GetBookmarkBarNode();
    model->AddURL(bookmark_bar, 0, L"foo", GURL(L"http://www.foo.com"));
    model->AddURL(bookmark_bar, 1, L"bar", GURL(L"http://www.bar.com"));
    return model.release();
  }

  void GetBookmarksBarChildValue(Value* value,
                                 size_t index,
                                 DictionaryValue** result_value) {
    ASSERT_EQ(Value::TYPE_DICTIONARY, value->GetType());

    DictionaryValue* d_value = static_cast<DictionaryValue*>(value);
    Value* roots;
    ASSERT_TRUE(d_value->Get(BookmarkCodec::kRootsKey, &roots));
    ASSERT_EQ(Value::TYPE_DICTIONARY, roots->GetType());

    DictionaryValue* roots_d_value = static_cast<DictionaryValue*>(roots);
    Value* bb_value;
    ASSERT_TRUE(roots_d_value->Get(BookmarkCodec::kRootFolderNameKey,
                                   &bb_value));
    ASSERT_EQ(Value::TYPE_DICTIONARY, bb_value->GetType());

    DictionaryValue* bb_d_value = static_cast<DictionaryValue*>(bb_value);
    Value* bb_children_value;
    ASSERT_TRUE(bb_d_value->Get(BookmarkCodec::kChildrenKey,
                                &bb_children_value));
    ASSERT_EQ(Value::TYPE_LIST, bb_children_value->GetType());

    ListValue* bb_children_l_value = static_cast<ListValue*>(bb_children_value);
    Value* child_value;
    ASSERT_TRUE(bb_children_l_value->Get(index, &child_value));
    ASSERT_EQ(Value::TYPE_DICTIONARY, child_value->GetType());

    *result_value = static_cast<DictionaryValue*>(child_value);
  }

  Value* EncodeHelper(BookmarkModel* model, std::string* checksum) {
    BookmarkCodec encoder;
    // Computed and stored checksums should be empty.
    EXPECT_EQ("", encoder.computed_checksum());
    EXPECT_EQ("", encoder.stored_checksum());

    scoped_ptr<Value> value(encoder.Encode(model));
    const std::string& computed_checksum = encoder.computed_checksum();
    const std::string& stored_checksum = encoder.stored_checksum();
 
    // Computed and stored checksums should not be empty and should be equal.
    EXPECT_FALSE(computed_checksum.empty());
    EXPECT_FALSE(stored_checksum.empty());
    EXPECT_EQ(computed_checksum, stored_checksum);

    *checksum = computed_checksum;
    return value.release();
  }

  BookmarkModel* DecodeHelper(const Value& value,
                              const std::string& expected_stored_checksum,
                              std::string* computed_checksum,
                              bool expected_changes) {
    BookmarkCodec decoder;
    // Computed and stored checksums should be empty.
    EXPECT_EQ("", decoder.computed_checksum());
    EXPECT_EQ("", decoder.stored_checksum());

    scoped_ptr<BookmarkModel> model(new BookmarkModel(NULL));
    EXPECT_TRUE(decoder.Decode(model.get(), value));

    *computed_checksum = decoder.computed_checksum();
    const std::string& stored_checksum = decoder.stored_checksum();
 
    // Computed and stored checksums should not be empty.
    EXPECT_FALSE(computed_checksum->empty());
    EXPECT_FALSE(stored_checksum.empty());

    // Stored checksum should be as expected.
    EXPECT_EQ(expected_stored_checksum, stored_checksum);

    // The two checksums should be equal if expected_changes is true; otherwise
    // they should be different.
    if (expected_changes)
      EXPECT_NE(*computed_checksum, stored_checksum);
    else
      EXPECT_EQ(*computed_checksum, stored_checksum);

    return model.release();
  }
};

TEST_F(BookmarkCodecTest, ChecksumEncodeDecodeTest) {
  scoped_ptr<BookmarkModel> model_to_encode(CreateModelWithOneUrl());
  std::string enc_checksum;
  scoped_ptr<Value> value(EncodeHelper(model_to_encode.get(), &enc_checksum));

  EXPECT_TRUE(value.get() != NULL);

  std::string dec_checksum;
  scoped_ptr<BookmarkModel> decoded_model(DecodeHelper(
      *value.get(), enc_checksum, &dec_checksum, false));
}

TEST_F(BookmarkCodecTest, ChecksumEncodeIdenticalModelsTest) {
  // Encode two identical models and make sure the check-sums are same as long
  // as the data is the same.
  scoped_ptr<BookmarkModel> model1(CreateModelWithOneUrl());
  std::string enc_checksum1;
  scoped_ptr<Value> value1(EncodeHelper(model1.get(), &enc_checksum1));
  EXPECT_TRUE(value1.get() != NULL);

  scoped_ptr<BookmarkModel> model2(CreateModelWithOneUrl());
  std::string enc_checksum2;
  scoped_ptr<Value> value2(EncodeHelper(model2.get(), &enc_checksum2));
  EXPECT_TRUE(value2.get() != NULL);

  ASSERT_EQ(enc_checksum1, enc_checksum2);
}

TEST_F(BookmarkCodecTest, ChecksumManualEditTest) {
  scoped_ptr<BookmarkModel> model_to_encode(CreateModelWithOneUrl());
  std::string enc_checksum;
  scoped_ptr<Value> value(EncodeHelper(model_to_encode.get(), &enc_checksum));

  EXPECT_TRUE(value.get() != NULL);

  // Change something in the encoded value before decoding it.
  DictionaryValue* child1_value;
  GetBookmarksBarChildValue(value.get(), 0, &child1_value);
  std::wstring title;
  ASSERT_TRUE(child1_value->GetString(BookmarkCodec::kNameKey, &title));
  ASSERT_TRUE(child1_value->SetString(BookmarkCodec::kNameKey, title + L"1"));

  std::string dec_checksum;
  scoped_ptr<BookmarkModel> decoded_model1(DecodeHelper(
      *value.get(), enc_checksum, &dec_checksum, true));

  // Undo the change and make sure the checksum is same as original.
  ASSERT_TRUE(child1_value->SetString(BookmarkCodec::kNameKey, title));
  scoped_ptr<BookmarkModel> decoded_model2(DecodeHelper(
      *value.get(), enc_checksum, &dec_checksum, false));
}
