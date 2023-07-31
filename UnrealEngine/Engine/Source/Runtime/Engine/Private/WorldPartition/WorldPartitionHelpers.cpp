// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/WorldPartitionHelpers.h"

#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionEditorHash.h"
#include "Algo/AnyOf.h"

#include "Commandlets/Commandlet.h"

DEFINE_LOG_CATEGORY_STATIC(LogWorldPartitionHelpers, Log, All);

#if WITH_EDITOR

bool FWorldPartitionHelpers::IsActorDescClassCompatibleWith(const FWorldPartitionActorDesc* ActorDesc, const UClass* Class)
{
	check(Class);

	UClass* ActorNativeClass = ActorDesc->GetActorNativeClass();
	UClass* ActorBaseClass = ActorNativeClass;

	if (!Class->IsNative())
	{
		if (FTopLevelAssetPath ActorBasePath = ActorDesc->GetBaseClass(); !ActorBasePath.IsNull())
		{
			ActorBaseClass = LoadClass<AActor>(nullptr, *ActorBasePath.ToString(), nullptr, LOAD_None, nullptr);

			if (!ActorBaseClass)
			{
				UE_LOG(LogWorldPartitionHelpers, Warning, TEXT("Failed to find actor base class: %s."), *ActorBasePath.ToString());
				ActorBaseClass = ActorNativeClass;
			}
		}
	}

	return ActorBaseClass->IsChildOf(Class);
}

void FWorldPartitionHelpers::ForEachIntersectingActorDesc(UWorldPartition* WorldPartition, const FBox& Box, TSubclassOf<AActor> ActorClass, TFunctionRef<bool(const FWorldPartitionActorDesc*)> Func)
{
	WorldPartition->EditorHash->ForEachIntersectingActor(Box, [&ActorClass, Func](const FWorldPartitionActorDesc* ActorDesc)
	{
		if (IsActorDescClassCompatibleWith(ActorDesc, ActorClass))
		{
			Func(ActorDesc);
		}
	});
}

void FWorldPartitionHelpers::ForEachActorDesc(UWorldPartition* WorldPartition, TSubclassOf<AActor> ActorClass, TFunctionRef<bool(const FWorldPartitionActorDesc*)> Func)
{
	for (FActorDescContainerCollection::TConstIterator<> ActorDescIterator(WorldPartition); ActorDescIterator; ++ActorDescIterator)
	{
		if (IsActorDescClassCompatibleWith(*ActorDescIterator, ActorClass))
		{
			if (!Func(*ActorDescIterator))
			{
				return;
			}
		}
	}
}

namespace WorldPartitionHelpers
{
	void LoadReferencesInternal(UWorldPartition* WorldPartition, const FGuid& ActorGuid, TMap<FGuid, FWorldPartitionReference>& InOutActorReferences)
	{
		if (InOutActorReferences.Contains(ActorGuid))
		{
			return;
		}

		if (const FWorldPartitionActorDesc* ActorDesc = WorldPartition->GetActorDesc(ActorGuid))
		{
			InOutActorReferences.Emplace(ActorGuid);

			for (FGuid ReferenceGuid : ActorDesc->GetReferences())
			{
				LoadReferencesInternal(WorldPartition, ReferenceGuid, InOutActorReferences);
			}

			InOutActorReferences[ActorGuid] = FWorldPartitionReference(WorldPartition, ActorGuid);
		}
	}

	void LoadReferences(UWorldPartition* WorldPartition, const FGuid& ActorGuid, TMap<FGuid, FWorldPartitionReference>& InOutActorReferences)
	{
		FWorldPartitionLoadingContext::FDeferred LoadingContext;
		LoadReferencesInternal(WorldPartition, ActorGuid, InOutActorReferences);
	}
}

FWorldPartitionHelpers::FForEachActorWithLoadingParams::FForEachActorWithLoadingParams()
	: bGCPerActor(false)
	, bKeepReferences(false)
	, ActorClasses({ AActor::StaticClass() })
{}

void FWorldPartitionHelpers::ForEachActorWithLoading(UWorldPartition* WorldPartition, TSubclassOf<AActor> ActorClass, TFunctionRef<bool(const FWorldPartitionActorDesc*)> Func, TFunctionRef<void()> OnReleasingActorReferences, bool bGCPerActor)
{
	FForEachActorWithLoadingParams Params;
	Params.ActorClasses = { ActorClass };
	Params.OnPreGarbageCollect = [&OnReleasingActorReferences]() { OnReleasingActorReferences(); };
	ForEachActorWithLoading(WorldPartition, Func, Params);
}

void FWorldPartitionHelpers::ForEachActorWithLoading(UWorldPartition* WorldPartition, const TArray<FGuid>& ActorGuids, TFunctionRef<bool(const FWorldPartitionActorDesc*)> Func, TFunctionRef<void()> OnReleasingActorReferences, bool bGCPerActor)
{
	TSet<FGuid> ActorGuidsSet(ActorGuids);
	FForEachActorWithLoadingParams Params;
	Params.FilterActorDesc = [&ActorGuidsSet](const FWorldPartitionActorDesc* ActorDesc) -> bool { return ActorGuidsSet.Contains(ActorDesc->GetGuid());	};
	Params.OnPreGarbageCollect = [&OnReleasingActorReferences]() { OnReleasingActorReferences(); };	
	ForEachActorWithLoading(WorldPartition, Func, Params);
}

