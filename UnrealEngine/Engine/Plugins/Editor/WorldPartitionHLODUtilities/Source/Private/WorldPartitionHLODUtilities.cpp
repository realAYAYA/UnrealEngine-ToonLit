// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartitionHLODUtilities.h"

#if WITH_EDITOR

#include "WorldPartition/ActorDescContainer.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionHandle.h"
#include "WorldPartition/DataLayer/DataLayerInstance.h"
#include "WorldPartition/HLOD/HLODActor.h"
#include "WorldPartition/HLOD/HLODSubActor.h"
#include "WorldPartition/HLOD/HLODActorDesc.h"
#include "WorldPartition/HLOD/HLODBuilder.h"
#include "WorldPartition/HLOD/HLODLayer.h"
#include "WorldPartition/WorldPartitionLevelStreamingDynamic.h"

#include "HLODBuilderInstancing.h"
#include "HLODBuilderMeshMerge.h"
#include "HLODBuilderMeshSimplify.h"
#include "HLODBuilderMeshApproximate.h"

#include "Algo/Transform.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/InstancedStaticMesh.h"
#include "Engine/StaticMesh.h"
#include "HAL/FileManager.h"
#include "Materials/MaterialInstance.h"
#include "PhysicsEngine/BodySetup.h"
#include "ProfilingDebugging/ScopedTimers.h"
#include "Serialization/ArchiveCrc32.h"
#include "StaticMeshCompiler.h"
#include "Templates/UniquePtr.h"
#include "TextureCompiler.h"
#include "UObject/GCObjectScopeGuard.h"


static UWorldPartitionLevelStreamingDynamic* CreateLevelStreamingFromHLODActor(AWorldPartitionHLOD* InHLODActor, bool& bOutDirty)
{
	UPackage::WaitForAsyncFileWrites();

	bOutDirty = false;
	UWorld* World = InHLODActor->GetWorld();
	UWorldPartition* WorldPartition = World->GetWorldPartition();
	check(WorldPartition);
	
	const FName LevelStreamingName = FName(*FString::Printf(TEXT("HLODLevelStreaming_%s"), *InHLODActor->GetName()));
	TArray<FWorldPartitionRuntimeCellObjectMapping> Mappings;
	Mappings.Reserve(InHLODActor->GetSubActors().Num());
	// @todo_ow: investigate if we need to pass in a content bundle id here
	Algo::Transform(InHLODActor->GetSubActors(), Mappings, [World](const FHLODSubActor& SubActor) { return FWorldPartitionRuntimeCellObjectMapping(SubActor.ActorPackage, SubActor.ActorPath, SubActor.ContainerID, SubActor.ContainerTransform, SubActor.ContainerPackage, World->GetPackage()->GetFName(), FGuid()); });

	UWorldPartitionLevelStreamingDynamic* LevelStreaming = UWorldPartitionLevelStreamingDynamic::LoadInEditor(World, LevelStreamingName, Mappings);
	check(LevelStreaming);

	if (!LevelStreaming->GetLoadSucceeded())
	{
		bOutDirty = true;
		UE_LOG(LogHLODBuilder, Warning, TEXT("HLOD actor \"%s\" needs to be rebuilt as it didn't succeed in loading all actors."), *InHLODActor->GetActorLabel());
	}

	return LevelStreaming;
}

static uint32 GetCRC(const UHLODLayer* InHLODLayer)
{
	UHLODLayer& HLODLayer= *const_cast<UHLODLayer*>(InHLODLayer);

	uint32 CRC;

	CRC = GetTypeHash(HLODLayer.GetLayerType());
	UE_LOG(LogHLODBuilder, VeryVerbose, TEXT(" - LayerType = %d"), CRC);

	CRC = HashCombine(HLODLayer.GetHLODBuilderSettings()->GetCRC(), CRC);
	UE_LOG(LogHLODBuilder, VeryVerbose, TEXT(" - HLODBuilderSettings = %d"), CRC);

	CRC = HashCombine(HLODLayer.GetCellSize(), CRC);
	UE_LOG(LogHLODBuilder, VeryVerbose, TEXT(" - CellSize = %d"), CRC);

	return CRC;
}

