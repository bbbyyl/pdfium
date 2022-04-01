// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxge/cfx_fontmapper.h"

#include <algorithm>
#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "build/build_config.h"
#include "core/fxcrt/fx_codepage.h"
#include "core/fxcrt/fx_extension.h"
#include "core/fxcrt/fx_memory.h"
#include "core/fxcrt/fx_memory_wrappers.h"
#include "core/fxcrt/stl_util.h"
#include "core/fxge/cfx_fontmgr.h"
#include "core/fxge/cfx_substfont.h"
#include "core/fxge/fx_font.h"
#include "core/fxge/systemfontinfo_iface.h"
#include "third_party/base/check_op.h"
#include "third_party/base/containers/contains.h"
#include "third_party/base/cxx17_backports.h"

namespace {

static_assert(CFX_FontMapper::kLast + 1 == CFX_FontMapper::kNumStandardFonts,
              "StandardFont enum count mismatch");

const char* const kBase14FontNames[CFX_FontMapper::kNumStandardFonts] = {
    "Courier",
    "Courier-Bold",
    "Courier-BoldOblique",
    "Courier-Oblique",
    "Helvetica",
    "Helvetica-Bold",
    "Helvetica-BoldOblique",
    "Helvetica-Oblique",
    "Times-Roman",
    "Times-Bold",
    "Times-BoldItalic",
    "Times-Italic",
    "Symbol",
    "ZapfDingbats",
};

struct AltFontName {
  const char* m_pName;  // Raw, POD struct.
  CFX_FontMapper::StandardFont m_Index;
};

constexpr AltFontName kAltFontNames[] = {
    {"Arial", CFX_FontMapper::kHelvetica},
    {"Arial,Bold", CFX_FontMapper::kHelveticaBold},
    {"Arial,BoldItalic", CFX_FontMapper::kHelveticaBoldOblique},
    {"Arial,Italic", CFX_FontMapper::kHelveticaOblique},
    {"Arial-Bold", CFX_FontMapper::kHelveticaBold},
    {"Arial-BoldItalic", CFX_FontMapper::kHelveticaBoldOblique},
    {"Arial-BoldItalicMT", CFX_FontMapper::kHelveticaBoldOblique},
    {"Arial-BoldMT", CFX_FontMapper::kHelveticaBold},
    {"Arial-Italic", CFX_FontMapper::kHelveticaOblique},
    {"Arial-ItalicMT", CFX_FontMapper::kHelveticaOblique},
    {"ArialBold", CFX_FontMapper::kHelveticaBold},
    {"ArialBoldItalic", CFX_FontMapper::kHelveticaBoldOblique},
    {"ArialItalic", CFX_FontMapper::kHelveticaOblique},
    {"ArialMT", CFX_FontMapper::kHelvetica},
    {"ArialMT,Bold", CFX_FontMapper::kHelveticaBold},
    {"ArialMT,BoldItalic", CFX_FontMapper::kHelveticaBoldOblique},
    {"ArialMT,Italic", CFX_FontMapper::kHelveticaOblique},
    {"ArialRoundedMTBold", CFX_FontMapper::kHelveticaBold},
    {"Courier", CFX_FontMapper::kCourier},
    {"Courier,Bold", CFX_FontMapper::kCourierBold},
    {"Courier,BoldItalic", CFX_FontMapper::kCourierBoldOblique},
    {"Courier,Italic", CFX_FontMapper::kCourierOblique},
    {"Courier-Bold", CFX_FontMapper::kCourierBold},
    {"Courier-BoldOblique", CFX_FontMapper::kCourierBoldOblique},
    {"Courier-Oblique", CFX_FontMapper::kCourierOblique},
    {"CourierBold", CFX_FontMapper::kCourierBold},
    {"CourierBoldItalic", CFX_FontMapper::kCourierBoldOblique},
    {"CourierItalic", CFX_FontMapper::kCourierOblique},
    {"CourierNew", CFX_FontMapper::kCourier},
    {"CourierNew,Bold", CFX_FontMapper::kCourierBold},
    {"CourierNew,BoldItalic", CFX_FontMapper::kCourierBoldOblique},
    {"CourierNew,Italic", CFX_FontMapper::kCourierOblique},
    {"CourierNew-Bold", CFX_FontMapper::kCourierBold},
    {"CourierNew-BoldItalic", CFX_FontMapper::kCourierBoldOblique},
    {"CourierNew-Italic", CFX_FontMapper::kCourierOblique},
    {"CourierNewBold", CFX_FontMapper::kCourierBold},
    {"CourierNewBoldItalic", CFX_FontMapper::kCourierBoldOblique},
    {"CourierNewItalic", CFX_FontMapper::kCourierOblique},
    {"CourierNewPS-BoldItalicMT", CFX_FontMapper::kCourierBoldOblique},
    {"CourierNewPS-BoldMT", CFX_FontMapper::kCourierBold},
    {"CourierNewPS-ItalicMT", CFX_FontMapper::kCourierOblique},
    {"CourierNewPSMT", CFX_FontMapper::kCourier},
    {"CourierStd", CFX_FontMapper::kCourier},
    {"CourierStd-Bold", CFX_FontMapper::kCourierBold},
    {"CourierStd-BoldOblique", CFX_FontMapper::kCourierBoldOblique},
    {"CourierStd-Oblique", CFX_FontMapper::kCourierOblique},
    {"Helvetica", CFX_FontMapper::kHelvetica},
    {"Helvetica,Bold", CFX_FontMapper::kHelveticaBold},
    {"Helvetica,BoldItalic", CFX_FontMapper::kHelveticaBoldOblique},
    {"Helvetica,Italic", CFX_FontMapper::kHelveticaOblique},
    {"Helvetica-Bold", CFX_FontMapper::kHelveticaBold},
    {"Helvetica-BoldItalic", CFX_FontMapper::kHelveticaBoldOblique},
    {"Helvetica-BoldOblique", CFX_FontMapper::kHelveticaBoldOblique},
    {"Helvetica-Italic", CFX_FontMapper::kHelveticaOblique},
    {"Helvetica-Oblique", CFX_FontMapper::kHelveticaOblique},
    {"HelveticaBold", CFX_FontMapper::kHelveticaBold},
    {"HelveticaBoldItalic", CFX_FontMapper::kHelveticaBoldOblique},
    {"HelveticaItalic", CFX_FontMapper::kHelveticaOblique},
    {"Symbol", CFX_FontMapper::kSymbol},
    {"SymbolMT", CFX_FontMapper::kSymbol},
    {"Times-Bold", CFX_FontMapper::kTimesBold},
    {"Times-BoldItalic", CFX_FontMapper::kTimesBoldOblique},
    {"Times-Italic", CFX_FontMapper::kTimesOblique},
    {"Times-Roman", CFX_FontMapper::kTimes},
    {"TimesBold", CFX_FontMapper::kTimesBold},
    {"TimesBoldItalic", CFX_FontMapper::kTimesBoldOblique},
    {"TimesItalic", CFX_FontMapper::kTimesOblique},
    {"TimesNewRoman", CFX_FontMapper::kTimes},
    {"TimesNewRoman,Bold", CFX_FontMapper::kTimesBold},
    {"TimesNewRoman,BoldItalic", CFX_FontMapper::kTimesBoldOblique},
    {"TimesNewRoman,Italic", CFX_FontMapper::kTimesOblique},
    {"TimesNewRoman-Bold", CFX_FontMapper::kTimesBold},
    {"TimesNewRoman-BoldItalic", CFX_FontMapper::kTimesBoldOblique},
    {"TimesNewRoman-Italic", CFX_FontMapper::kTimesOblique},
    {"TimesNewRomanBold", CFX_FontMapper::kTimesBold},
    {"TimesNewRomanBoldItalic", CFX_FontMapper::kTimesBoldOblique},
    {"TimesNewRomanItalic", CFX_FontMapper::kTimesOblique},
    {"TimesNewRomanPS", CFX_FontMapper::kTimes},
    {"TimesNewRomanPS-Bold", CFX_FontMapper::kTimesBold},
    {"TimesNewRomanPS-BoldItalic", CFX_FontMapper::kTimesBoldOblique},
    {"TimesNewRomanPS-BoldItalicMT", CFX_FontMapper::kTimesBoldOblique},
    {"TimesNewRomanPS-BoldMT", CFX_FontMapper::kTimesBold},
    {"TimesNewRomanPS-Italic", CFX_FontMapper::kTimesOblique},
    {"TimesNewRomanPS-ItalicMT", CFX_FontMapper::kTimesOblique},
    {"TimesNewRomanPSMT", CFX_FontMapper::kTimes},
    {"TimesNewRomanPSMT,Bold", CFX_FontMapper::kTimesBold},
    {"TimesNewRomanPSMT,BoldItalic", CFX_FontMapper::kTimesBoldOblique},
    {"TimesNewRomanPSMT,Italic", CFX_FontMapper::kTimesOblique},
    {"ZapfDingbats", CFX_FontMapper::kDingbats},
};

struct AltFontFamily {
  const char* m_pFontName;    // Raw, POD struct.
  const char* m_pFontFamily;  // Raw, POD struct.
};

constexpr AltFontFamily kAltFontFamilies[] = {
    {"AGaramondPro", "Adobe Garamond Pro"},
    {"BankGothicBT-Medium", "BankGothic Md BT"},
    {"ForteMT", "Forte"},
};

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || defined(OS_ASMJS)
const char kNarrowFamily[] = "LiberationSansNarrow";
#elif BUILDFLAG(IS_ANDROID)
const char kNarrowFamily[] = "RobotoCondensed";
#else
const char kNarrowFamily[] = "ArialNarrow";
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || defined(OS_ASMJS)

ByteString TT_NormalizeName(ByteString norm) {
  norm.Remove(' ');
  norm.Remove('-');
  norm.Remove(',');
  auto pos = norm.Find('+');
  if (pos.has_value() && pos.value() != 0)
    norm = norm.First(pos.value());
  norm.MakeLower();
  return norm;
}

void GetFontFamily(uint32_t nStyle, ByteString* fontName) {
  if (fontName->Contains("Script")) {
    if (FontStyleIsForceBold(nStyle))
      *fontName = "ScriptMTBold";
    else if (fontName->Contains("Palace"))
      *fontName = "PalaceScriptMT";
    else if (fontName->Contains("French"))
      *fontName = "FrenchScriptMT";
    else if (fontName->Contains("FreeStyle"))
      *fontName = "FreeStyleScript";
    return;
  }
  for (const auto& alternate : kAltFontFamilies) {
    if (fontName->Contains(alternate.m_pFontName)) {
      *fontName = alternate.m_pFontFamily;
      return;
    }
  }
}

ByteString ParseStyle(const ByteString& bsStyle, size_t iStart) {
  ByteStringView bsRegion = bsStyle.AsStringView().Substr(iStart);
  size_t iIndex = bsRegion.Find(',').value_or(bsRegion.GetLength());
  return ByteString(bsRegion.First(iIndex));
}

struct FX_FontStyle {
  const char* name;
  size_t len;
  uint32_t style;
};

const FX_FontStyle kFontStyles[] = {
    {"Bold", 4, FXFONT_FORCE_BOLD},
    {"Italic", 6, FXFONT_ITALIC},
    {"BoldItalic", 10, FXFONT_FORCE_BOLD | FXFONT_ITALIC},
    {"Reg", 3, FXFONT_NORMAL},
    {"Regular", 7, FXFONT_NORMAL},
};

// <exists, index, length>
std::tuple<bool, uint32_t, size_t> GetStyleType(const ByteString& bsStyle,
                                                bool reverse_search) {
  if (bsStyle.IsEmpty())
    return std::make_tuple(false, FXFONT_NORMAL, 0);

  for (int i = pdfium::size(kFontStyles) - 1; i >= 0; --i) {
    const FX_FontStyle* pStyle = kFontStyles + i;
    if (!pStyle || pStyle->len > bsStyle.GetLength())
      continue;

    if (reverse_search) {
      if (bsStyle.Last(pStyle->len) == pStyle->name)
        return std::make_tuple(true, pStyle->style, pStyle->len);
    } else {
      if (bsStyle.First(pStyle->len) == pStyle->name)
        return std::make_tuple(true, pStyle->style, pStyle->len);
    }
  }
  return std::make_tuple(false, FXFONT_NORMAL, 0);
}

bool CheckSupportThirdPartFont(const ByteString& name, int* pitch_family) {
  if (name != "MyriadPro")
    return false;
  *pitch_family &= ~FXFONT_FF_ROMAN;
  return true;
}

void UpdatePitchFamily(uint32_t flags, int* pitch_family) {
  if (FontStyleIsSerif(flags))
    *pitch_family |= FXFONT_FF_ROMAN;
  if (FontStyleIsScript(flags))
    *pitch_family |= FXFONT_FF_SCRIPT;
  if (FontStyleIsFixedPitch(flags))
    *pitch_family |= FXFONT_FF_FIXEDPITCH;
}

bool IsStrUpper(const ByteString& str) {
  for (size_t i = 0; i < str.GetLength(); ++i) {
    if (!FXSYS_IsUpperASCII(str[i]))
      return false;
  }
  return true;
}

void RemoveSubsettedFontPrefix(ByteString* subst_name) {
  constexpr size_t kPrefixLength = 6;
  if (subst_name->GetLength() > kPrefixLength &&
      (*subst_name)[kPrefixLength] == '+' &&
      IsStrUpper(subst_name->First(kPrefixLength))) {
    *subst_name =
        subst_name->Last(subst_name->GetLength() - (kPrefixLength + 1));
  }
}

ByteString GetSubstName(const ByteString& name, bool is_truetype) {
  ByteString subst_name = name;
  if (is_truetype && name.Front() == '@')
    subst_name.Delete(0);
  else
    subst_name.Remove(' ');
  RemoveSubsettedFontPrefix(&subst_name);
  CFX_FontMapper::GetStandardFontName(&subst_name);
  return subst_name;
}

bool IsNarrowFontName(const ByteString& name) {
  static const char kNarrowFonts[][10] = {"Narrow", "Condensed"};
  for (const char* font : kNarrowFonts) {
    absl::optional<size_t> pos = name.Find(font);
    if (pos.has_value() && pos.value() != 0)
      return true;
  }
  return false;
}

class ScopedFontDeleter {
 public:
  FX_STACK_ALLOCATED();

