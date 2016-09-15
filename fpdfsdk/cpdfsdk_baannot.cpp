// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/include/cpdfsdk_baannot.h"

#include "core/fpdfapi/fpdf_parser/include/cpdf_array.h"
#include "core/fpdfapi/fpdf_parser/include/cpdf_document.h"
#include "core/fpdfapi/fpdf_parser/include/cpdf_number.h"
#include "core/fpdfapi/fpdf_parser/include/cpdf_stream.h"
#include "core/fpdfapi/fpdf_parser/include/fpdf_parser_decode.h"
#include "fpdfsdk/include/cpdfsdk_datetime.h"
#include "fpdfsdk/include/cpdfsdk_document.h"
#include "fpdfsdk/include/cpdfsdk_pageview.h"

CPDFSDK_BAAnnot::CPDFSDK_BAAnnot(CPDF_Annot* pAnnot,
                                 CPDFSDK_PageView* pPageView)
    : CPDFSDK_Annot(pPageView), m_pAnnot(pAnnot) {}

CPDFSDK_BAAnnot::~CPDFSDK_BAAnnot() {}

CPDF_Annot* CPDFSDK_BAAnnot::GetPDFAnnot() const {
  return m_pAnnot;
}

CPDF_Annot* CPDFSDK_BAAnnot::GetPDFPopupAnnot() const {
  return m_pAnnot->GetPopupAnnot();
}

CPDF_Dictionary* CPDFSDK_BAAnnot::GetAnnotDict() const {
  return m_pAnnot->GetAnnotDict();
}

void CPDFSDK_BAAnnot::SetRect(const CFX_FloatRect& rect) {
  ASSERT(rect.right - rect.left >= GetMinWidth());
  ASSERT(rect.top - rect.bottom >= GetMinHeight());

  m_pAnnot->GetAnnotDict()->SetRectFor("Rect", rect);
}

CFX_FloatRect CPDFSDK_BAAnnot::GetRect() const {
  return m_pAnnot->GetRect();
}

CPDF_Annot::Subtype CPDFSDK_BAAnnot::GetAnnotSubtype() const {
  return m_pAnnot->GetSubtype();
}

void CPDFSDK_BAAnnot::DrawAppearance(CFX_RenderDevice* pDevice,
                                     const CFX_Matrix* pUser2Device,
                                     CPDF_Annot::AppearanceMode mode,
                                     const CPDF_RenderOptions* pOptions) {
  m_pAnnot->DrawAppearance(m_pPageView->GetPDFPage(), pDevice, pUser2Device,
                           mode, pOptions);
}

FX_BOOL CPDFSDK_BAAnnot::IsAppearanceValid() {
  return !!m_pAnnot->GetAnnotDict()->GetDictFor("AP");
}

FX_BOOL CPDFSDK_BAAnnot::IsAppearanceValid(CPDF_Annot::AppearanceMode mode) {
  CPDF_Dictionary* pAP = m_pAnnot->GetAnnotDict()->GetDictFor("AP");
  if (!pAP)
    return FALSE;

  // Choose the right sub-ap
  const FX_CHAR* ap_entry = "N";
  if (mode == CPDF_Annot::Down)
    ap_entry = "D";
  else if (mode == CPDF_Annot::Rollover)
    ap_entry = "R";
  if (!pAP->KeyExist(ap_entry))
    ap_entry = "N";

  // Get the AP stream or subdirectory
  CPDF_Object* psub = pAP->GetDirectObjectFor(ap_entry);
  return !!psub;
}

void CPDFSDK_BAAnnot::DrawBorder(CFX_RenderDevice* pDevice,
                                 const CFX_Matrix* pUser2Device,
                                 const CPDF_RenderOptions* pOptions) {
  m_pAnnot->DrawBorder(pDevice, pUser2Device, pOptions);
}

void CPDFSDK_BAAnnot::ClearCachedAP() {
  m_pAnnot->ClearCachedAP();
}

void CPDFSDK_BAAnnot::SetContents(const CFX_WideString& sContents) {
  if (sContents.IsEmpty())
    m_pAnnot->GetAnnotDict()->RemoveFor("Contents");
  else
    m_pAnnot->GetAnnotDict()->SetStringFor("Contents",
                                           PDF_EncodeText(sContents));
}

CFX_WideString CPDFSDK_BAAnnot::GetContents() const {
  return m_pAnnot->GetAnnotDict()->GetUnicodeTextFor("Contents");
}

