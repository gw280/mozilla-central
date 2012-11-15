/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/PLayersChild.h"
#include "nscore.h"
#include "mozilla/gfx/2D.h"
#include "BasicTiledThebesLayer.h"
#include "gfxImageSurface.h"
#include "sampler.h"
#include "gfxPlatform.h"

#ifdef GFX_TILEDLAYER_DEBUG_OVERLAY
#include "cairo.h"
#include <sstream>
using mozilla::layers::Layer;
static void DrawDebugOverlay(gfxImageSurface* imgSurf, int x, int y)
{
  gfxContext c(imgSurf);

  // Draw border
  c.NewPath();
  c.SetDeviceColor(gfxRGBA(0.0, 0.0, 0.0, 1.0));
  c.Rectangle(gfxRect(gfxPoint(0,0),imgSurf->GetSize()));
  c.Stroke();

  // Build tile description
  std::stringstream ss;
  ss << x << ", " << y;

  // Draw text using cairo toy text API
  cairo_t* cr = c.GetCairo();
  cairo_set_font_size(cr, 25);
  cairo_text_extents_t extents;
  cairo_text_extents(cr, ss.str().c_str(), &extents);

  int textWidth = extents.width + 6;

  c.NewPath();
  c.SetDeviceColor(gfxRGBA(0.0, 0.0, 0.0, 1.0));
  c.Rectangle(gfxRect(gfxPoint(2,2),gfxSize(textWidth, 30)));
  c.Fill();

  c.NewPath();
  c.SetDeviceColor(gfxRGBA(1.0, 0.0, 0.0, 1.0));
  c.Rectangle(gfxRect(gfxPoint(2,2),gfxSize(textWidth, 30)));
  c.Stroke();

  c.NewPath();
  cairo_move_to(cr, 4, 28);
  cairo_show_text(cr, ss.str().c_str());

}

#endif

