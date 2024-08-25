// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Context.h"
#include "FunctionMap.h"

#include "Containers/StringConv.h"

namespace AutoRTFM
{

inline void* FunctionMapLookup(void* OldFunction, const char* Where)
{
    void* Result = FunctionMapTryLookup(OldFunction);
    if (!Result)
    {
		if (Where)
		{
			UE_LOG(LogAutoRTFM, Fatal, TEXT("Could not find function %p '%s' where '%s'."), OldFunction, *GetFunctionDescription(OldFunction), ANSI_TO_TCHAR(Where));
		}
		else
		{
			UE_LOG(LogAutoRTFM, Fatal, TEXT("Could not find function %p '%s'."), OldFunction, *GetFunctionDescription(OldFunction));
		}
		
		FContext* Context = FContext::Get();
        Context->AbortByLanguageAndThrow();
    }
    return Result;
}

template<typename TReturnType, typename... TParameterTypes>
auto FunctionMapLookup(TReturnType (*Function)(TParameterTypes...), const char* Where) -> TReturnType (*)(TParameterTypes...)
{
    return reinterpret_cast<TReturnType (*)(TParameterTypes...)>(FunctionMapLookup(reinterpret_cast<void*>(Function), Where));
}

} // namespace AutoRTFM

