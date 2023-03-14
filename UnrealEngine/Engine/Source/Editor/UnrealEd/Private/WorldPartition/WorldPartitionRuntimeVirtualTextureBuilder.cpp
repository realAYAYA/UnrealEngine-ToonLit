// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/WorldPartitionRuntimeVirtualTextureBuilder.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionActorDesc.h"
#include "WorldPartition/WorldPartitionHelpers.h"
#include "WorldPartition/WorldPartitionHandle.h"
#include "Components/RuntimeVirtualTextureComponent.h"
#include "Components/PrimitiveComponent.h"
#include "VT/VirtualTextureBuilder.h"
#include "VirtualTexturingEditorModule.h"
#include "AssetCompilingManager.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectIterator.h"

DEFINE_LOG_CATEGORY_STATIC(LogWorldPartitionRuntimeVirtualTextureBuilder, All, All);

UWorldPartitionRuntimeVirtualTextureBuilder::UWorldPartitionRuntimeVirtualTextureBuilder(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UWorldPartitionRuntimeVirtualTextureBuilder::LoadRuntimeVirtualTextureActors(UWorldPartition* WorldPartition, FWorldPartitionHelpers::FForEachActorWithLoadingResult& Result)
{
	check(WorldPartition);

	FWorldPartitionHelpers::FForEachActorWithLoadingParams ForEachActorWithLoadingParams;
	ForEachActorWithLoadingParams.bKeepReferences = true;
	ForEachActorWithLoadingParams.FilterActorDesc = [](const FWorldPartitionActorDesc* ActorDesc) -> bool { return ActorDesc->HasProperty(UPrimitiveComponent::RVTActorDescProperty); };
		
	// @todo: in order to scale the RVTs should be generated with tiling so that we don't need to load all actors writing to RVTs at once.
	FWorldPartitionHelpers::ForEachActorWithLoading(WorldPartition, [](const FWorldPartitionActorDesc*) { return true; }, ForEachActorWithLoadingParams, Result);

	// Make sure all assets are finished compiling
	FAssetCompilingManager::Get().FinishAllCompilation();
}

bool UWorldPartitionRuntimeVirtualTextureBuilder::RunInternal(UWorld* World, const FCellInfo& InCellInfo, FPackageSourceControlHelper& PackageHelper)
{
	IVirtualTexturingEditorModule& VTModule = FModuleManager::Get().LoadModuleChecked<IVirtualTexturingEditorModule>("VirtualTexturingEditor");
	
	UWorldPartition* WorldPartition = World->GetWorldPartition();
	if (!WorldPartition)
	{
		UE_LOG(LogWorldPartitionRuntimeVirtualTextureBuilder, Error, TEXT("Failed to retrieve WorldPartition."));
		return false;
	}
		
	// Load required actors
	FWorldPartitionHelpers::FForEachActorWithLoadingResult ForEachActorWithLoadingResult;
	LoadRuntimeVirtualTextureActors(WorldPartition, ForEachActorWithLoadingResult);
		
	TSet<UPackage*> ModifiedPackages;
	for (TObjectIterator<URuntimeVirtualTextureComponent> It(RF_ClassDefaultObject, false, EInternalObjectFlags::Garbage); It; ++It)
	{
		if (It->GetWorld() == World)
		{
			if (VTModule.HasStreamedMips(*It) && VTModule.BuildStreamedMips(*It))
			{
				if (UVirtualTextureBuilder* VTBuilder = It->GetStreamingTexture(); VTBuilder->GetPackage()->IsDirty())
				{
					ModifiedPackages.Add(VTBuilder->GetPackage());
				}
			}
		}
	}

	// Wait for VT Textures to be ready before saving
	FAssetCompilingManager::Get().FinishAllCompilation();

	return SavePackages(ModifiedPackages.Array(), PackageHelper, false);
}
