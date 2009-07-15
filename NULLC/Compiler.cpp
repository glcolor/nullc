#include "stdafx.h"

#include "SupSpi/SupSpi.h"
using namespace supspi;

#include "CodeInfo.h"
using namespace CodeInfo;

#include "Bytecode.h"

#include "Compiler.h"

#include <time.h>

//////////////////////////////////////////////////////////////////////////
//						Code gen ops
//////////////////////////////////////////////////////////////////////////
std::string		warningLog;
#ifdef NULLC_LOG_FILES
FILE*			compileLog;
#endif

// ���������� � �������� ����� ����������. ��� ���������� �� ������ ��� ����, �����
// ������� ���������� � ����������, ����� ��� ������� �� ������� ���������
std::vector<VarTopInfo>		varInfoTop;
// ��������� ����������� ��������� �������� break, ������� ������ �����, �� ������� �������� ���� �����
// ����������, ����� �������� � � �� ���������, � ������� ��� ���������� ��, ���� �� �����������
// ����������� ��� ���������������� ������. ���� ���� (����������� ����� ���� ����������) ������ ������
// varInfoTop.
std::vector<unsigned int>			cycleBeginVarTop;
// ���������� � ���������� ����������� ������� �� ������ ������������ ������.
// ������ ��� ���� ����� ������� ������� �� ���� ������ �� ������� ���������.
std::vector<unsigned int>			funcInfoTop;

// ������� ��������������� ������� �����
TypeInfo*	typeVoid = NULL;
TypeInfo*	typeChar = NULL;
TypeInfo*	typeShort = NULL;
TypeInfo*	typeInt = NULL;
TypeInfo*	typeFloat = NULL;
TypeInfo*	typeLong = NULL;
TypeInfo*	typeDouble = NULL;
TypeInfo*	typeFile = NULL;

// Temp variables
// ��������� ����������:
// ���������� ������� ����� ����������, ������� ����� ����������
unsigned int negCount, varTop;

// ������������ ���������� ��� ������ � ������
unsigned int currAlign;

// ���� �� ���������� ���������� (��� addVarSetNode)
bool varDefined;

// �������� �� ������� ���������� �����������
bool currValConst;

// ����� ������� ���������� - ������������ �������
unsigned int inplaceArrayNum;

// ��� ����������� ����� �����
TypeInfo *newType = NULL;

// ���������� � ���� ������� ����������
TypeInfo*	currType = NULL;
// ���� ( :) )����� ����������
// ��� ����������� arr[arr[i.a.b].y].x;
std::vector<TypeInfo*>	currTypes;

// ������ ��������� �����
std::vector<std::string>	strs;
// ���� � ����������� ���������� ���������� �������.
// ���� ��� �������� ����� foo(1, bar(2, 3, 4), 5), ����� ����� ��������� ���������� ����������,
// ��� ������ �� �������� ��� �������� ���������� ���������� ���������� � ���, ������� ��������� �������
std::vector<unsigned int>			callArgCount;
// ����, ������� ������ ���� ��������, ������� ���������� �������.
// ������� ����� ���������� ���� � ������
std::vector<TypeInfo*>		retTypeStack;
std::vector<FunctionInfo*>	currDefinedFunc;

// ������ �����, ������� ���������� ��� �������
std::vector<NodeZeroOP*>	funcDefList;

int AddFunctionExternal(FunctionInfo* func, std::string name)
{
	for(unsigned int i = 0; i < func->external.size(); i++)
		if(func->external[i] == name)
			return i;

#ifdef NULLC_LOG_FILES
	fprintf(compileLog, "Function %s uses external variable %s\r\n", currDefinedFunc.back()->name.c_str(), name.c_str());
#endif
	func->external.push_back(name);
	return (int)func->external.size()-1;
}

long long parseInteger(char const* s, char const* e, int base)
{
	unsigned long long res = 0;
	for(const char *p = s; p < e; p++)
	{
		int digit = ((*p >= '0' && *p <= '9') ? *p - '0' : tolower(*p) - 'a' + 10);
		if(digit < 0 || digit > base)
		{
			char error[64];
			sprintf(error, "ERROR: Digit %d is not allowed in base %d", digit, base);
			lastError = CompilerError(error, p);
			supspi::Abort();
			return 0;
		}
		res = res * base + digit;
	}
	return res;
}

// ���������, �������� �� ������������� ����������������� ��� ��� �������
void checkIfDeclared(const std::string& str)
{
	if(str == "if" || str == "else" || str == "for" || str == "while" || str == "return" || str=="switch" || str=="case")
	{
		std::string fullError = std::string("ERROR: The name '" + str + "' is reserved");
		lastError = CompilerError(fullError, lastKnownStartPos);
		supspi::Abort();
		return;
	}
	for(unsigned int i = 0; i < funcInfo.size(); i++)
	{
		if(funcInfo[i]->name == str && funcInfo[i]->visible)
		{
			std::string fullError = std::string("ERROR: Name '" + str + "' is already taken for a function");
			lastError = CompilerError(fullError, lastKnownStartPos);
			supspi::Abort();
			return;
		}
	}
}

// ���������� � ������ ����� {}, ����� ��������� ���������� ����������� ����������, � �������� �����
// ����� �������� ����� ��������� �����.
void blockBegin(char const* s, char const* e)
{
	(void)s; (void)e;	// C4100
	varInfoTop.push_back(VarTopInfo((unsigned int)varInfo.size(), varTop));
	funcInfoTop.push_back((unsigned int)funcInfo.size());
}
// ���������� � ����� ����� {}, ����� ������ ���������� � ���������� ������ �����, ��� ����� �����������
// �� ����� �� ������� ���������. ����� ��������� ������� ����� ���������� � ������.
void blockEnd(char const* s, char const* e)
{
	(void)s; (void)e;	// C4100
	unsigned int varFormerTop = varTop;
	while(varInfo.size() > varInfoTop.back().activeVarCnt)
		varInfo.pop_back();
	varTop = varInfoTop.back().varStackSize;
	varInfoTop.pop_back();

	for(unsigned int i = funcInfoTop.back(); i < funcInfo.size(); i++)
		funcInfo[i]->visible = false;
	funcInfoTop.pop_back();

	nodeList.push_back(new NodeBlock(varFormerTop-varTop));
}

// ������� ��� ���������� ����� � ������������ ������� ������ �����
template<typename T>
void addNumberNode(char const*s, char const*e);

template<> void addNumberNode<char>(char const*s, char const*e)
{
	(void)e;	// C4100
	char res = s[1];
	if(res == '\\')
	{
		if(s[2] == 'n')
			res = '\n';
		if(s[2] == 'r')
			res = '\r';
		if(s[2] == 't')
			res = '\t';
		if(s[2] == '0')
			res = '\0';
		if(s[2] == '\'')
			res = '\'';
		if(s[2] == '\\')
			res = '\\';
	}
	nodeList.push_back(new NodeNumber<int>(res, typeChar));
}

template<> void addNumberNode<int>(char const*s, char const*e)
{
	(void)e;	// C4100
	nodeList.push_back(new NodeNumber<int>(atoi(s), typeInt));
}
template<> void addNumberNode<float>(char const*s, char const*e)
{
	(void)e;	// C4100
	nodeList.push_back(new NodeNumber<float>((float)atof(s), typeFloat));
}
template<> void addNumberNode<long long>(char const*s, char const*e)
{
	(void)e;	// C4100
	nodeList.push_back(new NodeNumber<long long>(parseInteger(s, e-1, 10), typeLong));
}
template<> void addNumberNode<double>(char const*s, char const*e)
{
	(void)e;	// C4100
	nodeList.push_back(new NodeNumber<double>(atof(s), typeDouble));
}

void addVoidNode(char const*s, char const*e)
{
	(void)s; (void)e;	// C4100
	nodeList.push_back(new NodeZeroOP());
}

void addHexInt(char const*s, char const*e)
{
	s += 2;
	if(int(e-s) > 16)
	{
		lastError = CompilerError("ERROR: Overflow in hexadecimal constant", s);
		supspi::Abort();
		return;
	}
	if(int(e-s) <= 8)
		nodeList.push_back(new NodeNumber<int>((unsigned int)parseInteger(s, e, 16), typeInt));
	else
		nodeList.push_back(new NodeNumber<long long>(parseInteger(s, e, 16), typeLong));
}

void addOctInt(char const*s, char const*e)
{
	s++;
	if(int(e-s) > 21)
	{
		lastError = CompilerError("ERROR: Overflow in octal constant", s);
		supspi::Abort();
		return;
	}
	if(int(e-s) <= 10)
		nodeList.push_back(new NodeNumber<int>((unsigned int)parseInteger(s, e, 8), typeInt));
	else
		nodeList.push_back(new NodeNumber<long long>(parseInteger(s, e, 8), typeLong));
}

void addBinInt(char const*s, char const*e)
{
	e--;
	if(int(e-s) > 64)
	{
		lastError = CompilerError("ERROR: Overflow in binary constant", s);
		supspi::Abort();
		return;
	}
	if(int(e-s) <= 32)
		nodeList.push_back(new NodeNumber<int>((unsigned int)parseInteger(s, e, 2), typeInt));
	else
		nodeList.push_back(new NodeNumber<long long>(parseInteger(s, e, 2), typeLong));
}
// ������� ��� �������� ����, ������� ����� ������ � ����
// ������������ NodeExpressionList, ��� �� �������� ����� ������� � �������� ���������
// �� ���� �� ���� ������ ��������� ����� � ����������� ���������� ������.
void addStringNode(char const*s, char const*e)
{
	lastKnownStartPos = s;

	const char *curr = s+1, *end = e-1;
	unsigned int len = 0;
	// Find the length of the string with collapsed escape-sequences
	for(; curr < end; curr++, len++)
	{
		if(*curr == '\\')
			curr++;
	}
	curr = s+1;
	end = e-1;

	nodeList.push_back(new NodeZeroOP());
	nodeList.push_back(new NodeNumber<int>(len+1, typeInt));
	TypeInfo *targetType = GetArrayType(typeChar);
	if(!targetType)
	{
		supspi::Abort();
		return;
	}
	nodeList.push_back(new NodeExpressionList(targetType));

	NodeZeroOP* temp = nodeList.back();
	nodeList.pop_back();

	NodeExpressionList *arrayList = static_cast<NodeExpressionList*>(temp);

	while(end-curr > 0)
	{
		char clean[4];
		*(int*)clean = 0;

		for(int i = 0; i < 4 && curr < end; i++, curr++)
		{
			clean[i] = *curr;
			if(*curr == '\\')
			{
				curr++;
				if(*curr == 'n')
					clean[i] = '\n';
				if(*curr == 'r')
					clean[i] = '\r';
				if(*curr == 't')
					clean[i] = '\t';
				if(*curr == '0')
					clean[i] = '\0';
				if(*curr == '\'')
					clean[i] = '\'';
				if(*curr == '\"')
					clean[i] = '\"';
				if(*curr == '\\')
					clean[i] = '\\';
			}
		}
		nodeList.push_back(new NodeNumber<int>(*(int*)clean, typeInt));
		arrayList->AddNode();
	}
	if(len % 4 == 0)
	{
		nodeList.push_back(new NodeNumber<int>(0, typeInt));
		arrayList->AddNode();
	}
	nodeList.push_back(temp);

	strs.pop_back();
}

// ������� ��� �������� ����, ������� ����� �������� �� ����� ����������
// ���� ������ � ���� ��������� ���� � ������.
void addPopNode(char const* s, char const* e)
{
	nodeList.back()->SetCodeInfo(s, e);
	// ���� ��������� ���� � ������ - ���� � ������, ����� ���
	if(nodeList.back()->GetNodeType() == typeNodeNumber)
	{
		delete nodeList.back();
		nodeList.pop_back();
		nodeList.push_back(new NodeZeroOP());
	}else if(nodeList.back()->GetNodeType() == typeNodePreOrPostOp){
		// ���� ��������� ����, ��� ����������, ������� ��������� ��� ����������� �� 1, �� ��������� �
		// ��������� � ��������, �� ����� ���������� ����������� ����.
		static_cast<NodePreOrPostOp*>(nodeList.back())->SetOptimised(true);
	}else{
		// ����� ������ �������� ���, ��� � ����������� � ������
		nodeList.push_back(new NodePopOp());
	}
}

// ������� ��� �������� ����, ������� �������� ���� �������� � �����
// ���� ������ � ���� ��������� ���� � ������.
void addNegNode(char const* s, char const* e)
{
	(void)e;	// C4100
	// ���� ���������� ���� ����� ������, �� �������������� ���� �� ����������
	// � ��� �� ����� ������� �����
	if(negCount % 2 == 0)
		return;
	// ���� ��������� ���� ��� �����, �� ������ �������� ���� � ���������
	if(nodeList.back()->GetNodeType() == typeNodeNumber)
	{
		TypeInfo *aType = nodeList.back()->GetTypeInfo();
		NodeZeroOP* zOP = nodeList.back();
		NodeZeroOP* Rd = NULL;
		if(aType == typeDouble)
		{
			Rd = new NodeNumber<double>(-static_cast<NodeNumber<double>* >(zOP)->GetVal(), zOP->GetTypeInfo());
		}else if(aType == typeFloat){
			Rd = new NodeNumber<float>(-static_cast<NodeNumber<float>* >(zOP)->GetVal(), zOP->GetTypeInfo());
		}else if(aType == typeLong){
			Rd = new NodeNumber<long long>(-static_cast<NodeNumber<long long>* >(zOP)->GetVal(), zOP->GetTypeInfo());
		}else if(aType == typeInt){
			Rd = new NodeNumber<int>(-static_cast<NodeNumber<int>* >(zOP)->GetVal(), zOP->GetTypeInfo());
		}else{
			std::string fullError = std::string("addNegNode() ERROR: unknown type ") + aType->name;
			lastError = CompilerError(fullError, s);
			supspi::Abort();
			return;
		}
		delete nodeList.back();
		nodeList.pop_back();
		nodeList.push_back(Rd);
	}else{
		// ����� ������ �������� ���, ��� � ����������� � ������
		nodeList.push_back(new NodeUnaryOp(cmdNeg));
	}
	// ������� �������� ���� ����� �� 0
	negCount = 0;
}

// ������� ��� �������� ����, ������� ��������� ���������� ��������� ��� ��������� � �����
// ���� ������ � ���� ��������� ���� � ������.
void addLogNotNode(char const* s, char const* e)
{
	(void)e;	// C4100
	// ���� ��������� ���� � ������ - �����, �� ��������� �������� �� ����� ���������
	if(nodeList.back()->GetNodeType() == typeNodeNumber)
	{
		TypeInfo *aType = nodeList.back()->GetTypeInfo();
		NodeZeroOP* zOP = nodeList.back();
		NodeZeroOP* Rd = NULL;
		if(aType == typeDouble)
		{
			Rd = new NodeNumber<double>(static_cast<NodeNumber<double>* >(zOP)->GetLogNotVal(), zOP->GetTypeInfo());
		}else if(aType == typeFloat){
			Rd = new NodeNumber<float>(static_cast<NodeNumber<float>* >(zOP)->GetLogNotVal(), zOP->GetTypeInfo());
		}else if(aType == typeLong){
			Rd = new NodeNumber<long long>(static_cast<NodeNumber<long long>* >(zOP)->GetLogNotVal(), zOP->GetTypeInfo());
		}else if(aType == typeInt){
			Rd = new NodeNumber<int>(static_cast<NodeNumber<int>* >(zOP)->GetLogNotVal(), zOP->GetTypeInfo());
		}else{
			std::string fullError = std::string("addLogNotNode() ERROR: unknown type ") + aType->name;
			lastError = CompilerError(fullError, s);
			supspi::Abort();
			return;
		}
		delete nodeList.back();
		nodeList.pop_back();
		nodeList.push_back(Rd);
	}else{
		// ����� ������ �������� ���, ��� � ����������� � ������
		nodeList.push_back(new NodeUnaryOp(cmdLogNot));
	}
}
void addBitNotNode(char const* s, char const* e)
{
	(void)e;	// C4100
	if(nodeList.back()->GetNodeType() == typeNodeNumber)
	{
		TypeInfo *aType = nodeList.back()->GetTypeInfo();
		NodeZeroOP* zOP = nodeList.back();
		NodeZeroOP* Rd = NULL;
		if(aType == typeDouble)
		{
			lastError = CompilerError("ERROR: bitwise NOT cannot be used on floating point numbers", s);
			supspi::Abort();
			return;
		}else if(aType == typeFloat){
			lastError = CompilerError("ERROR: bitwise NOT cannot be used on floating point numbers", s);
			supspi::Abort();
			return;
		}else if(aType == typeLong){
			Rd = new NodeNumber<long long>(static_cast<NodeNumber<long long>* >(zOP)->GetBitNotVal(), zOP->GetTypeInfo());
		}else if(aType == typeInt){
			Rd = new NodeNumber<int>(static_cast<NodeNumber<int>* >(zOP)->GetBitNotVal(), zOP->GetTypeInfo());
		}else{
			std::string fullError = std::string("addBitNotNode() ERROR: unknown type ") + aType->name;
			lastError = CompilerError(fullError, s);
			supspi::Abort();
			return;
		}
		delete nodeList.back();
		nodeList.pop_back();
		nodeList.push_back(Rd);
	}else{
		nodeList.push_back(new NodeUnaryOp(cmdBitNot));
	}
}