static uint32 ComputeHLODHash(AWorldPartitionHLOD* InHLODActor, const TArray<AActor*>& InActors)
{
	FArchiveCrc32 Ar;

	// Base key, changing this will force a rebuild of all HLODs
	FString HLODBaseKey = "5052091956924DB3BD9ACE00B71944AC";
	Ar << HLODBaseKey;

	// HLOD Layer
	uint32 HLODLayerHash = GetCRC(InHLODActor->GetSubActorsHLODLayer());
	UE_LOG(LogHLODBuilder, VeryVerbose, TEXT(" - HLOD Layer (%s) = %x"), *InHLODActor->GetSubActorsHLODLayer()->GetName(), HLODLayerHash);
	Ar << HLODLayerHash;

	// Min Visible Distance
	uint32 HLODMinVisibleDistanceHash = GetTypeHash(InHLODActor->GetMinVisibleDistance());
	UE_LOG(LogHLODBuilder, VeryVerbose, TEXT(" - HLOD Min Visible Distance (%.02f) = %x"), InHLODActor->GetMinVisibleDistance(), HLODMinVisibleDistanceHash);
	Ar << HLODMinVisibleDistanceHash;

	// Append all components CRCs
	uint32 HLODComponentsHash = UHLODBuilder::ComputeHLODHash(InActors);
	UE_LOG(LogHLODBuilder, VeryVerbose, TEXT(" - HLOD Source Components = %x"), HLODComponentsHash);
	Ar << HLODComponentsHash;

	return Ar.GetCrc();
}

