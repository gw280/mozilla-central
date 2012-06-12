/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_FT2EXTENTS_H
#define GFX_FT2EXTENTS_H

#include "cairo-ft.h"
#include "gfxFont.h"

typedef struct FT_FaceRec_* FT_Face;

class gfxFT2Extents {
public:
    /**
     * Get text extents for a particular glyph for a particular FT_Face for a given
     * style.
     */
    virtual void GetGlyphExtents(const gfxFontStyle* aStyle,
                                 PRUint32 aGlyphID,
                                 cairo_text_extents_t* aExtents) = 0;

    /**
      * Generate font metrics for a given FT_Face
      * This is used by the Cairo and FreeType-only gfxFont implementations
      */
    virtual void GenerateMetrics(gfxFont* aFont,
                                 gfxFont::Metrics* aMetrics,
                                 PRUint32* aSpaceGlyph) = 0;

    virtual bool GetFontTable(PRUint32 aTag, FallibleTArray<PRUint8>& aBuffer) = 0;
    virtual PRUint32 GetGlyph(PRUint32 aCharCode, PRUint32 aVariantSelector = 0) = 0;
    virtual gfxFloat XScale() = 0;
};

class gfxFT2CairoExtents : public gfxFT2Extents {
public:
    gfxFT2CairoExtents(cairo_scaled_font_t* aScaledFont) :
        mScaledFont(aScaledFont)
    { }

    virtual void GetGlyphExtents(const gfxFontStyle* aStyle,
                                 PRUint32 aGlyphID,
                                 cairo_text_extents_t* aExtents);
    
    virtual void GenerateMetrics(gfxFont* aFont,
                                 gfxFont::Metrics* aMetrics,
                                 PRUint32* aSpaceGlyph);

    virtual bool GetFontTable(PRUint32 aTag, FallibleTArray<PRUint8>& aBuffer);
    virtual PRUint32 GetGlyph(PRUint32 aCharCode, PRUint32 aVariantSelector = 0);
    virtual gfxFloat XScale();
private:
    cairo_scaled_font_t* mScaledFont;
};

#endif /* GFX_FT2EXTENTS_H */
