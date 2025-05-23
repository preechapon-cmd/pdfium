// Copyright 2017 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fxjs/xfa/cjx_object.h"

#include <set>
#include <tuple>
#include <utility>

#include "core/fxcrt/check.h"
#include "core/fxcrt/check_op.h"
#include "core/fxcrt/containers/contains.h"
#include "core/fxcrt/fx_extension.h"
#include "core/fxcrt/fx_memory.h"
#include "core/fxcrt/span.h"
#include "core/fxcrt/xml/cfx_xmlelement.h"
#include "core/fxcrt/xml/cfx_xmltext.h"
#include "fxjs/cjs_result.h"
#include "fxjs/fxv8.h"
#include "fxjs/gc/container_trace.h"
#include "fxjs/xfa/cfxjse_engine.h"
#include "fxjs/xfa/cfxjse_mapmodule.h"
#include "fxjs/xfa/cjx_boolean.h"
#include "fxjs/xfa/cjx_draw.h"
#include "fxjs/xfa/cjx_field.h"
#include "fxjs/xfa/cjx_instancemanager.h"
#include "v8/include/v8-forward.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-primitive.h"
#include "xfa/fgas/crt/cfgas_decimal.h"
#include "xfa/fgas/graphics/cfgas_gecolor.h"
#include "xfa/fxfa/cxfa_ffnotify.h"
#include "xfa/fxfa/cxfa_ffwidget.h"
#include "xfa/fxfa/parser/cxfa_border.h"
#include "xfa/fxfa/parser/cxfa_datavalue.h"
#include "xfa/fxfa/parser/cxfa_document.h"
#include "xfa/fxfa/parser/cxfa_edge.h"
#include "xfa/fxfa/parser/cxfa_fill.h"
#include "xfa/fxfa/parser/cxfa_font.h"
#include "xfa/fxfa/parser/cxfa_measurement.h"
#include "xfa/fxfa/parser/cxfa_node.h"
#include "xfa/fxfa/parser/cxfa_object.h"
#include "xfa/fxfa/parser/cxfa_occur.h"
#include "xfa/fxfa/parser/cxfa_proto.h"
#include "xfa/fxfa/parser/cxfa_subform.h"
#include "xfa/fxfa/parser/cxfa_validate.h"
#include "xfa/fxfa/parser/cxfa_value.h"
#include "xfa/fxfa/parser/xfa_basic_data.h"
#include "xfa/fxfa/parser/xfa_utils.h"

namespace {

enum XFA_KEYTYPE {
  XFA_KEYTYPE_Custom,
  XFA_KEYTYPE_Element,
};

uint32_t GetMapKey_Custom(WideStringView wsKey) {
  uint32_t dwKey = FX_HashCode_GetW(wsKey);
  return ((dwKey << 1) | XFA_KEYTYPE_Custom);
}

uint32_t GetMapKey_Element(XFA_Element eType, XFA_Attribute eAttribute) {
  return ((static_cast<uint32_t>(eType) << 16) |
          (static_cast<uint32_t>(eAttribute) << 8) | XFA_KEYTYPE_Element);
}

std::tuple<int32_t, int32_t, int32_t> StrToRGB(const WideString& strRGB) {
  int32_t r = 0;
  int32_t g = 0;
  int32_t b = 0;

  size_t iIndex = 0;
  for (size_t i = 0; i < strRGB.GetLength(); ++i) {
    wchar_t ch = strRGB[i];
    if (ch == L',') {
      ++iIndex;
    }
    if (iIndex > 2) {
      break;
    }

    int32_t iValue = ch - L'0';
    if (iValue >= 0 && iValue <= 9) {
      switch (iIndex) {
        case 0:
          r = r * 10 + iValue;
          break;
        case 1:
          g = g * 10 + iValue;
          break;
        default:
          b = b * 10 + iValue;
          break;
      }
    }
  }
  return {r, g, b};
}

v8::Local<v8::String> ColorToV8String(v8::Isolate* isolate, FX_ARGB color) {
  return fxv8::NewStringHelper(
      isolate, CFGAS_GEColor::ColorToString(color).AsStringView());
}

}  // namespace

CJX_Object::CJX_Object(CXFA_Object* obj) : object_(obj) {}

CJX_Object::~CJX_Object() = default;

CJX_Object* CJX_Object::AsCJXObject() {
  return this;
}

void CJX_Object::Trace(cppgc::Visitor* visitor) const {
  visitor->Trace(object_);
  visitor->Trace(layout_item_);
  visitor->Trace(calc_data_);
}

bool CJX_Object::DynamicTypeIs(TypeTag eType) const {
  return eType == static_type__;
}

void CJX_Object::DefineMethods(pdfium::span<const CJX_MethodSpec> methods) {
  for (const auto& item : methods) {
    method_specs_[item.pName] = item.pMethodCall;
  }
}

CXFA_Document* CJX_Object::GetDocument() const {
  return object_->GetDocument();
}

CXFA_Node* CJX_Object::GetXFANode() const {
  return ToNode(GetXFAObject());
}

void CJX_Object::className(v8::Isolate* pIsolate,
                           v8::Local<v8::Value>* pValue,
                           bool bSetting,
                           XFA_Attribute eAttribute) {
  if (bSetting) {
    ThrowInvalidPropertyException(pIsolate);
    return;
  }
  *pValue = fxv8::NewStringHelper(pIsolate, GetXFAObject()->GetClassName());
}

int32_t CJX_Object::Subform_and_SubformSet_InstanceIndex() {
  int32_t index = 0;
  for (CXFA_Node* pNode = GetXFANode()->GetPrevSibling(); pNode;
       pNode = pNode->GetPrevSibling()) {
    if ((pNode->GetElementType() != XFA_Element::Subform) &&
        (pNode->GetElementType() != XFA_Element::SubformSet)) {
      break;
    }
    index++;
  }
  return index;
}

bool CJX_Object::HasMethod(const WideString& func) const {
  return pdfium::Contains(method_specs_, func.ToUTF8());
}

CJS_Result CJX_Object::RunMethod(CFXJSE_Engine* pScriptContext,
                                 const WideString& func,
                                 pdfium::span<v8::Local<v8::Value>> params) {
  auto it = method_specs_.find(func.ToUTF8());
  if (it == method_specs_.end()) {
    return CJS_Result::Failure(JSMessage::kUnknownMethod);
  }

  return it->second(this, pScriptContext, params);
}

void CJX_Object::ThrowTooManyOccurrencesException(v8::Isolate* pIsolate,
                                                  const WideString& obj) const {
  ThrowException(
      pIsolate, WideString::FromASCII("The element [") + obj +
                    WideString::FromASCII(
                        "] has violated its allowable number of occurrences."));
}

void CJX_Object::ThrowInvalidPropertyException(v8::Isolate* pIsolate) const {
  ThrowException(pIsolate,
                 WideString::FromASCII("Invalid property set operation."));
}

void CJX_Object::ThrowIndexOutOfBoundsException(v8::Isolate* pIsolate) const {
  ThrowException(pIsolate,
                 WideString::FromASCII("Index value is out of bounds."));
}

