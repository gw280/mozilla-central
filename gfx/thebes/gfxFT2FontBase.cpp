/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxFT2Extents.h"
#include "gfxFT2FontBase.h"
#include "gfxFT2FontList.h"
#include "gfxFT2Utils.h"
#include "harfbuzz/hb.h"

#include "skia/SkTypeface.h"
#include "skia/SkTypeface_FreeType.h"

using namespace mozilla::gfx;

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include FT_TRUETYPE_TAGS_H
#include FT_TRUETYPE_TABLES_H

gfxFT2FontBase::gfxFT2FontBase(FT_Face aFontFace,
                               gfxFontEntry *aFontEntry,
                               const gfxFontStyle *aFontStyle)
    : gfxFont(aFontEntry, aFontStyle, kAntialiasDefault),
      mSpaceGlyph(0),
      mHasMetrics(false),
      mFace(aFontFace),
      mFontExtents(nsnull),
      mTypeface(nsnull)
{
    ConstructFontOptions();
    CreateExtentsCalculator();
}

gfxFT2FontBase::~gfxFT2FontBase()
{
      if (mScaledFont) {
          cairo_scaled_font_destroy(mScaledFont);
      }
}

const gfxFont::Metrics&
gfxFT2FontBase::GetMetrics()
{
    if (mHasMetrics)
        return mMetrics;

    if (NS_UNLIKELY(GetStyle()->size <= 0.0)) {
        new(&mMetrics) gfxFont::Metrics(); // zero initialize
        mSpaceGlyph = 0;
    } else {
        mFontExtents->GenerateMetrics(this, &mMetrics, &mSpaceGlyph);
    }

    SanitizeMetrics(&mMetrics, false);

#if 0
    //    printf("font name: %s %f\n", NS_ConvertUTF16toUTF8(GetName()).get(), GetStyle()->size);
    //    printf ("pango font %s\n", pango_font_description_to_string (pango_font_describe (font)));

    fprintf (stderr, "Font: %s\n", NS_ConvertUTF16toUTF8(GetName()).get());
    fprintf (stderr, "    emHeight: %f emAscent: %f emDescent: %f\n", mMetrics.emHeight, mMetrics.emAscent, mMetrics.emDescent);
    fprintf (stderr, "    maxAscent: %f maxDescent: %f\n", mMetrics.maxAscent, mMetrics.maxDescent);
    fprintf (stderr, "    internalLeading: %f externalLeading: %f\n", mMetrics.externalLeading, mMetrics.internalLeading);
    fprintf (stderr, "    spaceWidth: %f aveCharWidth: %f xHeight: %f\n", mMetrics.spaceWidth, mMetrics.aveCharWidth, mMetrics.xHeight);
    fprintf (stderr, "    uOff: %f uSize: %f stOff: %f stSize: %f suOff: %f suSize: %f\n", mMetrics.underlineOffset, mMetrics.underlineSize, mMetrics.strikeoutOffset, mMetrics.strikeoutSize, mMetrics.superscriptOffset, mMetrics.subscriptOffset);
#endif

    mHasMetrics = true;
    return mMetrics;
}

// Get the glyphID of a space
PRUint32
gfxFT2FontBase::GetSpaceGlyph()
{
    NS_ASSERTION(GetStyle()->size != 0,
                 "forgot to short-circuit a text run with zero-sized font?");
    GetMetrics();
    return mSpaceGlyph;
}

hb_blob_t *
gfxFT2FontBase::GetFontTable(PRUint32 aTag)
{
    hb_blob_t *blob;
    if (mFontEntry->GetExistingFontTable(aTag, &blob))
        return blob;

    FallibleTArray<PRUint8> buffer;
    bool haveTable = mFontExtents->GetFontTable(aTag, buffer);

    // Cache even when there is no table to save having to open the FT_Face
    // again.
    return mFontEntry->ShareFontTableAndGetBlob(aTag,
                                                haveTable ? &buffer : nsnull);
}


