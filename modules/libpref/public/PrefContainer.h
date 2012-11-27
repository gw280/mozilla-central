/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_PrefContainer_h
#define mozilla_PrefContainer_h

#include "nsString.h"
#include "mozilla/Preferences.h"

namespace mozilla {

/**
 * PrefContainer is cached and self updating preference container. It removes
 * the need for special code to cache a preference. Rather the preference is
 * read and subscribed when the object is created and it is unsubscribed when
 * the object is destroyed.
 *
 * The usage is something like this:
 *
 *   PrefContainer<boolean> mSelfDestructEnabled;
 *
 * And in the constructor
 *
 *   : mSelfDestructEnabled("self.destruct.enabled", true);
 *
 * It defines a default conversion so you can write:
 *
 *   if (mSelfDestructEnabled.Get())
 *     ...
 */

template <typename T>
class PrefContainerBase {
public:
  PrefContainerBase(const char *aPref, T aDefault)
   : mDefault(aDefault), mPref(aPref)
  {
    Update();
    Preferences::RegisterCallback(DoUpdate, mPref, this);
  }

  ~PrefContainerBase()
  {
    Preferences::UnregisterCallback(DoUpdate, mPref, this);
  }

  const T& Get() const { return mValue; }

private:
  static int DoUpdate(const char* aPref, void *aPrefContainer) {
    PrefContainerBase<T>* prefContainer =
      static_cast<PrefContainerBase<T>*>(aPrefContainer);
    prefContainer->Update();
    return 0;
  }

  void Update();

  T mValue;
  T mDefault;
  const char *mPref;
};

template <typename T>
class PrefContainer : public PrefContainerBase<T> {
public:
    PrefContainer(const char *aPref, T aDefault = T())
      : PrefContainerBase<T>(aPref, aDefault)
    { }
};

template <>
class PrefContainer<bool> : public PrefContainerBase<bool> {
public:
    PrefContainer(const char *aPref, bool aDefault = false)
      : PrefContainerBase<bool>(aPref, aDefault)
    { }
};

} // namespace mozilla

#endif // mozilla_PrefContainer_h
