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
 
#ifndef JSActivation_h
#define JSActivation_h

#include "object.h"
#include "nodes.h"
#include "SymbolTable.h"

namespace KJS {

    class Register;
    
    // This class will eventually inherit from JSVariableObject and replace
    // ActivationImp. That's why, for example, it accesses all its data members
    // through d().

    class JSActivation : public JSObject {
    typedef JSObject Base;
    public:
        JSActivation(PassRefPtr<FunctionBodyNode>, Vector<Register>*, int rOffset);
        virtual ~JSActivation();
        
        virtual bool isActivationObject() const { return true; }

        virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);

        virtual void put(ExecState*, const Identifier&, JSValue*);
        virtual void initializeVariable(ExecState*, const Identifier&, JSValue*, unsigned attributes);
        virtual bool deleteProperty(ExecState*, const Identifier& propertyName);

        virtual void mark();
        
        void copyRegisters();
        
        virtual const ClassInfo* classInfo() const { return &info; }
        static const ClassInfo info;
        
    private:
        struct JSActivationData {
            JSActivationData(PassRefPtr<FunctionBodyNode> functionBody_, Vector<Register>* registers_, int rOffset_)
                : didCopyRegisters(false)
                , functionBody(functionBody_)
                , registers(registers_)
                , rOffset(rOffset_)
                , symbolTable(&functionBody->symbolTable())
            {
            }

            bool didCopyRegisters;

            RefPtr<FunctionBodyNode> functionBody; // Owns the symbol table

            // Will be inherited from JSVariableObjectData
            Vector<Register>* registers;
            int rOffset;
            SymbolTable* symbolTable; // Maps name -> index in register file.
        };
        
        SymbolTable& symbolTable() { return *d()->symbolTable; }
        Vector<Register>& registers() { return *d()->registers; }
        JSActivationData* d() const { return static_cast<JSActivationData*>(m_data); }

        JSActivationData* m_data;
    };
    
} // namespace KJS

#endif // JSActivation_h
