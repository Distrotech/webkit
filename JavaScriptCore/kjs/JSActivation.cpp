/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
 
#include "config.h"
#include "JSActivation.h"

#include "Register.h"

namespace KJS {

const ClassInfo JSActivation::info = { "JSActivation", 0, 0, 0 };

JSActivation::JSActivation(PassRefPtr<FunctionBodyNode> functionBody, Vector<Register>* registers, int rOffset)
    : JSVariableObject(new JSActivationData(functionBody, registers, rOffset))
{
}

JSActivation::~JSActivation()
{
    if(d()->didCopyRegisters)
        delete d()->registers;
    delete d();
}

void JSActivation::copyRegisters()
{
    size_t numRegisters = symbolTable().size();
    Vector<Register>* registers = new Vector<Register>(numRegisters);
    
    Register* end = d()->registers->begin() + d()->rOffset; 
    Register* src = end - numRegisters;
    Register* dst = registers->data();
    while (src != end)
        *dst++ = *src++;
        
    d()->registers = registers;
    d()->rOffset = registers->size();
    d()->didCopyRegisters = true;
}

bool JSActivation::getOwnPropertySlot(ExecState*, const Identifier& propertyName, PropertySlot& slot)
{
    int index = symbolTable().get(propertyName.ustring().rep());
    if (index != missingSymbolMarker()) {
        slot.setValueSlot(this, &registers()[d()->rOffset + index].u.jsValue);
        return true;
    }
    
    // FIXME: Implement "arguments" property.
    ASSERT(propertyName != CommonIdentifiers::shared()->arguments);

    if (JSValue** location = getDirectLocation(propertyName)) {
        slot.setValueSlot(this, location);
        return true;
    }

    // We don't call through to JSObject because there's no way to give an 
    // activation object getter properties or a prototype.
    ASSERT(!_prop.hasGetterSetterProperties());
    ASSERT(prototype() == jsNull());
    return false;
}

void JSActivation::put(ExecState*, const Identifier& propertyName, JSValue* value)
{
    int index = symbolTable().get(propertyName.ustring().rep());
    if (index != missingSymbolMarker()) {
        registers()[d()->rOffset + index].u.jsValue = value;
        return;
    }

    // We don't call through to JSObject because __proto__ and getter/setter 
    // properties are non-standard extensions that other implementations do not
    // expose in the activation object.
    ASSERT(!_prop.hasGetterSetterProperties());
    _prop.put(propertyName, value, 0, true);
}

// FIXME: Make this function honor ReadOnly (const) and DontEnum
void JSActivation::initializeVariable(ExecState* exec, const Identifier& propertyName, JSValue* value, unsigned)
{
    put(exec, propertyName, value);
}

bool JSActivation::deleteProperty(ExecState* exec, const Identifier& propertyName)
{
    if (propertyName == exec->propertyNames().arguments)
        return false;

    return Base::deleteProperty(exec, propertyName);
}

void JSActivation::mark()
{
    Base::mark();
    
    // No need to mark our values if they're still in the regsiter file, since
    // the register file gets marked independently.
    if(!d()->didCopyRegisters)
        return;
    
    Vector<Register>::iterator end = d()->registers->end();
    for (Vector<Register>::iterator it = d()->registers->begin(); it != end; ++it) {
        JSValue* v = it->u.jsValue;
        if (!v->marked())
            v->mark();
    }
}

bool JSActivation::isActivationObject() const
{
    return true;
}

bool JSActivation::isDynamicScope() const
{
    return d()->functionBody->usesEval();
}

} // namespace KJS
