// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/HLOD/HLODSubsystem.h"
#include "WorldPartition/HLOD/HLODActor.h"
#include "WorldPartition/HLOD/HLODActorDesc.h"
#include "WorldPartition/HLOD/HLODLayer.h"
#include "WorldPartition/DataLayer/DataLayerInstance.h"
#include "WorldPartition/DataLayer/DataLayerSubsystem.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionSubsystem.h"
#include "WorldPartition/WorldPartitionRuntimeCell.h"
#include "WorldPartition/WorldPartitionRuntimeHash.h"
#include "WorldPartition/WorldPartitionLevelStreamingDynamic.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectGlobals.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineModule.h"
#include "EngineUtils.h"
#include "LevelUtils.h"
#include "Components/StaticMeshComponent.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Templates/UniquePtr.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HLODSubsystem)

#define LOCTEXT_NAMESPACE "HLODSubsystem"

DEFINE_LOG_CATEGORY_STATIC(LogHLODSubsystem, Log, All);

static TAutoConsoleVariable<int32> CVarHLODWarmupNumFrames(
	TEXT("wp.Runtime.HLOD.WarmupNumFrames"),
	5,
	TEXT("Delay unloading of a cell for this amount of frames to ensure HLOD assets are ready to be shown at the proper resolution. Set to 0 to force disable warmup."),
	ECVF_Default
);

static TAutoConsoleVariable<int32> CVarHLODWarmupEnabled(
	TEXT("wp.Runtime.HLOD.WarmupEnabled"),
	1,
	TEXT("Enable HLOD assets warmup. Will delay unloading of cells & transition to HLODs for wp.Runtime.HLOD.WarmupNumFrames frames."),
	ECVF_Default
);

static TAutoConsoleVariable<int32> CVarHLODWarmupDebugDraw(
	TEXT("wp.Runtime.HLOD.WarmupDebugDraw"),
	0,
	TEXT("Draw debug display for the warmup requests"),
	ECVF_Default
);

static TAutoConsoleVariable<float> CVarHLODWarmupVTScaleFactor(
	TEXT("wp.Runtime.HLOD.WarmupVTScaleFactor"),
	2.0f,
	TEXT("Scale the VT size we ask to prefetch by this factor."),
	ECVF_Default
);

static TAutoConsoleVariable<int32> CVarHLODWarmupVTSizeClamp(
	TEXT("wp.Runtime.HLOD.WarmupVTSizeClamp"),
	2048,
	TEXT("Clamp VT warmup requests for safety."),
	ECVF_Default
);

namespace FHLODSubsystem
{
	UWorldPartition* GetWorldPartition(AWorldPartitionHLOD* InWorldPartitionHLOD)
	{
		// Alwaysloaded Cell level will have a WorldPartition
		if (UWorldPartition* WorldPartition = InWorldPartitionHLOD->GetLevel()->GetWorldPartition())
		{
			return WorldPartition;
		} // If not find it through the cell
		else if (UWorldPartitionLevelStreamingDynamic* LevelStreaming = Cast<UWorldPartitionLevelStreamingDynamic>(FLevelUtils::FindStreamingLevel(InWorldPartitionHLOD->GetLevel())))
		{
			return LevelStreaming->GetWorldPartitionRuntimeCell()->GetCellOwner()->GetOuterWorld()->GetWorldPartition();;
		}

		return nullptr;
	}
};

UHLODSubsystem::UHLODSubsystem()
	: UWorldSubsystem()
{
}

UHLODSubsystem::~UHLODSubsystem()
{
}

bool UHLODSubsystem::WorldPartitionHLODEnabled = true;

