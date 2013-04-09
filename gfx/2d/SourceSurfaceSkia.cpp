/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "Logging.h"
#include "SourceSurfaceSkia.h"
#include "skia/SkBitmap.h"
#include "skia/SkDevice.h"
#include "HelpersSkia.h"
#include "DrawTargetSkia.h"

namespace mozilla {
namespace gfx {

SourceSurfaceSkia::SourceSurfaceSkia()
  : mDrawTarget(nullptr)
{
}

SourceSurfaceSkia::~SourceSurfaceSkia()
{
  MarkIndependent();
}

IntSize
SourceSurfaceSkia::GetSize() const
{
  return mSize;
}

SurfaceFormat
SourceSurfaceSkia::GetFormat() const
{
  return mFormat;
}

bool
SourceSurfaceSkia::InitFromCanvas(SkCanvas* aCanvas,
                                  SurfaceFormat aFormat,
                                  DrawTargetSkia* aOwner)
{
  SkISize size = aCanvas->getDeviceSize();

  mCanvas = aCanvas;

  if (aOwner) {
    mCanvas = aCanvas;
    mDrawTarget = aOwner;
  } else {
    // Skia only currently supports ARGB8888 for readPixels
    mBitmap.setConfig(SkBitmap::kARGB_8888_Config, size.fWidth, size.fHeight);

    if (!aCanvas->readPixels(&mBitmap, 0, 0)) {
      return false;
    }
  }

  mSize = IntSize(size.fWidth, size.fHeight);
  mFormat = FORMAT_B8G8R8A8;
  mStride = mBitmap.rowBytes();
  mDrawTarget = aOwner;

  return true;
}

bool 
SourceSurfaceSkia::InitFromData(unsigned char* aData,
                                const IntSize &aSize,
                                int32_t aStride,
                                SurfaceFormat aFormat)
{
  SkBitmap temp;
  temp.setConfig(GfxFormatToSkiaConfig(aFormat), aSize.width, aSize.height, aStride);
  temp.setPixels(aData);

  SkAutoTUnref<SkDevice> device(new SkDevice(temp.getConfig(), aSize.width, aSize.height, aFormat == FORMAT_B8G8R8X8 ? true : false));

  if (!temp.copyTo(&mBitmap, GfxFormatToSkiaConfig(aFormat))) {
    return false;
  }

  if (aFormat == FORMAT_B8G8R8X8) {
    const SkBitmap& bitmap = device->accessBitmap(true);
    bitmap.lockPixels();
    // We have to manually set the A channel to be 255 as Skia doesn't understand BGRX
    ConvertBGRXToBGRA(reinterpret_cast<unsigned char*>(mBitmap.getPixels()), aSize, aStride);
    bitmap.unlockPixels();
    bitmap.notifyPixelsChanged();
  }

  mSize = aSize;
  mFormat = aFormat;
  mStride = aStride;
  return true;
}

bool
SourceSurfaceSkia::InitWithBitmap(const SkBitmap& aBitmap)
{
  mFormat = SkiaConfigToGfxFormat(aBitmap.getConfig());
  mSize = IntSize(aBitmap.width(), aBitmap.height());

  if (!aBitmap.copyTo(&mBitmap, aBitmap.getConfig())) {
    return false;
  }

  mStride = mBitmap.rowBytes();
  return true;
}

unsigned char*
SourceSurfaceSkia::GetData()
{
  mBitmap.lockPixels();
  unsigned char *pixels = (unsigned char *)mBitmap.getPixels();
  mBitmap.unlockPixels();
  return pixels;

}

void
SourceSurfaceSkia::DrawTargetWillChange()
{
  if (mDrawTarget) {
    mDrawTarget = nullptr;
    SkBitmap temp = mBitmap;
    mBitmap.reset();
    temp.copyTo(&mBitmap, temp.getConfig());
  }
}

void
SourceSurfaceSkia::DrawTargetDestroyed()
{
  mDrawTarget = nullptr;
}

void
SourceSurfaceSkia::MarkIndependent()
{
  if (mDrawTarget) {
    mDrawTarget->RemoveSnapshot(this);
    mDrawTarget = nullptr;
  }
}

}
}