void CJX_Object::ThrowParamCountMismatchException(
    v8::Isolate* pIsolate,
    const WideString& method) const {
  ThrowException(
      pIsolate,
      WideString::FromASCII("Incorrect number of parameters calling method '") +
          method + WideString::FromASCII("'."));
}

void CJX_Object::ThrowArgumentMismatchException(v8::Isolate* pIsolate) const {
  ThrowException(pIsolate,
                 WideString::FromASCII(
                     "Argument mismatch in property or function argument."));
}

void CJX_Object::ThrowException(v8::Isolate* pIsolate,
                                const WideString& str) const {
  DCHECK(!str.IsEmpty());
  FXJSE_ThrowMessage(pIsolate, str.ToUTF8().AsStringView());
}

bool CJX_Object::HasAttribute(XFA_Attribute eAttr) const {
  uint32_t key = GetMapKey_Element(GetXFAObject()->GetElementType(), eAttr);
  return HasMapModuleKey(key);
}

void CJX_Object::SetAttributeByEnum(XFA_Attribute eAttr,
                                    const WideString& wsValue,
                                    bool bNotify) {
  switch (GetXFANode()->GetAttributeType(eAttr)) {
    case XFA_AttributeType::Enum: {
      std::optional<XFA_AttributeValue> item =
          XFA_GetAttributeValueByName(wsValue.AsStringView());
      SetEnum(eAttr,
              item.has_value() ? item.value()
                               : GetXFANode()->GetDefaultEnum(eAttr).value(),
              bNotify);
      break;
    }
    case XFA_AttributeType::CData:
      SetCDataImpl(eAttr, WideString(wsValue), bNotify, false);
      break;
    case XFA_AttributeType::Boolean:
      SetBoolean(eAttr, !wsValue.EqualsASCII("0"), bNotify);
      break;
    case XFA_AttributeType::Integer:
      SetInteger(eAttr, FXSYS_roundf(StringToFloat(wsValue.AsStringView())),
                 bNotify);
      break;
    case XFA_AttributeType::Measure:
      SetMeasure(eAttr, CXFA_Measurement(wsValue.AsStringView()), bNotify);
      break;
  }
}

void CJX_Object::SetAttributeByString(WideStringView wsAttr,
                                      const WideString& wsValue) {
  std::optional<XFA_ATTRIBUTEINFO> attr = XFA_GetAttributeByName(wsAttr);
  if (attr.has_value()) {
    SetAttributeByEnum(attr.value().attribute, wsValue, true);
    return;
  }
  uint32_t key = GetMapKey_Custom(wsAttr);
  SetMapModuleString(key, wsValue);
}

WideString CJX_Object::GetAttributeByString(WideStringView attr) const {
  std::optional<WideString> result;
  std::optional<XFA_ATTRIBUTEINFO> enum_attr = XFA_GetAttributeByName(attr);
  if (enum_attr.has_value()) {
    result = TryAttribute(enum_attr.value().attribute, true);
  } else {
    result = GetMapModuleStringFollowingChain(GetMapKey_Custom(attr));
  }
  return result.value_or(WideString());
}

WideString CJX_Object::GetAttributeByEnum(XFA_Attribute attr) const {
  return TryAttribute(attr, true).value_or(WideString());
}

std::optional<WideString> CJX_Object::TryAttribute(XFA_Attribute eAttr,
                                                   bool bUseDefault) const {
  switch (GetXFANode()->GetAttributeType(eAttr)) {
    case XFA_AttributeType::Enum: {
      std::optional<XFA_AttributeValue> value = TryEnum(eAttr, bUseDefault);
      if (!value.has_value()) {
        return std::nullopt;
      }
      return WideString::FromASCII(XFA_AttributeValueToName(value.value()));
    }
    case XFA_AttributeType::CData:
      return TryCData(eAttr, bUseDefault);

    case XFA_AttributeType::Boolean: {
      std::optional<bool> value = TryBoolean(eAttr, bUseDefault);
      if (!value.has_value()) {
        return std::nullopt;
      }
      return WideString(value.value() ? L"1" : L"0");
    }
    case XFA_AttributeType::Integer: {
      std::optional<int32_t> iValue = TryInteger(eAttr, bUseDefault);
      if (!iValue.has_value()) {
        return std::nullopt;
      }
      return WideString::FormatInteger(iValue.value());
    }
    case XFA_AttributeType::Measure: {
      std::optional<CXFA_Measurement> value = TryMeasure(eAttr, bUseDefault);
      if (!value.has_value()) {
        return std::nullopt;
      }
      return value->ToString();
    }
  }
  return std::nullopt;
}

void CJX_Object::RemoveAttribute(WideStringView wsAttr) {
  RemoveMapModuleKey(GetMapKey_Custom(wsAttr));
}

std::optional<bool> CJX_Object::TryBoolean(XFA_Attribute eAttr,
                                           bool bUseDefault) const {
  uint32_t key = GetMapKey_Element(GetXFAObject()->GetElementType(), eAttr);
  std::optional<int32_t> value = GetMapModuleValueFollowingChain(key);
  if (value.has_value()) {
    return !!value.value();
  }
  if (!bUseDefault) {
    return std::nullopt;
  }
  return GetXFANode()->GetDefaultBoolean(eAttr);
}

void CJX_Object::SetBoolean(XFA_Attribute eAttr, bool bValue, bool bNotify) {
  CFX_XMLElement* elem = SetValue(eAttr, static_cast<int32_t>(bValue), bNotify);
  if (elem) {
    elem->SetAttribute(WideString::FromASCII(XFA_AttributeToName(eAttr)),
                       bValue ? L"1" : L"0");
  }
}

bool CJX_Object::GetBoolean(XFA_Attribute eAttr) const {
  return TryBoolean(eAttr, true).value_or(false);
}

void CJX_Object::SetInteger(XFA_Attribute eAttr, int32_t iValue, bool bNotify) {
  CFX_XMLElement* elem = SetValue(eAttr, iValue, bNotify);
  if (elem) {
    elem->SetAttribute(WideString::FromASCII(XFA_AttributeToName(eAttr)),
                       WideString::FormatInteger(iValue));
  }
}

int32_t CJX_Object::GetInteger(XFA_Attribute eAttr) const {
  return TryInteger(eAttr, true).value_or(0);
}

std::optional<int32_t> CJX_Object::TryInteger(XFA_Attribute eAttr,
                                              bool bUseDefault) const {
  uint32_t key = GetMapKey_Element(GetXFAObject()->GetElementType(), eAttr);
  std::optional<int32_t> value = GetMapModuleValueFollowingChain(key);
  if (value.has_value()) {
    return value.value();
  }
  if (!bUseDefault) {
    return std::nullopt;
  }
  return GetXFANode()->GetDefaultInteger(eAttr);
}

std::optional<XFA_AttributeValue> CJX_Object::TryEnum(XFA_Attribute eAttr,
                                                      bool bUseDefault) const {
  uint32_t key = GetMapKey_Element(GetXFAObject()->GetElementType(), eAttr);
  std::optional<int32_t> value = GetMapModuleValueFollowingChain(key);
  if (value.has_value()) {
    return static_cast<XFA_AttributeValue>(value.value());
  }
  if (!bUseDefault) {
    return std::nullopt;
  }
  return GetXFANode()->GetDefaultEnum(eAttr);
}