FAutoConsoleCommand UHLODSubsystem::EnableHLODCommand(
	TEXT("wp.Runtime.HLOD"),
	TEXT("Turn on/off loading & rendering of world partition HLODs."),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		UHLODSubsystem::WorldPartitionHLODEnabled = (Args.Num() != 1) || (Args[0] != TEXT("0"));
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (World && World->IsGameWorld())
			{
				if (UHLODSubsystem* HLODSubSystem = World->GetSubsystem<UHLODSubsystem>()) 
				{
					for (const auto& KeyValuePair : HLODSubSystem->WorldPartitionsHLODRuntimeData)
					{
						for (const auto& CellHLODMapping : KeyValuePair.Value.CellsData)
						{
							const FCellData& CellData = CellHLODMapping.Value;
							bool bIsHLODVisible = UHLODSubsystem::WorldPartitionHLODEnabled && !CellData.bIsCellVisible;
							for (AWorldPartitionHLOD* HLODActor : CellData.LoadedHLODs)
							{
								HLODActor->SetVisibility(bIsHLODVisible);
							}
						}
					}
				}
			}
		}
	})
);

bool UHLODSubsystem::IsHLODEnabled()
{
	return UHLODSubsystem::WorldPartitionHLODEnabled;
}

bool UHLODSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UHLODSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	// Ensure the WorldPartitionSubsystem gets created before the HLODSubsystem
	Collection.InitializeDependency<UWorldPartitionSubsystem>();

	Super::Initialize(Collection);

	UWorld* World = GetWorld();
	check(World->IsGameWorld());

	GetWorld()->OnWorldPartitionInitialized().AddUObject(this, &UHLODSubsystem::OnWorldPartitionInitialized);
	GetWorld()->OnWorldPartitionUninitialized().AddUObject(this, &UHLODSubsystem::OnWorldPartitionUninitialized);

	SceneViewExtension = FSceneViewExtensions::NewExtension<FHLODResourcesResidencySceneViewExtension>(World);
}

void UHLODSubsystem::Deinitialize()
{
	Super::Deinitialize();

	GetWorld()->OnWorldPartitionInitialized().RemoveAll(this);
	GetWorld()->OnWorldPartitionUninitialized().RemoveAll(this);
}

void UHLODSubsystem::OnWorldPartitionInitialized(UWorldPartition* InWorldPartition)
{
	if (InWorldPartition && InWorldPartition->IsStreamingEnabled())
	{
		check(!WorldPartitionsHLODRuntimeData.Contains(InWorldPartition));

		FWorldPartitionHLODRuntimeData& WorldPartitionHLODRuntimeData = WorldPartitionsHLODRuntimeData.Add(InWorldPartition, FWorldPartitionHLODRuntimeData());
	
		// Build cell to HLOD mapping
		if (InWorldPartition && InWorldPartition->RuntimeHash)
		{
			InWorldPartition->RuntimeHash->ForEachStreamingCells([&WorldPartitionHLODRuntimeData](const UWorldPartitionRuntimeCell* Cell)
			{
				WorldPartitionHLODRuntimeData.CellsData.Emplace(Cell->GetFName());
				return true;
			});
		}
	}
}

void UHLODSubsystem::OnWorldPartitionUninitialized(UWorldPartition* InWorldPartition)
{
	if (InWorldPartition && InWorldPartition->IsStreamingEnabled())
	{
		check(WorldPartitionsHLODRuntimeData.Contains(InWorldPartition));
		WorldPartitionsHLODRuntimeData.Remove(InWorldPartition);
	}
}

