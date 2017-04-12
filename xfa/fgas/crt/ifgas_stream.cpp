// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fgas/crt/ifgas_stream.h"

#if _FX_OS_ == _FX_WIN32_DESKTOP_ || _FX_OS_ == _FX_WIN32_MOBILE_ || \
    _FX_OS_ == _FX_WIN64_
#include <io.h>
#endif

#include <algorithm>
#include <memory>
#include <utility>

#include "core/fxcrt/fx_ext.h"
#include "third_party/base/ptr_util.h"
#include "third_party/base/stl_util.h"
#include "xfa/fgas/crt/fgas_codepage.h"

namespace {

class IFGAS_StreamImp {
 public:
  virtual ~IFGAS_StreamImp() {}

  virtual int32_t GetLength() const = 0;
  virtual int32_t Seek(FX_STREAMSEEK eSeek, int32_t iOffset) = 0;
  virtual int32_t GetPosition() = 0;
  virtual bool IsEOF() const = 0;
  virtual int32_t ReadData(uint8_t* pBuffer, int32_t iBufferSize) = 0;
  virtual int32_t ReadString(wchar_t* pStr, int32_t iMaxLength, bool& bEOS) = 0;
  virtual int32_t WriteData(const uint8_t* pBuffer, int32_t iBufferSize) = 0;
  virtual int32_t WriteString(const wchar_t* pStr, int32_t iLength) = 0;
  virtual void Flush() = 0;
  virtual bool SetLength(int32_t iLength) = 0;

 protected:
  IFGAS_StreamImp();

  uint32_t GetAccessModes() const { return m_dwAccess; }

 private:
  uint32_t m_dwAccess;
};

class CFGAS_FileReadStreamImp : public IFGAS_StreamImp {
 public:
  CFGAS_FileReadStreamImp();
  ~CFGAS_FileReadStreamImp() override {}

  bool LoadFileRead(const CFX_RetainPtr<IFX_SeekableReadStream>& pFileRead);

  // IFGAS_StreamImp:
  int32_t GetLength() const override;
  int32_t Seek(FX_STREAMSEEK eSeek, int32_t iOffset) override;
  int32_t GetPosition() override { return m_iPosition; }
  bool IsEOF() const override;
  int32_t ReadData(uint8_t* pBuffer, int32_t iBufferSize) override;
  int32_t ReadString(wchar_t* pStr, int32_t iMaxLength, bool& bEOS) override;
  int32_t WriteData(const uint8_t* pBuffer, int32_t iBufferSize) override {
    return 0;
  }
  int32_t WriteString(const wchar_t* pStr, int32_t iLength) override {
    return 0;
  }
  void Flush() override {}
  bool SetLength(int32_t iLength) override { return false; }

 private:
  CFX_RetainPtr<IFX_SeekableReadStream> m_pFileRead;
  int32_t m_iPosition;
  int32_t m_iLength;
};

class CFGAS_FileWriteStreamImp : public IFGAS_StreamImp {
 public:
  CFGAS_FileWriteStreamImp();
  ~CFGAS_FileWriteStreamImp() override {}

  bool LoadFileWrite(const CFX_RetainPtr<IFX_SeekableWriteStream>& pFileWrite,
                     uint32_t dwAccess);

  // IFGAS_StreamImp:
  int32_t GetLength() const override;
  int32_t Seek(FX_STREAMSEEK eSeek, int32_t iOffset) override;
  int32_t GetPosition() override { return m_iPosition; }
  bool IsEOF() const override;
  int32_t ReadData(uint8_t* pBuffer, int32_t iBufferSize) override { return 0; }
  int32_t ReadString(wchar_t* pStr, int32_t iMaxLength, bool& bEOS) override {
    return 0;
  }
  int32_t WriteData(const uint8_t* pBuffer, int32_t iBufferSize) override;
  int32_t WriteString(const wchar_t* pStr, int32_t iLength) override;
  void Flush() override;
  bool SetLength(int32_t iLength) override { return false; }

