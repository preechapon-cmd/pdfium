// Copyright 2014 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com
// Original code is licensed as follows:
/*
 * Copyright 2008 ZXing authors
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

#include "fxbarcode/qrcode/BC_QRCoderEncoder.h"

#include <stdint.h>

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "core/fxcrt/check.h"
#include "core/fxcrt/check_op.h"
#include "core/fxcrt/data_vector.h"
#include "core/fxcrt/fx_extension.h"
#include "core/fxcrt/fx_string.h"
#include "core/fxcrt/stl_util.h"
#include "fxbarcode/common/BC_CommonByteMatrix.h"
#include "fxbarcode/common/reedsolomon/BC_ReedSolomon.h"
#include "fxbarcode/common/reedsolomon/BC_ReedSolomonGF256.h"
#include "fxbarcode/qrcode/BC_QRCoder.h"
#include "fxbarcode/qrcode/BC_QRCoderBitVector.h"
#include "fxbarcode/qrcode/BC_QRCoderECBlockData.h"
#include "fxbarcode/qrcode/BC_QRCoderMaskUtil.h"
#include "fxbarcode/qrcode/BC_QRCoderMatrixUtil.h"
#include "fxbarcode/qrcode/BC_QRCoderMode.h"
#include "fxbarcode/qrcode/BC_QRCoderVersion.h"

using ModeStringPair = std::pair<CBC_QRCoderMode*, ByteString>;

namespace {

CBC_ReedSolomonGF256* g_QRCodeField = nullptr;

struct QRCoderBlockPair {
  DataVector<uint8_t> data;
  DataVector<uint8_t> ecc;
};

// This is a mapping for an ASCII table, starting at an index of 32.
const auto kAlphaNumericTable = std::to_array<const int8_t>(
    {36, -1, -1, -1, 37, 38, -1, -1, -1, -1, 39, 40, -1, 41, 42, 43,  // 32-47
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  44, -1, -1, -1, -1, -1,  // 48-63
     -1, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,  // 64-79
     25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35});

int32_t GetAlphaNumericCode(int32_t code) {
  if (code < 32) {
    return -1;
  }
  size_t code_index = static_cast<size_t>(code - 32);
  if (code_index >= std::size(kAlphaNumericTable)) {
    return -1;
  }
  return kAlphaNumericTable[code_index];
}

bool AppendNumericBytes(const ByteString& content, CBC_QRCoderBitVector* bits) {
  size_t length = content.GetLength();
  size_t i = 0;
  while (i < length) {
    int32_t num1 = content[i] - '0';
    if (i + 2 < length) {
      int32_t num2 = content[i + 1] - '0';
      int32_t num3 = content[i + 2] - '0';
      bits->AppendBits(num1 * 100 + num2 * 10 + num3, 10);
      i += 3;
    } else if (i + 1 < length) {
      int32_t num2 = content[i + 1] - '0';
      bits->AppendBits(num1 * 10 + num2, 7);
      i += 2;
    } else {
      bits->AppendBits(num1, 4);
      i++;
    }
  }
  return true;
}

bool AppendAlphaNumericBytes(const ByteString& content,
                             CBC_QRCoderBitVector* bits) {
  size_t length = content.GetLength();
  size_t i = 0;
  while (i < length) {
    int32_t code1 = GetAlphaNumericCode(content[i]);
    if (code1 == -1) {
      return false;
    }

    if (i + 1 < length) {
      int32_t code2 = GetAlphaNumericCode(content[i + 1]);
      if (code2 == -1) {
        return false;
      }

      bits->AppendBits(code1 * 45 + code2, 11);
      i += 2;
    } else {
      bits->AppendBits(code1, 6);
      i++;
    }
  }
  return true;
}

bool Append8BitBytes(const ByteString& content, CBC_QRCoderBitVector* bits) {
  for (char c : content) {
    bits->AppendBits(c, 8);
  }
  return true;
}

void AppendModeInfo(CBC_QRCoderMode* mode, CBC_QRCoderBitVector* bits) {
  bits->AppendBits(mode->GetBits(), 4);
}

bool AppendLengthInfo(int32_t numLetters,
                      int32_t version,
                      CBC_QRCoderMode* mode,
                      CBC_QRCoderBitVector* bits) {
  const auto* qcv = CBC_QRCoderVersion::GetVersionForNumber(version);
  if (!qcv) {
    return false;
  }
  int32_t numBits = mode->GetCharacterCountBits(qcv->GetVersionNumber());
  if (numBits == 0) {
    return false;
  }
  if (numBits > ((1 << numBits) - 1)) {
    return true;
  }

  bits->AppendBits(numLetters, numBits);
  return true;
}

bool AppendBytes(const ByteString& content,
                 CBC_QRCoderMode* mode,
                 CBC_QRCoderBitVector* bits) {
  if (mode == CBC_QRCoderMode::sNUMERIC) {
    return AppendNumericBytes(content, bits);
  }
  if (mode == CBC_QRCoderMode::sALPHANUMERIC) {
    return AppendAlphaNumericBytes(content, bits);
  }
  if (mode == CBC_QRCoderMode::sBYTE) {
    return Append8BitBytes(content, bits);
  }
  return false;
}

bool InitQRCode(int32_t numInputBytes,
                const CBC_QRCoderErrorCorrectionLevel* ecLevel,
                CBC_QRCoder* qrCode) {
  qrCode->SetECLevel(ecLevel);
  for (int32_t i = 1; i <= CBC_QRCoderVersion::kMaxVersion; ++i) {
    const auto* version = CBC_QRCoderVersion::GetVersionForNumber(i);
    int32_t numBytes = version->GetTotalCodeWords();
    const auto* ecBlocks = version->GetECBlocksForLevel(*ecLevel);
    int32_t numEcBytes = ecBlocks->GetTotalECCodeWords();
    int32_t numRSBlocks = ecBlocks->GetNumBlocks();
    int32_t numDataBytes = numBytes - numEcBytes;
    if (numDataBytes >= numInputBytes + 3) {
      qrCode->SetVersion(i);
      qrCode->SetNumTotalBytes(numBytes);
      qrCode->SetNumDataBytes(numDataBytes);
      qrCode->SetNumRSBlocks(numRSBlocks);
      qrCode->SetNumECBytes(numEcBytes);
      qrCode->SetMatrixWidth(version->GetDimensionForVersion());
      return true;
    }
  }
  return false;
}

DataVector<uint8_t> GenerateECBytes(pdfium::span<const uint8_t> dataBytes,
                                    size_t numEcBytesInBlock) {
  // If |numEcBytesInBlock| is 0, the encoder will fail anyway.
  DCHECK(numEcBytesInBlock > 0);
  std::vector<int32_t> toEncode(dataBytes.size() + numEcBytesInBlock);
  std::copy(dataBytes.begin(), dataBytes.end(), toEncode.begin());

  DataVector<uint8_t> ecBytes;
  CBC_ReedSolomonEncoder encoder(g_QRCodeField);
  if (encoder.Encode(&toEncode, numEcBytesInBlock)) {
    ecBytes = DataVector<uint8_t>(toEncode.begin() + dataBytes.size(),
                                  toEncode.end());
    DCHECK_EQ(ecBytes.size(), static_cast<size_t>(numEcBytesInBlock));
  }
  return ecBytes;
}

int32_t CalculateMaskPenalty(CBC_CommonByteMatrix* matrix) {
  return CBC_QRCoderMaskUtil::ApplyMaskPenaltyRule1(matrix) +
         CBC_QRCoderMaskUtil::ApplyMaskPenaltyRule2(matrix) +
         CBC_QRCoderMaskUtil::ApplyMaskPenaltyRule3(matrix) +
         CBC_QRCoderMaskUtil::ApplyMaskPenaltyRule4(matrix);
}

std::optional<int32_t> ChooseMaskPattern(
    CBC_QRCoderBitVector* bits,
    const CBC_QRCoderErrorCorrectionLevel* ecLevel,
    int32_t version,
    CBC_CommonByteMatrix* matrix) {
  int32_t minPenalty = 65535;
  int32_t bestMaskPattern = -1;
  for (int32_t maskPattern = 0; maskPattern < CBC_QRCoder::kNumMaskPatterns;
       maskPattern++) {
    if (!CBC_QRCoderMatrixUtil::BuildMatrix(bits, ecLevel, version, maskPattern,
                                            matrix)) {
      return std::nullopt;
    }
    int32_t penalty = CalculateMaskPenalty(matrix);
    if (penalty < minPenalty) {
      minPenalty = penalty;
      bestMaskPattern = maskPattern;
    }
  }
  return bestMaskPattern;
}

void GetNumDataBytesAndNumECBytesForBlockID(int32_t numTotalBytes,
                                            int32_t numDataBytes,
                                            int32_t numRSBlocks,
                                            int32_t blockID,
                                            int32_t* numDataBytesInBlock,
                                            int32_t* numECBytesInBlock) {
  if (blockID >= numRSBlocks) {
    return;
  }

  int32_t numRsBlocksInGroup2 = numTotalBytes % numRSBlocks;
  int32_t numRsBlocksInGroup1 = numRSBlocks - numRsBlocksInGroup2;
  int32_t numTotalBytesInGroup1 = numTotalBytes / numRSBlocks;
  int32_t numTotalBytesInGroup2 = numTotalBytesInGroup1 + 1;
  int32_t numDataBytesInGroup1 = numDataBytes / numRSBlocks;
  int32_t numDataBytesInGroup2 = numDataBytesInGroup1 + 1;
  int32_t numEcBytesInGroup1 = numTotalBytesInGroup1 - numDataBytesInGroup1;
  int32_t numEcBytesInGroup2 = numTotalBytesInGroup2 - numDataBytesInGroup2;
  if (blockID < numRsBlocksInGroup1) {
    *numDataBytesInBlock = numDataBytesInGroup1;
    *numECBytesInBlock = numEcBytesInGroup1;
  } else {
    *numDataBytesInBlock = numDataBytesInGroup2;
    *numECBytesInBlock = numEcBytesInGroup2;
  }
}

bool TerminateBits(int32_t numDataBytes, CBC_QRCoderBitVector* bits) {
  size_t capacity = numDataBytes << 3;
  if (bits->Size() > capacity) {
    return false;
  }

  for (int32_t i = 0; i < 4 && bits->Size() < capacity; ++i) {
    bits->AppendBit(0);
  }

  int32_t numBitsInLastByte = bits->Size() % 8;
  if (numBitsInLastByte > 0) {
    int32_t numPaddingBits = 8 - numBitsInLastByte;
    for (int32_t j = 0; j < numPaddingBits; ++j) {
      bits->AppendBit(0);
    }
  }

  if (bits->Size() % 8 != 0) {
    return false;
  }

  int32_t numPaddingBytes = numDataBytes - bits->sizeInBytes();
  for (int32_t k = 0; k < numPaddingBytes; ++k) {
    bits->AppendBits(k % 2 ? 0x11 : 0xec, 8);
  }
  return bits->Size() == capacity;
}

CBC_QRCoderMode* ChooseMode(const ByteString& content) {
  bool hasNumeric = false;
  bool hasAlphaNumeric = false;
  for (size_t i = 0; i < content.GetLength(); i++) {
    if (FXSYS_IsDecimalDigit(content[i])) {
      hasNumeric = true;
    } else if (GetAlphaNumericCode(content[i]) != -1) {
      hasAlphaNumeric = true;
    } else {
      return CBC_QRCoderMode::sBYTE;
    }
  }
  if (hasAlphaNumeric) {
    return CBC_QRCoderMode::sALPHANUMERIC;
  }
  if (hasNumeric) {
    return CBC_QRCoderMode::sNUMERIC;
  }
  return CBC_QRCoderMode::sBYTE;
}

bool InterleaveWithECBytes(CBC_QRCoderBitVector* bits,
                           int32_t numTotalBytes,
                           int32_t numDataBytes,
                           int32_t numRSBlocks,
                           CBC_QRCoderBitVector* result) {
  DCHECK(numTotalBytes >= 0);
  DCHECK(numDataBytes >= 0);
  if (bits->sizeInBytes() != static_cast<size_t>(numDataBytes)) {
    return false;
  }

  int32_t dataBytesOffset = 0;
  size_t maxNumDataBytes = 0;
  size_t maxNumEcBytes = 0;
  std::vector<QRCoderBlockPair> blocks(numRSBlocks);
  for (int32_t i = 0; i < numRSBlocks; i++) {
    int32_t numDataBytesInBlock;
    int32_t numEcBytesInBlock;
    GetNumDataBytesAndNumECBytesForBlockID(numTotalBytes, numDataBytes,
                                           numRSBlocks, i, &numDataBytesInBlock,
                                           &numEcBytesInBlock);
    if (numDataBytesInBlock < 0 || numEcBytesInBlock <= 0) {
      return false;
    }

    DataVector<uint8_t> dataBytes(numDataBytesInBlock);
    fxcrt::Copy(
        bits->GetArray().subspan(static_cast<size_t>(dataBytesOffset),
                                 static_cast<size_t>(numDataBytesInBlock)),
        dataBytes);

    DataVector<uint8_t> ecBytes = GenerateECBytes(dataBytes, numEcBytesInBlock);
    if (ecBytes.empty()) {
      return false;
    }

    maxNumDataBytes = std::max(maxNumDataBytes, dataBytes.size());
    maxNumEcBytes = std::max(maxNumEcBytes, ecBytes.size());
    blocks[i].data = std::move(dataBytes);
    blocks[i].ecc = std::move(ecBytes);
    dataBytesOffset += numDataBytesInBlock;
  }
  if (numDataBytes != dataBytesOffset) {
    return false;
  }

  for (size_t x = 0; x < maxNumDataBytes; x++) {
    for (size_t j = 0; j < blocks.size(); j++) {
      const DataVector<uint8_t>& dataBytes = blocks[j].data;
      if (x < dataBytes.size()) {
        result->AppendBits(dataBytes[x], 8);
      }
    }
  }
  for (size_t y = 0; y < maxNumEcBytes; y++) {
    for (size_t l = 0; l < blocks.size(); l++) {
      const DataVector<uint8_t>& ecBytes = blocks[l].ecc;
      if (y < ecBytes.size()) {
        result->AppendBits(ecBytes[y], 8);
      }
    }
  }
  return static_cast<size_t>(numTotalBytes) == result->sizeInBytes();
}

}  // namespace

// static
void CBC_QRCoderEncoder::Initialize() {
  g_QRCodeField = new CBC_ReedSolomonGF256(0x011D);
  g_QRCodeField->Init();
}

// static
void CBC_QRCoderEncoder::Finalize() {
  delete g_QRCodeField;
  g_QRCodeField = nullptr;
}

// static
bool CBC_QRCoderEncoder::Encode(WideStringView content,
                                const CBC_QRCoderErrorCorrectionLevel* ecLevel,
                                CBC_QRCoder* qrCode) {
  ByteString utf8Data = FX_UTF8Encode(content);
  CBC_QRCoderMode* mode = ChooseMode(utf8Data);
  CBC_QRCoderBitVector dataBits;
  if (!AppendBytes(utf8Data, mode, &dataBits)) {
    return false;
  }
  int32_t numInputBytes = dataBits.sizeInBytes();
  if (!InitQRCode(numInputBytes, ecLevel, qrCode)) {
    return false;
  }
  CBC_QRCoderBitVector headerAndDataBits;
  AppendModeInfo(mode, &headerAndDataBits);
  int32_t numLetters = mode == CBC_QRCoderMode::sBYTE ? dataBits.sizeInBytes()
                                                      : content.GetLength();
  if (!AppendLengthInfo(numLetters, qrCode->GetVersion(), mode,
                        &headerAndDataBits)) {
    return false;
  }
  headerAndDataBits.AppendBitVector(&dataBits);
  if (!TerminateBits(qrCode->GetNumDataBytes(), &headerAndDataBits)) {
    return false;
  }
  CBC_QRCoderBitVector finalBits;
  if (!InterleaveWithECBytes(&headerAndDataBits, qrCode->GetNumTotalBytes(),
                             qrCode->GetNumDataBytes(),
                             qrCode->GetNumRSBlocks(), &finalBits)) {
    return false;
  }

  auto matrix = std::make_unique<CBC_CommonByteMatrix>(
      qrCode->GetMatrixWidth(), qrCode->GetMatrixWidth());
  std::optional<int32_t> maskPattern = ChooseMaskPattern(
      &finalBits, qrCode->GetECLevel(), qrCode->GetVersion(), matrix.get());
  if (!maskPattern.has_value()) {
    return false;
  }

  qrCode->SetMaskPattern(maskPattern.value());
  if (!CBC_QRCoderMatrixUtil::BuildMatrix(
          &finalBits, qrCode->GetECLevel(), qrCode->GetVersion(),
          qrCode->GetMaskPattern(), matrix.get())) {
    return false;
  }

  qrCode->SetMatrix(std::move(matrix));
  return qrCode->IsValid();
}
