/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxCairoFTFont.h"
#include "gfxFT2Utils.h"

#include <ft2build.h>
#include FT_TRUETYPE_TAGS_H
#include FT_TRUETYPE_TABLES_H

#ifdef HAVE_FONTCONFIG_FCFREETYPE_H
#include <fontconfig/fcfreetype.h>
#endif

#include "prlink.h"
#include "sampler.h"

typedef FT_UInt (*CharVariantFunction)(FT_Face  face,
                                       FT_ULong charcode,
                                       FT_ULong variantSelector);

static CharVariantFunction
FindCharVariantFunction(FT_Face aFace)
{
    // This function is available from FreeType 2.3.6 (June 2008).
    PRLibrary *lib = nsnull;
    CharVariantFunction function =
        reinterpret_cast<CharVariantFunction>
        (PR_FindFunctionSymbolAndLibrary("FT_Face_GetCharVariantIndex", &lib));
    if (!lib) {
        return nsnull;
    }

    FT_Int major;
    FT_Int minor;
    FT_Int patch;
    FT_Library_Version(aFace->glyph->library, &major, &minor, &patch);

    // Versions 2.4.0 to 2.4.3 crash if configured with
    // FT_CONFIG_OPTION_OLD_INTERNALS.  Presence of the symbol FT_Alloc
    // indicates FT_CONFIG_OPTION_OLD_INTERNALS.
    if (major == 2 && minor == 4 && patch < 4 &&
        PR_FindFunctionSymbol(lib, "FT_Alloc")) {
        function = nsnull;
    }

    // Decrement the reference count incremented in
    // PR_FindFunctionSymbolAndLibrary.
    PR_UnloadLibrary(lib);

    return function;
}

namespace gfxFT2Utils {

bool
GetFontTable(FT_Face aFace, PRUint32 aTag, FallibleTArray<PRUint8>& aBuffer)
{
    if (!aFace || !FT_IS_SFNT(aFace))
        return false;

    FT_ULong length = 0;
    // TRUETYPE_TAG is defined equivalent to FT_MAKE_TAG
    FT_Error error = FT_Load_Sfnt_Table(aFace, aTag, 0, NULL, &length);
    if (error != 0)
        return false;

    if (NS_UNLIKELY(length > static_cast<FallibleTArray<PRUint8>::size_type>(-1))
        || NS_UNLIKELY(!aBuffer.SetLength(length)))
        return false;
        
    error = FT_Load_Sfnt_Table(aFace, aTag, 0, aBuffer.Elements(), &length);
    if (NS_UNLIKELY(error != 0)) {
        aBuffer.Clear();
        return false;
    }

    return true;
}

PRUint32
GetGlyph(FT_Face aFace, PRUint32 aCharCode, PRUint32 aVariantSelector)
{
    SAMPLE_LABEL("gfxFT2Utils", "GetGlyph");
    if (NS_UNLIKELY(!aFace))
        return 0;

    static CharVariantFunction sGetCharVariantPtr;

    if (aVariantSelector) {
        // This function is available from FreeType 2.3.6 (June 2008).
        sGetCharVariantPtr = FindCharVariantFunction(aFace);
        if (!sGetCharVariantPtr)
            return 0;
    }

#ifdef HAVE_FONTCONFIG_FCFREETYPE_H
    // FcFreeTypeCharIndex will search starting from the most recently
    // selected charmap.  This can cause non-determistic behavior when more
    // than one charmap supports a character but with different glyphs, as
    // with older versions of MS Gothic, for example.  Always prefer a Unicode
    // charmap, if there is one.  (FcFreeTypeCharIndex usually does the
    // appropriate Unicode conversion, but some fonts have non-Roman glyphs
    // for FT_ENCODING_APPLE_ROMAN characters.)
    if (!aFace->charmap || aFace->charmap->encoding != FT_ENCODING_UNICODE) {
        FT_Select_Charmap(aFace, FT_ENCODING_UNICODE);
    }
#endif

    if (aVariantSelector) {
        return (*sGetCharVariantPtr)(aFace, aCharCode, aVariantSelector);
    } else {
#ifdef HAVE_FONTCONFIG_FCFREETYPE_H
        return FcFreeTypeCharIndex(aFace, aCharCode);
#else
        return FT_Get_Char_Index(aFace, aCharCode);
#endif
    }
}

gfxFloat
XScale(FT_Face aFace)
{
    SAMPLE_LABEL("gfxFT2Utils", "XScale");
    if (NS_UNLIKELY(!aFace))
        return 0.0;

    const FT_Size_Metrics& ftMetrics = aFace->size->metrics;

    if (FT_IS_SCALABLE(aFace)) {
        // Prefer FT_Size_Metrics::x_scale to x_ppem as x_ppem does not
        // have subpixel accuracy.
        //
        // FT_Size_Metrics::x_scale is in 16.16 fixed point format.  Its
        // (fractional) value is a factor that converts vertical metrics
        // from design units to units of 1/64 pixels, so that the result
        // may be interpreted as pixels in 26.6 fixed point format.
        return FLOAT_FROM_26_6(FLOAT_FROM_16_16(ftMetrics.x_scale));
    }

    // Not scalable.
    // FT_Size_Metrics doc says x_scale is "only relevant for scalable
    // font formats".
    return gfxFloat(ftMetrics.x_ppem) / gfxFloat(aFace->units_per_EM);
}

} // namespace gfxFT2Utils

