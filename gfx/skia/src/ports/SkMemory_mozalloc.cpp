
/*
 * Copyright 2011 Google Inc.
 * Copyright 2012 Mozilla Foundation
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "SkTypes.h"

#include "mozilla/mozalloc.h"

void sk_throw() {
    SkDEBUGFAIL("sk_throw");
    abort();
}

void sk_out_of_memory(void) {
    SkDEBUGFAIL("sk_out_of_memory");
    abort();
}

void* sk_malloc_throw(size_t size) {
    return sk_malloc_flags(size, SK_MALLOC_THROW);
}

void* sk_realloc_throw(void* addr, size_t size) {
    void* p = moz_xrealloc(addr, size);
    if (size == 0) {
        return p;
    }
    if (p == NULL) {
        sk_throw();
    }
    return p;
}

void sk_free(void* p) {
    if (p) {
        moz_free(p);
    }
}

void* sk_malloc_flags(size_t size, unsigned flags) {
    void* p = moz_xmalloc(size);
    if (p == NULL) {
        if (flags & SK_MALLOC_THROW) {
            sk_throw();
        }
    }
    return p;
}

