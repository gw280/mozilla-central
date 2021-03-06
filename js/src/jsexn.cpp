/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * JS standard exception implementation.
 */
#include <stdlib.h>
#include <string.h>

#include "mozilla/Util.h"

#include "jstypes.h"
#include "jsutil.h"
#include "jsprf.h"
#include "jsapi.h"
#include "jscntxt.h"
#include "jsversion.h"
#include "jsexn.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsgcmark.h"
#include "jsinterp.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsopcode.h"
#include "jsscope.h"
#include "jsscript.h"
#include "jswrapper.h"

#include "vm/GlobalObject.h"
#include "vm/StringBuffer.h"

#include "jsinferinlines.h"
#include "jsobjinlines.h"

#include "vm/Stack-inl.h"
#include "vm/String-inl.h"

using namespace mozilla;
using namespace js;
using namespace js::gc;
using namespace js::types;

/* Forward declarations for ErrorClass's initializer. */
static JSBool
Exception(JSContext *cx, unsigned argc, Value *vp);

static void
exn_trace(JSTracer *trc, JSObject *obj);

static void
exn_finalize(FreeOp *fop, JSObject *obj);

static JSBool
exn_resolve(JSContext *cx, JSObject *obj, jsid id, unsigned flags,
            JSObject **objp);

Class js::ErrorClass = {
    js_Error_str,
    JSCLASS_HAS_PRIVATE | JSCLASS_IMPLEMENTS_BARRIERS | JSCLASS_NEW_RESOLVE |
    JSCLASS_HAS_CACHED_PROTO(JSProto_Error),
    JS_PropertyStub,         /* addProperty */
    JS_PropertyStub,         /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    (JSResolveOp)exn_resolve,
    JS_ConvertStub,
    exn_finalize,
    NULL,                 /* checkAccess */
    NULL,                 /* call        */
    NULL,                 /* construct   */
    NULL,                 /* hasInstance */
    exn_trace
};

template <typename T>
struct JSStackTraceElemImpl {
    T                   funName;
    size_t              argc;
    const char          *filename;
    unsigned            ulineno;
};

typedef JSStackTraceElemImpl<HeapPtrString> JSStackTraceElem;
typedef JSStackTraceElemImpl<JSString *>    JSStackTraceStackElem;

typedef struct JSExnPrivate {
    /* A copy of the JSErrorReport originally generated. */
    JSErrorReport       *errorReport;
    js::HeapPtrString   message;
    js::HeapPtrString   filename;
    unsigned            lineno;
    size_t              stackDepth;
    int                 exnType;
    JSStackTraceElem    stackElems[1];
} JSExnPrivate;

static JSString *
StackTraceToString(JSContext *cx, JSExnPrivate *priv);

static JSErrorReport *
CopyErrorReport(JSContext *cx, JSErrorReport *report)
{
    /*
     * We use a single malloc block to make a deep copy of JSErrorReport with
     * the following layout:
     *   JSErrorReport
     *   array of copies of report->messageArgs
     *   jschar array with characters for all messageArgs
     *   jschar array with characters for ucmessage
     *   jschar array with characters for uclinebuf and uctokenptr
     *   char array with characters for linebuf and tokenptr
     *   char array with characters for filename
     * Such layout together with the properties enforced by the following
     * asserts does not need any extra alignment padding.
     */
    JS_STATIC_ASSERT(sizeof(JSErrorReport) % sizeof(const char *) == 0);
    JS_STATIC_ASSERT(sizeof(const char *) % sizeof(jschar) == 0);

    size_t filenameSize;
    size_t linebufSize;
    size_t uclinebufSize;
    size_t ucmessageSize;
    size_t i, argsArraySize, argsCopySize, argSize;
    size_t mallocSize;
    JSErrorReport *copy;
    uint8_t *cursor;

#define JS_CHARS_SIZE(jschars) ((js_strlen(jschars) + 1) * sizeof(jschar))

    filenameSize = report->filename ? strlen(report->filename) + 1 : 0;
    linebufSize = report->linebuf ? strlen(report->linebuf) + 1 : 0;
    uclinebufSize = report->uclinebuf ? JS_CHARS_SIZE(report->uclinebuf) : 0;
    ucmessageSize = 0;
    argsArraySize = 0;
    argsCopySize = 0;
    if (report->ucmessage) {
        ucmessageSize = JS_CHARS_SIZE(report->ucmessage);
        if (report->messageArgs) {
            for (i = 0; report->messageArgs[i]; ++i)
                argsCopySize += JS_CHARS_SIZE(report->messageArgs[i]);

            /* Non-null messageArgs should have at least one non-null arg. */
            JS_ASSERT(i != 0);
            argsArraySize = (i + 1) * sizeof(const jschar *);
        }
    }

    /*
     * The mallocSize can not overflow since it represents the sum of the
     * sizes of already allocated objects.
     */
    mallocSize = sizeof(JSErrorReport) + argsArraySize + argsCopySize +
                 ucmessageSize + uclinebufSize + linebufSize + filenameSize;
    cursor = (uint8_t *)cx->malloc_(mallocSize);
    if (!cursor)
        return NULL;

    copy = (JSErrorReport *)cursor;
    memset(cursor, 0, sizeof(JSErrorReport));
    cursor += sizeof(JSErrorReport);

    if (argsArraySize != 0) {
        copy->messageArgs = (const jschar **)cursor;
        cursor += argsArraySize;
        for (i = 0; report->messageArgs[i]; ++i) {
            copy->messageArgs[i] = (const jschar *)cursor;
            argSize = JS_CHARS_SIZE(report->messageArgs[i]);
            js_memcpy(cursor, report->messageArgs[i], argSize);
            cursor += argSize;
        }
        copy->messageArgs[i] = NULL;
        JS_ASSERT(cursor == (uint8_t *)copy->messageArgs[0] + argsCopySize);
    }

    if (report->ucmessage) {
        copy->ucmessage = (const jschar *)cursor;
        js_memcpy(cursor, report->ucmessage, ucmessageSize);
        cursor += ucmessageSize;
    }

    if (report->uclinebuf) {
        copy->uclinebuf = (const jschar *)cursor;
        js_memcpy(cursor, report->uclinebuf, uclinebufSize);
        cursor += uclinebufSize;
        if (report->uctokenptr) {
            copy->uctokenptr = copy->uclinebuf + (report->uctokenptr -
                                                  report->uclinebuf);
        }
    }

    if (report->linebuf) {
        copy->linebuf = (const char *)cursor;
        js_memcpy(cursor, report->linebuf, linebufSize);
        cursor += linebufSize;
        if (report->tokenptr) {
            copy->tokenptr = copy->linebuf + (report->tokenptr -
                                              report->linebuf);
        }
    }

    if (report->filename) {
        copy->filename = (const char *)cursor;
        js_memcpy(cursor, report->filename, filenameSize);
    }
    JS_ASSERT(cursor + filenameSize == (uint8_t *)copy + mallocSize);

    /* HOLD called by the destination error object. */
    copy->originPrincipals = report->originPrincipals;

    /* Copy non-pointer members. */
    copy->lineno = report->lineno;
    copy->errorNumber = report->errorNumber;

    /* Note that this is before it gets flagged with JSREPORT_EXCEPTION */
    copy->flags = report->flags;

#undef JS_CHARS_SIZE
    return copy;
}

