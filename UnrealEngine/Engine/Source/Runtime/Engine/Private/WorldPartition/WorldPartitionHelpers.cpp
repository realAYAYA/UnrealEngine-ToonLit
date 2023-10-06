// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/WorldPartitionHelpers.h"

#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionLog.h"
#include "WorldPartition/WorldPartitionEditorHash.h"
#include "WorldPartition/WorldPartitionRuntimeCell.h"
#include "Algo/AnyOf.h"

#include "Commandlets/Commandlet.h"
#include "WorldPartition/WorldPartitionLog.h"

#if WITH_EDITOR
#include "Misc/RedirectCollector.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Modules/ModuleManager.h"
#endif

namespace FWorldPartitionHelpersPrivate
{
	UWorldPartition* GetWorldPartitionFromObject(const UObject* InObject)
	{
		for (const UObject* Object = InObject; IsValid(Object); Object = Object->GetOuter())
		{
			if (const AActor* Actor = Cast<const AActor>(Object))
			{
				return GetWorldPartition(Actor);
			}
			else if (const ULevel* Level = Cast<const ULevel>(Object))
			{
				return GetWorldPartition(Level);
			}
			else if (const UWorld* World = Cast<const UWorld>(Object))
			{
				return GetWorldPartition(World);
			}
			else if (const UWorldPartition* WorldPartition = Cast<const UWorldPartition>(Object))
			{
				return const_cast<UWorldPartition*>(WorldPartition);
			}
		}

		return nullptr;
	}
}

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
				UE_LOG(LogWorldPartition, Warning, TEXT("Failed to find actor base class: %s."), *ActorBasePath.ToString());
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

				if (!Params.bKeepReferences && (Params.bGCPerActor || FWorldPartitionHelpers::ShouldCollectGarbage()))
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

	return bHasExceededMaxMemory;
}

bool FWorldPartitionHelpers::ShouldCollectGarbage()
{
	return HasExceededMaxMemory();
}

void FWorldPartitionHelpers::DoCollectGarbage()
{
	const FPlatformMemoryStats MemStatsBefore = FPlatformMemory::GetStats();
	CollectGarbage(IsRunningCommandlet() ? RF_NoFlags : GARBAGE_COLLECTION_KEEPFLAGS, true);
	const FPlatformMemoryStats MemStatsAfter = FPlatformMemory::GetStats();

	UE_LOG(LogWorldPartition, Log, TEXT("GC Performed - Available Physical: %.2fGB, Available Virtual: %.2fGB"),
		(int64)MemStatsAfter.AvailablePhysical / (1024.0 * 1024.0 * 1024.0),
		(int64)MemStatsAfter.AvailableVirtual / (1024.0 * 1024.0 * 1024.0)
	);
}

void FWorldPartitionHelpers::FakeEngineTick(UWorld* InWorld)
{
	check(InWorld);

	CommandletHelpers::TickEngine(InWorld);
}

