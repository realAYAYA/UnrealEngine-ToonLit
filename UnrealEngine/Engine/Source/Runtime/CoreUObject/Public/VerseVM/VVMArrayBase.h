// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VVMAux.h"
#include "VVMEmergentTypeCreator.h"
#include "VVMGlobalTrivialEmergentTypePtr.h"
#include "VVMHeap.h"

namespace Verse
{

struct VInt;
struct FOpResult;

struct VArrayBase : VHeapValue
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VHeapValue);
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

protected:
	TWriteBarrier<TAux<TWriteBarrier<VValue>>> Values;
	uint32 NumValues;

	VArrayBase(FAllocationContext Context, uint32 InNumValues, VEmergentType* Type)
		: VHeapValue(Context, Type)
		, NumValues(InNumValues)
	{
		SetIsDeeplyMutable();
		TAux<TWriteBarrier<VValue>> NewValues(Context.AllocateAuxCell(sizeof(TWriteBarrier<VValue>) * NumValues));
		Values.Set(Context, NewValues);
		for (uint32 Index = 0; Index < NumValues; ++Index)
		{
			new (&Values.Get()[Index]) TWriteBarrier<VValue>();
		}
	}

	VArrayBase(FAllocationContext Context, std::initializer_list<VValue> InitList, VEmergentType* Type)
		: VHeapValue(Context, Type)
		, NumValues(static_cast<uint32>(InitList.size()))
	{
		SetIsDeeplyMutable();
		TAux<TWriteBarrier<VValue>> NewValues(Context.AllocateAuxCell(sizeof(TWriteBarrier<VValue>) * NumValues));
		Values.Set(Context, NewValues);
		uint32 Index = 0;
		for (const VValue& Value : InitList)
		{
			new (&Values.Get()[Index++]) TWriteBarrier<VValue>(Context, Value);
		}
	}

	template <typename InitIndexFunc>
	VArrayBase(FAllocationContext Context, uint32 InNumValues, InitIndexFunc&& InitFunc, VEmergentType* Type)
		: VHeapValue(Context, Type)
		, NumValues(InNumValues)
	{
		SetIsDeeplyMutable();
		TAux<TWriteBarrier<VValue>> NewValues(Context.AllocateAuxCell(sizeof(TWriteBarrier<VValue>) * NumValues));
		Values.Set(Context, NewValues);
		for (uint32 Index = 0; Index < NumValues; ++Index)
		{
			new (&Values.Get()[Index]) TWriteBarrier<VValue>(Context, InitFunc(Index));
		}
	}

	TWriteBarrier<VValue>* GetData() { return Values.Get().GetPtr(); }
	const TWriteBarrier<VValue>* GetData() const { return Values.Get().GetPtr(); }

	void SetValue(FAccessContext Context, uint32 Index, VValue Value);

public:
	uint32 Num() const { return NumValues; }
	bool IsInBounds(uint32 Index) const;
	bool IsInBounds(const VInt& Index, const uint32 Bounds) const;
	VValue GetValue(uint32 Index);

	template <typename T>
	static T& Concat(FAllocationContext Context, VArrayBase& Lhs, VArrayBase& Rhs);

	COREUOBJECT_API bool EqualImpl(FRunningContext Context, VCell* Other, const TFunction<void(::Verse::VValue, ::Verse::VValue)>& HandlePlaceholder);

	COREUOBJECT_API FOpResult MeltImpl(FRunningContext Context);

	COREUOBJECT_API uint32 GetTypeHashImpl();

	COREUOBJECT_API void ToStringImpl(FStringBuilderBase& Builder, FAllocationContext Context, const FCellFormatter& Formatter);

	// C++ ranged-based iteration
	class FConstIterator
	{
	public:
		FORCEINLINE VValue operator*() const { return CurrentValue->Get(); }
		FORCEINLINE bool operator==(const FConstIterator& Rhs) const { return CurrentValue == Rhs.CurrentValue; }
		FORCEINLINE bool operator!=(const FConstIterator& Rhs) const { return CurrentValue != Rhs.CurrentValue; }
		FORCEINLINE FConstIterator& operator++()
		{
			++CurrentValue;
			return *this;
		}

	private:
		friend struct VArrayBase;
		FORCEINLINE FConstIterator(const TWriteBarrier<VValue>* InCurrentValue)
			: CurrentValue(InCurrentValue) {}
		const TWriteBarrier<VValue>* CurrentValue;
	};
	COREUOBJECT_API FConstIterator begin() const;
	COREUOBJECT_API FConstIterator end() const;
};

} // namespace Verse
#endif // WITH_VERSE_VM
