// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Class.h"
#include "StructView.h"
#include "Templates/UnrealTypeTraits.h"

#ifndef WITH_STRUCTUTILS_DEBUG
#define WITH_STRUCTUTILS_DEBUG (!(UE_BUILD_SHIPPING || UE_BUILD_SHIPPING_WITH_EDITOR || UE_BUILD_TEST) && 1)
#endif // WITH_STRUCTUTILS_DEBUG

struct FConstStructView;
class FReferenceCollector;

namespace UE::StructUtils
{
	extern STRUCTUTILS_API uint32 GetStructCrc32(const UScriptStruct& ScriptStruct, const uint8* StructMemory, const uint32 CRC = 0);

	extern STRUCTUTILS_API uint32 GetStructCrc32(const FConstStructView StructView, const uint32 CRC = 0);

	template <typename T>
	typename TEnableIf<!TIsDerivedFrom<T, UObject>::IsDerived, UScriptStruct*>::Type GetAsUStruct()
	{
		return T::StaticStruct();
	}

	template <typename T>
	typename TEnableIf<TIsDerivedFrom<T, UObject>::IsDerived, UClass*>::Type GetAsUStruct()
	{
		return T::StaticClass();
	}
}

/* Predicate useful to find a struct of a specific type in an container */
struct FStructTypeEqualOperator
{
	const UScriptStruct* TypePtr;
	FStructTypeEqualOperator(const UScriptStruct* InTypePtr) : TypePtr(InTypePtr) {}
	FStructTypeEqualOperator(const FConstStructView InRef) : TypePtr(InRef.GetScriptStruct()) {}

	bool operator()(const FConstStructView Other) const { return Other.GetScriptStruct() == TypePtr; }
};

struct FScriptStructSortOperator
{
	template<typename T>
	bool operator()(const T& A, const T& B) const
	{
		return (A.GetStructureSize() > B.GetStructureSize())
			|| (A.GetStructureSize() == B.GetStructureSize() && B.GetFName().FastLess(A.GetFName()));
	}
};

struct FStructTypeSortOperator
{
	bool operator()(const FConstStructView A, const FConstStructView B) const
	{
		const UScriptStruct* AScriptStruct = A.GetScriptStruct();
		const UScriptStruct* BScriptStruct = B.GetScriptStruct();
		if (!AScriptStruct)
		{
			return true;
		}
		else if (!BScriptStruct)
		{
			return false;
		}
		return FScriptStructSortOperator()(*AScriptStruct, *BScriptStruct);
	}
};