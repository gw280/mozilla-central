/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#ifndef nsIPlatformCharset_h__
#define nsIPlatformCharset_h__

#include "nsString.h"
#include "nsISupports.h"

// Interface ID for our nsIPlatformCharset interface

// {84B0F181-C6C7-11d2-B3B0-00805F8A6670}
NS_DECLARE_ID(kIPlatformCharsetIID, 
 0x84b0f181, 0xc6c7, 0x11d2, 0xb3, 0xb0, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 );


// Class ID for our PlatformCharset implementation
// {84B0F182-C6C7-11d2-B3B0-00805F8A6670}
NS_DECLARE_ID(kPlatformCharsetCID,  
 0x84b0f182, 0xc6c7, 0x11d2, 0xb3, 0xb0, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 );

typedef enum {
     kPlatformCharsetSel_PlainTextInClipboard = 0,
     kPlatformCharsetSel_FileName = 1,
     kPlatformCharsetSel_Menu = 2
} nsPlatformCharsetSel;

class nsIPlatformCharset : public nsISupports
{
public:

  NS_IMETHOD GetCharset(nsPlatformCharsetSel selector, nsString& oResult) = 0;

};

#endif /* nsIPlatformCharset_h__ */