  ScopedFontDeleter(SystemFontInfoIface* font_info, void* font)
      : font_info_(font_info), font_(font) {}
  ~ScopedFontDeleter() { font_info_->DeleteFont(font_); }

 private:
  UnownedPtr<SystemFontInfoIface> const font_info_;
  void* const font_;
};

}  // namespace

CFX_FontMapper::CFX_FontMapper(CFX_FontMgr* mgr) : m_pFontMgr(mgr) {}

CFX_FontMapper::~CFX_FontMapper() = default;

void CFX_FontMapper::SetSystemFontInfo(
    std::unique_ptr<SystemFontInfoIface> pFontInfo) {
  if (!pFontInfo)
    return;

  m_bListLoaded = false;
  m_pFontInfo = std::move(pFontInfo);
}

std::unique_ptr<SystemFontInfoIface> CFX_FontMapper::TakeSystemFontInfo() {
  return std::move(m_pFontInfo);
}

uint32_t CFX_FontMapper::GetChecksumFromTT(void* font_handle) {
  uint32_t buffer[256];
  m_pFontInfo->GetFontData(
      font_handle, kTableTTCF,
      pdfium::as_writable_bytes(pdfium::make_span(buffer)));

  uint32_t checksum = 0;
  for (auto x : buffer)
    checksum += x;

  return checksum;
}