static HeapValue *
GetStackTraceValueBuffer(JSExnPrivate *priv)
{
    /*
     * We use extra memory after JSExnPrivateInfo.stackElems to store jsvals
     * that helps to produce more informative stack traces. The following
     * assert allows us to assume that no gap after stackElems is necessary to
     * align the buffer properly.
     */
    JS_STATIC_ASSERT(sizeof(JSStackTraceElem) % sizeof(jsval) == 0);

    return reinterpret_cast<HeapValue *>(priv->stackElems + priv->stackDepth);
}

struct SuppressErrorsGuard
{
    JSContext *cx;
    JSErrorReporter prevReporter;
    JSExceptionState *prevState;

    SuppressErrorsGuard(JSContext *cx)
      : cx(cx),
        prevReporter(JS_SetErrorReporter(cx, NULL)),
        prevState(JS_SaveExceptionState(cx))
    {}

    ~SuppressErrorsGuard()
    {
        JS_RestoreExceptionState(cx, prevState);
        JS_SetErrorReporter(cx, prevReporter);
    }
};

struct AppendWrappedArg {
    JSContext *cx;
    AutoValueVector &values;
    AppendWrappedArg(JSContext *cx, AutoValueVector &values)
      : cx(cx),
        values(values)
    {}

    bool operator()(unsigned, Value *vp) {
        Value v = *vp;

        /*
         * Try to wrap.
         *
         * If wrap() fails, there's a good chance that it's because we're
         * already in the process of throwing a native stack limit exception.
         *
         * This causes wrap() to throw, but it can't actually create an exception
         * because we're already making one here, and cx->generatingError is true.
         * So it returns false without an exception set on the stack. If we propagate
         * that, it constitutes an uncatchable exception.
         *
         * So we just ignore exceptions. If wrap actually does set a pending
         * exception, or if the caller sloppily left an exception on cx (which the
         * e4x parser does), it doesn't matter - it will be overwritten shortly.
         *
         * NB: In the sloppy e4x case, one might thing we should clear the
         * exception before calling wrap(). But wrap() has to be ok with pending
         * exceptions, since it wraps exception objects during cross-compartment
         * unwinding.
         */
        if (!cx->compartment->wrap(cx, &v))
            v = JSVAL_VOID;

        /* Append the value. */
        return values.append(v);
    }
};

static void
SetExnPrivate(JSContext *cx, JSObject *exnObject, JSExnPrivate *priv);