void UHLODSubsystem::RegisterHLODActor(AWorldPartitionHLOD* InWorldPartitionHLOD)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UHLODSubsystem::RegisterHLODActor);

	const UWorldPartition* WorldPartition = FHLODSubsystem::GetWorldPartition(InWorldPartitionHLOD);
	check(WorldPartition && WorldPartition->IsStreamingEnabled());

	FWorldPartitionHLODRuntimeData* WorldPartitionHLODRuntimeData = WorldPartitionsHLODRuntimeData.Find(WorldPartition);
	if (WorldPartitionHLODRuntimeData)
	{
		const FName CellName = InWorldPartitionHLOD->GetSourceCellName();
		FCellData* CellData = WorldPartitionHLODRuntimeData->CellsData.Find(CellName);

#if WITH_EDITOR
		UE_LOG(LogHLODSubsystem, Verbose, TEXT("Registering HLOD %s (%s) for cell %s"), *InWorldPartitionHLOD->GetActorLabel(), *InWorldPartitionHLOD->GetActorGuid().ToString(), *CellName.ToString());
#endif

		if (CellData)
		{
			CellData->LoadedHLODs.Add(InWorldPartitionHLOD);
			InWorldPartitionHLOD->SetVisibility(UHLODSubsystem::WorldPartitionHLODEnabled && !CellData->bIsCellVisible);
		}
		else
		{
			UE_LOG(LogHLODSubsystem, Verbose, TEXT("Found HLOD referencing nonexistent cell '%s'"), *CellName.ToString());
			InWorldPartitionHLOD->SetVisibility(false);
		}
	}
}

void UHLODSubsystem::UnregisterHLODActor(AWorldPartitionHLOD* InWorldPartitionHLOD)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UHLODSubsystem::UnregisterHLODActor);

	const UWorldPartition* WorldPartition = FHLODSubsystem::GetWorldPartition(InWorldPartitionHLOD);
	check(WorldPartition && WorldPartition->IsStreamingEnabled());

	FWorldPartitionHLODRuntimeData* WorldPartitionHLODRuntimeData = WorldPartitionsHLODRuntimeData.Find(WorldPartition);
	if (WorldPartitionHLODRuntimeData)
	{
		const FName CellName = InWorldPartitionHLOD->GetSourceCellName();
		FCellData* CellData = WorldPartitionHLODRuntimeData->CellsData.Find(CellName);
		if (CellData)
		{
#if WITH_EDITOR
			UE_LOG(LogHLODSubsystem, Verbose, TEXT("Unregistering HLOD %s (%s) for cell %s"), *InWorldPartitionHLOD->GetActorLabel(), *InWorldPartitionHLOD->GetActorGuid().ToString(), *CellName.ToString());
#endif

			int32 NumRemoved = CellData->LoadedHLODs.Remove(InWorldPartitionHLOD);
			check(NumRemoved == 1);
		}
	}
}

void UHLODSubsystem::OnCellShown(const UWorldPartitionRuntimeCell* InCell)
{
	const UWorldPartition* WorldPartition = InCell->GetCellOwner()->GetOuterWorld()->GetWorldPartition();
	check(WorldPartition && WorldPartition->IsStreamingEnabled());

	FWorldPartitionHLODRuntimeData* WorldPartitionHLODRuntimeData = WorldPartitionsHLODRuntimeData.Find(WorldPartition);
	if (WorldPartitionHLODRuntimeData)
	{
		FCellData* CellData = WorldPartitionHLODRuntimeData->CellsData.Find(InCell->GetFName());
		if (CellData)
		{
			CellData->bIsCellVisible = true;

#if WITH_EDITOR
			UE_LOG(LogHLODSubsystem, Verbose, TEXT("Cell shown - %s - hiding %d HLOD actors"), *InCell->GetName(), CellData->LoadedHLODs.Num());
#endif

			for (AWorldPartitionHLOD* HLODActor : CellData->LoadedHLODs)
			{
#if WITH_EDITOR
				UE_LOG(LogHLODSubsystem, Verbose, TEXT("\t\t%s - %s"), *HLODActor->GetActorLabel(), *HLODActor->GetActorGuid().ToString());
#endif
				HLODActor->SetVisibility(false);
			}
		}
	}
}

