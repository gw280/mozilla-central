/*
 * Copyright 2012 Mozilla Foundation
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkTypeface_FreeType_DEFINED
#define SkTypeface_FreeType_DEFINED

/**
 *  Like the other Typeface create methods, this returns a new reference to the
 *  corresponding typeface for the specified FT_Face. The caller must call
 *  unref() when it is finished.
 */
SK_API extern SkTypeface* SkCreateTypefaceFromFTFace(FT_Face);

#endif