TArray<AWorldPartitionHLOD*> FWorldPartitionHLODUtilities::CreateHLODActors(FHLODCreationContext& InCreationContext, const FHLODCreationParams& InCreationParams, const TArray<IStreamingGenerationContext::FActorInstance>& InActors, const TArray<const UDataLayerInstance*>& InDataLayersInstances)
{
	struct FSubActorsInfo
	{
		TArray<FHLODSubActor>	SubActors;
		bool					bIsSpatiallyLoaded;
	};
	TMap<UHLODLayer*, FSubActorsInfo> SubActorsInfos;

	for (const IStreamingGenerationContext::FActorInstance& ActorInstance : InActors)
	{
		const FWorldPartitionActorDescView& ActorDescView = ActorInstance.GetActorDescView();
		if (ActorDescView.GetActorIsHLODRelevant())
		{
			UHLODLayer* HLODLayer = UHLODLayer::GetHLODLayer(ActorDescView, InCreationParams.WorldPartition);
			if (HLODLayer)
			{
				FSubActorsInfo& SubActorsInfo = SubActorsInfos.FindOrAdd(HLODLayer);

				// Leaving this as deprecated for now until we fix up the serialization format for SubActorsInfo and do proper upgrade/deprecation 
PRAGMA_DISABLE_DEPRECATION_WARNINGS
				SubActorsInfo.SubActors.Emplace(ActorDescView.GetGuid(), ActorDescView.GetActorPackage(), ActorDescView.GetActorPath(), ActorInstance.GetContainerID(), ActorInstance.GetActorDescContainer()->GetContainerPackage(), ActorInstance.GetTransform());
PRAGMA_ENABLE_DEPRECATION_WARNINGS
				if (ActorDescView.GetIsSpatiallyLoaded())
				{
					SubActorsInfo.bIsSpatiallyLoaded = true;
				}
			}
		}
	}

	TArray<AWorldPartitionHLOD*> HLODActors;
	for (const auto& Pair : SubActorsInfos)
	{
		const UHLODLayer* HLODLayer = Pair.Key;
		const FSubActorsInfo& SubActorsInfo = Pair.Value;
		check(!SubActorsInfo.SubActors.IsEmpty());

		auto ComputeCellHash = [](const FString& HLODLayerName, const FName CellName)
		{
			uint64 HLODLayerNameHash = FCrc::StrCrc32(*HLODLayerName);
			uint32 CellNameHash = FCrc::StrCrc32(*CellName.ToString());
			return HashCombine(HLODLayerNameHash, CellNameHash);
		};

		uint64 CellHash = ComputeCellHash(HLODLayer->GetName(), InCreationParams.CellName);
		FName HLODActorName = *FString::Printf(TEXT("%s_%016llx"), *HLODLayer->GetName(), CellHash);		

		AWorldPartitionHLOD* HLODActor = nullptr;
		FWorldPartitionHandle HLODActorHandle;
		if (InCreationContext.HLODActorDescs.RemoveAndCopyValue(HLODActorName, HLODActorHandle))
		{
			InCreationContext.ActorReferences.Add(HLODActorHandle.ToReference());
			HLODActor = CastChecked<AWorldPartitionHLOD>(HLODActorHandle->GetActor());
		}

		bool bNewActor = HLODActor == nullptr;
		if (bNewActor)
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.Name = HLODActorName;
			SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Required_Fatal;
			HLODActor = InCreationParams.WorldPartition->GetWorld()->SpawnActor<AWorldPartitionHLOD>(SpawnParams);

			HLODActor->SetSourceCellName(InCreationParams.CellName);
			HLODActor->SetSubActorsHLODLayer(HLODLayer);

			// Make sure the generated HLOD actor has the same data layers as the source actors
			for (const UDataLayerInstance* DataLayerInstance : InDataLayersInstances)
			{
				HLODActor->AddDataLayer(DataLayerInstance);
			}
		}
		else
		{
#if DO_CHECK
			check(HLODActor->GetSourceCellName() == InCreationParams.CellName);
			check(HLODActor->GetSubActorsHLODLayer() == HLODLayer);
#endif
		}

		bool bIsDirty = false;

		// Sub actors
		{
			bool bSubActorsChanged = HLODActor->GetSubActors().Num() != SubActorsInfo.SubActors.Num();
			if (!bSubActorsChanged)
			{
				TArray<FHLODSubActor> A = HLODActor->GetSubActors();
				TArray<FHLODSubActor> B = SubActorsInfo.SubActors;
				A.Sort();
				B.Sort();
				bSubActorsChanged = A != B;
			}

			if (bSubActorsChanged)
			{
				HLODActor->SetSubActors(SubActorsInfo.SubActors);
				bIsDirty = true;
			}
		}

		// Runtime grid
		FName RuntimeGrid = HLODLayer->GetRuntimeGrid(InCreationParams.HLODLevel);
		if (HLODActor->GetRuntimeGrid() != RuntimeGrid)
		{
			HLODActor->SetRuntimeGrid(RuntimeGrid);
			bIsDirty = true;
		}

		// Spatially loaded
		// HLOD that are always loaded will not take the SubActorsInfo.GridPlacement into account
		bool bExpectedIsSpatiallyLoaded = !HLODLayer->IsSpatiallyLoaded() ? false : SubActorsInfo.bIsSpatiallyLoaded;
		if (HLODActor->GetIsSpatiallyLoaded() != bExpectedIsSpatiallyLoaded)
		{
			HLODActor->SetIsSpatiallyLoaded(bExpectedIsSpatiallyLoaded);
			bIsDirty = true;
		}

		// HLOD level
		if (HLODActor->GetLODLevel() != InCreationParams.HLODLevel)
		{
			HLODActor->SetLODLevel(InCreationParams.HLODLevel);
			bIsDirty = true;
		}

		// Require warmup
		if (HLODActor->DoesRequireWarmup() != HLODLayer->DoesRequireWarmup())
		{
			HLODActor->SetRequireWarmup(HLODLayer->DoesRequireWarmup());
			bIsDirty = true;
		}

		// Parent HLOD layer
		UHLODLayer* ParentHLODLayer = HLODLayer->GetParentLayer().LoadSynchronous();
		if (HLODActor->GetHLODLayer() != ParentHLODLayer)
		{
			HLODActor->SetHLODLayer(ParentHLODLayer);
			bIsDirty = true;
		}

		// Actor label
		const FString ActorLabel = FString::Printf(TEXT("%s/%s"), *HLODLayer->GetName(), *InCreationParams.CellName.ToString());
		if (HLODActor->GetActorLabel() != ActorLabel)
		{
			HLODActor->SetActorLabel(ActorLabel);
			bIsDirty = true;
		}

		// Folder name
		const FName FolderPath(FString::Printf(TEXT("HLOD/%s"), *HLODLayer->GetName()));
		if (HLODActor->GetFolderPath() != FolderPath)
		{
			HLODActor->SetFolderPath(FolderPath);
			bIsDirty = true;
		}

		// Cell bounds
		if (!HLODActor->GetHLODBounds().Equals(InCreationParams.CellBounds))
		{
			HLODActor->SetHLODBounds(InCreationParams.CellBounds);
			bIsDirty = true;
		}

		// Minimum visible distance
		if (!FMath::IsNearlyEqual(HLODActor->GetMinVisibleDistance(), InCreationParams.MinVisibleDistance))
		{
			HLODActor->SetMinVisibleDistance(InCreationParams.MinVisibleDistance);
			bIsDirty = true;
		}

		// If any change was performed, mark HLOD package as dirty
		if (bIsDirty)
		{
			HLODActor->MarkPackageDirty();
		}

		HLODActors.Add(HLODActor);
	}

	return HLODActors;
}

