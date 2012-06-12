/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxFT2Extents.h"
#include "gfxFT2Utils.h"

#include <ft2build.h>
#include FT_TRUETYPE_TAGS_H
#include FT_TRUETYPE_TABLES_H

#ifdef HAVE_FONTCONFIG_FCFREETYPE_H
#include <fontconfig/fcfreetype.h>
#endif

// aScale is intended for a 16.16 x/y_scale of an FT_Size_Metrics
static inline FT_Long
ScaleRoundDesignUnits(FT_Short aDesignMetric, FT_Fixed aScale)
{
    FT_Long fixed26dot6 = FT_MulFix(aDesignMetric, aScale);
    return ROUND_26_6_TO_INT(fixed26dot6);
}

// Snap a line to pixels while keeping the center and size of the line as
// close to the original position as possible.
//
// Pango does similar snapping for underline and strikethrough when fonts are
// hinted, but nsCSSRendering::GetTextDecorationRectInternal always snaps the
// top and size of lines.  Optimizing the distance between the line and
// baseline is probably good for the gap between text and underline, but
// optimizing the center of the line is better for positioning strikethough.
static void
SnapLineToPixels(gfxFloat& aOffset, gfxFloat& aSize)
{
    gfxFloat snappedSize = NS_MAX(floor(aSize + 0.5), 1.0);
    // Correct offset for change in size
    gfxFloat offset = aOffset - 0.5 * (aSize - snappedSize);
    // Snap offset
    aOffset = floor(offset + 0.5);
    aSize = snappedSize;
}

// TODO: We should really have an API in Azure to determine
// text extents using whatever backend we're using
void
gfxFT2CairoExtents::GetGlyphExtents(const gfxFontStyle* aStyle,
                                  PRUint32 aGlyphID,
                                  cairo_text_extents_t* aExtents)
{
    NS_PRECONDITION(aExtents != NULL, "aExtents must not be NULL");

    cairo_glyph_t glyphs[1];
    glyphs[0].index = aGlyphID;
    glyphs[0].x = 0.0;
    glyphs[0].y = 0.0;
    // cairo does some caching for us here but perhaps a small gain could be
    // made by caching more.  It is usually only the advance that is needed,
    // so caching only the advance could allow many requests to be cached with
    // little memory use.  Ideally this cache would be merged with
    // gfxGlyphExtents.
    cairo_scaled_font_glyph_extents(mScaledFont, glyphs, 1, (cairo_text_extents_t*)aExtents);
}