template<typename T>
T optDoOperation(CmdID cmd, T a, T b, bool swap = false)
{
	if(swap)
		std::swap(a, b);
	if(cmd == cmdAdd)
		return a + b;
	if(cmd == cmdSub)
		return a - b;
	if(cmd == cmdMul)
		return a * b;
	if(cmd == cmdDiv)
		return a / b;
	if(cmd == cmdPow)
		return (T)pow((double)a, (double)b);
	if(cmd == cmdLess)
		return a < b;
	if(cmd == cmdGreater)
		return a > b;
	if(cmd == cmdGEqual)
		return a >= b;
	if(cmd == cmdLEqual)
		return a <= b;
	if(cmd == cmdEqual)
		return a == b;
	if(cmd == cmdNEqual)
		return a != b;
	return optDoSpecial(cmd, a, b);
}
template<typename T>
T optDoSpecial(CmdID cmd, T a, T b)
{
	(void)cmd; (void)b; (void)a;	// C4100
	lastError = CompilerError("ERROR: optDoSpecial call with unknown type", lastKnownStartPos);
	supspi::Abort();
	return 0;
}
template<> int optDoSpecial<>(CmdID cmd, int a, int b)
{
	if(cmd == cmdShl)
		return a << b;
	if(cmd == cmdShr)
		return a >> b;
	if(cmd == cmdMod)
		return a % b;
	if(cmd == cmdBitAnd)
		return a & b;
	if(cmd == cmdBitXor)
		return a ^ b;
	if(cmd == cmdBitOr)
		return a | b;
	if(cmd == cmdLogAnd)
		return a && b;
	if(cmd == cmdLogXor)
		return !!a ^ !!b;
	if(cmd == cmdLogOr)
		return a || b;
	lastError = CompilerError("ERROR: optDoSpecial<int> call with unknown command", lastKnownStartPos);
	supspi::Abort();
	return 0;
}
template<> long long optDoSpecial<>(CmdID cmd, long long a, long long b)
{
	if(cmd == cmdShl)
		return a << b;
	if(cmd == cmdShr)
		return a >> b;
	if(cmd == cmdMod)
		return a % b;
	if(cmd == cmdBitAnd)
		return a & b;
	if(cmd == cmdBitXor)
		return a ^ b;
	if(cmd == cmdBitOr)
		return a | b;
	if(cmd == cmdLogAnd)
		return a && b;
	if(cmd == cmdLogXor)
		return !!a ^ !!b;
	if(cmd == cmdLogOr)
		return a || b;
	lastError = CompilerError("ERROR: optDoSpecial<long long> call with unknown command", lastKnownStartPos);
	supspi::Abort();
	return 0;
}
template<> double optDoSpecial<>(CmdID cmd, double a, double b)
{
	if(cmd == cmdShl)
	{
		lastError = CompilerError("ERROR: optDoSpecial<double> call with << operation is illegal", lastKnownStartPos);
		supspi::Abort();
		return 0.0;
	}
	if(cmd == cmdShr)
	{
		lastError = CompilerError("ERROR: optDoSpecial<double> call with >> operation is illegal", lastKnownStartPos);
		supspi::Abort();
		return 0.0;
	}
	if(cmd == cmdMod)
		return fmod(a,b);
	if(cmd >= cmdBitAnd && cmd <= cmdBitXor)
	{
		lastError = CompilerError("ERROR: optDoSpecial<double> call with binary operation is illegal", lastKnownStartPos);
		supspi::Abort();
		return 0.0;
	}
	if(cmd == cmdLogAnd)
		return (int)a && (int)b;
	if(cmd == cmdLogXor)
		return !!(int)a ^ !!(int)b;
	if(cmd == cmdLogOr)
		return (int)a || (int)b;
	lastError = CompilerError("ERROR: optDoSpecial<double> call with unknown command", lastKnownStartPos);
	supspi::Abort();
	return 0.0;
}

void popLastNodeCond(bool swap)
{
	if(swap)
	{
		NodeZeroOP* temp = nodeList.back();
		nodeList.pop_back();
		delete nodeList.back();
		nodeList.back() = temp;
	}else{
		delete nodeList.back();
		nodeList.pop_back();
	}
}

void addTwoAndCmpNode(CmdID id)
{
	unsigned int aNodeType = nodeList[nodeList.size()-2]->GetNodeType();
	unsigned int bNodeType = nodeList[nodeList.size()-1]->GetNodeType();
	unsigned int shA = 2, shB = 1;	//Shifts to operand A and B in array
	TypeInfo *aType, *bType;

	if(aNodeType == typeNodeNumber && bNodeType == typeNodeNumber)
	{
		//If we have operation between two known numbers, we can optimize code by calculating the result in place
		aType = nodeList[nodeList.size()-2]->GetTypeInfo();
		bType = nodeList[nodeList.size()-1]->GetTypeInfo();

		//Swap operands, to reduce number of combinations
		if((aType == typeFloat || aType == typeLong || aType == typeInt) && bType == typeDouble)
			std::swap(shA, shB);
		if((aType == typeLong || aType == typeInt) && bType == typeFloat)
			std::swap(shA, shB);
		if(aType == typeInt && bType == typeLong)
			std::swap(shA, shB);

		bool swapOper = shA != 2;

		aType = nodeList[nodeList.size()-shA]->GetTypeInfo();
		bType = nodeList[nodeList.size()-shB]->GetTypeInfo();
		if(aType == typeDouble)
		{
			NodeNumber<double> *Ad = static_cast<NodeNumber<double>* >(nodeList[nodeList.size()-shA]);
			NodeNumber<double>* Rd = NULL;
			if(bType == typeDouble)
			{
				NodeNumber<double> *Bd = static_cast<NodeNumber<double>* >(nodeList[nodeList.size()-shB]);
				Rd = new NodeNumber<double>(optDoOperation<double>(id, Ad->GetVal(), Bd->GetVal()), typeDouble);
			}else if(bType == typeFloat){
				NodeNumber<float> *Bd = static_cast<NodeNumber<float>* >(nodeList[nodeList.size()-shB]);
				Rd = new NodeNumber<double>(optDoOperation<double>(id, Ad->GetVal(), (double)Bd->GetVal(), swapOper), typeDouble);
			}else if(bType == typeLong){
				NodeNumber<long long> *Bd = static_cast<NodeNumber<long long>* >(nodeList[nodeList.size()-shB]);
				Rd = new NodeNumber<double>(optDoOperation<double>(id, Ad->GetVal(), (double)Bd->GetVal(), swapOper), typeDouble);
			}else if(bType == typeInt){
				NodeNumber<int> *Bd = static_cast<NodeNumber<int>* >(nodeList[nodeList.size()-shB]);
				Rd = new NodeNumber<double>(optDoOperation<double>(id, Ad->GetVal(), (double)Bd->GetVal(), swapOper), typeDouble);
			}
			delete nodeList.back();
			nodeList.pop_back();
			delete nodeList.back();
			nodeList.pop_back();
			nodeList.push_back(Rd);
		}else if(aType == typeFloat){
			NodeNumber<float> *Ad = static_cast<NodeNumber<float>* >(nodeList[nodeList.size()-shA]);
			NodeNumber<float>* Rd = NULL;
			if(bType == typeFloat){
				NodeNumber<float> *Bd = static_cast<NodeNumber<float>* >(nodeList[nodeList.size()-shB]);
				Rd = new NodeNumber<float>(optDoOperation<float>(id, Ad->GetVal(), Bd->GetVal()), typeFloat);
			}else if(bType == typeLong){
				NodeNumber<long long> *Bd = static_cast<NodeNumber<long long>* >(nodeList[nodeList.size()-shB]);
				Rd = new NodeNumber<float>(optDoOperation<float>(id, Ad->GetVal(), (float)Bd->GetVal(), swapOper), typeFloat);
			}else if(bType == typeInt){
				NodeNumber<int> *Bd = static_cast<NodeNumber<int>* >(nodeList[nodeList.size()-shB]);
				Rd = new NodeNumber<float>(optDoOperation<float>(id, Ad->GetVal(), (float)Bd->GetVal(), swapOper), typeFloat);
			}
			delete nodeList.back();
			nodeList.pop_back();
			delete nodeList.back();
			nodeList.pop_back();
			nodeList.push_back(Rd);
		}else if(aType == typeLong){
			NodeNumber<long long> *Ad = static_cast<NodeNumber<long long>* >(nodeList[nodeList.size()-shA]);
			NodeNumber<long long>* Rd = NULL;
			if(bType == typeLong){
				NodeNumber<long long> *Bd = static_cast<NodeNumber<long long>* >(nodeList[nodeList.size()-shB]);
				Rd = new NodeNumber<long long>(optDoOperation<long long>(id, Ad->GetVal(), Bd->GetVal()), typeLong);
			}else if(bType == typeInt){
				NodeNumber<int> *Bd = static_cast<NodeNumber<int>* >(nodeList[nodeList.size()-shB]);
				Rd = new NodeNumber<long long>(optDoOperation<long long>(id, Ad->GetVal(), (long long)Bd->GetVal(), swapOper), typeLong);
			}
			delete nodeList.back();
			nodeList.pop_back();
			delete nodeList.back();
			nodeList.pop_back();
			nodeList.push_back(Rd);
		}else if(aType == typeInt){
			NodeNumber<int> *Ad = static_cast<NodeNumber<int>* >(nodeList[nodeList.size()-shA]);
			NodeNumber<int>* Rd;
			//bType is also int!
			NodeNumber<int> *Bd = static_cast<NodeNumber<int>* >(nodeList[nodeList.size()-shB]);
			Rd = new NodeNumber<int>(optDoOperation<int>(id, Ad->GetVal(), Bd->GetVal()), typeInt);
			delete nodeList.back();
			nodeList.pop_back();
			delete nodeList.back();
			nodeList.pop_back();
			nodeList.push_back(Rd);
		}
		return;	// ����������� �������, �������
	}
	if(aNodeType == typeNodeNumber || bNodeType == typeNodeNumber)
	{
		// ���� ���� �� ����� - �����, �� �������� ��������� ������� ���, ����� ���� � ������ ��� � A
		if(bNodeType == typeNodeNumber)
		{
			std::swap(shA, shB);
			std::swap(aNodeType, bNodeType);
		}

		// ����������� ����� ����������, ���� ������ ������� - typeNodeTwoAndCmdOp ��� typeNodeVarGet
		if(bNodeType != typeNodeTwoAndCmdOp && bNodeType != typeNodeDereference)
		{
			// �����, ������� ��� �����������
			nodeList.push_back(new NodeTwoAndCmdOp(id));
			if(!lastError.IsEmpty())
				supspi::Abort();
			return;
		}

		// ����������� ����� ����������, ���� ����� == 0 ��� ����� == 1
		bool success = false;
		bType = nodeList[nodeList.size()-shA]->GetTypeInfo();
		if(bType == typeDouble)
		{
			NodeNumber<double> *Ad = static_cast<NodeNumber<double>* >(nodeList[nodeList.size()-shA]);
			if(Ad->GetVal() == 0.0 && id == cmdMul)
			{
				popLastNodeCond(shA == 1); // a*0.0 -> 0.0
				success = true;
			}
			if((Ad->GetVal() == 0.0 && id == cmdAdd) || (Ad->GetVal() == 1.0 && id == cmdMul))
			{
				popLastNodeCond(shA == 2); // a+0.0 -> a || a*1.0 -> a
				success = true;
			}
		}else if(bType == typeFloat){
			NodeNumber<float> *Ad = static_cast<NodeNumber<float>* >(nodeList[nodeList.size()-shA]);
			if(Ad->GetVal() == 0.0f && id == cmdMul)
			{
				popLastNodeCond(shA == 1); // a*0.0f -> 0.0f
				success = true;
			}
			if((Ad->GetVal() == 0.0f && id == cmdAdd) || (Ad->GetVal() == 1.0f && id == cmdMul))
			{
				popLastNodeCond(shA == 2); // a+0.0f -> a || a*1.0f -> a
				success = true;
			}
		}else if(bType == typeLong){
			NodeNumber<long long> *Ad = static_cast<NodeNumber<long long>* >(nodeList[nodeList.size()-shA]);
			if(Ad->GetVal() == 0 && id == cmdMul)
			{
				popLastNodeCond(shA == 1); // a*0L -> 0L
				success = true;
			}
			if((Ad->GetVal() == 0 && id == cmdAdd) || (Ad->GetVal() == 1 && id == cmdMul))
			{
				popLastNodeCond(shA == 2); // a+0L -> a || a*1L -> a
				success = true;
			}
		}else if(bType == typeInt){
			NodeNumber<int> *Ad = static_cast<NodeNumber<int>* >(nodeList[nodeList.size()-shA]);
			if(Ad->GetVal() == 0 && id == cmdMul)
			{
				popLastNodeCond(shA == 1); // a*0 -> 0
				success = true;
			}
			if((Ad->GetVal() == 0 && id == cmdAdd) || (Ad->GetVal() == 1 && id == cmdMul))
			{
				popLastNodeCond(shA == 2); // a+0 -> a || a*1 -> a
				success = true;
			}
		}
		if(success)	// ����������� �������, ������� �����
			return;
	}
	// ����������� �� �������, ������� �������� ���������
	nodeList.push_back(new NodeTwoAndCmdOp(id));
	if(!lastError.IsEmpty())
		supspi::Abort();
}

template<CmdID cmd> void createTwoAndCmd(char const* s, char const* e)
{
	(void)e;	// C4100
	lastKnownStartPos = s;
	addTwoAndCmpNode(cmd);
}

typedef void (*ParseFuncPtr)(char const* s, char const* e);

static ParseFuncPtr addCmd(CmdID cmd)
{
	if(cmd == cmdAdd) return &createTwoAndCmd<cmdAdd>;
	if(cmd == cmdSub) return &createTwoAndCmd<cmdSub>;
	if(cmd == cmdMul) return &createTwoAndCmd<cmdMul>;
	if(cmd == cmdDiv) return &createTwoAndCmd<cmdDiv>;
	if(cmd == cmdPow) return &createTwoAndCmd<cmdPow>;
	if(cmd == cmdLess) return &createTwoAndCmd<cmdLess>;
	if(cmd == cmdGreater) return &createTwoAndCmd<cmdGreater>;
	if(cmd == cmdLEqual) return &createTwoAndCmd<cmdLEqual>;
	if(cmd == cmdGEqual) return &createTwoAndCmd<cmdGEqual>;
	if(cmd == cmdEqual) return &createTwoAndCmd<cmdEqual>;
	if(cmd == cmdNEqual) return &createTwoAndCmd<cmdNEqual>;
	if(cmd == cmdShl) return &createTwoAndCmd<cmdShl>;
	if(cmd == cmdShr) return &createTwoAndCmd<cmdShr>;
	if(cmd == cmdMod) return &createTwoAndCmd<cmdMod>;
	if(cmd == cmdBitAnd) return &createTwoAndCmd<cmdBitAnd>;
	if(cmd == cmdBitOr) return &createTwoAndCmd<cmdBitOr>;
	if(cmd == cmdBitXor) return &createTwoAndCmd<cmdBitXor>;
	if(cmd == cmdLogAnd) return &createTwoAndCmd<cmdLogAnd>;
	if(cmd == cmdLogOr) return &createTwoAndCmd<cmdLogOr>;
	if(cmd == cmdLogXor) return &createTwoAndCmd<cmdLogXor>;
	lastError = CompilerError("ERROR: addCmd call with unknown command", lastKnownStartPos);
	supspi::Abort();
	return NULL;
}

