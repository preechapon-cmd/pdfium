// Copyright 2019 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/test_loader.h"

#include <string.h>

#include "third_party/base/check_op.h"
#include "third_party/base/numerics/checked_math.h"

TestLoader::TestLoader(pdfium::span<const uint8_t> span) : m_Span(span) {}

// static
int TestLoader::GetBlock(void* param,
                         unsigned long pos,
                         unsigned char* pBuf,
                         unsigned long size) {
  TestLoader* pLoader = static_cast<TestLoader*>(param);
  pdfium::base::CheckedNumeric<size_t> end = pos;
  end += size;
  CHECK_LE(end.ValueOrDie(), pLoader->m_Span.size());

  memcpy(pBuf, &pLoader->m_Span[pos], size);
  return 1;
}