TSubclassOf<UHLODBuilder> FWorldPartitionHLODUtilities::GetHLODBuilderClass(const UHLODLayer* InHLODLayer)
{
	EHLODLayerType HLODLayerType = InHLODLayer->GetLayerType();
	switch (HLODLayerType)
	{
	case EHLODLayerType::Instancing:
		return UHLODBuilderInstancing::StaticClass();
		break;

	case EHLODLayerType::MeshMerge:
		return UHLODBuilderMeshMerge::StaticClass();
		break;

	case EHLODLayerType::MeshSimplify:
		return UHLODBuilderMeshSimplify::StaticClass();
		break;

	case EHLODLayerType::MeshApproximate:
		return UHLODBuilderMeshApproximate::StaticClass();
		break;

	case EHLODLayerType::Custom:
		return InHLODLayer->GetHLODBuilderClass();
		break;

	default:
		checkf(false, TEXT("Unsupported type"));
		return nullptr;
	}
}

UHLODBuilderSettings* FWorldPartitionHLODUtilities::CreateHLODBuilderSettings(UHLODLayer* InHLODLayer)
{
	// Retrieve the HLOD builder class
	TSubclassOf<UHLODBuilder> HLODBuilderClass = GetHLODBuilderClass(InHLODLayer);
	if (!HLODBuilderClass)
	{
		return NewObject<UHLODBuilderSettings>(InHLODLayer, UHLODBuilderSettings::StaticClass());
	}

	// Retrieve the HLOD builder settings class
	TSubclassOf<UHLODBuilderSettings> HLODBuilderSettingsClass = HLODBuilderClass->GetDefaultObject<UHLODBuilder>()->GetSettingsClass();
	if (!ensure(HLODBuilderSettingsClass))
	{
		return NewObject<UHLODBuilderSettings>(InHLODLayer, UHLODBuilderSettings::StaticClass());
	}

	UHLODBuilderSettings* HLODBuilderSettings = NewObject<UHLODBuilderSettings>(InHLODLayer, HLODBuilderSettingsClass);

	// Deprecated properties handling
	if (InHLODLayer->GetHLODBuilderSettings() == nullptr)
	{
		EHLODLayerType HLODLayerType = InHLODLayer->GetLayerType();
		switch (HLODLayerType)
		{
		case EHLODLayerType::MeshMerge:
			CastChecked<UHLODBuilderMeshMergeSettings>(HLODBuilderSettings)->MeshMergeSettings = InHLODLayer->MeshMergeSettings_DEPRECATED;
			CastChecked<UHLODBuilderMeshMergeSettings>(HLODBuilderSettings)->HLODMaterial = InHLODLayer->HLODMaterial_DEPRECATED;
			break;

		case EHLODLayerType::MeshSimplify:
			CastChecked<UHLODBuilderMeshSimplifySettings>(HLODBuilderSettings)->MeshSimplifySettings = InHLODLayer->MeshSimplifySettings_DEPRECATED;
			CastChecked<UHLODBuilderMeshSimplifySettings>(HLODBuilderSettings)->HLODMaterial = InHLODLayer->HLODMaterial_DEPRECATED;
			break;

		case EHLODLayerType::MeshApproximate:
			CastChecked<UHLODBuilderMeshApproximateSettings>(HLODBuilderSettings)->MeshApproximationSettings = InHLODLayer->MeshApproximationSettings_DEPRECATED;
			CastChecked<UHLODBuilderMeshApproximateSettings>(HLODBuilderSettings)->HLODMaterial = InHLODLayer->HLODMaterial_DEPRECATED;
			break;
		};
	}

	return HLODBuilderSettings;
}