void
gfxFT2Extents::GenerateMetrics(FT_Face aFace,
                               gfxFont* aFont,
                               gfxFont::Metrics* aMetrics,
                               PRUint32* aSpaceGlyph)
{
    NS_PRECONDITION(aMetrics != NULL, "aMetrics must not be NULL");
    NS_PRECONDITION(aSpaceGlyph != NULL, "aSpaceGlyph must not be NULL");

    if (NS_UNLIKELY(!aFace)) {
        // No aFace.  This unfortunate situation might happen if the font
        // file is (re)moved at the wrong time.
        aMetrics->emHeight = aFont->GetStyle()->size;
        aMetrics->emAscent = 0.8 * aMetrics->emHeight;
        aMetrics->emDescent = 0.2 * aMetrics->emHeight;
        aMetrics->maxAscent = aMetrics->emAscent;
        aMetrics->maxDescent = aMetrics->maxDescent;
        aMetrics->maxHeight = aMetrics->emHeight;
        aMetrics->internalLeading = 0.0;
        aMetrics->externalLeading = 0.2 * aMetrics->emHeight;
        aSpaceGlyph = 0;
        aMetrics->spaceWidth = 0.5 * aMetrics->emHeight;
        aMetrics->maxAdvance = aMetrics->spaceWidth;
        aMetrics->aveCharWidth = aMetrics->spaceWidth;
        aMetrics->zeroOrAveCharWidth = aMetrics->spaceWidth;
        aMetrics->xHeight = 0.5 * aMetrics->emHeight;
        aMetrics->underlineSize = aMetrics->emHeight / 14.0;
        aMetrics->underlineOffset = -aMetrics->underlineSize;
        aMetrics->strikeoutOffset = 0.25 * aMetrics->emHeight;
        aMetrics->strikeoutSize = aMetrics->underlineSize;
        aMetrics->superscriptOffset = aMetrics->xHeight;
        aMetrics->subscriptOffset = aMetrics->xHeight;

        return;
    }

    const FT_Size_Metrics& ftMetrics = aFace->size->metrics;

    gfxFloat emHeight;
    // Scale for vertical design metric conversion: pixels per design unit.
    gfxFloat yScale;
    if (FT_IS_SCALABLE(aFace)) {
        // Prefer FT_Size_Metrics::x_scale to x_ppem as x_ppem does not
        // have subpixel accuracy.
        //
        // FT_Size_Metrics::y_scale is in 16.16 fixed point format.  Its
        // (fractional) value is a factor that converts vertical metrics from
        // design units to units of 1/64 pixels, so that the result may be
        // interpreted as pixels in 26.6 fixed point format.
        yScale = FLOAT_FROM_26_6(FLOAT_FROM_16_16(ftMetrics.y_scale));
        emHeight = aFace->units_per_EM * yScale;
    } else { // Not scalable.
        // FT_Size_Metrics doc says x_scale is "only relevant for scalable
        // font formats".
        gfxFloat emUnit = aFace->units_per_EM;
        emHeight = ftMetrics.y_ppem;
        yScale = emHeight / emUnit;
    }

    TT_OS2 *os2 =
        static_cast<TT_OS2*>(FT_Get_Sfnt_Table(aFace, ft_sfnt_os2));

    aMetrics->maxAscent = FLOAT_FROM_26_6(ftMetrics.ascender);
    aMetrics->maxDescent = -FLOAT_FROM_26_6(ftMetrics.descender);
    aMetrics->maxAdvance = FLOAT_FROM_26_6(ftMetrics.max_advance);

    gfxFloat lineHeight;
    if (os2 && os2->sTypoAscender) {
        aMetrics->emAscent = os2->sTypoAscender * yScale;
        aMetrics->emDescent = -os2->sTypoDescender * yScale;
        FT_Short typoHeight =
            os2->sTypoAscender - os2->sTypoDescender + os2->sTypoLineGap;
        lineHeight = typoHeight * yScale;

        // maxAscent/maxDescent get used for frame heights, and some fonts
        // don't have the HHEA table ascent/descent set (bug 279032).
        if (aMetrics->emAscent > aMetrics->maxAscent)
            aMetrics->maxAscent = aMetrics->emAscent;
        if (aMetrics->emDescent > aMetrics->maxDescent)
            aMetrics->maxDescent = aMetrics->emDescent;
    } else {
        aMetrics->emAscent = aMetrics->maxAscent;
        aMetrics->emDescent = aMetrics->maxDescent;
        lineHeight = FLOAT_FROM_26_6(ftMetrics.height);
    }

    cairo_text_extents_t extents;

    *aSpaceGlyph = aFont->GetGlyph(' ', 0);
    GetGlyphExtents(aFont->GetStyle(), *aSpaceGlyph, &extents);

    if (*aSpaceGlyph) {
        aMetrics->spaceWidth = extents.x_advance;
    } else {
        aMetrics->spaceWidth = aMetrics->maxAdvance; // guess
    }

    aMetrics->zeroOrAveCharWidth = 0.0;

    PRUint32 glyph = aFont->GetGlyph('0', 0);
    if (glyph) {
        GetGlyphExtents(aFont->GetStyle(), glyph, &extents);
        aMetrics->zeroOrAveCharWidth = extents.x_advance;
    }

    // Prefering a measured x over sxHeight because sxHeight doesn't consider
    // hinting, but maybe the x extents are not quite right in some fancy
    // script fonts.  CSS 2.1 suggests possibly using the height of an "o",
    // which would have a more consistent glyph across fonts.
    glyph = aFont->GetGlyph('x', 0);
    if (glyph && extents.y_bearing < 0.0) {
        GetGlyphExtents(aFont->GetStyle(), glyph, &extents);
        aMetrics->xHeight = -extents.y_bearing;
        aMetrics->aveCharWidth = extents.x_advance;
    } else {
        if (os2 && os2->sxHeight) {
            aMetrics->xHeight = os2->sxHeight * yScale;
        } else {
            // CSS 2.1, section 4.3.2 Lengths: "In the cases where it is
            // impossible or impractical to determine the x-height, a value of
            // 0.5em should be used."
            aMetrics->xHeight = 0.5 * emHeight;
        }
        aMetrics->aveCharWidth = 0.0; // updated below
    }
    // aveCharWidth is used for the width of text input elements so be
    // liberal rather than conservative in the estimate.
    if (os2 && os2->xAvgCharWidth) {
        // Round to pixels as this is compared with maxAdvance to guess
        // whether this is a fixed width font.
        gfxFloat avgCharWidth =
            ScaleRoundDesignUnits(os2->xAvgCharWidth, ftMetrics.x_scale);
        aMetrics->aveCharWidth =
            NS_MAX(aMetrics->aveCharWidth, avgCharWidth);
    }
    aMetrics->aveCharWidth =
        NS_MAX(aMetrics->aveCharWidth, aMetrics->zeroOrAveCharWidth);
    if (aMetrics->aveCharWidth == 0.0) {
        aMetrics->aveCharWidth = aMetrics->spaceWidth;
    }
    if (aMetrics->zeroOrAveCharWidth == 0.0) {
        aMetrics->zeroOrAveCharWidth = aMetrics->aveCharWidth;
    }
    // Apparently hinting can mean that max_advance is not always accurate.
    aMetrics->maxAdvance =
        NS_MAX(aMetrics->maxAdvance, aMetrics->aveCharWidth);

    // gfxFont::Metrics::underlineOffset is the position of the top of the
    // underline.
    //
    // FT_FaceRec documentation describes underline_position as "the
    // center of the underlining stem".  This was the original definition
    // of the PostScript metric, but in the PostScript table of OpenType
    // fonts the metric is "the top of the underline"
    // (http://www.microsoft.com/typography/otspec/post.htm), and FreeType
    // (up to version 2.3.7) doesn't make any adjustment.
    //
    // Therefore get the underline position directly from the table
    // ourselves when this table exists.  Use FreeType's metrics for
    // other (including older PostScript) fonts.
    if (aFace->underline_position && aFace->underline_thickness) {
        aMetrics->underlineSize = aFace->underline_thickness * yScale;
        TT_Postscript *post = static_cast<TT_Postscript*>
            (FT_Get_Sfnt_Table(aFace, ft_sfnt_post));
        if (post && post->underlinePosition) {
            aMetrics->underlineOffset = post->underlinePosition * yScale;
        } else {
            aMetrics->underlineOffset = aFace->underline_position * yScale
                + 0.5 * aMetrics->underlineSize;
        }
    } else { // No underline info.
        // Imitate Pango.
        aMetrics->underlineSize = emHeight / 14.0;
        aMetrics->underlineOffset = -aMetrics->underlineSize;
    }

    if (os2 && os2->yStrikeoutSize && os2->yStrikeoutPosition) {
        aMetrics->strikeoutSize = os2->yStrikeoutSize * yScale;
        aMetrics->strikeoutOffset = os2->yStrikeoutPosition * yScale;
    } else { // No strikeout info.
        aMetrics->strikeoutSize = aMetrics->underlineSize;
        // Use OpenType spec's suggested position for Roman font.
        aMetrics->strikeoutOffset = emHeight * 409.0 / 2048.0
            + 0.5 * aMetrics->strikeoutSize;
    }
    SnapLineToPixels(aMetrics->strikeoutOffset, aMetrics->strikeoutSize);

    if (os2 && os2->ySuperscriptYOffset) {
        gfxFloat val = ScaleRoundDesignUnits(os2->ySuperscriptYOffset,
                                             ftMetrics.y_scale);
        aMetrics->superscriptOffset = NS_MAX(1.0, val);
    } else {
        aMetrics->superscriptOffset = aMetrics->xHeight;
    }
    
    if (os2 && os2->ySubscriptYOffset) {
        gfxFloat val = ScaleRoundDesignUnits(os2->ySubscriptYOffset,
                                             ftMetrics.y_scale);
        // some fonts have the incorrect sign. 
        val = fabs(val);
        aMetrics->subscriptOffset = NS_MAX(1.0, val);
    } else {
        aMetrics->subscriptOffset = aMetrics->xHeight;
    }

    aMetrics->maxHeight = aMetrics->maxAscent + aMetrics->maxDescent;

    // Make the line height an integer number of pixels so that lines will be
    // equally spaced (rather than just being snapped to pixels, some up and
    // some down).  Layout calculates line height from the emHeight +
    // internalLeading + externalLeading, but first each of these is rounded
    // to layout units.  To ensure that the result is an integer number of
    // pixels, round each of the components to pixels.
    aMetrics->emHeight = floor(emHeight + 0.5);

    // maxHeight will normally be an integer, but round anyway in case
    // FreeType is configured differently.
    aMetrics->internalLeading =
        floor(aMetrics->maxHeight - aMetrics->emHeight + 0.5);

    // Text input boxes currently don't work well with lineHeight
    // significantly less than maxHeight (with Verdana, for example).
    lineHeight = floor(NS_MAX(lineHeight, aMetrics->maxHeight) + 0.5);
    aMetrics->externalLeading =
        lineHeight - aMetrics->internalLeading - aMetrics->emHeight;

    // Ensure emAscent + emDescent == emHeight
    gfxFloat sum = aMetrics->emAscent + aMetrics->emDescent;
    aMetrics->emAscent = sum > 0.0 ?
        aMetrics->emAscent * aMetrics->emHeight / sum : 0.0;
    aMetrics->emDescent = aMetrics->emHeight - aMetrics->emAscent;
}

