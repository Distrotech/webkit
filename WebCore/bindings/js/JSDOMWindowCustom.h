/*
 *  Copyright (C) 2008 Apple Inc. All rights reseved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef JSDOMWindowCustom_h
#define JSDOMWindowCustom_h

#include "JSDOMWindow.h"

namespace WebCore {

inline JSDOMWindow* asJSDOMWindow(KJS::JSGlobalObject* globalObject)
{
    return static_cast<JSDOMWindow*>(globalObject);
}
 
inline const JSDOMWindow* asJSDOMWindow(const KJS::JSGlobalObject* globalObject)
{
    return static_cast<const JSDOMWindow*>(globalObject);
}

inline bool JSDOMWindow::customGetOwnPropertySlot(KJS::ExecState* exec, const KJS::Identifier& propertyName, KJS::PropertySlot& slot)
{
    // When accessing a Window cross-domain, functions are always the native built-in ones, and they
    // are not affected by properties changed on the Window or anything in its prototype chain.
    // This is consistent with the behavior of Firefox.

    const KJS::HashEntry* entry;

    // We don't want any properties other than "close" and "closed" on a closed window.
    if (!impl()->frame()) {
        // The following code is safe for cross-domain and same domain use.
        // It ignores any custom properties that might be set on the DOMWindow (including a custom prototype).
        entry = s_info.propHashTable(exec)->entry(propertyName);
        if (entry && !(entry->attributes & KJS::Function) && entry->integerValue == ClosedAttrNum) {
            slot.setStaticEntry(this, entry, KJS::staticValueGetter<JSDOMWindow>);
            return true;
        }
        entry = JSDOMWindowPrototype::s_info.propHashTable(exec)->entry(propertyName);
        if (entry && (entry->attributes & KJS::Function) && entry->functionValue == jsDOMWindowPrototypeFunctionClose) {
            slot.setStaticEntry(this, entry, nonCachingStaticFunctionGetter);
            return true;
        }

        // FIXME: We should have a message here that explains why the property access/function call was
        // not allowed. 
        slot.setUndefined(this);
        return true;
    }

    // We need to check for cross-domain access here without printing the generic warning message
    // because we always allow access to some function, just different ones depending whether access
    // is allowed.
    bool allowsAccess = allowsAccessFromNoErrorMessage(exec);

    // Look for overrides before looking at any of our own properties.
    if (JSGlobalObject::getOwnPropertySlot(exec, propertyName, slot)) {
        // But ignore overrides completely if this is cross-domain access.
        if (allowsAccess)
            return true;
    }

    // We need this code here because otherwise KJS::Window will stop the search before we even get to the
    // prototype due to the blanket same origin (allowsAccessFrom) check at the end of getOwnPropertySlot.
    // Also, it's important to get the implementation straight out of the DOMWindow prototype regardless of
    // what prototype is actually set on this object.
    entry = JSDOMWindowPrototype::s_info.propHashTable(exec)->entry(propertyName);
    if (entry) {
        if ((entry->attributes & KJS::Function)
                && (entry->functionValue == jsDOMWindowPrototypeFunctionBlur
                    || entry->functionValue == jsDOMWindowPrototypeFunctionClose
                    || entry->functionValue == jsDOMWindowPrototypeFunctionFocus
                    || entry->functionValue == jsDOMWindowPrototypeFunctionPostMessage)) {
            if (!allowsAccess) {
                slot.setStaticEntry(this, entry, nonCachingStaticFunctionGetter);
                return true;
            }
        }
    } else {
        // Allow access to toString() cross-domain, but always Object.prototype.toString.
        if (propertyName == exec->propertyNames().toString) {
            if (!allowsAccess) {
                slot.setCustom(this, objectToStringFunctionGetter);
                return true;
            }
        }
    }

    return false;
}

inline bool JSDOMWindow::customPut(KJS::ExecState* exec, const KJS::Identifier& propertyName, KJS::JSValue* value)
{
    if (!impl()->frame())
        return true;

    // We have a local override (e.g. "var location"), save time and jump directly to JSGlobalObject.
    KJS::PropertySlot slot;
    if (JSGlobalObject::getOwnPropertySlot(exec, propertyName, slot)) {
        if (allowsAccessFrom(exec))
            JSGlobalObject::put(exec, propertyName, value);
        return true;
    }

    return false;
}



}

#endif // JSDOMWindowCustom_h
