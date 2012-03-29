/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_BASICBUFFERS_H
#define GFX_BASICBUFFERS_H

#include "BasicLayersImpl.h"

#include "mozilla/gfx/2D.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace layers {

class BasicThebesLayer;
class BasicThebesLayerBuffer : public ThebesLayerBuffer {
  typedef ThebesLayerBuffer Base;

public:
  BasicThebesLayerBuffer(BasicThebesLayer* aLayer)
    : Base(ContainsVisibleBounds)
    , mLayer(aLayer)
  {
  }

  virtual ~BasicThebesLayerBuffer()
  {}

  using Base::BufferRect;
  using Base::BufferRotation;

  /**
   * Complete the drawing operation. The region to draw must have been
   * drawn before this is called. The contents of the buffer are drawn
   * to aTarget.
   */
  void DrawTo(ThebesLayer* aLayer, gfxContext* aTarget, float aOpacity,
              Layer* aMaskLayer);

  virtual already_AddRefed<gfxASurface>
  CreateBuffer(ContentType aType, const nsIntSize& aSize, PRUint32 aFlags);

  virtual TemporaryRef<DrawTarget>
  CreateDrawTarget(const gfx::IntSize& aSize, gfx::SurfaceFormat aFormat);

  /**
   * Swap out the old backing buffer for |aBuffer| and attributes.
   */
  void SetBackingBuffer(gfxASurface* aBuffer,
                        const nsIntRect& aRect, const nsIntPoint& aRotation)
  {
    gfxIntSize prevSize = gfxIntSize(BufferRect().width, BufferRect().height);
    gfxIntSize newSize = aBuffer->GetSize();
    NS_ABORT_IF_FALSE(newSize == prevSize,
                      "Swapped-in buffer size doesn't match old buffer's!");
    nsRefPtr<gfxASurface> oldBuffer;
    oldBuffer = SetBuffer(aBuffer, aRect, aRotation);
  }

  void SetBackingBuffer(DrawTarget* aBuffer,
                        const nsIntRect& aRect, const nsIntPoint& aRotation)
  {
    IntSize prevSize = IntSize(BufferRect().width, BufferRect().height);
    IntSize newSize = aBuffer->GetSize();
    NS_ABORT_IF_FALSE(newSize == prevSize,
                      "Swapped-in buffer size doesn't match old buffer's!");
    RefPtr<DrawTarget> oldBuffer;
    oldBuffer = SetDT(aBuffer, aRect, aRotation);
  }

  void SetBackingBufferAndUpdateFrom(
    gfxASurface* aBuffer,
    gfxASurface* aSource, const nsIntRect& aRect, const nsIntPoint& aRotation,
    const nsIntRegion& aUpdateRegion);

  void SetBackingBufferAndUpdateFrom(
    DrawTarget* aBuffer,
    DrawTarget* aSource, const nsIntRect& aRect, const nsIntPoint& aRotation,
    const nsIntRegion& aUpdateRegion);


private:
  BasicThebesLayerBuffer(gfxASurface* aBuffer,
                         const nsIntRect& aRect, const nsIntPoint& aRotation)
    // The size policy doesn't really matter here; this constructor is
    // intended to be used for creating temporaries
    : ThebesLayerBuffer(ContainsVisibleBounds)
  {
    SetBuffer(aBuffer, aRect, aRotation);
  }

  BasicThebesLayerBuffer(DrawTarget* aBuffer,
                         const nsIntRect& aRect, const nsIntPoint& aRotation)
    : ThebesLayerBuffer(ContainsVisibleBounds)
  {
    SetDT(aBuffer, aRect, aRotation);
  }


  BasicThebesLayer* mLayer;
};

class ShadowThebesLayerBuffer : public BasicThebesLayerBuffer
{
  typedef BasicThebesLayerBuffer Base;

public:
  ShadowThebesLayerBuffer()
    : Base(NULL)
  {
    MOZ_COUNT_CTOR(ShadowThebesLayerBuffer);
  }

  ~ShadowThebesLayerBuffer()
  {
    MOZ_COUNT_DTOR(ShadowThebesLayerBuffer);
  }

  void Swap(gfxASurface* aNewBuffer,
            const nsIntRect& aNewRect, const nsIntPoint& aNewRotation,
            gfxASurface** aOldBuffer,
            nsIntRect* aOldRect, nsIntPoint* aOldRotation)
  {
    *aOldRect = BufferRect();
    *aOldRotation = BufferRotation();

    nsRefPtr<gfxASurface> oldBuffer;
    oldBuffer = SetBuffer(aNewBuffer,
                          aNewRect, aNewRotation);
    oldBuffer.forget(aOldBuffer);
  }

protected:
  virtual already_AddRefed<gfxASurface>
  CreateBuffer(ContentType, const nsIntSize&, PRUint32)
  {
    NS_RUNTIMEABORT("ShadowThebesLayer can't paint content");
    return nsnull;
  }

  virtual TemporaryRef<DrawTarget>
  CreateDrawTarget(const IntSize&, SurfaceFormat)
  {
    NS_RUNTIMEABORT("ShadowThebesLayer can't paint content");
    return nsnull;
  }
};

}
}

#endif