ByteString CFX_FontMapper::GetPSNameFromTT(void* font_handle) {
  size_t size = m_pFontInfo->GetFontData(font_handle, kTableNAME, {});
  if (!size)
    return ByteString();

  std::vector<uint8_t, FxAllocAllocator<uint8_t>> buffer(size);
  size_t bytes_read = m_pFontInfo->GetFontData(font_handle, kTableNAME, buffer);
  return bytes_read == size ? GetNameFromTT(buffer, 6) : ByteString();
}

void CFX_FontMapper::AddInstalledFont(const ByteString& name,
                                      FX_Charset charset) {
  if (!m_pFontInfo)
    return;

  m_FaceArray.push_back({name, static_cast<uint32_t>(charset)});
  if (name == m_LastFamily)
    return;

  bool is_localized = std::any_of(name.begin(), name.end(), [](const char& c) {
    return static_cast<uint8_t>(c) > 0x80;
  });

  if (is_localized) {
    void* font_handle = m_pFontInfo->GetFont(name);
    if (!font_handle) {
      font_handle =
          m_pFontInfo->MapFont(0, false, FX_Charset::kDefault, 0, name);
      if (!font_handle)
        return;
    }

    ScopedFontDeleter scoped_font(m_pFontInfo.get(), font_handle);
    ByteString new_name = GetPSNameFromTT(font_handle);
    if (!new_name.IsEmpty())
      m_LocalizedTTFonts.push_back(std::make_pair(new_name, name));
  }
  m_InstalledTTFonts.push_back(name);
  m_LastFamily = name;
}

