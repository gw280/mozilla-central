/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Foundation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#if defined(MOZ_WIDGET_GTK2)
#include "gfxPlatformGtk.h"
#define gfxToolkitPlatform gfxPlatformGtk
#elif defined(ANDROID)
#include "gfxAndroidPlatform.h"
#define gfxToolkitPlatform gfxAndroidPlatform
#endif

#include "gfxTypes.h"
#include "gfxFT2Fonts.h"
#include "gfxFT2FontBase.h"
#include "gfxFT2Utils.h"
#include "gfxFT2FontList.h"
#include <locale.h>
#include "gfxHarfBuzzShaper.h"
#ifdef MOZ_GRAPHITE
#include "gfxGraphiteShaper.h"
#endif
#include "nsGkAtoms.h"
#include "nsTArray.h"
#include "nsUnicodeRange.h"
#include "nsCRT.h"
#include "nsXULAppAPI.h"

#include "prlog.h"
#include "prinit.h"

#include "mozilla/Preferences.h"

static PRLogModuleInfo *gFontLog = PR_NewLogModule("ft2fonts");

// rounding and truncation functions for a Freetype floating point number
// (FT26Dot6) stored in a 32bit integer with high 26 bits for the integer
// part and low 6 bits for the fractional part.
#define MOZ_FT_ROUND(x) (((x) + 32) & ~63) // 63 = 2^6 - 1
#define MOZ_FT_TRUNC(x) ((x) >> 6)
#define CONVERT_DESIGN_UNITS_TO_PIXELS(v, s) \
        MOZ_FT_TRUNC(MOZ_FT_ROUND(FT_MulFix((v) , (s))))

/**
 * gfxFT2Font
 */

bool
gfxFT2Font::ShapeWord(gfxContext *aContext,
                      gfxShapedWord *aShapedWord,
                      const PRUnichar *aString,
                      bool aPreferPlatformShaping)
{
    bool ok = false;

#ifdef MOZ_GRAPHITE
    if (FontCanSupportGraphite()) {
        if (gfxPlatform::GetPlatform()->UseGraphiteShaping()) {
            if (!mGraphiteShaper) {
                mGraphiteShaper = new gfxGraphiteShaper(this);
            }
            ok = mGraphiteShaper->ShapeWord(aContext, aShapedWord, aString);
        }
    }
#endif

    if (!ok && gfxPlatform::GetPlatform()->UseHarfBuzzForScript(aShapedWord->Script())) {
        if (!mHarfBuzzShaper) {
            gfxFT2LockedFace face(this);
            mFUnitsConvFactor = face.XScale();

            mHarfBuzzShaper = new gfxHarfBuzzShaper(this);
        }
        ok = mHarfBuzzShaper->ShapeWord(aContext, aShapedWord, aString);
    }

    if (!ok) {
        AddRange(aShapedWord, aString);
    }

    if (IsSyntheticBold()) {
        float synBoldOffset =
            GetSyntheticBoldOffset() * CalcXScale(aContext);
        aShapedWord->AdjustAdvancesForSyntheticBold(synBoldOffset);
    }

    return true;
}

void
gfxFT2Font::AddRange(gfxShapedWord *aShapedWord, const PRUnichar *str)
{
    const PRUint32 appUnitsPerDevUnit = aShapedWord->AppUnitsPerDevUnit();
    // we'll pass this in/figure it out dynamically, but at this point there can be only one face.
    gfxFT2LockedFace faceLock(this);
    FT_Face face = faceLock.get();

    gfxShapedWord::CompressedGlyph g;

    const gfxFT2Font::CachedGlyphData *cgd = nsnull, *cgdNext = nsnull;

    FT_UInt spaceGlyph = GetSpaceGlyph();

    PRUint32 len = aShapedWord->Length();
    for (PRUint32 i = 0; i < len; i++) {
        PRUnichar ch = str[i];

        if (ch == 0) {
            // treat this null byte as a missing glyph, don't create a glyph for it
            aShapedWord->SetMissingGlyph(i, 0, this);
            continue;
        }

        NS_ASSERTION(!gfxFontGroup::IsInvalidChar(ch), "Invalid char detected");

        if (cgdNext) {
            cgd = cgdNext;
            cgdNext = nsnull;
        } else {
            cgd = GetGlyphDataForChar(ch);
        }

        FT_UInt gid = cgd->glyphIndex;
        PRInt32 advance = 0;

        if (gid == 0) {
            advance = -1; // trigger the missing glyphs case below
        } else {
            // find next character and its glyph -- in case they exist
            // and exist in the current font face -- to compute kerning
            PRUnichar chNext = 0;
            FT_UInt gidNext = 0;
            FT_Pos lsbDeltaNext = 0;

            if (FT_HAS_KERNING(face) && i + 1 < len) {
                chNext = str[i + 1];
                if (chNext != 0) {
                    cgdNext = GetGlyphDataForChar(chNext);
                    gidNext = cgdNext->glyphIndex;
                    if (gidNext && gidNext != spaceGlyph)
                        lsbDeltaNext = cgdNext->lsbDelta;
                }
            }

            advance = cgd->xAdvance;

            // now add kerning to the current glyph's advance
            if (chNext && gidNext) {
                FT_Vector kerning; kerning.x = 0;
                FT_Get_Kerning(face, gid, gidNext, FT_KERNING_DEFAULT, &kerning);
                advance += kerning.x;
                if (cgd->rsbDelta - lsbDeltaNext >= 32) {
                    advance -= 64;
                } else if (cgd->rsbDelta - lsbDeltaNext < -32) {
                    advance += 64;
                }
            }

            // convert 26.6 fixed point to app units
            // round rather than truncate to nearest pixel
            // because these advances are often scaled
            advance = ((advance * appUnitsPerDevUnit + 32) >> 6);
        }

        if (advance >= 0 &&
            gfxShapedWord::CompressedGlyph::IsSimpleAdvance(advance) &&
            gfxShapedWord::CompressedGlyph::IsSimpleGlyphID(gid)) {
            aShapedWord->SetSimpleGlyph(i, g.SetSimpleGlyph(advance, gid));
        } else if (gid == 0) {
            // gid = 0 only happens when the glyph is missing from the font
            aShapedWord->SetMissingGlyph(i, ch, this);
        } else {
            gfxTextRun::DetailedGlyph details;
            details.mGlyphID = gid;
            NS_ASSERTION(details.mGlyphID == gid, "Seriously weird glyph ID detected!");
            details.mAdvance = advance;
            details.mXOffset = 0;
            details.mYOffset = 0;
            g.SetComplex(aShapedWord->IsClusterStart(i), true, 1);
            aShapedWord->SetGlyphs(i, g, &details);
        }
    }
}

