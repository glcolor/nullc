#include "dynamic.h"
#include "../nullc.h"
#include "../nullc_debug.h"

namespace NULLCDynamic
{
	Linker *linker = NULL;

	void OverrideFunction(NULLCRef dest, NULLCRef src)
	{
		if(linker->exTypes[dest.typeID].subCat != ExternTypeInfo::CAT_FUNCTION)
		{
			nullcThrowError("Destination variable is not a function");
			return;
		}
		if(linker->exTypes[src.typeID].subCat != ExternTypeInfo::CAT_FUNCTION)
		{
			nullcThrowError("Source variable is not a function");
			return;
		}
		if(nullcGetCurrentExecutor(NULL) == NULLC_X86)
		{
			nullcThrowError("Function rewrite is supported only on VM");
			return;
		}
		if(dest.typeID != src.typeID)
		{
			nullcThrowError("Cannot convert from '%s' to '%s'", &linker->exSymbols[linker->exTypes[src.typeID].offsetToName], &linker->exSymbols[linker->exTypes[dest.typeID].offsetToName]);
			return;
		}
		ExternFuncInfo &destFunc = linker->exFunctions[((NULLCFuncPtr*)dest.ptr)->id];
		ExternFuncInfo &srcFunc = linker->exFunctions[((NULLCFuncPtr*)src.ptr)->id];
		destFunc.address = srcFunc.address;
		destFunc.codeSize = srcFunc.codeSize;
	}

	void Override(NULLCRef dest, NullCArray code)
	{
		static unsigned int overrideID = 0;

		if(linker->exTypes[dest.typeID].subCat != ExternTypeInfo::CAT_FUNCTION)
		{
			nullcThrowError("Destination variable is not a function");
			return;
		}
		if(nullcGetCurrentExecutor(NULL) == NULLC_X86)
		{
			nullcThrowError("Function rewrite is supported only on VM");
			return;
		}

		char tmp[2048];
		char *it = tmp;
		unsigned int	*memberList = &linker->exTypeExtra[linker->exTypes[dest.typeID].memberOffset];
		ExternTypeInfo	&returnType = linker->exTypes[memberList[0]];
		it += SafeSprintf(it, 2048 - int(it - tmp), "import __last;\r\n%s __override%d(", &linker->exSymbols[0] + returnType.offsetToName, overrideID);

		for(unsigned int i = 0, memberCount = linker->exTypes[dest.typeID].memberCount; i != memberCount; i++)
			it += SafeSprintf(it, 2048 - int(it - tmp), "%s arg%d%s", &linker->exSymbols[0] + linker->exTypes[memberList[i + 1]].offsetToName, i, i == memberCount - 1 ? "" : ", ");
		it += SafeSprintf(it, 2048 - int(it - tmp), "){ %s }", code.ptr);
		overrideID++;

		if(!nullcCompile(tmp))
		{
			nullcThrowError("%s", nullcGetLastError());
			return;
		}
		char *bytecode = NULL;
		nullcGetBytecodeNoCache(&bytecode);
		if(!nullcLinkCode(bytecode, 0))
		{
			delete[] bytecode;
			nullcThrowError("%s", nullcGetLastError());
			return;
		}
		delete[] bytecode;

		ExternFuncInfo &destFunc = linker->exFunctions[((NULLCFuncPtr*)dest.ptr)->id];
		ExternFuncInfo &srcFunc = linker->exFunctions.back();
		destFunc.address = srcFunc.address;
		destFunc.codeSize = srcFunc.codeSize;
	}
}

#define REGISTER_FUNC(funcPtr, name, index) if(!nullcAddModuleFunction("std.dynamic", (void(*)())NULLCDynamic::funcPtr, name, index)) return false;
bool	nullcInitDynamicModule(Linker* linker)
{
	NULLCDynamic::linker = linker;

	REGISTER_FUNC(OverrideFunction, "override", 0);
	REGISTER_FUNC(Override, "override", 1);

	return true;
}
