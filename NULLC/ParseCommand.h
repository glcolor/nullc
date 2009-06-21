#pragma once
#include "stdafx.h"

typedef short int CmdID;

// Command definition file. For extended description, check out CmdRef.txt
// ���� ����������� �������� ������. ��� ���������� �������� �� ������ �������� CmdRef.txt

// ����������� ������ ����� ��������� ������ ��� ���������� ���������� ����������:
// 1) �������� ����. ���� ���������� �����, ������ � ������������ �������� ��� ����.
// 1a) ���� ���� ���������� �������������� ���� ���������� � �������� �����. ������������ ��� �������,
//		� ����� ��� ����������� ���� ���������� ��� ������ ���������� �������.
// 2) ���� ����������. ���� ���������� �������� �������� ����������.
// 3) ���� ��� �������� ���������� ���������� � ����� ����������. ������������ ��� �������� ���������� ���
//		������ �� ������� ���������.
// 4) ���� �������. ��������� ��������� �� ���. ������������ ��� �������� �� �������.

// Special functions
// ������ ������� [����� �� ������������]
	// no operation
	// :)
const CmdID cmdNop		= 0;
	// converts number on top of the stack to integer and multiples it by some number [int after instruction]
	// ������������ ����� �� ������� ����� � int � �������� �� ��������� ����� [int �� �����������]
const CmdID cmdCTI		= 103;
	// get variable address, shifted by the parameter stack base
	// �������� ����� ����������, ������������ ���� ����� ����������
const CmdID cmdGetAddr	= 201;
	// get function address
	// �������� ����� �������
const CmdID cmdFuncAddr	= 202;

// general commands [using command flag. check CmdRef.txt]
// �������� ������� [������������ ���� �������, �������� CmdRef.txt]
	// pushes a number on top of general stack
	// �������� �������� �� �������� �����
const CmdID cmdPush		= 100;

const CmdID cmdMovRTaP	= 98;	// cmdMov + (relative to top and pop)
const CmdID cmdPushImmt	= 99;

	// removes a number from top
	// ������ �������� � �������� �����
const CmdID cmdPop		= 101;
	// copy's number from top of stack to value in value stack
	// ����������� �������� � �������� ����� � ������ ��� ������������� ����������
const CmdID cmdMov		= 102;
	// converts real numbers to integer
	// ������������� �������������� ����� � ����� (�� ������� �����)
const CmdID cmdRTOI		= 104;
	// converts integer numbers to real
	// ������������� ����� ����� � �������������� (�� ������� �����)
const CmdID cmdITOR		= 105;
	// converts integer numbers to long
	// ������������� int � long (�� ������� �����)
const CmdID cmdITOL		= 106;
	// converts long numbers to integer
	// ������������� long � int (�� ������� �����)
const CmdID cmdLTOI		= 107;
	// swaps two values on top of the general stack
	// �������� ������� ��� �������� �� �������� �����
const CmdID cmdSwap		= 108;
	// copy value on top of the stack, and push it on the top again
	// ����������� �������� �� �������� ����� � �������� ��� � ����
const CmdID cmdCopy		= 109;
	// set value to a range of memory. data type are provided, as well as starting address and count
	// ���������� �������� ������� ������. ������� ��� ������, � ����� ��������� ������� � ����������
const CmdID cmdSetRange = 200;

// conditional and unconditional jumps  [using operation flag. check CmdRef.txt]
// �������� � ����������� �������� [������������ ���� ��������, �������� CmdRef.txt]
	// unconditional jump
	// ����������� �������
const CmdID cmdJmp		= 110;
	// jump on zero
	// �������, ���� �������� �� ������� == 0
const CmdID cmdJmpZ		= 111;
	// jump on not zero
	// �������, ���� �������� �� ������� != 0
const CmdID cmdJmpNZ	= 112;

// commands for functions	[not using flags]
// ������� ��� �������	[����� �� ������������]
	// call script function
	// ����� �������, ����������� � �������
const CmdID cmdCall		= 113;
	// call standard function
	// ����� ����������� (����������) �������
const CmdID cmdCallStd	= 114;
//							[using Operation Flag]
	// return from function
	// ������� �� ������� � ���������� POPT n ���, ��� n ��� �� ��������
const CmdID cmdReturn	= 115;

