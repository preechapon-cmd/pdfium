// Copyright 2014 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FXFA_CXFA_FWLTHEME_H_
#define XFA_FXFA_CXFA_FWLTHEME_H_

#include <memory>

#include "core/fxcrt/retain_ptr.h"
#include "core/fxcrt/widestring.h"
#include "fxjs/gc/heap.h"
#include "v8/include/cppgc/garbage-collected.h"
#include "v8/include/cppgc/member.h"
#include "v8/include/cppgc/prefinalizer.h"
#include "xfa/fwl/ifwl_themeprovider.h"

class CXFA_FFApp;
class CXFA_FFDoc;

namespace pdfium {

class CFDE_TextOut;

class CXFA_FWLTheme final : public cppgc::GarbageCollected<CXFA_FWLTheme>,
                            public IFWL_ThemeProvider {
  CPPGC_USING_PRE_FINALIZER(CXFA_FWLTheme, PreFinalize);

 public:
  CONSTRUCT_VIA_MAKE_GARBAGE_COLLECTED;
  ~CXFA_FWLTheme() override;

  void PreFinalize();

  // IFWL_ThemeProvider:
  void Trace(cppgc::Visitor* visitor) const override;
  void DrawBackground(const CFWL_ThemeBackground& pParams) override;
  void DrawText(const CFWL_ThemeText& pParams) override;
  void CalcTextRect(const CFWL_ThemeText& pParams, CFX_RectF* pRect) override;
  float GetCXBorderSize() const override;
  float GetCYBorderSize() const override;
  CFX_RectF GetUIMargin(const CFWL_ThemePart& pThemePart) const override;
  float GetFontSize(const CFWL_ThemePart& pThemePart) const override;
  RetainPtr<CFGAS_GEFont> GetFont(const CFWL_ThemePart& pThemePart) override;
  RetainPtr<CFGAS_GEFont> GetFWLFont() override;
  float GetLineHeight(const CFWL_ThemePart& pThemePart) const override;
  float GetScrollBarWidth() const override;
  FX_COLORREF GetTextColor(const CFWL_ThemePart& pThemePart) const override;
  CFX_SizeF GetSpaceAboveBelow(const CFWL_ThemePart& pThemePart) const override;

  bool LoadCalendarFont(CXFA_FFDoc* doc);

 private:
  CXFA_FWLTheme(cppgc::Heap* pHeap, CXFA_FFApp* pApp);

  std::unique_ptr<CFDE_TextOut> text_out_;
  RetainPtr<CFGAS_GEFont> fwlfont_;
  RetainPtr<CFGAS_GEFont> calendar_font_;
  cppgc::Member<CXFA_FFApp> const app_;
  WideString resource_;
  CFX_RectF rect_;
};

}  // namespace pdfium

// TODO(crbug.com/42271761): Remove.
using pdfium::CXFA_FWLTheme;

#endif  // XFA_FXFA_CXFA_FWLTHEME_H_
