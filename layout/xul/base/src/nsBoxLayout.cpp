/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

//
// Eric Vaughan
// Netscape Communications
//
// See documentation in associated header file
//

#include "nsBox.h"
#include "nsIStyleContext.h"
#include "nsIPresContext.h"
#include "nsCOMPtr.h"
#include "nsIContent.h"
#include "nsIViewManager.h"
#include "nsIView.h"
#include "nsIPresShell.h"
#include "nsHTMLContainerFrame.h"
#include "nsIFrame.h"
#include "nsBoxLayout.h"

nsBoxLayout::nsBoxLayout()
{
  NS_INIT_ISUPPORTS();
}

void
nsBoxLayout::AddBorderAndPadding(nsIBox* aBox, nsSize& aSize)
{
  nsBox::AddBorderAndPadding(aBox, aSize);
}

void
nsBoxLayout::AddMargin(nsIBox* aBox, nsSize& aSize)
{
  nsBox::AddMargin(aBox, aSize);
}

void
nsBoxLayout::AddMargin(nsSize& aSize, const nsMargin& aMargin)
{
  nsBox::AddMargin(aSize, aMargin);
}

void
nsBoxLayout::AddInset(nsIBox* aBox, nsSize& aSize)
{
  nsBox::AddInset(aBox, aSize);
}

NS_IMETHODIMP
nsBoxLayout::GetFlex(nsIBox* aBox, nsBoxLayoutState& aState, nscoord& aFlex)
{
  return aBox->GetFlex(aState, aFlex);
}


NS_IMETHODIMP
nsBoxLayout::IsCollapsed(nsIBox* aBox, nsBoxLayoutState& aState, PRBool& aCollapsed)
{
  return aBox->IsCollapsed(aState, aCollapsed);
}

NS_IMETHODIMP
nsBoxLayout::GetPrefSize(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState, nsSize& aSize)
{
  aSize.width = 0;
  aSize.height = 0;
  AddBorderAndPadding(aBox, aSize);
  AddInset(aBox, aSize);

  return NS_OK;
}

NS_IMETHODIMP
nsBoxLayout::GetMinSize(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState, nsSize& aSize)
{
  aSize.width = 0;
  aSize.height = 0;
  AddBorderAndPadding(aBox, aSize);
  AddInset(aBox, aSize);
  return NS_OK;
}

NS_IMETHODIMP
nsBoxLayout::GetMaxSize(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState, nsSize& aSize)
{
  aSize.width = NS_INTRINSICSIZE;
  aSize.height = NS_INTRINSICSIZE;
  AddBorderAndPadding(aBox, aSize);
  AddInset(aBox, aSize);
  return NS_OK;
}


NS_IMETHODIMP
nsBoxLayout::GetAscent(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState, nscoord& aAscent)
{
  aAscent = 0;
  return NS_OK;
}


NS_IMETHODIMP
nsBoxLayout::Layout(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState)
{
  return NS_OK;
}

void
nsBoxLayout::AddLargestSize(nsSize& aSize, const nsSize& aSize2)
{
  if (aSize2.width > aSize.width)
     aSize.width = aSize2.width;

  if (aSize2.height > aSize.height)
     aSize.height = aSize2.height;
}

void
nsBoxLayout::AddSmallestSize(nsSize& aSize, const nsSize& aSize2)
{
  if (aSize2.width < aSize.width)
     aSize.width = aSize2.width;

  if (aSize2.height < aSize.height)
     aSize.height = aSize2.height;
}

NS_IMETHODIMP
nsBoxLayout::PaintDebug(nsIBox* aBox, 
                        nsIPresContext* aPresContext,
                        nsIRenderingContext& aRenderingContext,
                        const nsRect& aDirtyRect,
                        nsFramePaintLayer aWhichLayer)

{
  return NS_OK;
}

NS_IMETHODIMP
nsBoxLayout::DisplayDebugInfoFor(nsIBox* aBox,
                                     nsIPresContext* aPresContext,
                                     nsPoint&        aPoint,
                                     PRInt32&        aCursor)
{
  return NS_OK;
}


// nsISupports
NS_IMPL_ADDREF(nsBoxLayout);
NS_IMPL_RELEASE(nsBoxLayout);

//
// QueryInterface
//
NS_INTERFACE_MAP_BEGIN(nsBoxLayout)
  NS_INTERFACE_MAP_ENTRY(nsIBoxLayout)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END