 private:
  CFX_RetainPtr<IFX_SeekableWriteStream> m_pFileWrite;
  int32_t m_iPosition;
};

class CFGAS_Stream : public IFGAS_Stream {
 public:
  template <typename T, typename... Args>
  friend CFX_RetainPtr<T> pdfium::MakeRetain(Args&&... args);

  // IFGAS_Stream
  uint32_t GetAccessModes() const override;
  int32_t GetLength() const override;
  int32_t Seek(FX_STREAMSEEK eSeek, int32_t iOffset) override;
  int32_t GetPosition() override;
  bool IsEOF() const override;
  int32_t ReadData(uint8_t* pBuffer, int32_t iBufferSize) override;
  int32_t ReadString(wchar_t* pStr, int32_t iMaxLength, bool& bEOS) override;
  int32_t WriteData(const uint8_t* pBuffer, int32_t iBufferSize) override;
  int32_t WriteString(const wchar_t* pStr, int32_t iLength) override;
  void Flush() override;
  bool SetLength(int32_t iLength) override;
  int32_t GetBOM(uint8_t bom[4]) const override;
  uint16_t GetCodePage() const override;
  uint16_t SetCodePage(uint16_t wCodePage) override;

 private:
  CFGAS_Stream(std::unique_ptr<IFGAS_StreamImp> imp, uint32_t dwAccess);
  ~CFGAS_Stream() override;

  std::unique_ptr<IFGAS_StreamImp> m_pStreamImp;
  uint32_t m_dwAccess;
  int32_t m_iTotalSize;
  int32_t m_iPosition;
  int32_t m_iLength;
  int32_t m_iRefCount;
};

class CFGAS_TextStream : public IFGAS_Stream {
 public:
  template <typename T, typename... Args>
  friend CFX_RetainPtr<T> pdfium::MakeRetain(Args&&... args);

  // IFGAS_Stream
  uint32_t GetAccessModes() const override;
  int32_t GetLength() const override;
  int32_t Seek(FX_STREAMSEEK eSeek, int32_t iOffset) override;
  int32_t GetPosition() override;
  bool IsEOF() const override;
  int32_t ReadData(uint8_t* pBuffer, int32_t iBufferSize) override;
  int32_t ReadString(wchar_t* pStr, int32_t iMaxLength, bool& bEOS) override;
  int32_t WriteData(const uint8_t* pBuffer, int32_t iBufferSize) override;
  int32_t WriteString(const wchar_t* pStr, int32_t iLength) override;
  void Flush() override;
  bool SetLength(int32_t iLength) override;
  int32_t GetBOM(uint8_t bom[4]) const override;
  uint16_t GetCodePage() const override;
  uint16_t SetCodePage(uint16_t wCodePage) override;

 private:
  explicit CFGAS_TextStream(const CFX_RetainPtr<IFGAS_Stream>& pStream);
  ~CFGAS_TextStream() override;

  void InitStream();

  uint16_t m_wCodePage;
  int32_t m_wBOMLength;
  uint32_t m_dwBOM;
  uint8_t* m_pBuf;
  int32_t m_iBufSize;
  CFX_RetainPtr<IFGAS_Stream> m_pStreamImp;
};

class CFGAS_WideStringReadStream : public IFGAS_Stream {
 public:
  template <typename T, typename... Args>
  friend CFX_RetainPtr<T> pdfium::MakeRetain(Args&&... args);