// commands for work with variable stack	[not using flags]
// ������� ��� ������ �� ������ ����������	[����� �� ������������]
	// save active variable count to "top value" stack
	// ��������� ���������� �������� ���������� � ���� ������ ����� ����������
const CmdID cmdPushVTop	= 116;
	// pop data from variable stack until last top position and remove last top position
	// ������ ������ �� ����� ���������� �� ����������� �������� ������� � ������ �������� ������� �� ����� �������� ������
const CmdID cmdPopVTop	= 117;
	// shift value stack top
	// �������� ��������� � ������� ���������� ���� � ���� ����������
const CmdID cmdPushV	= 118;

// binary commands	[using operation flag. check CmdRef.txt]
// they take two top numbers (both the same type!), remove them from stack,
// perform operation and place result on top of stack (the same type as two values!)
// �������� �������� [������������ ���� ��������, �������� CmdRef.txt]
// ��� ����� ��� �������� � ������� ����� ���������� (����������� ����!), ������� �� �� �����
// ���������� �������� � �������� ��������� � ���� ���������� (������-�� ����, ��� �������� ����������)
	// a + b
const CmdID cmdAdd		= 130;
	// a - b
const CmdID cmdSub		= 131;
	// a * b
const CmdID cmdMul		= 132;
	// a / b 
const CmdID cmdDiv		= 133;
	// power(a, b) (**)
const CmdID cmdPow		= 134;
	// a % b
const CmdID cmdMod		= 135;
	// a < b
const CmdID cmdLess		= 136;
	// a > b
const CmdID cmdGreater	= 137;
	// a <= b
const CmdID cmdLEqual	= 138;
	// a >= b
const CmdID cmdGEqual	= 139;
	// a == b
const CmdID cmdEqual	= 140;
	// a != b
const CmdID cmdNEqual	= 141;

// the following commands work only with integer numbers
// ��������� ������� �������� ������ � �������������� �����������
	// a << b
const CmdID cmdShl		= 142;
	// a >> b
const CmdID cmdShr		= 143;
	// a & b	binary AND/�������� �
const CmdID cmdBitAnd	= 144;
	// a | b	binary OR/�������� ���
const CmdID cmdBitOr	= 145;
	// a ^ b	binary XOR/�������� ����������� ���
const CmdID cmdBitXor	= 146;
	// a && b	logical AND/���������� �
const CmdID cmdLogAnd	= 147;
	// a || b	logical OR/���������� ���
const CmdID cmdLogOr	= 148;
	// a logical_xor b	logical XOR/���������� ����������� ���
const CmdID cmdLogXor	= 149;

// unary commands	[using operation flag. check CmdRef.txt]
// they take one value from top of the stack, and replace it with resulting value
// ������� �������� [������������ ���� ��������, �������� CmdRef.txt]
// ��� ������ �������� �� ������� �����
	// negation
	// ���������
const CmdID cmdNeg		= 160;
	// ~	binary NOT | Only for integers! |
	// ~	�������� ��������� | ������ ��� ����� ����� |
const CmdID cmdBitNot	= 163;
	// !	logical NOT
	// !	���������� ��
const CmdID cmdLogNot	= 164;

// Special unary commands	[using command flag. check CmdRef.txt]
// They work with variable at address
// ������ ������� ��������	[������������ ���� �������, �������� CmdRef.txt]
// �������� �������� ����� � ����� ����������
	// increment at address
	// ��������� �������� �� ������
const CmdID cmdIncAt	= 171;
	// decrement at address
	// ��������� �������� �� ������
const CmdID cmdDecAt	= 172;

// Flag types
// ���� ������
typedef unsigned short int CmdFlag;	// command flag
typedef unsigned char OperFlag;		// operation flag
typedef unsigned short int RetFlag; // return flag

// Types of values on stack
// ���� �������� � �����
enum asmStackType
{
	STYPE_INT,
	STYPE_LONG,
	STYPE_COMPLEX_TYPE,
	STYPE_DOUBLE,
	STYPE_FORCE_DWORD = 1<<30,
};
// Types of values on variable stack
// ���� �������� � ����� ����������
enum asmDataType
{
	DTYPE_CHAR=0,
	DTYPE_SHORT=4,
	DTYPE_INT=8,
	DTYPE_LONG=12,
	DTYPE_FLOAT=16,
	DTYPE_DOUBLE=20,
	DTYPE_COMPLEX_TYPE=24,
	DTYPE_FORCE_DWORD = 1<<30,
};
// Type of operation (for operation flag)
// ���� �������� (��� ����� ��������)
enum asmOperType
{
	OTYPE_DOUBLE,
	OTYPE_FLOAT_DEPRECATED,
	OTYPE_LONG,
	OTYPE_INT,
};

