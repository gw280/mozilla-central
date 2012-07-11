/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_PLATFORM_GTK_H
#define GFX_PLATFORM_GTK_H

#include "gfxPlatform.h"
#include "nsAutoRef.h"
#include "nsTArray.h"

#if (MOZ_WIDGET_GTK == 2)
extern "C" {
    typedef struct _GdkDrawable GdkDrawable;
}
#endif

class gfxFontconfigUtils;
class FT2FontFamily;
class FT2FontEntry;

typedef struct FT_LibraryRec_ *FT_Library;

class THEBES_API gfxPlatformGtk : public gfxPlatform {
public:
    gfxPlatformGtk();
    virtual ~gfxPlatformGtk();

    static gfxPlatformGtk *GetPlatform() {
        return (gfxPlatformGtk*) gfxPlatform::GetPlatform();
    }

    already_AddRefed<gfxASurface> CreateOffscreenSurface(const gfxIntSize& size,
                                                         gfxASurface::gfxContentType contentType);

    mozilla::RefPtr<mozilla::gfx::ScaledFont>
      GetScaledFontForFont(gfxFont *aFont);

    virtual bool SupportsAzure(mozilla::gfx::BackendType& aBackend);

    nsresult GetFontList(nsIAtom *aLangGroup,
                         const nsACString& aGenericFamily,
                         nsTArray<nsString>& aListOfFonts);

    nsresult UpdateFontList();

    nsresult ResolveFontName(const nsAString& aFontName,
                             FontResolverCallback aCallback,
                             void *aClosure, bool& aAborted);

    nsresult GetStandardFamilyName(const nsAString& aFontName, nsAString& aFamilyName);

    gfxFontGroup *CreateFontGroup(const nsAString &aFamilies,
                                  const gfxFontStyle *aStyle,
                                  gfxUserFontSet *aUserFontSet);

#ifdef MOZ_PANGO
    /**
     * Look up a local platform font using the full font face name (needed to
     * support @font-face src local() )
     */
    virtual gfxFontEntry* LookupLocalFont(const gfxProxyFontEntry *aProxyEntry,
                                          const nsAString& aFontName);

    /**
     * Activate a platform font (needed to support @font-face src url() )
     *
     */
    virtual gfxFontEntry* MakePlatformFont(const gfxProxyFontEntry *aProxyEntry,
                                           const PRUint8 *aFontData,
                                           PRUint32 aLength);

    /**
     * Check whether format is supported on a platform or not (if unclear,
     * returns true).
     */
    virtual bool IsFontFormatSupported(nsIURI *aFontURI,
                                         PRUint32 aFormatFlags);
#endif

    FT2FontFamily *FindFontFamily(const nsAString& aName);
    FT2FontEntry *FindFontEntry(const nsAString& aFamilyName, const gfxFontStyle& aFontStyle);
    already_AddRefed<gfxFont> FindFontForChar(PRUint32 aCh, gfxFont *aFont);
    bool GetPrefFontEntries(const nsCString& aLangGroup, nsTArray<nsRefPtr<gfxFontEntry> > *aFontEntryList);
    void SetPrefFontEntries(const nsCString& aLangGroup, nsTArray<nsRefPtr<gfxFontEntry> >& aFontEntryList);
    virtual gfxPlatformFontList* CreatePlatformFontList();

    FT_Library GetFTLibrary();

#if (MOZ_WIDGET_GTK == 2)
    static void SetGdkDrawable(gfxASurface *target,
                               GdkDrawable *drawable);
    static GdkDrawable *GetGdkDrawable(gfxASurface *target);
#endif

    static PRInt32 GetDPI();

    static bool UseXRender() {
#if defined(MOZ_X11) && defined(MOZ_PLATFORM_MAEMO)
        // XRender is not accelerated on the Maemo at the moment, and 
        // X server pixman is out of our control; it's likely to be 
        // older than (our) cairo's.   So fall back on software 
        // rendering for more predictable performance.
        // This setting will likely not be relevant when we have
        // GL-accelerated compositing. We know of other platforms 
        // with bad drivers where we'd like to also use client side 
        // rendering, but until we have the ability to featuer test 
        // this, we'll only disable this for maemo.
        return true;
#elif defined(MOZ_X11)
        return sUseXRender;
#else
        if (gfxPlatform::UseAzureContentDrawing()) {
            // We want to use image surfaces with Azure
            return true;
        } else {
            return false;
        }
#endif
    }

    static bool UsePangoFonts() {
#ifndef MOZ_PANGO
        return false;
#else
        // Azure/Skia requires the FreeType-only gfxFont implementation
        // rather than the Pango-based one
        return !gfxPlatform::UseAzureContentDrawing();
#endif
    }

    virtual gfxImageFormat GetOffscreenFormat();

protected:
    static gfxFontconfigUtils *sFontconfigUtils;

private:
    virtual qcms_profile *GetPlatformCMSOutputProfile();
#ifdef MOZ_X11
    static bool sUseXRender;
#endif
};

#endif /* GFX_PLATFORM_GTK_H */