void CFX_FontMapper::LoadInstalledFonts() {
  if (!m_pFontInfo || m_bListLoaded)
    return;

  m_pFontInfo->EnumFontList(this);
  m_bListLoaded = true;
}

ByteString CFX_FontMapper::MatchInstalledFonts(const ByteString& norm_name) {
  LoadInstalledFonts();
  int i;
  for (i = fxcrt::CollectionSize<int>(m_InstalledTTFonts) - 1; i >= 0; i--) {
    if (TT_NormalizeName(m_InstalledTTFonts[i]) == norm_name)
      return m_InstalledTTFonts[i];
  }
  for (i = fxcrt::CollectionSize<int>(m_LocalizedTTFonts) - 1; i >= 0; i--) {
    if (TT_NormalizeName(m_LocalizedTTFonts[i].first) == norm_name)
      return m_LocalizedTTFonts[i].second;
  }
  return ByteString();
}

RetainPtr<CFX_Face> CFX_FontMapper::UseInternalSubst(
    int base_font,
    int weight,
    int italic_angle,
    int pitch_family,
    CFX_SubstFont* subst_font) {
  if (base_font < kNumStandardFonts) {
    if (!m_StandardFaces[base_font]) {
      m_StandardFaces[base_font] = m_pFontMgr->NewFixedFace(
          nullptr, m_pFontMgr->GetStandardFont(base_font), 0);
    }
    return m_StandardFaces[base_font];
  }

  subst_font->m_bFlagMM = true;
  subst_font->m_ItalicAngle = italic_angle;
  if (weight)
    subst_font->m_Weight = weight;
  if (FontFamilyIsRoman(pitch_family)) {
    subst_font->UseChromeSerif();
    if (!m_GenericSerifFace) {
      m_GenericSerifFace = m_pFontMgr->NewFixedFace(
          nullptr, m_pFontMgr->GetGenericSerifFont(), 0);
    }
    return m_GenericSerifFace;
  }
  subst_font->m_Family = "Chrome Sans";
  if (!m_GenericSansFace) {
    m_GenericSansFace =
        m_pFontMgr->NewFixedFace(nullptr, m_pFontMgr->GetGenericSansFont(), 0);
  }
  return m_GenericSansFace;
}