void CJX_Object::SetEnum(XFA_Attribute eAttr,
                         XFA_AttributeValue eValue,
                         bool bNotify) {
  CFX_XMLElement* elem = SetValue(eAttr, static_cast<int32_t>(eValue), bNotify);
  if (elem) {
    elem->SetAttribute(WideString::FromASCII(XFA_AttributeToName(eAttr)),
                       WideString::FromASCII(XFA_AttributeValueToName(eValue)));
  }
}

XFA_AttributeValue CJX_Object::GetEnum(XFA_Attribute eAttr) const {
  return TryEnum(eAttr, true).value_or(XFA_AttributeValue::Unknown);
}

void CJX_Object::SetMeasure(XFA_Attribute eAttr,
                            const CXFA_Measurement& mValue,
                            bool bNotify) {
  // Can't short-circuit update here when the value is the same since it
  // might have come from further up the chain from where we are setting it.
  uint32_t key = GetMapKey_Element(GetXFAObject()->GetElementType(), eAttr);
  if (bNotify) {
    OnChanging(eAttr);
  }
  SetMapModuleMeasurement(key, mValue);
  if (bNotify) {
    OnChanged(eAttr, false);
  }
}

std::optional<CXFA_Measurement> CJX_Object::TryMeasure(XFA_Attribute eAttr,
                                                       bool bUseDefault) const {
  uint32_t key = GetMapKey_Element(GetXFAObject()->GetElementType(), eAttr);
  std::optional<CXFA_Measurement> result =
      GetMapModuleMeasurementFollowingChain(key);
  if (result.has_value()) {
    return result.value();
  }
  if (!bUseDefault) {
    return std::nullopt;
  }
  return GetXFANode()->GetDefaultMeasurement(eAttr);
}

std::optional<float> CJX_Object::TryMeasureAsFloat(XFA_Attribute attr) const {
  std::optional<CXFA_Measurement> measure = TryMeasure(attr, false);
  if (!measure.has_value()) {
    return std::nullopt;
  }
  return measure->ToUnit(XFA_Unit::Pt);
}

CXFA_Measurement CJX_Object::GetMeasure(XFA_Attribute eAttr) const {
  return TryMeasure(eAttr, true).value_or(CXFA_Measurement());
}

float CJX_Object::GetMeasureInUnit(XFA_Attribute eAttr, XFA_Unit unit) const {
  return GetMeasure(eAttr).ToUnit(unit);
}

WideString CJX_Object::GetCData(XFA_Attribute eAttr) const {
  return TryCData(eAttr, true).value_or(WideString());
}

void CJX_Object::SetCData(XFA_Attribute eAttr, const WideString& wsValue) {
  return SetCDataImpl(eAttr, wsValue, false, false);
}

void CJX_Object::SetCDataImpl(XFA_Attribute eAttr,
                              const WideString& wsValue,
                              bool bNotify,
                              bool bScriptModify) {
  CXFA_Node* xfaObj = GetXFANode();
  uint32_t key = GetMapKey_Element(xfaObj->GetElementType(), eAttr);
  std::optional<WideString> old_value = GetMapModuleString(key);
  if (!old_value.has_value() || old_value.value() != wsValue) {
    if (bNotify) {
      OnChanging(eAttr);
    }
    SetMapModuleString(key, wsValue);
    if (eAttr == XFA_Attribute::Name) {
      xfaObj->UpdateNameHash();
    }
    if (bNotify) {
      OnChanged(eAttr, bScriptModify);
    }
  }

  if (!xfaObj->IsNeedSavingXMLNode() || eAttr == XFA_Attribute::QualifiedName ||
      eAttr == XFA_Attribute::BindingNode) {
    return;
  }

  if (eAttr == XFA_Attribute::Name &&
      (xfaObj->GetElementType() == XFA_Element::DataValue ||
       xfaObj->GetElementType() == XFA_Element::DataGroup)) {
    return;
  }

  if (eAttr == XFA_Attribute::Value) {
    xfaObj->SetToXML(wsValue);
    return;
  }

  CFX_XMLElement* elem = ToXMLElement(xfaObj->GetXMLMappingNode());
  if (!elem) {
    return;
  }

  WideString wsAttrName = WideString::FromASCII(XFA_AttributeToName(eAttr));
  if (eAttr == XFA_Attribute::ContentType) {
    wsAttrName = L"xfa:" + wsAttrName;
  }
  elem->SetAttribute(wsAttrName, wsValue);
}

void CJX_Object::SetAttributeValue(const WideString& wsValue,
                                   const WideString& wsXMLValue) {
  SetAttributeValueImpl(wsValue, wsXMLValue, false, false);
}

void CJX_Object::SetAttributeValueImpl(const WideString& wsValue,
                                       const WideString& wsXMLValue,
                                       bool bNotify,
                                       bool bScriptModify) {
  auto* xfaObj = GetXFANode();
  uint32_t key =
      GetMapKey_Element(xfaObj->GetElementType(), XFA_Attribute::Value);
  std::optional<WideString> old_value = GetMapModuleString(key);
  if (!old_value.has_value() || old_value.value() != wsValue) {
    if (bNotify) {
      OnChanging(XFA_Attribute::Value);
    }
    SetMapModuleString(key, wsValue);
    if (bNotify) {
      OnChanged(XFA_Attribute::Value, bScriptModify);
    }
    if (xfaObj->IsNeedSavingXMLNode()) {
      xfaObj->SetToXML(wsXMLValue);
    }
  }
}

std::optional<WideString> CJX_Object::TryCData(XFA_Attribute eAttr,
                                               bool bUseDefault) const {
  uint32_t key = GetMapKey_Element(GetXFAObject()->GetElementType(), eAttr);
  std::optional<WideString> value = GetMapModuleStringFollowingChain(key);
  if (value.has_value()) {
    return value;
  }

  if (!bUseDefault) {
    return std::nullopt;
  }

  return GetXFANode()->GetDefaultCData(eAttr);
}

CFX_XMLElement* CJX_Object::SetValue(XFA_Attribute eAttr,
                                     int32_t value,
                                     bool bNotify) {
  uint32_t key = GetMapKey_Element(GetXFAObject()->GetElementType(), eAttr);
  std::optional<int32_t> old_value = GetMapModuleValue(key);
  if (!old_value.has_value() || old_value.value() != value) {
    if (bNotify) {
      OnChanging(eAttr);
    }
    SetMapModuleValue(key, value);
    if (bNotify) {
      OnChanged(eAttr, false);
    }
  }
  CXFA_Node* pNode = GetXFANode();
  return pNode->IsNeedSavingXMLNode() ? ToXMLElement(pNode->GetXMLMappingNode())
                                      : nullptr;
}

