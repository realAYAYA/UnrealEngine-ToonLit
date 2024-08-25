// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/WorldPartitionRuntimeVirtualTextureBuilder.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionActorDesc.h"
#include "WorldPartition/WorldPartitionHelpers.h"
#include "WorldPartition/WorldPartitionHandle.h"
#include "WorldPartition/WorldPartitionActorDescInstance.h"
#include "Components/RuntimeVirtualTextureComponent.h"
#include "Components/PrimitiveComponent.h"
#include "VT/VirtualTextureBuilder.h"
#include "VirtualTexturingEditorModule.h"
#include "AssetCompilingManager.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectIterator.h"
#include "ComponentRecreateRenderStateContext.h"
#include "Materials/MaterialInstance.h"
#include "Materials/Material.h"
#include "ShaderCompiler.h"

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
	FWorldPartitionHelpers::ForEachActorWithLoading(WorldPartition, [](const FWorldPartitionActorDescInstance*) { return true; }, ForEachActorWithLoadingParams, Result);

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
	
	// Recreate render state after shader compilation complete
	{
		FGlobalComponentRecreateRenderStateContext Context;
	}

	// We will need to build VTs for both shading paths
	const ERHIFeatureLevel::Type CurFeatureLevel = World->GetFeatureLevel();
	const ERHIFeatureLevel::Type AltFeatureLevel = (CurFeatureLevel == ERHIFeatureLevel::ES3_1 ? GMaxRHIFeatureLevel : ERHIFeatureLevel::ES3_1);
	const EShadingPath CurShadingPath = FSceneInterface::GetShadingPath(CurFeatureLevel);
	const EShadingPath AltShadingPath = FSceneInterface::GetShadingPath(AltFeatureLevel);

	TSet<UPackage*> ModifiedPackages;
	TArray<URuntimeVirtualTextureComponent*> Components[2];
	for (TObjectIterator<URuntimeVirtualTextureComponent> It(RF_ClassDefaultObject, false, EInternalObjectFlags::Garbage); It; ++It)
	{
		if (It->GetWorld() == World)
		{
			if (VTModule.HasStreamedMips(CurShadingPath, *It))
			{
				Components[0].Add(*It);
			}

			if (VTModule.HasStreamedMips(AltShadingPath, *It))
			{
				Components[1].Add(*It);
			}
		}
	}

	// Build for a current feature level first
	if (Components[0].Num() != 0)
	{
		for (URuntimeVirtualTextureComponent* Component : Components[0])
		{
			if (VTModule.BuildStreamedMips(CurShadingPath, Component))
			{
				if (UVirtualTextureBuilder* VTBuilder = Component->GetStreamingTexture(); VTBuilder->GetPackage()->IsDirty())
				{
					ModifiedPackages.Add(VTBuilder->GetPackage());
				}
			}
		}
	}

	// Build for others if any
	if (Components[1].Num() != 0)
	{
		// Commandlets do not initialize shader resources for alternate feature levels, do it now
		{
			bool bUpdateProgressDialog = false;
			bool bCacheAllRemainingShaders = true;
			UMaterialInterface::SetGlobalRequiredFeatureLevel(AltFeatureLevel, true);
			UMaterial::AllMaterialsCacheResourceShadersForRendering(bUpdateProgressDialog, bCacheAllRemainingShaders);
			UMaterialInstance::AllMaterialsCacheResourceShadersForRendering(bUpdateProgressDialog, bCacheAllRemainingShaders);
			CompileGlobalShaderMap(AltFeatureLevel);
		}
	
		World->ChangeFeatureLevel(AltFeatureLevel);
		
		// Make sure all assets are finished compiling. Recreate render state after shader compilation complete
		{
			UMaterialInterface::SubmitRemainingJobsForWorld(World);
			FAssetCompilingManager::Get().FinishAllCompilation();
			FAssetCompilingManager::Get().ProcessAsyncTasks();
			FGlobalComponentRecreateRenderStateContext Context;
		}
				
		for (URuntimeVirtualTextureComponent* Component : Components[1])
		{
			if (VTModule.BuildStreamedMips(AltShadingPath, Component))
			{
				if (UVirtualTextureBuilder* VTBuilder = Component->GetStreamingTexture(); VTBuilder->GetPackage()->IsDirty())
				{
					ModifiedPackages.Add(VTBuilder->GetPackage());
				}
			}
		}
	}

	// Restore world feature level
	World->ChangeFeatureLevel(CurFeatureLevel);

	// Wait for VT Textures to be ready before saving
	FAssetCompilingManager::Get().FinishAllCompilation();

	return SavePackages(ModifiedPackages.Array(), PackageHelper, false);
}
