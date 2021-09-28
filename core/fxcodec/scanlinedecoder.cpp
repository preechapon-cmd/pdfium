// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxcodec/scanlinedecoder.h"

#include "core/fxcrt/pauseindicator_iface.h"

namespace fxcodec {

ScanlineDecoder::ScanlineDecoder() : ScanlineDecoder(0, 0, 0, 0, 0, 0, 0) {}

ScanlineDecoder::ScanlineDecoder(int nOrigWidth,
                                 int nOrigHeight,
                                 int nOutputWidth,
                                 int nOutputHeight,
                                 int nComps,
                                 int nBpc,
                                 uint32_t nPitch)
    : m_OrigWidth(nOrigWidth),
      m_OrigHeight(nOrigHeight),
      m_OutputWidth(nOutputWidth),
      m_OutputHeight(nOutputHeight),
      m_nComps(nComps),
      m_bpc(nBpc),
      m_Pitch(nPitch) {}

ScanlineDecoder::~ScanlineDecoder() = default;

const uint8_t* ScanlineDecoder::GetScanline(int line) {
  if (m_NextLine == line + 1)
    return m_pLastScanline.data();

  if (m_NextLine < 0 || m_NextLine > line) {
    if (!Rewind())
      return nullptr;
    m_NextLine = 0;
  }
  while (m_NextLine < line) {
    GetNextLine();
    m_NextLine++;
  }
  m_pLastScanline = GetNextLine();
  m_NextLine++;
  return m_pLastScanline.data();
}

bool ScanlineDecoder::SkipToScanline(int line, PauseIndicatorIface* pPause) {
  if (m_NextLine == line || m_NextLine == line + 1)
    return false;

  if (m_NextLine < 0 || m_NextLine > line) {
    Rewind();
    m_NextLine = 0;
  }
  m_pLastScanline = pdfium::span<uint8_t>();
  while (m_NextLine < line) {
    m_pLastScanline = GetNextLine();
    m_NextLine++;
    if (pPause && pPause->NeedToPauseNow()) {
      return true;
    }
  }
  return false;
}

}  // namespace fxcodec