void addReturnNode(char const* s, char const* e)
{
	int t = (int)varInfoTop.size();
	int c = 0;
	if(funcInfo.size() != 0)
	{
		while(t > (int)funcInfo.back()->vTopSize)
		{
			c++;
			t--;
		}
	}
	TypeInfo *realRetType = nodeList.back()->GetTypeInfo();
	if(retTypeStack.back() && (retTypeStack.back()->type == TypeInfo::TYPE_COMPLEX || realRetType->type == TypeInfo::TYPE_COMPLEX) && retTypeStack.back() != realRetType)
	{
		lastError = CompilerError("ERROR: function returns " + retTypeStack.back()->GetTypeName() + " but supposed to return " + realRetType->GetTypeName(), s);
		supspi::Abort();
		return;
	}
	if(retTypeStack.back() && retTypeStack.back()->type == TypeInfo::TYPE_VOID && realRetType != typeVoid)
	{
		lastError = CompilerError("ERROR: function returning a value", s);
		supspi::Abort();
		return;
	}
	if(retTypeStack.back() && retTypeStack.back() != typeVoid && realRetType == typeVoid)
	{
		lastError = CompilerError("ERROR: function should return " + retTypeStack.back()->GetTypeName(), s);
		supspi::Abort();
		return;
	}
	if(!retTypeStack.back() && realRetType == typeVoid)
	{
		lastError = CompilerError("ERROR: global return cannot accept void", s);
		supspi::Abort();
		return;
	}
	nodeList.push_back(new NodeReturnOp(c, retTypeStack.back()));
	nodeList.back()->SetCodeInfo(s, e);
}

void addBreakNode(char const* s, char const* e)
{
	(void)e;	// C4100
	if(cycleBeginVarTop.empty())
	{
		lastError = CompilerError("ERROR: break used outside loop statements", s);
		supspi::Abort();
		return;
	}
	int t = (int)varInfoTop.size();
	int c = 0;
	while(t > (int)cycleBeginVarTop.back())
	{
		c++;
		t--;
	}
	nodeList.push_back(new NodeBreakOp(c));
}

void AddContinueNode(char const* s, char const* e)
{
	(void)e;	// C4100
	if(cycleBeginVarTop.empty())
	{
		lastError = CompilerError("ERROR: continue used outside loop statements", s);
		supspi::Abort();
		return;
	}
	int t = (int)varInfoTop.size();
	int c = 0;
	while(t > (int)cycleBeginVarTop.back())
	{
		c++;
		t--;
	}
	nodeList.push_back(new NodeContinueOp(c));
}

//Finds TypeInfo in a typeInfo list by name
void selType(char const* s, char const* e)
{
	string vType = std::string(s,e);
	if(vType == "auto")
	{
		currType = NULL;
		return;
	}
	for(unsigned int i = 0; i < typeInfo.size(); i++)
	{
		if(typeInfo[i]->name == vType)
		{
			currType = typeInfo[i];
			return;
		}
	}
	lastError = CompilerError("ERROR: Variable type '" + vType + "' is unknown\r\n", s);
	supspi::Abort();
}

void addTwoExprNode(char const* s, char const* e);

unsigned int	offsetBytes = 0;

#include <set>
std::set<VariableInfo*> varInfoAll;

void addVar(char const* s, char const* e)
{
	(void)e;	// C4100
	lastKnownStartPos = s;
	string vName = strs.back();

	for(unsigned int i = varInfoTop.back().activeVarCnt; i < varInfo.size(); i++)
	{
		if(varInfo[i]->name == vName)
		{
			lastError = CompilerError("ERROR: Name '" + vName + "' is already taken for a variable in current scope\r\n", s);
			supspi::Abort();
			return;
		}
	}
	checkIfDeclared(vName);

	if(currType && currType->size == TypeInfo::UNSIZED_ARRAY)
	{
		lastError = CompilerError("ERROR: variable '" + vName + "' can't be an unfixed size array", s);
		supspi::Abort();
		return;
	}
	if(currType && currType->size > 64*1024*1024)
	{
		lastError = CompilerError("ERROR: variable '" + vName + "' has to big length (>64 Mb)", s);
		supspi::Abort();
		return;
	}
	
	if((currType && currType->alignBytes != 0) || currAlign != TypeInfo::UNSPECIFIED_ALIGNMENT)
	{
		unsigned int activeAlign = currAlign != TypeInfo::UNSPECIFIED_ALIGNMENT ? currAlign : currType->alignBytes;
		if(activeAlign > 16)
		{
			lastError = CompilerError("ERROR: alignment must me less than 16 bytes", s);
			supspi::Abort();
			return;
		}
		if(activeAlign != 0 && varTop % activeAlign != 0)
		{
			unsigned int offset = activeAlign - (varTop % activeAlign);
			varTop += offset;
			offsetBytes += offset;
		}
	}
	varInfo.push_back(new VariableInfo(vName, varTop, currType, currValConst));
	varInfoAll.insert(varInfo.back());
	varDefined = true;
	if(currType)
		varTop += currType->size;
}

void addVarDefNode(char const* s, char const* e)
{
	(void)e;	// C4100
	assert(varDefined);
	if(!currType)
	{
		lastError = CompilerError("ERROR: auto variable must be initialized in place of definition", s);
		supspi::Abort();
		return;
	}
	nodeList.push_back(new NodeVarDef(strs.back()));
	varInfo.back()->dataReserved = true;
	varDefined = 0;
	offsetBytes = 0;
}

void pushType(char const* s, char const* e)
{
	(void)s; (void)e;	// C4100
	currTypes.push_back(currType);
}

void popType(char const* s, char const* e)
{
	(void)s; (void)e;	// C4100
	currTypes.pop_back();
}

void convertTypeToRef(char const* s, char const* e)
{
	(void)e;	// C4100
	lastKnownStartPos = s;
	if(!currType)
	{
		lastError = CompilerError("ERROR: auto variable cannot have reference flag", s);
		supspi::Abort();
		return;
	}
	currType = GetReferenceType(currType);
}

void convertTypeToArray(char const* s, char const* e)
{
	(void)e;	// C4100
	lastKnownStartPos = s;
	if(!currType)
	{
		lastError = CompilerError("ERROR: cannot specify array size for auto variable", s);
		supspi::Abort();
		return;
	}
	currType = GetArrayType(currType);
	if(!currType)
		supspi::Abort();
}

//////////////////////////////////////////////////////////////////////////
//					New functions for work with variables

void GetVariableType(char const* s, char const* e)
{
	int fID = -1;
	// ���� ���������� �� �����
	int i = (int)varInfo.size()-1;
	string vName(s, e);
	while(i >= 0 && varInfo[i]->name != vName)
		i--;
	if(i == -1)
	{
		// ���� ������� �� �����
		for(int k = 0; k < (int)funcInfo.size(); k++)
		{
			if(funcInfo[k]->name == vName && funcInfo[k]->visible)
			{
				if(fID != -1)
				{
					lastError = CompilerError("ERROR: there are more than one '" + vName + "' function, and the decision isn't clear", s);
					supspi::Abort();
					return;
				}
				fID = k;
			}
		}
		if(fID == -1)
		{
			lastError = CompilerError("ERROR: variable '" + vName + "' is not defined", s);
			supspi::Abort();
			return;
		}
	}

	if(fID == -1)
		currType = varInfo[i]->varType;
	else
		currType = funcInfo[fID]->funcType;
}

bool sizeOfExpr = false;
void GetTypeSize(char const* s, char const* e)
{
	(void)e;	// C4100
	if(!sizeOfExpr && !currTypes.back())
	{
		lastError = CompilerError("ERROR: sizeof(auto) is illegal", s);
		supspi::Abort();
		return;
	}
	if(sizeOfExpr)
	{
		currTypes.back() = nodeList.back()->GetTypeInfo();
		delete nodeList.back();
		nodeList.pop_back();
	}
	nodeList.push_back(new NodeNumber<int>(currTypes.back()->size, typeInt));

	sizeOfExpr = false;
}

void SetTypeOfLastNode(char const* s, char const* e)
{
	(void)s; (void)e;	// C4100
	currType = nodeList.back()->GetTypeInfo();
	delete nodeList.back();
	nodeList.pop_back();
}

void AddInplaceArray(char const* s, char const* e);
void AddDereferenceNode(char const* s, char const* e);
void AddArrayIndexNode(char const* s, char const* e);
void AddMemberAccessNode(char const* s, char const* e);
void addFuncCallNode(char const* s, char const* e);

// ������� ��� ��������� ������ ����������, ��� ������� ��������� � ����������
void AddGetAddressNode(char const* s, char const* e)
{
	lastKnownStartPos = s;

	int fID = -1;
	// ���� ���������� �� �����
	int i = (int)varInfo.size()-1;
	string vName(s, e);
	while(i >= 0 && varInfo[i]->name != vName)
		i--;
	if(i == -1)
	{
		// ���� ������� �� �����
		for(int k = 0; k < (int)funcInfo.size(); k++)
		{
			if(funcInfo[k]->name == vName && funcInfo[k]->visible)
			{
				if(fID != -1)
				{
					lastError = CompilerError("ERROR: there are more than one '" + vName + "' function, and the decision isn't clear", s);
					supspi::Abort();
					return;
				}
				fID = k;
			}
		}
		if(fID == -1)
		{
			lastError = CompilerError("ERROR: variable '" + vName + "' is not defined", s);
			supspi::Abort();
			return;
		}
	}
	// ����� � ���� ����� � ���
	if(fID == -1)
		currTypes.push_back(varInfo[i]->varType);
	else
		currTypes.push_back(funcInfo[fID]->funcType);

	if(newType && (currDefinedFunc.back()->type == FunctionInfo::THISCALL) && vName != "this")
	{
		bool member = false;
		for(unsigned int i = 0; i < newType->memberData.size(); i++)
			if(newType->memberData[i].name == vName)
				member = true;
		if(member)
		{
			// ���������� ���� ���������� ����� ��������� this
			std::string bName = "this";

			FunctionInfo *currFunc = currDefinedFunc.back();

			TypeInfo *temp = GetReferenceType(newType);
			currTypes.push_back(temp);

			nodeList.push_back(new NodeGetAddress(NULL, currFunc->allParamSize, false, temp));

			AddDereferenceNode(0,0);
			strs.push_back(vName);
			AddMemberAccessNode(s, e);

			currTypes.pop_back();
			return;
		}
	}
	// ���� �� ��������� � ��������� �������, � ���������� ��������� � �������� ������� ���������
	if((int)retTypeStack.size() > 1 && (currDefinedFunc.back()->type == FunctionInfo::LOCAL) && i != -1 && i < (int)varInfoTop[currDefinedFunc.back()->vTopSize].activeVarCnt)
	{
		FunctionInfo *currFunc = currDefinedFunc.back();
		// ������� ��� ���������� � ������ ������� ���������� �������
		int num = AddFunctionExternal(currFunc, vName);

		nodeList.push_back(new NodeNumber<int>((int)currDefinedFunc.back()->external.size(), typeInt));
		TypeInfo *temp = GetReferenceType(typeInt);
		temp = GetArrayType(temp);
		if(!temp)
		{
			supspi::Abort();
			return;
		}
		temp = GetReferenceType(temp);
		currTypes.push_back(temp);

		nodeList.push_back(new NodeGetAddress(NULL, currFunc->allParamSize, false, temp));
		AddDereferenceNode(0,0);
		nodeList.push_back(new NodeNumber<int>(num, typeInt));
		AddArrayIndexNode(0,0);
		AddDereferenceNode(0,0);
		// ������ ������� ���
		currTypes.pop_back();
	}else{
		if(fID == -1)
		{
			// ���� ���������� ��������� � ���������� ������� ���������, � ����� - ����������, ��� �������
			bool absAddress = ((varInfoTop.size() > 1) && (varInfo[i]->pos < varInfoTop[1].varStackSize)) || varInfoTop.back().varStackSize == 0;

			int varAddress = varInfo[i]->pos;
			if(!absAddress)
				varAddress -= (int)(varInfoTop.back().varStackSize);

			// ������� ���� ��� ��������� ��������� �� ����������
			nodeList.push_back(new NodeGetAddress(varInfo[i], varAddress, absAddress));
		}else{
			if(funcInfo[fID]->funcPtr != 0)
			{
				lastError = CompilerError("ERROR: Can't get a pointer to an extern function", s);
				supspi::Abort();
				return;
			}
			if(funcInfo[fID]->address == -1 && funcInfo[fID]->funcPtr == NULL)
			{
				lastError = CompilerError("ERROR: Can't get a pointer to a build-in function", s);
				supspi::Abort();
				return;
			}
			if(funcInfo[fID]->type == FunctionInfo::LOCAL)
			{
				std::string bName = "$" + funcInfo[fID]->name + "_ext";
				int i = (int)varInfo.size()-1;
				while(i >= 0 && varInfo[i]->name != bName)
					i--;
				if(i == -1)
				{
					nodeList.push_back(new NodeNumber<int>(0, GetReferenceType(typeInt)));
				}else{
					AddGetAddressNode(bName.c_str(), bName.c_str()+bName.length());
					currTypes.pop_back();
				}
			}

			// ������� ���� ��� ��������� ��������� �� �������
			nodeList.push_back(new NodeFunctionAddress(funcInfo[fID]));
		}
	}
}

// ������� ���������� ��� ���������� �������
void AddArrayIndexNode(char const* s, char const* e)
{
	(void)e;	// C4100
	lastKnownStartPos = s;

	// ��� ������ ���� ��������
	if(currTypes.back()->arrLevel == 0)
	{
		lastError = CompilerError("ERROR: indexing variable that is not an array", s);
		supspi::Abort();
		return;
	}
	// ���� ��� ������������ ������ (��������� �� ������)
	if(currTypes.back()->arrSize == TypeInfo::UNSIZED_ARRAY)
	{
		// �� ����� ����������� ���������� �������� ��������� �� ������, ������� �������� � ����������
		NodeZeroOP* temp = nodeList.back();
		nodeList.pop_back();
		nodeList.push_back(new NodeDereference(GetReferenceType(currTypes.back()->subType)));
		nodeList.push_back(temp);
	}
	// ���� ������ - ����������� ����� � ������� ���� - �����
	if(nodeList.back()->GetNodeType() == typeNodeNumber && nodeList[nodeList.size()-2]->GetNodeType() == typeNodeGetAddress)
	{
		// �������� �������� ������
		int shiftValue;
		NodeZeroOP* indexNode = nodeList.back();
		TypeInfo *aType = indexNode->GetTypeInfo();
		NodeZeroOP* zOP = indexNode;
		if(aType == typeDouble)
		{
			shiftValue = (int)static_cast<NodeNumber<double>* >(zOP)->GetVal();
		}else if(aType == typeFloat){
			shiftValue = (int)static_cast<NodeNumber<float>* >(zOP)->GetVal();
		}else if(aType == typeLong){
			shiftValue = (int)static_cast<NodeNumber<long long>* >(zOP)->GetVal();
		}else if(aType == typeInt){
			shiftValue = static_cast<NodeNumber<int>* >(zOP)->GetVal();
		}else{
			lastError = CompilerError("AddArrayIndexNode() ERROR: unknown index type " + aType->name, lastKnownStartPos);
			supspi::Abort();
			return;
		}

		// �������� ������ �� ����� �� ������� �������
		if(shiftValue < 0)
		{
			lastError = CompilerError("ERROR: Array index cannot be negative", s);
			supspi::Abort();
			return;
		}
		if((unsigned int)shiftValue >= currTypes.back()->arrSize)
		{
			lastError = CompilerError("ERROR: Array index out of bounds", s);
			supspi::Abort();
			return;
		}

		// ����������� ������������ ����
		static_cast<NodeGetAddress*>(nodeList[nodeList.size()-2])->IndexArray(shiftValue);
		delete nodeList.back();
		nodeList.pop_back();
	}else{
		// ����� ������ ���� ����������
		nodeList.push_back(new NodeArrayIndex(currTypes.back()));
	}
	// ������ ������� ��� - ��� �������� �������
	currTypes.back() = currTypes.back()->subType;
}

// ������� ���������� ��� ������������� ���������
void AddDereferenceNode(char const* s, char const* e)
{
	(void)e;	// C4100
	lastKnownStartPos = s;

	// ������ ���� �������������
	nodeList.push_back(new NodeDereference(currTypes.back()));
	// ������ ������� ��� - ��� �� ������� ��������� ������
	currTypes.back() = GetDereferenceType(currTypes.back());
	if(!currTypes.back())
		supspi::Abort();
}

