/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
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

#ifndef _NSSSLIOLAYER_H
#define _NSSSLIOLAYER_H

#include "prtypes.h"
#include "prio.h"

PR_BEGIN_EXTERN_C

//typedef PRFileDesc* (PR_CALLBACK *NSPRSocketFN)(void);  ??

PR_EXTERN(PRFileDesc *) nsSSLIOLayerNewSocket(const char* hostName, PRFileDesc **fd, nsISupports **securityInfo);

PR_END_EXTERN_C

#endif /* _NSSSLIOLAYER_H */
