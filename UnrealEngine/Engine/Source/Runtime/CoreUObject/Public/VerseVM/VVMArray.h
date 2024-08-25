// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VVMArrayBase.h"
#include "VVMEmergentTypeCreator.h"
#include "VVMGlobalTrivialEmergentTypePtr.h"
#include "VVMType.h"
#include "VVMTypeCreator.h"
#include "VVMUniqueCreator.h"

namespace Verse
{

struct VInt;

// Array, fix number of elements, each with its own type
// No type information for the parts here.
struct VTypeArray : VType
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VType);
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

	uint32 Size;

	static VTypeArray* New(FAllocationContext Context, uint32 S)
	{
		return new (Context.AllocateFastCell(sizeof(VTypeArray))) VTypeArray(Context, S);
	}
	static bool Equals(const VType& Type, uint32 S)
	{
		if (Type.IsA<VTypeArray>())
		{
			const VTypeArray& Other = Type.StaticCast<VTypeArray>();
			return Other.Size == S;
		}
		return false;
	}

	uint32 Num() const
	{
		return Size;
	}

private:
	explicit VTypeArray(FAllocationContext& Context, uint32 S)
		: VType(Context, &GlobalTrivialEmergentType.Get(Context))
		, Size(S)
	{
	}
};

struct VArray : VArrayBase
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VArrayBase);
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

	static VArray& New(FAllocationContext Context, uint32 NumValues)
	{
		return *new (Context.AllocateFastCell(sizeof(VArray))) VArray(Context, NumValues);
	}

	static VArray& New(FAllocationContext Context, std::initializer_list<VValue> InitList)
	{
		return *new (Context.AllocateFastCell(sizeof(VArray))) VArray(Context, InitList);
	}

	template <typename InitIndexFunc>
	static VArray& New(FAllocationContext Context, uint32 NumValues, InitIndexFunc&& InitFunc)
	{
		return *new (Context.AllocateFastCell(sizeof(VArray))) VArray(Context, NumValues, InitFunc);
	}

	static void SerializeImpl(VArray*& This, FAllocationContext Context, FAbstractVisitor& Visitor);

private:
	friend struct VMutableArray;
	VArray(FAllocationContext Context, uint32 InNumValues)
		: VArrayBase(Context, InNumValues, VEmergentTypeCreator::GetOrCreate(Context, VTypeCreator::GetOrCreate<VTypeArray>(Context, InNumValues), &StaticCppClassInfo)) {}

	VArray(FAllocationContext Context, std::initializer_list<VValue> InitList)
		: VArrayBase(Context, InitList, VEmergentTypeCreator::GetOrCreate(Context, VTypeCreator::GetOrCreate<VTypeArray>(Context, static_cast<uint32>(InitList.size())), &StaticCppClassInfo)) {}

	template <typename InitIndexFunc>
	VArray(FAllocationContext Context, uint32 InNumValues, InitIndexFunc&& InitFunc)
		: VArrayBase(Context, InNumValues, InitFunc, VEmergentTypeCreator::GetOrCreate(Context, VTypeCreator::GetOrCreate<VTypeArray>(Context, InNumValues), &StaticCppClassInfo)) {}
};

} // namespace Verse
#endif // WITH_VERSE_VM
