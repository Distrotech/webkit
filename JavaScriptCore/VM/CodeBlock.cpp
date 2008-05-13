/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Cameron Zwarich <cwzwarich@uwaterloo.ca>
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
#include "CodeBlock.h"

#include "Machine.h"
#include "value.h"

namespace KJS {

static UString escapeQuotes(const UString& str)
{
    UString result = str;
    int pos = 0;
    while ((pos = result.find('\"', pos)) >= 0) {
        result = result.substr(0, pos) + "\"\\\"\"" + result.substr(pos + 1);
        pos += 4;
    }
    return result;
}

static UString valueToSourceString(ExecState* exec, JSValue* val) 
{
    if (val->isString()) {
        UString result("\"");
        result += escapeQuotes(val->toString(exec)) + "\"";
        return result;
    } 

    return val->toString(exec);
}

static CString registerName(int r)
{
    if (r < 0)
        return (UString("lr") + UString::from(-r)).UTF8String(); 

    return (UString("tr") + UString::from(r)).UTF8String();
}

static CString constantName(ExecState* exec, int k, JSValue* value)
{
    return (valueToSourceString(exec, value) + "(@k" + UString::from(k) + ")").UTF8String();
}

static CString idName(int id0, const Identifier& ident)
{
    return (ident.ustring() + "(@id" + UString::from(id0) +")").UTF8String();
}

static int jumpTarget(const Vector<Instruction>::iterator& begin, Vector<Instruction>::iterator& it, int offset)
{
    return it - begin + offset;
}

static void printUnaryOp(int location, Vector<Instruction>::iterator& it, const char* op)
{
    int r0 = (++it)->u.operand;
    int r1 = (++it)->u.operand;

    printf("[%4d] %s\t\t%s, %s\n", location, op, registerName(r0).c_str(), registerName(r1).c_str());
}

static void printBinaryOp(int location, Vector<Instruction>::iterator& it, const char* op)
{
    int r0 = (++it)->u.operand;
    int r1 = (++it)->u.operand;
    int r2 = (++it)->u.operand;
    printf("[%4d] %s\t\t%s, %s, %s\n", location, op, registerName(r0).c_str(), registerName(r1).c_str(), registerName(r2).c_str());
}

static void printConditionalJump(const Vector<Instruction>::iterator& begin, Vector<Instruction>::iterator& it, int location, const char* op)
{
    int r0 = (++it)->u.operand;
    int offset = (++it)->u.operand;
    printf("[%4d] %s\t\t%s, %d(->%d)\n", location, op, registerName(r0).c_str(), offset, jumpTarget(begin, it, offset));
}

void CodeBlock::dump(ExecState* exec)
{
    Vector<Instruction>::iterator begin = instructions.begin();
    Vector<Instruction>::iterator end = instructions.end();

    size_t instructionCount = 0;
    for (Vector<Instruction>::iterator it = begin; it != end; ++it)
        if (machine().isOpcode(it->u.opcode))
            ++instructionCount;

    printf("%lu instructions; %lu bytes at %p; %d locals (%d parameters); %d temporaries\n\n", instructionCount, instructions.size() * sizeof(Instruction), this, numParameters + numVars, numParameters, numTemporaries);
    
    for (Vector<Instruction>::iterator it = begin; it != end; ++it)
        dump(exec, begin, it);

    printf("\nIdentifiers:\n");
    
    for (size_t i = 0; i < identifiers.size(); ++i)
        printf("  id%u = %s\n", static_cast<unsigned>(i), identifiers[i].ascii());

    printf("\nConstants:\n");
    for (size_t i = 0; i < jsValues.size(); ++i)
        printf("  k%u = %s\n", static_cast<unsigned>(i), valueToSourceString(exec, jsValues[i]).ascii());
}

void CodeBlock::dump(ExecState* exec, const Vector<Instruction>::iterator& begin, Vector<Instruction>::iterator& it)
{
    int location = it - begin;
    switch(machine().getOpcodeID(it->u.opcode)) {
        case op_load: {
            int r0 = (++it)->u.operand;
            int k0 = (++it)->u.operand;
            printf("[%4d] load\t\t%s, %s\t\t\n", location, registerName(r0).c_str(), constantName(exec, k0, jsValues[k0]).c_str());
            break;
        }
        case op_new_object: {
            int r0 = (++it)->u.operand;
            printf("[%4d] new_object\t%s\n", location, registerName(r0).c_str());
            break;
        }
        case op_new_array: {
            int r0 = (++it)->u.operand;
            printf("[%4d] new_array\t%s\n", location, registerName(r0).c_str());
            break;
        }
        case op_mov: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            printf("[%4d] mov\t\t%s, %s\n", location, registerName(r0).c_str(), registerName(r1).c_str());
            break;
        }
        case op_equal: {
            printBinaryOp(location, it, "equal");
            break;
        }
        case op_nequal: {
            printBinaryOp(location, it, "nequal");
            break;
        }
        case op_stricteq: {
            printBinaryOp(location, it, "stricteq");
            break;
        }
        case op_nstricteq: {
            printBinaryOp(location, it, "nstricteq");
            break;
        }
        case op_less: {
            printBinaryOp(location, it, "less");
            break;
        }
        case op_lesseq: {
            printBinaryOp(location, it, "lesseq");
            break;
        }
        case op_pre_inc: {
            int r0 = (++it)->u.operand;
            printf("[%4d] pre_inc\t\t%s\n", location, registerName(r0).c_str());
            break;
        }
        case op_post_inc: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            printf("[%4d] post_inc\t\t%s, %s\n", location, registerName(r0).c_str(), registerName(r1).c_str());
            break;
        }
        case op_to_jsnumber: {
            printUnaryOp(location, it, "to_jsnumber");
            break;
        }
        case op_negate: {
            printUnaryOp(location, it, "negate");
            break;
        }
        case op_add: {
            printBinaryOp(location, it, "add");
            break;
        }
        case op_mult: {
            printBinaryOp(location, it, "mult");
            break;
        }
        case op_div: {
            printBinaryOp(location, it, "div");
            break;
        }
        case op_mod: {
            printBinaryOp(location, it, "mod");
            break;
        }
        case op_sub: {
            printBinaryOp(location, it, "sub");
            break;
        }
        case op_lshift: {
            printBinaryOp(location, it, "lshift");
            break;            
        }
        case op_rshift: {
            printBinaryOp(location, it, "rshift");
            break;
        }
        case op_urshift: {
            printBinaryOp(location, it, "urshift");
            break;
        }
        case op_bitand: {
            printBinaryOp(location, it, "bitand");
            break;
        }
        case op_bitxor: {
            printBinaryOp(location, it, "bitxor");
            break;
        }
        case op_bitor: {
            printBinaryOp(location, it, "bitor");
            break;
        }
         case op_bitnot: {
             printUnaryOp(location, it, "bitnot");
             break;
         }
         case op_not: {
             printUnaryOp(location, it, "not");
             break;
         }
        case op_instance_of: {
            printBinaryOp(location, it, "instance_of");
            break;
        }
        case op_resolve: {
            int r0 = (++it)->u.operand;
            int id0 = (++it)->u.operand;
            printf("[%4d] resolve\t\t%s, %s\n", location, registerName(r0).c_str(), idName(id0, identifiers[id0]).c_str());
            break;
        }
        case op_resolve_base: {
            int r0 = (++it)->u.operand;
            int id0 = (++it)->u.operand;
            printf("[%4d] resolve_base\t%s, %s\n", location, registerName(r0).c_str(), idName(id0, identifiers[id0]).c_str());
            break;
        }
        case op_get_prop_id: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int id0 = (++it)->u.operand;
            printf("[%4d] get_prop_id\t%s, %s, %s\n", location, registerName(r0).c_str(), registerName(r1).c_str(), idName(id0, identifiers[id0]).c_str());
            break;
        }
        case op_put_prop_id: {
            int r0 = (++it)->u.operand;
            int id0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            printf("[%4d] put_prop_id\t%s, %s, %s\n", location, registerName(r0).c_str(), idName(id0, identifiers[id0]).c_str(), registerName(r1).c_str());
            break;
        }
        case op_get_prop_val: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int r2 = (++it)->u.operand;
            printf("[%4d] get_prop_val\t%s, %s, %s\n", location, registerName(r0).c_str(), registerName(r1).c_str(), registerName(r2).c_str());
            break;
        }
        case op_put_prop_val: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int r2 = (++it)->u.operand;
            printf("[%4d] put_prop_val\t%s, %s, %s\n", location, registerName(r0).c_str(), registerName(r1).c_str(), registerName(r2).c_str());
            break;
        }
        case op_put_prop_index: {
            int r0 = (++it)->u.operand;
            unsigned n0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            printf("[%4d] put_prop_index\t%s, %u, %s\n", location, registerName(r0).c_str(), n0, registerName(r1).c_str());
            break;
        }
        case op_jmp: {
            int offset = (++it)->u.operand;
            printf("[%4d] jmp\t\t%d(->%d)\n", location, offset, jumpTarget(begin, it, offset));
            break;
        }
        case op_jtrue: {
            printConditionalJump(begin, it, location, "jtrue");
            break;
        }
        case op_jfalse: {
            printConditionalJump(begin, it, location, "jfalse");
            break;
        }
        case op_new_func: {
            int r0 = (++it)->u.operand;
            int f0 = (++it)->u.operand;
            printf("[%4d] new_func\t\t%s, f%d\n", location, registerName(r0).c_str(), f0);
            break;
        }
        case op_call: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int r2 = (++it)->u.operand;
            int tempCount = (++it)->u.operand;
            int argCount = (++it)->u.operand;
            printf("[%4d] call\t\t%s, %s, %s, %d, %d\n", location, registerName(r0).c_str(), registerName(r1).c_str(), registerName(r2).c_str(), tempCount, argCount);
            break;
        }
        case op_ret: {
            int r0 = (++it)->u.operand;
            printf("[%4d] ret\t\t%s\n", location, registerName(r0).c_str());
            break;
        }
        case op_push_scope: {
            int r0 = (++it)->u.operand;
            printf("[%4d] push_scope\t%s\n", location, registerName(r0).c_str());
            break;
        }
        case op_pop_scope: {
            printf("[%4d] pop_scope\n", location);
            break;
        }
        case op_jmp_scopes: {
            int scopeDelta = (++it)->u.operand;
            int offset = (++it)->u.operand;
            printf("[%4d] jmp_scopes\t^%d, %d(->%d)\n", location, scopeDelta, offset, jumpTarget(begin, it, offset));
            break;
        }
        case op_end: {
            int r0 = (++it)->u.operand;
            printf("[%4d] end\t\t%s\n", location, registerName(r0).c_str());
            break;
        }
        default: {
            ASSERT_NOT_REACHED();
            break;
        }
    }
}

} // namespace KJS
