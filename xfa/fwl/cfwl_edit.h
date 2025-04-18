// Copyright 2014 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FWL_CFWL_EDIT_H_
#define XFA_FWL_CFWL_EDIT_H_

#include <memory>
#include <utility>

#include "xfa/fde/cfde_texteditengine.h"
#include "xfa/fgas/graphics/cfgas_gepath.h"
#include "xfa/fwl/cfwl_event.h"
#include "xfa/fwl/cfwl_scrollbar.h"
#include "xfa/fwl/cfwl_widget.h"

class CFX_RenderDevice;

namespace pdfium {

#define FWL_STYLEEXT_EDT_ReadOnly (1L << 0)
#define FWL_STYLEEXT_EDT_MultiLine (1L << 1)
#define FWL_STYLEEXT_EDT_WantReturn (1L << 2)
#define FWL_STYLEEXT_EDT_AutoHScroll (1L << 4)
#define FWL_STYLEEXT_EDT_AutoVScroll (1L << 5)
#define FWL_STYLEEXT_EDT_Validate (1L << 7)
#define FWL_STYLEEXT_EDT_Password (1L << 8)
#define FWL_STYLEEXT_EDT_Number (1L << 9)
#define FWL_STYLEEXT_EDT_CombText (1L << 17)
#define FWL_STYLEEXT_EDT_HNear 0
#define FWL_STYLEEXT_EDT_HCenter (1L << 18)
#define FWL_STYLEEXT_EDT_HFar (2L << 18)
#define FWL_STYLEEXT_EDT_VNear 0
#define FWL_STYLEEXT_EDT_VCenter (1L << 20)
#define FWL_STYLEEXT_EDT_VFar (2L << 20)
#define FWL_STYLEEXT_EDT_Justified (1L << 22)
#define FWL_STYLEEXT_EDT_HAlignMask (3L << 18)
#define FWL_STYLEEXT_EDT_VAlignMask (3L << 20)
#define FWL_STYLEEXT_EDT_HAlignModeMask (3L << 22)
#define FWL_STYLEEXT_EDT_ShowScrollbarFocus (1L << 25)
#define FWL_STYLEEXT_EDT_OuterScrollbar (1L << 26)

class CFWL_MessageKey;
class CFWL_MessageMouse;
class CFWL_Caret;

class CFWL_Edit : public CFWL_Widget, public CFDE_TextEditEngine::Delegate {
 public:
  CONSTRUCT_VIA_MAKE_GARBAGE_COLLECTED;
  ~CFWL_Edit() override;

  // CFWL_Widget:
  void PreFinalize() override;
  void Trace(cppgc::Visitor* visitor) const override;
  FWL_Type GetClassID() const override;
  CFX_RectF GetAutosizedWidgetRect() override;
  CFX_RectF GetWidgetRect() override;
  void Update() override;
  FWL_WidgetHit HitTest(const CFX_PointF& point) override;
  void SetStates(uint32_t dwStates) override;
  void DrawWidget(CFGAS_GEGraphics* pGraphics,
                  const CFX_Matrix& matrix) override;
  void OnProcessMessage(CFWL_Message* pMessage) override;
  void OnProcessEvent(CFWL_Event* pEvent) override;
  void OnDrawWidget(CFGAS_GEGraphics* pGraphics,
                    const CFX_Matrix& matrix) override;

  virtual void SetText(const WideString& wsText);
  virtual void SetTextSkipNotify(const WideString& wsText);

  size_t GetTextLength() const;
  WideString GetText() const;
  void ClearText();

  void SelectAll();
  void ClearSelection();
  bool HasSelection() const;
  // Returns <start, count> of the selection.
  std::pair<size_t, size_t> GetSelection() const;

  int32_t GetLimit() const;
  void SetLimit(int32_t nLimit);
  void SetAliasChar(wchar_t wAlias);
  std::optional<WideString> Copy();
  std::optional<WideString> Cut();
  bool Paste(const WideString& wsPaste);
  bool Undo();
  bool Redo();
  bool CanUndo();
  bool CanRedo();

  // CFDE_TextEditEngine::Delegate
  void NotifyTextFull() override;
  void OnCaretChanged() override;
  void OnTextWillChange(CFDE_TextEditEngine::TextChange* change) override;
  void OnTextChanged() override;
  void OnSelChanged() override;
  bool OnValidate(const WideString& wsText) override;
  void SetScrollOffset(float fScrollOffset) override;

 protected:
  CFWL_Edit(CFWL_App* app, const Properties& properties, CFWL_Widget* pOuter);

  void ShowCaret(CFX_RectF* pRect);
  void HideCaret(CFX_RectF* pRect);
  const CFX_RectF& GetRTClient() const { return client_rect_; }
  CFDE_TextEditEngine* GetTxtEdtEngine() { return edit_engine_.get(); }

 private:
  void RenderText(CFX_RenderDevice* pRenderDev,
                  const CFX_RectF& clipRect,
                  const CFX_Matrix& mt);
  void DrawContent(CFGAS_GEGraphics* pGraphics, const CFX_Matrix& mtMatrix);
  void DrawContentNonComb(CFGAS_GEGraphics* pGraphics,
                          const CFX_Matrix& mtMatrix);

  void UpdateEditEngine();
  void UpdateEditParams();
  void UpdateEditLayout();
  bool UpdateOffset();
  bool UpdateOffset(CFWL_ScrollBar* pScrollBar, float fPosChanged);
  void UpdateVAlignment();
  void UpdateCaret();
  CFWL_ScrollBar* UpdateScroll();
  void Layout();
  void LayoutScrollBar();
  CFX_PointF DeviceToEngine(const CFX_PointF& pt);
  void InitVerticalScrollBar();
  void InitEngine();
  void InitCaret();
  bool IsShowVertScrollBar() const;
  bool IsContentHeightOverflow() const;
  void SetCursorPosition(size_t position);
  void UpdateCursorRect();

  void DoRButtonDown(CFWL_MessageMouse* pMsg);
  void OnFocusGained();
  void OnFocusLost();
  void OnLButtonDown(CFWL_MessageMouse* pMsg);
  void OnLButtonUp(CFWL_MessageMouse* pMsg);
  void OnButtonDoubleClick(CFWL_MessageMouse* pMsg);
  void OnMouseMove(CFWL_MessageMouse* pMsg);
  void OnKeyDown(CFWL_MessageKey* pMsg);
  void OnChar(CFWL_MessageKey* pMsg);
  bool OnScroll(CFWL_ScrollBar* pScrollBar,
                CFWL_EventScroll::Code dwCode,
                float fPos);

  CFX_RectF client_rect_;
  CFX_RectF engine_rect_;
  CFX_RectF static_rect_;
  CFX_RectF caret_rect_;
  bool lbutton_down_ = false;
  int32_t limit_ = -1;
  float valign_offset_ = 0.0f;
  float scroll_offset_x_ = 0.0f;
  float scroll_offset_y_ = 0.0f;
  float font_size_ = 0.0f;
  size_t cursor_position_ = 0;
  std::unique_ptr<CFDE_TextEditEngine> const edit_engine_;
  cppgc::Member<CFWL_ScrollBar> vert_scroll_bar_;
  cppgc::Member<CFWL_Caret> caret_;
  WideString cache_;
  WideString font_;
};

}  // namespace pdfium

// TODO(crbug.com/42271761): Remove.
using pdfium::CFWL_Edit;

#endif  // XFA_FWL_CFWL_EDIT_H_