void CJX_Object::SetContent(const WideString& wsContent,
                            const WideString& wsXMLValue,
                            bool bNotify,
                            bool bScriptModify,
                            bool bSyncData) {
  CXFA_Node* pNode = nullptr;
  CXFA_Node* pBindNode = nullptr;
  switch (GetXFANode()->GetObjectType()) {
    case XFA_ObjectType::ContainerNode: {
      if (XFA_FieldIsMultiListBox(GetXFANode())) {
        CXFA_Value* pValue =
            GetOrCreateProperty<CXFA_Value>(0, XFA_Element::Value);
        if (!pValue) {
          break;
        }

        CXFA_Node* pChildValue = pValue->GetFirstChild();
        pChildValue->JSObject()->SetCData(XFA_Attribute::ContentType,
                                          WideString::FromASCII("text/xml"));
        pChildValue->JSObject()->SetContent(wsContent, wsContent, bNotify,
                                            bScriptModify, false);

        CXFA_Node* pBind = GetXFANode()->GetBindData();
        if (bSyncData && pBind) {
          std::vector<WideString> wsSaveTextArray =
              fxcrt::Split(wsContent, L'\n');
          std::vector<CXFA_Node*> valueNodes =
              pBind->GetNodeListForType(XFA_Element::DataValue);

          // Adusting node count might have side effects, do not trust that
          // we'll ever actually get there.
          size_t tries = 0;
          while (valueNodes.size() != wsSaveTextArray.size()) {
            if (++tries > 4) {
              return;
            }
            if (valueNodes.size() < wsSaveTextArray.size()) {
              size_t iAddNodes = wsSaveTextArray.size() - valueNodes.size();
              while (iAddNodes-- > 0) {
                CXFA_Node* pValueNodes =
                    pBind->CreateSamePacketNode(XFA_Element::DataValue);
                pValueNodes->JSObject()->SetCData(
                    XFA_Attribute::Name, WideString::FromASCII("value"));
                pValueNodes->CreateXMLMappingNode();
                pBind->InsertChildAndNotify(pValueNodes, nullptr);
              }
            } else {
              size_t iDelNodes = valueNodes.size() - wsSaveTextArray.size();
              for (size_t i = 0; i < iDelNodes; ++i) {
                pBind->RemoveChildAndNotify(valueNodes[i], true);
              }
            }
            valueNodes = pBind->GetNodeListForType(XFA_Element::DataValue);
          }
          DCHECK_EQ(valueNodes.size(), wsSaveTextArray.size());
          size_t i = 0;
          for (CXFA_Node* pValueNode : valueNodes) {
            pValueNode->JSObject()->SetAttributeValue(wsSaveTextArray[i],
                                                      wsSaveTextArray[i]);
            i++;
          }
          for (auto* pArrayNode : pBind->GetBindItemsCopy()) {
            if (pArrayNode != GetXFANode()) {
              pArrayNode->JSObject()->SetContent(wsContent, wsContent, bNotify,
                                                 bScriptModify, false);
            }
          }
        }
        break;
      }
      if (GetXFANode()->GetElementType() == XFA_Element::ExclGroup) {
        pNode = GetXFANode();
      } else {
        CXFA_Value* pValue =
            GetOrCreateProperty<CXFA_Value>(0, XFA_Element::Value);
        if (!pValue) {
          break;
        }

        CXFA_Node* pChildValue = pValue->GetFirstChild();
        if (pChildValue) {
          pChildValue->JSObject()->SetContent(wsContent, wsContent, bNotify,
                                              bScriptModify, false);
        }
      }
      pBindNode = GetXFANode()->GetBindData();
      if (pBindNode && bSyncData) {
        pBindNode->JSObject()->SetContent(wsContent, wsXMLValue, bNotify,
                                          bScriptModify, false);
        for (auto* pArrayNode : pBindNode->GetBindItemsCopy()) {
          if (pArrayNode != GetXFANode()) {
            pArrayNode->JSObject()->SetContent(wsContent, wsContent, bNotify,
                                               true, false);
          }
        }
      }
      pBindNode = nullptr;
      break;
    }
    case XFA_ObjectType::ContentNode: {
      WideString wsContentType;
      if (GetXFANode()->GetElementType() == XFA_Element::ExData) {
        std::optional<WideString> ret =
            TryAttribute(XFA_Attribute::ContentType, false);
        if (ret.has_value()) {
          wsContentType = ret.value();
        }
        if (wsContentType.EqualsASCII("text/html")) {
          wsContentType.clear();
          SetAttributeByEnum(XFA_Attribute::ContentType, wsContentType, false);
        }
      }

      CXFA_Node* pContentRawDataNode = GetXFANode()->GetFirstChild();
      if (!pContentRawDataNode) {
        pContentRawDataNode = GetXFANode()->CreateSamePacketNode(
            wsContentType.EqualsASCII("text/xml") ? XFA_Element::Sharpxml
                                                  : XFA_Element::Sharptext);
        GetXFANode()->InsertChildAndNotify(pContentRawDataNode, nullptr);
      }
      pContentRawDataNode->JSObject()->SetContent(
          wsContent, wsXMLValue, bNotify, bScriptModify, bSyncData);
      return;
    }
    case XFA_ObjectType::NodeC:
    case XFA_ObjectType::TextNode:
      pNode = GetXFANode();
      break;
    case XFA_ObjectType::NodeV:
      pNode = GetXFANode();
      if (bSyncData && GetXFANode()->GetPacketType() == XFA_PacketType::Form) {
        CXFA_Node* pParent = GetXFANode()->GetParent();
        if (pParent) {
          pParent = pParent->GetParent();
        }
        if (pParent && pParent->GetElementType() == XFA_Element::Value) {
          pParent = pParent->GetParent();
          if (pParent && pParent->IsContainerNode()) {
            pBindNode = pParent->GetBindData();
            if (pBindNode) {
              pBindNode->JSObject()->SetContent(wsContent, wsXMLValue, bNotify,
                                                bScriptModify, false);
            }
          }
        }
      }
      break;
    default:
      if (GetXFANode()->GetElementType() == XFA_Element::DataValue) {
        pNode = GetXFANode();
        pBindNode = GetXFANode();
      }
      break;
  }
  if (!pNode) {
    return;
  }

  SetAttributeValueImpl(wsContent, wsXMLValue, bNotify, bScriptModify);
  if (pBindNode && bSyncData) {
    for (auto* pArrayNode : pBindNode->GetBindItemsCopy()) {
      pArrayNode->JSObject()->SetContent(wsContent, wsContent, bNotify,
                                         bScriptModify, false);
    }
  }
}

WideString CJX_Object::GetContent(bool bScriptModify) const {
  return TryContent(bScriptModify, true).value_or(WideString());
}

