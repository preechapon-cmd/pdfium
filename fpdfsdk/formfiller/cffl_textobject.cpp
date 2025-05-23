// Copyright 2017 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/formfiller/cffl_textobject.h"

#include "core/fpdfapi/page/cpdf_page.h"
#include "core/fpdfdoc/cpdf_bafontmap.h"
#include "fpdfsdk/cpdfsdk_widget.h"

CFFL_TextObject::CFFL_TextObject(CFFL_InteractiveFormFiller* pFormFiller,
                                 CPDFSDK_Widget* pWidget)
    : CFFL_FormField(pFormFiller, pWidget) {}

CFFL_TextObject::~CFFL_TextObject() {
  // Destroy view classes before this object's members are destroyed since
  // the view classes have pointers to font_map_ that would be left dangling.
  DestroyWindows();
}

CPWL_Wnd* CFFL_TextObject::ResetPWLWindow(const CPDFSDK_PageView* pPageView) {
  DestroyPWLWindow(pPageView);
  ObservedPtr<CPWL_Wnd> pRet(CreateOrUpdatePWLWindow(pPageView));
  widget_->UpdateField();  // May invoke JS, invalidating |pRet|.
  return pRet.Get();
}

CPWL_Wnd* CFFL_TextObject::RestorePWLWindow(const CPDFSDK_PageView* pPageView) {
  SavePWLWindowState(pPageView);
  DestroyPWLWindow(pPageView);
  RecreatePWLWindowFromSavedState(pPageView);
  ObservedPtr<CPWL_Wnd> pRet(GetPWLWindow(pPageView));
  widget_->UpdateField();  // May invoke JS, invalidating |pRet|.
  return pRet.Get();
}

CPDF_BAFontMap* CFFL_TextObject::GetOrCreateFontMap() {
  if (!font_map_) {
    font_map_ = std::make_unique<CPDF_BAFontMap>(
        widget_->GetPDFPage()->GetDocument(),
        widget_->GetPDFAnnot()->GetMutableAnnotDict(), "N");
  }
  return font_map_.get();
}