void
gfxFT2CairoExtents::GenerateMetrics(gfxFont* aFont,
                                    gfxFont::Metrics* aMetrics,
                                    PRUint32* aSpaceGlyph)
{
    gfxCairoLockedFace faceLock(mScaledFont);
    return gfxFT2Extents::GenerateMetrics(faceLock.get(), aFont, aMetrics, aSpaceGlyph);
}

bool
gfxFT2CairoExtents::GetFontTable(PRUint32 aTag, FallibleTArray<PRUint8>& aBuffer)
{
    gfxCairoLockedFace faceLock(mScaledFont);
    return gfxFT2Utils::GetFontTable(faceLock.get(), aTag, aBuffer);
}

PRUint32
gfxFT2CairoExtents::GetGlyph(PRUint32 aCharCode, PRUint32 aVariantSelector)
{
    gfxCairoLockedFace faceLock(mScaledFont);
    return gfxFT2Utils::GetGlyph(faceLock.get(), aCharCode, aVariantSelector);
}

gfxFloat
gfxFT2CairoExtents::XScale()
{
    gfxCairoLockedFace faceLock(mScaledFont);
    return gfxFT2Utils::XScale(faceLock.get());
}

void gfxFT2SkiaExtents::GenerateMetrics(gfxFont* aFont,
                             gfxFont::Metrics* aMetrics,
                             PRUint32* aSpaceGlyph)
{
    return gfxFT2Extents::GenerateMetrics(mFace, aFont, aMetrics, aSpaceGlyph);
}

bool gfxFT2SkiaExtents::GetFontTable(PRUint32 aTag, FallibleTArray<PRUint8>& aBuffer)
{
    return gfxFT2Utils::GetFontTable(mFace, aTag, aBuffer);
}

PRUint32 gfxFT2SkiaExtents::GetGlyph(PRUint32 aCharCode, PRUint32 aVariantSelector)
{
    return gfxFT2Utils::GetGlyph(mFace, aCharCode, aVariantSelector);
}

gfxFloat gfxFT2SkiaExtents::XScale()
{
    return gfxFT2Utils::XScale(mFace);
}