// ���������� � ������ ������������, ��� ����� ���������� ����� ��������� ���� ������������
// ����� ��� ����, ������� ��������� ������� ����
void FailedSetVariable(char const* s, char const* e)
{
	(void)s; (void)e;	// C4100
	delete nodeList.back();
	nodeList.pop_back();
}

// ������� ���������� ��� ���������� ���������� � ������������� ������������� �� ��������
void AddDefineVariableNode(char const* s, char const* e)
{
	lastKnownStartPos = s;

	// ���� ���������� �� �����
	int i = (int)varInfo.size()-1;
	string vName = strs.back();
	while(i >= 0 && varInfo[i]->name != vName)
		i--;
	if(i == -1)
	{
		lastError = CompilerError("ERROR: variable '" + vName + "' is not defined", s);
		supspi::Abort();
		return;
	}
	// ����� � ���� ����� � ���
	currTypes.push_back(varInfo[i]->varType);

	// ���� ���������� ��������� � ���������� ������� ���������, � ����� - ����������, ��� �������
	bool absAddress = ((varInfoTop.size() > 1) && (varInfo[i]->pos < varInfoTop[1].varStackSize)) || varInfoTop.back().varStackSize == 0;

	// ���� ��������� �� ������� ��� ����� NULL, ������ ��� ���������� ��������� ��� ������������� ��������� (auto)
	// � ����� ������, � �������� ���� ������ ������������ ��������� ����� AST
	TypeInfo *realCurrType = currTypes.back() ? currTypes.back() : nodeList.back()->GetTypeInfo();

	// ��������, ��� ����������� �������� ���������� ����������� �������� ��������������� ���� ������
	// ���������� ������ ��� ����, ������������, ��� ��� ���� ���� ���������� � ����
	bool unifyTwo = false;
	// ���� ��� ���������� - ������������ ������, � ������������� �� �������� ������� ����
	if(realCurrType->arrSize == TypeInfo::UNSIZED_ARRAY && realCurrType != nodeList.back()->GetTypeInfo())
	{
		TypeInfo *nodeType = nodeList.back()->GetTypeInfo();
		// ���� ������ ����� �������� (����������������, ��������) ���������
		if(realCurrType->subType == nodeType->subType)
		{
			// � ���� ������ �� ��������� ���� ��������� �������� ����������
			if(nodeList.back()->GetNodeType() != typeNodeDereference)
			{
				// �����, ���� ������ - ����������� ������� �������
				if(nodeList.back()->GetNodeType() == typeNodeExpressionList)
				{
					// ������� ����, ������������� ������� ���������� �������� ����� ������
					AddInplaceArray(s, e);
					// �������� ������ ����, ����������� �� ���������� � �����
					unifyTwo = true;
				}else{
					// �����, ���� �� ����������, ������� ��������������� �� ������
					lastError = CompilerError("ERROR: cannot convert from " + nodeList.back()->GetTypeInfo()->GetTypeName() + " to " + realCurrType->GetTypeName(), s);
					supspi::Abort();
					return;
				}
			}
			// �����, ��� ��� �� ����������� ������������ ������� �������� ����������,
			// ��� ���� ������������� ��� � ���� ���������;������
			// ������ ��������� �� ������, �� - ����, ����������� � ���� ������������� ���������
			NodeZeroOP	*oldNode = nodeList.back();
			nodeList.back() = static_cast<NodeDereference*>(oldNode)->GetFirstNode();
			static_cast<NodeDereference*>(oldNode)->SetFirstNode(NULL);
			delete oldNode;
			// ������ ������ �������
			unsigned int typeSize = (nodeType->size - nodeType->paddingBytes) / nodeType->subType->size;
			// �������� ������ ���������, ������������ ��� ������������� �������
			// ����������� ������ �������� ���������� ���� � ����
			NodeExpressionList *listExpr = new NodeExpressionList(varInfo[i]->varType);
			// �������� ����, ������������ ����� - ������ �������
			nodeList.push_back(new NodeNumber<int>(typeSize, typeInt));
			// ������� � ������
			listExpr->AddNode();
			// ������� ������ � ������ �����
			nodeList.push_back(listExpr);
		}
	}
	// ���� ���������� ������������� �������, �� ������ ��������� �� ��
	if(nodeList.back()->GetNodeType() == typeNodeFuncDef ||
		(nodeList.back()->GetNodeType() == typeNodeExpressionList && static_cast<NodeExpressionList*>(nodeList.back())->GetFirstNode()->GetNodeType() == typeNodeFuncDef))
	{
		NodeFuncDef*	funcDefNode = (NodeFuncDef*)(nodeList.back()->GetNodeType() == typeNodeFuncDef ? nodeList.back() : static_cast<NodeExpressionList*>(nodeList.back())->GetFirstNode());
		AddGetAddressNode(funcDefNode->GetFuncInfo()->name.c_str(), funcDefNode->GetFuncInfo()->name.c_str() + funcDefNode->GetFuncInfo()->name.length());
		currTypes.pop_back();
		unifyTwo = true;
		realCurrType = nodeList.back()->GetTypeInfo();
		varDefined = true;
		varTop -= realCurrType->size;
	}

	// ���������� ����������, �� ������� ���� ��������� ���� ����������
	unsigned int varSizeAdd = offsetBytes;	// �� ���������, ��� ������ ������������� �����
	offsetBytes = 0;	// ������� ����� �� ����������
	// ���� ��� �� ��� ����� ��������, ������ � ������� ���������� ���������� ������������ �� ���� �����������
	if(!currTypes.back())
	{
		// ���� ������������ �� ��������� ��� ���� �������� ������ �� ����� ���� (��� ������������)
		// ��� ���� ������������ ������� �������������
		if(realCurrType->alignBytes != 0 || currAlign != TypeInfo::UNSPECIFIED_ALIGNMENT)
		{
			// �������� ������������. ��������� ������������� ����� ������� ���������, ��� ������������ �� ���������
			unsigned int activeAlign = currAlign != TypeInfo::UNSPECIFIED_ALIGNMENT ? currAlign : realCurrType->alignBytes;
			if(activeAlign > 16)
			{
				lastError = CompilerError("ERROR: alignment must me less than 16 bytes", s);
				supspi::Abort();
				return;
			}
			// ���� ��������� ������������ (���� ������������ noalign, � ����� ��� �� ��������)
			if(activeAlign != 0 && varTop % activeAlign != 0)
			{
				unsigned int offset = activeAlign - (varTop % activeAlign);
				varSizeAdd += offset;
				varInfo[i]->pos += offset;
				varTop += offset;
			}
		}
		varInfo[i]->varType = realCurrType;
		varTop += realCurrType->size;
	}
	varSizeAdd += !varInfo[i]->dataReserved ? realCurrType->size : 0;
	varInfo[i]->dataReserved = true;

	nodeList.push_back(new NodeGetAddress(varInfo[i], varInfo[i]->pos-(int)(varInfoTop.back().varStackSize), absAddress));

	nodeList.push_back(new NodeVariableSet(realCurrType, varSizeAdd, false));

	if(unifyTwo)
	{
		nodeList.push_back(new NodeExpressionList(nodeList.back()->GetTypeInfo()));
		NodeZeroOP* temp = nodeList.back();
		nodeList.pop_back();
		static_cast<NodeExpressionList*>(temp)->AddNode();
		nodeList.push_back(temp);
	}
}

void AddSetVariableNode(char const* s, char const* e)
{
	lastKnownStartPos = s;

	TypeInfo *realCurrType = currTypes.back();
	bool unifyTwo = false;
	if(realCurrType->arrSize == TypeInfo::UNSIZED_ARRAY && realCurrType != nodeList.back()->GetTypeInfo())
	{
		TypeInfo *nodeType = nodeList.back()->GetTypeInfo();
		if(realCurrType->subType == nodeType->subType)
		{
			if(nodeList.back()->GetNodeType() != typeNodeDereference)
			{
				if(nodeList.back()->GetNodeType() == typeNodeExpressionList)
				{
					AddInplaceArray(s, e);
					currTypes.pop_back();
					unifyTwo = true;
				}else{
					lastError = CompilerError("ERROR: cannot convert from " + nodeList.back()->GetTypeInfo()->GetTypeName() + " to " + realCurrType->GetTypeName(), s);
					supspi::Abort();
					return;
				}
			}
			NodeZeroOP	*oldNode = nodeList.back();
			nodeList.back() = static_cast<NodeDereference*>(oldNode)->GetFirstNode();
			static_cast<NodeDereference*>(oldNode)->SetFirstNode(NULL);
			delete oldNode;

			unsigned int typeSize = (nodeType->size - nodeType->paddingBytes) / nodeType->subType->size;
			NodeExpressionList *listExpr = new NodeExpressionList(realCurrType);
			nodeList.push_back(new NodeNumber<int>(typeSize, typeInt));
			listExpr->AddNode();
			nodeList.push_back(listExpr);

			if(unifyTwo)
				std::swap(nodeList[nodeList.size()-2], nodeList[nodeList.size()-3]);
		}
	}
	if(nodeList.back()->GetNodeType() == typeNodeFuncDef ||
		(nodeList.back()->GetNodeType() == typeNodeExpressionList && static_cast<NodeExpressionList*>(nodeList.back())->GetFirstNode()->GetNodeType() == typeNodeFuncDef))
	{
		NodeFuncDef*	funcDefNode = (NodeFuncDef*)(nodeList.back()->GetNodeType() == typeNodeFuncDef ? nodeList.back() : static_cast<NodeExpressionList*>(nodeList.back())->GetFirstNode());
		AddGetAddressNode(funcDefNode->GetFuncInfo()->name.c_str(), funcDefNode->GetFuncInfo()->name.c_str() + funcDefNode->GetFuncInfo()->name.length());
		currTypes.pop_back();
		unifyTwo = true;
		std::swap(nodeList[nodeList.size()-2], nodeList[nodeList.size()-3]);
	}

	nodeList.push_back(new NodeVariableSet(currTypes.back(), 0, true));
	if(!lastError.IsEmpty())
	{
		supspi::Abort();
		return;
	}

	if(unifyTwo)
	{
		nodeList.push_back(new NodeExpressionList(nodeList.back()->GetTypeInfo()));
		NodeZeroOP* temp = nodeList.back();
		nodeList.pop_back();
		static_cast<NodeExpressionList*>(temp)->AddNode();
		nodeList.push_back(temp);
	}
}

void AddGetVariableNode(char const* s, char const* e)
{
	(void)e;	// C4100
	lastKnownStartPos = s;

	if(nodeList.back()->GetTypeInfo()->funcType == NULL)
		nodeList.push_back(new NodeDereference(currTypes.back()));
}

void AddMemberAccessNode(char const* s, char const* e)
{
	(void)e;	// C4100
	lastKnownStartPos = s;

	std::string memberName = strs.back();

	// ��, ��� ��������� ���������� � ������, ��� � ����������!
	TypeInfo *currType = currTypes.back();

	if(currType->refLevel == 1)
	{
		nodeList.push_back(new NodeDereference(currTypes.back()));
		currTypes.back() = GetDereferenceType(currTypes.back());
		if(!currTypes.back())
		{
			supspi::Abort();
			return;
		}
		currType = currTypes.back();
	}
 
	int fID = -1;
	int i = (int)currType->memberData.size()-1;
	while(i >= 0 && currType->memberData[i].name != memberName)
		i--;
	if(i == -1)
	{
		// ���� ������� �� �����
		for(int k = 0; k < (int)funcInfo.size(); k++)
		{
			if(funcInfo[k]->name == (currType->name + "::" + memberName) && funcInfo[k]->visible)
			{
				if(fID != -1)
				{
					lastError = CompilerError("ERROR: there are more than one '" + memberName + "' function, and the decision isn't clear", s);
					supspi::Abort();
					return;
				}
				fID = k;
			}
		}
		if(fID == -1)
		{
			lastError = CompilerError("ERROR: variable '" + memberName + "' is not a member of '" + currType->GetTypeName() + "'", s);
			supspi::Abort();
			return;
		}
	}
	
	if(fID == -1)
	{
		if(nodeList.back()->GetNodeType() == typeNodeGetAddress)
		{
			static_cast<NodeGetAddress*>(nodeList.back())->ShiftToMember(i);
		}else{
			nodeList.push_back(new NodeShiftAddress(currType->memberData[i].offset, currType->memberData[i].type));
		}
		currTypes.back() = currType->memberData[i].type;
	}else{
		// ������� ���� ��� ��������� ��������� �� �������
		nodeList.push_back(new NodeFunctionAddress(funcInfo[fID]));

		currTypes.back() = funcInfo[fID]->funcType;
	}

	strs.pop_back();
}

void AddMemberFunctionCall(char const* s, char const* e)
{
	strs.back() = currTypes.back()->name + "::" + strs.back();
	addFuncCallNode(s, e);
	currTypes.back() = nodeList.back()->GetTypeInfo();
}

void AddPreOrPostOpNode(bool isInc, bool prefixOp)
{
	nodeList.push_back(new NodePreOrPostOp(currTypes.back(), isInc, prefixOp));
}

struct AddPreOrPostOp
{
	AddPreOrPostOp(bool isInc, bool isPrefixOp)
	{
		incOp = isInc;
		prefixOp = isPrefixOp;
	}

	void operator() (char const* s, char const* e)
	{
		(void)e;	// C4100
		lastKnownStartPos = s;
		AddPreOrPostOpNode(incOp, prefixOp);
	}
	bool incOp;
	bool prefixOp;
};

void AddModifyVariableNode(char const* s, char const* e, CmdID cmd)
{
	lastKnownStartPos = s;
	(void)e;	// C4100
	TypeInfo *targetType = GetDereferenceType(nodeList[nodeList.size()-2]->GetTypeInfo());
	if(!targetType)
	{
		supspi::Abort();
		return;
	}
	nodeList.push_back(new NodeVariableModify(targetType, cmd));
}

template<CmdID cmd>
struct AddModifyVariable
{
	void operator() (char const* s, char const* e)
	{
		AddModifyVariableNode(s, e, cmd);
	}
};

void AddInplaceArray(char const* s, char const* e)
{
	char asString[16];
	sprintf(asString, "%d", inplaceArrayNum++);
	strs.push_back("$carr");
	strs.back() += asString;

	TypeInfo *saveCurrType = currType;
	bool saveVarDefined = varDefined;

	currType = NULL;
	addVar(s, e);

	AddDefineVariableNode(s, e);
	addPopNode(s, e);
	currTypes.pop_back();

	AddGetAddressNode(strs.back().c_str(), strs.back().c_str()+strs.back().length());
	AddGetVariableNode(s, e);

	varDefined = saveVarDefined;
	currType = saveCurrType;
	strs.pop_back();
}

//////////////////////////////////////////////////////////////////////////
void addOneExprNode(char const* s, char const* e)
{
	(void)s; (void)e;	// C4100
	nodeList.push_back(new NodeExpressionList());
}
void addTwoExprNode(char const* s, char const* e)
{
	if(nodeList.back()->GetNodeType() != typeNodeExpressionList)
		addOneExprNode(s, e);
	// Take the expression list from the top
	NodeZeroOP* temp = nodeList.back();
	nodeList.pop_back();
	static_cast<NodeExpressionList*>(temp)->AddNode();
	nodeList.push_back(temp);
}

std::vector<unsigned int> arrElementCount;

void addArrayConstructor(char const* s, char const* e)
{
	arrElementCount.back()++;

	TypeInfo *currType = nodeList[nodeList.size()-arrElementCount.back()]->GetTypeInfo();

	if(currType == typeShort || currType == typeChar)
	{
		currType = typeInt;
		warningLog.append("WARNING: short and char will be promoted to int during array construction\r\n At ");
		warningLog.append(s, e);
		warningLog.append("\r\n");
	}
	if(currType == typeFloat)
	{
		currType = typeDouble;
		warningLog.append("WARNING: float will be promoted to double during array construction\r\n At ");
		warningLog.append(s, e);
		warningLog.append("\r\n");
	}
	if(currType == typeVoid)
	{
		lastError = CompilerError("ERROR: array cannot be constructed from void type elements", s);
		supspi::Abort();
		return;
	}

	nodeList.push_back(new NodeZeroOP());
	nodeList.push_back(new NodeNumber<int>(arrElementCount.back(), typeInt));
	TypeInfo *targetType = GetArrayType(currType);
	if(!targetType)
	{
		supspi::Abort();
		return;
	}
	nodeList.push_back(new NodeExpressionList(targetType));

	NodeZeroOP* temp = nodeList.back();
	nodeList.pop_back();

	NodeExpressionList *arrayList = static_cast<NodeExpressionList*>(temp);

	TypeInfo *realType = nodeList.back()->GetTypeInfo();
	for(unsigned int i = 0; i < arrElementCount.back(); i++)
	{
		if(realType != currType && !((realType == typeShort || realType == typeChar) && currType == typeInt) && !(realType == typeFloat && currType == typeDouble))
		{
			char tempStr[16];
			sprintf(tempStr, "%d", arrElementCount.back()-i-1);
			lastError = CompilerError(std::string("ERROR: element ") + tempStr + " doesn't match the type of element 0 (" + currType->GetTypeName() + ")", s);
			supspi::Abort();
			return;
		}
		arrayList->AddNode(false);
	}

	nodeList.push_back(temp);

	arrElementCount.pop_back();
}