  // IFGAS_Stream
  uint32_t GetAccessModes() const override;
  int32_t GetLength() const override;
  int32_t Seek(FX_STREAMSEEK eSeek, int32_t iOffset) override;
  int32_t GetPosition() override;
  bool IsEOF() const override;
  int32_t ReadData(uint8_t* pBuffer, int32_t iBufferSize) override;
  int32_t ReadString(wchar_t* pStr, int32_t iMaxLength, bool& bEOS) override;
  int32_t WriteData(const uint8_t* pBuffer, int32_t iBufferSize) override;
  int32_t WriteString(const wchar_t* pStr, int32_t iLength) override;
  void Flush() override {}
  bool SetLength(int32_t iLength) override;
  int32_t GetBOM(uint8_t bom[4]) const override;
  uint16_t GetCodePage() const override;
  uint16_t SetCodePage(uint16_t wCodePage) override;

 private:
  explicit CFGAS_WideStringReadStream(const CFX_WideString& wsBuffer);
  ~CFGAS_WideStringReadStream() override;

  CFX_WideString m_wsBuffer;
  int32_t m_iPosition;
};

IFGAS_StreamImp::IFGAS_StreamImp() : m_dwAccess(0) {}

CFGAS_FileReadStreamImp::CFGAS_FileReadStreamImp()
    : m_pFileRead(nullptr), m_iPosition(0), m_iLength(0) {}

bool CFGAS_FileReadStreamImp::LoadFileRead(
    const CFX_RetainPtr<IFX_SeekableReadStream>& pFileRead) {
  ASSERT(!m_pFileRead && pFileRead);

  m_pFileRead = pFileRead;
  m_iLength = m_pFileRead->GetSize();
  return true;
}

int32_t CFGAS_FileReadStreamImp::GetLength() const {
  return m_iLength;
}

int32_t CFGAS_FileReadStreamImp::Seek(FX_STREAMSEEK eSeek, int32_t iOffset) {
  switch (eSeek) {
    case FX_STREAMSEEK_Begin:
      m_iPosition = iOffset;
      break;
    case FX_STREAMSEEK_Current:
      m_iPosition += iOffset;
      break;
    case FX_STREAMSEEK_End:
      m_iPosition = m_iLength + iOffset;
      break;
  }
  if (m_iPosition < 0) {
    m_iPosition = 0;
  } else if (m_iPosition >= m_iLength) {
    m_iPosition = m_iLength;
  }
  return m_iPosition;
}

bool CFGAS_FileReadStreamImp::IsEOF() const {
  return m_iPosition >= m_iLength;
}
int32_t CFGAS_FileReadStreamImp::ReadData(uint8_t* pBuffer,
                                          int32_t iBufferSize) {
  ASSERT(m_pFileRead);
  ASSERT(pBuffer && iBufferSize > 0);
  if (iBufferSize > m_iLength - m_iPosition) {
    iBufferSize = m_iLength - m_iPosition;
  }
  if (m_pFileRead->ReadBlock(pBuffer, m_iPosition, iBufferSize)) {
    m_iPosition += iBufferSize;
    return iBufferSize;
  }
  return 0;
}
int32_t CFGAS_FileReadStreamImp::ReadString(wchar_t* pStr,
                                            int32_t iMaxLength,
                                            bool& bEOS) {
  ASSERT(m_pFileRead);
  ASSERT(pStr && iMaxLength > 0);
  iMaxLength = ReadData((uint8_t*)pStr, iMaxLength * 2) / 2;
  if (iMaxLength <= 0) {
    return 0;
  }
  int32_t i = 0;
  while (i < iMaxLength && pStr[i] != L'\0') {
    ++i;
  }
  bEOS = (m_iPosition >= m_iLength) || pStr[i] == L'\0';
  return i;
}

CFGAS_FileWriteStreamImp::CFGAS_FileWriteStreamImp()
    : m_pFileWrite(nullptr), m_iPosition(0) {}

bool CFGAS_FileWriteStreamImp::LoadFileWrite(
    const CFX_RetainPtr<IFX_SeekableWriteStream>& pFileWrite,
    uint32_t dwAccess) {
  ASSERT(!m_pFileWrite && pFileWrite);

  m_iPosition = pFileWrite->GetSize();
  m_pFileWrite = pFileWrite;
  return true;
}

int32_t CFGAS_FileWriteStreamImp::GetLength() const {
  if (!m_pFileWrite)
    return 0;

  return (int32_t)m_pFileWrite->GetSize();
}
int32_t CFGAS_FileWriteStreamImp::Seek(FX_STREAMSEEK eSeek, int32_t iOffset) {
  int32_t iLength = GetLength();
  switch (eSeek) {
    case FX_STREAMSEEK_Begin:
      m_iPosition = iOffset;
      break;
    case FX_STREAMSEEK_Current:
      m_iPosition += iOffset;
      break;
    case FX_STREAMSEEK_End:
      m_iPosition = iLength + iOffset;
      break;
  }
  if (m_iPosition < 0) {
    m_iPosition = 0;
  } else if (m_iPosition >= iLength) {
    m_iPosition = iLength;
  }
  return m_iPosition;
}
bool CFGAS_FileWriteStreamImp::IsEOF() const {
  return m_iPosition >= GetLength();
}
int32_t CFGAS_FileWriteStreamImp::WriteData(const uint8_t* pBuffer,
                                            int32_t iBufferSize) {
  if (!m_pFileWrite) {
    return 0;
  }
  if (m_pFileWrite->WriteBlock(pBuffer, m_iPosition, iBufferSize)) {
    m_iPosition += iBufferSize;
  }
  return iBufferSize;
}
int32_t CFGAS_FileWriteStreamImp::WriteString(const wchar_t* pStr,
                                              int32_t iLength) {
  return WriteData((const uint8_t*)pStr, iLength * sizeof(wchar_t));
}
void CFGAS_FileWriteStreamImp::Flush() {
  if (m_pFileWrite) {
    m_pFileWrite->Flush();
  }
}

CFGAS_TextStream::CFGAS_TextStream(const CFX_RetainPtr<IFGAS_Stream>& pStream)
    : m_wCodePage(FX_CODEPAGE_DefANSI),
      m_wBOMLength(0),
      m_dwBOM(0),
      m_pBuf(nullptr),
      m_iBufSize(0),
      m_pStreamImp(pStream) {
  ASSERT(m_pStreamImp);
  InitStream();
}

CFGAS_TextStream::~CFGAS_TextStream() {
  if (m_pBuf)
    FX_Free(m_pBuf);
}

void CFGAS_TextStream::InitStream() {
  int32_t iPosition = m_pStreamImp->GetPosition();
  m_pStreamImp->Seek(FX_STREAMSEEK_Begin, 0);
  m_pStreamImp->ReadData((uint8_t*)&m_dwBOM, 3);
#if _FX_ENDIAN_ == _FX_LITTLE_ENDIAN_
  m_dwBOM &= 0x00FFFFFF;
  if (m_dwBOM == 0x00BFBBEF) {
    m_wBOMLength = 3;
    m_wCodePage = FX_CODEPAGE_UTF8;
  } else {
    m_dwBOM &= 0x0000FFFF;
    if (m_dwBOM == 0x0000FFFE) {
      m_wBOMLength = 2;
      m_wCodePage = FX_CODEPAGE_UTF16BE;
    } else if (m_dwBOM == 0x0000FEFF) {
      m_wBOMLength = 2;
      m_wCodePage = FX_CODEPAGE_UTF16LE;
    } else {
      m_wBOMLength = 0;
      m_dwBOM = 0;
      m_wCodePage = FXSYS_GetACP();
    }
  }
#else
  m_dwBOM &= 0xFFFFFF00;
  if (m_dwBOM == 0xEFBBBF00) {
    m_wBOMLength = 3;
    m_wCodePage = FX_CODEPAGE_UTF8;
  } else {
    m_dwBOM &= 0xFFFF0000;
    if (m_dwBOM == 0xFEFF0000) {
      m_wBOMLength = 2;
      m_wCodePage = FX_CODEPAGE_UTF16BE;
    } else if (m_dwBOM == 0xFFFE0000) {
      m_wBOMLength = 2;
      m_wCodePage = FX_CODEPAGE_UTF16LE;
    } else {
      m_wBOMLength = 0;
      m_dwBOM = 0;
      m_wCodePage = FXSYS_GetACP();
    }
  }
#endif
  m_pStreamImp->Seek(FX_STREAMSEEK_Begin, std::max(m_wBOMLength, iPosition));
}

uint32_t CFGAS_TextStream::GetAccessModes() const {
  return m_pStreamImp->GetAccessModes();
}

int32_t CFGAS_TextStream::GetLength() const {
  return m_pStreamImp->GetLength();
}

int32_t CFGAS_TextStream::Seek(FX_STREAMSEEK eSeek, int32_t iOffset) {
  return m_pStreamImp->Seek(eSeek, iOffset);
}

int32_t CFGAS_TextStream::GetPosition() {
  return m_pStreamImp->GetPosition();
}

bool CFGAS_TextStream::IsEOF() const {
  return m_pStreamImp->IsEOF();
}

int32_t CFGAS_TextStream::ReadData(uint8_t* pBuffer, int32_t iBufferSize) {
  return m_pStreamImp->ReadData(pBuffer, iBufferSize);
}

int32_t CFGAS_TextStream::WriteData(const uint8_t* pBuffer,
                                    int32_t iBufferSize) {
  return m_pStreamImp->WriteData(pBuffer, iBufferSize);
}

void CFGAS_TextStream::Flush() {
  m_pStreamImp->Flush();
}

bool CFGAS_TextStream::SetLength(int32_t iLength) {
  return m_pStreamImp->SetLength(iLength);
}

uint16_t CFGAS_TextStream::GetCodePage() const {
  return m_wCodePage;
}

int32_t CFGAS_TextStream::GetBOM(uint8_t bom[4]) const {
  if (m_wBOMLength < 1)
    return 0;

  *(uint32_t*)bom = m_dwBOM;
  return m_wBOMLength;
}

uint16_t CFGAS_TextStream::SetCodePage(uint16_t wCodePage) {
  if (m_wBOMLength > 0)
    return m_wCodePage;

  uint16_t v = m_wCodePage;
  m_wCodePage = wCodePage;
  return v;
}

int32_t CFGAS_TextStream::ReadString(wchar_t* pStr,
                                     int32_t iMaxLength,
                                     bool& bEOS) {
  ASSERT(pStr && iMaxLength > 0);
  if (!m_pStreamImp) {
    return -1;
  }
  int32_t iLen;
  if (m_wCodePage == FX_CODEPAGE_UTF16LE ||
      m_wCodePage == FX_CODEPAGE_UTF16BE) {
    int32_t iBytes = iMaxLength * 2;
    iLen = m_pStreamImp->ReadData((uint8_t*)pStr, iBytes);
    iMaxLength = iLen / 2;
    if (sizeof(wchar_t) > 2) {
      FX_UTF16ToWChar(pStr, iMaxLength);
    }
#if _FX_ENDIAN_ == _FX_BIG_ENDIAN_
    if (m_wCodePage == FX_CODEPAGE_UTF16LE) {
      FX_SwapByteOrder(pStr, iMaxLength);
    }
#else
    if (m_wCodePage == FX_CODEPAGE_UTF16BE) {
      FX_SwapByteOrder(pStr, iMaxLength);
    }
#endif
  } else {
    int32_t pos = m_pStreamImp->GetPosition();
    int32_t iBytes = iMaxLength;
    iBytes = std::min(iBytes, m_pStreamImp->GetLength() - pos);
    if (iBytes > 0) {
      if (!m_pBuf) {
        m_pBuf = FX_Alloc(uint8_t, iBytes);
        m_iBufSize = iBytes;
      } else if (iBytes > m_iBufSize) {
        m_pBuf = FX_Realloc(uint8_t, m_pBuf, iBytes);
        m_iBufSize = iBytes;
      }
      iLen = m_pStreamImp->ReadData(m_pBuf, iBytes);
      int32_t iSrc = iLen;
      int32_t iDecode = FX_DecodeString(m_wCodePage, (const char*)m_pBuf, &iSrc,
                                        pStr, &iMaxLength, true);
      m_pStreamImp->Seek(FX_STREAMSEEK_Current, iSrc - iLen);
      if (iDecode < 1) {
        return -1;
      }
    } else {
      iMaxLength = 0;
    }
  }
  bEOS = m_pStreamImp->IsEOF();
  return iMaxLength;
}

int32_t CFGAS_TextStream::WriteString(const wchar_t* pStr, int32_t iLength) {
  ASSERT(pStr && iLength > 0);
  if ((m_pStreamImp->GetAccessModes() & FX_STREAMACCESS_Write) == 0)
    return -1;

  if (m_wCodePage == FX_CODEPAGE_UTF8) {
    int32_t len = iLength;
    CFX_UTF8Encoder encoder;
    while (len-- > 0) {
      encoder.Input(*pStr++);
    }
    CFX_ByteStringC bsResult = encoder.GetResult();
    m_pStreamImp->WriteData((const uint8_t*)bsResult.c_str(),
                            bsResult.GetLength());
  }
  return iLength;
}

CFGAS_Stream::CFGAS_Stream(std::unique_ptr<IFGAS_StreamImp> imp,
                           uint32_t dwAccess)
    : m_pStreamImp(std::move(imp)),
      m_dwAccess(dwAccess),
      m_iTotalSize(0),
      m_iPosition(0),
      m_iLength(m_pStreamImp->GetLength()),
      m_iRefCount(1) {}

CFGAS_Stream::~CFGAS_Stream() {}

uint32_t CFGAS_Stream::GetAccessModes() const {
  return m_dwAccess;
}

int32_t CFGAS_Stream::GetLength() const {
  return m_pStreamImp ? m_pStreamImp->GetLength() : -1;
}

int32_t CFGAS_Stream::Seek(FX_STREAMSEEK eSeek, int32_t iOffset) {
  if (!m_pStreamImp)
    return -1;
  m_iPosition = m_pStreamImp->Seek(eSeek, iOffset);
  return m_iPosition;
}

int32_t CFGAS_Stream::GetPosition() {
  if (!m_pStreamImp)
    return -1;
  m_iPosition = m_pStreamImp->GetPosition();
  return m_iPosition;
}

bool CFGAS_Stream::IsEOF() const {
  return m_pStreamImp ? m_pStreamImp->IsEOF() : true;
}

int32_t CFGAS_Stream::ReadData(uint8_t* pBuffer, int32_t iBufferSize) {
  ASSERT(pBuffer && iBufferSize > 0);
  if (!m_pStreamImp)
    return -1;

  int32_t iLen = std::min(m_iLength - m_iPosition, iBufferSize);
  if (iLen <= 0)
    return 0;
  if (m_pStreamImp->GetPosition() != m_iPosition)
    m_pStreamImp->Seek(FX_STREAMSEEK_Begin, m_iPosition);

  iLen = m_pStreamImp->ReadData(pBuffer, iLen);
  m_iPosition = m_pStreamImp->GetPosition();
  return iLen;
}

int32_t CFGAS_Stream::ReadString(wchar_t* pStr,
                                 int32_t iMaxLength,
                                 bool& bEOS) {
  ASSERT(pStr && iMaxLength > 0);
  if (!m_pStreamImp)
    return -1;

  int32_t iLen = std::min((m_iLength - m_iPosition) / 2, iMaxLength);
  if (iLen <= 0)
    return 0;
  if (m_pStreamImp->GetPosition() != m_iPosition)
    m_pStreamImp->Seek(FX_STREAMSEEK_Begin, m_iPosition);

  iLen = m_pStreamImp->ReadString(pStr, iLen, bEOS);
  m_iPosition = m_pStreamImp->GetPosition();
  if (iLen > 0 && m_iPosition >= m_iLength)
    bEOS = true;

  return iLen;
}

int32_t CFGAS_Stream::WriteData(const uint8_t* pBuffer, int32_t iBufferSize) {
  ASSERT(pBuffer && iBufferSize > 0);
  if (!m_pStreamImp)
    return -1;
  if ((m_dwAccess & FX_STREAMACCESS_Write) == 0)
    return -1;
  if (m_pStreamImp->GetPosition() != m_iPosition)
    m_pStreamImp->Seek(FX_STREAMSEEK_Begin, m_iPosition);

  int32_t iLen = m_pStreamImp->WriteData(pBuffer, iBufferSize);
  m_iPosition = m_pStreamImp->GetPosition();
  m_iLength = std::max(m_iLength, m_iPosition);

  return iLen;
}

int32_t CFGAS_Stream::WriteString(const wchar_t* pStr, int32_t iLength) {
  ASSERT(pStr && iLength > 0);
  if (!m_pStreamImp)
    return -1;
  if ((m_dwAccess & FX_STREAMACCESS_Write) == 0)
    return -1;
  if (m_pStreamImp->GetPosition() != m_iPosition)
    m_pStreamImp->Seek(FX_STREAMSEEK_Begin, m_iPosition);

  int32_t iLen = m_pStreamImp->WriteString(pStr, iLength);
  m_iPosition = m_pStreamImp->GetPosition();
  m_iLength = std::max(m_iLength, m_iPosition);
  return iLen;
}

void CFGAS_Stream::Flush() {
  if (!m_pStreamImp)
    return;
  if ((m_dwAccess & FX_STREAMACCESS_Write) == 0)
    return;
  m_pStreamImp->Flush();
}

bool CFGAS_Stream::SetLength(int32_t iLength) {
  if (!m_pStreamImp)
    return false;
  if ((m_dwAccess & FX_STREAMACCESS_Write) == 0)
    return false;
  return m_pStreamImp->SetLength(iLength);
}

int32_t CFGAS_Stream::GetBOM(uint8_t bom[4]) const {
  if (!m_pStreamImp)
    return -1;
  return 0;
}

uint16_t CFGAS_Stream::GetCodePage() const {
#if _FX_ENDIAN_ == _FX_LITTLE_ENDIAN_
  return FX_CODEPAGE_UTF16LE;
#else
  return FX_CODEPAGE_UTF16BE;
#endif
}
uint16_t CFGAS_Stream::SetCodePage(uint16_t wCodePage) {
#if _FX_ENDIAN_ == _FX_LITTLE_ENDIAN_
  return FX_CODEPAGE_UTF16LE;
#else
  return FX_CODEPAGE_UTF16BE;
#endif
}

CFGAS_WideStringReadStream::CFGAS_WideStringReadStream(
    const CFX_WideString& wsBuffer)
    : m_wsBuffer(wsBuffer), m_iPosition(0) {}

CFGAS_WideStringReadStream::~CFGAS_WideStringReadStream() {}

uint32_t CFGAS_WideStringReadStream::GetAccessModes() const {
  return 0;
}

int32_t CFGAS_WideStringReadStream::GetLength() const {
  return m_wsBuffer.GetLength() * sizeof(wchar_t);
}

int32_t CFGAS_WideStringReadStream::Seek(FX_STREAMSEEK eSeek, int32_t iOffset) {
  switch (eSeek) {
    case FX_STREAMSEEK_Begin:
      m_iPosition = iOffset;
      break;
    case FX_STREAMSEEK_Current:
      m_iPosition += iOffset;
      break;
    case FX_STREAMSEEK_End:
      m_iPosition = m_wsBuffer.GetLength() + iOffset;
      break;
  }
  m_iPosition = pdfium::clamp(0, m_iPosition, m_wsBuffer.GetLength());
  return GetPosition();
}

int32_t CFGAS_WideStringReadStream::GetPosition() {
  return m_iPosition * sizeof(wchar_t);
}

bool CFGAS_WideStringReadStream::IsEOF() const {
  return m_iPosition >= m_wsBuffer.GetLength();
}

int32_t CFGAS_WideStringReadStream::ReadData(uint8_t* pBuffer,
                                             int32_t iBufferSize) {
  return 0;
}

int32_t CFGAS_WideStringReadStream::ReadString(wchar_t* pStr,
                                               int32_t iMaxLength,
                                               bool& bEOS) {
  iMaxLength = std::min(iMaxLength, m_wsBuffer.GetLength() - m_iPosition);
  if (iMaxLength == 0)
    return 0;

  FXSYS_wcsncpy(pStr, m_wsBuffer.c_str() + m_iPosition, iMaxLength);
  m_iPosition += iMaxLength;
  bEOS = IsEOF();
  return iMaxLength;
}

int32_t CFGAS_WideStringReadStream::WriteData(const uint8_t* pBuffer,
                                              int32_t iBufferSize) {
  return 0;
}

int32_t CFGAS_WideStringReadStream::WriteString(const wchar_t* pStr,
                                                int32_t iLength) {
  return 0;
}

bool CFGAS_WideStringReadStream::SetLength(int32_t iLength) {
  return false;
}

int32_t CFGAS_WideStringReadStream::GetBOM(uint8_t bom[4]) const {
  return 0;
}

uint16_t CFGAS_WideStringReadStream::GetCodePage() const {
  return (sizeof(wchar_t) == 2) ? FX_CODEPAGE_UTF16LE : FX_CODEPAGE_UTF32LE;
}

uint16_t CFGAS_WideStringReadStream::SetCodePage(uint16_t wCodePage) {
  return GetCodePage();
}

}  // namespace