static bool
InitExnPrivate(JSContext *cx, JSObject *exnObject, JSString *message,
               JSString *filename, unsigned lineno, JSErrorReport *report, int exnType)
{
    JS_ASSERT(exnObject->isError());
    JS_ASSERT(!exnObject->getPrivate());

    JSCheckAccessOp checkAccess = cx->runtime->securityCallbacks->checkObjectAccess;

    Vector<JSStackTraceStackElem> frames(cx);
    AutoValueVector values(cx);
    {
        SuppressErrorsGuard seg(cx);
        for (FrameRegsIter i(cx); !i.done(); ++i) {
            StackFrame *fp = i.fp();

            /*
             * Ask the crystal CAPS ball whether we can see values across
             * compartment boundaries.
             *
             * NB: 'fp' may point to cross-compartment values that require wrapping.
             */
            if (checkAccess && fp->isNonEvalFunctionFrame()) {
                Value v = NullValue();
                jsid callerid = ATOM_TO_JSID(cx->runtime->atomState.callerAtom);
                if (!checkAccess(cx, &fp->callee(), callerid, JSACC_READ, &v))
                    break;
            }

            if (!frames.growBy(1))
                return false;
            JSStackTraceStackElem &frame = frames.back();
            if (fp->isNonEvalFunctionFrame()) {
                frame.funName = fp->fun()->atom ? fp->fun()->atom : cx->runtime->emptyString;
                frame.argc = fp->numActualArgs();
                if (!fp->forEachCanonicalActualArg(AppendWrappedArg(cx, values)))
                    return false;
            } else {
                frame.funName = NULL;
                frame.argc = 0;
            }
            if (fp->isScriptFrame()) {
                frame.filename = SaveScriptFilename(cx, fp->script()->filename);
                if (!frame.filename)
                    return false;
                frame.ulineno = PCToLineNumber(fp->script(), i.pc());
            } else {
                frame.ulineno = 0;
                frame.filename = NULL;
            }
        }
    }

    /* Do not need overflow check: the vm stack is already bigger. */
    JS_STATIC_ASSERT(sizeof(JSStackTraceElem) <= sizeof(StackFrame));

    size_t nbytes = offsetof(JSExnPrivate, stackElems) +
                    frames.length() * sizeof(JSStackTraceElem) +
                    values.length() * sizeof(HeapValue);

    JSExnPrivate *priv = (JSExnPrivate *)cx->malloc_(nbytes);
    if (!priv)
        return false;

    /* Initialize to zero so that write barriers don't witness undefined values. */
    memset(priv, 0, nbytes);

    if (report) {
        /*
         * Construct a new copy of the error report struct. We can't use the
         * error report struct that was passed in, because it's allocated on
         * the stack, and also because it may point to transient data in the
         * TokenStream.
         */
        priv->errorReport = CopyErrorReport(cx, report);
        if (!priv->errorReport) {
            cx->free_(priv);
            return false;
        }
    } else {
        priv->errorReport = NULL;
    }

    priv->message.init(message);
    priv->filename.init(filename);
    priv->lineno = lineno;
    priv->stackDepth = frames.length();
    priv->exnType = exnType;

    JSStackTraceElem *framesDest = priv->stackElems;
    HeapValue *valuesDest = reinterpret_cast<HeapValue *>(framesDest + frames.length());
    JS_ASSERT(valuesDest == GetStackTraceValueBuffer(priv));

    for (size_t i = 0; i < frames.length(); ++i) {
        framesDest[i].funName.init(frames[i].funName);
        framesDest[i].argc = frames[i].argc;
        framesDest[i].filename = frames[i].filename;
        framesDest[i].ulineno = frames[i].ulineno;
    }
    for (size_t i = 0; i < values.length(); ++i)
        valuesDest[i].init(cx->compartment, values[i]);

    SetExnPrivate(cx, exnObject, priv);
    return true;
}

static inline JSExnPrivate *
GetExnPrivate(JSObject *obj)
{
    JS_ASSERT(obj->isError());
    return (JSExnPrivate *) obj->getPrivate();
}

static void
exn_trace(JSTracer *trc, JSObject *obj)
{
    JSExnPrivate *priv;
    JSStackTraceElem *elem;
    size_t vcount, i;
    HeapValue *vp;

    priv = GetExnPrivate(obj);
    if (priv) {
        if (priv->message)
            MarkString(trc, &priv->message, "exception message");
        if (priv->filename)
            MarkString(trc, &priv->filename, "exception filename");

        elem = priv->stackElems;
        for (vcount = i = 0; i != priv->stackDepth; ++i, ++elem) {
            if (elem->funName)
                MarkString(trc, &elem->funName, "stack trace function name");
            if (IS_GC_MARKING_TRACER(trc) && elem->filename)
                MarkScriptFilename(elem->filename);
            vcount += elem->argc;
        }
        vp = GetStackTraceValueBuffer(priv);
        for (i = 0; i != vcount; ++i, ++vp)
            MarkValue(trc, vp, "stack trace argument");
    }
}

/* NB: An error object's private must be set through this function. */
static void
SetExnPrivate(JSContext *cx, JSObject *exnObject, JSExnPrivate *priv)
{
    JS_ASSERT(!exnObject->getPrivate());
    JS_ASSERT(exnObject->isError());
    if (JSErrorReport *report = priv->errorReport) {
        if (JSPrincipals *prin = report->originPrincipals)
            JS_HoldPrincipals(prin);
    }
    exnObject->setPrivate(priv);
}

static void
exn_finalize(FreeOp *fop, JSObject *obj)
{
    if (JSExnPrivate *priv = GetExnPrivate(obj)) {
        if (JSErrorReport *report = priv->errorReport) {
            /* HOLD called by SetExnPrivate. */
            if (JSPrincipals *prin = report->originPrincipals)
                JS_DropPrincipals(fop->runtime(), prin);
            fop->free_(report);
        }
        fop->free_(priv);
    }
}

static JSBool
exn_resolve(JSContext *cx, JSObject *obj, jsid id, unsigned flags,
            JSObject **objp)
{
    JSExnPrivate *priv;
    JSString *str;
    JSAtom *atom;
    JSString *stack;
    const char *prop;
    jsval v;
    unsigned attrs;

    *objp = NULL;
    priv = GetExnPrivate(obj);
    if (priv && JSID_IS_ATOM(id)) {
        str = JSID_TO_STRING(id);

        atom = cx->runtime->atomState.messageAtom;
        if (str == atom) {
            prop = js_message_str;

            /*
             * Per ES5 15.11.1.1, if Error is called with no argument or with
             * undefined as the argument, it returns an Error object with no
             * own message property.
             */
            if (!priv->message)
                return true;

            v = STRING_TO_JSVAL(priv->message);
            attrs = 0;
            goto define;
        }

        atom = cx->runtime->atomState.fileNameAtom;
        if (str == atom) {
            prop = js_fileName_str;
            v = STRING_TO_JSVAL(priv->filename);
            attrs = JSPROP_ENUMERATE;
            goto define;
        }

        atom = cx->runtime->atomState.lineNumberAtom;
        if (str == atom) {
            prop = js_lineNumber_str;
            v = INT_TO_JSVAL(priv->lineno);
            attrs = JSPROP_ENUMERATE;
            goto define;
        }

        atom = cx->runtime->atomState.stackAtom;
        if (str == atom) {
            stack = StackTraceToString(cx, priv);
            if (!stack)
                return false;

            prop = js_stack_str;
            v = STRING_TO_JSVAL(stack);
            attrs = JSPROP_ENUMERATE;
            goto define;
        }
    }
    return true;

  define:
    if (!JS_DefineProperty(cx, obj, prop, v, NULL, NULL, attrs))
        return false;
    *objp = obj;
    return true;
}