void UHLODSubsystem::OnCellHidden(const UWorldPartitionRuntimeCell* InCell)
{
	const UWorldPartition* WorldPartition = InCell->GetCellOwner()->GetOuterWorld()->GetWorldPartition();
	check(WorldPartition && WorldPartition->IsStreamingEnabled());

	FWorldPartitionHLODRuntimeData* WorldPartitionHLODRuntimeData = WorldPartitionsHLODRuntimeData.Find(WorldPartition);
	if (WorldPartitionHLODRuntimeData)
	{
		FCellData* CellData = WorldPartitionHLODRuntimeData->CellsData.Find(InCell->GetFName());
		if (CellData)
		{
			CellData->bIsCellVisible = false;

#if WITH_EDITOR
			UE_LOG(LogHLODSubsystem, Verbose, TEXT("Cell hidden - %s - showing %d HLOD actors"), *InCell->GetName(), CellData->LoadedHLODs.Num());
#endif

			for (AWorldPartitionHLOD* HLODActor : CellData->LoadedHLODs)
			{
#if WITH_EDITOR
				UE_LOG(LogHLODSubsystem, Verbose, TEXT("\t\t%s - %s"), *HLODActor->GetActorLabel(), *HLODActor->GetActorGuid().ToString());
#endif
				HLODActor->SetVisibility(UHLODSubsystem::WorldPartitionHLODEnabled);
			}
		}
	}
}

static void PrepareVTRequests(TMap<UMaterialInterface*, float>& InOutVTRequests, UStaticMeshComponent* InStaticMeshComponent, float InPixelSize)
{
	float PixelSize = InPixelSize;

	// Assume the texture is wrapped around the object, so the screen size is actually less than the resolution we require.
	PixelSize *= CVarHLODWarmupVTScaleFactor.GetValueOnAnyThread();

	// Clamp for safety
	PixelSize = FMath::Min(PixelSize, CVarHLODWarmupVTSizeClamp.GetValueOnAnyThread());

	for (UMaterialInterface* MaterialInterface : InStaticMeshComponent->GetMaterials())
	{
		// We have a VT we'd like to prefetch, add or update a request in our request map.
		// If the texture was already requested by another component, fetch the highest required resolution only.
		float& CurrentMaxPixel = InOutVTRequests.FindOrAdd(MaterialInterface);
		CurrentMaxPixel = FMath::Max(CurrentMaxPixel, PixelSize);
	}
}

static void PrepareNaniteRequests(TSet<Nanite::FResources*>& InOutNaniteRequests, UStaticMeshComponent* InStaticMeshComponent)
{
	UStaticMesh* StaticMesh = InStaticMeshComponent->GetStaticMesh();
	if (StaticMesh && StaticMesh->HasValidNaniteData())
	{
		// UE_LOG(LogHLODSubsystem, Warning, TEXT("NanitePrefetch: %s, %d pages"), *StaticMesh->GetFullName(), StaticMesh->GetRenderData()->NaniteResources.PageStreamingStates.Num());
		InOutNaniteRequests.Add(&StaticMesh->GetRenderData()->NaniteResources);
	}
}

static float EstimateScreenSize(UStaticMeshComponent* InStaticMeshComponent, const FSceneViewFamily& InViewFamily)
{
	float MaxPixels = 0;

	// Estimate the highest screen pixel size of this component in the provided views
	for (const FSceneView* View : InViewFamily.Views)
	{
		// Make sure the HLOD actor we're about to show is actually in the frustum
		if (View->ViewFrustum.IntersectSphere(InStaticMeshComponent->Bounds.Origin, InStaticMeshComponent->Bounds.SphereRadius))
		{
			float ScreenDiameter = ComputeBoundsScreenSize(InStaticMeshComponent->Bounds.Origin, InStaticMeshComponent->Bounds.SphereRadius, *View);
			float PixelSize = ScreenDiameter * View->ViewMatrices.GetScreenScale() * 2.0f;

			MaxPixels = FMath::Max(MaxPixels, PixelSize);
		}
	}

	return MaxPixels;
}

