// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/WorldPartitionBlueprintLibrary.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WorldPartitionBlueprintLibrary)

#if WITH_EDITOR
#include "Subsystems/UnrealEditorSubsystem.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionActorDesc.h"
#include "WorldPartition/WorldPartitionHelpers.h"
#include "Editor.h"
#endif

FActorDesc::FActorDesc()
	: Bounds(ForceInit)
	, bIsSpatiallyLoaded(false)
	, bActorIsEditorOnly(false)
{}

#if WITH_EDITOR
FActorDesc::FActorDesc(const FWorldPartitionActorDesc& InActorDesc, const FTransform& InTransform)
{
	Guid = InActorDesc.GetGuid();

	if (!InActorDesc.GetBaseClass().IsNull())
	{
		Class = FSoftObjectPath(InActorDesc.GetBaseClass(), {});
	}
	else
	{
		Class = FSoftObjectPath(InActorDesc.GetActorNativeClass());
	}

	Name = InActorDesc.GetActorName();
	Label = InActorDesc.GetActorLabel();
	Bounds = InActorDesc.GetBounds().TransformBy(InTransform);
	RuntimeGrid = InActorDesc.GetRuntimeGrid();
	bIsSpatiallyLoaded = InActorDesc.GetIsSpatiallyLoaded();
	bActorIsEditorOnly = InActorDesc.GetActorIsEditorOnly();
	ActorPackage = InActorDesc.GetActorPackage();
	
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	ActorPath = InActorDesc.GetActorPath();
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

TMap<UWorldPartition*, TUniquePtr<FLoaderAdapterActorList>> UWorldPartitionBlueprintLibrary::LoaderAdapterActorListMap;
FDelegateHandle UWorldPartitionBlueprintLibrary::OnWorldPartitionUninitializedHandle;

UWorld* UWorldPartitionBlueprintLibrary::GetEditorWorld()
{
	UUnrealEditorSubsystem* UnrealEditorSubsystem = GEditor->GetEditorSubsystem<UUnrealEditorSubsystem>();
	return UnrealEditorSubsystem ? UnrealEditorSubsystem->GetEditorWorld() : nullptr;
}

UWorldPartition* UWorldPartitionBlueprintLibrary::GetWorldPartition()
{
	if (UWorld* World = GetEditorWorld())
	{
		return World->GetWorldPartition();
	}
	return nullptr;
}

void UWorldPartitionBlueprintLibrary::OnWorldPartitionUninitialized(UWorldPartition* InWorldPartition)
{
	if (LoaderAdapterActorListMap.Remove(InWorldPartition) && LoaderAdapterActorListMap.IsEmpty())
	{
		InWorldPartition->GetWorld()->OnWorldPartitionUninitialized().Remove(OnWorldPartitionUninitializedHandle);
	}
}

bool UWorldPartitionBlueprintLibrary::GetActorDescs(const UWorldPartition* WorldPartiton, TArray<FActorDesc>& OutActorDescs)
{
	bool bResult = true;

	WorldPartiton->ForEachActorDescContainer([&](const UActorDescContainer* ActorDescContainer)
	{
		bResult &= GetActorDescs(ActorDescContainer, FTransform::Identity, OutActorDescs);
	});
	
	return bResult;
}

bool UWorldPartitionBlueprintLibrary::GetActorDescs(const UActorDescContainer* InContainer, const FTransform& InTransform, TArray<FActorDesc>& OutActorDescs)
{
	bool bResult = true;

	for (FActorDescList::TConstIterator<> ActorDescIt(InContainer); ActorDescIt; ++ActorDescIt)
	{
		const UActorDescContainer* SubContainer;
		EContainerClusterMode SubClusterMode;
		FTransform SubTransform;

		if (ActorDescIt->IsContainerInstance())
		{
			if (ActorDescIt->GetContainerInstance(SubContainer, SubTransform, SubClusterMode))
			{
				bResult &= GetActorDescs(SubContainer, SubTransform * InTransform, OutActorDescs);
			}
			else
			{
				bResult = false;
			}
		}
		else
		{
			OutActorDescs.Emplace(**ActorDescIt, InTransform);
		}
	}

	return bResult;
}

bool UWorldPartitionBlueprintLibrary::HandleIntersectingActorDesc(const FWorldPartitionActorDesc* ActorDesc, const FBox& InBox, const FTransform& InTransform, TArray<FActorDesc>& OutActorDescs)
{
	bool bResult = true;
	const UActorDescContainer* SubContainer;
	EContainerClusterMode SubClusterMode;
	FTransform SubTransform;

	if (ActorDesc->IsContainerInstance())
	{
		if (ActorDesc->GetContainerInstance(SubContainer, SubTransform, SubClusterMode))
		{
			bResult &= GetIntersectingActorDescs(SubContainer, InBox, SubTransform * InTransform, OutActorDescs);
		}
		else
		{
			bResult = false;
		}
	}
	else
	{
		OutActorDescs.Emplace(*ActorDesc, InTransform);
	}

	return bResult;
}

bool UWorldPartitionBlueprintLibrary::GetIntersectingActorDescs(UWorldPartition* WorldPartition, const FBox& InBox, TArray<FActorDesc>& OutActorDescs)
{
	bool bResult = true;

	FWorldPartitionHelpers::ForEachIntersectingActorDesc(WorldPartition, InBox, [&bResult, &InBox, &OutActorDescs](const FWorldPartitionActorDesc* ActorDesc)
	{
		bResult &= HandleIntersectingActorDesc(ActorDesc, InBox, FTransform::Identity, OutActorDescs);
		return true;
	});

	return bResult;
}

bool UWorldPartitionBlueprintLibrary::GetIntersectingActorDescs(const UActorDescContainer* InContainer, const FBox& InBox, const FTransform& InTransform, TArray<FActorDesc>& OutActorDescs)
{
	bool bResult = true;

	for (FActorDescList::TConstIterator<> ActorDescIt(InContainer); ActorDescIt; ++ActorDescIt)
	{
		if (ActorDescIt->GetBounds().Intersect(InBox))
		{
			bResult &= HandleIntersectingActorDesc(*ActorDescIt, InBox, InTransform, OutActorDescs);
		}
	}

	return bResult;
}
#endif

FBox UWorldPartitionBlueprintLibrary::GetEditorWorldBounds()
{
#if WITH_EDITOR
	if (UWorldPartition* WorldPartition = GetWorldPartition())
	{
		return WorldPartition->GetEditorWorldBounds();
	}
#endif
	return FBox(ForceInit);
}

FBox UWorldPartitionBlueprintLibrary::GetRuntimeWorldBounds()
{
#if WITH_EDITOR
	if (UWorldPartition* WorldPartition = GetWorldPartition())
	{
		return WorldPartition->GetRuntimeWorldBounds();
	}
#endif
	return FBox(ForceInit);
}

void UWorldPartitionBlueprintLibrary::LoadActors(const TArray<FGuid>& InActorsToLoad)
{
#if WITH_EDITOR
	if (UWorldPartition* WorldPartition = GetWorldPartition())
	{
		if (LoaderAdapterActorListMap.IsEmpty())
		{
			OnWorldPartitionUninitializedHandle = WorldPartition->GetWorld()->OnWorldPartitionUninitialized().AddStatic(&UWorldPartitionBlueprintLibrary::OnWorldPartitionUninitialized);
		}

		TUniquePtr<FLoaderAdapterActorList>& LoaderAdapterActorList = LoaderAdapterActorListMap.FindOrAdd(WorldPartition, MakeUnique<FLoaderAdapterActorList>(WorldPartition->GetWorld()));

		LoaderAdapterActorList->AddActors(InActorsToLoad);
	}
#endif
}

void UWorldPartitionBlueprintLibrary::UnloadActors(const TArray<FGuid>& InActorsToLoad)
{
#if WITH_EDITOR
	if (UWorldPartition* WorldPartition = GetWorldPartition())
	{
		if (TUniquePtr<FLoaderAdapterActorList>* LoaderAdapterActorList = LoaderAdapterActorListMap.Find(WorldPartition))
		{
			(*LoaderAdapterActorList)->RemoveActors(InActorsToLoad);
		}
	}
#endif
}

bool UWorldPartitionBlueprintLibrary::GetActorDescs(TArray<FActorDesc>& OutActorDescs)
{
#if WITH_EDITOR
	if (UWorldPartition* WorldPartition = GetWorldPartition())
	{
		return GetActorDescs(WorldPartition, OutActorDescs);
	}
#endif
	return false;
}

bool UWorldPartitionBlueprintLibrary::GetIntersectingActorDescs(const FBox& InBox, TArray<FActorDesc>& OutActorDescs)
{
#if WITH_EDITOR
	if (UWorldPartition* WorldPartition = GetWorldPartition())
	{
		return GetIntersectingActorDescs(WorldPartition, InBox, OutActorDescs);
	}
#endif
	return false;
}
