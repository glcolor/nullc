#pragma once

#include "stdafx.h"

#include "Instruction_X86.h"
#include "ParseClass.h"
#include "Bytecode.h"

char* InlFmt(const char *str, ...);

void EMIT_LABEL(const char* Label);
void EMIT_OP(x86Command op);
void EMIT_OP_LABEL(x86Command op, const char* Label);
void EMIT_OP_REG(x86Command op, x86Reg reg1);
void EMIT_OP_FPUREG(x86Command op, x87Reg reg1);
void EMIT_OP_NUM(x86Command op, unsigned int num);
void EMIT_OP_ADDR(x86Command op, x86Size size, unsigned int addr);
void EMIT_OP_RPTR(x86Command op, x86Size size, x86Reg reg2, unsigned int shift);
void EMIT_OP_REG_NUM(x86Command op, x86Reg reg1, unsigned int num);
void EMIT_OP_REG_REG(x86Command op, x86Reg reg1, x86Reg reg2);
void EMIT_OP_REG_ADDR(x86Command op, x86Reg reg1, x86Size size, unsigned int addr);
void EMIT_OP_REG_RPTR(x86Command op, x86Reg reg1, x86Size size, x86Reg reg2, unsigned int shift);
void EMIT_OP_REG_LABEL(x86Command op, x86Reg reg1, const char* Label, unsigned int shift);
void EMIT_OP_ADDR_REG(x86Command op, x86Size size, unsigned int addr, x86Reg reg2);
void EMIT_OP_RPTR_REG(x86Command op, x86Size size, x86Reg reg1, unsigned int shift, x86Reg reg2);
void EMIT_OP_RPTR_NUM(x86Command op, x86Size size, x86Reg reg1, unsigned int shift, unsigned int num);

void ResetStackTracking();
unsigned int GetStackTrackInfo();

void SetParamBase(unsigned int base);
void SetInstructionList(FastVector<x86Instruction, true, true> *instList);

void GenCodeCmdNop(VMCmd cmd);

void GenCodeCmdPushCharAbs(VMCmd cmd);
void GenCodeCmdPushShortAbs(VMCmd cmd);
void GenCodeCmdPushIntAbs(VMCmd cmd);
void GenCodeCmdPushFloatAbs(VMCmd cmd);
void GenCodeCmdPushDorLAbs(VMCmd cmd);
void GenCodeCmdPushCmplxAbs(VMCmd cmd);

void GenCodeCmdPushCharRel(VMCmd cmd);
void GenCodeCmdPushShortRel(VMCmd cmd);
void GenCodeCmdPushIntRel(VMCmd cmd);
void GenCodeCmdPushFloatRel(VMCmd cmd);
void GenCodeCmdPushDorLRel(VMCmd cmd);
void GenCodeCmdPushCmplxRel(VMCmd cmd);

void GenCodeCmdPushCharStk(VMCmd cmd);
void GenCodeCmdPushShortStk(VMCmd cmd);
void GenCodeCmdPushIntStk(VMCmd cmd);
void GenCodeCmdPushFloatStk(VMCmd cmd);
void GenCodeCmdPushDorLStk(VMCmd cmd);
void GenCodeCmdPushCmplxStk(VMCmd cmd);

void GenCodeCmdPushImmt(VMCmd cmd);

void GenCodeCmdMovCharAbs(VMCmd cmd);
void GenCodeCmdMovShortAbs(VMCmd cmd);
void GenCodeCmdMovIntAbs(VMCmd cmd);
void GenCodeCmdMovFloatAbs(VMCmd cmd);
void GenCodeCmdMovDorLAbs(VMCmd cmd);
void GenCodeCmdMovCmplxAbs(VMCmd cmd);

void GenCodeCmdMovCharRel(VMCmd cmd);
void GenCodeCmdMovShortRel(VMCmd cmd);
void GenCodeCmdMovIntRel(VMCmd cmd);
void GenCodeCmdMovFloatRel(VMCmd cmd);
void GenCodeCmdMovDorLRel(VMCmd cmd);
void GenCodeCmdMovCmplxRel(VMCmd cmd);

void GenCodeCmdMovCharStk(VMCmd cmd);
void GenCodeCmdMovShortStk(VMCmd cmd);
void GenCodeCmdMovIntStk(VMCmd cmd);
void GenCodeCmdMovFloatStk(VMCmd cmd);
void GenCodeCmdMovDorLStk(VMCmd cmd);
void GenCodeCmdMovCmplxStk(VMCmd cmd);

void GenCodeCmdReserveV(VMCmd cmd);

void GenCodeCmdPopCharTop(VMCmd cmd);
void GenCodeCmdPopShortTop(VMCmd cmd);
void GenCodeCmdPopIntTop(VMCmd cmd);
void GenCodeCmdPopFloatTop(VMCmd cmd);
void GenCodeCmdPopDorLTop(VMCmd cmd);
void GenCodeCmdPopCmplxTop(VMCmd cmd);
void GenCodeCmdPop(VMCmd cmd);

void GenCodeCmdDtoI(VMCmd cmd);
void GenCodeCmdDtoL(VMCmd cmd);
void GenCodeCmdDtoF(VMCmd cmd);
void GenCodeCmdItoD(VMCmd cmd);
void GenCodeCmdLtoD(VMCmd cmd);
void GenCodeCmdItoL(VMCmd cmd);
void GenCodeCmdLtoI(VMCmd cmd);

