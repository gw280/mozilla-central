/*
 * Copyright 2012 Mozilla Foundation
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkFontHost.h"
#include "SkTypefaceCache.h"

void SkFontHost::Serialize(const SkTypeface*, SkWStream*) {
    SkASSERT(!"SkFontHost::Serialize unimplemented");
}

SkTypeface* SkFontHost::Deserialize(SkStream* stream) {
    SkASSERT(!"SkFontHost::Deserialize unimplemented");
    return NULL;
}

SkStream* SkFontHost::OpenStream(uint32_t id)
{
    SkASSERT(!"SkFontHost::OpenStream unimplemented");
    return NULL;
}

uint32_t SkFontHost::NextLogicalFont(SkFontID curr, SkFontID orig) {
    // We don't handle font fallback, WebKit does.
    return 0;
}

size_t SkFontHost::GetFileName(SkFontID fontID, char path[], size_t length,
                               int32_t* index) {
    SkASSERT(!"SkFontHost::GetFileName unimplemented");
    return 0;
}

SkTypeface* SkFontHost::CreateTypeface(const SkTypeface* familyFace,
                                     const char famillyName[],
                                     SkTypeface::Style style) {
    SkDEBUGFAIL("SkFontHost::FindTypeface unimplemented");
    return NULL;
}

SkTypeface* SkFontHost::CreateTypefaceFromStream(SkStream*) {
    SkDEBUGFAIL("SkFontHost::CreateTypeface unimplemented");
    return NULL;
}

SkTypeface* SkFontHost::CreateTypefaceFromFile(char const*) {
    SkDEBUGFAIL("SkFontHost::CreateTypefaceFromFile unimplemented");
    return NULL;
}