RetainPtr<CFX_Face> CFX_FontMapper::UseExternalSubst(
    void* font_handle,
    ByteString face_name,
    int weight,
    bool is_italic,
    int italic_angle,
    FX_Charset charset,
    CFX_SubstFont* subst_font) {
  if (!font_handle)
    return nullptr;

  ScopedFontDeleter scoped_font(m_pFontInfo.get(), font_handle);
  m_pFontInfo->GetFaceName(font_handle, &face_name);
  if (charset == FX_Charset::kDefault)
    m_pFontInfo->GetFontCharset(font_handle, &charset);
  size_t ttc_size = m_pFontInfo->GetFontData(font_handle, kTableTTCF, {});
  size_t font_size = m_pFontInfo->GetFontData(font_handle, 0, {});
  if (font_size == 0 && ttc_size == 0)
    return nullptr;

  RetainPtr<CFX_Face> face =
      ttc_size
          ? GetCachedTTCFace(font_handle, ttc_size, font_size)
          : GetCachedFace(font_handle, face_name, weight, is_italic, font_size);
  if (!face)
    return nullptr;

  subst_font->m_Family = face_name;
  subst_font->m_Charset = charset;
  int face_weight =
      FXFT_Is_Face_Bold(face->GetRec()) ? FXFONT_FW_BOLD : FXFONT_FW_NORMAL;
  if (weight != face_weight)
    subst_font->m_Weight = weight;
  if (is_italic && !FXFT_Is_Face_Italic(face->GetRec())) {
    if (italic_angle == 0)
      italic_angle = -12;
    else if (abs(italic_angle) < 5)
      italic_angle = 0;
    subst_font->m_ItalicAngle = italic_angle;
  }
  return face;
}