void CPDFSDK_BAAnnot::SetAnnotName(const CFX_WideString& sName) {
  if (sName.IsEmpty())
    m_pAnnot->GetAnnotDict()->RemoveFor("NM");
  else
    m_pAnnot->GetAnnotDict()->SetStringFor("NM", PDF_EncodeText(sName));
}

CFX_WideString CPDFSDK_BAAnnot::GetAnnotName() const {
  return m_pAnnot->GetAnnotDict()->GetUnicodeTextFor("NM");
}

void CPDFSDK_BAAnnot::SetModifiedDate(const FX_SYSTEMTIME& st) {
  CPDFSDK_DateTime dt(st);
  CFX_ByteString str = dt.ToPDFDateTimeString();

  if (str.IsEmpty())
    m_pAnnot->GetAnnotDict()->RemoveFor("M");
  else
    m_pAnnot->GetAnnotDict()->SetStringFor("M", str);
}

FX_SYSTEMTIME CPDFSDK_BAAnnot::GetModifiedDate() const {
  FX_SYSTEMTIME systime;
  CFX_ByteString str = m_pAnnot->GetAnnotDict()->GetStringFor("M");

  CPDFSDK_DateTime dt(str);
  dt.ToSystemTime(systime);

  return systime;
}

void CPDFSDK_BAAnnot::SetFlags(uint32_t nFlags) {
  m_pAnnot->GetAnnotDict()->SetIntegerFor("F", nFlags);
}

uint32_t CPDFSDK_BAAnnot::GetFlags() const {
  return m_pAnnot->GetAnnotDict()->GetIntegerFor("F");
}

void CPDFSDK_BAAnnot::SetAppState(const CFX_ByteString& str) {
  if (str.IsEmpty())
    m_pAnnot->GetAnnotDict()->RemoveFor("AS");
  else
    m_pAnnot->GetAnnotDict()->SetStringFor("AS", str);
}

CFX_ByteString CPDFSDK_BAAnnot::GetAppState() const {
  return m_pAnnot->GetAnnotDict()->GetStringFor("AS");
}

void CPDFSDK_BAAnnot::SetStructParent(int key) {
  m_pAnnot->GetAnnotDict()->SetIntegerFor("StructParent", key);
}

int CPDFSDK_BAAnnot::GetStructParent() const {
  return m_pAnnot->GetAnnotDict()->GetIntegerFor("StructParent");
}

// border
void CPDFSDK_BAAnnot::SetBorderWidth(int nWidth) {
  CPDF_Array* pBorder = m_pAnnot->GetAnnotDict()->GetArrayFor("Border");

  if (pBorder) {
    pBorder->SetAt(2, new CPDF_Number(nWidth));
  } else {
    CPDF_Dictionary* pBSDict = m_pAnnot->GetAnnotDict()->GetDictFor("BS");

    if (!pBSDict) {
      pBSDict = new CPDF_Dictionary;
      m_pAnnot->GetAnnotDict()->SetFor("BS", pBSDict);
    }

    pBSDict->SetIntegerFor("W", nWidth);
  }
}

int CPDFSDK_BAAnnot::GetBorderWidth() const {
  if (CPDF_Array* pBorder = m_pAnnot->GetAnnotDict()->GetArrayFor("Border")) {
    return pBorder->GetIntegerAt(2);
  }
  if (CPDF_Dictionary* pBSDict = m_pAnnot->GetAnnotDict()->GetDictFor("BS")) {
    return pBSDict->GetIntegerFor("W", 1);
  }
  return 1;
}

void CPDFSDK_BAAnnot::SetBorderStyle(BorderStyle nStyle) {
  CPDF_Dictionary* pBSDict = m_pAnnot->GetAnnotDict()->GetDictFor("BS");
  if (!pBSDict) {
    pBSDict = new CPDF_Dictionary;
    m_pAnnot->GetAnnotDict()->SetFor("BS", pBSDict);
  }

  switch (nStyle) {
    case BorderStyle::SOLID:
      pBSDict->SetNameFor("S", "S");
      break;
    case BorderStyle::DASH:
      pBSDict->SetNameFor("S", "D");
      break;
    case BorderStyle::BEVELED:
      pBSDict->SetNameFor("S", "B");
      break;
    case BorderStyle::INSET:
      pBSDict->SetNameFor("S", "I");
      break;
    case BorderStyle::UNDERLINE:
      pBSDict->SetNameFor("S", "U");
      break;
    default:
      break;
  }
}

