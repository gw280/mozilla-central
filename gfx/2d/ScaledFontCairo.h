/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_SCALEDFONTCAIRO_H_
#define MOZILLA_GFX_SCALEDFONTCAIRO_H_

#include "ScaledFontBase.h"

#include "cairo.h"

namespace mozilla {
namespace gfx {

class ScaledFontCairo : public ScaledFontBase
{
public:

  ScaledFontCairo(cairo_scaled_font_t* aScaledFont, Float aSize);
};

// We need to be able to tell Skia whether or not to use
// hinting when rendering text, so that the glyphs it renders
// are the same as what layout is expecting. At the moment, only
// Skia uses this class when rendering with FreeType, as gfxFT2Font
// is the only gfxFont that honours gfxPlatform::FontHintingEnabled().
//
// We may want to have a range of different hinting modes, but for now
// a simple boolean will suffice as FontHintingEnabled() only returns
// a bool.
class GlyphRenderingOptionsCairo : public GlyphRenderingOptions
{
public:
  GlyphRenderingOptionsCairo()
    : mHinting(true)
  {
  }

  void SetHinting(bool aHinting) { mHinting = aHinting; }
  bool GetHinting() const { return mHinting; }
  virtual FontType GetType() const { return FONT_CAIRO; }
private:
  bool mHinting;
};

}
}

#endif /* MOZILLA_GFX_SCALEDFONTCAIRO_H_ */