cairo_scaled_font_t*
gfxFT2FontBase::CairoScaledFont()
{
    if (mScaledFont) {
      return mScaledFont;
    }

    int flags = gfxPlatform::GetPlatform()->FontHintingEnabled() ?
                FT_LOAD_DEFAULT :
                (FT_LOAD_NO_AUTOHINT | FT_LOAD_NO_HINTING);

    cairo_font_face_t *temporaryFont = cairo_ft_font_face_create_for_ft_face(mFace, flags);

    cairo_scaled_font_t *scaledFont = NULL;
    cairo_matrix_t sizeMatrix;
    cairo_matrix_t identityMatrix;

    // XXX deal with adjusted size
    cairo_matrix_init_scale(&sizeMatrix, GetStyle()->size, GetStyle()->size);
    cairo_matrix_init_identity(&identityMatrix);

    // Given face is not italic but style requests it; set a skew matrix to emulate
    bool needsOblique = !(mFace->style_flags & FT_STYLE_FLAG_ITALIC) &&
                        (GetStyle()->style & (NS_FONT_STYLE_ITALIC | NS_FONT_STYLE_OBLIQUE));

    if (needsOblique) {
        const double kSkewFactor = 0.25;

        cairo_matrix_t style;
        cairo_matrix_init(&style,
                          1,                //xx
                          0,                //yx
                          -1 * kSkewFactor, //xy
                          1,                //yy
                          0,                //x0
                          0);               //y0
        cairo_matrix_multiply(&sizeMatrix, &sizeMatrix, &style);
    }

    cairo_font_options_t *fontOptions = cairo_font_options_create();

    if (!gfxPlatform::GetPlatform()->FontHintingEnabled()) {
        cairo_font_options_set_hint_metrics(fontOptions, CAIRO_HINT_METRICS_OFF);
    }

    scaledFont = cairo_scaled_font_create(temporaryFont,
                                          &sizeMatrix,
                                          &identityMatrix, fontOptions);
    cairo_font_options_destroy(fontOptions);


    cairo_font_face_destroy(temporaryFont);

    NS_ASSERTION(cairo_scaled_font_status(scaledFont) == CAIRO_STATUS_SUCCESS,
                 "Failed to make scaled font");

    mScaledFont = scaledFont;

    return mScaledFont;
}

SkTypeface*
gfxFT2FontBase::SkiaTypeface()
{
    if (!mTypeface) {
        mTypeface = SkCreateTypefaceFromFTFace(mFace);
    }

    return mTypeface;
}

void
gfxFT2FontBase::CreateExtentsCalculator()
{
    if (gfxPlatform::UseAzureContentDrawing()) {
        BackendType backend;
        if (gfxPlatform::GetPlatform()->SupportsAzure(backend)) {
            switch(backend) {
            case BACKEND_SKIA:
                mFontExtents = new gfxFT2SkiaExtents(SkiaTypeface(), mFace);
                return;
            default:
                break;
            }
        }
    }

    // If not using Azure, or we failed to create an Azure specific extents calculator,
    // create a cairo one instead and keep hold of a cairo_scaled_font_t
    mFontExtents = new gfxFT2CairoExtents(CairoScaledFont());
}

/**
 * Create a cairo_scaled_font_t in order to have a font to set on the cairo_t
 * for painting
 */
bool
gfxFT2FontBase::SetupCairoFont(gfxContext *aContext)
{
    cairo_t *cr = aContext->GetCairo();

    if (!CairoScaledFont()) {
      // Don't cairo_set_scaled_font as that would propagate the error to
      // the cairo_t, precluding any further drawing.
      return false;
    }

    cairo_set_scaled_font(cr, mScaledFont);
    return true;
}

void
gfxFT2FontBase::ConstructFontOptions()
{
  NS_LossyConvertUTF16toASCII name(this->GetName());
  mFontOptions.mName = name.get();

  const gfxFontStyle* style = this->GetStyle();
  if (style->style == NS_FONT_STYLE_ITALIC) {
    if (style->weight == NS_FONT_WEIGHT_BOLD) {
      mFontOptions.mStyle = FONT_STYLE_BOLD_ITALIC;
    } else {
      mFontOptions.mStyle = FONT_STYLE_ITALIC;
    }
  } else {
    if (style->weight == NS_FONT_WEIGHT_BOLD) {
      mFontOptions.mStyle = FONT_STYLE_BOLD;
    } else {
      mFontOptions.mStyle = FONT_STYLE_NORMAL;
    }
  }
}

PRUint32
gfxFT2FontBase::GetGlyph(PRUint32 unicode, PRUint32 variation_selector)
{
    return mFontExtents->GetGlyph(unicode, variation_selector);
}

PRInt32
gfxFT2FontBase::GetGlyphWidth(gfxContext *aCtx, PRUint16 aGlyphID)
{
    cairo_text_extents_t extents;
    mFontExtents->GetGlyphExtents(GetStyle(), aGlyphID, &extents);
    // convert to 16.16 fixed point
    return NS_lround(0x10000 * extents.x_advance);
}