JSErrorReport *
js_ErrorFromException(JSContext *cx, jsval exn)
{
    JSObject *obj;
    JSExnPrivate *priv;

    if (JSVAL_IS_PRIMITIVE(exn))
        return NULL;
    obj = JSVAL_TO_OBJECT(exn);
    if (!obj->isError())
        return NULL;
    priv = GetExnPrivate(obj);
    if (!priv)
        return NULL;
    return priv->errorReport;
}

static JSString *
ValueToShortSource(JSContext *cx, const Value &v)
{
    JSString *str;

    /* Avoid toSource bloat and fallibility for object types. */
    if (!v.isObject())
        return js_ValueToSource(cx, v);

    JSObject *obj = &v.toObject();
    AutoCompartment ac(cx, obj);
    if (!ac.enter())
        return NULL;

    if (obj->isFunction()) {
        /*
         * XXX Avoid function decompilation bloat for now.
         */
        str = JS_GetFunctionId(obj->toFunction());
        if (!str && !(str = js_ValueToSource(cx, v))) {
            /*
             * Continue to soldier on if the function couldn't be
             * converted into a string.
             */
            JS_ClearPendingException(cx);
            str = JS_NewStringCopyZ(cx, "[unknown function]");
        }
    } else {
        /*
         * XXX Avoid toString on objects, it takes too long and uses too much
         * memory, for too many classes (see Mozilla bug 166743).
         */
        char buf[100];
        JS_snprintf(buf, sizeof buf, "[object %s]", js::UnwrapObject(obj, false)->getClass()->name);
        str = JS_NewStringCopyZ(cx, buf);
    }

    ac.leave();

    if (!str || !cx->compartment->wrap(cx, &str))
        return NULL;
    return str;
}

static JSString *
StackTraceToString(JSContext *cx, JSExnPrivate *priv)
{
    jschar *stackbuf;
    size_t stacklen, stackmax;
    JSStackTraceElem *elem, *endElem;
    HeapValue *values;
    size_t i;
    JSString *str;
    const char *cp;
    char ulnbuf[11];

    /* After this point, failing control flow must goto bad. */
    stackbuf = NULL;
    stacklen = stackmax = 0;

/* Limit the stackbuf length to a reasonable value to avoid overflow checks. */
#define STACK_LENGTH_LIMIT JS_BIT(20)

#define APPEND_CHAR_TO_STACK(c)                                               \
    JS_BEGIN_MACRO                                                            \
        if (stacklen == stackmax) {                                           \
            void *ptr_;                                                       \
            if (stackmax >= STACK_LENGTH_LIMIT)                               \
                goto done;                                                    \
            stackmax = stackmax ? 2 * stackmax : 64;                          \
            ptr_ = cx->realloc_(stackbuf, (stackmax+1) * sizeof(jschar));      \
            if (!ptr_)                                                        \
                goto bad;                                                     \
            stackbuf = (jschar *) ptr_;                                       \
        }                                                                     \
        stackbuf[stacklen++] = (c);                                           \
    JS_END_MACRO

#define APPEND_STRING_TO_STACK(str)                                           \
    JS_BEGIN_MACRO                                                            \
        JSString *str_ = str;                                                 \
        size_t length_ = str_->length();                                      \
        const jschar *chars_ = str_->getChars(cx);                            \
        if (!chars_)                                                          \
            goto bad;                                                         \
                                                                              \
        if (length_ > stackmax - stacklen) {                                  \
            void *ptr_;                                                       \
            if (stackmax >= STACK_LENGTH_LIMIT ||                             \
                length_ >= STACK_LENGTH_LIMIT - stacklen) {                   \
                goto done;                                                    \
            }                                                                 \
            stackmax = RoundUpPow2(stacklen + length_);                       \
            ptr_ = cx->realloc_(stackbuf, (stackmax+1) * sizeof(jschar));     \
            if (!ptr_)                                                        \
                goto bad;                                                     \
            stackbuf = (jschar *) ptr_;                                       \
        }                                                                     \
        js_strncpy(stackbuf + stacklen, chars_, length_);                     \
        stacklen += length_;                                                  \
    JS_END_MACRO

    values = GetStackTraceValueBuffer(priv);
    elem = priv->stackElems;
    for (endElem = elem + priv->stackDepth; elem != endElem; elem++) {
        if (elem->funName) {
            APPEND_STRING_TO_STACK(elem->funName);
            APPEND_CHAR_TO_STACK('(');
            for (i = 0; i != elem->argc; i++, values++) {
                if (i > 0)
                    APPEND_CHAR_TO_STACK(',');
                str = ValueToShortSource(cx, *values);
                if (!str)
                    goto bad;
                APPEND_STRING_TO_STACK(str);
            }
            APPEND_CHAR_TO_STACK(')');
        }
        APPEND_CHAR_TO_STACK('@');
        if (elem->filename) {
            for (cp = elem->filename; *cp; cp++)
                APPEND_CHAR_TO_STACK(*cp);
        }
        APPEND_CHAR_TO_STACK(':');
        JS_snprintf(ulnbuf, sizeof ulnbuf, "%u", elem->ulineno);
        for (cp = ulnbuf; *cp; cp++)
            APPEND_CHAR_TO_STACK(*cp);
        APPEND_CHAR_TO_STACK('\n');
    }
#undef APPEND_CHAR_TO_STACK
#undef APPEND_STRING_TO_STACK
#undef STACK_LENGTH_LIMIT

  done:
    if (stacklen == 0) {
        JS_ASSERT(!stackbuf);
        return cx->runtime->emptyString;
    }
    if (stacklen < stackmax) {
        /*
         * Realloc can fail when shrinking on some FreeBSD versions, so
         * don't use JS_realloc here; simply let the oversized allocation
         * be owned by the string in that rare case.
         */
        void *shrunk = cx->realloc_(stackbuf, (stacklen+1) * sizeof(jschar));
        if (shrunk)
            stackbuf = (jschar *) shrunk;
    }

    stackbuf[stacklen] = 0;
    str = js_NewString(cx, stackbuf, stacklen);
    if (str)
        return str;

  bad:
    if (stackbuf)
        cx->free_(stackbuf);
    return NULL;
}