void FunctionAdd(char const* s, char const* e)
{
	(void)e;	// C4100
	for(unsigned int i = varInfoTop.back().activeVarCnt; i < varInfo.size(); i++)
	{
		if(varInfo[i]->name == strs.back())
		{
			lastError = CompilerError("ERROR: Name '" + strs.back() + "' is already taken for a variable in current scope", s);
			supspi::Abort();
			return;
		}
	}
	std::string name = strs.back();
	if(name == "if" || name == "else" || name == "for" || name == "while" || name == "return" || name=="switch" || name=="case")
	{
		lastError = CompilerError("ERROR: The name '" + name + "' is reserved", s);
		supspi::Abort();
		return;
	}
	if(!currType)
	{
		lastError = CompilerError("ERROR: function return type cannot be auto", s);
		supspi::Abort();
		return;
	}
	funcInfo.push_back(new FunctionInfo());
	funcInfo.back()->name = name;
	funcInfo.back()->vTopSize = (unsigned int)varInfoTop.size();
	retTypeStack.push_back(currType);
	funcInfo.back()->retType = currType;
	if(newType)
		funcInfo.back()->type = FunctionInfo::THISCALL;
	if(newType ? varInfoTop.size() > 2 : varInfoTop.size() > 1)
		funcInfo.back()->type = FunctionInfo::LOCAL;
	currDefinedFunc.push_back(funcInfo.back());

	if(newType)
		funcInfo.back()->name = newType->name + "::" + name;

	funcInfo.back()->nameHash = GetStringHash(funcInfo.back()->name.c_str());

	if(varDefined && varInfo.back()->varType == NULL)
		varTop += 8;
}

void FunctionParam(char const* s, char const* e)
{
	(void)e;	// C4100
	if(!currType)
	{
		lastError = CompilerError("ERROR: function parameter cannot be an auto type", s);
		supspi::Abort();
		return;
	}
	funcInfo.back()->params.push_back(VariableInfo(strs.back(), 0, currType, currValConst));
	funcInfo.back()->allParamSize += currType->size;
	strs.pop_back();
}
void FunctionStart(char const* s, char const* e)
{
	(void)s; (void)e;	// C4100
	varInfoTop.push_back(VarTopInfo((unsigned int)varInfo.size(), varTop));

	for(int i = (int)funcInfo.back()->params.size()-1; i >= 0; i--)
	{
		strs.push_back(funcInfo.back()->params[i].name);
		
		currValConst = funcInfo.back()->params[i].isConst;
		currType = funcInfo.back()->params[i].varType;
		currAlign = 1;
		addVar(0,0);
		varDefined = false;

		strs.pop_back();
	}

	strs.push_back("$" + funcInfo.back()->name + "_ext");
	currType = GetReferenceType(typeInt);
	currAlign = 1;
	addVar(0, 0);
	varDefined = false;
	strs.pop_back();

	funcInfo.back()->funcType = GetFunctionType(funcInfo.back());
}
void FunctionEnd(char const* s, char const* e)
{
	FunctionInfo &lastFunc = *currDefinedFunc.back();

	std::string fName = strs.back();
	if(newType)
	{
		fName = newType->name + "::" + fName;
	}

	int i = (int)funcInfo.size()-1;
	while(i >= 0 && funcInfo[i]->name != fName)
		i--;

	// Find all the functions with the same name
	//int count = 0;
	for(int n = 0; n < i; n++)
	{
		if(funcInfo[n]->name == funcInfo[i]->name && funcInfo[n]->params.size() == funcInfo[i]->params.size() && funcInfo[n]->visible)
		{
			// Check all parameter types
			bool paramsEqual = true;
			for(unsigned int k = 0; k < funcInfo[i]->params.size(); k++)
			{
				if(funcInfo[n]->params[k].varType->GetTypeName() != funcInfo[i]->params[k].varType->GetTypeName())
					paramsEqual = false;
			}
			if(paramsEqual)
			{
				lastError = CompilerError("ERROR: function '" + funcInfo[i]->name + "' is being defined with the same set of parameters", s);
				supspi::Abort();
				return;
			}
		}
	}

	unsigned int varFormerTop = varTop;
	while(varInfo.size() > varInfoTop.back().activeVarCnt)
		varInfo.pop_back();
	varTop = varInfoTop.back().varStackSize;
	varInfoTop.pop_back();
	nodeList.push_back(new NodeBlock(varFormerTop-varTop, false));

	nodeList.push_back(new NodeFuncDef(funcInfo[i]));
	funcDefList.push_back(nodeList.back());
	strs.pop_back();

	retTypeStack.pop_back();
	currDefinedFunc.pop_back();

	// If function is local, create function parameters block
	if(lastFunc.type == FunctionInfo::LOCAL && !lastFunc.external.empty())
	{
		nodeList.push_back(new NodeZeroOP());
		nodeList.push_back(new NodeNumber<int>((int)lastFunc.external.size(), typeInt));
		TypeInfo *targetType = GetReferenceType(typeInt);
		if(!targetType)
		{
			supspi::Abort();
			return;
		}
		targetType = GetArrayType(targetType);
		if(!targetType)
		{
			supspi::Abort();
			return;
		}
		nodeList.push_back(new NodeExpressionList(targetType));

		NodeZeroOP* temp = nodeList.back();
		nodeList.pop_back();

		NodeExpressionList *arrayList = static_cast<NodeExpressionList*>(temp);

		for(unsigned int n = 0; n < lastFunc.external.size(); n++)
		{
			const char *s = lastFunc.external[n].c_str(), *e = s + lastFunc.external[n].length();
			AddGetAddressNode(s, e);
			currTypes.pop_back();
			arrayList->AddNode();
		}
		nodeList.push_back(temp);

		strs.push_back("$" + lastFunc.name + "_ext");

		TypeInfo *saveCurrType = currType;
		bool saveVarDefined = varDefined;

		currType = NULL;
		addVar(s, e);

		AddDefineVariableNode(s, e);
		addPopNode(s, e);
		currTypes.pop_back();

		varDefined = saveVarDefined;
		currType = saveCurrType;
		strs.pop_back();
		addTwoExprNode(0,0);
	}

	if(newType)
	{
		newType->memberFunctions.push_back(TypeInfo::MemberFunction());
		TypeInfo::MemberFunction &clFunc = newType->memberFunctions.back();
		clFunc.name = lastFunc.name;
		clFunc.func = &lastFunc;
		clFunc.defNode = nodeList.back();
	}
}

void addFuncCallNode(char const* s, char const* e)
{
	string fname = strs.back();
	strs.pop_back();

	// Searching, if fname is actually a variable name (which means, either it is a pointer to function, or an error)
	int vID = (int)varInfo.size()-1;
	while(vID >= 0 && varInfo[vID]->name != fname)
		vID--;

	FunctionInfo	*fInfo = NULL;
	FunctionType	*fType = NULL;

	if(vID == -1)
	{
		//Find all functions with given name
		FunctionInfo *fList[32];
		unsigned int	fRating[32];
		memset(fRating, 0, 32*4);

		unsigned int count = 0;
		for(int k = 0; k < (int)funcInfo.size(); k++)
			if(funcInfo[k]->name == fname && funcInfo[k]->visible)
				fList[count++] = funcInfo[k];
		if(count == 0)
		{
			lastError = CompilerError("ERROR: function '" + fname + "' is undefined", s);
			supspi::Abort();
			return;
		}
		// Find the best suited function
		unsigned int minRating = 1024*1024;
		unsigned int minRatingIndex = (unsigned int)~0;
		for(unsigned int k = 0; k < count; k++)
		{
			if(fList[k]->params.size() != callArgCount.back())
			{
				fRating[k] += 65000;	// Definitely, this isn't the function we are trying to call. Parameter count does not match.
				continue;
			}
			for(unsigned int n = 0; n < fList[k]->params.size(); n++)
			{
				NodeZeroOP* activeNode = nodeList[nodeList.size()-fList[k]->params.size()+n];
				TypeInfo *paramType = activeNode->GetTypeInfo();
				unsigned int	nodeType = activeNode->GetNodeType();
				TypeInfo *expectedType = fList[k]->params[n].varType;
				if(expectedType != paramType)
				{
					if(expectedType->arrSize == TypeInfo::UNSIZED_ARRAY && paramType->arrSize != 0 && paramType->subType == expectedType->subType)
						fRating[k] += 5;
					else if(expectedType->funcType != NULL && nodeType == typeNodeFuncDef ||
							(nodeType == typeNodeExpressionList && static_cast<NodeExpressionList*>(activeNode)->GetFirstNode()->GetNodeType() == typeNodeFuncDef))
						fRating[k] += 5;
					else if(expectedType->type == TypeInfo::TYPE_COMPLEX)
						fRating[k] += 65000;	// Definitely, this isn't the function we are trying to call. Function excepts different complex type.
					else if(paramType->type == TypeInfo::TYPE_COMPLEX)
						fRating[k] += 65000;	// Again. Function excepts complex type, and all we have is simple type (cause previous condition failed).
					else if(paramType->subType != expectedType->subType)
						fRating[k] += 65000;	// Pointer or array with a different types inside. Doesn't matter if simple or complex.
					else	// Build-in types can convert to each other, but the fact of conversion tells us, that there could be a better suited function
						fRating[k] += 1;
				}
			}
			if(fRating[k] < minRating)
			{
				minRating = fRating[k];
				minRatingIndex = k;
			}
		}
		// Maybe the function we found can't be used at all
		if(minRating > 1000)
		{
			std::string errTemp;
			errTemp.append("ERROR: can't find function '");
			errTemp.append(fname);
			errTemp.append("' with following parameters:\r\n  ");
			errTemp.append(fname);
			errTemp.append("(");
			for(unsigned int n = 0; n < callArgCount.back(); n++)
			{
				errTemp.append(nodeList[nodeList.size()-callArgCount.back()+n]->GetTypeInfo()->GetTypeName());
				errTemp.append(n != callArgCount.back()-1 ? ", " : "");
			}
			errTemp.append(")\r\n");
			errTemp.append(" the only available are:\r\n");
			for(unsigned int n = 0; n < count; n++)
			{
				errTemp.append("  ");
				errTemp.append(fname);
				errTemp.append("(");
				for(unsigned int m = 0; m < fList[n]->params.size(); m++)
				{
					errTemp.append(fList[n]->params[m].varType->GetTypeName());
					errTemp.append(m != fList[n]->params.size()-1 ? ", " : "");
				}
				errTemp.append(")\r\n");
			}
			lastError = CompilerError(errTemp.c_str(), s);
			supspi::Abort();
			return;
		}
		// Check, is there are more than one function, that share the same rating
		for(unsigned int k = 0; k < count; k++)
		{
			if(k != minRatingIndex && fRating[k] == minRating)
			{
				std::string errTemp;
				errTemp.append("ERROR: ambiguity, there is more than one overloaded function available for the call.\r\n");
				errTemp.append("  ");
				errTemp.append(fname);
				errTemp.append("(");
				for(unsigned int n = 0; n < callArgCount.back(); n++)
				{
					errTemp.append(nodeList[nodeList.size()-callArgCount.back()+n]->GetTypeInfo()->GetTypeName());
					errTemp.append(n != callArgCount.back()-1 ? ", " : "");
				}
				errTemp.append(")\r\n");
				errTemp.append(" candidates are:\r\n");
				for(unsigned int n = 0; n < count; n++)
				{
					if(fRating[n] != minRating)
						continue;
					errTemp.append("  ");
					errTemp.append(fname);
					errTemp.append("(");
					for(unsigned int m = 0; m < fList[n]->params.size(); m++)
					{
						errTemp.append(fList[n]->params[m].varType->GetTypeName());
						errTemp.append(m != fList[n]->params.size()-1 ? ", " : "");
					}
					errTemp.append(")\r\n");
				}
				lastError = CompilerError(errTemp.c_str(), s);
				supspi::Abort();
				return;
			}
		}
		fType = fList[minRatingIndex]->funcType->funcType;
		fInfo = fList[minRatingIndex];
	}else{
		AddGetAddressNode(fname.c_str(), fname.length()+fname.c_str());
		AddGetVariableNode(s, e);
		fType = nodeList.back()->GetTypeInfo()->funcType;
	}

	vector<NodeZeroOP*> paramNodes;
	for(unsigned int i = 0; i < fType->paramType.size(); i++)
	{
		paramNodes.push_back(nodeList.back());
		nodeList.pop_back();
	}
	vector<NodeZeroOP*> inplaceArray;

	for(unsigned int i = 0; i < fType->paramType.size(); i++)
	{
		unsigned int index = (unsigned int)(fType->paramType.size()) - i - 1;

		TypeInfo *expectedType = fType->paramType[i];
		TypeInfo *realType = paramNodes[index]->GetTypeInfo();
		
		if(paramNodes[index]->GetNodeType() == typeNodeFuncDef ||
			(paramNodes[index]->GetNodeType() == typeNodeExpressionList && static_cast<NodeExpressionList*>(paramNodes[index])->GetFirstNode()->GetNodeType() == typeNodeFuncDef))
		{
			NodeFuncDef*	funcDefNode = (NodeFuncDef*)(paramNodes[index]->GetNodeType() == typeNodeFuncDef ? paramNodes[index] : static_cast<NodeExpressionList*>(paramNodes[index])->GetFirstNode());
			AddGetAddressNode(funcDefNode->GetFuncInfo()->name.c_str(), funcDefNode->GetFuncInfo()->name.c_str() + funcDefNode->GetFuncInfo()->name.length());
			currTypes.pop_back();

			NodeExpressionList* listExpr = new NodeExpressionList(paramNodes[index]->GetTypeInfo());
			nodeList.push_back(paramNodes[index]);
			listExpr->AddNode();
			nodeList.push_back(listExpr);
		}else if(expectedType->arrSize == TypeInfo::UNSIZED_ARRAY && expectedType->subType == realType->subType && expectedType != realType){
			if(paramNodes[index]->GetNodeType() != typeNodeDereference)
			{
				if(paramNodes[index]->GetNodeType() == typeNodeExpressionList)
				{
					nodeList.push_back(paramNodes[index]);
					AddInplaceArray(s, e);

					paramNodes[index] = nodeList.back();
					nodeList.pop_back();
					inplaceArray.push_back(nodeList.back());
					nodeList.pop_back();
				}else{
					char chTemp[16];
					sprintf(chTemp, "%d", i);
					lastError = CompilerError(std::string("ERROR: array expected as a parameter ") + chTemp, s);
					supspi::Abort();
					return;
				}
			}
			unsigned int typeSize = (paramNodes[index]->GetTypeInfo()->size - paramNodes[index]->GetTypeInfo()->paddingBytes) / paramNodes[index]->GetTypeInfo()->subType->size;
			nodeList.push_back(static_cast<NodeDereference*>(paramNodes[index])->GetFirstNode());
			static_cast<NodeDereference*>(paramNodes[index])->SetFirstNode(NULL);
			delete paramNodes[index];
			paramNodes[index] = NULL;
			NodeExpressionList *listExpr = new NodeExpressionList(varInfo[i]->varType);
			nodeList.push_back(new NodeNumber<int>(typeSize, typeInt));
			listExpr->AddNode();
			nodeList.push_back(listExpr);
		}else{
			nodeList.push_back(paramNodes[index]);
		}
	}

	if(fInfo && (fInfo->type == FunctionInfo::LOCAL))
	{
		std::string bName = "$" + fInfo->name + "_ext";
		int i = (int)varInfo.size()-1;
		while(i >= 0 && varInfo[i]->name != bName)
			i--;
		if(i == -1)
		{
			nodeList.push_back(new NodeNumber<int>(0, GetReferenceType(typeInt)));
		}else{
			AddGetAddressNode(bName.c_str(), bName.c_str()+bName.length());
			if(currTypes.back()->refLevel == 1)
				AddDereferenceNode(s, e);
			currTypes.pop_back();
		}
	}

	if(!fInfo)
		currTypes.pop_back();

	nodeList.push_back(new NodeFuncCall(fInfo, fType));

	if(inplaceArray.size() > 0)
	{
		NodeZeroOP* temp = nodeList.back();
		nodeList.pop_back();
		for(unsigned int i = 0; i < inplaceArray.size(); i++)
			nodeList.push_back(inplaceArray[i]);
		nodeList.push_back(temp);

		nodeList.push_back(new NodeExpressionList(temp->GetTypeInfo()));
		for(unsigned int i = 0; i < inplaceArray.size(); i++)
			addTwoExprNode(s, e);
	}
	callArgCount.pop_back();
}

