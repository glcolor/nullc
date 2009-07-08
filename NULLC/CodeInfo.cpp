#include "stdafx.h"
#include "CodeInfo.h"

//////////////////////////////////////////////////////////////////////////
// ������� ���������� ��� - ��������� �� ��������
TypeInfo* CodeInfo::GetReferenceType(TypeInfo* type)
{
	// ������ ������ ��� � ������
	unsigned int targetRefLevel = type->refLevel+1;
	for(unsigned int i = 0; i < typeInfo.size(); i++)
	{
		if(type == typeInfo[i]->subType && type->name == typeInfo[i]->name && targetRefLevel == typeInfo[i]->refLevel)
		{
			return typeInfo[i];
		}
	}
	// �������� ����� ���
	TypeInfo* newInfo = new TypeInfo();
	newInfo->name = type->name;
	newInfo->size = 4;
	newInfo->type = TypeInfo::TYPE_INT;
	newInfo->refLevel = type->refLevel + 1;
	newInfo->subType = type;

	typeInfo.push_back(newInfo);
	return newInfo;
}

// ������� ���������� ���, ���������� ��� ������������� ���������
TypeInfo* CodeInfo::GetDereferenceType(TypeInfo* type)
{
	if(!type->subType || type->refLevel == 0)
	{
		std::string fullError = std::string("Cannot dereference type ") + type->GetTypeName() + std::string(" there is no result type available");
		throw CompilerError(fullError, lastKnownStartPos);
	}
	return type->subType;
}

// ������� ���������� ��� - ������ �������� ����� (���-�� ��������� � varSize)
TypeInfo* CodeInfo::GetArrayType(TypeInfo* type, unsigned int sizeInArgument)
{
	int arrSize = -1;
	bool unFixed = false;
	if(sizeInArgument == 0)
	{
		// � ��������� ���� ������ ���������� ����������� �����
		if(nodeList.back()->GetNodeType() == typeNodeNumber)
		{
			TypeInfo *aType = nodeList.back()->GetTypeInfo();
			NodeZeroOP* zOP = nodeList.back();
			if(aType == typeDouble)
			{
				arrSize = (int)static_cast<NodeNumber<double>* >(zOP)->GetVal();
			}else if(aType == typeFloat){
				arrSize = (int)static_cast<NodeNumber<float>* >(zOP)->GetVal();
			}else if(aType == typeLong){
				arrSize = (int)static_cast<NodeNumber<long long>* >(zOP)->GetVal();
			}else if(aType == typeInt){
				arrSize = static_cast<NodeNumber<int>* >(zOP)->GetVal();
			}else if(aType == typeVoid){
				arrSize = -1;
				unFixed = true;
			}else{
				std::string fullError = std::string("ERROR: unknown type of constant number node ") + aType->name;
				throw CompilerError(fullError, lastKnownStartPos);
			}
			delete nodeList.back();
			nodeList.pop_back();
		}else{
			throw CompilerError("ERROR: Array size must be a constant expression", lastKnownStartPos);
		}
	}else{
		arrSize = sizeInArgument;
		if(arrSize == -1)
			unFixed = true;
	}

	if(!unFixed && arrSize < 1)
		throw CompilerError("ERROR: Array size can't be negative or zero", lastKnownStartPos);
	// ������ ������ ��� � ������
	unsigned int targetArrLevel = type->arrLevel+1;
	for(unsigned int i = 0; i < typeInfo.size(); i++)
	{
		if(type == typeInfo[i]->subType && type->name == typeInfo[i]->name && targetArrLevel == typeInfo[i]->arrLevel && typeInfo[i]->arrSize == (unsigned int)arrSize)
		{
			return typeInfo[i];
		}
	}
	// �������� ����� ���
	TypeInfo* newInfo = new TypeInfo();
	newInfo->name = type->name;

	if(unFixed)
	{
		newInfo->size = 4;
		newInfo->AddMember("size", typeInt);
	}else{
		newInfo->size = type->size * arrSize;
		if(newInfo->size % 4 != 0)
		{
			newInfo->paddingBytes = 4 - (newInfo->size % 4);
			newInfo->size += 4 - (newInfo->size % 4);
		}
	}

	newInfo->type = TypeInfo::TYPE_COMPLEX;
	newInfo->arrLevel = type->arrLevel + 1;
	newInfo->arrSize = arrSize;
	newInfo->subType = type;

	typeInfo.push_back(newInfo);
	return newInfo;
}

// ������� ���������� ��� �������� �������
TypeInfo* CodeInfo::GetArrayElementType(TypeInfo* type)
{
	if(!type->subType || type->arrLevel == 0)
	{
		std::string fullErr = std::string("Cannot return array element type, ") + type->GetTypeName() + std::string(" is not an array");
		throw CompilerError(fullErr, lastKnownStartPos);
	}
	return type->subType;
}

TypeInfo* CodeInfo::GetFunctionType(FunctionInfo* info)
{
	// Find out the function type
	TypeInfo	*bestFit = NULL;
	// Search through active types
	for(unsigned int i = 0; i < typeInfo.size(); i++)
	{
		if(typeInfo[i]->funcType)
		{
			if(typeInfo[i]->funcType->retType != info->retType)
				continue;
			if(typeInfo[i]->funcType->paramType.size() != info->params.size())
				continue;
			bool good = true;
			for(unsigned int n = 0; n < info->params.size(); n++)
			{
				if(info->params[n].varType != typeInfo[i]->funcType->paramType[n])
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
		bestFit = typeInfo.back();

#ifdef _DEBUG
		bestFit->AddMember("context", typeInt);
		bestFit->AddMember("ptr", typeInt);
#endif

		bestFit->funcType = new FunctionType();
		bestFit->size = 8;

		bestFit->type = TypeInfo::TYPE_COMPLEX;

		bestFit->funcType->retType = info->retType;
		for(unsigned int n = 0; n < info->params.size(); n++)
		{
			bestFit->funcType->paramType.push_back(info->params[n].varType);
		}
	}
	return bestFit;
}