RetainPtr<CFX_Face> CFX_FontMapper::FindSubstFont(const ByteString& name,
                                                  bool is_truetype,
                                                  uint32_t flags,
                                                  int weight,
                                                  int italic_angle,
                                                  FX_CodePage code_page,
                                                  CFX_SubstFont* subst_font) {
  if (weight == 0)
    weight = FXFONT_FW_NORMAL;

  if (!(flags & FXFONT_USEEXTERNATTR)) {
    weight = FXFONT_FW_NORMAL;
    italic_angle = 0;
  }
  const ByteString subst_name = GetSubstName(name, is_truetype);
  if (subst_name == "Symbol" && !is_truetype) {
    subst_font->m_Family = "Chrome Symbol";
    subst_font->m_Charset = FX_Charset::kSymbol;
    return UseInternalSubst(kSymbol, weight, italic_angle, 0, subst_font);
  }
  if (subst_name == "ZapfDingbats") {
    subst_font->m_Family = "Chrome Dingbats";
    subst_font->m_Charset = FX_Charset::kSymbol;
    return UseInternalSubst(kDingbats, weight, italic_angle, 0, subst_font);
  }
  int base_font = 0;
  ByteString family;
  ByteString style;
  bool has_comma = false;
  bool has_hyphen = false;
  {
    absl::optional<size_t> pos = subst_name.Find(",");
    if (pos.has_value()) {
      family = subst_name.First(pos.value());
      GetStandardFontName(&family);
      style = subst_name.Last(subst_name.GetLength() - (pos.value() + 1));
      has_comma = true;
    } else {
      family = subst_name;
    }
  }
  for (; base_font < kSymbol; base_font++) {
    if (family == kBase14FontNames[base_font])
      break;
  }
  int pitch_family = 0;
  uint32_t nStyle = FXFONT_NORMAL;
  bool is_style_available = false;
  if (base_font < kSymbol) {
    if ((base_font % 4) == 1 || (base_font % 4) == 2)
      nStyle |= FXFONT_FORCE_BOLD;
    if ((base_font % 4) / 2)
      nStyle |= FXFONT_ITALIC;
    if (base_font < 4)
      pitch_family |= FXFONT_FF_FIXEDPITCH;
    if (base_font >= 8)
      pitch_family |= FXFONT_FF_ROMAN;
  } else {
    base_font = kNumStandardFonts;
    if (!has_comma) {
      absl::optional<size_t> pos = family.ReverseFind('-');
      if (pos.has_value()) {
        style = family.Last(family.GetLength() - (pos.value() + 1));
        family = family.First(pos.value());
        has_hyphen = true;
      }
    }
    if (!has_hyphen) {
      size_t nLen = family.GetLength();
      bool has_style_type;
      uint32_t style_type;
      size_t len;
      std::tie(has_style_type, style_type, len) =
          GetStyleType(family, /*reverse_search=*/true);
      if (has_style_type) {
        family = family.First(nLen - len);
        nStyle |= style_type;
      }
    }
    UpdatePitchFamily(flags, &pitch_family);
  }

  const int old_weight = weight;
  if (FontStyleIsForceBold(nStyle))
    weight = FXFONT_FW_BOLD;

  if (!style.IsEmpty()) {
    size_t i = 0;
    bool is_first_item = true;
    while (i < style.GetLength()) {
      ByteString buf = ParseStyle(style, i);
      bool has_style_type;
      uint32_t style_type;
      size_t len;
      std::tie(has_style_type, style_type, len) =
          GetStyleType(buf, /*reverse_search=*/false);
      if ((i && !is_style_available) || (!i && !has_style_type)) {
        family = subst_name;
        base_font = kNumStandardFonts;
        break;
      }
      if (has_style_type)
        is_style_available = true;

      if (FontStyleIsForceBold(style_type)) {
        // If we're already bold, then we're double bold, use special weight.
        if (FontStyleIsForceBold(nStyle)) {
          weight = FXFONT_FW_BOLD_BOLD;
        } else {
          weight = FXFONT_FW_BOLD;
          nStyle |= FXFONT_FORCE_BOLD;
        }

        is_first_item = false;
      }
      if (FontStyleIsItalic(style_type) && FontStyleIsForceBold(style_type)) {
        nStyle |= FXFONT_ITALIC;
      } else if (FontStyleIsItalic(style_type)) {
        if (is_first_item) {
          nStyle |= FXFONT_ITALIC;
        } else {
          family = subst_name;
          base_font = kNumStandardFonts;
        }
        break;
      }
      i += buf.GetLength() + 1;
    }
  }

  if (!m_pFontInfo) {
    return UseInternalSubst(base_font, old_weight, italic_angle, pitch_family,
                            subst_font);
  }

  FX_Charset Charset = FX_Charset::kANSI;
  if (code_page != FX_CodePage::kDefANSI)
    Charset = FX_GetCharsetFromCodePage(code_page);
  else if (base_font == kNumStandardFonts && FontStyleIsSymbolic(flags))
    Charset = FX_Charset::kSymbol;
  const bool is_cjk = FX_CharSetIsCJK(Charset);
  bool is_italic = FontStyleIsItalic(nStyle);

  GetFontFamily(nStyle, &family);
  ByteString match = MatchInstalledFonts(TT_NormalizeName(family));
  if (match.IsEmpty() && family != subst_name &&
      (!has_comma && (!has_hyphen || (has_hyphen && !is_style_available)))) {
    match = MatchInstalledFonts(TT_NormalizeName(subst_name));
  }
  if (match.IsEmpty() && base_font >= kNumStandardFonts) {
    if (!is_cjk) {
      if (!CheckSupportThirdPartFont(family, &pitch_family)) {
        is_italic = italic_angle != 0;
        weight = old_weight;
      }
      if (IsNarrowFontName(subst_name))
        family = kNarrowFamily;
    } else {
      subst_font->m_bSubstCJK = true;
      if (nStyle)
        subst_font->m_WeightCJK = nStyle ? weight : FXFONT_FW_NORMAL;
      if (FontStyleIsItalic(nStyle))
        subst_font->m_bItalicCJK = true;
    }
  } else {
    italic_angle = 0;
    if (nStyle == FXFONT_NORMAL)
      weight = FXFONT_FW_NORMAL;
  }

  if (!match.IsEmpty() || base_font < kNumStandardFonts) {
    if (!match.IsEmpty())
      family = match;
    if (base_font < kNumStandardFonts) {
      if (nStyle && !(base_font % 4)) {
        if (FontStyleIsForceBold(nStyle) && FontStyleIsItalic(nStyle))
          base_font += 2;
        else if (FontStyleIsForceBold(nStyle))
          base_font += 1;
        else if (FontStyleIsItalic(nStyle))
          base_font += 3;
      }
      family = kBase14FontNames[base_font];
    }
  } else if (FontStyleIsItalic(flags)) {
    is_italic = true;
  }
  void* font_handle =
      m_pFontInfo->MapFont(weight, is_italic, Charset, pitch_family, family);
  if (!font_handle) {
    if (is_cjk) {
      is_italic = italic_angle != 0;
      weight = old_weight;
    }
    if (!match.IsEmpty()) {
      font_handle = m_pFontInfo->GetFont(match);
      if (!font_handle) {
        return UseInternalSubst(base_font, old_weight, italic_angle,
                                pitch_family, subst_font);
      }
    } else {
      if (Charset == FX_Charset::kSymbol) {
#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_ANDROID)
        if (subst_name == "Symbol") {
          subst_font->m_Family = "Chrome Symbol";
          subst_font->m_Charset = FX_Charset::kSymbol;
          return UseInternalSubst(kSymbol, old_weight, italic_angle,
                                  pitch_family, subst_font);
        }
#endif
        return FindSubstFont(family, is_truetype, flags & ~FXFONT_SYMBOLIC,
                             weight, italic_angle, FX_CodePage::kDefANSI,
                             subst_font);
      }
      if (Charset == FX_Charset::kANSI) {
        return UseInternalSubst(base_font, old_weight, italic_angle,
                                pitch_family, subst_font);
      }

      auto it =
          std::find_if(m_FaceArray.begin(), m_FaceArray.end(),
                       [Charset](const FaceData& face) {
                         return face.charset == static_cast<uint32_t>(Charset);
                       });
      if (it == m_FaceArray.end()) {
        return UseInternalSubst(base_font, old_weight, italic_angle,
                                pitch_family, subst_font);
      }
      font_handle = m_pFontInfo->GetFont(it->name);
    }
  }
  return UseExternalSubst(font_handle, subst_name, weight, is_italic,
                          italic_angle, Charset, subst_font);
}