/* XXXbe Consolidate the ugly truth that we don't treat filename as UTF-8
         with these two functions. */
static JSString *
FilenameToString(JSContext *cx, const char *filename)
{
    return JS_NewStringCopyZ(cx, filename);
}

static JSBool
Exception(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    /*
     * ECMA ed. 3, 15.11.1 requires Error, etc., to construct even when
     * called as functions, without operator new.  But as we do not give
     * each constructor a distinct JSClass, whose .name member is used by
     * NewNativeClassInstance to find the class prototype, we must get the
     * class prototype ourselves.
     */
    Value protov;
    if (!args.callee().getProperty(cx, cx->runtime->atomState.classPrototypeAtom, &protov))
        return false;

    if (!protov.isObject()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_PROTOTYPE, "Error");
        return false;
    }

    JSObject *errProto = &protov.toObject();
    JSObject *obj = NewObjectWithGivenProto(cx, &ErrorClass, errProto, NULL);
    if (!obj)
        return false;

    /* Set the 'message' property. */
    JSString *message;
    if (args.hasDefined(0)) {
        message = ToString(cx, args[0]);
        if (!message)
            return false;
        args[0].setString(message);
    } else {
        message = NULL;
    }

    /* Find the scripted caller. */
    FrameRegsIter iter(cx);
    while (!iter.done() && !iter.fp()->isScriptFrame())
        ++iter;

    /* Set the 'fileName' property. */
    JSString *filename;
    if (args.length() > 1) {
        filename = ToString(cx, args[1]);
        if (!filename)
            return false;
        args[1].setString(filename);
    } else {
        if (!iter.done()) {
            filename = FilenameToString(cx, iter.fp()->script()->filename);
            if (!filename)
                return false;
        } else {
            filename = cx->runtime->emptyString;
        }
    }

    /* Set the 'lineNumber' property. */
    uint32_t lineno;
    if (args.length() > 2) {
        if (!ToUint32(cx, args[2], &lineno))
            return false;
    } else {
        lineno = iter.done() ? 0 : PCToLineNumber(iter.fp()->script(), iter.pc());
    }

    int exnType = args.callee().toFunction()->getExtendedSlot(0).toInt32();
    if (!InitExnPrivate(cx, obj, message, filename, lineno, NULL, exnType))
        return false;

    args.rval().setObject(*obj);
    return true;
}

/* ES5 15.11.4.4 (NB: with subsequent errata). */
static JSBool
exn_toString(JSContext *cx, unsigned argc, Value *vp)
{
    JS_CHECK_RECURSION(cx, return false);
    CallArgs args = CallArgsFromVp(argc, vp);

    /* Step 2. */
    if (!args.thisv().isObject()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_PROTOTYPE, "Error");
        return false;
    }

    /* Step 1. */
    JSObject &obj = args.thisv().toObject();

    /* Step 3. */
    Value nameVal;
    if (!obj.getProperty(cx, cx->runtime->atomState.nameAtom, &nameVal))
        return false;

    /* Step 4. */
    JSString *name;
    if (nameVal.isUndefined()) {
        name = CLASS_ATOM(cx, Error);
    } else {
        name = ToString(cx, nameVal);
        if (!name)
            return false;
    }

    /* Step 5. */
    Value msgVal;
    if (!obj.getProperty(cx, cx->runtime->atomState.messageAtom, &msgVal))
        return false;

    /* Step 6. */
    JSString *message;
    if (msgVal.isUndefined()) {
        message = cx->runtime->emptyString;
    } else {
        message = ToString(cx, msgVal);
        if (!message)
            return false;
    }

    /* Step 7. */
    if (name->empty() && message->empty()) {
        args.rval().setString(CLASS_ATOM(cx, Error));
        return true;
    }

    /* Step 8. */
    if (name->empty()) {
        args.rval().setString(message);
        return true;
    }

    /* Step 9. */
    if (message->empty()) {
        args.rval().setString(name);
        return true;
    }

    /* Step 10. */
    StringBuffer sb(cx);
    if (!sb.append(name) || !sb.append(": ") || !sb.append(message))
        return false;

    JSString *str = sb.finishString();
    if (!str)
        return false;
    args.rval().setString(str);
    return true;
}

#if JS_HAS_TOSOURCE
/*
 * Return a string that may eval to something similar to the original object.
 */
