/*
 * Copyright 2012 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef SkAddIntersections_DEFINED
#define SkAddIntersections_DEFINED

#include "SkIntersectionHelper.h"
#include "SkIntersections.h"
#include "SkTDArray.h"

bool AddIntersectTs(SkOpContour* test, SkOpContour* next);
void AddSelfIntersectTs(SkOpContour* test);
void CoincidenceCheck(SkTDArray<SkOpContour*>* contourList, int total);

#endif
