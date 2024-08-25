// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "Containers/Map.h"
#include "CoreTypes.h"
#include "VVMCell.h"
#include "VVMContext.h"
#include "VVMGlobalTrivialEmergentTypePtr.h"
#include "VVMHeap.h"
#include "VVMValue.h"
#include "VVMWriteBarrier.h"

namespace Verse
{

/*
 * The following types can be used as keys:
 * logic
 * int
 * float
 * char
 * string
 * enum
 * A class, if it’s comparable
 * An option, if the element type is comparable
 * An array, if the element type is comparable
 * A map if both the key and the value types are comparable
 * A tuple if all elements in the tuple are comparable
 */

struct VMapBaseInternalKeyFuncs : TDefaultMapKeyFuncs<TWriteBarrier<VValue>, TWriteBarrier<VValue>, false>
{
	static FORCEINLINE bool Matches(KeyInitType A, KeyInitType B)
	{
		return VValue::Equal(FRunningContextPromise(), A.Get(), B.Get(), [](VValue Left, VValue Right) {
			checkSlow(!Left.IsPlaceholder());
			checkSlow(!Right.IsPlaceholder());
		});
	}

	static FORCEINLINE bool Matches(KeyInitType A, VValue B)
	{
		return VValue::Equal(FRunningContextPromise(), A.Get(), B, [](VValue Left, VValue Right) {
			checkSlow(!Left.IsPlaceholder());
			checkSlow(!Right.IsPlaceholder());
		});
	}

	COREUOBJECT_API static uint32 GetKeyHash(KeyInitType Key);
	COREUOBJECT_API static uint32 GetKeyHash(VValue Key);
};
using VMapBaseInternal = TMap<TWriteBarrier<VValue>, TWriteBarrier<VValue>, FDefaultSetAllocator, VMapBaseInternalKeyFuncs>;

struct VMapBase : VHeapValue
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VHeapValue);

protected:
	// TODO: Create an allocator for this map which uses the GC's Aux allocation so we don't have to count external memory
	// NB: Right now, we rely on nothing being removed from this map because we look things up by index.
	VMapBaseInternal InternalMap;

	VMapBase(FAllocationContext Context, uint32 InitialCapacity, VEmergentType* Type)
		: VHeapValue(Context, Type)
	{
		SetIsDeeplyMutable();
		InternalMap.Reserve(InitialCapacity);
		FHeap::ReportAllocatedNativeBytes(InternalMap.GetAllocatedSize());
	}

	template <typename GetEntryByIndex>
	VMapBase(FAllocationContext Context, uint32 MaxNumEntries, const GetEntryByIndex& GetEntry, VEmergentType* Type);

	~VMapBase();

public:
	int32 Num() const
	{
		return InternalMap.Num();
	}

	VValue Find(const VValue Key);

	// GetKey/GetValue doesn't verify that Index is within limits and
	// only works as long as nothing is removed from the map.
	VValue GetKey(const int32 Index);
	VValue GetValue(const int32 Index);
	void Add(FAllocationContext Context, VValue Key, VValue Value);

	size_t GetAllocatedSize() const
	{
		return InternalMap.GetAllocatedSize();
	}

	// These `new` calls are templated so as to avoid boilerplate News/Ctors in VMapBase's subclasses.
	template <typename MapType>
	static VMapBase& New(FAllocationContext Context, uint32 InitialCapacity = 0)
	{
		return *new (Context.Allocate(Verse::FHeap::DestructorSpace, sizeof(VMapBase))) VMapBase(Context, InitialCapacity, &MapType::GlobalTrivialEmergentType.Get(Context));
	}

	template <typename MapType, typename GetEntryByIndex>
	static VMapBase& New(FAllocationContext Context, uint32 MaxNumEntries, const GetEntryByIndex& GetEntry);

	template <typename MapType>
	static void Serialize(MapType*& This, FAllocationContext Context, FAbstractVisitor& Visitor);

	template <typename MapType, typename TranslationFunc>
	FOpResult Copy(FRunningContext Context, TranslationFunc&& Func);

	COREUOBJECT_API FOpResult MeltImpl(FRunningContext Context);

	COREUOBJECT_API bool EqualImpl(FRunningContext Context, VCell* Other, const TFunction<void(::Verse::VValue, ::Verse::VValue)>& HandlePlaceholder);

	COREUOBJECT_API uint32 GetTypeHashImpl();

	// C++ ranged-based iteration
	class FConstIterator
	{
	public:
		FORCEINLINE TPair<VValue, VValue> operator*() const { return {CurrentValue->Key.Get(), CurrentValue->Value.Get()}; }
		FORCEINLINE bool operator==(const FConstIterator& Rhs) const { return CurrentValue == Rhs.CurrentValue; }
		FORCEINLINE bool operator!=(const FConstIterator& Rhs) const { return CurrentValue != Rhs.CurrentValue; }
		FORCEINLINE FConstIterator& operator++()
		{
			++CurrentValue;
			return *this;
		}

	private:
		friend struct VMapBase;
		FORCEINLINE FConstIterator(VMapBaseInternal::TRangedForConstIterator InCurrentValue)
			: CurrentValue(InCurrentValue) {}
		VMapBaseInternal::TRangedForConstIterator CurrentValue;
	};
	FORCEINLINE FConstIterator begin() const { return InternalMap.begin(); }
	FORCEINLINE FConstIterator end() const { return InternalMap.end(); }
};

struct VMap : VMapBase
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VMapBase);
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;
	static void SerializeImpl(VMap*& This, FAllocationContext Context, FAbstractVisitor& Visitor) { Super::Serialize<VMap>(This, Context, Visitor); }
};

struct VMutableMap : VMapBase
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VMapBase);
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;
	COREUOBJECT_API FOpResult FreezeImpl(FRunningContext Context);
	static void SerializeImpl(VMutableMap*& This, FAllocationContext Context, FAbstractVisitor& Visitor) { Super::Serialize<VMutableMap>(This, Context, Visitor); }
};

} // namespace Verse
#endif // WITH_VERSE_VM