static JSBool
exn_toSource(JSContext *cx, unsigned argc, Value *vp)
{
    JS_CHECK_RECURSION(cx, return false);
    CallArgs args = CallArgsFromVp(argc, vp);

    JSObject *obj = ToObject(cx, &args.thisv());
    if (!obj)
        return false;

    Value nameVal;
    JSString *name;
    if (!obj->getProperty(cx, cx->runtime->atomState.nameAtom, &nameVal) ||
        !(name = ToString(cx, nameVal)))
    {
        return false;
    }

    Value messageVal;
    JSString *message;
    if (!obj->getProperty(cx, cx->runtime->atomState.messageAtom, &messageVal) ||
        !(message = js_ValueToSource(cx, messageVal)))
    {
        return false;
    }

    Value filenameVal;
    JSString *filename;
    if (!obj->getProperty(cx, cx->runtime->atomState.fileNameAtom, &filenameVal) ||
        !(filename = js_ValueToSource(cx, filenameVal)))
    {
        return false;
    }

    Value linenoVal;
    uint32_t lineno;
    if (!obj->getProperty(cx, cx->runtime->atomState.lineNumberAtom, &linenoVal) ||
        !ToUint32(cx, linenoVal, &lineno))
    {
        return false;
    }

    StringBuffer sb(cx);
    if (!sb.append("(new ") || !sb.append(name) || !sb.append("("))
        return false;

    if (!sb.append(message))
        return false;

    if (!filename->empty()) {
        if (!sb.append(", ") || !sb.append(filename))
            return false;
    }
    if (lineno != 0) {
        /* We have a line, but no filename, add empty string */
        if (filename->empty() && !sb.append(", \"\""))
                return false;

        JSString *linenumber = ToString(cx, linenoVal);
        if (!linenumber)
            return false;
        if (!sb.append(", ") || !sb.append(linenumber))
            return false;
    }

    if (!sb.append("))"))
        return false;

    JSString *str = sb.finishString();
    if (!str)
        return false;
    args.rval().setString(str);
    return true;
}
#endif

static JSFunctionSpec exception_methods[] = {
#if JS_HAS_TOSOURCE
    JS_FN(js_toSource_str,   exn_toSource,           0,0),
#endif
    JS_FN(js_toString_str,   exn_toString,           0,0),
    JS_FS_END
};

/* JSProto_ ordering for exceptions shall match JSEXN_ constants. */
JS_STATIC_ASSERT(JSEXN_ERR == 0);
JS_STATIC_ASSERT(JSProto_Error + JSEXN_INTERNALERR  == JSProto_InternalError);
JS_STATIC_ASSERT(JSProto_Error + JSEXN_EVALERR      == JSProto_EvalError);
JS_STATIC_ASSERT(JSProto_Error + JSEXN_RANGEERR     == JSProto_RangeError);
JS_STATIC_ASSERT(JSProto_Error + JSEXN_REFERENCEERR == JSProto_ReferenceError);
JS_STATIC_ASSERT(JSProto_Error + JSEXN_SYNTAXERR    == JSProto_SyntaxError);
JS_STATIC_ASSERT(JSProto_Error + JSEXN_TYPEERR      == JSProto_TypeError);
JS_STATIC_ASSERT(JSProto_Error + JSEXN_URIERR       == JSProto_URIError);

static JSObject *
InitErrorClass(JSContext *cx, GlobalObject *global, int type, JSObject &proto)
{
    JSProtoKey key = GetExceptionProtoKey(type);
    JSAtom *name = cx->runtime->atomState.classAtoms[key];
    JSObject *errorProto = global->createBlankPrototypeInheriting(cx, &ErrorClass, proto);
    if (!errorProto)
        return NULL;

    Value empty = StringValue(cx->runtime->emptyString);
    jsid nameId = ATOM_TO_JSID(cx->runtime->atomState.nameAtom);
    jsid messageId = ATOM_TO_JSID(cx->runtime->atomState.messageAtom);
    jsid fileNameId = ATOM_TO_JSID(cx->runtime->atomState.fileNameAtom);
    jsid lineNumberId = ATOM_TO_JSID(cx->runtime->atomState.lineNumberAtom);
    if (!DefineNativeProperty(cx, errorProto, nameId, StringValue(name),
                              JS_PropertyStub, JS_StrictPropertyStub, 0, 0, 0) ||
        !DefineNativeProperty(cx, errorProto, messageId, empty,
                              JS_PropertyStub, JS_StrictPropertyStub, 0, 0, 0) ||
        !DefineNativeProperty(cx, errorProto, fileNameId, empty,
                              JS_PropertyStub, JS_StrictPropertyStub, JSPROP_ENUMERATE, 0, 0) ||
        !DefineNativeProperty(cx, errorProto, lineNumberId, Int32Value(0),
                              JS_PropertyStub, JS_StrictPropertyStub, JSPROP_ENUMERATE, 0, 0))
    {
        return NULL;
    }

    /* Create the corresponding constructor. */
    JSFunction *ctor = global->createConstructor(cx, Exception, name, 1,
                                                 JSFunction::ExtendedFinalizeKind);
    if (!ctor)
        return NULL;
    ctor->setExtendedSlot(0, Int32Value(int32_t(type)));

    if (!LinkConstructorAndPrototype(cx, ctor, errorProto))
        return NULL;

    if (!DefineConstructorAndPrototype(cx, global, key, ctor, errorProto))
        return NULL;

    JS_ASSERT(!errorProto->getPrivate());

    return errorProto;
}

