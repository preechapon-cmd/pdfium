// Copyright 2014 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com
// Original code is licensed as follows:
/*
 * Copyright 2007 ZXing authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "fxbarcode/qrcode/BC_QRCoderMode.h"

#include <utility>

#include "core/fxcrt/check.h"
#include "core/fxcrt/fx_system.h"

CBC_QRCoderMode* CBC_QRCoderMode::sBYTE = nullptr;
CBC_QRCoderMode* CBC_QRCoderMode::sNUMERIC = nullptr;
CBC_QRCoderMode* CBC_QRCoderMode::sALPHANUMERIC = nullptr;

CBC_QRCoderMode::CBC_QRCoderMode(std::vector<int32_t> charCountBits,
                                 int32_t bits)
    : character_count_bits_for_versions_(std::move(charCountBits)),
      bits_(bits) {}

CBC_QRCoderMode::~CBC_QRCoderMode() = default;

void CBC_QRCoderMode::Initialize() {
  sBYTE = new CBC_QRCoderMode({8, 16, 16}, 0x4);
  sALPHANUMERIC = new CBC_QRCoderMode({9, 11, 13}, 0x2);
  sNUMERIC = new CBC_QRCoderMode({10, 12, 14}, 0x1);
}

void CBC_QRCoderMode::Finalize() {
  delete sBYTE;
  sBYTE = nullptr;
  delete sALPHANUMERIC;
  sALPHANUMERIC = nullptr;
  delete sNUMERIC;
  sNUMERIC = nullptr;
}

int32_t CBC_QRCoderMode::GetBits() const {
  return bits_;
}

int32_t CBC_QRCoderMode::GetCharacterCountBits(int32_t number) const {
  if (character_count_bits_for_versions_.empty()) {
    return 0;
  }

  int32_t offset;
  if (number <= 9) {
    offset = 0;
  } else if (number <= 26) {
    offset = 1;
  } else {
    offset = 2;
  }

  int32_t result = character_count_bits_for_versions_[offset];
  DCHECK(result != 0);
  return result;
}
