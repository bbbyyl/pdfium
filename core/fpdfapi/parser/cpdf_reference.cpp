// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/parser/cpdf_reference.h"

#include "core/fpdfapi/parser/cpdf_indirect_object_holder.h"
#include "core/fxcrt/fx_stream.h"
#include "third_party/base/check_op.h"
#include "third_party/base/containers/contains.h"

CPDF_Reference::CPDF_Reference(CPDF_IndirectObjectHolder* pDoc, uint32_t objnum)
    : m_pObjList(pDoc), m_RefObjNum(objnum) {}

CPDF_Reference::~CPDF_Reference() = default;

CPDF_Object::Type CPDF_Reference::GetType() const {
  return kReference;
}

ByteString CPDF_Reference::GetString() const {
  RetainPtr<const CPDF_Object> obj = SafeGetDirect();
  return obj ? obj->GetString() : ByteString();
}

float CPDF_Reference::GetNumber() const {
  RetainPtr<const CPDF_Object> obj = SafeGetDirect();
  return obj ? obj->GetNumber() : 0;
}

int CPDF_Reference::GetInteger() const {
  RetainPtr<const CPDF_Object> obj = SafeGetDirect();
  return obj ? obj->GetInteger() : 0;
}

const CPDF_Dictionary* CPDF_Reference::GetDict() const {
  RetainPtr<const CPDF_Object> obj = SafeGetDirect();
  return obj ? obj->GetDict() : nullptr;
}

CPDF_Reference* CPDF_Reference::AsMutableReference() {
  return this;
}

RetainPtr<CPDF_Object> CPDF_Reference::Clone() const {
  return CloneObjectNonCyclic(false);
}

RetainPtr<CPDF_Object> CPDF_Reference::CloneNonCyclic(
    bool bDirect,
    std::set<const CPDF_Object*>* pVisited) const {
  pVisited->insert(this);
  if (!bDirect) {
    return pdfium::MakeRetain<CPDF_Reference>(m_pObjList.Get(), m_RefObjNum);
  }
  RetainPtr<const CPDF_Object> pDirect = GetDirect();
  return pDirect && !pdfium::Contains(*pVisited, pDirect.Get())
             ? pDirect->CloneNonCyclic(true, pVisited)
             : nullptr;
}

RetainPtr<const CPDF_Object> CPDF_Reference::SafeGetDirect() const {
  RetainPtr<const CPDF_Object> obj = GetDirect();
  return (obj && !obj->IsReference()) ? obj : nullptr;
}

void CPDF_Reference::SetRef(CPDF_IndirectObjectHolder* pDoc, uint32_t objnum) {
  m_pObjList = pDoc;
  m_RefObjNum = objnum;
}

RetainPtr<CPDF_Object> CPDF_Reference::GetMutableDirect() {
  return m_pObjList ? m_pObjList->GetOrParseIndirectObject(m_RefObjNum)
                    : nullptr;
}

bool CPDF_Reference::WriteTo(IFX_ArchiveStream* archive,
                             const CPDF_Encryptor* encryptor) const {
  return archive->WriteString(" ") && archive->WriteDWord(GetRefObjNum()) &&
         archive->WriteString(" 0 R ");
}

RetainPtr<CPDF_Reference> CPDF_Reference::MakeReference(
    CPDF_IndirectObjectHolder* holder) const {
  DCHECK_EQ(holder, m_pObjList);
  // Do not allow reference to reference, just create other reference for same
  // object.
  return pdfium::MakeRetain<CPDF_Reference>(holder, GetRefObjNum());
}
