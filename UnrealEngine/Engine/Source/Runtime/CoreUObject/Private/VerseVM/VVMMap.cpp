// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMMap.h"
#include "Async/ExternalMutex.h"
#include "Async/UniqueLock.h"
#include "Templates/TypeHash.h"
#include "VerseVM/Inline/VVMAbstractVisitorInline.h"
#include "VerseVM/Inline/VVMEqualInline.h"
#include "VerseVM/Inline/VVMMapInline.h"
#include "VerseVM/Inline/VVMValueInline.h"
#include "VerseVM/VVMMarkStackVisitor.h"
#include "VerseVM/VVMOpResult.h"
#include "VerseVM/VVMWriteBarrier.h"

namespace Verse
{

uint32 VMapBaseInternalKeyFuncs::GetKeyHash(KeyInitType Key)
{
	return GetTypeHash(Key);
}

uint32 VMapBaseInternalKeyFuncs::GetKeyHash(VValue Key)
{
	return GetTypeHash(Key);
}

DEFINE_DERIVED_VCPPCLASSINFO(VMapBase);

template <typename TVisitor>
void VMapBase::VisitReferencesImpl(TVisitor& Visitor)
{
	UE::FExternalMutex ExternalMutex(Mutex);
	UE::TUniqueLock Lock(ExternalMutex);
	Visitor.Visit(InternalMap, TEXT("Values"));

	Visitor.ReportNativeBytes(GetAllocatedSize());
}

// TODO: Using the empty value to indicate not found
// won't work if we have a map of [t]void and use VValue()
// to represent void.
VValue VMapBase::Find(const VValue Key)
{
	TWriteBarrier<VValue>* Result = InternalMap.FindByHash(GetTypeHash(Key), Key);
	if (Result)
	{
		return Result->Follow();
	}
	return VValue();
}

uint32 VMapBase::GetTypeHashImpl()
{
	uint32 Result = 0;
	for (VMapBaseInternal::TConstIterator MapIt = InternalMap.CreateConstIterator(); MapIt; ++MapIt)
	{
		::HashCombineFast(Result, ::HashCombineFast(GetTypeHash(MapIt.Key()), GetTypeHash(MapIt.Value())));
	}
	return Result;
}

bool VMapBase::EqualImpl(FRunningContext Context, VCell* Other, const TFunction<void(::Verse::VValue, ::Verse::VValue)>& HandlePlaceholder)
{
	if (!Other->IsA<VMapBase>())
	{
		return false;
	}

	VMapBase& OtherMap = Other->StaticCast<VMapBase>();
	if (InternalMap.Num() != OtherMap.InternalMap.Num())
	{
		return false;
	}

	auto LhsIter = InternalMap.begin();
	auto RhsIter = OtherMap.InternalMap.begin();
	for (; LhsIter != InternalMap.end(); ++LhsIter, ++RhsIter)
	{
		VValue LhsKey = LhsIter.Key().Get();
		VValue RhsKey = RhsIter.Key().Get();
		VValue LhsValue = LhsIter.Value().Get();
		VValue RhsValue = RhsIter.Value().Get();

		if (!VValue::Equal(Context, LhsKey, RhsKey, HandlePlaceholder)
			|| !VValue::Equal(Context, LhsValue, RhsValue, HandlePlaceholder))
		{
			return false;
		}
	}
	return true;
}

VMapBase::~VMapBase()
{
	FHeap::ReportDeallocatedNativeBytes(GetAllocatedSize());
}

template <typename MapType, typename TranslationFunc>
FOpResult VMapBase::Copy(FRunningContext Context, TranslationFunc&& Func)
{
	VMapBase& MapCopy = VMapBase::New<MapType>(Context, Num());
	for (TPair<Verse::TWriteBarrier<Verse::VValue>, Verse::TWriteBarrier<Verse::VValue>>& Pair : InternalMap)
	{
		FOpResult KeyResult = Func(Context, Pair.Key.Get());
		if (KeyResult.Kind == FOpResult::ShouldSuspend)
		{
			return KeyResult;
		}
		FOpResult ValueResult = Func(Context, Pair.Value.Get());
		if (ValueResult.Kind == FOpResult::ShouldSuspend)
		{
			return ValueResult;
		}
		MapCopy.Add(Context, KeyResult.Value, ValueResult.Value);
	}
	return {FOpResult::Normal, VValue(MapCopy)};
}

FOpResult VMapBase::MeltImpl(FRunningContext Context)
{
	return Copy<VMutableMap>(Context, [](FRunningContext Context, VValue Value) { return VValue::Melt(Context, Value); });
}

FOpResult VMutableMap::FreezeImpl(FRunningContext Context)
{
	return Copy<VMap>(Context, [](FRunningContext Context, VValue Value) { return VValue::Freeze(Context, Value); });
}

DEFINE_DERIVED_VCPPCLASSINFO(VMap);
DEFINE_TRIVIAL_VISIT_REFERENCES(VMap);
TGlobalTrivialEmergentTypePtr<&VMap::StaticCppClassInfo> VMap::GlobalTrivialEmergentType;

DEFINE_DERIVED_VCPPCLASSINFO(VMutableMap);
DEFINE_TRIVIAL_VISIT_REFERENCES(VMutableMap);
TGlobalTrivialEmergentTypePtr<&VMutableMap::StaticCppClassInfo> VMutableMap::GlobalTrivialEmergentType;

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
