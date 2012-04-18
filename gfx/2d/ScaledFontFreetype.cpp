/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScaledFontFreetype.h"
#include "Logging.h"

#include "gfxFont.h"

#ifdef USE_SKIA
#include "skia/SkTypeface.h"
#include "skia/SkTypeface_FreeType.h"
#endif

#include <string>

using namespace std;

namespace mozilla {
namespace gfx {

#ifdef USE_SKIA
static SkTypeface::Style
fontStyleToSkia(FontStyle aStyle)
{
  switch (aStyle) {
  case FONT_STYLE_NORMAL:
    return SkTypeface::kNormal;
  case FONT_STYLE_ITALIC:
    return SkTypeface::kItalic;
  case FONT_STYLE_BOLD:
    return SkTypeface::kBold;
  case FONT_STYLE_BOLD_ITALIC:
    return SkTypeface::kBoldItalic;
   }

  gfxWarning() << "Unknown font style";
  return SkTypeface::kNormal;
}
#endif

ScaledFontFreetype::ScaledFontFreetype(FT_Face aFace, Float aSize)
  : ScaledFontBase(aSize)
{
  mFace = aFace;
}

#ifdef USE_SKIA
SkTypeface* ScaledFontFreetype::GetSkTypeface()
{
    if (!mTypeface) {
        mTypeface = SkCreateTypefaceFromFTFace(mFace);
    }

    return mTypeface;
}
#endif

}
}

