// Copyright 2016 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FXBARCODE_CBC_CODE39_H_
#define FXBARCODE_CBC_CODE39_H_

#include "core/fxcrt/widestring.h"
#include "fxbarcode/BC_Library.h"
#include "fxbarcode/cbc_onecode.h"

class CBC_OnedCode39Writer;
class CFX_Matrix;
class CFX_RenderDevice;

class CBC_Code39 final : public CBC_OneCode {
 public:
  CBC_Code39();
  ~CBC_Code39() override;

  // CBC_OneCode:
  BC_TYPE GetType() override;
  bool Encode(WideStringView contents) override;
  bool RenderDevice(CFX_RenderDevice* device,
                    const CFX_Matrix& matrix) override;

 private:
  CBC_OnedCode39Writer* GetOnedCode39Writer();

  WideString render_contents_;
};

#endif  // FXBARCODE_CBC_CODE39_H_