std::optional<WideString> CJX_Object::TryContent(bool bScriptModify,
                                                 bool bProto) const {
  CXFA_Node* pNode = nullptr;
  switch (GetXFANode()->GetObjectType()) {
    case XFA_ObjectType::ContainerNode:
      if (GetXFANode()->GetElementType() == XFA_Element::ExclGroup) {
        pNode = GetXFANode();
      } else {
        CXFA_Value* pValue =
            GetXFANode()->GetChild<CXFA_Value>(0, XFA_Element::Value, false);
        if (!pValue) {
          return std::nullopt;
        }

        CXFA_Node* pChildValue = pValue->GetFirstChild();
        if (pChildValue && XFA_FieldIsMultiListBox(GetXFANode())) {
          pChildValue->JSObject()->SetAttributeByEnum(
              XFA_Attribute::ContentType, WideString::FromASCII("text/xml"),
              false);
        }
        if (!pChildValue) {
          return std::nullopt;
        }
        return pChildValue->JSObject()->TryContent(bScriptModify, bProto);
      }
      break;
    case XFA_ObjectType::ContentNode: {
      CXFA_Node* pContentRawDataNode = GetXFANode()->GetFirstChild();
      if (!pContentRawDataNode) {
        XFA_Element element = XFA_Element::Sharptext;
        if (GetXFANode()->GetElementType() == XFA_Element::ExData) {
          std::optional<WideString> contentType =
              TryAttribute(XFA_Attribute::ContentType, false);
          if (contentType.has_value()) {
            if (contentType.value().EqualsASCII("text/html")) {
              element = XFA_Element::SharpxHTML;
            } else if (contentType.value().EqualsASCII("text/xml")) {
              element = XFA_Element::Sharpxml;
            }
          }
        }
        pContentRawDataNode = GetXFANode()->CreateSamePacketNode(element);
        GetXFANode()->InsertChildAndNotify(pContentRawDataNode, nullptr);
      }
      return pContentRawDataNode->JSObject()->TryContent(bScriptModify, true);
    }
    case XFA_ObjectType::NodeC:
    case XFA_ObjectType::NodeV:
    case XFA_ObjectType::TextNode:
      pNode = GetXFANode();
      [[fallthrough]];
    default:
      if (GetXFANode()->GetElementType() == XFA_Element::DataValue) {
        pNode = GetXFANode();
      }
      break;
  }
  if (pNode) {
    if (bScriptModify) {
      CFXJSE_Engine* pScriptContext = GetDocument()->GetScriptContext();
      pScriptContext->AddNodesOfRunScript(GetXFANode());
    }
    return TryCData(XFA_Attribute::Value, false);
  }
  return std::nullopt;
}

std::optional<WideString> CJX_Object::TryNamespace() const {
  if (GetXFANode()->IsModelNode() ||
      GetXFANode()->GetElementType() == XFA_Element::Packet) {
    CFX_XMLNode* pXMLNode = GetXFANode()->GetXMLMappingNode();
    CFX_XMLElement* element = ToXMLElement(pXMLNode);
    if (!element) {
      return std::nullopt;
    }

    return element->GetNamespaceURI();
  }

  if (GetXFANode()->GetPacketType() != XFA_PacketType::Datasets) {
    return GetXFANode()->GetModelNode()->JSObject()->TryNamespace();
  }

  CFX_XMLNode* pXMLNode = GetXFANode()->GetXMLMappingNode();
  CFX_XMLElement* element = ToXMLElement(pXMLNode);
  if (!element) {
    return std::nullopt;
  }

  if (GetXFANode()->GetElementType() == XFA_Element::DataValue &&
      GetEnum(XFA_Attribute::Contains) == XFA_AttributeValue::MetaData) {
    WideString wsNamespace;
    if (!XFA_FDEExtension_ResolveNamespaceQualifier(
            element, GetCData(XFA_Attribute::QualifiedName), &wsNamespace)) {
      return std::nullopt;
    }
    return wsNamespace;
  }
  return element->GetNamespaceURI();
}

CXFA_Node* CJX_Object::GetPropertyInternal(int32_t index,
                                           XFA_Element eProperty) const {
  return GetXFANode()->GetProperty(index, eProperty).first;
}

CXFA_Node* CJX_Object::GetOrCreatePropertyInternal(int32_t index,
                                                   XFA_Element eProperty) {
  return GetXFANode()->GetOrCreateProperty(index, eProperty);
}

CFXJSE_MapModule* CJX_Object::CreateMapModule() {
  if (!map_module_) {
    map_module_ = std::make_unique<CFXJSE_MapModule>();
  }
  return map_module_.get();
}

CFXJSE_MapModule* CJX_Object::GetMapModule() const {
  return map_module_.get();
}

void CJX_Object::SetMapModuleValue(uint32_t key, int32_t value) {
  CreateMapModule()->SetValue(key, value);
}

void CJX_Object::SetMapModuleString(uint32_t key, const WideString& wsValue) {
  CreateMapModule()->SetString(key, wsValue);
}

void CJX_Object::SetMapModuleMeasurement(uint32_t key,
                                         const CXFA_Measurement& value) {
  CreateMapModule()->SetMeasurement(key, value);
}

std::optional<int32_t> CJX_Object::GetMapModuleValue(uint32_t key) const {
  CFXJSE_MapModule* pModule = GetMapModule();
  if (!pModule) {
    return std::nullopt;
  }
  return pModule->GetValue(key);
}

std::optional<WideString> CJX_Object::GetMapModuleString(uint32_t key) const {
  CFXJSE_MapModule* pModule = GetMapModule();
  if (!pModule) {
    return std::nullopt;
  }
  return pModule->GetString(key);
}

std::optional<CXFA_Measurement> CJX_Object::GetMapModuleMeasurement(
    uint32_t key) const {
  CFXJSE_MapModule* pModule = GetMapModule();
  if (!pModule) {
    return std::nullopt;
  }
  return pModule->GetMeasurement(key);
}

std::optional<int32_t> CJX_Object::GetMapModuleValueFollowingChain(
    uint32_t key) const {
  std::set<const CXFA_Node*> visited;
  for (const CXFA_Node* pNode = GetXFANode(); pNode;
       pNode = pNode->GetTemplateNodeIfExists()) {
    if (!visited.insert(pNode).second) {
      break;
    }

    std::optional<int32_t> result = pNode->JSObject()->GetMapModuleValue(key);
    if (result.has_value()) {
      return result;
    }

    if (pNode->GetPacketType() == XFA_PacketType::Datasets) {
      break;
    }
  }
  return std::nullopt;
}

std::optional<WideString> CJX_Object::GetMapModuleStringFollowingChain(
    uint32_t key) const {
  std::set<const CXFA_Node*> visited;
  for (const CXFA_Node* pNode = GetXFANode(); pNode;
       pNode = pNode->GetTemplateNodeIfExists()) {
    if (!visited.insert(pNode).second) {
      break;
    }

    std::optional<WideString> result =
        pNode->JSObject()->GetMapModuleString(key);
    if (result.has_value()) {
      return result;
    }

    if (pNode->GetPacketType() == XFA_PacketType::Datasets) {
      break;
    }
  }
  return std::nullopt;
}

std::optional<CXFA_Measurement>
CJX_Object::GetMapModuleMeasurementFollowingChain(uint32_t key) const {
  std::set<const CXFA_Node*> visited;
  for (const CXFA_Node* pNode = GetXFANode(); pNode;
       pNode = pNode->GetTemplateNodeIfExists()) {
    if (!visited.insert(pNode).second) {
      break;
    }

    std::optional<CXFA_Measurement> result =
        pNode->JSObject()->GetMapModuleMeasurement(key);
    if (result.has_value()) {
      return result;
    }

    if (pNode->GetPacketType() == XFA_PacketType::Datasets) {
      break;
    }
  }
  return std::nullopt;
}