bool FWorldPartitionHelpers::ConvertRuntimePathToEditorPath(const FSoftObjectPath& InPath, FSoftObjectPath& OutPath)
{
	//
	// Try to convert from /.../WorldName/_Generated_/MainGrid_L0_X0_Y0_DL0.WorldName:PersistentLevel.ActorName
	//                  to /.../WorldName.WorldName:PersistentLevel.ActorName
	//
	FString OutPathString = InPath.ToString();
	const FStringView InPathView(OutPathString);

	if (int32 GeneratedPos = InPathView.Find(TEXTVIEW("/_Generated_/"), 0); GeneratedPos != INDEX_NONE)
	{
		if (int32 NextDotPos = InPathView.Find(TEXTVIEW("."), GeneratedPos); NextDotPos != INDEX_NONE)
		{
			if (int32 NextColonPos = InPathView.Find(TEXTVIEW(":"), NextDotPos); NextColonPos != INDEX_NONE)
			{
				OutPathString.RemoveAt(GeneratedPos, NextDotPos - GeneratedPos);

				// In the editor, the _LevelInstance_ID is appended to the persistent level, while at runtime it is appended to each cell package, so we need to remap it there if present.
				FString WorldAssetPackageName = InPath.GetAssetPath().GetPackageName().ToString();
				const int32 LevelInstancePos = WorldAssetPackageName.Find(TEXT("_LevelInstance_"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
				if (LevelInstancePos != INDEX_NONE)
				{
					FString LevelInstanceTag = WorldAssetPackageName.RightChop(LevelInstancePos);
					OutPathString.InsertAt(GeneratedPos, LevelInstanceTag);
				}

				OutPath = OutPathString;
				return true;
			}
		}
	}
	else
	{
		OutPath = UWorld::RemovePIEPrefix(InPath.ToString());
		return true;
	}

	return false;
}

bool FWorldPartitionHelpers::FixupRedirectedAssetPath(FSoftObjectPath& InOutSoftObjectPath)
{
	if (InOutSoftObjectPath.IsNull())
	{
		// Empty path, no redirect
		return true;
	}

	// Check GRedirectCollector first for faster fixup
	FSoftObjectPath FoundRedirection = GRedirectCollector.GetAssetPathRedirection(InOutSoftObjectPath.GetWithoutSubPath());
	if (!FoundRedirection.IsNull())
	{
		InOutSoftObjectPath.SetPath(FoundRedirection.GetAssetPath(), InOutSoftObjectPath.GetSubPathString());
		return true;
	}

	if (InOutSoftObjectPath.GetAssetName().IsEmpty())
	{
		// A package name. No need to ask the asset registry for assets, it wont find any
		return true;
	}

	const FAssetData* AssetData;
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

	FTopLevelAssetPath AssetPath = InOutSoftObjectPath.GetAssetPath();

	for (;;)
	{
		TArray<FAssetData> Assets;
		AssetRegistry.ScanFilesSynchronous({ AssetPath.GetPackageName().ToString() }, /*bForceRescan*/false);
		AssetRegistry.GetAssetsByPackageName(AssetPath.GetPackageName(), Assets, /*bIncludeOnlyOnDiskAssets*/true);

		if (!Assets.Num())
		{
			UE_LOG(LogWorldPartition, Warning, TEXT("Failed to find assets for asset path '%s'"), *AssetPath.ToString());
			return false;
		}

		AssetData = Assets.FindByPredicate([&AssetPath](const FAssetData& AssetData)
		{
			return (AssetData.ToSoftObjectPath().GetAssetPath() == AssetPath);
		});

		if (!AssetData)
		{
			UE_LOG(LogWorldPartition, Warning, TEXT("Failed to find asset for asset path '%s'"), *AssetPath.ToString());
			return false;
		}

		if (!AssetData->IsRedirector())
		{
			break;
		}

		FString DestinationObjectPath;
		if (!AssetData->GetTagValue(TEXT("DestinationObject"), DestinationObjectPath))
		{
			UE_LOG(LogWorldPartition, Warning, TEXT("Failed to follow redirector for '%s'"), *AssetPath.ToString());
			return false;
		}

		// Update asset path
		AssetPath = FTopLevelAssetPath(DestinationObjectPath);
	}

	InOutSoftObjectPath.SetPath(AssetPath, InOutSoftObjectPath.GetSubPathString());

	return true;
}

bool FWorldPartitionHelpers::FixupRedirectedAssetPath(FName& InOutAssetPath)
{
	FSoftObjectPath SoftObjectPath(InOutAssetPath.ToString());
	if (FixupRedirectedAssetPath(SoftObjectPath))
	{
		InOutAssetPath = FName(*SoftObjectPath.ToString());
		return true;
	}

	return false;
}

#endif // #if WITH_EDITOR

bool FWorldPartitionHelpers::ConvertEditorPathToRuntimePath(const FSoftObjectPath& InPath, FSoftObjectPath& OutPath)
{
	//
	// Try to convert from /.../WorldName.WorldName:PersistentLevel.ActorName
	//                  to /.../WorldName/_Generated_/MainGrid_L0_X0_Y0_DL0.WorldName:PersistentLevel.ActorName
	//
	FSoftObjectPath Path(InPath);
	FSoftObjectPath WorldPath(Path.GetAssetPath(), FString());

	if (UWorld* World = Cast<UWorld>(WorldPath.ResolveObject()))
	{
		if (UWorldPartition* WorldPartition = World->GetWorldPartition())
		{
			return WorldPartition->ConvertEditorPathToRuntimePath(InPath, OutPath);
		}
	}

	return false;
}