BorderStyle CPDFSDK_BAAnnot::GetBorderStyle() const {
  CPDF_Dictionary* pBSDict = m_pAnnot->GetAnnotDict()->GetDictFor("BS");
  if (pBSDict) {
    CFX_ByteString sBorderStyle = pBSDict->GetStringFor("S", "S");
    if (sBorderStyle == "S")
      return BorderStyle::SOLID;
    if (sBorderStyle == "D")
      return BorderStyle::DASH;
    if (sBorderStyle == "B")
      return BorderStyle::BEVELED;
    if (sBorderStyle == "I")
      return BorderStyle::INSET;
    if (sBorderStyle == "U")
      return BorderStyle::UNDERLINE;
  }

  CPDF_Array* pBorder = m_pAnnot->GetAnnotDict()->GetArrayFor("Border");
  if (pBorder) {
    if (pBorder->GetCount() >= 4) {
      CPDF_Array* pDP = pBorder->GetArrayAt(3);
      if (pDP && pDP->GetCount() > 0)
        return BorderStyle::DASH;
    }
  }

  return BorderStyle::SOLID;
}

void CPDFSDK_BAAnnot::SetColor(FX_COLORREF color) {
  CPDF_Array* pArray = new CPDF_Array;
  pArray->AddNumber((FX_FLOAT)FXSYS_GetRValue(color) / 255.0f);
  pArray->AddNumber((FX_FLOAT)FXSYS_GetGValue(color) / 255.0f);
  pArray->AddNumber((FX_FLOAT)FXSYS_GetBValue(color) / 255.0f);
  m_pAnnot->GetAnnotDict()->SetFor("C", pArray);
}

void CPDFSDK_BAAnnot::RemoveColor() {
  m_pAnnot->GetAnnotDict()->RemoveFor("C");
}

FX_BOOL CPDFSDK_BAAnnot::GetColor(FX_COLORREF& color) const {
  if (CPDF_Array* pEntry = m_pAnnot->GetAnnotDict()->GetArrayFor("C")) {
    size_t nCount = pEntry->GetCount();
    if (nCount == 1) {
      FX_FLOAT g = pEntry->GetNumberAt(0) * 255;

      color = FXSYS_RGB((int)g, (int)g, (int)g);

      return TRUE;
    } else if (nCount == 3) {
      FX_FLOAT r = pEntry->GetNumberAt(0) * 255;
      FX_FLOAT g = pEntry->GetNumberAt(1) * 255;
      FX_FLOAT b = pEntry->GetNumberAt(2) * 255;

      color = FXSYS_RGB((int)r, (int)g, (int)b);

      return TRUE;
    } else if (nCount == 4) {
      FX_FLOAT c = pEntry->GetNumberAt(0);
      FX_FLOAT m = pEntry->GetNumberAt(1);
      FX_FLOAT y = pEntry->GetNumberAt(2);
      FX_FLOAT k = pEntry->GetNumberAt(3);

      FX_FLOAT r = 1.0f - std::min(1.0f, c + k);
      FX_FLOAT g = 1.0f - std::min(1.0f, m + k);
      FX_FLOAT b = 1.0f - std::min(1.0f, y + k);

      color = FXSYS_RGB((int)(r * 255), (int)(g * 255), (int)(b * 255));

      return TRUE;
    }
  }

  return FALSE;
}

