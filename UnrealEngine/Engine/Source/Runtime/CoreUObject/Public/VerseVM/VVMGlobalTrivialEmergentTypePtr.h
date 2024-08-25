// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VVMEmergentType.h"
#include "VVMGlobalHeapRoot.h"
#include "VVMLazyInitialized.h"
#include "VVMMarkStack.h"
#include "VVMWriteBarrier.h"

namespace Verse
{

struct FGlobalTrivialEmergentTypePtrRoot : FGlobalHeapRoot
{
	FGlobalTrivialEmergentTypePtrRoot(FAccessContext Context, VEmergentType* Type)
		: EmergentType(Context, Type)
	{
	}

	COREUOBJECT_API void Visit(FAbstractVisitor& Visitor) override;
	COREUOBJECT_API void Visit(FMarkStackVisitor& Visitor) override;

	TWriteBarrier<VEmergentType> EmergentType;

private:
	template <typename TVisitor>
	FORCEINLINE void VisitImpl(TVisitor&);
};

template <VCppClassInfo* ClassInfo>
struct TGlobalTrivialEmergentTypePtr
{
	TGlobalTrivialEmergentTypePtr() = default;

	VEmergentType& Get(FAllocationContext Context)
	{
		VEmergentType* Result = EmergentType.load(std::memory_order_relaxed);
		std::atomic_signal_fence(std::memory_order_seq_cst);
		if (Result)
		{
			return *Result;
		}
		else
		{
			return GetSlow(Context);
		}
	}

private:
	FORCENOINLINE VEmergentType& GetSlow(FAllocationContext Context)
	{
		VEmergentType* Object = VEmergentType::New(Context, VTrivialType::Singleton.Get(), ClassInfo);
		VEmergentType* Expected = nullptr;
		EmergentType.compare_exchange_strong(Expected, Object);
		VEmergentType* Result;
		if (Expected)
		{
			Result = Expected;
		}
		else
		{
			Result = Object;
			new FGlobalTrivialEmergentTypePtrRoot(Context, Object);
		}
		V_DIE_UNLESS(EmergentType.load() == Result);
		return *Result;
	}

	std::atomic<VEmergentType*> EmergentType = nullptr;
};

} // namespace Verse
#endif // WITH_VERSE_VM