void GenCodeCmdImmtMul(VMCmd cmd);

void GenCodeCmdCopyDorL(VMCmd cmd);
void GenCodeCmdCopyI(VMCmd cmd);

void GenCodeCmdGetAddr(VMCmd cmd);
void GenCodeCmdSetRange(VMCmd cmd);

void GenCodeCmdJmp(VMCmd cmd);
void GenCodeCmdJmpZI(VMCmd cmd);
void GenCodeCmdJmpZD(VMCmd cmd);
void GenCodeCmdJmpZL(VMCmd cmd);
void GenCodeCmdJmpNZI(VMCmd cmd);
void GenCodeCmdJmpNZD(VMCmd cmd);
void GenCodeCmdJmpNZL(VMCmd cmd);

void GenCodeCmdCall(VMCmd cmd);
void GenCodeCmdReturn(VMCmd cmd);

void GenCodeCmdPushVTop(VMCmd cmd);
void GenCodeCmdPopVTop(VMCmd cmd);

void GenCodeCmdPushV(VMCmd cmd);

void GenCodeCmdAdd(VMCmd cmd);
void GenCodeCmdSub(VMCmd cmd);
void GenCodeCmdMul(VMCmd cmd);
void GenCodeCmdDiv(VMCmd cmd);
void GenCodeCmdPow(VMCmd cmd);
void GenCodeCmdMod(VMCmd cmd);
void GenCodeCmdLess(VMCmd cmd);
void GenCodeCmdGreater(VMCmd cmd);
void GenCodeCmdLEqual(VMCmd cmd);
void GenCodeCmdGEqual(VMCmd cmd);
void GenCodeCmdEqual(VMCmd cmd);
void GenCodeCmdNEqual(VMCmd cmd);
void GenCodeCmdShl(VMCmd cmd);
void GenCodeCmdShr(VMCmd cmd);
void GenCodeCmdBitAnd(VMCmd cmd);
void GenCodeCmdBitOr(VMCmd cmd);
void GenCodeCmdBitXor(VMCmd cmd);
void GenCodeCmdLogAnd(VMCmd cmd);
void GenCodeCmdLogOr(VMCmd cmd);
void GenCodeCmdLogXor(VMCmd cmd);

void GenCodeCmdAddL(VMCmd cmd);
void GenCodeCmdSubL(VMCmd cmd);
void GenCodeCmdMulL(VMCmd cmd);
void GenCodeCmdDivL(VMCmd cmd);
void GenCodeCmdPowL(VMCmd cmd);
void GenCodeCmdModL(VMCmd cmd);
void GenCodeCmdLessL(VMCmd cmd);
void GenCodeCmdGreaterL(VMCmd cmd);
void GenCodeCmdLEqualL(VMCmd cmd);
void GenCodeCmdGEqualL(VMCmd cmd);
void GenCodeCmdEqualL(VMCmd cmd);
void GenCodeCmdNEqualL(VMCmd cmd);
void GenCodeCmdShlL(VMCmd cmd);
void GenCodeCmdShrL(VMCmd cmd);
void GenCodeCmdBitAndL(VMCmd cmd);
void GenCodeCmdBitOrL(VMCmd cmd);
void GenCodeCmdBitXorL(VMCmd cmd);
void GenCodeCmdLogAndL(VMCmd cmd);
void GenCodeCmdLogOrL(VMCmd cmd);
void GenCodeCmdLogXorL(VMCmd cmd);

void GenCodeCmdAddD(VMCmd cmd);
void GenCodeCmdSubD(VMCmd cmd);
void GenCodeCmdMulD(VMCmd cmd);
void GenCodeCmdDivD(VMCmd cmd);
void GenCodeCmdPowD(VMCmd cmd);
void GenCodeCmdModD(VMCmd cmd);
void GenCodeCmdLessD(VMCmd cmd);
void GenCodeCmdGreaterD(VMCmd cmd);
void GenCodeCmdLEqualD(VMCmd cmd);
void GenCodeCmdGEqualD(VMCmd cmd);
void GenCodeCmdEqualD(VMCmd cmd);
void GenCodeCmdNEqualD(VMCmd cmd);

void GenCodeCmdNeg(VMCmd cmd);
void GenCodeCmdBitNot(VMCmd cmd);
void GenCodeCmdLogNot(VMCmd cmd);

void GenCodeCmdNegL(VMCmd cmd);
void GenCodeCmdBitNotL(VMCmd cmd);
void GenCodeCmdLogNotL(VMCmd cmd);

void GenCodeCmdNegD(VMCmd cmd);
void GenCodeCmdLogNotD(VMCmd cmd);

void GenCodeCmdIncI(VMCmd cmd);
void GenCodeCmdIncD(VMCmd cmd);
void GenCodeCmdIncL(VMCmd cmd);

void GenCodeCmdDecI(VMCmd cmd);
void GenCodeCmdDecD(VMCmd cmd);
void GenCodeCmdDecL(VMCmd cmd);

void GenCodeCmdAddAtCharStk(VMCmd cmd);
void GenCodeCmdAddAtShortStk(VMCmd cmd);
void GenCodeCmdAddAtIntStk(VMCmd cmd);
void GenCodeCmdAddAtLongStk(VMCmd cmd);
void GenCodeCmdAddAtFloatStk(VMCmd cmd);
void GenCodeCmdAddAtDoubleStk(VMCmd cmd);