JSObject *
js_InitExceptionClasses(JSContext *cx, JSObject *obj)
{
    JS_ASSERT(obj->isGlobal());
    JS_ASSERT(obj->isNative());

    GlobalObject *global = &obj->asGlobal();

    JSObject *objectProto = global->getOrCreateObjectPrototype(cx);
    if (!objectProto)
        return NULL;

    /* Initialize the base Error class first. */
    JSObject *errorProto = InitErrorClass(cx, global, JSEXN_ERR, *objectProto);
    if (!errorProto)
        return NULL;

    /* |Error.prototype| alone has method properties. */
    if (!DefinePropertiesAndBrand(cx, errorProto, NULL, exception_methods))
        return NULL;

    /* Define all remaining *Error constructors. */
    for (int i = JSEXN_ERR + 1; i < JSEXN_LIMIT; i++) {
        if (!InitErrorClass(cx, global, i, *errorProto))
            return NULL;
    }

    return errorProto;
}

const JSErrorFormatString*
js_GetLocalizedErrorMessage(JSContext* cx, void *userRef, const char *locale,
                            const unsigned errorNumber)
{
    const JSErrorFormatString *errorString = NULL;

    if (cx->localeCallbacks && cx->localeCallbacks->localeGetErrorMessage) {
        errorString = cx->localeCallbacks
                        ->localeGetErrorMessage(userRef, locale, errorNumber);
    }
    if (!errorString)
        errorString = js_GetErrorMessage(userRef, locale, errorNumber);
    return errorString;
}

#if defined ( DEBUG_mccabe ) && defined ( PRINTNAMES )
/* For use below... get character strings for error name and exception name */
static struct exnname { char *name; char *exception; } errortoexnname[] = {
#define MSG_DEF(name, number, count, exception, format) \
    {#name, #exception},
#include "js.msg"
#undef MSG_DEF
};
#endif /* DEBUG */

JSBool
js_ErrorToException(JSContext *cx, const char *message, JSErrorReport *reportp,
                    JSErrorCallback callback, void *userRef)
{
    JSErrNum errorNumber;
    const JSErrorFormatString *errorString;
    JSExnType exn;
    jsval tv[4];
    JSObject *errProto, *errObject;
    JSString *messageStr, *filenameStr;

    /*
     * Tell our caller to report immediately if this report is just a warning.
     */
    JS_ASSERT(reportp);
    if (JSREPORT_IS_WARNING(reportp->flags))
        return false;

    /* Find the exception index associated with this error. */
    errorNumber = (JSErrNum) reportp->errorNumber;
    if (!callback || callback == js_GetErrorMessage)
        errorString = js_GetLocalizedErrorMessage(cx, NULL, NULL, errorNumber);
    else
        errorString = callback(userRef, NULL, errorNumber);
    exn = errorString ? (JSExnType) errorString->exnType : JSEXN_NONE;
    JS_ASSERT(exn < JSEXN_LIMIT);

#if defined( DEBUG_mccabe ) && defined ( PRINTNAMES )
    /* Print the error name and the associated exception name to stderr */
    fprintf(stderr, "%s\t%s\n",
            errortoexnname[errorNumber].name,
            errortoexnname[errorNumber].exception);
#endif

    /*
     * Return false (no exception raised) if no exception is associated
     * with the given error number.
     */
    if (exn == JSEXN_NONE)
        return false;

    /* Prevent infinite recursion. */
    if (cx->generatingError)
        return false;
    AutoScopedAssign<bool> asa(&cx->generatingError, true);

    /* Protect the newly-created strings below from nesting GCs. */
    PodArrayZero(tv);
    AutoArrayRooter tvr(cx, ArrayLength(tv), tv);

    /*
     * Try to get an appropriate prototype by looking up the corresponding
     * exception constructor name in the scope chain of the current context's
     * top stack frame, or in the global object if no frame is active.
     */
    if (!js_GetClassPrototype(cx, NULL, GetExceptionProtoKey(exn), &errProto))
        return false;
    tv[0] = OBJECT_TO_JSVAL(errProto);

    if (!(errObject = NewObjectWithGivenProto(cx, &ErrorClass, errProto, NULL)))
        return false;
    tv[1] = OBJECT_TO_JSVAL(errObject);

    if (!(messageStr = JS_NewStringCopyZ(cx, message)))
        return false;
    tv[2] = STRING_TO_JSVAL(messageStr);

    if (!(filenameStr = JS_NewStringCopyZ(cx, reportp->filename)))
        return false;
    tv[3] = STRING_TO_JSVAL(filenameStr);

    if (!InitExnPrivate(cx, errObject, messageStr, filenameStr,
                        reportp->lineno, reportp, exn)) {
        return false;
    }

    JS_SetPendingException(cx, OBJECT_TO_JSVAL(errObject));

    /* Flag the error report passed in to indicate an exception was raised. */
    reportp->flags |= JSREPORT_EXCEPTION;
    return true;
}

static bool
IsDuckTypedErrorObject(JSContext *cx, JSObject *exnObject, const char **filename_strp)
{
    JSBool found;
    if (!JS_HasProperty(cx, exnObject, js_message_str, &found) || !found)
        return false;

    const char *filename_str = *filename_strp;
    if (!JS_HasProperty(cx, exnObject, filename_str, &found) || !found) {
        /* DOMException duck quacks "filename" (all lowercase) */
        filename_str = "filename";
        if (!JS_HasProperty(cx, exnObject, filename_str, &found) || !found)
            return false;
    }

    if (!JS_HasProperty(cx, exnObject, js_lineNumber_str, &found) || !found)
        return false;

    *filename_strp = filename_str;
    return true;
}

JSBool
js_ReportUncaughtException(JSContext *cx)
{
    jsval exn;
    JSObject *exnObject;
    jsval roots[6];
    JSErrorReport *reportp, report;
    JSString *str;

    if (!JS_IsExceptionPending(cx))
        return true;

    if (!JS_GetPendingException(cx, &exn))
        return false;

    PodArrayZero(roots);
    AutoArrayRooter tvr(cx, ArrayLength(roots), roots);

    /*
     * Because ToString below could error and an exception object could become
     * unrooted, we must root exnObject.  Later, if exnObject is non-null, we
     * need to root other intermediates, so allocate an operand stack segment
     * to protect all of these values.
     */
    if (JSVAL_IS_PRIMITIVE(exn)) {
        exnObject = NULL;
    } else {
        exnObject = JSVAL_TO_OBJECT(exn);
        roots[0] = exn;
    }

    JS_ClearPendingException(cx);
    reportp = js_ErrorFromException(cx, exn);

    /* XXX L10N angels cry once again. see also everywhere else */
    str = ToString(cx, exn);
    if (str)
        roots[1] = StringValue(str);

    const char *filename_str = js_fileName_str;
    JSAutoByteString filename;
    if (!reportp && exnObject &&
        (exnObject->isError() ||
         IsDuckTypedErrorObject(cx, exnObject, &filename_str)))
    {
        JSString *name = NULL;
        if (JS_GetProperty(cx, exnObject, js_name_str, &roots[2]) &&
            JSVAL_IS_STRING(roots[2]))
        {
            name = JSVAL_TO_STRING(roots[2]);
        }

        JSString *msg = NULL;
        if (JS_GetProperty(cx, exnObject, js_message_str, &roots[3]) &&
            JSVAL_IS_STRING(roots[3]))
        {
            msg = JSVAL_TO_STRING(roots[3]);
        }

        if (name && msg) {
            JSString *colon = JS_NewStringCopyZ(cx, ": ");
            if (!colon)
                return false;
            JSString *nameColon = JS_ConcatStrings(cx, name, colon);
            if (!nameColon)
                return false;
            str = JS_ConcatStrings(cx, nameColon, msg);
            if (!str)
                return false;
        } else if (name) {
            str = name;
        } else if (msg) {
            str = msg;
        }

        if (JS_GetProperty(cx, exnObject, filename_str, &roots[4])) {
            JSString *tmp = ToString(cx, roots[4]);
            if (tmp)
                filename.encode(cx, tmp);
        }

        uint32_t lineno;
        if (!JS_GetProperty(cx, exnObject, js_lineNumber_str, &roots[5]) ||
            !ToUint32(cx, roots[5], &lineno))
        {
            lineno = 0;
        }

        reportp = &report;
        PodZero(&report);
        report.filename = filename.ptr();
        report.lineno = (unsigned) lineno;
        if (str) {
            if (JSFixedString *fixed = str->ensureFixed(cx))
                report.ucmessage = fixed->chars();
        }
    }

    JSAutoByteString bytesStorage;
    const char *bytes = NULL;
    if (str)
        bytes = bytesStorage.encode(cx, str);
    if (!bytes)
        bytes = "unknown (can't convert to string)";

    if (!reportp) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_UNCAUGHT_EXCEPTION, bytes);
    } else {
        /* Flag the error as an exception. */
        reportp->flags |= JSREPORT_EXCEPTION;

        /* Pass the exception object. */
        JS_SetPendingException(cx, exn);
        js_ReportErrorAgain(cx, bytes, reportp);
        JS_ClearPendingException(cx);
    }

    return true;
}

