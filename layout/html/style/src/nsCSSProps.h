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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef nsCSSProps_h___
#define nsCSSProps_h___

#include "nslayout.h"
#include "nsString.h"

/*
   Declare the enum list using the magic of preprocessing
   enum values are "eCSSProperty_foo" (where foo is the property)

   To change the list of properties, see nsCSSPropList.h

 */
#define CSS_PROP(_name, _id, _hint) eCSSProperty_##_id,
enum nsCSSProperty {
  eCSSProperty_UNKNOWN = -1,
#include "nsCSSPropList.h"
  eCSSProperty_COUNT
};
#undef CSS_PROP


class NS_LAYOUT nsCSSProps {
public:
  static void AddRefTable(void);
  static void ReleaseTable(void);

  // Given a property string, return the enum value
  static nsCSSProperty LookupProperty(const nsAReadableString& aProperty);
  static nsCSSProperty LookupProperty(const nsCString& aProperty);

  // Given a property enum, get the string value
  static const nsCString& GetStringValue(nsCSSProperty aProperty);

  // Given a CSS Property and a Property Enum Value
  // Return back a const nsString& representation of the 
  // value. Return back nullstr if no value is found
  static const nsCString& LookupPropertyValue(nsCSSProperty aProperty, PRInt32 aValue);

  // Get a color name for a predefined color value like buttonhighlight or activeborder
  // Sets the aStr param to the name of the propertyID
  static PRBool GetColorName(PRInt32 aPropID, nsCString &aStr);

  static const PRInt32  kHintTable[];

  static PRInt32 SearchKeywordTableInt(PRInt32 aValue, const PRInt32 aTable[]);
  static const nsCString& SearchKeywordTable(PRInt32 aValue, const PRInt32 aTable[]);

  // Keyword/Enum value tables
  static const PRInt32 kAzimuthKTable[];
  static const PRInt32 kBackgroundAttachmentKTable[];
  static const PRInt32 kBackgroundColorKTable[];
  static const PRInt32 kBackgroundRepeatKTable[];
  static const PRInt32 kBorderCollapseKTable[];
  static const PRInt32 kBorderColorKTable[];
  static const PRInt32 kBorderStyleKTable[];
  static const PRInt32 kBorderWidthKTable[];
  static const PRInt32 kBoxSizingKTable[];
  static const PRInt32 kCaptionSideKTable[];
  static const PRInt32 kClearKTable[];
  static const PRInt32 kColorKTable[];
  static const PRInt32 kContentKTable[];
  static const PRInt32 kCursorKTable[];
  static const PRInt32 kDirectionKTable[];
  static const PRInt32 kDisplayKTable[];
  static const PRInt32 kElevationKTable[];
  static const PRInt32 kEmptyCellsKTable[];
  static const PRInt32 kFloatKTable[];
  static const PRInt32 kFloatEdgeKTable[];
  static const PRInt32 kFontKTable[];
  static const PRInt32 kFontSizeKTable[];
  static const PRInt32 kFontStretchKTable[];
  static const PRInt32 kFontStyleKTable[];
  static const PRInt32 kFontVariantKTable[];
  static const PRInt32 kFontWeightKTable[];
  static const PRInt32 kKeyEquivalentKTable[];
  static const PRInt32 kListStylePositionKTable[];
  static const PRInt32 kListStyleKTable[];
  static const PRInt32 kOutlineColorKTable[];
  static const PRInt32 kOverflowKTable[];
  static const PRInt32 kPageBreakKTable[];
  static const PRInt32 kPageBreakInsideKTable[];
  static const PRInt32 kPageMarksKTable[];
  static const PRInt32 kPageSizeKTable[];
  static const PRInt32 kPitchKTable[];
  static const PRInt32 kPlayDuringKTable[];
  static const PRInt32 kPositionKTable[];
  static const PRInt32 kResizerKTable[];
  static const PRInt32 kSpeakKTable[];
  static const PRInt32 kSpeakHeaderKTable[];
  static const PRInt32 kSpeakNumeralKTable[];
  static const PRInt32 kSpeakPunctuationKTable[];
  static const PRInt32 kSpeechRateKTable[];
  static const PRInt32 kTableLayoutKTable[];
  static const PRInt32 kTextAlignKTable[];
  static const PRInt32 kTextDecorationKTable[];
  static const PRInt32 kTextTransformKTable[];
  static const PRInt32 kUnicodeBidiKTable[];
  static const PRInt32 kUserFocusKTable[];
  static const PRInt32 kUserInputKTable[];
  static const PRInt32 kUserModifyKTable[];
  static const PRInt32 kUserSelectKTable[];
  static const PRInt32 kVerticalAlignKTable[];
  static const PRInt32 kVisibilityKTable[];
  static const PRInt32 kVolumeKTable[];
  static const PRInt32 kWhitespaceKTable[];
};

#endif /* nsCSSProps_h___ */