bool CJX_Object::HasMapModuleKey(uint32_t key) const {
  CFXJSE_MapModule* pModule = GetMapModule();
  return pModule && pModule->HasKey(key);
}

void CJX_Object::RemoveMapModuleKey(uint32_t key) {
  CFXJSE_MapModule* pModule = GetMapModule();
  if (pModule) {
    pModule->RemoveKey(key);
  }
}

void CJX_Object::MergeAllData(CXFA_Object* pDstObj) {
  CFXJSE_MapModule* pDstModule = ToNode(pDstObj)->JSObject()->CreateMapModule();
  CFXJSE_MapModule* pSrcModule = GetMapModule();
  if (!pSrcModule) {
    return;
  }

  pDstModule->MergeDataFrom(pSrcModule);
}

void CJX_Object::MoveBufferMapData(CXFA_Object* pDstObj) {
  if (!pDstObj) {
    return;
  }

  if (pDstObj->GetElementType() == GetXFAObject()->GetElementType()) {
    ToNode(pDstObj)->JSObject()->TakeCalcDataFrom(this);
  }

  if (!pDstObj->IsNodeV()) {
    return;
  }

  WideString wsValue = ToNode(pDstObj)->JSObject()->GetContent(false);
  WideString wsFormatValue(wsValue);
  CXFA_Node* pNode = ToNode(pDstObj)->GetContainerNode();
  if (pNode) {
    wsFormatValue = pNode->GetFormatDataValue(wsValue);
  }

  ToNode(pDstObj)->JSObject()->SetContent(wsValue, wsFormatValue, true, true,
                                          true);
}

void CJX_Object::MoveBufferMapData(CXFA_Object* pSrcObj, CXFA_Object* pDstObj) {
  if (!pSrcObj || !pDstObj) {
    return;
  }

  CXFA_Node* pSrcChild = ToNode(pSrcObj)->GetFirstChild();
  CXFA_Node* pDstChild = ToNode(pDstObj)->GetFirstChild();
  while (pSrcChild && pDstChild) {
    MoveBufferMapData(pSrcChild, pDstChild);
    pSrcChild = pSrcChild->GetNextSibling();
    pDstChild = pDstChild->GetNextSibling();
  }
  ToNode(pSrcObj)->JSObject()->MoveBufferMapData(pDstObj);
}

void CJX_Object::OnChanging(XFA_Attribute eAttr) {
  if (!GetXFANode()->IsInitialized()) {
    return;
  }

  CXFA_FFNotify* pNotify = GetDocument()->GetNotify();
  if (!pNotify) {
    return;
  }

  pNotify->OnValueChanging(GetXFANode(), eAttr);
}

void CJX_Object::OnChanged(XFA_Attribute eAttr, bool bScriptModify) {
  if (!GetXFANode()->IsInitialized()) {
    return;
  }

  GetXFANode()->SendAttributeChangeMessage(eAttr, bScriptModify);
}

CJX_Object::CalcData* CJX_Object::GetOrCreateCalcData(cppgc::Heap* heap) {
  if (!calc_data_) {
    calc_data_ =
        cppgc::MakeGarbageCollected<CalcData>(heap->GetAllocationHandle());
  }
  return calc_data_;
}

void CJX_Object::TakeCalcDataFrom(CJX_Object* that) {
  calc_data_ = that->calc_data_;
  that->calc_data_ = nullptr;
}

void CJX_Object::ScriptAttributeString(v8::Isolate* pIsolate,
                                       v8::Local<v8::Value>* pValue,
                                       bool bSetting,
                                       XFA_Attribute eAttribute) {
  if (!bSetting) {
    *pValue = fxv8::NewStringHelper(
        pIsolate, GetAttributeByEnum(eAttribute).ToUTF8().AsStringView());
    return;
  }

  WideString wsValue = fxv8::ReentrantToWideStringHelper(pIsolate, *pValue);
  SetAttributeByEnum(eAttribute, wsValue, true);
  if (eAttribute != XFA_Attribute::Use ||
      GetXFAObject()->GetElementType() != XFA_Element::Desc) {
    return;
  }

  CXFA_Node* pTemplateNode =
      ToNode(GetDocument()->GetXFAObject(XFA_HASHCODE_Template));
  CXFA_Subform* pSubForm =
      pTemplateNode->GetFirstChildByClass<CXFA_Subform>(XFA_Element::Subform);
  CXFA_Proto* pProtoRoot =
      pSubForm ? pSubForm->GetFirstChildByClass<CXFA_Proto>(XFA_Element::Proto)
               : nullptr;

  WideString wsID;
  WideString wsSOM;
  if (!wsValue.IsEmpty()) {
    if (wsValue[0] == '#') {
      wsID = wsValue.Substr(1);
    } else {
      wsSOM = std::move(wsValue);
    }
  }

  CXFA_Node* pProtoNode = nullptr;
  if (!wsSOM.IsEmpty()) {
    std::optional<CFXJSE_Engine::ResolveResult> maybeResult =
        GetDocument()->GetScriptContext()->ResolveObjects(
            pProtoRoot, wsSOM.AsStringView(),
            Mask<XFA_ResolveFlag>{
                XFA_ResolveFlag::kChildren, XFA_ResolveFlag::kAttributes,
                XFA_ResolveFlag::kProperties, XFA_ResolveFlag::kParent,
                XFA_ResolveFlag::kSiblings});
    if (maybeResult.has_value() &&
        maybeResult.value().objects.front()->IsNode()) {
      pProtoNode = maybeResult.value().objects.front()->AsNode();
    }
  } else if (!wsID.IsEmpty()) {
    pProtoNode = GetDocument()->GetNodeByID(pProtoRoot, wsID.AsStringView());
  }
  if (!pProtoNode || pProtoNode->GetPacketType() != XFA_PacketType::Template) {
    return;
  }

  CXFA_Node* pHeadChild = GetXFANode()->GetFirstChild();
  while (pHeadChild) {
    CXFA_Node* pSibling = pHeadChild->GetNextSibling();
    GetXFANode()->RemoveChildAndNotify(pHeadChild, true);
    pHeadChild = pSibling;
  }

  CXFA_Node* pProtoForm = pProtoNode->CloneTemplateToForm(true);
  pHeadChild = pProtoForm->GetFirstChild();
  while (pHeadChild) {
    CXFA_Node* pSibling = pHeadChild->GetNextSibling();
    pProtoForm->RemoveChildAndNotify(pHeadChild, true);
    GetXFANode()->InsertChildAndNotify(pHeadChild, nullptr);
    pHeadChild = pSibling;
  }
}

void CJX_Object::ScriptAttributeBool(v8::Isolate* pIsolate,
                                     v8::Local<v8::Value>* pValue,
                                     bool bSetting,
                                     XFA_Attribute eAttribute) {
  if (bSetting) {
    SetBoolean(eAttribute, fxv8::ReentrantToBooleanHelper(pIsolate, *pValue),
               true);
    return;
  }
  *pValue = fxv8::NewStringHelper(pIsolate, GetBoolean(eAttribute) ? "1" : "0");
}

