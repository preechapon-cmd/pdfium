// Copyright 2017 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXCRT_CSS_CFX_CSSSTYLESELECTOR_H_
#define CORE_FXCRT_CSS_CFX_CSSSTYLESELECTOR_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "core/fxcrt/css/cfx_css.h"
#include "core/fxcrt/css/cfx_cssrulecollection.h"
#include "core/fxcrt/css/cfx_cssvalue.h"
#include "core/fxcrt/mask.h"
#include "core/fxcrt/retain_ptr.h"

class CFX_CSSComputedStyle;
class CFX_CSSCustomProperty;
class CFX_CSSDeclaration;
class CFX_CSSPropertyHolder;
class CFX_CSSSelector;
class CFX_CSSStyleSheet;
class CFX_CSSValueList;

class CFX_CSSStyleSelector {
 public:
  CFX_CSSStyleSelector();
  ~CFX_CSSStyleSelector();

  void SetDefaultFontSize(float fFontSize);
  void SetUAStyleSheet(std::unique_ptr<CFX_CSSStyleSheet> pSheet);
  void UpdateStyleIndex();

  RetainPtr<CFX_CSSComputedStyle> CreateComputedStyle(
      const CFX_CSSComputedStyle* pParentStyle);

  // Note, the dest style has to be an out param because the CXFA_TextParser
  // adds non-inherited data from the parent style. Attempting to copy
  // internally will fail as you'll lose the non-inherited data.
  void ComputeStyle(const std::vector<const CFX_CSSDeclaration*>& declArray,
                    const WideString& styleString,
                    const WideString& alignString,
                    CFX_CSSComputedStyle* pDestStyle);

  std::vector<const CFX_CSSDeclaration*> MatchDeclarations(
      const WideString& tagname);

 private:
  bool MatchSelector(const WideString& tagname, CFX_CSSSelector* pSel);

  void AppendInlineStyle(CFX_CSSDeclaration* pDecl, const WideString& style);
  void ApplyDeclarations(
      const std::vector<const CFX_CSSDeclaration*>& declArray,
      const CFX_CSSDeclaration* extraDecl,
      CFX_CSSComputedStyle* pDestStyle);
  void ApplyProperty(CFX_CSSProperty eProperty,
                     const RetainPtr<CFX_CSSValue>& pValue,
                     CFX_CSSComputedStyle* pComputedStyle);
  void ExtractValues(const CFX_CSSDeclaration* decl,
                     std::vector<const CFX_CSSPropertyHolder*>* importants,
                     std::vector<const CFX_CSSPropertyHolder*>* normals,
                     std::vector<const CFX_CSSCustomProperty*>* custom);

  bool SetLengthWithPercent(CFX_CSSLength& width,
                            CFX_CSSValue::PrimitiveType eType,
                            const RetainPtr<CFX_CSSValue>& pValue,
                            float fFontSize);
  float ToFontSize(CFX_CSSPropertyValue eValue, float fCurFontSize);
  CFX_CSSDisplay ToDisplay(CFX_CSSPropertyValue eValue);
  CFX_CSSTextAlign ToTextAlign(CFX_CSSPropertyValue eValue);
  uint16_t ToFontWeight(CFX_CSSPropertyValue eValue);
  CFX_CSSFontStyle ToFontStyle(CFX_CSSPropertyValue eValue);
  CFX_CSSVerticalAlign ToVerticalAlign(CFX_CSSPropertyValue eValue);
  Mask<CFX_CSSTEXTDECORATION> ToTextDecoration(
      const RetainPtr<CFX_CSSValueList>& pList);
  CFX_CSSFontVariant ToFontVariant(CFX_CSSPropertyValue eValue);

  float default_font_size_ = 12.0f;
  std::unique_ptr<CFX_CSSStyleSheet> ua_styles_;
  CFX_CSSRuleCollection ua_rules_;
};

#endif  // CORE_FXCRT_CSS_CFX_CSSSTYLESELECTOR_H_