void GatherInputStats(AWorldPartitionHLOD* InHLODActor, const TArray<AActor*> SubActors)
{
	int64 NumActors = 0;
	int64 NumTriangles = 0;
	int64 NumVertices = 0;

	for (AActor* SubActor : SubActors)
	{
		if (SubActor && SubActor->IsHLODRelevant())
		{
			if (AWorldPartitionHLOD* SubHLODActor = Cast<AWorldPartitionHLOD>(SubActor))
			{
				NumActors += SubHLODActor->GetStat(FWorldPartitionHLODStats::InputActorCount);
				NumTriangles += SubHLODActor->GetStat(FWorldPartitionHLODStats::InputTriangleCount);
				NumVertices += SubHLODActor->GetStat(FWorldPartitionHLODStats::InputVertexCount);				
			}
			else
			{
				NumActors++;

				SubActor->ForEachComponent<UStaticMeshComponent>(false, [&] (UStaticMeshComponent* SMComponent)
				{
					const UStaticMesh* StaticMesh = SMComponent->GetStaticMesh();
					const FStaticMeshRenderData* RenderData = StaticMesh ? StaticMesh->GetRenderData() : nullptr;
					const bool bHasRenderData = RenderData && !RenderData->LODResources.IsEmpty();

					if (bHasRenderData)
					{
						const UInstancedStaticMeshComponent* ISMComponent = Cast<UInstancedStaticMeshComponent>(SMComponent);
						const int64 LOD0TriCount = RenderData->LODResources[0].GetNumTriangles();
						const int64 LOD0VtxCount = RenderData->LODResources[0].GetNumVertices();
						const int64 NumInstances = ISMComponent ? ISMComponent->GetInstanceCount() : 1;

						NumTriangles += LOD0TriCount * NumInstances;
						NumVertices += LOD0VtxCount * NumInstances;
					}
				});
			}
		}
	}

	InHLODActor->SetStat(FWorldPartitionHLODStats::InputActorCount, NumActors);
	InHLODActor->SetStat(FWorldPartitionHLODStats::InputTriangleCount, NumTriangles);
	InHLODActor->SetStat(FWorldPartitionHLODStats::InputVertexCount, NumVertices);	
}