size_t CFX_FontMapper::GetFaceSize() const {
  return m_FaceArray.size();
}

ByteString CFX_FontMapper::GetFaceName(size_t index) const {
  CHECK_LT(index, m_FaceArray.size());
  return m_FaceArray[index].name;
}

bool CFX_FontMapper::HasInstalledFont(ByteStringView name) const {
  for (const auto& font : m_InstalledTTFonts) {
    if (font == name)
      return true;
  }
  return false;
}

bool CFX_FontMapper::HasLocalizedFont(ByteStringView name) const {
  for (const auto& fontPair : m_LocalizedTTFonts) {
    if (fontPair.first == name)
      return true;
  }
  return false;
}

#if BUILDFLAG(IS_WIN)
absl::optional<ByteString> CFX_FontMapper::InstalledFontNameStartingWith(
    const ByteString& name) const {
  for (const auto& thisname : m_InstalledTTFonts) {
    if (thisname.First(name.GetLength()) == name)
      return thisname;
  }
  return absl::nullopt;
}

absl::optional<ByteString> CFX_FontMapper::LocalizedFontNameStartingWith(
    const ByteString& name) const {
  for (const auto& thispair : m_LocalizedTTFonts) {
    if (thispair.first.First(name.GetLength()) == name)
      return thispair.second;
  }
  return absl::nullopt;
}
#endif  // BUILDFLAG(IS_WIN)