void addIfNode(char const* s, char const* e)
{
	(void)s; (void)e;	// C4100
	nodeList.push_back(new NodeIfElseExpr(false));
}
void addIfElseNode(char const* s, char const* e)
{
	(void)s; (void)e;	// C4100
	nodeList.push_back(new NodeIfElseExpr(true));
}
void addIfElseTermNode(char const* s, char const* e)
{
	(void)e;	// C4100
	TypeInfo* typeA = nodeList[nodeList.size()-1]->GetTypeInfo();
	TypeInfo* typeB = nodeList[nodeList.size()-2]->GetTypeInfo();
	if(typeA != typeB)
	{
		lastError = CompilerError("ERROR: trinary operator ?: \r\n result types are not equal (" + typeB->name + " : " + typeA->name + ")", s);
		supspi::Abort();
		return;
	}
	nodeList.push_back(new NodeIfElseExpr(true, true));
}

void saveVarTop(char const* s, char const* e)
{
	(void)s; (void)e;	// C4100
	cycleBeginVarTop.push_back((unsigned int)varInfoTop.size());
}
void addForNode(char const* s, char const* e)
{
	(void)s; (void)e;	// C4100
	nodeList.push_back(new NodeForExpr());
	cycleBeginVarTop.pop_back();
}
void addWhileNode(char const* s, char const* e)
{
	(void)s; (void)e;	// C4100
	nodeList.push_back(new NodeWhileExpr());
	cycleBeginVarTop.pop_back();
}
void addDoWhileNode(char const* s, char const* e)
{
	(void)s; (void)e;	// C4100
	nodeList.push_back(new NodeDoWhileExpr());
	cycleBeginVarTop.pop_back();
}

void preSwitchNode(char const* s, char const* e)
{
	(void)s; (void)e;	// C4100
	cycleBeginVarTop.push_back((unsigned int)varInfoTop.size());
	varInfoTop.push_back(VarTopInfo((unsigned int)varInfo.size(), varTop));
	nodeList.push_back(new NodeSwitchExpr());
}
void addCaseNode(char const* s, char const* e)
{
	(void)s; (void)e;	// C4100
	NodeZeroOP* temp = nodeList[nodeList.size()-3];
	static_cast<NodeSwitchExpr*>(temp)->AddCase();
}
void addSwitchNode(char const* s, char const* e)
{
	(void)s; (void)e;	// C4100
	cycleBeginVarTop.pop_back();
	while(varInfo.size() > varInfoTop.back().activeVarCnt)
	{
		varTop--;
		varInfo.pop_back();
	}
	varInfoTop.pop_back();
}

void TypeBegin(char const* s, char const* e)
{
	if(newType)
	{
		lastError = CompilerError("ERROR: Different type is being defined", s);
		supspi::Abort();
		return;
	}
	if((int)currAlign < 0)
	{
		lastError = CompilerError("ERROR: alignment must be a positive number", s);
		supspi::Abort();
		return;
	}
	if(currAlign > 16)
	{
		lastError = CompilerError("ERROR: alignment must me less than 16 bytes", s);
		supspi::Abort();
		return;
	}
	newType = new TypeInfo();
	newType->name = std::string(s, e);
	newType->type = TypeInfo::TYPE_COMPLEX;
	newType->alignBytes = currAlign;
	currAlign = TypeInfo::UNSPECIFIED_ALIGNMENT;

	typeInfo.push_back(newType);
	
	varInfoTop.push_back(VarTopInfo((unsigned int)varInfo.size(), varTop));
}

void TypeAddMember(char const* s, char const* e)
{
	if(!currType)
	{
		lastError = CompilerError("ERROR: auto cannot be used for class members", s);
		supspi::Abort();
		return;
	}
	newType->AddMember(std::string(s, e), currType);

	strs.push_back(std::string(s, e));
	addVar(0,0);
	strs.pop_back();
}