void UHLODSubsystem::MakeRenderResourcesResident(const FCellData& CellData, const FSceneViewFamily& InViewFamily)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UHLODSubsystem::MakeRenderResourcesResident)

	TMap<UMaterialInterface*, float> VTRequests;
	TSet<Nanite::FResources*> NaniteRequests;

	// For each HLOD actor representing this cell
	for(AWorldPartitionHLOD* HLODActor : CellData.LoadedHLODs)
	{
		// Skip HLOD actors that doesn't require warmup.
		// For example, instanced HLODs, as they reuse the same meshes/textures as their source actors.
		// These resources should already be resident & at the proper resolution.
		if (!HLODActor->DoesRequireWarmup())
		{
			continue;
		}

		HLODActor->ForEachComponent<UStaticMeshComponent>(false, [&](UStaticMeshComponent* SMC)
		{
			float PixelSize = EstimateScreenSize(SMC, InViewFamily);

			if (PixelSize > 0)
			{
				PrepareVTRequests(VTRequests, SMC, PixelSize);

				// Only issue Nanite requests on the first warmup frame
				if (CellData.WarmupStartFrame == InViewFamily.FrameNumber)
				{
					PrepareNaniteRequests(NaniteRequests, SMC);
				}

#if ENABLE_DRAW_DEBUG
				if (CVarHLODWarmupDebugDraw.GetValueOnAnyThread())
				{
					const FBox& Box = SMC->CalcLocalBounds().GetBox();
					DrawDebugBox(HLODActor->GetWorld(), Box.GetCenter(), Box.GetExtent(), FColor::Yellow, /*bPersistentLine*/ false, /*Lifetime*/ 1.0f);
				}
#endif
			}
		});
	}

	if (!VTRequests.IsEmpty() || !NaniteRequests.IsEmpty())
	{
		ENQUEUE_RENDER_COMMAND(MakeHLODRenderResourcesResident)(
			[VTRequests = MoveTemp(VTRequests), NaniteRequests = MoveTemp(NaniteRequests), FeatureLevel = InViewFamily.GetFeatureLevel()](FRHICommandListImmediate& RHICmdList)
			{
				for (const TPair<UMaterialInterface*, float>& VTRequest : VTRequests)
				{
					UMaterialInterface* Material = VTRequest.Key;
					FMaterialRenderProxy* MaterialRenderProxy = Material->GetRenderProxy();

					GetRendererModule().RequestVirtualTextureTiles(MaterialRenderProxy, FVector2D(VTRequest.Value, VTRequest.Value), FeatureLevel);
				}

				const uint32 NumFramesBeforeRender = CVarHLODWarmupNumFrames.GetValueOnRenderThread();
				for (const Nanite::FResources* Resource : NaniteRequests)
				{
					GetRendererModule().PrefetchNaniteResource(Resource, NumFramesBeforeRender);
				}
			});
	}
}

bool UHLODSubsystem::RequestUnloading(const UWorldPartitionRuntimeCell* InCell)
{
	// Test if warmup is disabled globally.
	bool bPerformWarmpup = (CVarHLODWarmupEnabled.GetValueOnGameThread() != 0) && (CVarHLODWarmupNumFrames.GetValueOnGameThread() > 0) && (GetWorld()->GetNetMode() != NM_DedicatedServer);
	if (!bPerformWarmpup)
	{
		return true;
	}

	const UWorldPartition* WorldPartition = InCell->GetCellOwner()->GetOuterWorld()->GetWorldPartition();;
	FWorldPartitionHLODRuntimeData* WorldPartitionHLODRuntimeData = WorldPartitionsHLODRuntimeData.Find(WorldPartition);
	if (!WorldPartitionHLODRuntimeData)
	{
		return true;
	}

	FCellData* CellData = WorldPartitionHLODRuntimeData->CellsData.Find(InCell->GetFName());
	if (!CellData)
	{
		return true;
	}

	// If cell wasn't even visible yet or doesn't have HLOD actors, skip warmup.
	bPerformWarmpup = !CellData->LoadedHLODs.IsEmpty() && CellData->bIsCellVisible;
	if (!bPerformWarmpup)
	{
		return true;
	}

	// Verify that at least one HLOD actor associated with this cell needs warmup.
	bPerformWarmpup = Algo::AnyOf(CellData->LoadedHLODs, [](const AWorldPartitionHLOD* HLODActor) { return HLODActor->DoesRequireWarmup(); });
	if (!bPerformWarmpup)
	{
		return true;
	}

	// In case a previous request to unload was aborted and the cell never unloaded... assume warmup requests are expired after a given amount of frames.
	const uint32 WarmupExpiredFrames = 30;

	uint32 CurrentFrameNumber = GetWorld()->Scene->GetFrameNumber();

	// Trigger warmup on the first request to unload, or if a warmup request expired
	if (CellData->WarmupEndFrame == INDEX_NONE || CurrentFrameNumber > (CellData->WarmupEndFrame + WarmupExpiredFrames))
	{
		// Warmup will be triggered in the next BeginRenderView() call, at which point the frame number will have been incremented.
		CellData->WarmupStartFrame = CurrentFrameNumber + 1; 
		CellData->WarmupEndFrame = CellData->WarmupStartFrame + CVarHLODWarmupNumFrames.GetValueOnGameThread();
		WorldPartitionHLODRuntimeData->CellsToWarmup.Add(CellData);
	}

	// Test if warmup is completed
	bool bCanUnload = CurrentFrameNumber >= CellData->WarmupEndFrame;
	if (bCanUnload)
	{
		CellData->WarmupStartFrame = INDEX_NONE;
		CellData->WarmupEndFrame = INDEX_NONE;
	}

	return bCanUnload;
}

