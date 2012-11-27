/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/PrefContainer.h"

namespace mozilla {

template<>
void
PrefContainerBase<bool>::Update()
{
  mValue = Preferences::GetBool(mPref, mDefault);
}

template<>
void
PrefContainerBase<int32_t>::Update()
{
  mValue = Preferences::GetInt(mPref, mDefault);
}

template<>
void
PrefContainerBase<uint32_t>::Update()
{
  mValue = Preferences::GetUint(mPref, mDefault);
}

template<>
void
PrefContainerBase<nsCString>::Update()
{
  if (NS_FAILED(Preferences::GetCString(mPref, &mValue))) {
    mValue = mDefault;
  }
}

} // namespace mozilla
