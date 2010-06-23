// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/renderer_host/gtk_im_context_wrapper.h"

#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"

namespace {

struct AttributeInfo {
  int type;
  int value;
  int start_offset;
  int end_offset;
};

struct Underline {
  unsigned startOffset;
  unsigned endOffset;
  uint32 color;
  bool thick;
};

struct TestData {
  const char* text;
  const AttributeInfo attrs[10];
  const Underline underlines[10];
};

const TestData kTestData[] = {
  // Normal case
  { "One Two Three",
    { { PANGO_ATTR_UNDERLINE, PANGO_UNDERLINE_SINGLE, 0, 3 },
      { PANGO_ATTR_UNDERLINE, PANGO_UNDERLINE_DOUBLE, 4, 7 },
      { PANGO_ATTR_BACKGROUND, 0, 4, 7 },
      { PANGO_ATTR_UNDERLINE, PANGO_UNDERLINE_SINGLE, 8, 13 },
      { 0, 0, 0, 0 } },
    { { 0, 3, SK_ColorBLACK, false },
      { 4, 7, SK_ColorBLACK, true },
      { 8, 13, SK_ColorBLACK, false },
      { 0, 0, 0, false } }
  },

  // Offset overflow.
  { "One Two Three",
    { { PANGO_ATTR_UNDERLINE, PANGO_UNDERLINE_SINGLE, 0, 3 },
      { PANGO_ATTR_BACKGROUND, 0, 4, 7 },
      { PANGO_ATTR_UNDERLINE, PANGO_UNDERLINE_SINGLE, 8, 20 },
      { 0, 0, 0, 0 } },
    { { 0, 3, SK_ColorBLACK, false },
      { 4, 7, SK_ColorBLACK, true },
      { 8, 13, SK_ColorBLACK, false },
      { 0, 0, 0, false} }
  },

  // Error underline.
  { "One Two Three",
    { { PANGO_ATTR_UNDERLINE, PANGO_UNDERLINE_SINGLE, 0, 3 },
      { PANGO_ATTR_UNDERLINE, PANGO_UNDERLINE_ERROR, 4, 7 },
      { PANGO_ATTR_UNDERLINE, PANGO_UNDERLINE_SINGLE, 8, 13 },
      { 0, 0, 0, 0 } },
    { { 0, 3, SK_ColorBLACK, false },
      { 4, 7, SK_ColorRED, false },
      { 8, 13, SK_ColorBLACK, false },
      { 0, 0, 0, false} }
  },

  // Default underline.
  { "One Two Three",
    { { 0, 0, 0, 0 } },
    { { 0, 13, SK_ColorBLACK, false },
      { 0, 0, 0, false } }
  },

  // Unicode, including non-BMP characters: "123你好𠀀𠀁一丁 456"
  { "123\xE4\xBD\xA0\xE5\xA5\xBD\xF0\xA0\x80\x80\xF0\xA0\x80\x81\xE4\xB8\x80\xE4\xB8\x81 456",
    { { PANGO_ATTR_UNDERLINE, PANGO_UNDERLINE_SINGLE, 0, 3 },
      { PANGO_ATTR_UNDERLINE, PANGO_UNDERLINE_SINGLE, 3, 5 },
      { PANGO_ATTR_BACKGROUND, 0, 5, 7 },
      { PANGO_ATTR_UNDERLINE, PANGO_UNDERLINE_SINGLE, 7, 13 },
      { 0, 0, 0, 0 } },
    { { 0, 3, SK_ColorBLACK, false },
      { 3, 5, SK_ColorBLACK, false },
      { 5, 9, SK_ColorBLACK, true },
      { 9, 15, SK_ColorBLACK, false },
      { 0, 0, 0, false } }
  },
};

void CompareUnderline(const Underline& a,
                      const WebKit::WebCompositionUnderline& b) {
  EXPECT_EQ(a.startOffset, b.startOffset);
  EXPECT_EQ(a.endOffset, b.endOffset);
  EXPECT_EQ(a.color, b.color);
  EXPECT_EQ(a.thick, b.thick);
}

class GtkIMContextWrapperTest : public testing::Test {
};

}  // namespace

TEST(GtkIMContextWrapperTest, ExtractCompositionInfo) {
  for (size_t i = 0; i < arraysize(kTestData); ++i) {
    const char* text = kTestData[i].text;
    const AttributeInfo* attrs = kTestData[i].attrs;
    SCOPED_TRACE(testing::Message() << "Testing:" << i
                 << " text:" << text);

    PangoAttrList* pango_attrs = pango_attr_list_new();
    for (size_t a = 0; attrs[a].type; ++a) {
      PangoAttribute* pango_attr = NULL;
      switch (attrs[a].type) {
        case PANGO_ATTR_UNDERLINE:
          pango_attr = pango_attr_underline_new(
              static_cast<PangoUnderline>(attrs[a].value));
          break;
        case PANGO_ATTR_BACKGROUND:
          pango_attr = pango_attr_background_new(0, 0, 0);
          break;
        default:
          NOTREACHED();
      }
      pango_attr->start_index =
          g_utf8_offset_to_pointer(text, attrs[a].start_offset) - text;
      pango_attr->end_index =
          g_utf8_offset_to_pointer(text, attrs[a].end_offset) - text;
      pango_attr_list_insert(pango_attrs, pango_attr);
    }

    string16 utf16_text;
    std::vector<WebKit::WebCompositionUnderline> results;
    int selection_start;
    int selection_end;

    GtkIMContextWrapper::ExtractCompositionInfo(text, pango_attrs, 0,
        &utf16_text, &results, &selection_start, &selection_end);

    const Underline* underlines = kTestData[i].underlines;
    for (size_t u = 0; underlines[u].color && u < results.size(); ++u) {
      SCOPED_TRACE(testing::Message() << "Underline:" << u);
      CompareUnderline(underlines[u], results[u]);
    }

    pango_attr_list_unref(pango_attrs);
  }
}