void GatherOutputStats(AWorldPartitionHLOD* InHLODActor)
{
	const UPackage* HLODPackage = InHLODActor->GetPackage();

	// Gather relevant assets and process them outside of this ForEach as it's possible that async compilation completion
	// triggers insertion of new objects, which would break the iteration over UObjects
	TArray<UStaticMesh*> StaticMeshes;
	TArray<UTexture*> Textures;
	TArray<UMaterialInstance*> MaterialInstances;
	ForEachObjectWithPackage(HLODPackage, [&](UObject* Object)
	{
		if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(Object))
		{
			StaticMeshes.Add(StaticMesh);
		}
		else if (UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(Object))
		{
			MaterialInstances.Add(MaterialInstance);
		}
		else if (UTexture* Texture = Cast<UTexture>(Object))
		{
			Textures.Add(Texture);
		}
		return true;
	}, false);

	// Process static meshes
	int64 MeshResourceSize = 0;
	{
		int64 InstanceCount = 0;
		int64 NaniteTriangleCount = 0;
		int64 NaniteVertexCount = 0;
		int64 TriangleCount = 0;
		int64 VertexCount = 0;
		int64 UVChannelCount = 0;

		InHLODActor->ForEachComponent<UInstancedStaticMeshComponent>(false, [&](const UInstancedStaticMeshComponent* ISMC)
		{
			InstanceCount += ISMC->GetInstanceCount();
		});

		FStaticMeshCompilingManager::Get().FinishCompilation(StaticMeshes);

		for (UStaticMesh* StaticMesh : StaticMeshes)
		{
			MeshResourceSize += StaticMesh->GetResourceSizeBytes(EResourceSizeMode::Exclusive);

			TriangleCount += StaticMesh->GetNumTriangles(0);
			VertexCount += StaticMesh->GetNumVertices(0);
			UVChannelCount = FMath::Max(UVChannelCount, StaticMesh->GetNumTexCoords(0));

			NaniteTriangleCount += StaticMesh->GetNumNaniteTriangles();
			NaniteVertexCount += StaticMesh->GetNumNaniteVertices();
		}

		// Mesh stats
		InHLODActor->SetStat(FWorldPartitionHLODStats::MeshInstanceCount, InstanceCount);
		InHLODActor->SetStat(FWorldPartitionHLODStats::MeshNaniteTriangleCount, NaniteTriangleCount);
		InHLODActor->SetStat(FWorldPartitionHLODStats::MeshNaniteVertexCount, NaniteVertexCount);
		InHLODActor->SetStat(FWorldPartitionHLODStats::MeshTriangleCount, TriangleCount);
		InHLODActor->SetStat(FWorldPartitionHLODStats::MeshVertexCount, VertexCount);
		InHLODActor->SetStat(FWorldPartitionHLODStats::MeshUVChannelCount, UVChannelCount);
	}

	// Process materials
	int64 TexturesResourceSize = 0;
	{
		int64 BaseColorTextureSize = 0;
		int64 NormalTextureSize = 0;
		int64 EmissiveTextureSize = 0;
		int64 MetallicTextureSize = 0;
		int64 RoughnessTextureSize = 0;
		int64 SpecularTextureSize = 0;

		FTextureCompilingManager::Get().FinishCompilation(Textures);

		for (UMaterialInstance* MaterialInstance : MaterialInstances)
		{
			// Retrieve the texture size for a texture that can have different names
			auto GetTextureSize = [&](const TArray<FName>& TextureParamNames) -> int64
			{
				for (FName TextureParamName : TextureParamNames)
				{
					UTexture* Texture = nullptr;
					MaterialInstance->GetTextureParameterValue(TextureParamName, Texture, true);

					if (Texture && Texture->GetPackage() == HLODPackage)
					{
						TexturesResourceSize += Texture->GetResourceSizeBytes(EResourceSizeMode::Exclusive);
						return Texture->GetSurfaceWidth();
					}
				}

				return 0;
			};

			int64 LocalBaseColorTextureSize = GetTextureSize({ "BaseColorTexture", "DiffuseTexture" });
			int64 LocalNormalTextureSize = GetTextureSize({ "NormalTexture" });
			int64 LocalEmissiveTextureSize = GetTextureSize({ "EmissiveTexture", "EmissiveColorTexture" });
			int64 LocalMetallicTextureSize = GetTextureSize({ "MetallicTexture" });
			int64 LocalRoughnessTextureSize = GetTextureSize({ "RoughnessTexture" });
			int64 LocalSpecularTextureSize = GetTextureSize({ "SpecularTexture" });

			int64 MRSTextureSize = GetTextureSize({ "PackedTexture" });
			if (MRSTextureSize != 0)
			{
				LocalMetallicTextureSize = LocalRoughnessTextureSize = LocalSpecularTextureSize = MRSTextureSize;
			}

			BaseColorTextureSize = FMath::Max(BaseColorTextureSize, LocalBaseColorTextureSize);
			NormalTextureSize = FMath::Max(NormalTextureSize, LocalNormalTextureSize);
			EmissiveTextureSize = FMath::Max(EmissiveTextureSize, LocalEmissiveTextureSize);
			MetallicTextureSize = FMath::Max(MetallicTextureSize, LocalMetallicTextureSize);
			RoughnessTextureSize = FMath::Max(RoughnessTextureSize, LocalRoughnessTextureSize);
			SpecularTextureSize = FMath::Max(SpecularTextureSize, LocalSpecularTextureSize);
		}

		// Material stats
		InHLODActor->SetStat(FWorldPartitionHLODStats::MaterialBaseColorTextureSize, BaseColorTextureSize);
		InHLODActor->SetStat(FWorldPartitionHLODStats::MaterialNormalTextureSize, NormalTextureSize);
		InHLODActor->SetStat(FWorldPartitionHLODStats::MaterialEmissiveTextureSize, EmissiveTextureSize);
		InHLODActor->SetStat(FWorldPartitionHLODStats::MaterialMetallicTextureSize, MetallicTextureSize);
		InHLODActor->SetStat(FWorldPartitionHLODStats::MaterialRoughnessTextureSize, RoughnessTextureSize);
		InHLODActor->SetStat(FWorldPartitionHLODStats::MaterialSpecularTextureSize, SpecularTextureSize);
	}

	// Memory stats
	InHLODActor->SetStat(FWorldPartitionHLODStats::MemoryMeshResourceSizeBytes, MeshResourceSize);
	InHLODActor->SetStat(FWorldPartitionHLODStats::MemoryTexturesResourceSizeBytes, TexturesResourceSize);
	InHLODActor->SetStat(FWorldPartitionHLODStats::MemoryDiskSizeBytes, 0); // Clear disk size as it is unknown at this point. It will be assigned at package loading
}