// static
CFX_RetainPtr<IFGAS_Stream> IFGAS_Stream::CreateReadStream(
    const CFX_RetainPtr<IFX_SeekableReadStream>& pFileRead) {
  if (!pFileRead)
    return nullptr;

  std::unique_ptr<IFGAS_StreamImp> pImp =
      pdfium::MakeUnique<CFGAS_FileReadStreamImp>();
  if (!static_cast<CFGAS_FileReadStreamImp*>(pImp.get())
           ->LoadFileRead(pFileRead)) {
    return nullptr;
  }
  return pdfium::MakeRetain<CFGAS_TextStream>(
      pdfium::MakeRetain<CFGAS_Stream>(std::move(pImp), 0));
}

// static
CFX_RetainPtr<IFGAS_Stream> IFGAS_Stream::CreateWriteStream(
    const CFX_RetainPtr<IFX_SeekableWriteStream>& pFileWrite) {
  if (!pFileWrite)
    return nullptr;

  std::unique_ptr<IFGAS_StreamImp> pImp =
      pdfium::MakeUnique<CFGAS_FileWriteStreamImp>();
  if (!static_cast<CFGAS_FileWriteStreamImp*>(pImp.get())
           ->LoadFileWrite(pFileWrite, FX_STREAMACCESS_Write)) {
    return nullptr;
  }

  return pdfium::MakeRetain<CFGAS_TextStream>(
      pdfium::MakeRetain<CFGAS_Stream>(std::move(pImp), FX_STREAMACCESS_Write));
}

// static
CFX_RetainPtr<IFGAS_Stream> IFGAS_Stream::CreateWideStringReadStream(
    const CFX_WideString& buffer) {
  return pdfium::MakeRetain<CFGAS_WideStringReadStream>(buffer);
}