namespace mozilla {
namespace layers {

bool
BasicTiledLayerBuffer::HasFormatChanged(BasicTiledThebesLayer* aThebesLayer) const
{
  return aThebesLayer->CanUseOpaqueSurface() != mLastPaintOpaque;
}


gfxASurface::gfxImageFormat
BasicTiledLayerBuffer::GetFormat() const
{
  if (mThebesLayer->CanUseOpaqueSurface()) {
    return gfxASurface::ImageFormatRGB16_565;
  } else {
    return gfxASurface::ImageFormatARGB32;
  }
}

void
BasicTiledLayerBuffer::PaintThebes(BasicTiledThebesLayer* aLayer,
                                   const nsIntRegion& aNewValidRegion,
                                   const nsIntRegion& aPaintRegion,
                                   LayerManager::DrawThebesLayerCallback aCallback,
                                   void* aCallbackData)
{
  mThebesLayer = aLayer;
  mCallback = aCallback;
  mCallbackData = aCallbackData;

#ifdef GFX_TILEDLAYER_PREF_WARNINGS
  long start = PR_IntervalNow();
#endif

  // If this region is empty XMost() - 1 will give us a negative value.
  NS_ASSERTION(!aPaintRegion.GetBounds().IsEmpty(), "Empty paint region\n");

  bool useSinglePaintBuffer = UseSinglePaintBuffer();
  if (useSinglePaintBuffer) {
    // Check if the paint only spans a single tile. If that's
    // the case there's no point in using a single paint buffer.
    nsIntRect paintBounds = aPaintRegion.GetBounds();
    useSinglePaintBuffer = GetTileStart(paintBounds.x) !=
                           GetTileStart(paintBounds.XMost() - 1) ||
                           GetTileStart(paintBounds.y) !=
                           GetTileStart(paintBounds.YMost() - 1);
  }

  if (useSinglePaintBuffer) {
    nsRefPtr<gfxContext> ctxt;

    const nsIntRect bounds = aPaintRegion.GetBounds();
    {
      SAMPLE_LABEL("BasicTiledLayerBuffer", "PaintThebesSingleBufferAlloc");

      if (gfxPlatform::GetPlatform()->SupportsAzureContent()) {
        mSinglePaintDrawTarget =
          gfxPlatform::GetPlatform()->CreateOffscreenDrawTarget(gfx::IntSize(bounds.width, bounds.height),
                                                                gfx::SurfaceFormatForImageFormat(GetFormat()));

        ctxt = new gfxContext(mSinglePaintDrawTarget);
      } else {
        mSinglePaintBuffer = new gfxImageSurface(gfxIntSize(bounds.width, bounds.height), GetFormat(), !aLayer->CanUseOpaqueSurface());

        ctxt = new gfxContext(mSinglePaintBuffer);
      }

      mSinglePaintBufferOffset = nsIntPoint(bounds.x, bounds.y);
    }

    ctxt->NewPath();
    ctxt->Translate(gfxPoint(-bounds.x, -bounds.y));
#ifdef GFX_TILEDLAYER_PREF_WARNINGS
    if (PR_IntervalNow() - start > 3) {
      printf_stderr("Slow alloc %i\n", PR_IntervalNow() - start);
    }
    start = PR_IntervalNow();
#endif
    SAMPLE_LABEL("BasicTiledLayerBuffer", "PaintThebesSingleBufferDraw");

    mCallback(mThebesLayer, ctxt, aPaintRegion, nsIntRegion(), mCallbackData);
  }

#ifdef GFX_TILEDLAYER_PREF_WARNINGS
  if (PR_IntervalNow() - start > 30) {
    const nsIntRect bounds = aPaintRegion.GetBounds();
    printf_stderr("Time to draw %i: %i, %i, %i, %i\n", PR_IntervalNow() - start, bounds.x, bounds.y, bounds.width, bounds.height);
    if (aPaintRegion.IsComplex()) {
      printf_stderr("Complex region\n");
      nsIntRegionRectIterator it(aPaintRegion);
      for (const nsIntRect* rect = it.Next(); rect != nullptr; rect = it.Next()) {
        printf_stderr(" rect %i, %i, %i, %i\n", rect->x, rect->y, rect->width, rect->height);
      }
    }
  }
  start = PR_IntervalNow();
#endif

  SAMPLE_LABEL("BasicTiledLayerBuffer", "PaintThebesUpdate");
  Update(aNewValidRegion, aPaintRegion);

#ifdef GFX_TILEDLAYER_PREF_WARNINGS
  if (PR_IntervalNow() - start > 10) {
    const nsIntRect bounds = aPaintRegion.GetBounds();
    printf_stderr("Time to tile %i: %i, %i, %i, %i\n", PR_IntervalNow() - start, bounds.x, bounds.y, bounds.width, bounds.height);
  }
#endif

  mLastPaintOpaque = mThebesLayer->CanUseOpaqueSurface();
  mThebesLayer = nullptr;
  mCallback = nullptr;
  mCallbackData = nullptr;
  mSinglePaintBuffer = nullptr;
  mSinglePaintDrawTarget = nullptr;
}

BasicTiledLayerTile
BasicTiledLayerBuffer::ValidateTileInternal(BasicTiledLayerTile aTile,
                                            const nsIntPoint& aTileOrigin,
                                            const nsIntRect& aDirtyRect)
{
  if (aTile == GetPlaceholderTile() || aTile.mSurface->Format() != GetFormat()) {
    gfxImageSurface* tmpTile = new gfxImageSurface(gfxIntSize(GetTileLength(), GetTileLength()),
                                                   GetFormat(), !mThebesLayer->CanUseOpaqueSurface());
    aTile = BasicTiledLayerTile(tmpTile);
  }

  gfxRect drawRect(aDirtyRect.x - aTileOrigin.x, aDirtyRect.y - aTileOrigin.y,
                   aDirtyRect.width, aDirtyRect.height);

  // Use the gfxReusableSurfaceWrapper, which will reuse the surface
  // if the compositor no longer has a read lock, otherwise the surface
  // will be copied into a new writable surface.
  gfxImageSurface* writableSurface;
  aTile.mSurface = aTile.mSurface->GetWritable(&writableSurface);

  // Bug 742100, this gfxContext really should live on the stack.
  nsRefPtr<gfxContext> ctxt;

  RefPtr<gfx::DrawTarget> writableDrawTarget;
  if (gfxPlatform::GetPlatform()->SupportsAzureContent()) {
    // TODO: Instead of creating a gfxImageSurface to back the tile we should
    // create an offscreen DrawTarget. This would need to be shared cross-thread
    // and support copy on write semantics.
    gfx::SurfaceFormat format = gfx::SurfaceFormatForImageFormat(writableSurface->Format());

    writableDrawTarget =
        gfxPlatform::GetPlatform()->CreateDrawTargetForData(writableSurface->Data(),
                                                            gfx::IntSize(writableSurface->Width(),
                                                                         writableSurface->Height()),
                                                            writableSurface->Stride(),
                                                            format);
    ctxt = new gfxContext(writableDrawTarget);
  } else {
    ctxt = new gfxContext(writableSurface);
    ctxt->SetOperator(gfxContext::OPERATOR_SOURCE);
  }

  if (mSinglePaintBuffer || mSinglePaintDrawTarget) {
    if (gfxPlatform::GetPlatform()->SupportsAzureContent()) {
      RefPtr<gfx::SourceSurface> source = mSinglePaintDrawTarget->Snapshot();
      writableDrawTarget->CopySurface(source,
                                      gfx::IntRect(gfx::IntPoint(0, 0), source->GetSize()),
                                      gfx::IntPoint(mSinglePaintBufferOffset.x - aDirtyRect.x + drawRect.x,
                                                    mSinglePaintBufferOffset.y - aDirtyRect.y + drawRect.y));
    } else {
      ctxt->NewPath();
      ctxt->SetSource(mSinglePaintBuffer.get(),
                      gfxPoint(mSinglePaintBufferOffset.x - aDirtyRect.x + drawRect.x,
                               mSinglePaintBufferOffset.y - aDirtyRect.y + drawRect.y));
      ctxt->Rectangle(drawRect, true);
      ctxt->Fill();
    }
  } else {
    ctxt->NewPath();
    ctxt->Translate(gfxPoint(-aTileOrigin.x, -aTileOrigin.y));
    nsIntPoint a = aTileOrigin;
    mCallback(mThebesLayer, ctxt, nsIntRegion(nsIntRect(a, nsIntSize(GetTileLength(), GetTileLength()))), nsIntRegion(), mCallbackData);
  }

#ifdef GFX_TILEDLAYER_DEBUG_OVERLAY
  DrawDebugOverlay(writableSurface, aTileOrigin.x, aTileOrigin.y);
#endif

  return aTile;
}

BasicTiledLayerTile
BasicTiledLayerBuffer::ValidateTile(BasicTiledLayerTile aTile,
                                    const nsIntPoint& aTileOrigin,
                                    const nsIntRegion& aDirtyRegion)
{

  SAMPLE_LABEL("BasicTiledLayerBuffer", "ValidateTile");

#ifdef GFX_TILEDLAYER_PREF_WARNINGS
  if (aDirtyRegion.IsComplex()) {
    printf_stderr("Complex region\n");
  }
#endif

  nsIntRegionRectIterator it(aDirtyRegion);
  for (const nsIntRect* rect = it.Next(); rect != nullptr; rect = it.Next()) {
#ifdef GFX_TILEDLAYER_PREF_WARNINGS
    printf_stderr(" break into subrect %i, %i, %i, %i\n", rect->x, rect->y, rect->width, rect->height);
#endif
    aTile = ValidateTileInternal(aTile, aTileOrigin, *rect);
  }

  return aTile;
}

void
BasicTiledThebesLayer::FillSpecificAttributes(SpecificLayerAttributes& aAttrs)
{
  aAttrs = ThebesLayerAttributes(GetValidRegion());
}

static nsIntRect
RoundedTransformViewportBounds(const gfx::Rect& aViewport,
                               const gfx::Point& aScrollOffset,
                               const gfxSize& aResolution,
                               float aScaleX,
                               float aScaleY,
                               const gfx3DMatrix& aTransform)
{
  gfxRect transformedViewport(aViewport.x - (aScrollOffset.x * aResolution.width),
                              aViewport.y - (aScrollOffset.y * aResolution.height),
                              aViewport.width, aViewport.height);
  transformedViewport.Scale((aScaleX / aResolution.width) / aResolution.width,
                            (aScaleY / aResolution.height) / aResolution.height);
  transformedViewport = aTransform.TransformBounds(transformedViewport);

  return nsIntRect((int32_t)floor(transformedViewport.x),
                   (int32_t)floor(transformedViewport.y),
                   (int32_t)ceil(transformedViewport.width),
                   (int32_t)ceil(transformedViewport.height));
}

bool
BasicTiledThebesLayer::ComputeProgressiveUpdateRegion(const nsIntRegion& aInvalidRegion,
                                                      const nsIntRegion& aOldValidRegion,
                                                      nsIntRegion& aRegionToPaint,
                                                      const gfx3DMatrix& aTransform,
                                                      const gfx::Point& aScrollOffset,
                                                      const gfxSize& aResolution,
                                                      bool aIsRepeated)
{
  aRegionToPaint = aInvalidRegion;

  // Find out if we have any non-stale content to update.
  nsIntRegion freshRegion;
  if (!mFirstPaint) {
    freshRegion.And(aInvalidRegion, aOldValidRegion);
    freshRegion.Sub(aInvalidRegion, freshRegion);
  }

  // Find out the current view transform to determine which tiles to draw
  // first, and see if we should just abort this paint. Aborting is usually
  // caused by there being an incoming, more relevant paint.
  gfx::Rect viewport;
  float scaleX, scaleY;
  if (BasicManager()->ProgressiveUpdateCallback(!freshRegion.IsEmpty(), viewport, scaleX, scaleY)) {
    SAMPLE_MARKER("Abort painting");
    aRegionToPaint.SetEmpty();
    return aIsRepeated;
  }

  // Transform the screen coordinates into local layer coordinates.
  nsIntRect roundedTransformedViewport =
    RoundedTransformViewportBounds(viewport, aScrollOffset, aResolution,
                                   scaleX, scaleY, aTransform);

  // Paint tiles that have no content before tiles that only have stale content.
  bool drawingStale = freshRegion.IsEmpty();
  if (!drawingStale) {
    aRegionToPaint = freshRegion;
  }

  // Prioritise tiles that are currently visible on the screen.
  bool paintVisible = false;
  if (aRegionToPaint.Intersects(roundedTransformedViewport)) {
    aRegionToPaint.And(aRegionToPaint, roundedTransformedViewport);
    paintVisible = true;
  }

  // The following code decides what order to draw tiles in, based on the
  // current scroll direction of the primary scrollable layer.
  NS_ASSERTION(!aRegionToPaint.IsEmpty(), "Unexpectedly empty paint region!");
  nsIntRect paintBounds = aRegionToPaint.GetBounds();

  int startX, incX, startY, incY;
  if (aScrollOffset.x >= mLastScrollOffset.x) {
    startX = mTiledBuffer.RoundDownToTileEdge(paintBounds.x);
    incX = mTiledBuffer.GetTileLength();
  } else {
    startX = mTiledBuffer.RoundDownToTileEdge(paintBounds.XMost() - 1);
    incX = -mTiledBuffer.GetTileLength();
  }

  if (aScrollOffset.y >= mLastScrollOffset.y) {
    startY = mTiledBuffer.RoundDownToTileEdge(paintBounds.y);
    incY = mTiledBuffer.GetTileLength();
  } else {
    startY = mTiledBuffer.RoundDownToTileEdge(paintBounds.YMost() - 1);
    incY = -mTiledBuffer.GetTileLength();
  }

  // Find a tile to draw.
  nsIntRect tileBounds(startX, startY,
                       mTiledBuffer.GetTileLength(),
                       mTiledBuffer.GetTileLength());
  int32_t scrollDiffX = aScrollOffset.x - mLastScrollOffset.x;
  int32_t scrollDiffY = aScrollOffset.y - mLastScrollOffset.y;
  // This loop will always terminate, as there is at least one tile area
  // along the first/last row/column intersecting with regionToPaint, or its
  // bounds would have been smaller.
  while (true) {
    aRegionToPaint.And(aInvalidRegion, tileBounds);
    if (!aRegionToPaint.IsEmpty()) {
      break;
    }
    if (NS_ABS(scrollDiffY) >= NS_ABS(scrollDiffX)) {
      tileBounds.x += incX;
    } else {
      tileBounds.y += incY;
    }
  }

  bool repeatImmediately = false;
  if (!aRegionToPaint.Contains(aInvalidRegion)) {
    // The region needed to paint is larger then our progressive chunk size
    // therefore update what we want to paint and ask for a new paint transaction.

    // If we're drawing stale, visible content, make sure that it happens
    // in one go by repeating this work without calling the painted
    // callback. The remaining content is then drawn tile-by-tile in
    // multiple transactions.
    if (paintVisible && drawingStale) {
      repeatImmediately = true;
    } else {
      BasicManager()->SetRepeatTransaction();
    }
  } else {
    // The transaction is completed, store the last scroll offset.
    mLastScrollOffset = aScrollOffset;
  }

  return repeatImmediately;
}

void
BasicTiledThebesLayer::PaintThebes(gfxContext* aContext,
                                   Layer* aMaskLayer,
                                   LayerManager::DrawThebesLayerCallback aCallback,
                                   void* aCallbackData,
                                   ReadbackProcessor* aReadback)
{
  if (!aCallback) {
    BasicManager()->SetTransactionIncomplete();
    return;
  }

  if (!HasShadow()) {
    NS_ASSERTION(false, "Shadow requested for painting\n");
    return;
  }

  if (mTiledBuffer.HasFormatChanged(this)) {
    mValidRegion = nsIntRegion();
  }

  nsIntRegion invalidRegion = mVisibleRegion;
  invalidRegion.Sub(invalidRegion, mValidRegion);
  if (invalidRegion.IsEmpty())
    return;

  gfxSize resolution(1, 1);
  for (ContainerLayer* parent = GetParent(); parent; parent = parent->GetParent()) {
    const FrameMetrics& metrics = parent->GetFrameMetrics();
    resolution.width *= metrics.mResolution.width;
    resolution.height *= metrics.mResolution.height;
  }

  // Only draw progressively when the resolution is unchanged.
  if (gfxPlatform::UseProgressiveTilePainting() &&
      !BasicManager()->HasShadowTarget() &&
      mTiledBuffer.GetResolution() == resolution) {
    // Calculate the transform required to convert screen space into layer space
    gfx3DMatrix transform = GetEffectiveTransform();
    // XXX Not sure if this code for intermediate surfaces is correct.
    //     It rarely gets hit though, and shouldn't have terrible consequences
    //     even if it is wrong.
    for (ContainerLayer* parent = GetParent(); parent; parent = parent->GetParent()) {
      if (parent->UseIntermediateSurface()) {
        transform.PreMultiply(parent->GetEffectiveTransform());
      }
    }
    transform.Invert();

    // Store the old valid region, then clear it before painting.
    // We clip the old valid region to the visible region, as it only gets
    // used to decide stale content (currently valid and previously visible)
    nsIntRegion oldValidRegion = mTiledBuffer.GetValidRegion();
    oldValidRegion.And(oldValidRegion, mVisibleRegion);
    mTiledBuffer.ClearPaintedRegion();

    // Make sure that tiles that fall outside of the visible region are
    // discarded on the first update.
    if (!BasicManager()->IsRepeatTransaction()) {
      mValidRegion.And(mValidRegion, mVisibleRegion);
    }

    // Calculate the scroll offset since the last transaction.
    gfx::Point scrollOffset(0, 0);
    Layer* primaryScrollable = BasicManager()->GetPrimaryScrollableLayer();
    if (primaryScrollable) {
      const FrameMetrics& metrics = primaryScrollable->AsContainerLayer()->GetFrameMetrics();
      scrollOffset = metrics.mScrollOffset;
    }

    bool repeat = false;
    do {
      // Compute the region that should be updated. Repeat as many times as
      // is required.
      nsIntRegion regionToPaint;
      repeat = ComputeProgressiveUpdateRegion(invalidRegion,
                                              oldValidRegion,
                                              regionToPaint,
                                              transform,
                                              scrollOffset,
                                              resolution,
                                              repeat);

      // There's no further work to be done, return if nothing has been
      // drawn, or give what has been drawn to the shadow layer to upload.
      if (regionToPaint.IsEmpty()) {
        if (repeat) {
          break;
        } else {
          return;
        }
      }

      // Keep track of what we're about to refresh.
      mValidRegion.Or(mValidRegion, regionToPaint);

      // mValidRegion would have been altered by InvalidateRegion, but we still
      // want to display stale content until it gets progressively updated.
      // Create a region that includes stale content.
      nsIntRegion validOrStale;
      validOrStale.Or(mValidRegion, oldValidRegion);

      // Paint the computed region and subtract it from the invalid region.
      mTiledBuffer.PaintThebes(this, validOrStale, regionToPaint, aCallback, aCallbackData);
      invalidRegion.Sub(invalidRegion, regionToPaint);
    } while (repeat);
  } else {
    mTiledBuffer.ClearPaintedRegion();
    mTiledBuffer.SetResolution(resolution);
    mValidRegion = mVisibleRegion;
    mTiledBuffer.PaintThebes(this, mValidRegion, invalidRegion, aCallback, aCallbackData);
  }

  mTiledBuffer.ReadLock();

  // Only paint the mask layer on the first transaction.
  if (aMaskLayer && !BasicManager()->IsRepeatTransaction()) {
    static_cast<BasicImplData*>(aMaskLayer->ImplData())
      ->Paint(aContext, nullptr);
  }

  // Create a heap copy owned and released by the compositor. This is needed
  // since we're sending this over an async message and content needs to be
  // be able to modify the tiled buffer in the next transaction.
  // TODO: Remove me once Bug 747811 lands.
  BasicTiledLayerBuffer *heapCopy = new BasicTiledLayerBuffer(mTiledBuffer);

  BasicManager()->PaintedTiledLayerBuffer(BasicManager()->Hold(this), heapCopy);
  mFirstPaint = false;
}

} // mozilla
} // layers