void TypeFinish(char const* s, char const* e)
{
	(void)s; (void)e;	// C4100
	if(newType->size % 4 != 0)
	{
		newType->paddingBytes = 4 - (newType->size % 4);
		newType->size += 4 - (newType->size % 4);
	}

	nodeList.push_back(new NodeZeroOP());
	for(unsigned int i = 0; i < newType->memberFunctions.size(); i++)
		addTwoExprNode(0,0);

	newType = NULL;

	while(varInfo.size() > varInfoTop.back().activeVarCnt)
		varInfo.pop_back();
	varTop = varInfoTop.back().varStackSize;
	varInfoTop.pop_back();
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void addUnfixedArraySize(char const*s, char const*e)
{
	(void)s; (void)e;	// C4100
	nodeList.push_back(new NodeNumber<int>(1, typeVoid));
}

namespace CompilerGrammar
{
	// �������, �������� � ��������� ������ �� ����� �����
	// ���� ����� ����� �������������� ��� �������� ��������� �������� ����� ������� ����������
	// �������� ��� ������� ������������� ���������� a[i], ����� ��������� "a" � ����,
	// ������ ��� � ������� ��������� "a[i]" �������
	void ParseStrPush(char const *s, char const *e)
	{
		strs.push_back(string(s,e));
	}
	void ParseStrPop(char const *s, char const *e)
	{
		(void)s; (void)e;	// C4100
		strs.pop_back();
	}
	void ParseStrCopy(char const *s, char const *e)
	{
		(void)s; (void)e;	// C4100
		strs.push_back(*(strs.end()-2));
	}
	void ParseStrAdd(char const *s, char const *e)
	{
		strs.back() += std::string(s, e);
	}

	// ��� ������� ����������, ����� ��������� ������ ���� � ����, ������� ��� �����������
	void SetStringToLastNode(char const *s, char const *e)
	{
		nodeList.back()->SetCodeInfo(s, e);
	}
	struct StringIndex
	{
		StringIndex(char const *s, char const *e)
		{
			indexS = s;
			indexE = e;
		}
		const char *indexS, *indexE;
	};
	vector<StringIndex> sIndexes;
	void SaveStringIndex(char const *s, char const *e)
	{
		sIndexes.push_back(StringIndex(s, e));
	}
	void SetStringFromIndex(char const *s, char const *e)
	{
		(void)s; (void)e;	// C4100
		nodeList.back()->SetCodeInfo(sIndexes.back().indexS, sIndexes.back().indexE);
		sIndexes.pop_back();
	}

	// Callbacks
	typedef void (*parserCallback)(char const*, char const*);
	parserCallback addChar, addInt, addFloat, addLong, addDouble;
	parserCallback strPush, strPop, strCopy;

	class ThrowError
	{
	public:
		ThrowError(): err(NULL){ }
		ThrowError(const char* str): err(str){ }

		void operator() (char const* s, char const* e)
		{
			(void)e;	// C4100
			assert(err);
			lastError = CompilerError(err, s);
			supspi::Abort();
		}
	private:
		const char* err;
	};
	class TypeNameP: public BaseP
	{
	public:
		TypeNameP(Rule a): m_a(a.getPtr()){ }
		virtual ~TypeNameP(){ }

		virtual bool	Parse(char** str, BaseP* space)
		{
			SkipSpaces(str, space);
			char* curr = *str;
			m_a->Parse(str, NULL);
			if(curr == *str)
				return false;
			std::string type(curr, *str);
			for(unsigned int i = 0; i < typeInfo.size(); i++)
				if(typeInfo[i]->name == type)
					return true;
			return false;
		}
	protected:
		Rule m_a;
	};
	class MySpaceP: public BaseP
	{
	public:
		MySpaceP(){ }
		virtual ~MySpaceP(){ }

		virtual bool	Parse(char** str, BaseP* space)
		{
			(void)space;
			for(;;)
			{
				while((unsigned char)((*str)[0] - 1) < ' ')
					(*str)++;
				if((*str)[0] == '/'){
					if((*str)[1] == '/')
					{
						while((*str)[0] != '\n' && (*str)[0] != '\0')
							(*str)++;
					}else if((*str)[1] == '*'){
						while(!((*str)[0] == '*' && (*str)[1] == '/') && (*str)[0] != '\0')
							(*str)++;
						(*str) += 2;
					}else{
						break;
					}
				}else{
					break;
				}
			}
			return true;
		}
	protected:
	};
	Rule	typenameP(Rule a){ return Rule(new TypeNameP(a)); }
	Rule	myspaceP(){ return Rule(new MySpaceP()); }
	Rule	strWP(char* str){ return (lexemeD[strP(str) >> (epsP - alnumP)]); }

	class Grammar
	{
		bool  ParseNumber(char** str, BaseP* space)
		{
			SkipSpaces(str, space);
			char* start = *str;
			if(start[0] == '0' && start[1] == 'x')	// hexadecimal
			{
				*str += 2;
				while(isDigit(**str) || ((unsigned char)(**str - 'a') < 6) || ((unsigned char)(**str - 'A') < 6))
					(*str)++;
				if(*str == start+2)
				{
					lastError = CompilerError("ERROR: '0x' must be followed by number", *str);
					supspi::Abort();
					return false;
				}
				addHexInt(start, *str);
				return true;
			}
			while(isDigit(**str))
				(*str)++;
			if(start == *str && **str != '.')
				return false;

			if(**str == 'b')
			{
				(*str)++;
				addBinInt(start, *str);
				return true;
			}else if(**str == 'l'){
				(*str)++;
				addLong(start, *str);
				return true;
			}else if(**str != '.' && start[0] == '0' && isDigit(start[1])){
				addOctInt(start, *str);
				return true;
			}else if(**str != '.' && **str != 'e'){
				addInt(start, *str);
				return true;
			}

			if(*str[0] == '.')
			{
				(*str)++;
				while(isDigit(*str[0]))
					(*str)++;
			}
			if(*str[0] == 'e')
			{
				(*str)++;
				if(*str[0] == '-')
					(*str)++;
				while(isDigit(*str[0]))
					(*str)++;
			}
			if(start[0] == '.' && (*str)-start == 1)
			{
				(*str) = start;
				return false;
			}
			if(**str == 'f')
			{
				(*str)++;
				addFloat(start, *str);
				return true;
			}else{
				addDouble(start, *str);
				return true;
			}
			return true;
		}
		class NumberP: public BaseP
		{
		public:
			NumberP(Grammar *gram): grammar(gram){ }
			virtual ~NumberP(){ }

			virtual bool	Parse(char** str, BaseP* space)
			{
				return grammar->ParseNumber(str, space);
			}
		protected:
			Grammar *grammar;
		};
		Rule	numberP(Grammar *grammar){ return Rule(new NumberP(grammar)); }
	public:
		void InitGrammar()
		{
			strPush	=	CompilerGrammar::ParseStrPush;
			strPop	=	CompilerGrammar::ParseStrPop;
			strCopy =	CompilerGrammar::ParseStrCopy;

			addChar		=	addNumberNode<char>;
			addInt		=	addNumberNode<int>;
			addFloat	=	addNumberNode<float>;
			addLong		=	addNumberNode<long long>;
			addDouble	=	addNumberNode<double>;

			arrayDef	=	('[' >> (term4_9 | epsP[addUnfixedArraySize]) >> ']' >> !arrayDef)[convertTypeToArray];
			seltype		=	((strP("auto") | typenameP(varname))[selType] | (strP("typeof") >> chP('(') >> ((varname[GetVariableType] >> chP(')')) | (term5[SetTypeOfLastNode] >> chP(')'))))) >> *((lexemeD[strP("ref") >> (~alnumP | nothingP)])[convertTypeToRef] | arrayDef);

			isconst		=	epsP[AssignVar<bool>(currValConst, false)] >> !strP("const")[AssignVar<bool>(currValConst, true)];
			varname		=	lexemeD[alphaP >> *(alnumP | '_')];

			classdef	=	((strP("align") >> '(' >> intP[StrToInt(currAlign)] >> ')') | (strP("noalign") | epsP)[AssignVar<unsigned int>(currAlign, 0)]) >>
							strP("class") >> varname[TypeBegin] >> chP('{') >>
							*(
								funcdef |
								(seltype >> varname[TypeAddMember] >> *(',' >> varname[TypeAddMember]) >> chP(';'))
							)
							>> chP('}')[TypeFinish];

			funccall	=	varname[strPush] >> 
					('(' | (epsP[strPop] >> nothingP)) >>
					epsP[PushBackVal<std::vector<unsigned int>, unsigned int>(callArgCount, 0)] >> 
					!(
					term5[ArrBackInc<std::vector<unsigned int> >(callArgCount)] >>
					*(',' >> term5[ArrBackInc<std::vector<unsigned int> >(callArgCount)])
					) >>
					(')' | epsP[ThrowError("ERROR: ')' not found after function call")]);

			funcvars	=	!(isconst >> seltype >> varname[strPush][FunctionParam]) >> *(',' >> isconst >> seltype >> varname[strPush][FunctionParam]);
			funcdef		=	seltype >> varname[strPush] >> (chP('(')[FunctionAdd] | (epsP[strPop] >> nothingP)) >>  funcvars[FunctionStart] >> chP(')') >> chP('{') >> (code | epsP[addVoidNode])[FunctionEnd] >> chP('}');
			funcProt	=	seltype >> varname[strPush] >> (chP('(')[FunctionAdd] | (epsP[strPop] >> nothingP)) >>  funcvars >> chP(')') >> chP(';');

			addvarp		=
				(
					varname[strPush] >>
					!('[' >> (term4_9 | epsP[addUnfixedArraySize]) >> ']')[convertTypeToArray]
				)[pushType][addVar] >>
				(('=' >> (term5 | epsP[ThrowError("ERROR: expression not found after '='")]))[AddDefineVariableNode][addPopNode][popType] | epsP[addVarDefNode])[popType][strPop];
			
			vardefsub	= addvarp[SetStringToLastNode] >> *(',' >> vardefsub)[addTwoExprNode];
			vardef		=
				epsP[AssignVar<unsigned int>(currAlign, 0xFFFFFFFF)] >>
				isconst >>
				!(strP("noalign")[AssignVar<unsigned int>(currAlign, 0)] | (strP("align") >> '(' >> intP[StrToInt(currAlign)] >> ')')) >>
				seltype >>
				vardefsub;

			ifexpr		=
				(
					strWP("if") >>
					('(' | epsP[ThrowError("ERROR: '(' not found after 'if'")]) >>
					(term5 | epsP[ThrowError("ERROR: condition not found in 'if' statement")]) >>
					(')' | epsP[ThrowError("ERROR: closing ')' not found after 'if' condition")])
				)[SaveStringIndex] >>
				expression >>
				((strP("else") >> expression)[addIfElseNode] | epsP[addIfNode])[SetStringFromIndex];
			forexpr		=
				(
					strWP("for")[saveVarTop] >>
					('(' | epsP[ThrowError("ERROR: '(' not found after 'for'")]) >>
					(
						(
							chP('{') >>
							(code | epsP[addVoidNode]) >>
							(chP('}') | epsP[ThrowError("ERROR: '}' not found after '{'")])
						) |
						vardef |
						term5[addPopNode] |
						epsP[addVoidNode]
					) >>
					(';' | epsP[ThrowError("ERROR: ';' not found after initializer in 'for'")]) >>
					(term5 | epsP[ThrowError("ERROR: condition not found in 'for' statement")]) >>
					(';' | epsP[ThrowError("ERROR: ';' not found after condition in 'for'")]) >>
					((chP('{') >> code >> chP('}')) | term5[addPopNode] | epsP[addVoidNode]) >>
					(')' | epsP[ThrowError("ERROR: ')' not found after 'for' statement")])
				)[SaveStringIndex] >> expression[addForNode][SetStringFromIndex];
			whileexpr	=
				strWP("while")[saveVarTop] >>
				(
					('(' | epsP[ThrowError("ERROR: '(' not found after 'while'")]) >>
					(term5 | epsP[ThrowError("ERROR: expression expected after 'while('")]) >>
					(')' | epsP[ThrowError("ERROR: closing ')' not found after expression in 'while' statement")])
				) >>
				(expression[addWhileNode] | epsP[ThrowError("ERROR: expression expected after 'while(...)'")]);
			doexpr		=	
				strWP("do")[saveVarTop] >> 
				(expression | epsP[ThrowError("ERROR: expression expected after 'do'")]) >> 
				(strP("while") | epsP[ThrowError("ERROR: 'while' expected after 'do' statement")]) >>
				(
					('(' | epsP[ThrowError("ERROR: '(' not found after 'while'")]) >> 
					(term5 | epsP[ThrowError("ERROR: expression not found after 'while('")]) >> 
					(')' | epsP[ThrowError("ERROR: closing ')' not found after expression in 'while' statement")])
				)[addDoWhileNode] >> 
				(';' | epsP[ThrowError("ERROR: while(...) should be followed by ';'")]);
			switchexpr	=
				strP("switch") >>
				('(') >>
				(term5 | epsP[ThrowError("ERROR: expression not found after 'switch('")])[preSwitchNode] >>
				(')' | epsP[ThrowError("ERROR: closing ')' not found after expression in 'switch' statement")]) >>
				('{' | epsP[ThrowError("ERROR: '{' not found after 'switch(...)'")]) >>
				(strWP("case") >> term5 >> ':' >> expression >> *expression[addTwoExprNode])[addCaseNode] >>
				*(strWP("case") >> term5 >> ':' >> expression >> *expression[addTwoExprNode])[addCaseNode] >>
				('}' | epsP[ThrowError("ERROR: '}' not found after 'switch' statement")])[addSwitchNode];

			retexpr		=
				(
					strWP("return") >>
					(term5 | epsP[addVoidNode]) >>
					(+chP(';') | epsP[ThrowError("ERROR: return must be followed by ';'")])
				)[addReturnNode];
			breakexpr	=	(
				strWP("break") >>
				(+chP(';') | epsP[ThrowError("ERROR: break must be followed by ';'")])
				)[addBreakNode];
			continueExpr	=
				(
					strWP("continue") >>
					(+chP(';') | epsP[ThrowError("ERROR: continue must be followed by ';'")])
				)[AddContinueNode];

			group		=	'(' >> term5 >> (')' | epsP[ThrowError("ERROR: closing ')' not found after '('")]);

			variable	= (chP('*') >> variable)[AddDereferenceNode] | (((varname - strP("case")) >> (~chP('(') | nothingP))[AddGetAddressNode] >> *postExpr);
			postExpr	=	('.' >> varname[strPush] >> (~chP('(') | (epsP[strPop] >> nothingP)))[AddMemberAccessNode] |
							('[' >> term5 >> ']')[AddArrayIndexNode];

			term1		=
				numberP(this) |
				(chP('&') >> variable)[popType] |
				(strP("--") >> variable[AddPreOrPostOp(false, true)])[popType] | 
				(strP("++") >> variable[AddPreOrPostOp(true, true)])[popType] |
				(+(chP('-')[IncVar<unsigned int>(negCount)]) >> term1)[addNegNode] |
				(+chP('+') >> term1) |
				('!' >> term1)[addLogNotNode] |
				('~' >> term1)[addBitNotNode] |
				(chP('\"') >> *(strP("\\\"") | (anycharP - chP('\"'))) >> chP('\"'))[strPush][addStringNode] |
				(chP('\'') >> ((chP('\\') >> anycharP) | anycharP) >> chP('\''))[addChar] |
				(strP("sizeof") >> chP('(')[pushType] >> (seltype[pushType][GetTypeSize][popType] | term5[AssignVar<bool>(sizeOfExpr, true)][GetTypeSize]) >> chP(')')[popType]) |
				(chP('{')[PushBackVal<std::vector<unsigned int>, unsigned int>(arrElementCount, 0)] >> term4_9 >> *(chP(',') >> term4_9[ArrBackInc<std::vector<unsigned int> >(arrElementCount)]) >> chP('}'))[addArrayConstructor] |
				group |
				funcdef |
				funccall[addFuncCallNode] |
				(variable >>
					(
						strP("++")[AddPreOrPostOp(true, false)] |
						strP("--")[AddPreOrPostOp(false, false)] |
						('.' >> funccall)[AddMemberFunctionCall] |
						epsP[AddGetVariableNode]
					)[popType]
				);

			term2		=	term1 >> *((strP("**") >> term1)[addCmd(cmdPow)]);
			term3		=	term2 >> *(('*' >> term2)[addCmd(cmdMul)] | ('/' >> term2)[addCmd(cmdDiv)] | ('%' >> term2)[addCmd(cmdMod)]);
			term4		=	term3 >> *(('+' >> term3)[addCmd(cmdAdd)] | ('-' >> term3)[addCmd(cmdSub)]);
			term4_1		=	term4 >> *((strP("<<") >> term4)[addCmd(cmdShl)] | (strP(">>") >> term4)[addCmd(cmdShr)]);
			term4_2		=	term4_1 >> *(('<' >> term4_1)[addCmd(cmdLess)] | ('>' >> term4_1)[addCmd(cmdGreater)] | (strP("<=") >> term4_1)[addCmd(cmdLEqual)] | (strP(">=") >> term4_1)[addCmd(cmdGEqual)]);
			term4_4		=	term4_2 >> *((strP("==") >> term4_2)[addCmd(cmdEqual)] | (strP("!=") >> term4_2)[addCmd(cmdNEqual)]);
			term4_6		=	term4_4 >> *(strP("&") >> (term4_4 | epsP[ThrowError("ERROR: expression not found after &")]))[addCmd(cmdBitAnd)];
			term4_65	=	term4_6 >> *(strP("^") >> (term4_6 | epsP[ThrowError("ERROR: expression not found after ^")]))[addCmd(cmdBitXor)];
			term4_7		=	term4_65 >> *(strP("|") >> (term4_65 | epsP[ThrowError("ERROR: expression not found after |")]))[addCmd(cmdBitOr)];
			term4_75	=	term4_7 >> *(strP("and") >> (term4_7 | epsP[ThrowError("ERROR: expression not found after and")]))[addCmd(cmdLogAnd)];
			term4_8		=	term4_75 >> *(strP("xor") >> (term4_75 | epsP[ThrowError("ERROR: expression not found after xor")]))[addCmd(cmdLogXor)];
			term4_85	=	term4_8 >> *(strP("or") >> (term4_8 | epsP[ThrowError("ERROR: expression not found after or")]))[addCmd(cmdLogOr)];
			term4_9		=	term4_85 >> !('?' >> term5 >> ':' >> term5)[addIfElseTermNode];
#ifdef _MSC_VER
#pragma warning(disable: 4709)
#endif
			term5		=	(!(seltype) >>
							variable >> (
							(strP("=") >> term5)[AddSetVariableNode][popType] |
							(strP("+=") >> (term5 | epsP[ThrowError("ERROR: expression not found after '+='")]))[AddModifyVariable<cmdAdd>()][popType] |
							(strP("-=") >> (term5 | epsP[ThrowError("ERROR: expression not found after '-='")]))[AddModifyVariable<cmdSub>()][popType] |
							(strP("*=") >> (term5 | epsP[ThrowError("ERROR: expression not found after '*='")]))[AddModifyVariable<cmdMul>()][popType] |
							(strP("/=") >> (term5 | epsP[ThrowError("ERROR: expression not found after '/='")]))[AddModifyVariable<cmdDiv>()][popType] |
							(strP("**=") >> (term5 | epsP[ThrowError("ERROR: expression not found after '**='")]))[AddModifyVariable<cmdPow>()][popType] |
							(epsP[FailedSetVariable][popType] >> nothingP))
							) |
							term4_9;
#ifdef _MSC_VER
#pragma warning(default: 4709)
#endif

			block		=	chP('{')[blockBegin] >> (code | epsP[ThrowError("ERROR: {} block cannot be empty")]) >> chP('}')[blockEnd];
			expression	=	*chP(';') >> (classdef | (vardef >> +chP(';')) | block | breakexpr | continueExpr | ifexpr | forexpr | whileexpr | doexpr | switchexpr | retexpr | (term5 >> (+chP(';')  | epsP[ThrowError("ERROR: ';' not found after expression")]))[addPopNode]);
			code		=	((funcdef | expression) >> (code[addTwoExprNode] | epsP[addOneExprNode]));
		
			mySpaceP = myspaceP();
		}
		void DeInitGrammar()
		{
			DeleteParsers();
		}
		// Parser rules
		Rule group, term5, term4_9, term4_8, term4_85, term4_7, term4_75, term4_6, term4_65, term4_4, term4_2, term4_1, term4, term3, term2, term1, expression;
		Rule varname, funccall, funcdef, funcvars, block, vardef, vardefsub, ifexpr, whileexpr, forexpr, retexpr;
		Rule doexpr, breakexpr, switchexpr, isconst, addvarp, seltype, arrayDef;
		Rule classdef, variable, postExpr, continueExpr;
		Rule funcProt;	// user function prototype

		Rule code, mySpaceP;
	};
};

unsigned int buildInFuncs;
unsigned int buildInTypes;

CompilerError::CompilerError(const std::string& errStr, const char* apprPos)
{
	Init(errStr.c_str(), apprPos);
}
CompilerError::CompilerError(const char* errStr, const char* apprPos)
{
	Init(errStr, apprPos);
}

void CompilerError::Init(const char* errStr, const char* apprPos)
{
	empty = 0;
	unsigned int len = (unsigned int)strlen(errStr) < 128 ? (unsigned int)strlen(errStr) : 127;
	memcpy(error, errStr, len);
	error[len] = 0;
	if(apprPos)
	{
		const char *begin = apprPos;
		while((begin > codeStart) && (*begin != '\n') && (*begin != '\r'))
			begin--;
		if(begin < apprPos)
			begin++;

		lineNum = 1;
		const char *scan = codeStart;
		while(scan < begin)
			if(*(scan++) == '\n')
				lineNum++;

		const char *end = apprPos;
		while((*end != '\r') && (*end != '\n') && (*end != 0))
			end++;
		len = (unsigned int)(end - begin) < 128 ? (unsigned int)(end - begin) : 127;
		memcpy(line, begin, len);
		line[len] = 0;
		shift = (unsigned int)(apprPos-begin);
	}else{
		line[0] = 0;
		shift = 0;
		lineNum = 0;
	}
}
const char *CompilerError::codeStart = NULL;

Compiler::Compiler()
{
	// Add types
	TypeInfo* info;
	info = new TypeInfo();
	info->name = "void";
	info->size = 0;
	info->type = TypeInfo::TYPE_VOID;
	typeVoid = info;
	typeInfo.push_back(info);

	info = new TypeInfo();
	info->alignBytes = 8;
	info->name = "double";
	info->size = 8;
	info->type = TypeInfo::TYPE_DOUBLE;
	typeDouble = info;
	typeInfo.push_back(info);

	info = new TypeInfo();
	info->alignBytes = 4;
	info->name = "float";
	info->size = 4;
	info->type = TypeInfo::TYPE_FLOAT;
	typeFloat = info;
	typeInfo.push_back(info);

	info = new TypeInfo();
	info->name = "long";
	info->size = 8;
	info->type = TypeInfo::TYPE_LONG;
	typeLong = info;
	typeInfo.push_back(info);

	info = new TypeInfo();
	info->alignBytes = 4;
	info->name = "int";
	info->size = 4;
	info->type = TypeInfo::TYPE_INT;
	typeInt = info;
	typeInfo.push_back(info);

	info = new TypeInfo();
	info->name = "short";
	info->size = 2;
	info->type = TypeInfo::TYPE_SHORT;
	typeShort = info;
	typeInfo.push_back(info);

	info = new TypeInfo();
	info->name = "char";
	info->size = 1;
	info->type = TypeInfo::TYPE_CHAR;
	typeChar = info;
	typeInfo.push_back(info);

	info = new TypeInfo();
	info->alignBytes = 4;
	info->name = "float2";
	info->type = TypeInfo::TYPE_COMPLEX;
	info->AddMember("x", typeFloat);
	info->AddMember("y", typeFloat);
	typeInfo.push_back(info);

	info = new TypeInfo();
	info->alignBytes = 4;
	info->name = "float3";
	info->type = TypeInfo::TYPE_COMPLEX;
	info->AddMember("x", typeFloat);
	info->AddMember("y", typeFloat);
	info->AddMember("z", typeFloat);
	typeInfo.push_back(info);

	info = new TypeInfo();
	info->alignBytes = 4;
	info->name = "float4";
	info->type = TypeInfo::TYPE_COMPLEX;
	info->AddMember("x", typeFloat);
	info->AddMember("y", typeFloat);
	info->AddMember("z", typeFloat);
	info->AddMember("w", typeFloat);
	typeInfo.push_back(info);

	TypeInfo *typeFloat4 = info;

	info = new TypeInfo();
	info->alignBytes = 8;
	info->name = "double2";
	info->type = TypeInfo::TYPE_COMPLEX;
	info->AddMember("x", typeDouble);
	info->AddMember("y", typeDouble);
	typeInfo.push_back(info);

	info = new TypeInfo();
	info->alignBytes = 8;
	info->name = "double3";
	info->type = TypeInfo::TYPE_COMPLEX;
	info->AddMember("x", typeDouble);
	info->AddMember("y", typeDouble);
	info->AddMember("z", typeDouble);
	typeInfo.push_back(info);

	info = new TypeInfo();
	info->alignBytes = 8;
	info->name = "double4";
	info->type = TypeInfo::TYPE_COMPLEX;
	info->AddMember("x", typeDouble);
	info->AddMember("y", typeDouble);
	info->AddMember("z", typeDouble);
	info->AddMember("w", typeDouble);
	typeInfo.push_back(info);

	info = new TypeInfo();
	info->alignBytes = 4;
	info->name = "float4x4";
	info->type = TypeInfo::TYPE_COMPLEX;
	info->AddMember("row1", typeFloat4);
	info->AddMember("row2", typeFloat4);
	info->AddMember("row3", typeFloat4);
	info->AddMember("row4", typeFloat4);
	typeInfo.push_back(info);

	info = new TypeInfo();
	info->name = "file";
	info->size = 4;
	info->type = TypeInfo::TYPE_COMPLEX;
	typeFile = info;
	typeInfo.push_back(info);

	// Add functions
	FunctionInfo	*fInfo;
	fInfo = new FunctionInfo();
	fInfo->address = -1;
	fInfo->name = "cos";
	fInfo->nameHash = GetStringHash(fInfo->name.c_str());
	fInfo->params.push_back(VariableInfo("deg", 0, typeDouble));
	fInfo->retType = typeDouble;
	fInfo->vTopSize = 1;
	fInfo->funcType = GetFunctionType(fInfo);
	funcInfo.push_back(fInfo);

	fInfo = new FunctionInfo();
	fInfo->address = -1;
	fInfo->name = "sin";
	fInfo->nameHash = GetStringHash(fInfo->name.c_str());
	fInfo->params.push_back(VariableInfo("deg", 0, typeDouble));
	fInfo->retType = typeDouble;
	fInfo->vTopSize = 1;
	fInfo->funcType = GetFunctionType(fInfo);
	funcInfo.push_back(fInfo);

	fInfo = new FunctionInfo();
	fInfo->address = -1;
	fInfo->name = "tan";
	fInfo->nameHash = GetStringHash(fInfo->name.c_str());
	fInfo->params.push_back(VariableInfo("deg", 0, typeDouble));
	fInfo->retType = typeDouble;
	fInfo->vTopSize = 1;
	fInfo->funcType = GetFunctionType(fInfo);
	funcInfo.push_back(fInfo);

	fInfo = new FunctionInfo();
	fInfo->address = -1;
	fInfo->name = "ctg";
	fInfo->nameHash = GetStringHash(fInfo->name.c_str());
	fInfo->params.push_back(VariableInfo("deg", 0, typeDouble));
	fInfo->retType = typeDouble;
	fInfo->vTopSize = 1;
	fInfo->funcType = GetFunctionType(fInfo);
	funcInfo.push_back(fInfo);

	fInfo = new FunctionInfo();
	fInfo->address = -1;
	fInfo->name = "ceil";
	fInfo->nameHash = GetStringHash(fInfo->name.c_str());
	fInfo->params.push_back(VariableInfo("deg", 0, typeDouble));
	fInfo->retType = typeDouble;
	fInfo->vTopSize = 1;
	fInfo->funcType = GetFunctionType(fInfo);
	funcInfo.push_back(fInfo);

	fInfo = new FunctionInfo();
	fInfo->address = -1;
	fInfo->name = "floor";
	fInfo->nameHash = GetStringHash(fInfo->name.c_str());
	fInfo->params.push_back(VariableInfo("deg", 0, typeDouble));
	fInfo->retType = typeDouble;
	fInfo->vTopSize = 1;
	fInfo->funcType = GetFunctionType(fInfo);
	funcInfo.push_back(fInfo);

	fInfo = new FunctionInfo();
	fInfo->address = -1;
	fInfo->name = "sqrt";
	fInfo->nameHash = GetStringHash(fInfo->name.c_str());
	fInfo->params.push_back(VariableInfo("deg", 0, typeDouble));
	fInfo->retType = typeDouble;
	fInfo->vTopSize = 1;
	fInfo->funcType = GetFunctionType(fInfo);
	funcInfo.push_back(fInfo);

	buildInTypes = (int)typeInfo.size();
	buildInFuncs = (int)funcInfo.size();

	syntax = new CompilerGrammar::Grammar();
	syntax->InitGrammar();

#ifdef NULLC_LOG_FILES
	compileLog = NULL;
#endif
}

Compiler::~Compiler()
{
	for(unsigned int i = 0; i < typeInfo.size(); i++)
	{
		delete typeInfo[i]->funcType;
		delete typeInfo[i];
	}
	for(unsigned int i = 0; i < funcInfo.size(); i++)
		delete funcInfo[i];

	syntax->DeInitGrammar();
	delete syntax;

	for(std::set<VariableInfo*>::iterator s = varInfoAll.begin(), e = varInfoAll.end(); s!=e; s++)
		delete *s;

	varInfoAll.clear();
	varInfo.clear();
	funcInfo.clear();
	typeInfo.clear();

#ifdef NULLC_LOG_FILES
	if(compileLog)
		fclose(compileLog);
#endif
}

void Compiler::ClearState()
{
	varInfoTop.clear();
	funcInfoTop.clear();

	for(std::set<VariableInfo*>::iterator s = varInfoAll.begin(), e = varInfoAll.end(); s!=e; s++)
		delete *s;
	varInfoAll.clear();
	varInfo.clear();

	for(unsigned int i = buildInTypes; i < typeInfo.size(); i++)
	{
		delete typeInfo[i]->funcType;
		delete typeInfo[i];
	}
	for(unsigned int i = buildInFuncs; i < funcInfo.size(); i++)
		delete funcInfo[i];

	typeInfo.resize(buildInTypes);
	funcInfo.resize(buildInFuncs);

	callArgCount.clear();
	retTypeStack.clear();
	currDefinedFunc.clear();

	currTypes.clear();

	nodeList.clear();
	funcDefList.clear();

	varDefined = 0;
	negCount = 0;
	varTop = 24;
	newType = NULL;

	currAlign = TypeInfo::UNSPECIFIED_ALIGNMENT;
	inplaceArrayNum = 1;

	varInfo.push_back(new VariableInfo("ERROR", 0, typeDouble, true));
	varInfoAll.insert(varInfo.back());
	varInfo.push_back(new VariableInfo("pi", 8, typeDouble, true));
	varInfoAll.insert(varInfo.back());
	varInfo.push_back(new VariableInfo("e", 16, typeDouble, true));
	varInfoAll.insert(varInfo.back());

	varInfoTop.push_back(VarTopInfo(0,0));

	funcInfoTop.push_back(0);

	retTypeStack.push_back(NULL);	//global return can return anything

	arrElementCount.clear();

#ifdef NULLC_LOG_FILES
	if(compileLog)
		fclose(compileLog);
	compileLog = fopen("compilelog.txt", "wb");
#endif
	warningLog.clear();

	lastError = CompilerError();
}

bool Compiler::AddExternalFunction(void (NCDECL *ptr)(), const char* prototype)
{
	ClearState();

	ParseResult pRes = Parse(syntax->funcProt, (char*)prototype, syntax->mySpaceP);
	if(pRes != PARSE_OK)
		return false;

	funcInfo.back()->address = -1;
	funcInfo.back()->funcPtr = (void*)ptr;

	strs.pop_back();
	retTypeStack.pop_back();
	buildInFuncs++;

	FunctionInfo	&lastFunc = *funcInfo.back();
	// Find out the function type
	TypeInfo	*bestFit = NULL;
	// Search through active types
	for(unsigned int i = 0; i < typeInfo.size(); i++)
	{
		if(typeInfo[i]->funcType)
		{
			if(typeInfo[i]->funcType->retType != lastFunc.retType)
				continue;
			if(typeInfo[i]->funcType->paramType.size() != lastFunc.params.size())
				continue;
			bool good = true;
			for(unsigned int n = 0; n < lastFunc.params.size(); n++)
			{
				if(lastFunc.params[n].varType != typeInfo[i]->funcType->paramType[n])
				{
					good = false;
					break;
				}
			}
			if(good)
			{
				bestFit = typeInfo[i];
				break;
			}
		}
	}
	// If none found, create new
	if(!bestFit)
	{
		typeInfo.push_back(new TypeInfo());
		typeInfo.back()->funcType = new FunctionType();
		typeInfo.back()->size = 8;
		bestFit = typeInfo.back();

		bestFit->type = TypeInfo::TYPE_COMPLEX;

		bestFit->funcType->retType = lastFunc.retType;
		for(unsigned int n = 0; n < lastFunc.params.size(); n++)
		{
			bestFit->funcType->paramType.push_back(lastFunc.params[n].varType);
		}
	}
	lastFunc.funcType = bestFit;

	buildInTypes = (int)typeInfo.size();

	return true;
}

bool Compiler::Compile(string str)
{
	ClearState();

	cmdInfoList->Clear();
	cmdList.clear();

#ifdef NULLC_LOG_FILES
	FILE *fCode = fopen("code.txt", "wb");
	fwrite(str.c_str(), 1, str.length(), fCode);
	fclose(fCode);
#endif

	char* ptr = (char*)str.c_str();
	CompilerError::codeStart = ptr;

#ifdef NULLC_LOG_FILES
	FILE *fTime = fopen("time.txt", "wb");
#endif

	unsigned int t = clock();
	ParseResult pRes = Parse(syntax->code, ptr, syntax->mySpaceP, false);
	if(pRes == PARSE_NOTFULL)
	{
		lastError = CompilerError("Parsing wasn't full", NULL);
		return false;
	}
	if(pRes == PARSE_FAILED)
	{
		lastError = CompilerError("Parsing failed", NULL);
		return false;
	}
	if(pRes == PARSE_ABORTED)
		return false;
	unsigned int tem = clock()-t;
#ifdef NULLC_LOG_FILES
	fprintf(fTime, "Parsing and AST tree gen. time: %d ms\r\n", tem * 1000 / CLOCKS_PER_SEC);
#endif
	//return true;
	// Emulate global block end
	CodeInfo::globalSize = varTop;

	t = clock();
	for(unsigned int i = 0; i < funcDefList.size(); i++)
	{
		funcDefList[i]->Compile();
		((NodeFuncDef*)funcDefList[i])->Disable();
	}
	if(nodeList.back())
		nodeList.back()->Compile();
	tem = clock()-t;
#ifdef NULLC_LOG_FILES
	fprintf(fTime, "Compile time: %d ms\r\n", tem * 1000 / CLOCKS_PER_SEC);
	fclose(fTime);
#endif

#ifdef NULLC_LOG_FILES
	FILE *fGraph = fopen("graph.txt", "wb");
	for(unsigned int i = 0; i < funcDefList.size(); i++)
		funcDefList[i]->LogToStream(fGraph);
	if(nodeList.back())
		nodeList.back()->LogToStream(fGraph);
	fclose(fGraph);
#endif

#ifdef NULLC_LOG_FILES
	fprintf(compileLog, "\r\n%s", warningLog.c_str());

	fprintf(compileLog, "\r\nActive types (%d):\r\n", typeInfo.size());
	for(unsigned int i = 0; i < typeInfo.size(); i++)
		fprintf(compileLog, "%s (%d bytes)\r\n", typeInfo[i]->GetTypeName().c_str(), typeInfo[i]->size);

	fprintf(compileLog, "\r\nActive functions (%d):\r\n", funcInfo.size());
	for(unsigned int i = 0; i < funcInfo.size(); i++)
	{
		FunctionInfo &currFunc = *funcInfo[i];
		fprintf(compileLog, "%s", currFunc.type == FunctionInfo::LOCAL ? "local " : (currFunc.type == FunctionInfo::NORMAL ? "global " : "thiscall "));
		fprintf(compileLog, "%s %s(", currFunc.retType->GetTypeName().c_str(), currFunc.name.c_str());

		for(unsigned int n = 0; n < currFunc.params.size(); n++)
			fprintf(compileLog, "%s %s%s", currFunc.params[n].varType->GetTypeName().c_str(), currFunc.params[n].name.c_str(), (n==currFunc.params.size()-1 ? "" :", "));
		
		fprintf(compileLog, ")\r\n");
		if(currFunc.type == FunctionInfo::LOCAL)
		{
			for(unsigned int n = 0; n < currFunc.external.size(); n++)
				fprintf(compileLog, "  external var: %s\r\n", currFunc.external[n].c_str());
		}
	}
	fflush(compileLog);
#endif

	if(nodeList.back())
	{
		delete nodeList.back();
		nodeList.pop_back();
	}

	if(nodeList.size() != 0)
	{
		lastError = CompilerError("Compilation failed, AST contains more than one node", NULL);
		return false;
	}

	return true;
}

const char* Compiler::GetError()
{
	return lastError.GetErrorString();
}

void Compiler::SaveListing(const char *fileName)
{
#ifdef NULLC_LOG_FILES
	FILE *compiledAsm = fopen(fileName, "wb");
	char instBuf[128];
	for(unsigned int i = 0; i < CodeInfo::cmdList.size(); i++)
	{
		CodeInfo::cmdList[i].Decode(instBuf);
		fprintf(compiledAsm, "%s\r\n", instBuf);
	}
	fclose(compiledAsm);
#else
	(void)fileName;
#endif
}

unsigned int GetTypeIndexByPtr(TypeInfo* type)
{
	for(unsigned int n = 0; n < CodeInfo::typeInfo.size(); n++)
		if(CodeInfo::typeInfo[n] == type)
			return n;
	assert(!"type not found");
	return ~0u;
}

unsigned int Compiler::GetBytecode(char **bytecode)
{
	// find out the size of generated bytecode
	unsigned int size = sizeof(ByteCode);

	for(unsigned int i = 0; i < CodeInfo::typeInfo.size(); i++)
	{
		size += sizeof(ExternTypeInfo);
		size += (int)CodeInfo::typeInfo[i]->GetTypeName().length()+1;
	}

	unsigned int offsetToVar = size;
	for(unsigned int i = 0; i < CodeInfo::varInfo.size(); i++)
	{
		size += sizeof(ExternVarInfo);
		size += (int)CodeInfo::varInfo[i]->name.length()+1;
	}

	unsigned int offsetToFunc = size;
	for(unsigned int i = 0; i < CodeInfo::funcInfo.size(); i++)
	{
		size += sizeof(ExternFuncInfo);
		size += (int)CodeInfo::funcInfo[i]->name.length()+1;
		size += (unsigned int)CodeInfo::funcInfo[i]->params.size() * sizeof(unsigned int);
	}
	unsigned int offsetToCode = size;
	size += CodeInfo::cmdList.size() * sizeof(VMCmd);

	*bytecode = new char[size];

	ByteCode	*code = (ByteCode*)(*bytecode);
	code->size = size;

	code->typeCount = (unsigned int)CodeInfo::typeInfo.size();

	code->globalVarSize = varTop;
	code->variableCount = (unsigned int)CodeInfo::varInfo.size();
	code->offsetToFirstVar = offsetToVar;

	code->functionCount = (unsigned int)CodeInfo::funcInfo.size();
	code->offsetToFirstFunc = offsetToFunc;

	code->codeSize = CodeInfo::cmdList.size();
	code->offsetToCode = offsetToCode;

	ExternTypeInfo *tInfo = FindFirstType(code);
	code->firstType = tInfo;
	for(unsigned int i = 0; i < CodeInfo::typeInfo.size(); i++)
	{
		tInfo->size = CodeInfo::typeInfo[i]->size;
		tInfo->nameLength = (unsigned int)CodeInfo::typeInfo[i]->GetTypeName().length();
		tInfo->structSize = sizeof(ExternTypeInfo) + tInfo->nameLength + 1;

		tInfo->type = (ExternTypeInfo::TypeCategory)CodeInfo::typeInfo[i]->type;
		// ! write name after the pointer to name
		char *namePtr = (char*)(&tInfo->name) + sizeof(tInfo->name);
		memcpy(namePtr, CodeInfo::typeInfo[i]->GetTypeName().c_str(), tInfo->nameLength+1);
		tInfo->name = namePtr;

		if(i+1 == CodeInfo::typeInfo.size())
			tInfo->next = NULL;
		else
			tInfo->next = FindNextType(tInfo);

		// Fill up next
		tInfo = tInfo->next;
	}

	ExternVarInfo *varInfo = FindFirstVar(code);
	code->firstVar = varInfo;
	for(unsigned int i = 0; i < CodeInfo::varInfo.size(); i++)
	{
		varInfo->size = CodeInfo::varInfo[i]->varType->size;
		varInfo->nameLength = (unsigned int)CodeInfo::varInfo[i]->name.length();
		varInfo->structSize = sizeof(ExternVarInfo) + varInfo->nameLength + 1;

		varInfo->type = GetTypeIndexByPtr(CodeInfo::varInfo[i]->varType);

		// ! write name after the pointer to name
		char *namePtr = (char*)(&varInfo->name) + sizeof(varInfo->name);
		memcpy(namePtr, CodeInfo::varInfo[i]->name.c_str(), varInfo->nameLength+1);
		varInfo->name = namePtr;

		if(i+1 == CodeInfo::varInfo.size())
			varInfo->next = NULL;
		else
			varInfo->next = FindNextVar(varInfo);

		// Fill up next
		varInfo = varInfo->next;
	}

	unsigned int offsetToGlobal = 0;
	ExternFuncInfo *fInfo = FindFirstFunc(code);
	code->firstFunc = fInfo;
	for(unsigned int i = 0; i < CodeInfo::funcInfo.size(); i++)
	{
		fInfo->oldAddress = fInfo->address = CodeInfo::funcInfo[i]->address;
		fInfo->codeSize = CodeInfo::funcInfo[i]->codeSize;
		fInfo->funcPtr = CodeInfo::funcInfo[i]->funcPtr;
		fInfo->isVisible = CodeInfo::funcInfo[i]->visible;
		fInfo->funcType = (ExternFuncInfo::FunctionType)CodeInfo::funcInfo[i]->type;

		offsetToGlobal += fInfo->codeSize;

		fInfo->nameHash = CodeInfo::funcInfo[i]->nameHash;
		fInfo->nameLength = (unsigned int)CodeInfo::funcInfo[i]->name.length();

		fInfo->retType = GetTypeIndexByPtr(CodeInfo::funcInfo[i]->retType);
		fInfo->paramCount = (unsigned int)CodeInfo::funcInfo[i]->params.size();
		fInfo->paramList = (unsigned int*)((char*)(&fInfo->name) + sizeof(fInfo->name) + fInfo->nameLength + 1);

		fInfo->structSize = sizeof(ExternFuncInfo) + fInfo->nameLength + 1 + fInfo->paramCount * sizeof(unsigned int);

		for(unsigned int n = 0; n < fInfo->paramCount; n++)
			fInfo->paramList[n] = GetTypeIndexByPtr(CodeInfo::funcInfo[i]->params[n].varType);

		// ! write name after the pointer to name
		char *namePtr = (char*)(&fInfo->name) + sizeof(fInfo->name);
		memcpy(namePtr, CodeInfo::funcInfo[i]->name.c_str(), fInfo->nameLength+1);
		fInfo->name = namePtr;

		if(i+1 == CodeInfo::funcInfo.size())
			fInfo->next = NULL;
		else
			fInfo->next = FindNextFunc(fInfo);

		// Fill up next
		fInfo = fInfo->next;
	}

	code->code = FindCode(code);
	code->globalCodeStart = offsetToGlobal;
	memcpy(code->code, &CodeInfo::cmdList[0], CodeInfo::cmdList.size() * sizeof(VMCmd));

	return size;
}