void FWorldPartitionHelpers::ForEachActorWithLoading(UWorldPartition* WorldPartition, TFunctionRef<bool(const FWorldPartitionActorDesc*)> Func, const FForEachActorWithLoadingParams& Params)
{
	FForEachActorWithLoadingResult Result;
	ForEachActorWithLoading(WorldPartition, Func, Params, Result);
}

void FWorldPartitionHelpers::ForEachActorWithLoading(UWorldPartition* WorldPartition, TFunctionRef<bool(const FWorldPartitionActorDesc*)> Func, const FForEachActorWithLoadingParams& Params, FForEachActorWithLoadingResult& Result)
{
	check(Result.ActorReferences.IsEmpty());
	auto CallGarbageCollect = [&Params, &Result]()
	{
		check(!Params.bKeepReferences);
		if (Params.OnPreGarbageCollect)
		{
			Params.OnPreGarbageCollect();
		}
		Result.ActorReferences.Empty();
		DoCollectGarbage();
	};

	auto ForEachActorWithLoadingImpl = [&](const FWorldPartitionActorDesc* ActorDesc)
	{
		if (Algo::AnyOf(Params.ActorClasses, [ActorDesc](UClass* ActorClass) { return IsActorDescClassCompatibleWith(ActorDesc, ActorClass); }))
		{
			if (!Params.FilterActorDesc || Params.FilterActorDesc(ActorDesc))
			{
				WorldPartitionHelpers::LoadReferences(WorldPartition, ActorDesc->GetGuid(), Result.ActorReferences);

				FWorldPartitionReference ActorReference(WorldPartition, ActorDesc->GetGuid());
				if (!Func(ActorReference.Get()))
				{
					return false;
				}

				if (!Params.bKeepReferences && (Params.bGCPerActor || FWorldPartitionHelpers::HasExceededMaxMemory()))
				{
					CallGarbageCollect();
				}
			}
		}

		return true;
	};

	if (Params.ActorGuids.IsEmpty())
	{
		for (FActorDescContainerCollection::TConstIterator<> ActorDescIterator(WorldPartition); ActorDescIterator; ++ActorDescIterator)
		{
			if (const FWorldPartitionActorDesc* ActorDesc = *ActorDescIterator)
			{
				if (!ForEachActorWithLoadingImpl(ActorDesc))
				{
					break;
				}
			}
			
		}
	}
	else
	{
		for (const FGuid& ActorGuid : Params.ActorGuids)
		{
			if (const FWorldPartitionActorDesc* ActorDesc = WorldPartition->GetActorDesc(ActorGuid))
			{
				if (!ForEachActorWithLoadingImpl(ActorDesc))
				{
					break;
				}
			}
		}
	}

	if (!Params.bKeepReferences)
	{
		CallGarbageCollect();
	}
}

bool FWorldPartitionHelpers::HasExceededMaxMemory()
{
	const FPlatformMemoryStats MemStats = FPlatformMemory::GetStats();

	const uint64 MemoryMinFreePhysical = 1llu * 1024 * 1024 * 1024;
	const uint64 MemoryMaxUsedPhysical = FMath::Max(32llu * 1024 * 1024 * 1024, MemStats.TotalPhysical / 2);

	const bool bHasExceededMinFreePhysical = MemStats.AvailablePhysical < MemoryMinFreePhysical;
	const bool bHasExceededMaxUsedPhysical = MemStats.UsedPhysical >= MemoryMaxUsedPhysical;
	const bool bHasExceededMaxMemory = bHasExceededMinFreePhysical || bHasExceededMaxUsedPhysical;

	// Even if we're not exhausting memory, GC should be run at periodic intervals
	return bHasExceededMaxMemory || (FPlatformTime::Seconds() - GetLastGCTime()) > 30;
};

void FWorldPartitionHelpers::DoCollectGarbage()
{
	const FPlatformMemoryStats MemStatsBefore = FPlatformMemory::GetStats();
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS, true);
	const FPlatformMemoryStats MemStatsAfter = FPlatformMemory::GetStats();

	UE_LOG(LogWorldPartition, Log, TEXT("GC Performed - Available Physical: %.2fGB, Available Virtual: %.2fGB"),
		(int64)MemStatsAfter.AvailablePhysical / (1024.0 * 1024.0 * 1024.0),
		(int64)MemStatsAfter.AvailableVirtual / (1024.0 * 1024.0 * 1024.0)
	);
};

void FWorldPartitionHelpers::FakeEngineTick(UWorld* InWorld)
{
	check(InWorld);

	CommandletHelpers::TickEngine(InWorld);
}

#endif // #if WITH_EDITOR