void UHLODSubsystem::OnBeginRenderViews(const FSceneViewFamily& InViewFamily)
{
	for (auto& KeyValuePair : WorldPartitionsHLODRuntimeData)
	{
		for (TSet<FCellData*>::TIterator CellIt(KeyValuePair.Value.CellsToWarmup); CellIt; ++CellIt)
		{
			FCellData* CellPendingUnload = *CellIt;

			MakeRenderResourcesResident(*CellPendingUnload, InViewFamily);

			// Stop processing this cell if warmup is done.
			if (InViewFamily.FrameNumber >= CellPendingUnload->WarmupEndFrame)
			{
				CellIt.RemoveCurrent();
			}
		}
	}
}

void FHLODResourcesResidencySceneViewExtension::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
	GetWorld()->GetSubsystem<UHLODSubsystem>()->OnBeginRenderViews(InViewFamily);
}

#if WITH_EDITOR

static void DumpHLODStats(UWorld* World)
{
	UWorldPartition* WorldPartition = World ? World->GetWorldPartition() : nullptr;
	if (!WorldPartition)
	{
		return;
	}
	
	typedef TFunction<FString(const FHLODActorDesc&)> FGetStatFunc;

	auto GetHLODStat = [](FName InStatName)
	{
		return TPair<FName, FGetStatFunc>(InStatName, [InStatName](const FHLODActorDesc& InActorDesc)
		{
			return FString::Printf(TEXT("%lld"), InActorDesc.GetStat(InStatName));
		});
	};

	const UDataLayerSubsystem* DataLayerSubsystem = UWorld::GetSubsystem<UDataLayerSubsystem>(World);

	auto GetDataLayerShortName = [DataLayerSubsystem](FName DataLayerInstanceName)
	{ 
		UDataLayerInstance* DataLayerInstance = DataLayerSubsystem ? DataLayerSubsystem->GetDataLayerInstance(DataLayerInstanceName) : nullptr;
		return DataLayerInstance ? DataLayerInstance->GetDataLayerShortName() : DataLayerInstanceName.ToString();
	};

	TArray<TPair<FName, FGetStatFunc>> StatsToWrite =
	{
		{ "Name",				[](const FHLODActorDesc& InActorDesc) { return InActorDesc.GetActorLabel().ToString(); } },
		{ "HLODLayer",			[](const FHLODActorDesc& InActorDesc) { return InActorDesc.GetSourceHLODLayerName().ToString(); } },
		{ "SpatiallyLoaded",	[](const FHLODActorDesc& InActorDesc) { return InActorDesc.GetIsSpatiallyLoaded() ? TEXT("true") : TEXT("false"); } },
		{ "DataLayers",			[&GetDataLayerShortName](const FHLODActorDesc& InActorDesc) { return FString::JoinBy(InActorDesc.GetDataLayerInstanceNames(), TEXT(" | "), GetDataLayerShortName); } },

		GetHLODStat(FWorldPartitionHLODStats::InputActorCount),
		GetHLODStat(FWorldPartitionHLODStats::InputTriangleCount),
		GetHLODStat(FWorldPartitionHLODStats::InputVertexCount),

		GetHLODStat(FWorldPartitionHLODStats::MeshInstanceCount),
		GetHLODStat(FWorldPartitionHLODStats::MeshNaniteTriangleCount),
		GetHLODStat(FWorldPartitionHLODStats::MeshNaniteVertexCount),
		GetHLODStat(FWorldPartitionHLODStats::MeshTriangleCount),
		GetHLODStat(FWorldPartitionHLODStats::MeshVertexCount),
		GetHLODStat(FWorldPartitionHLODStats::MeshUVChannelCount),

		GetHLODStat(FWorldPartitionHLODStats::MaterialBaseColorTextureSize),
		GetHLODStat(FWorldPartitionHLODStats::MaterialNormalTextureSize),
		GetHLODStat(FWorldPartitionHLODStats::MaterialEmissiveTextureSize),
		GetHLODStat(FWorldPartitionHLODStats::MaterialMetallicTextureSize),
		GetHLODStat(FWorldPartitionHLODStats::MaterialRoughnessTextureSize),
		GetHLODStat(FWorldPartitionHLODStats::MaterialSpecularTextureSize),
		
		GetHLODStat(FWorldPartitionHLODStats::MemoryMeshResourceSizeBytes),
		GetHLODStat(FWorldPartitionHLODStats::MemoryTexturesResourceSizeBytes),
		GetHLODStat(FWorldPartitionHLODStats::MemoryDiskSizeBytes),
		
		GetHLODStat(FWorldPartitionHLODStats::BuildTimeLoadMilliseconds),
		GetHLODStat(FWorldPartitionHLODStats::BuildTimeBuildMilliseconds),
		GetHLODStat(FWorldPartitionHLODStats::BuildTimeTotalMilliseconds)				
	};

	// Write header
	FStringOutputDevice Output;
	const FString StatHeader = FString::JoinBy(StatsToWrite, TEXT(","), [](const TPair<FName, FGetStatFunc>& Pair) { return Pair.Key.ToString(); });
	Output.Logf(TEXT("%s" LINE_TERMINATOR_ANSI), *StatHeader);

	// Write one line per HLOD actor desc
	for (FActorDescList::TIterator<AWorldPartitionHLOD> HLODIterator(WorldPartition->GetActorDescContainer()); HLODIterator; ++HLODIterator)
	{
		const FString StatLine = FString::JoinBy(StatsToWrite, TEXT(","), [&HLODIterator](const TPair<FName, FGetStatFunc>& Pair) { return Pair.Value(**HLODIterator); });
		Output.Logf(TEXT("%s" LINE_TERMINATOR_ANSI), *StatLine);
	}

	// Write to file
	const FString HLODStatsOutputFilename = FPaths::ProjectSavedDir() / TEXT("WorldPartition") / FString::Printf(TEXT("HLODStats-%s-%08x-%s.csv"), *World->GetName(), FPlatformProcess::GetCurrentProcessId(), *FDateTime::Now().ToString());
	FFileHelper::SaveStringToFile(Output, *HLODStatsOutputFilename);
}

FAutoConsoleCommand HLODDumpStats(
	TEXT("wp.Editor.HLOD.DumpStats"),
	TEXT("Write various HLOD stats to a CSV formatted file."),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (UWorld* World = Context.World())
			{
				DumpHLODStats(World);
			}
		}
	})
);

#endif // #if WITH_EDITOR

#undef LOCTEXT_NAMESPACE

