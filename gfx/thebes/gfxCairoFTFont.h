/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_CAIROFONTBASE_H
#define GFX_CAIROFONTBASE_H

#include "cairo.h"
#include "gfxContext.h"
#include "gfxFont.h"

class gfxFT2Extents;

class gfxCairoFTFont : public gfxFont {
public:
    gfxCairoFTFont(cairo_scaled_font_t *aScaledFont,
                     gfxFontEntry *aFontEntry,
                     const gfxFontStyle *aFontStyle);
    virtual ~gfxCairoFTFont();

    PRUint32 GetGlyph(PRUint32 aCharCode);
    void GetGlyphExtents(PRUint32 aGlyph,
                         cairo_text_extents_t* aExtents);
    virtual const gfxFont::Metrics& GetMetrics();
    virtual PRUint32 GetSpaceGlyph();
    virtual hb_blob_t *GetFontTable(PRUint32 aTag);
    virtual bool ProvidesGetGlyph() const { return true; }
    virtual PRUint32 GetGlyph(PRUint32 unicode, PRUint32 variation_selector);
    virtual bool ProvidesGlyphWidths() { return true; }
    virtual PRInt32 GetGlyphWidth(gfxContext *aCtx, PRUint16 aGID);

    cairo_scaled_font_t *CairoScaledFont() { return mScaledFont; };
    virtual bool SetupCairoFont(gfxContext *aContext);

    virtual FontType GetType() const { return FONT_TYPE_FT2; }
protected:
    PRUint32 mSpaceGlyph;
    bool mHasMetrics;
    Metrics mMetrics;
    gfxFT2Extents* mFontExtents;
};

#endif /* GFX_CAIROFONTBASE_H */
