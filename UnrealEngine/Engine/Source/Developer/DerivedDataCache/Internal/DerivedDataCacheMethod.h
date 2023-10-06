// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/StringFwd.h"

#define UE_API DERIVEDDATACACHE_API

class FCbFieldView;
class FCbWriter;

namespace UE::DerivedData
{

enum class ECacheMethod : uint8
{
	Put,
	Get,
	PutValue,
	GetValue,
	GetChunks,
};

UE_API FAnsiStringBuilderBase& operator<<(FAnsiStringBuilderBase& Builder, ECacheMethod Method);
UE_API FWideStringBuilderBase& operator<<(FWideStringBuilderBase& Builder, ECacheMethod Method);
UE_API FUtf8StringBuilderBase& operator<<(FUtf8StringBuilderBase& Builder, ECacheMethod Method);

UE_API bool TryLexFromString(ECacheMethod& OutMethod, FUtf8StringView String);
UE_API bool TryLexFromString(ECacheMethod& OutMethod, FWideStringView String);

UE_API FCbWriter& operator<<(FCbWriter& Writer, ECacheMethod Method);
UE_API bool LoadFromCompactBinary(FCbFieldView Field, ECacheMethod& OutMethod);

class FCacheMethodFilter
{
public:
	/** Parse from method names delimited by ',' or '+'. */
	UE_API static FCacheMethodFilter Parse(FStringView MethodNames);

	inline bool IsMatch(ECacheMethod Method) const
	{
		return (MethodMask & (1 << uint32(Method))) == 0;
	}

private:
	uint32 MethodMask = 0;
};

} // UE::DerivedData

#undef UE_API
