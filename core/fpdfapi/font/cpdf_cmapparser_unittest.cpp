// Copyright 2015 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fpdfapi/font/cpdf_cmapparser.h"

#include "core/fxcrt/span.h"
#include "core/fxcrt/span_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Helps with default construction of the appropriate span rather than
// writing make_span() and using span_equal() directly.
bool uint_ranges_equal(pdfium::span<const uint8_t> a,
                       pdfium::span<const uint8_t> b) {
  return fxcrt::span_equals(a, b);
}

}  // namespace

TEST(CPDFCMapParserTest, GetCode) {
  EXPECT_EQ(0u, CPDF_CMapParser::GetCode(""));
  EXPECT_EQ(0u, CPDF_CMapParser::GetCode("<"));
  EXPECT_EQ(194u, CPDF_CMapParser::GetCode("<c2"));
  EXPECT_EQ(162u, CPDF_CMapParser::GetCode("<A2"));
  EXPECT_EQ(2802u, CPDF_CMapParser::GetCode("<Af2"));
  EXPECT_EQ(162u, CPDF_CMapParser::GetCode("<A2z"));

  EXPECT_EQ(12u, CPDF_CMapParser::GetCode("12"));
  EXPECT_EQ(12u, CPDF_CMapParser::GetCode("12d"));
  EXPECT_EQ(128u, CPDF_CMapParser::GetCode("128"));

  EXPECT_EQ(4294967295u, CPDF_CMapParser::GetCode("<FFFFFFFF"));

  // Overflow a uint32_t.
  EXPECT_EQ(0u, CPDF_CMapParser::GetCode("<100000000"));
}

TEST(CPDFCMapParserTest, GetCodeRange) {
  std::optional<CPDF_CMap::CodeRange> range;

  // Must start with a <
  range = CPDF_CMapParser::GetCodeRange("", "");
  EXPECT_FALSE(range.has_value());
  range = CPDF_CMapParser::GetCodeRange("A", "");
  EXPECT_FALSE(range.has_value());

  // m_CharSize must be <= 4
  range = CPDF_CMapParser::GetCodeRange("<aaaaaaaaaa>", "");
  EXPECT_FALSE(range.has_value());

  range = CPDF_CMapParser::GetCodeRange("<12345678>", "<87654321>");
  ASSERT_TRUE(range.has_value());
  ASSERT_EQ(4u, range.value().m_CharSize);
  {
    constexpr uint8_t kLower[4] = {18, 52, 86, 120};
    constexpr uint8_t kUpper[4] = {135, 101, 67, 33};
    EXPECT_TRUE(uint_ranges_equal(kLower, range.value().m_Lower));
    EXPECT_TRUE(uint_ranges_equal(kUpper, range.value().m_Upper));
  }

  // Hex characters
  range = CPDF_CMapParser::GetCodeRange("<a1>", "<F3>");
  ASSERT_TRUE(range.has_value());
  ASSERT_EQ(1u, range.value().m_CharSize);
  EXPECT_EQ(161, range.value().m_Lower[0]);
  EXPECT_EQ(243, range.value().m_Upper[0]);

  // The second string should return 0's if it is shorter
  range = CPDF_CMapParser::GetCodeRange("<a1>", "");
  ASSERT_TRUE(range.has_value());
  ASSERT_EQ(1u, range.value().m_CharSize);
  EXPECT_EQ(161, range.value().m_Lower[0]);
  EXPECT_EQ(0, range.value().m_Upper[0]);
}
