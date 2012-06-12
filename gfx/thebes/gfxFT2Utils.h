/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_FT2UTILS_H
#define GFX_FT2UTILS_H

#include "cairo-ft.h"

// Rounding and truncation functions for a FreeType fixed point number 
// (FT26Dot6) stored in a 32bit integer with high 26 bits for the integer
// part and low 6 bits for the fractional part. 
#define FLOAT_FROM_26_6(x) ((x) / 64.0)
#define FLOAT_TO_26_6(x) ((x) * 64.0)

#define FLOAT_FROM_16_16(x) ((x) / 65536.0)
#define FLOAT_TO_16_16(x) ((x) * 65536.0)

#define ROUND_26_6_TO_INT(x) ((x) >= 0 ?  ((32 + (x)) >> 6) \
                                       : -((32 - (x)) >> 6))

typedef struct FT_FaceRec_* FT_Face;

namespace gfxFT2Utils {

bool GetFontTable(FT_Face aFace, PRUint32 aTag, FallibleTArray<PRUint8>& aBuffer);

/**
 * Get the glyph id for a Unicode character representable by a single
 * glyph, or return zero if there is no such glyph.  This does no caching,
 * so you probably want gfxFcFont::GetGlyph.
 */
PRUint32 GetGlyph(FT_Face aFace, PRUint32 aCharCode, PRUint32 aVariantSelector = 0);

/** 
  * A scale factor for use in converting horizontal metrics from font units
  * to pixels.
  */
gfxFloat XScale(FT_Face aFace);

} // namespace gfxFT2Utils

/**
  * Class to obtain an FT_Face from a cairo_scaled_font_t and
  * automatically release it when it goes out of scope
  */
class gfxCairoLockedFace {
public:
    gfxCairoLockedFace(cairo_scaled_font_t *aScaledFont) :
        mScaledFont(aScaledFont),
        mFace(cairo_ft_scaled_font_lock_face(mScaledFont))
    { }

    virtual ~gfxCairoLockedFace()
    {
        if (mFace) {
            cairo_ft_scaled_font_unlock_face(mScaledFont);
        }
    }

    FT_Face get() { return mFace; };

protected:

    cairo_scaled_font_t* mScaledFont;
    FT_Face mFace;
};

#endif /* GFX_FT2UTILS_H */
