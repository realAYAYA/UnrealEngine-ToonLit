// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Context.h"
#include "FunctionMap.h"

#include "Containers/StringConv.h"

namespace AutoRTFM
{

inline void* FunctionMapLookup(void* OldFunction, FContext* Context, const char* Where)
{
    void* Result = FunctionMapTryLookup(OldFunction);
    if (!Result)
    {
		if (Where)
		{
			UE_LOG(LogAutoRTFM, Warning, TEXT("Could not find function %p '%s' where '%s'."), OldFunction, *GetFunctionDescription(OldFunction), Where);
		}
		else
		{
			UE_LOG(LogAutoRTFM, Warning, TEXT("Could not find function %p '%s'."), OldFunction, *GetFunctionDescription(OldFunction));
		}

        Context->AbortByLanguageAndThrow();
    }
    return Result;
}

template<typename TReturnType, typename... TParameterTypes>
auto FunctionMapLookup(TReturnType (*Function)(TParameterTypes...), FContext* Context, const char* Where) -> TReturnType (*)(TParameterTypes..., FContext*)
{
    return reinterpret_cast<TReturnType (*)(TParameterTypes..., FContext*)>(FunctionMapLookup(reinterpret_cast<void*>(Function), Context, Where));
}

} // namespace AutoRTFM

