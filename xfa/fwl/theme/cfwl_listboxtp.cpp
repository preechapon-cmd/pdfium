// Copyright 2014 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fwl/theme/cfwl_listboxtp.h"

#include "build/build_config.h"
#include "xfa/fgas/graphics/cfgas_gecolor.h"
#include "xfa/fgas/graphics/cfgas_gegraphics.h"
#include "xfa/fgas/graphics/cfgas_gepath.h"
#include "xfa/fwl/cfwl_listbox.h"
#include "xfa/fwl/cfwl_themebackground.h"
#include "xfa/fwl/cfwl_widget.h"

namespace pdfium {

CFWL_ListBoxTP::CFWL_ListBoxTP() = default;

CFWL_ListBoxTP::~CFWL_ListBoxTP() = default;

void CFWL_ListBoxTP::DrawBackground(const CFWL_ThemeBackground& pParams) {
  switch (pParams.GetPart()) {
    case CFWL_ThemePart::Part::kBorder: {
      DrawBorder(pParams.GetGraphics(), pParams.part_rect_, pParams.matrix_);
      break;
    }
    case CFWL_ThemePart::Part::kBackground: {
      FillSolidRect(pParams.GetGraphics(), ArgbEncode(255, 255, 255, 255),
                    pParams.part_rect_, pParams.matrix_);
      if (pParams.data_rect_) {
        FillSolidRect(pParams.GetGraphics(), FWLTHEME_COLOR_Background,
                      *pParams.data_rect_, pParams.matrix_);
      }
      break;
    }
    case CFWL_ThemePart::Part::kListItem: {
      DrawListBoxItem(pParams.GetGraphics(), pParams.states_,
                      pParams.part_rect_, pParams.data_rect_, pParams.matrix_);
      break;
    }
    case CFWL_ThemePart::Part::kCheck: {
      uint32_t color = 0xFF000000;
      if (pParams.states_ == CFWL_PartState::kChecked) {
        color = 0xFFFF0000;
      } else if (pParams.states_ == CFWL_PartState::kNormal) {
        color = 0xFF0000FF;
      }
      FillSolidRect(pParams.GetGraphics(), color, pParams.part_rect_,
                    pParams.matrix_);
      break;
    }
    default:
      break;
  }
}

void CFWL_ListBoxTP::DrawListBoxItem(CFGAS_GEGraphics* pGraphics,
                                     Mask<CFWL_PartState> dwStates,
                                     const CFX_RectF& rtItem,
                                     const CFX_RectF* pData,
                                     const CFX_Matrix& matrix) {
  if (dwStates & CFWL_PartState::kSelected) {
    CFGAS_GEGraphics::StateRestorer restorer(pGraphics);
    pGraphics->SetFillColor(CFGAS_GEColor(FWLTHEME_COLOR_BKSelected));
    CFGAS_GEPath path;
#if BUILDFLAG(IS_APPLE)
    path.AddRectangle(rtItem.left, rtItem.top, rtItem.width - 1,
                      rtItem.height - 1);
#else
    path.AddRectangle(rtItem.left, rtItem.top, rtItem.width, rtItem.height);
#endif
    pGraphics->FillPath(path, CFX_FillRenderOptions::FillType::kWinding,
                        matrix);
  }
  if ((dwStates & CFWL_PartState::kFocused) && pData) {
    DrawFocus(pGraphics, *pData, matrix);
  }
}

}  // namespace pdfium