void CJX_Object::ScriptAttributeInteger(v8::Isolate* pIsolate,
                                        v8::Local<v8::Value>* pValue,
                                        bool bSetting,
                                        XFA_Attribute eAttribute) {
  if (bSetting) {
    SetInteger(eAttribute, fxv8::ReentrantToInt32Helper(pIsolate, *pValue),
               true);
    return;
  }
  *pValue = fxv8::NewNumberHelper(pIsolate, GetInteger(eAttribute));
}

void CJX_Object::ScriptSomFontColor(v8::Isolate* pIsolate,
                                    v8::Local<v8::Value>* pValue,
                                    bool bSetting,
                                    XFA_Attribute eAttribute) {
  CXFA_Font* font = ToNode(object_.Get())->GetOrCreateFontIfPossible();
  if (!font) {
    return;
  }

  if (bSetting) {
    auto [r, g, b] =
        StrToRGB(fxv8::ReentrantToWideStringHelper(pIsolate, *pValue));
    FX_ARGB color = ArgbEncode(0xff, r, g, b);
    font->SetColor(color);
    return;
  }

  *pValue = ColorToV8String(pIsolate, font->GetColor());
}

void CJX_Object::ScriptSomFillColor(v8::Isolate* pIsolate,
                                    v8::Local<v8::Value>* pValue,
                                    bool bSetting,
                                    XFA_Attribute eAttribute) {
  CXFA_Border* border = ToNode(object_.Get())->GetOrCreateBorderIfPossible();
  CXFA_Fill* borderfill = border->GetOrCreateFillIfPossible();
  if (!borderfill) {
    return;
  }

  if (bSetting) {
    auto [r, g, b] =
        StrToRGB(fxv8::ReentrantToWideStringHelper(pIsolate, *pValue));
    FX_ARGB color = ArgbEncode(0xff, r, g, b);
    borderfill->SetColor(color);
    return;
  }

  *pValue = ColorToV8String(pIsolate, borderfill->GetFillColor());
}

void CJX_Object::ScriptSomBorderColor(v8::Isolate* pIsolate,
                                      v8::Local<v8::Value>* pValue,
                                      bool bSetting,
                                      XFA_Attribute eAttribute) {
  CXFA_Border* border = ToNode(object_.Get())->GetOrCreateBorderIfPossible();
  int32_t iSize = border->CountEdges();
  if (bSetting) {
    auto [r, g, b] =
        StrToRGB(fxv8::ReentrantToWideStringHelper(pIsolate, *pValue));
    FX_ARGB rgb = ArgbEncode(100, r, g, b);
    for (int32_t i = 0; i < iSize; ++i) {
      CXFA_Edge* edge = border->GetEdgeIfExists(i);
      if (edge) {
        edge->SetColor(rgb);
      }
    }

    return;
  }

  CXFA_Edge* edge = border->GetEdgeIfExists(0);
  FX_ARGB color = edge ? edge->GetColor() : CXFA_Edge::kDefaultColor;
  *pValue = ColorToV8String(pIsolate, color);
}

void CJX_Object::ScriptSomBorderWidth(v8::Isolate* pIsolate,
                                      v8::Local<v8::Value>* pValue,
                                      bool bSetting,
                                      XFA_Attribute eAttribute) {
  CXFA_Border* border = ToNode(object_.Get())->GetOrCreateBorderIfPossible();
  if (bSetting) {
    CXFA_Edge* edge = border->GetEdgeIfExists(0);
    CXFA_Measurement thickness =
        edge ? edge->GetMSThickness() : CXFA_Measurement(0.5, XFA_Unit::Pt);
    *pValue = fxv8::NewStringHelper(
        pIsolate, thickness.ToString().ToUTF8().AsStringView());
    return;
  }

  if (pValue->IsEmpty()) {
    return;
  }

  WideString wsThickness = fxv8::ReentrantToWideStringHelper(pIsolate, *pValue);
  for (size_t i = 0; i < border->CountEdges(); ++i) {
    CXFA_Edge* edge = border->GetEdgeIfExists(i);
    if (edge) {
      edge->SetMSThickness(CXFA_Measurement(wsThickness.AsStringView()));
    }
  }
}

void CJX_Object::ScriptSomMessage(v8::Isolate* pIsolate,
                                  v8::Local<v8::Value>* pValue,
                                  bool bSetting,
                                  SOMMessageType iMessageType) {
  bool bNew = false;
  CXFA_Validate* validate = ToNode(object_.Get())->GetValidateIfExists();
  if (!validate) {
    validate = ToNode(object_.Get())->GetOrCreateValidateIfPossible();
    bNew = true;
  }

  if (bSetting) {
    if (validate) {
      switch (iMessageType) {
        case SOMMessageType::kValidationMessage:
          validate->SetScriptMessageText(
              fxv8::ReentrantToWideStringHelper(pIsolate, *pValue));
          break;
        case SOMMessageType::kFormatMessage:
          validate->SetFormatMessageText(
              fxv8::ReentrantToWideStringHelper(pIsolate, *pValue));
          break;
        case SOMMessageType::kMandatoryMessage:
          validate->SetNullMessageText(
              fxv8::ReentrantToWideStringHelper(pIsolate, *pValue));
          break;
      }
    }

    if (!bNew) {
      CXFA_FFNotify* pNotify = GetDocument()->GetNotify();
      if (!pNotify) {
        return;
      }

      pNotify->AddCalcValidate(GetXFANode());
    }
    return;
  }

  if (!validate) {
    // TODO(dsinclair): Better error message?
    ThrowInvalidPropertyException(pIsolate);
    return;
  }

  WideString wsMessage;
  switch (iMessageType) {
    case SOMMessageType::kValidationMessage:
      wsMessage = validate->GetScriptMessageText();
      break;
    case SOMMessageType::kFormatMessage:
      wsMessage = validate->GetFormatMessageText();
      break;
    case SOMMessageType::kMandatoryMessage:
      wsMessage = validate->GetNullMessageText();
      break;
  }
  *pValue = fxv8::NewStringHelper(pIsolate, wsMessage.ToUTF8().AsStringView());
}

void CJX_Object::ScriptSomValidationMessage(v8::Isolate* pIsolate,
                                            v8::Local<v8::Value>* pValue,
                                            bool bSetting,
                                            XFA_Attribute eAttribute) {
  ScriptSomMessage(pIsolate, pValue, bSetting,
                   SOMMessageType::kValidationMessage);
}

void CJX_Object::ScriptSomMandatoryMessage(v8::Isolate* pIsolate,
                                           v8::Local<v8::Value>* pValue,
                                           bool bSetting,
                                           XFA_Attribute eAttribute) {
  ScriptSomMessage(pIsolate, pValue, bSetting,
                   SOMMessageType::kMandatoryMessage);
}

