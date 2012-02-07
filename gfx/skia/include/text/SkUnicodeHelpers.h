/*
 * Copyright 2012 Mozilla Foundation
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkUnicodeHelpers_DEFINED
#define SkUnicodeHelpers_DEFINED

#ifndef SK_U16_IS_SURROGATE
#define SK_U16_IS_SURROGATE(c) (((c)&0xfffff800)==0xd800)
#endif

#ifndef SK_U16_IS_SURROGATE_LEAD
#define SK_U16_IS_SURROGATE_LEAD(c) (((c)&0x400)==0)
#endif

#ifndef SK_U16_IS_TRAIL
#define SK_U16_IS_TRAIL(c) (((c)&0xfffffc00)==0xdc00)
#endif

#ifndef SK_U16_GET_SUPPLEMENTARY
#define SK_U16_GET_SUPPLEMENTARY(lead, trail) (((uint32_t)(lead)<<10UL)+(uint32_t)(trail)-SK_U16_SURROGATE_OFFSET)
#endif

#ifndef SK_U16_SURROGATE_OFFSET
#define SK_U16_SURROGATE_OFFSET ((0xd800<<10UL)+0xdc00-0x10000)
#endif


#endif

