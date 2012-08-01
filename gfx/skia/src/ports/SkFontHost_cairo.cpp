
/*
 * Copyright 2012 Mozilla Foundation
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "cairo.h"
#include "cairo-ft.h"

#include "SkFontHost_FreeType_common.h"

#include "SkAdvancedTypefaceMetrics.h"
#include "SkFontHost.h"
#include "SkPath.h"
#include "SkScalerContext.h"
#include "SkTypefaceCache.h"

#include <ft2build.h>
#include FT_FREETYPE_H

static cairo_user_data_key_t kSkTypefaceKey;

class SkScalerContext_CairoFT : public SkScalerContext_FreeType_Base {
public:
    SkScalerContext_CairoFT(SkTypeface* typeface, const SkDescriptor* desc);
    virtual ~SkScalerContext_CairoFT();

protected:
    virtual unsigned generateGlyphCount() SK_OVERRIDE;
    virtual uint16_t generateCharToGlyph(SkUnichar uniChar) SK_OVERRIDE;
    virtual void generateAdvance(SkGlyph* glyph) SK_OVERRIDE;
    virtual void generateMetrics(SkGlyph* glyph) SK_OVERRIDE;
    virtual void generateImage(const SkGlyph& glyph) SK_OVERRIDE;
    virtual void generatePath(const SkGlyph& glyph, SkPath* path) SK_OVERRIDE;
    virtual void generateFontMetrics(SkPaint::FontMetrics* mx,
                                     SkPaint::FontMetrics* my) SK_OVERRIDE;
    virtual SkUnichar generateGlyphToChar(uint16_t glyph) SK_OVERRIDE;
private:
    cairo_scaled_font_t* fScaledFont;
    uint32_t fLoadGlyphFlags;
};

class CairoLockedFTFace {
public:
    CairoLockedFTFace(cairo_scaled_font_t* scaledFont)
        : fScaledFont(scaledFont)
        , fFace(cairo_ft_scaled_font_lock_face(scaledFont))
    {}

    ~CairoLockedFTFace()
    {
        cairo_ft_scaled_font_unlock_face(fScaledFont);
    }

    FT_Face getFace()
    {
        return fFace;
    }

private:
    cairo_scaled_font_t* fScaledFont;
    FT_Face fFace;
};

class SkCairoFTTypeface : public SkTypeface {
public:
    static SkTypeface* CreateTypeface(cairo_font_face_t* fontFace, SkTypeface::Style style, bool isFixedWidth) {
        SkASSERT(fontFace != NULL);
        SkASSERT(cairo_font_face_get_type(fontFace) == CAIRO_FONT_TYPE_FT);

        SkFontID newId = SkTypefaceCache::NewFontID();

        return SkNEW_ARGS(SkCairoFTTypeface, (fontFace, style, newId, isFixedWidth));
    }

    cairo_font_face_t* getFontFace() {
        return fFontFace;
    }

    virtual SkStream* onOpenStream(int*) const SK_OVERRIDE { return NULL; }

    virtual SkAdvancedTypefaceMetrics*
        onGetAdvancedTypefaceMetrics(SkAdvancedTypefaceMetrics::PerGlyphInfo,
                                     const uint32_t*, uint32_t) const SK_OVERRIDE
    {
        return NULL;
    }

    virtual SkScalerContext* onCreateScalerContext(const SkDescriptor* desc) const SK_OVERRIDE
    {
        return SkNEW_ARGS(SkScalerContext_CairoFT, (const_cast<SkCairoFTTypeface*>(this), desc));
    }

    virtual void onFilterRec(SkScalerContextRec*) const SK_OVERRIDE
    {
        return;
    }

    virtual void onGetFontDescriptor(SkFontDescriptor*, bool*) const SK_OVERRIDE
    {
        return;
    }


private:

    SkCairoFTTypeface(cairo_font_face_t* fontFace, SkTypeface::Style style, SkFontID id, bool isFixedWidth)
        : SkTypeface(style, id, isFixedWidth)
        , fFontFace(fontFace)
    {
        cairo_font_face_set_user_data(fFontFace, &kSkTypefaceKey, this, NULL);
    }

    ~SkCairoFTTypeface()
    {
        cairo_font_face_set_user_data(fFontFace, &kSkTypefaceKey, NULL, NULL);
    }

    cairo_font_face_t* fFontFace;
};

static bool FindByCairoFont(SkTypeface* typeface, SkTypeface::Style style, void* ctx)
{
    cairo_font_face_t* fontFace = static_cast<cairo_font_face_t*>(ctx);
    SkCairoFTTypeface* cairoTypeface = static_cast<SkCairoFTTypeface*>(typeface);
    cairo_font_face_t* comparisonFont = cairoTypeface->getFontFace();

    return fontFace == comparisonFont;
}

SkTypeface* SkCreateTypefaceFromCairoFont(cairo_font_face_t* fontFace, SkTypeface::Style style, bool isFixedWidth)
{
    SkTypeface* typeface = reinterpret_cast<SkTypeface*>(cairo_font_face_get_user_data(fontFace, &kSkTypefaceKey));

    if (typeface) {
        typeface->ref();
    } else {
        typeface = SkCairoFTTypeface::CreateTypeface(fontFace, style, isFixedWidth);
        SkTypefaceCache::Add(typeface, style);
    }

    return typeface;
}

SkTypeface* SkFontHost::CreateTypeface(const SkTypeface* familyFace,
                                     const char famillyName[],
                                     SkTypeface::Style style)
{
    SkDEBUGFAIL("SkFontHost::FindTypeface unimplemented");
    return NULL;
}

SkTypeface* SkFontHost::CreateTypefaceFromStream(SkStream*)
{
    SkDEBUGFAIL("SkFontHost::CreateTypeface unimplemented");
    return NULL;
}

SkTypeface* SkFontHost::CreateTypefaceFromFile(char const*)
{
    SkDEBUGFAIL("SkFontHost::CreateTypefaceFromFile unimplemented");
    return NULL;
}

///////////////////////////////////////////////////////////////////////////////

static bool isLCD(const SkScalerContext::Rec& rec) {
    switch (rec.fMaskFormat) {
        case SkMask::kLCD16_Format:
        case SkMask::kLCD32_Format:
            return true;
        default:
            return false;
    }
}

///////////////////////////////////////////////////////////////////////////////
SkScalerContext_CairoFT::SkScalerContext_CairoFT(SkTypeface* typeface, const SkDescriptor* desc)
    : SkScalerContext_FreeType_Base(typeface, desc)
{
    SkMatrix matrix;
    fRec.getSingleMatrix(&matrix);

    cairo_font_face_t* fontFace = static_cast<SkCairoFTTypeface*>(typeface)->getFontFace();

    cairo_matrix_t fontMatrix, ctMatrix;
    cairo_matrix_init(&fontMatrix, matrix.getScaleX(), matrix.getSkewX(), matrix.getSkewY(), matrix.getScaleY(), 0.0, 0.0);
    cairo_matrix_init_scale(&ctMatrix, 1.0, 1.0);

    // We can just use the default load options here because we're not using
    // cairo to actually do any rendering but rather calling FreeType directly
    // with our own flags
    cairo_font_options_t *fontOptions = cairo_font_options_create();

    fScaledFont = cairo_scaled_font_create(fontFace, &fontMatrix, &ctMatrix, fontOptions);

    FT_Int32 loadFlags = FT_LOAD_DEFAULT;
    bool linearMetrics = SkToBool(fRec.fFlags & SkScalerContext::kSubpixelPositioning_Flag);

    if (SkMask::kBW_Format == fRec.fMaskFormat) {
        // See http://code.google.com/p/chromium/issues/detail?id=43252#c24
        loadFlags = FT_LOAD_TARGET_MONO;
        if (fRec.getHinting() == SkPaint::kNo_Hinting) {
            loadFlags = FT_LOAD_NO_HINTING;
        }
    } else {
        switch (fRec.getHinting()) {
        case SkPaint::kNo_Hinting:
            loadFlags = FT_LOAD_NO_HINTING;
            break;
        case SkPaint::kSlight_Hinting:
            loadFlags = FT_LOAD_TARGET_LIGHT;  // This implies FORCE_AUTOHINT
            break;
        case SkPaint::kNormal_Hinting:
            if (fRec.fFlags & SkScalerContext::kAutohinting_Flag)
                loadFlags = FT_LOAD_FORCE_AUTOHINT;
            else
                loadFlags = FT_LOAD_NO_AUTOHINT;
            break;
        case SkPaint::kFull_Hinting:
            if (fRec.fFlags & SkScalerContext::kAutohinting_Flag) {
                loadFlags = FT_LOAD_FORCE_AUTOHINT;
                break;
            }
            loadFlags = FT_LOAD_TARGET_NORMAL;
            if (isLCD(fRec)) {
                if (SkToBool(fRec.fFlags & SkScalerContext::kLCD_Vertical_Flag)) {
                    loadFlags = FT_LOAD_TARGET_LCD_V;
                } else {
                    loadFlags = FT_LOAD_TARGET_LCD;
                }
            }
            break;
        default:
            SkDebugf("---------- UNKNOWN hinting %d\n", fRec.getHinting());
            break;
        }
    }

    if ((fRec.fFlags & SkScalerContext::kEmbeddedBitmapText_Flag) == 0) {
        loadFlags |= FT_LOAD_NO_BITMAP;
    }

    // Always using FT_LOAD_IGNORE_GLOBAL_ADVANCE_WIDTH to get correct
    // advances, as fontconfig and cairo do.
    // See http://code.google.com/p/skia/issues/detail?id=222.
    loadFlags |= FT_LOAD_IGNORE_GLOBAL_ADVANCE_WIDTH;

    fLoadGlyphFlags = loadFlags;
}

SkScalerContext_CairoFT::~SkScalerContext_CairoFT()
{
}

#ifdef SK_BUILD_FOR_ANDROID
uint32_t SkFontHost::GetUnitsPerEm(SkFontID fontID) {
    return 0;
}
#endif

unsigned SkScalerContext_CairoFT::generateGlyphCount()
{
    CairoLockedFTFace faceLock(fScaledFont);
    return faceLock.getFace()->num_glyphs;
}

uint16_t SkScalerContext_CairoFT::generateCharToGlyph(SkUnichar uniChar)
{
    CairoLockedFTFace faceLock(fScaledFont);
    return SkToU16(FT_Get_Char_Index(faceLock.getFace(), uniChar));
}

void SkScalerContext_CairoFT::generateAdvance(SkGlyph* glyph)
{
    cairo_text_extents_t extents;
    cairo_glyph_t cairoGlyph = { glyph->getGlyphID(fBaseGlyphCount), 0.0, 0.0 };
    cairo_scaled_font_glyph_extents(fScaledFont, &cairoGlyph, 1, &extents);
    glyph->fAdvanceX = SkDoubleToFixed(SkScalarCeil(extents.x_advance));
    glyph->fAdvanceY = SkDoubleToFixed(SkScalarCeil(extents.y_advance));
}

void SkScalerContext_CairoFT::generateMetrics(SkGlyph* glyph)
{
    SkASSERT(fScaledFont != NULL);
    cairo_text_extents_t extents;
    cairo_glyph_t cairoGlyph = { glyph->getGlyphID(fBaseGlyphCount), 0.0, 0.0 };
    cairo_scaled_font_glyph_extents(fScaledFont, &cairoGlyph, 1, &extents);

    glyph->fAdvanceX = SkDoubleToFixed(extents.x_advance);
    glyph->fAdvanceY = SkDoubleToFixed(extents.y_advance);
    glyph->fWidth = SkToU16(SkScalarCeil(extents.width));
    glyph->fHeight = SkToU16(SkScalarCeil(extents.height));
    glyph->fLeft = SkToS16(SkScalarCeil(extents.x_bearing));
    glyph->fTop = SkToS16(SkScalarCeil(extents.y_bearing));
    glyph->fLsbDelta = 0;
    glyph->fRsbDelta = 0;
}

void SkScalerContext_CairoFT::generateImage(const SkGlyph& glyph)
{
    SkASSERT(fScaledFont != NULL);
    CairoLockedFTFace faceLock(fScaledFont);
    FT_Face face = faceLock.getFace();

    FT_Error err = FT_Load_Glyph(face, glyph.getGlyphID(fBaseGlyphCount), fLoadGlyphFlags);

    if (err != 0) {
        memset(glyph.fImage, 0, glyph.rowBytes() * glyph.fHeight);
        return;
    }

    generateGlyphImage(face, glyph);
}

void SkScalerContext_CairoFT::generatePath(const SkGlyph& glyph, SkPath* path)
{
    SkASSERT(fScaledFont != NULL);
    CairoLockedFTFace faceLock(fScaledFont);
    FT_Face face = faceLock.getFace();

    SkASSERT(&glyph && path);

    uint32_t flags = fLoadGlyphFlags;
    flags |= FT_LOAD_NO_BITMAP; // ignore embedded bitmaps so we're sure to get the outline
    flags &= ~FT_LOAD_RENDER;   // don't scan convert (we just want the outline)

    FT_Error err = FT_Load_Glyph(face, glyph.getGlyphID(fBaseGlyphCount), flags);

    if (err != 0) {
        path->reset();
        return;
    }

    generateGlyphPath(face, path);
}

void SkScalerContext_CairoFT::generateFontMetrics(SkPaint::FontMetrics* mx,
                                                  SkPaint::FontMetrics* my)
{
}

SkUnichar SkScalerContext_CairoFT::generateGlyphToChar(uint16_t glyph)
{
    SkASSERT(fScaledFont != NULL);
    CairoLockedFTFace faceLock(fScaledFont);
    FT_Face face = faceLock.getFace();

    FT_UInt glyphIndex;
    SkUnichar charCode = FT_Get_First_Char(face, &glyphIndex);
    while (glyphIndex != 0) {
        if (glyphIndex == glyph) {
            return charCode;
        }
        charCode = FT_Get_Next_Char(face, charCode, &glyphIndex);
    }

    return 0;
}