#ifdef PDF_ENABLE_XFA
std::unique_ptr<uint8_t, FxFreeDeleter> CFX_FontMapper::RawBytesForIndex(
    size_t index,
    size_t* returned_length) {
  CHECK_LT(index, m_FaceArray.size());

  void* font_handle = m_pFontInfo->MapFont(0, false, FX_Charset::kDefault, 0,
                                           GetFaceName(index));
  if (!font_handle)
    return nullptr;

  ScopedFontDeleter scoped_font(m_pFontInfo.get(), font_handle);
  size_t required_size = m_pFontInfo->GetFontData(font_handle, 0, {});
  if (required_size == 0)
    return nullptr;

  std::unique_ptr<uint8_t, FxFreeDeleter> pBuffer(
      FX_Alloc(uint8_t, required_size + 1));
  *returned_length =
      m_pFontInfo->GetFontData(font_handle, 0, {pBuffer.get(), required_size});
  return pBuffer;
}
#endif  // PDF_ENABLE_XFA

RetainPtr<CFX_Face> CFX_FontMapper::GetCachedTTCFace(void* font_handle,
                                                     size_t ttc_size,
                                                     size_t data_size) {
  uint32_t checksum = GetChecksumFromTT(font_handle);
  RetainPtr<CFX_FontMgr::FontDesc> pFontDesc =
      m_pFontMgr->GetCachedTTCFontDesc(ttc_size, checksum);
  if (!pFontDesc) {
    std::unique_ptr<uint8_t, FxFreeDeleter> pFontData(
        FX_Alloc(uint8_t, ttc_size));
    m_pFontInfo->GetFontData(font_handle, kTableTTCF,
                             {pFontData.get(), ttc_size});
    pFontDesc = m_pFontMgr->AddCachedTTCFontDesc(
        ttc_size, checksum, std::move(pFontData), ttc_size);
  }
  CHECK(ttc_size >= data_size);
  size_t font_offset = ttc_size - data_size;
  size_t face_index =
      GetTTCIndex(pFontDesc->FontData().first(ttc_size), font_offset);
  RetainPtr<CFX_Face> pFace(pFontDesc->GetFace(face_index));
  if (pFace)
    return pFace;

  pFace = m_pFontMgr->NewFixedFace(
      pFontDesc, pFontDesc->FontData().first(ttc_size), face_index);
  if (!pFace)
    return nullptr;

  pFontDesc->SetFace(face_index, pFace.Get());
  return pFace;
}

RetainPtr<CFX_Face> CFX_FontMapper::GetCachedFace(void* font_handle,
                                                  ByteString subst_name,
                                                  int weight,
                                                  bool is_italic,
                                                  size_t data_size) {
  RetainPtr<CFX_FontMgr::FontDesc> pFontDesc =
      m_pFontMgr->GetCachedFontDesc(subst_name, weight, is_italic);
  if (!pFontDesc) {
    std::unique_ptr<uint8_t, FxFreeDeleter> pFontData(
        FX_Alloc(uint8_t, data_size));
    m_pFontInfo->GetFontData(font_handle, 0, {pFontData.get(), data_size});
    pFontDesc = m_pFontMgr->AddCachedFontDesc(subst_name, weight, is_italic,
                                              std::move(pFontData), data_size);
  }
  RetainPtr<CFX_Face> pFace(pFontDesc->GetFace(0));
  if (pFace)
    return pFace;

  pFace = m_pFontMgr->NewFixedFace(pFontDesc,
                                   pFontDesc->FontData().first(data_size), 0);
  if (!pFace)
    return nullptr;

  pFontDesc->SetFace(0, pFace.Get());
  return pFace;
}

// static
absl::optional<CFX_FontMapper::StandardFont>
CFX_FontMapper::GetStandardFontName(ByteString* name) {
  const auto* end = std::end(kAltFontNames);
  const auto* found =
      std::lower_bound(std::begin(kAltFontNames), end, name->c_str(),
                       [](const AltFontName& element, const char* name) {
                         return FXSYS_stricmp(element.m_pName, name) < 0;
                       });
  if (found == end || FXSYS_stricmp(found->m_pName, name->c_str()))
    return absl::nullopt;

  *name = kBase14FontNames[static_cast<size_t>(found->m_Index)];
  return found->m_Index;
}

// static
bool CFX_FontMapper::IsStandardFontName(const ByteString& name) {
  return pdfium::Contains(kBase14FontNames, name);
}

// static
bool CFX_FontMapper::IsSymbolicFont(StandardFont font) {
  return font == StandardFont::kSymbol || font == StandardFont::kDingbats;
}

// static
bool CFX_FontMapper::IsFixedFont(StandardFont font) {
  return font == StandardFont::kCourier || font == StandardFont::kCourierBold ||
         font == StandardFont::kCourierBoldOblique ||
         font == StandardFont::kCourierOblique;
}