extern JSObject *
js_CopyErrorObject(JSContext *cx, JSObject *errobj, JSObject *scope)
{
    assertSameCompartment(cx, scope);
    JSExnPrivate *priv = GetExnPrivate(errobj);

    uint32_t stackDepth = priv->stackDepth;
    size_t valueCount = 0;
    for (uint32_t i = 0; i < stackDepth; i++)
        valueCount += priv->stackElems[i].argc;

    size_t size = offsetof(JSExnPrivate, stackElems) +
                  stackDepth * sizeof(JSStackTraceElem) +
                  valueCount * sizeof(jsval);

    JSExnPrivate *copy = (JSExnPrivate *)cx->malloc_(size);
    if (!copy)
        return NULL;

    struct AutoFree {
        JSContext *cx;
        JSExnPrivate *p;
        ~AutoFree() {
            if (p) {
                cx->free_(p->errorReport);
                cx->free_(p);
            }
        }
    } autoFree = {cx, copy};

    // Copy each field. Don't bother copying the stack elements.
    if (priv->errorReport) {
        copy->errorReport = CopyErrorReport(cx, priv->errorReport);
        if (!copy->errorReport)
            return NULL;
    } else {
        copy->errorReport = NULL;
    }
    copy->message.init(priv->message);
    if (!cx->compartment->wrap(cx, &copy->message))
        return NULL;
    JS::Anchor<JSString *> messageAnchor(copy->message);
    copy->filename.init(priv->filename);
    if (!cx->compartment->wrap(cx, &copy->filename))
        return NULL;
    JS::Anchor<JSString *> filenameAnchor(copy->filename);
    copy->lineno = priv->lineno;
    copy->stackDepth = 0;
    copy->exnType = priv->exnType;

    // Create the Error object.
    JSObject *proto = scope->global().getOrCreateCustomErrorPrototype(cx, copy->exnType);
    if (!proto)
        return NULL;
    JSObject *copyobj = NewObjectWithGivenProto(cx, &ErrorClass, proto, NULL);
    SetExnPrivate(cx, copyobj, copy);
    autoFree.p = NULL;
    return copyobj;
}