void CJX_Object::ScriptSomDefaultValue(v8::Isolate* pIsolate,
                                       v8::Local<v8::Value>* pValue,
                                       bool bSetting,
                                       XFA_Attribute /* unused */) {
  XFA_Element eType = GetXFANode()->GetElementType();

  // TODO(dsinclair): This should look through the properties on the node to see
  // if defaultValue is defined and, if so, call that one. Just have to make
  // sure that those defaultValue calls don't call back to this one ....
  if (eType == XFA_Element::Field) {
    static_cast<CJX_Field*>(this)->defaultValue(pIsolate, pValue, bSetting,
                                                XFA_Attribute::Unknown);
    return;
  }
  if (eType == XFA_Element::Draw) {
    static_cast<CJX_Draw*>(this)->defaultValue(pIsolate, pValue, bSetting,
                                               XFA_Attribute::Unknown);
    return;
  }
  if (eType == XFA_Element::Boolean) {
    static_cast<CJX_Boolean*>(this)->defaultValue(pIsolate, pValue, bSetting,
                                                  XFA_Attribute::Unknown);
    return;
  }

  if (bSetting) {
    WideString wsNewValue;
    if (pValue && !(pValue->IsEmpty() || fxv8::IsNull(*pValue) ||
                    fxv8::IsUndefined(*pValue))) {
      wsNewValue = fxv8::ReentrantToWideStringHelper(pIsolate, *pValue);
    }

    WideString wsFormatValue = wsNewValue;
    CXFA_Node* pContainerNode = nullptr;
    if (GetXFANode()->GetPacketType() == XFA_PacketType::Datasets) {
      WideString wsPicture;
      for (auto* pFormNode : GetXFANode()->GetBindItemsCopy()) {
        if (!pFormNode || pFormNode->HasRemovedChildren()) {
          continue;
        }

        pContainerNode = pFormNode->GetContainerNode();
        if (pContainerNode) {
          wsPicture =
              pContainerNode->GetPictureContent(XFA_ValuePicture::kDataBind);
        }
        if (!wsPicture.IsEmpty()) {
          break;
        }

        pContainerNode = nullptr;
      }
    } else if (GetXFANode()->GetPacketType() == XFA_PacketType::Form) {
      pContainerNode = GetXFANode()->GetContainerNode();
    }

    if (pContainerNode) {
      wsFormatValue = pContainerNode->GetFormatDataValue(wsNewValue);
    }

    SetContent(wsNewValue, wsFormatValue, true, true, true);
    return;
  }

  WideString content = GetContent(true);
  if (content.IsEmpty() && eType != XFA_Element::Text &&
      eType != XFA_Element::SubmitUrl) {
    *pValue = fxv8::NewNullHelper(pIsolate);
  } else if (eType == XFA_Element::Integer) {
    *pValue = fxv8::NewNumberHelper(pIsolate, FXSYS_wtoi(content.c_str()));
  } else if (eType == XFA_Element::Float || eType == XFA_Element::Decimal) {
    CFGAS_Decimal decimal(content.AsStringView());
    *pValue = fxv8::NewNumberHelper(pIsolate, decimal.ToFloat());
  } else {
    *pValue = fxv8::NewStringHelper(pIsolate, content.ToUTF8().AsStringView());
  }
}

void CJX_Object::ScriptSomDefaultValue_Read(v8::Isolate* pIsolate,
                                            v8::Local<v8::Value>* pValue,
                                            bool bSetting,
                                            XFA_Attribute eAttribute) {
  if (bSetting) {
    ThrowInvalidPropertyException(pIsolate);
    return;
  }

  WideString content = GetContent(true);
  if (content.IsEmpty()) {
    *pValue = fxv8::NewNullHelper(pIsolate);
    return;
  }
  *pValue = fxv8::NewStringHelper(pIsolate, content.ToUTF8().AsStringView());
}

void CJX_Object::ScriptSomDataNode(v8::Isolate* pIsolate,
                                   v8::Local<v8::Value>* pValue,
                                   bool bSetting,
                                   XFA_Attribute eAttribute) {
  if (bSetting) {
    ThrowInvalidPropertyException(pIsolate);
    return;
  }

  CXFA_Node* pDataNode = GetXFANode()->GetBindData();
  if (!pDataNode) {
    *pValue = fxv8::NewNullHelper(pIsolate);
    return;
  }

  *pValue =
      GetDocument()->GetScriptContext()->GetOrCreateJSBindingFromMap(pDataNode);
}

void CJX_Object::ScriptSomMandatory(v8::Isolate* pIsolate,
                                    v8::Local<v8::Value>* pValue,
                                    bool bSetting,
                                    XFA_Attribute eAttribute) {
  CXFA_Validate* validate =
      ToNode(object_.Get())->GetOrCreateValidateIfPossible();
  if (!validate) {
    return;
  }

  if (bSetting) {
    validate->SetNullTest(fxv8::ReentrantToWideStringHelper(pIsolate, *pValue));
    return;
  }

  *pValue = fxv8::NewStringHelper(
      pIsolate, XFA_AttributeValueToName(validate->GetNullTest()));
}

void CJX_Object::ScriptSomInstanceIndex(v8::Isolate* pIsolate,
                                        v8::Local<v8::Value>* pValue,
                                        bool bSetting,
                                        XFA_Attribute eAttribute) {
  if (!bSetting) {
    *pValue =
        fxv8::NewNumberHelper(pIsolate, Subform_and_SubformSet_InstanceIndex());
    return;
  }

  int32_t iTo = fxv8::ReentrantToInt32Helper(pIsolate, *pValue);
  int32_t iFrom = Subform_and_SubformSet_InstanceIndex();
  CXFA_Node* pManagerNode = nullptr;
  for (CXFA_Node* pNode = GetXFANode()->GetPrevSibling(); pNode;
       pNode = pNode->GetPrevSibling()) {
    if (pNode->GetElementType() == XFA_Element::InstanceManager) {
      pManagerNode = pNode;
      break;
    }
  }
  if (!pManagerNode) {
    return;
  }

  auto* mgr = static_cast<CJX_InstanceManager*>(pManagerNode->JSObject());
  mgr->MoveInstance(pIsolate, iTo, iFrom);
  CXFA_FFNotify* pNotify = GetDocument()->GetNotify();
  if (!pNotify) {
    return;
  }

  auto* pToInstance =
      CXFA_Subform::FromNode(pManagerNode->GetItemIfExists(iTo));
  if (pToInstance) {
    pNotify->RunSubformIndexChange(pToInstance);
  }

  auto* pFromInstance =
      CXFA_Subform::FromNode(pManagerNode->GetItemIfExists(iFrom));
  if (pFromInstance) {
    pNotify->RunSubformIndexChange(pFromInstance);
  }
}

void CJX_Object::ScriptSubmitFormatMode(v8::Isolate* pIsolate,
                                        v8::Local<v8::Value>* pValue,
                                        bool bSetting,
                                        XFA_Attribute eAttribute) {}

CJX_Object::CalcData::CalcData() = default;

CJX_Object::CalcData::~CalcData() = default;

void CJX_Object::CalcData::Trace(cppgc::Visitor* visitor) const {
  ContainerTrace(visitor, globals_);
}
