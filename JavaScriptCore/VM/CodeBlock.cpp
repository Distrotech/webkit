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

static void printUnaryOp(int location, Vector<Instruction>::iterator& it, const char* op)
{
    int r0 = (++it)->u.operand;
    int r1 = (++it)->u.operand;
    printf("[%4d] %s\t\tr[%d], r[%d]\n", location, op, r0, r1);
}

static void printBinaryOp(int location, Vector<Instruction>::iterator& it, const char* op)
{
    int r0 = (++it)->u.operand;
    int r1 = (++it)->u.operand;
    int r2 = (++it)->u.operand;
    printf("[%4d] %s\t\tr[%d], r[%d], r[%d]\n", location, op, r0, r1, r2);
}

void CodeBlock::dump(ExecState* exec)
{
    Vector<Instruction>::iterator begin = instructions.begin();
    Vector<Instruction>::iterator end = instructions.end();

    size_t instructionCount = 0;
    for (Vector<Instruction>::iterator it = begin; it != end; ++it)
        if (machine().isOpcode(it->u.opcode))
            ++instructionCount;

    printf("%lu instructions; %lu bytes at %p; %d parameters; %d locals; %d temporaries\n\n", instructionCount, instructions.size() * sizeof(Instruction), this, numParameters, numLocals, numTemporaries);
    
    for (Vector<Instruction>::iterator it = begin; it != end; ++it)
        dump(exec, begin, it);
}

void CodeBlock::dump(ExecState* exec, const Vector<Instruction>::iterator& begin, Vector<Instruction>::iterator& it)
{
    int location = it - begin;
    switch(machine().getOpcodeID(it->u.opcode)) {
        case op_load: {
            int r0 = (++it)->u.operand;
            int k0 = (++it)->u.operand;
            printf("[%4d] load\t\tr[%d], k[%d]\t\t; %s\n", location, r0, k0, jsValues[k0]->toString(exec).ascii());
            break;
        }
        case op_mov: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            printf("[%4d] mov\t\tr[%d], r[%d]\n", location, r0, r1);
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
            printf("[%4d] pre_inc\t\tr[%d]\n", location, r0);
            break;
        }
        case op_post_inc: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            printf("[%4d] post_inc\t\tr[%d], r[%d]\n", location, r0, r1);
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
            int k0 = (++it)->u.operand;
            printf("[%4d] resolve\t\tr[%d], k[%d]\n", location, r0, k0);
            break;
        }
        case op_resolve_base: {
            int r0 = (++it)->u.operand;
            int k0 = (++it)->u.operand;
            printf("[%4d] resolve_base\tr[%d], k[%d]\n", location, r0, k0);
            break;
        }
        case op_object_get: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int k0 = (++it)->u.operand;
            printf("[%4d] object_get\tr[%d], r[%d], k[%d]\n", location, r0, r1, k0);
            break;
        }
        case op_object_put: {
            int r0 = (++it)->u.operand;
            int k0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            printf("[%4d] object_put\tr[%d], k[%d], r[%d]\n", location, r0, k0, r1);
            break;
        }
        case op_jmp: {
            int offset = (++it)->u.operand;
            printf("[%4d] jmp\t\t%d\t\t\t; %d\n", location, offset, (it - begin) + offset);
            break;
        }
        case op_jtrue: {
            int r0 = (++it)->u.operand;
            int offset = (++it)->u.operand;
            printf("[%4d] jtrue\t\tr[%d], %d\t\t\t; %d\n", location, r0, offset, (it - begin) + offset);
            break;
        }
        case op_jfalse: {
            int r0 = (++it)->u.operand;
            int offset = (++it)->u.operand;
            printf("[%4d] jfalse\t\tr[%d], %d\t\t\t; %d\n", location, r0, offset, (it - begin) + offset);
            break;
        }
        case op_new_func: {
            int r0 = (++it)->u.operand;
            int k0 = (++it)->u.operand;
            printf("[%4d] new_func\t\tr[%d], k[%d]\n", location, r0, k0);
            break;
        }
        case op_call: {
            int r0 = (++it)->u.operand;
            int r1 = (++it)->u.operand;
            int tempCount = (++it)->u.operand;
            int argCount = (++it)->u.operand;
            printf("[%4d] call\t\tr[%d], r[%d], %d, %d\n", location, r0, r1, tempCount, argCount);
            break;
        }
        case op_ret: {
            int r0 = (++it)->u.operand;
            printf("[%4d] ret\t\tr[%d]\n", location, r0);
            break;
        }
        case op_push_scope: {
            int r0 = (++it)->u.operand;
            printf("[%4d] push_scope\tr[%d]\n", location, r0);
            break;
        }
        case op_pop_scope: {
            printf("[%4d] pop_scope\n", location);
            break;
        }
        case op_end: {
            int r0 = (++it)->u.operand;
            printf("[%4d] end\t\tr[%d]\n", location, r0);
            break;
        }
        default: {
            ASSERT_NOT_REACHED();
            break;
        }
    }
}

} // namespace KJS