uint32 FWorldPartitionHLODUtilities::BuildHLOD(AWorldPartitionHLOD* InHLODActor)
{
	FAutoScopedDurationTimer TotalTimeScope;

	// Keep track of timings related to this build
	int64 LoadTimeMS = 0;
	int64 BuildTimeMS = 0;
	int64 TotalTimeMS = 0;
	
	// Load actors relevant to HLODs
	bool bIsDirty = false;
	UWorldPartitionLevelStreamingDynamic* LevelStreaming = nullptr;
	{
		FAutoScopedDurationTimer LoadTimeScope;
		LevelStreaming = CreateLevelStreamingFromHLODActor(InHLODActor, bIsDirty);
		LoadTimeMS = LoadTimeScope.GetTime() * 1000;
	}

	ON_SCOPE_EXIT
	{
		UWorldPartitionLevelStreamingDynamic::UnloadFromEditor(LevelStreaming);
	};

	uint32 OldHLODHash = bIsDirty ? 0 : InHLODActor->GetHLODHash();
	uint32 NewHLODHash = ComputeHLODHash(InHLODActor, LevelStreaming->GetLoadedLevel()->Actors);

	if (OldHLODHash == NewHLODHash)
	{
		UE_LOG(LogHLODBuilder, Verbose, TEXT("HLOD actor \"%s\" doesn't need to be rebuilt."), *InHLODActor->GetActorLabel());
		return OldHLODHash;
	}

	// Clear stats as we're about to refresh them
	InHLODActor->ResetStats();
	
	// Gather stats from the input to our HLOD build
	GatherInputStats(InHLODActor, LevelStreaming->GetLoadedLevel()->Actors);
	
	const UHLODLayer* HLODLayer = InHLODActor->GetSubActorsHLODLayer();
	TSubclassOf<UHLODBuilder> HLODBuilderClass = GetHLODBuilderClass(HLODLayer);

	if (HLODBuilderClass)
	{
		UHLODBuilder* HLODBuilder = NewObject<UHLODBuilder>(GetTransientPackage(), HLODBuilderClass);
		if (ensure(HLODBuilder))
		{
			FGCObjectScopeGuard BuilderGCScopeGuard(HLODBuilder);

			HLODBuilder->SetHLODBuilderSettings(HLODLayer->GetHLODBuilderSettings());

			FHLODBuildContext HLODBuildContext;
			HLODBuildContext.World = InHLODActor->GetWorld();
			HLODBuildContext.AssetsOuter = InHLODActor->GetPackage();
			HLODBuildContext.AssetsBaseName = InHLODActor->GetActorLabel();
			HLODBuildContext.MinVisibleDistance = InHLODActor->GetMinVisibleDistance();

			// Build
			TArray<UActorComponent*> HLODComponents;
			{
				FAutoScopedDurationTimer BuildTimeScope;
				HLODComponents = HLODBuilder->Build(HLODBuildContext, LevelStreaming->GetLoadedLevel()->Actors);
				BuildTimeMS = BuildTimeScope.GetTime() * 1000;
			}

			if (HLODComponents.IsEmpty())
			{
				UE_LOG(LogHLODBuilder, Warning, TEXT("HLOD generation created no component for %s"), *InHLODActor->GetActorLabel());
			}

			// Ideally, this should be performed elsewhere, to allow more flexibility in the HLOD generation
			for (UActorComponent* HLODComponent : HLODComponents)
			{
				HLODComponent->SetCanEverAffectNavigation(false);

				if (USceneComponent* SceneComponent = Cast<USceneComponent>(HLODComponent))
				{
					// Change Mobility to be Static
					SceneComponent->SetMobility(EComponentMobility::Static);

					// Enable bounds optimizations
					SceneComponent->bComputeFastLocalBounds = true;
					SceneComponent->bComputeBoundsOnceForGame = true;
				}

				if (UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(HLODComponent))
				{
					// Disable collisions
					PrimitiveComponent->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
					PrimitiveComponent->SetGenerateOverlapEvents(false);
					PrimitiveComponent->CanCharacterStepUpOn = ECanBeCharacterBase::ECB_No;
					PrimitiveComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				}

				if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(HLODComponent))
				{
					if (UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh())
					{
						// If the HLOD process did create this static mesh
						if (StaticMesh->GetPackage() == HLODBuildContext.AssetsOuter)
						{
							// Set up ray tracing far fields for always loaded HLODs
							if (!HLODLayer->IsSpatiallyLoaded() && StaticMesh->bSupportRayTracing)
							{
								StaticMeshComponent->bRayTracingFarField = true;
							}

							// Disable collisions
							StaticMesh->MarkAsNotHavingNavigationData();
							if (UBodySetup* BodySetup = StaticMesh->GetBodySetup())
							{
								BodySetup->DefaultInstance.SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
								BodySetup->CollisionTraceFlag = CTF_UseSimpleAsComplex;
							}

							// Rename owned static mesh
							StaticMesh->Rename(*MakeUniqueObjectName(StaticMesh->GetOuter(), StaticMesh->GetClass(), *FString::Printf(TEXT("StaticMesh_%s"), *HLODLayer->GetName())).ToString());
						}
					}
				}
			}

			InHLODActor->SetHLODComponents(HLODComponents);
		}
	}

	// Gather stats pertaining to the assets generated during this build
	GatherOutputStats(InHLODActor);

	TotalTimeMS = TotalTimeScope.GetTime() * 1000;

	// Build timings stats
	InHLODActor->SetStat(FWorldPartitionHLODStats::BuildTimeLoadMilliseconds, LoadTimeMS);
	InHLODActor->SetStat(FWorldPartitionHLODStats::BuildTimeBuildMilliseconds, BuildTimeMS);
	InHLODActor->SetStat(FWorldPartitionHLODStats::BuildTimeTotalMilliseconds, TotalTimeMS);

	return NewHLODHash;
}

#endif