void CPDFSDK_BAAnnot::WriteAppearance(const CFX_ByteString& sAPType,
                                      const CFX_FloatRect& rcBBox,
                                      const CFX_Matrix& matrix,
                                      const CFX_ByteString& sContents,
                                      const CFX_ByteString& sAPState) {
  CPDF_Dictionary* pAPDict = m_pAnnot->GetAnnotDict()->GetDictFor("AP");

  if (!pAPDict) {
    pAPDict = new CPDF_Dictionary;
    m_pAnnot->GetAnnotDict()->SetFor("AP", pAPDict);
  }

  CPDF_Stream* pStream = nullptr;
  CPDF_Dictionary* pParentDict = nullptr;

  if (sAPState.IsEmpty()) {
    pParentDict = pAPDict;
    pStream = pAPDict->GetStreamFor(sAPType);
  } else {
    CPDF_Dictionary* pAPTypeDict = pAPDict->GetDictFor(sAPType);
    if (!pAPTypeDict) {
      pAPTypeDict = new CPDF_Dictionary;
      pAPDict->SetFor(sAPType, pAPTypeDict);
    }
    pParentDict = pAPTypeDict;
    pStream = pAPTypeDict->GetStreamFor(sAPState);
  }

  if (!pStream) {
    pStream = new CPDF_Stream(nullptr, 0, nullptr);
    CPDF_Document* pDoc = m_pPageView->GetPDFDocument();
    int32_t objnum = pDoc->AddIndirectObject(pStream);
    pParentDict->SetReferenceFor(sAPType, pDoc, objnum);
  }

  CPDF_Dictionary* pStreamDict = pStream->GetDict();
  if (!pStreamDict) {
    pStreamDict = new CPDF_Dictionary;
    pStreamDict->SetNameFor("Type", "XObject");
    pStreamDict->SetNameFor("Subtype", "Form");
    pStreamDict->SetIntegerFor("FormType", 1);
    pStream->InitStream(nullptr, 0, pStreamDict);
  }

  if (pStreamDict) {
    pStreamDict->SetMatrixFor("Matrix", matrix);
    pStreamDict->SetRectFor("BBox", rcBBox);
  }

  pStream->SetData((uint8_t*)sContents.c_str(), sContents.GetLength(), FALSE,
                   FALSE);
}

FX_BOOL CPDFSDK_BAAnnot::IsVisible() const {
  uint32_t nFlags = GetFlags();
  return !((nFlags & ANNOTFLAG_INVISIBLE) || (nFlags & ANNOTFLAG_HIDDEN) ||
           (nFlags & ANNOTFLAG_NOVIEW));
}

CPDF_Action CPDFSDK_BAAnnot::GetAction() const {
  return CPDF_Action(m_pAnnot->GetAnnotDict()->GetDictFor("A"));
}

void CPDFSDK_BAAnnot::SetAction(const CPDF_Action& action) {
  ASSERT(action.GetDict());
  if (action.GetDict() != m_pAnnot->GetAnnotDict()->GetDictFor("A")) {
    CPDF_Document* pDoc = m_pPageView->GetPDFDocument();
    CPDF_Dictionary* pDict = action.GetDict();
    if (pDict && pDict->GetObjNum() == 0) {
      pDoc->AddIndirectObject(pDict);
    }
    m_pAnnot->GetAnnotDict()->SetReferenceFor("A", pDoc, pDict->GetObjNum());
  }
}

void CPDFSDK_BAAnnot::RemoveAction() {
  m_pAnnot->GetAnnotDict()->RemoveFor("A");
}

CPDF_AAction CPDFSDK_BAAnnot::GetAAction() const {
  return CPDF_AAction(m_pAnnot->GetAnnotDict()->GetDictFor("AA"));
}

void CPDFSDK_BAAnnot::SetAAction(const CPDF_AAction& aa) {
  if (aa.GetDict() != m_pAnnot->GetAnnotDict()->GetDictFor("AA"))
    m_pAnnot->GetAnnotDict()->SetFor("AA", aa.GetDict());
}

void CPDFSDK_BAAnnot::RemoveAAction() {
  m_pAnnot->GetAnnotDict()->RemoveFor("AA");
}

CPDF_Action CPDFSDK_BAAnnot::GetAAction(CPDF_AAction::AActionType eAAT) {
  CPDF_AAction AAction = GetAAction();

  if (AAction.ActionExist(eAAT))
    return AAction.GetAction(eAAT);

  if (eAAT == CPDF_AAction::ButtonUp)
    return GetAction();

  return CPDF_Action();
}

void CPDFSDK_BAAnnot::Annot_OnDraw(CFX_RenderDevice* pDevice,
                                   CFX_Matrix* pUser2Device,
                                   CPDF_RenderOptions* pOptions) {
  m_pAnnot->GetAPForm(m_pPageView->GetPDFPage(), CPDF_Annot::Normal);
  m_pAnnot->DrawAppearance(m_pPageView->GetPDFPage(), pDevice, pUser2Device,
                           CPDF_Annot::Normal, nullptr);
}

void CPDFSDK_BAAnnot::SetOpenState(bool bOpenState) {
  if (CPDF_Annot* pAnnot = m_pAnnot->GetPopupAnnot())
    pAnnot->SetOpenState(bOpenState);
}