gfxFT2Font::gfxFT2Font(FT_Face *aFontFace,
                       FT2FontEntry *aFontEntry,
                       const gfxFontStyle *aFontStyle,
                       bool aNeedsBold)
    : gfxFT2FontBase(aFontFace, aFontEntry, aFontStyle)
{
    NS_ASSERTION(mFontEntry, "Unable to find font entry for font.  Something is whack.");
    mApplySyntheticBold = aNeedsBold;
    mCharGlyphCache.Init(64);
}

gfxFT2Font::~gfxFT2Font()
{
}

cairo_font_face_t *
gfxFT2Font::CairoFontFace()
{
    return GetFontEntry()->CairoFontFace();
}

/**
 * Look up the font in the gfxFont cache. If we don't find it, create one.
 * In either case, add a ref, append it to the aFonts array, and return it ---
 * except for OOM in which case we do nothing and return null.
 */
already_AddRefed<gfxFT2Font>
gfxFT2Font::GetOrMakeFont(const nsAString& aName, const gfxFontStyle *aStyle,
                          bool aNeedsBold)
{
#ifdef ANDROID
    FT2FontEntry *fe = static_cast<FT2FontEntry*>
        (gfxPlatformFontList::PlatformFontList()->
            FindFontForFamily(aName, aStyle, aNeedsBold));
#else
    FT2FontEntry *fe = static_cast<FT2FontEntry*>
        (gfxToolkitPlatform::GetPlatform()->FindFontEntry(aName, *aStyle));
#endif
    if (!fe) {
        NS_WARNING("Failed to find font entry for font!");
        return nsnull;
    }

    nsRefPtr<gfxFT2Font> font = GetOrMakeFont(fe, aStyle, aNeedsBold);
    return font.forget();
}

already_AddRefed<gfxFT2Font>
gfxFT2Font::GetOrMakeFont(FT2FontEntry *aFontEntry, const gfxFontStyle *aStyle,
                          bool aNeedsBold)
{
    nsRefPtr<gfxFont> font = gfxFontCache::GetCache()->Lookup(aFontEntry, aStyle);
    if (!font) {
        cairo_scaled_font_t *scaledFont = aFontEntry->CreateScaledFont(aStyle);
        font = new gfxFT2Font(scaledFont, aFontEntry, aStyle, aNeedsBold);
        cairo_scaled_font_destroy(scaledFont);
        if (!font)
            return nsnull;
        gfxFontCache::GetCache()->AddNew(font);
    }
    gfxFont *f = nsnull;
    font.swap(f);
    return static_cast<gfxFT2Font *>(f);
}

void
gfxFT2Font::FillGlyphDataForChar(PRUint32 ch, CachedGlyphData *gd)
{
    gfxFT2LockedFace faceLock(this);
    FT_Face face = faceLock.get();

    FT_UInt gid = FT_Get_Char_Index(face, ch);

    if (gid == 0) {
        // this font doesn't support this char!
        NS_ASSERTION(gid != 0, "We don't have a glyph, but font indicated that it supported this char in tables?");
        gd->glyphIndex = 0;
        return;
    }

    FT_Int32 flags = gfxPlatform::GetPlatform()->FontHintingEnabled() ?
                     FT_LOAD_DEFAULT :
                     (FT_LOAD_NO_AUTOHINT | FT_LOAD_NO_HINTING);
    FT_Error err = FT_Load_Glyph(face, gid, flags);

    if (err) {
        // hmm, this is weird, we failed to load a glyph that we had?
        NS_WARNING("Failed to load glyph that we got from Get_Char_index");

        gd->glyphIndex = 0;
        return;
    }

    gd->glyphIndex = gid;
    gd->lsbDelta = face->glyph->lsb_delta;
    gd->rsbDelta = face->glyph->rsb_delta;
    gd->xAdvance = face->glyph->advance.x;
}

void
gfxFT2Font::SizeOfExcludingThis(nsMallocSizeOfFun aMallocSizeOf,
                                FontCacheSizes*   aSizes) const
{
    gfxFont::SizeOfExcludingThis(aMallocSizeOf, aSizes);
    aSizes->mFontInstances +=
        mCharGlyphCache.SizeOfExcludingThis(nsnull, aMallocSizeOf);
}

void
gfxFT2Font::SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf,
                                FontCacheSizes*   aSizes) const
{
    aSizes->mFontInstances += aMallocSizeOf(this);
    SizeOfExcludingThis(aMallocSizeOf, aSizes);
}