static int		stackTypeSize[] = { 4, 8, -1, 8 };
// Conversion of asmStackType to appropriate asmOperType
// �������������� asmStackType � ���������� asmOperType
static asmOperType operTypeForStackType[] = { OTYPE_INT, OTYPE_LONG, (asmOperType)0, OTYPE_DOUBLE };

// Conversion of asmStackType to appropriate asmDataType
// �������������� asmStackType � ���������� asmDataType
static asmDataType dataTypeForStackType[] = { DTYPE_INT, DTYPE_LONG, (asmDataType)0, DTYPE_DOUBLE };

// Conversion of asmDataType to appropriate asmStackType
// �������������� asmDataType � ���������� asmStackType
static asmStackType stackTypeForDataTypeArr[] = { STYPE_INT, STYPE_INT, STYPE_INT, STYPE_LONG, STYPE_DOUBLE, STYPE_DOUBLE, STYPE_COMPLEX_TYPE };
__forceinline asmStackType stackTypeForDataType(asmDataType dt){ return stackTypeForDataTypeArr[dt/4]; }

// Functions for extraction of different bits from flags
// ������� ��� ���������� �������� ��������� ����� �� ������
	// extract asmStackType
	// ������� asmStackType
__forceinline asmStackType	flagStackType(const CmdFlag& flag){ return (asmStackType)(flag&0x00000003); }
	// extract asmDataType
	// ������� asmDataType
__forceinline asmDataType	flagDataType(const CmdFlag& flag){ return (asmDataType)(((flag>>2)&0x00000007)<<2); }
	// addressing is performed relatively to the base of variable stack
	// ��������� ������������ ������������ ���� ����� ����������
__forceinline UINT			flagAddrRel(const CmdFlag& flag){ return (flag>>5)&0x00000001; }
	// addressing is perform using absolute address
	// ��������� ������������ �� ����������� ������
__forceinline UINT			flagAddrAbs(const CmdFlag& flag){ return (flag>>6)&0x00000001; }
	// addressing is performed relatively to the top of variable stack
	// ��������� ������������ ������������ ������� ����� ����������
__forceinline UINT			flagAddrRelTop(const CmdFlag& flag){ return (flag>>7)&0x00000001; }

	// shift is placed in stack
	// ����� ��������� � �������� �����
__forceinline UINT			flagShiftStk(const CmdFlag& flag){ return (flag>>9)&0x00000001; }
	// address is clamped to the maximum
	// ����� �� ����� ��������� ��������� ��������
__forceinline UINT			flagSizeOn(const CmdFlag& flag){ return (flag>>10)&0x00000001; }
	// maximum is placed in stack
	// �������� ��������� � �������� �����
__forceinline UINT			flagSizeStk(const CmdFlag& flag){ return (flag>>11)&0x00000001; }

	// push value on stack before modifying
	// �������� �������� � ���� �� ���������
__forceinline UINT			flagPushBefore(const CmdFlag& flag){ return (flag>>12)&0x00000001; }
	// push value on stack after modifying
	// �������� �������� � ���� ����� ���������
__forceinline UINT			flagPushAfter(const CmdFlag& flag){ return (flag>>13)&0x00000001; }

	// addressing is not performed
	// ��������� �����������
__forceinline UINT			flagNoAddr(const CmdFlag& flag){ return !(flag&0x00000060); }

// constants for CmdFlag creation from different bits
// ��������� ��� �������� ����� �������� �� ��������� �����
const UINT	bitAddrRel	= 1 << 5;
const UINT	bitAddrAbs	= 1 << 6;
const UINT	bitAddrRelTop= 1 << 7;

const UINT	bitShiftStk	= 1 << 9;
const UINT	bitSizeOn	= 1 << 10;
const UINT	bitSizeStk	= 1 << 11;

// ��� cmdIncAt � cmdDecAt
const UINT	bitPushBefore = 1 << 12;	// �������� �������� � ���� �� ���������
const UINT	bitPushAfter = 1 << 13;		// �������� �������� � ���� ����� ���������

// constants for RetFlag creation from different bits
// ��������� ��� �������� ����� �������� �� ��������� �����
const UINT	bitRetError	= 1 << 15;	// ������������ ����� ���������� ��������, ���������� ����������
const UINT	bitRetSimple= 1 << 14;	// ������� ���������� ������� ���

// Command list (bytecode)
// ������� ������ (�������)
class CommandList
{
	struct CodeInfo
	{
		CodeInfo(UINT position, const char* beginPos, const char* endPos)
		{
			byteCodePos = position;
			memcpy(info, beginPos, (endPos-beginPos < 128 ? endPos-beginPos : 127));
			info[(endPos-beginPos < 128 ? endPos-beginPos : 127)] = '\0';
			for(int i = 0; i < 128; i++)
				if(info[i] == '\n' || info[i] == '\r')
					info[i] = ' ';
		}

		UINT	byteCodePos;	// ������� � ��������, � ������� ��������� ������
		char	info[128];		// ��������� �� ������ � ����� ������
	};
public:
	// ������ ����� ����� ��� ������� ������
	CommandList(UINT count = 65000)
	{
		bytecode = new char[count];
		max = count;
		curr = 0;
	}
	CommandList(const CommandList& r)
	{
		bytecode = new char[r.max];
		max = r.max;
		curr = 0;
	}
	~CommandList()
	{
		delete[] bytecode;
	}

	// �������� ������ (��� ����� ���� �������, ������, ����� � �����)
	// �������� ������ ����
	template<class T> void AddData(const T& data)
	{
		memcpy(bytecode+curr, &data, sizeof(T));
		curr += sizeof(T);
	}
	// ��������� ������������ ����
	void AddData(const void* data, size_t size)
	{
		memcpy(bytecode+curr, data, size);
		curr += (UINT)size;
	}

	// �������� int �� ������
	__inline	bool GetINT(UINT pos, int& ret)
	{
		ret = *(reinterpret_cast<int*>(bytecode+pos));
		return true;
	}
	// �������� short int �� ������
	__inline	bool GetSHORT(UINT pos, short int& ret)
	{
		if(pos+2 > curr)
			return false;
		ret = *(reinterpret_cast<short int*>(bytecode+pos));
		return true;
	}
	// �������� unsigned short int �� ������
	__inline	bool GetUSHORT(UINT pos, unsigned short& ret)
	{
		ret = *(reinterpret_cast<unsigned short*>(bytecode+pos));
		return true;
	}
	// �������� unsinged int �� ������
	__inline	bool GetUINT(UINT pos, UINT& ret)
	{
		ret = *(reinterpret_cast<UINT*>(bytecode+pos));
		return true;
	}
	// �������� unsigned char �� ������
	__inline	bool GetUCHAR(UINT pos, unsigned char& ret)
	{
		ret = *(reinterpret_cast<unsigned char*>(bytecode+pos));
		return true;
	}

	// �������� �������� ������������� ���� �� ������
	template<class T> __inline bool GetData(UINT pos, T& ret)
	{
		if(pos+sizeof(T) > curr)
			return false;
		memcpy(&ret, bytecode+pos, sizeof(T));
		return true;
	}
	// �������� ������������ ���-�� ���� �� ������
	__inline bool	GetData(UINT pos, void* data, size_t size)
	{
		if(pos+size > curr)
			return false;
		memcpy(data, bytecode+pos, size);
		return true;
	}

	// �������� ������������ ���������� ���� �� ������
	__inline void	SetData(UINT pos, const void* data, size_t size)
	{
		memcpy(bytecode+pos, data, size);
	}

	// �������� ������� ������ ��������
	__inline const UINT&	GetCurrPos()
	{
		return curr;
	}

	// ������� �������
	__inline void	Clear()
	{
		curr = 0;
		memset(bytecode, 0, max);
		codeInfo.clear();
	}

	void		AddDescription(UINT position, const char* start, const char* end)
	{
		codeInfo.push_back(CodeInfo(position, start, end));
	}
	const char*	GetDescription(UINT position)
	{
		for(int s = 0, e = (int)codeInfo.size(); s != e; s++)
		{
			if(codeInfo[s].byteCodePos == position)
				return codeInfo[s].info;
			if(codeInfo[s].byteCodePos > position)
				return NULL;
		}
		return NULL;
	}
//private:
	char*	bytecode;
	UINT	curr;
	UINT	max;

	std::vector<CodeInfo>	codeInfo;	// ������ ����� � ����
};
