// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/HLOD/HLODRuntimeSubsystem.h"
#include "Engine/Level.h"
#include "WorldPartition/HLOD/HLODActor.h"
#include "Engine/LevelStreaming.h"
#include "Engine/World.h"
#include "WorldPartition/HLOD/HLODActorDesc.h"
#include "Materials/MaterialInterface.h"
#include "WorldPartition/DataLayer/DataLayerManager.h"
#include "Misc/Paths.h"
#include "WorldPartition/WorldPartition.h"
#include "RenderUtils.h"
#include "WorldPartition/WorldPartitionSubsystem.h"
#include "SceneInterface.h"
#include "SceneView.h"
#include "WorldPartition/WorldPartitionRuntimeHash.h"
#include "SceneManagement.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "EngineModule.h"
#include "LevelUtils.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Misc/FileHelper.h"
#include "SceneViewExtension.h"
#include "StaticMeshResources.h"
#include "Subsystems/Subsystem.h"
#include "UObject/UObjectIterator.h"
#include "WorldPartition/DataLayer/WorldDataLayers.h"
#include "WorldPartition/HLOD/HLODStats.h"
#include "WorldPartition/WorldPartitionActorDescInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HLODRuntimeSubsystem)

#define LOCTEXT_NAMESPACE "HLODRuntimeSubsystem"

DEFINE_LOG_CATEGORY_STATIC(LogHLODRuntimeSubsystem, Log, All);

static TAutoConsoleVariable<int32> CVarHLODWarmupEnabled(
	TEXT("wp.Runtime.HLOD.WarmupEnabled"),
	1,
	TEXT("Enable HLOD assets warmup. Will delay unloading of cells & transition to HLODs for wp.Runtime.HLOD.WarmupNumFrames frames."));

static TAutoConsoleVariable<int32> CVarHLODWarmupVT(
	TEXT("wp.Runtime.HLOD.WarmupVT"),
	1,
	TEXT("Enable virtual texture warmup for HLOD assets. Requires wp.Runtime.HLOD.WarmupEnabled to be 1."));

static TAutoConsoleVariable<int32> CVarHLODWarmupNanite(
	TEXT("wp.Runtime.HLOD.WarmupNanite"),
	1,
	TEXT("Enable Nanite warmup for HLOD assets. Requires wp.Runtime.HLOD.WarmupEnabled to be 1."));

static TAutoConsoleVariable<int32> CVarHLODWarmupNumFrames(
	TEXT("wp.Runtime.HLOD.WarmupNumFrames"),
	5,
	TEXT("Delay unloading of a cell for this amount of frames to ensure HLOD assets are ready to be shown at the proper resolution. Set to 0 to force disable warmup."));

static TAutoConsoleVariable<int32> CVarHLODWarmupDebugDraw(
	TEXT("wp.Runtime.HLOD.WarmupDebugDraw"),
	0,
	TEXT("Draw debug display for the warmup requests"));

static TAutoConsoleVariable<float> CVarHLODWarmupVTScaleFactor(
	TEXT("wp.Runtime.HLOD.WarmupVTScaleFactor"),
	2.0f,
	TEXT("Scale the VT size we ask to prefetch by this factor."));

static TAutoConsoleVariable<int32> CVarHLODWarmupVTSizeClamp(
	TEXT("wp.Runtime.HLOD.WarmupVTSizeClamp"),
	2048,
	TEXT("Clamp VT warmup requests for safety."));

static void HLODRuntimeSubsystemCVarSinkFunction()
{
	for (UWorld* World : TObjectRange<UWorld>(RF_ClassDefaultObject | RF_ArchetypeObject, true, EInternalObjectFlags::Garbage))
	{
		if (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE)
		{
			if (UWorldPartitionHLODRuntimeSubsystem* HLODRuntimeSubsystem = World->GetSubsystem<UWorldPartitionHLODRuntimeSubsystem>())
			{
				HLODRuntimeSubsystem->OnCVarsChanged();
			}
		}
	}
}

static FAutoConsoleVariableSink CVarHLODSink(FConsoleCommandDelegate::CreateStatic(&HLODRuntimeSubsystemCVarSinkFunction));

namespace FHLODRuntimeSubsystem
{
    static const UWorldPartitionRuntimeCell* GetActorRuntimeCell(AActor* InActor)
    {
	    const ULevel* ActorLevel = InActor->GetLevel();
	    const ULevelStreaming* LevelStreaming = FLevelUtils::FindStreamingLevel(ActorLevel);
	    return LevelStreaming ? Cast<const UWorldPartitionRuntimeCell>(LevelStreaming->GetWorldPartitionCell()) : nullptr;
    }
    
    static UWorldPartition* GetWorldPartition(AWorldPartitionHLOD* InWorldPartitionHLOD)
    {
	    // Alwaysloaded Cell level will have a WorldPartition
	    if (UWorldPartition* WorldPartition = InWorldPartitionHLOD->GetLevel()->GetWorldPartition())
	    {
		    return WorldPartition;
	    } // If not find it through the cell
	    else if (const UWorldPartitionRuntimeCell* RuntimeCell = GetActorRuntimeCell(InWorldPartitionHLOD))
	    {
		    return RuntimeCell->GetOuterWorld()->GetWorldPartition();
	    }
    
	    return nullptr;
    }
}


class FHLODResourcesResidencySceneViewExtension : public FWorldSceneViewExtension
{
public:
	FHLODResourcesResidencySceneViewExtension(const FAutoRegister& AutoRegister, UWorld* InWorld)
		: FWorldSceneViewExtension(AutoRegister, InWorld)
	{
	}

	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override {}
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {}
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override
	{
		GetWorld()->GetSubsystem<UWorldPartitionHLODRuntimeSubsystem>()->OnBeginRenderViews(InViewFamily);
	}
};


UWorldPartitionHLODRuntimeSubsystem::UWorldPartitionHLODRuntimeSubsystem()
	: UWorldSubsystem()
	, bCachedShouldPerformWarmup(true)
{
}

bool UWorldPartitionHLODRuntimeSubsystem::WorldPartitionHLODEnabled = true;

FAutoConsoleCommand UWorldPartitionHLODRuntimeSubsystem::EnableHLODCommand(
	TEXT("wp.Runtime.HLOD"),
	TEXT("Turn on/off loading & rendering of world partition HLODs."),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		UWorldPartitionHLODRuntimeSubsystem::WorldPartitionHLODEnabled = (Args.Num() != 1) || (Args[0] != TEXT("0"));
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (World && World->IsGameWorld())
			{
				if (UWorldPartitionHLODRuntimeSubsystem* HLODSubSystem = World->GetSubsystem<UWorldPartitionHLODRuntimeSubsystem>()) 
				{
					for (const auto& KeyValuePair : HLODSubSystem->WorldPartitionsHLODRuntimeData)
					{
						for (const auto& CellHLODMapping : KeyValuePair.Value.CellsData)
						{
							const FCellData& CellData = CellHLODMapping.Value;
							bool bIsHLODVisible = UWorldPartitionHLODRuntimeSubsystem::WorldPartitionHLODEnabled && !CellData.bIsCellVisible;
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

bool UWorldPartitionHLODRuntimeSubsystem::IsHLODEnabled()
{
	return UWorldPartitionHLODRuntimeSubsystem::WorldPartitionHLODEnabled;
}

bool UWorldPartitionHLODRuntimeSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UWorldPartitionHLODRuntimeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	// Ensure the WorldPartitionSubsystem gets created before the HLODRuntimeSubsystem
	Collection.InitializeDependency<UWorldPartitionSubsystem>();

	Super::Initialize(Collection);

	UWorld* World = GetWorld();
	check(World->IsGameWorld());

	GetWorld()->OnWorldPartitionInitialized().AddUObject(this, &UWorldPartitionHLODRuntimeSubsystem::OnWorldPartitionInitialized);
	GetWorld()->OnWorldPartitionUninitialized().AddUObject(this, &UWorldPartitionHLODRuntimeSubsystem::OnWorldPartitionUninitialized);

	bCachedShouldPerformWarmup = ShouldPerformWarmup();

	SceneViewExtension = FSceneViewExtensions::NewExtension<FHLODResourcesResidencySceneViewExtension>(World);
}

void UWorldPartitionHLODRuntimeSubsystem::Deinitialize()
{
	Super::Deinitialize();

	GetWorld()->OnWorldPartitionInitialized().RemoveAll(this);
	GetWorld()->OnWorldPartitionUninitialized().RemoveAll(this);
}

void UWorldPartitionHLODRuntimeSubsystem::OnWorldPartitionInitialized(UWorldPartition* InWorldPartition)
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
				UE_LOG(LogHLODRuntimeSubsystem, Verbose, TEXT("Registering cell %s - %s"), *Cell->GetGuid().ToString(), *Cell->GetDebugName());

				WorldPartitionHLODRuntimeData.CellsData.Emplace(Cell->GetGuid());
				return true;
			});
		}
	}
}

void UWorldPartitionHLODRuntimeSubsystem::OnWorldPartitionUninitialized(UWorldPartition* InWorldPartition)
{
	if (InWorldPartition && InWorldPartition->IsStreamingEnabled())
	{
		check(WorldPartitionsHLODRuntimeData.Contains(InWorldPartition));

#if !NO_LOGGING
		FWorldPartitionHLODRuntimeData& WorldPartitionHLODRuntimeData = WorldPartitionsHLODRuntimeData.FindChecked(InWorldPartition);
		InWorldPartition->RuntimeHash->ForEachStreamingCells([&WorldPartitionHLODRuntimeData](const UWorldPartitionRuntimeCell* Cell)
		{
			UE_LOG(LogHLODRuntimeSubsystem, Verbose, TEXT("Unregistering cell %s - %s"), *Cell->GetGuid().ToString(), *Cell->GetDebugName());
			return true;
		});
#endif

		WorldPartitionsHLODRuntimeData.Remove(InWorldPartition);
	}
}

void UWorldPartitionHLODRuntimeSubsystem::OnExternalStreamingObjectInjected(URuntimeHashExternalStreamingObjectBase* ExternalStreamingObject)
{
	UWorldPartition* OwnerPartition = ExternalStreamingObject->GetOuterWorld()->GetWorldPartition();
	FWorldPartitionHLODRuntimeData* WorldPartitionHLODRuntimeData = WorldPartitionsHLODRuntimeData.Find(OwnerPartition);
	if (WorldPartitionHLODRuntimeData)
	{
		ExternalStreamingObject->ForEachStreamingCells([WorldPartitionHLODRuntimeData](const UWorldPartitionRuntimeCell& Cell)
		{
			UE_LOG(LogHLODRuntimeSubsystem, Verbose, TEXT("Registering external cell %s - %s"), *Cell.GetGuid().ToString(), *Cell.GetDebugName());
			WorldPartitionHLODRuntimeData->CellsData.Emplace(Cell.GetGuid());
			return true;
		});
	}
}

void UWorldPartitionHLODRuntimeSubsystem::OnExternalStreamingObjectRemoved(URuntimeHashExternalStreamingObjectBase* ExternalStreamingObject)
{
	UWorldPartition* OwnerPartition = ExternalStreamingObject->GetOuterWorld()->GetWorldPartition();
	FWorldPartitionHLODRuntimeData* WorldPartitionHLODRuntimeData = WorldPartitionsHLODRuntimeData.Find(OwnerPartition);
	if (WorldPartitionHLODRuntimeData)
	{
		ExternalStreamingObject->ForEachStreamingCells([WorldPartitionHLODRuntimeData](const UWorldPartitionRuntimeCell& Cell)
		{
			UE_LOG(LogHLODRuntimeSubsystem, Verbose, TEXT("Unregistering external cell %s - %s"), *Cell.GetGuid().ToString(), *Cell.GetDebugName());
			WorldPartitionHLODRuntimeData->CellsData.Remove(Cell.GetGuid());
			return true;
		});
	}
}

const UWorldPartitionHLODRuntimeSubsystem::FCellData* UWorldPartitionHLODRuntimeSubsystem::GetCellData(const UWorldPartitionRuntimeCell* InCell) const
{
	return const_cast<UWorldPartitionHLODRuntimeSubsystem*>(this)->GetCellData(InCell);
}

UWorldPartitionHLODRuntimeSubsystem::FCellData* UWorldPartitionHLODRuntimeSubsystem::GetCellData(const UWorldPartitionRuntimeCell* InCell)
{
	const UWorldPartition* WorldPartition = InCell->GetOuterWorld()->GetWorldPartition();
	if (WorldPartition)
	{
		FWorldPartitionHLODRuntimeData* WorldPartitionHLODRuntimeData = WorldPartitionsHLODRuntimeData.Find(WorldPartition);
		if (WorldPartitionHLODRuntimeData)
		{
			check(WorldPartition->IsStreamingEnabled());
			return WorldPartitionHLODRuntimeData->CellsData.Find(InCell->GetGuid());
		}
	}

	return nullptr;
}

UWorldPartitionHLODRuntimeSubsystem::FCellData* UWorldPartitionHLODRuntimeSubsystem::GetCellData(AWorldPartitionHLOD* InWorldPartitionHLOD)
{
	const UWorldPartition* WorldPartition = FHLODRuntimeSubsystem::GetWorldPartition(InWorldPartitionHLOD);
	if (WorldPartition)
	{
		FWorldPartitionHLODRuntimeData* WorldPartitionHLODRuntimeData = WorldPartitionsHLODRuntimeData.Find(WorldPartition);
		if (WorldPartitionHLODRuntimeData)
		{
			check(WorldPartition->IsStreamingEnabled());
			const FGuid CellGuid = InWorldPartitionHLOD->GetSourceCellGuid();
			return WorldPartitionHLODRuntimeData->CellsData.Find(CellGuid);
		}
	}

	return nullptr;		
}

const TArray<AWorldPartitionHLOD*>& UWorldPartitionHLODRuntimeSubsystem::GetHLODActorsForCell(const UWorldPartitionRuntimeCell* InCell) const
{
	if (const FCellData* CellData = GetCellData(InCell))
	{
		return CellData->LoadedHLODs;
	}

	// No HLOD found for the given cell, return a dummy array
	static const TArray<AWorldPartitionHLOD*> DummyArray;
	return DummyArray;
}

void UWorldPartitionHLODRuntimeSubsystem::RegisterHLODActor(AWorldPartitionHLOD* InWorldPartitionHLOD)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWorldPartitionHLODRuntimeSubsystem::RegisterHLODActor);
	
	if (FCellData* CellData = GetCellData(InWorldPartitionHLOD))
	{
		UE_LOG(LogHLODRuntimeSubsystem, Verbose, TEXT("Registering HLOD %s for cell %s"), *InWorldPartitionHLOD->GetActorNameOrLabel(), *InWorldPartitionHLOD->GetSourceCellGuid().ToString());

		CellData->LoadedHLODs.Add(InWorldPartitionHLOD);
		InWorldPartitionHLOD->SetVisibility(UWorldPartitionHLODRuntimeSubsystem::WorldPartitionHLODEnabled && !CellData->bIsCellVisible);
	}
	else
	{
		UE_LOG(LogHLODRuntimeSubsystem, Verbose, TEXT("Found HLOD %s referencing nonexistent cell '%s'"), *InWorldPartitionHLOD->GetActorNameOrLabel(), *InWorldPartitionHLOD->GetSourceCellGuid().ToString());
		InWorldPartitionHLOD->SetVisibility(false);

#if WITH_EDITOR
		OutdatedHLODActors.Add(InWorldPartitionHLOD);
#endif
	}

	HLODActorsToWarmup.Remove(InWorldPartitionHLOD);
	HLODActorRegisteredEvent.Broadcast(InWorldPartitionHLOD);
}

void UWorldPartitionHLODRuntimeSubsystem::UnregisterHLODActor(AWorldPartitionHLOD* InWorldPartitionHLOD)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWorldPartitionHLODRuntimeSubsystem::UnregisterHLODActor);

	if (FCellData* CellData = GetCellData(InWorldPartitionHLOD))
	{
		UE_LOG(LogHLODRuntimeSubsystem, Verbose, TEXT("Unregistering HLOD %s for cell %s"), *InWorldPartitionHLOD->GetActorNameOrLabel(), *InWorldPartitionHLOD->GetSourceCellGuid().ToString());

		verify(CellData->LoadedHLODs.Remove(InWorldPartitionHLOD));
	}
	else
	{
#if WITH_EDITOR
		OutdatedHLODActors.Remove(InWorldPartitionHLOD);
#endif
	}

	HLODActorUnregisteredEvent.Broadcast(InWorldPartitionHLOD);
}

void UWorldPartitionHLODRuntimeSubsystem::OnCellShown(const UWorldPartitionRuntimeCell* InCell)
{
	if (FCellData* CellData = GetCellData(InCell))
	{
		CellData->bIsCellVisible = true;

		if (!CellData->LoadedHLODs.IsEmpty())
		{
			UE_LOG(LogHLODRuntimeSubsystem, Verbose, TEXT("Cell shown - %s - hiding %d HLOD actors"), *InCell->GetGuid().ToString(), CellData->LoadedHLODs.Num());

			for (AWorldPartitionHLOD* HLODActor : CellData->LoadedHLODs)
			{
				UE_LOG(LogHLODRuntimeSubsystem, Verbose, TEXT("\t\t* %s"), *HLODActor->GetActorNameOrLabel());
				HLODActor->SetVisibility(false);
			}
		}
	}
}

void UWorldPartitionHLODRuntimeSubsystem::OnCellHidden(const UWorldPartitionRuntimeCell* InCell)
{
	if (FCellData* CellData = GetCellData(InCell))
	{
		CellData->bIsCellVisible = false;

		if (!CellData->LoadedHLODs.IsEmpty())
		{
			UE_LOG(LogHLODRuntimeSubsystem, Verbose, TEXT("Cell hidden - %s - showing %d HLOD actors"), *InCell->GetGuid().ToString(), CellData->LoadedHLODs.Num());

			for (AWorldPartitionHLOD* HLODActor : CellData->LoadedHLODs)
			{
				UE_LOG(LogHLODRuntimeSubsystem, Verbose, TEXT("\t\t* %s"), *HLODActor->GetActorNameOrLabel());
				HLODActor->SetVisibility(UWorldPartitionHLODRuntimeSubsystem::WorldPartitionHLODEnabled);
				HLODActorsToWarmup.Remove(HLODActor);
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

static void PrepareNaniteRequests(TMap<Nanite::FResources*, int32>& InOutNaniteRequests, UStaticMeshComponent* InStaticMeshComponent, int32 InNumFramesUntilRender)
{
	UStaticMesh* StaticMesh = InStaticMeshComponent->GetStaticMesh();
	if (StaticMesh && StaticMesh->HasValidNaniteData())
	{
		int32& NumFramesUntilRender = InOutNaniteRequests.FindOrAdd(StaticMesh->GetRenderData()->NaniteResourcesPtr.Get());
		NumFramesUntilRender = FMath::Max(InNumFramesUntilRender, 1);
	}
}

bool UWorldPartitionHLODRuntimeSubsystem::PrepareToWarmup(const UWorldPartitionRuntimeCell* InCell, AWorldPartitionHLOD* InHLODActor)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWorldPartitionHLODRuntimeSubsystem::PrepareToWarmup)

	bool bHLODActorNeedsWarmUp = false;

	if (InHLODActor->DoesRequireWarmup())
	{
		FWorldPartitionHLODWarmupState& WarmupState = HLODActorsToWarmup.FindOrAdd(InHLODActor);

		// Trigger warmup for CVarHLODWarmupNumFrames frames on the first request, or if a warmup wasn't requested in the last frame
		const bool bResetWarmup =  WarmupState.WarmupLastRequestedFrame == INDEX_NONE ||
								  (WarmupState.WarmupLastRequestedFrame + 1) < GFrameNumber;
				
		if (bResetWarmup)
		{
			WarmupState.WarmupCallsUntilReady = CVarHLODWarmupNumFrames.GetValueOnGameThread();
			WarmupState.WarmupBounds = InCell->GetContentBounds();

			// If we're dealing with an instanced world partition, take the instance transform into account
			const UWorldPartition* WorldPartition = InCell->GetOuterWorld()->GetWorldPartition();
			if (ensure(WorldPartition) && !WorldPartition->IsMainWorldPartition())
			{
				WarmupState.WarmupBounds = WarmupState.WarmupBounds.TransformBy(WorldPartition->GetInstanceTransform());
			}			
		}
		else if (WarmupState.WarmupCallsUntilReady != 0)
		{	
			// Progress toward warmup readiness
			WarmupState.WarmupCallsUntilReady--;
		}
		
		bHLODActorNeedsWarmUp = WarmupState.WarmupCallsUntilReady != 0;
		WarmupState.WarmupLastRequestedFrame = GFrameNumber;
	}

	return bHLODActorNeedsWarmUp;
}

void UWorldPartitionHLODRuntimeSubsystem::OnCVarsChanged()
{
	bCachedShouldPerformWarmup = ShouldPerformWarmup();
}

bool UWorldPartitionHLODRuntimeSubsystem::ShouldPerformWarmup() const
{
	// Test if warmup is disabled globally.
	const bool bWarmupEnabled = CVarHLODWarmupEnabled.GetValueOnGameThread() != 0;
	if (!bWarmupEnabled)
	{
		return false;
	}

	// If warmup num of frames is invalid, no warmup needed
	if (CVarHLODWarmupNumFrames.GetValueOnGameThread() <= 0)
	{
		return false;
	}

	// If warmup num of frames is invalid, no warmup needed
	const EShaderPlatform ShaderPlatform = GShaderPlatformForFeatureLevel[GMaxRHIFeatureLevel];
	const bool bNaniteEnabled = UseNanite(ShaderPlatform);
	const bool bVirtualTextureEnabled = UseVirtualTexturing(ShaderPlatform);
	const bool bWarmupNanite = CVarHLODWarmupNanite.GetValueOnGameThread() != 0;
	const bool bWarmupVT = CVarHLODWarmupVT.GetValueOnGameThread() != 0;
	const bool bWarmupNeeded = (bNaniteEnabled && bWarmupNanite) || (bVirtualTextureEnabled && bWarmupVT);
	if (!bWarmupNeeded)
	{
		return false;
	}

	// If we're running a dedicated server, no warmup needed
	const bool bIsDedicatedServer = GetWorld()->GetNetMode() == NM_DedicatedServer;
	if (bIsDedicatedServer)
	{
		return false;
	}

	return true;
}

bool UWorldPartitionHLODRuntimeSubsystem::ShouldPerformWarmupForCell(const UWorldPartitionRuntimeCell* InCell) const
{
	if (!bCachedShouldPerformWarmup)
	{
		return false;
	}

	const UWorld* World = GetWorld();

	// Blocking loading shouldn't trigger warmup
	const bool bIsInBlockingLoading = World->GetIsInBlockTillLevelStreamingCompleted();
	if (bIsInBlockingLoading)
	{
		return false;
	}

	// If processing for this cell in Add/RemoveFromWorld() has already started, it's too late for warmup
	const ULevel* CellLevel = InCell->GetLevel();
	const bool bCurrentlyProcessingLevel = CellLevel == World->GetCurrentLevelPendingVisibility() ||
										   CellLevel == World->GetCurrentLevelPendingInvisibility();
	if (bCurrentlyProcessingLevel)
	{
		return false;
	}

	return true;
}

bool UWorldPartitionHLODRuntimeSubsystem::CanMakeVisible(const UWorldPartitionRuntimeCell* InCell)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWorldPartitionHLODRuntimeSubsystem::CanMakeVisible)

	if (!ShouldPerformWarmupForCell(InCell))
	{
		return true;
	}

	bool bCanMakeVisible = true;

	// Prevent cells containing HLODs actors from being made visible until warmup has been performed
	if (InCell->GetIsHLOD() && InCell->GetLevel())
	{
		for (AActor* Actor : InCell->GetLevel()->Actors)
		{
			if (AWorldPartitionHLOD* HLODActor = Cast<AWorldPartitionHLOD>(Actor))
			{
				const bool bHLODActorNeedsWarmup = PrepareToWarmup(InCell, HLODActor);
				bCanMakeVisible &= !bHLODActorNeedsWarmup;
			}
		}
	}

	return bCanMakeVisible;
}

bool UWorldPartitionHLODRuntimeSubsystem::CanMakeInvisible(const UWorldPartitionRuntimeCell* InCell)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWorldPartitionHLODRuntimeSubsystem::CanMakeInvisible)

	if (!ShouldPerformWarmupForCell(InCell))
	{
		return true;
	}

	bool bCanMakeInvisible = true;

	// Prevent cells which have an HLOD representation from being hidden until their matching HLOD actors have been warmed up
	if (FCellData* CellData = GetCellData(InCell))
	{
		for (AWorldPartitionHLOD* HLODActor : CellData->LoadedHLODs)
		{
			const bool bHLODActorNeedsWarmup = PrepareToWarmup(InCell, HLODActor);
			bCanMakeInvisible &= !bHLODActorNeedsWarmup;
		}
	}

	return bCanMakeInvisible;
}

static float IsInView(const FVector& BoundsOrigin, const FVector& BoundsExtent, const FSceneViewFamily& InViewFamily, bool bComputeScreenSize, float& MaxScreenSizePixels)
{
	MaxScreenSizePixels = 0;

	// Estimate the highest screen pixel size of this component in the provided views
	for (const FSceneView* View : InViewFamily.Views)
	{
		// Make sure the HLOD actor we're about to show is actually in the frustum
		if (View->ViewFrustum.IntersectBox(BoundsOrigin, BoundsExtent))
		{
			if (bComputeScreenSize)
			{
				float ScreenDiameter = ComputeBoundsScreenSize(BoundsOrigin, BoundsExtent.Size(), *View);
				float ScreenSizePixels = ScreenDiameter * View->ViewMatrices.GetScreenScale() * 2.0f;

				MaxScreenSizePixels = FMath::Max(MaxScreenSizePixels, ScreenSizePixels);
			}
			else
			{
				return true;
			}
		}
	}

	return MaxScreenSizePixels > 0;
}

static void MakeHLODRenderResourcesResident(TMap<UMaterialInterface*, float>& VTRequests, TMap<Nanite::FResources*, int32>& NaniteRequests, const FSceneViewFamily& InViewFamily)
{
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

			for (const TPair<Nanite::FResources*, int32> NaniteRequest : NaniteRequests)
			{
				const Nanite::FResources* NaniteResource = NaniteRequest.Key;
				const int32 NumFramesUntilRender = NaniteRequest.Value;

				GetRendererModule().PrefetchNaniteResource(NaniteResource, NumFramesUntilRender);
			}
		});
	}
}

void UWorldPartitionHLODRuntimeSubsystem::OnBeginRenderViews(const FSceneViewFamily& InViewFamily)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWorldPartitionHLODRuntimeSubsystem::OnBeginRenderViews)

	TMap<UMaterialInterface*, float> VTRequests;
	TMap<Nanite::FResources*, int32> NaniteRequests;

	const bool bWarmupNanite = CVarHLODWarmupNanite.GetValueOnGameThread() != 0;
	const bool bWarmupVT = CVarHLODWarmupVT.GetValueOnGameThread() != 0;

	for (FHLODWarmupStateMap::TIterator HLODActorWarmupStateIt(HLODActorsToWarmup); HLODActorWarmupStateIt; ++HLODActorWarmupStateIt)
	{
		const FObjectKey& HLODActorObjectKey = HLODActorWarmupStateIt.Key();
		FWorldPartitionHLODWarmupState& HLODWarmupState = HLODActorWarmupStateIt.Value();
		const AWorldPartitionHLOD* HLODActor = Cast<AWorldPartitionHLOD>(HLODActorObjectKey.ResolveObjectPtr());
		
		if (HLODActor)
		{
			// Retrieve this component's bound - we must support getting the bounds before the component is even registered.
			FVector BoundsOrigin;
			FVector BoundsExtent;
			HLODWarmupState.WarmupBounds.GetCenterAndExtents(BoundsOrigin, BoundsExtent);

			float ScreenSizePixels = 0;
			if (IsInView(BoundsOrigin, BoundsExtent, InViewFamily, bWarmupVT, ScreenSizePixels))
			{
				HLODActor->ForEachComponent<UStaticMeshComponent>(false, [&](UStaticMeshComponent* SMC)
				{
					// Assume ISM HLOD don't need warmup, as they are actually found in the source level
					if (SMC->IsA<UInstancedStaticMeshComponent>())
					{
						return;
					}
				
					if (bWarmupVT)
					{
						PrepareVTRequests(VTRequests, SMC, ScreenSizePixels);
					}

					if (bWarmupNanite)
					{
						if (HLODWarmupState.WarmupCallsUntilReady == CVarHLODWarmupNumFrames.GetValueOnGameThread())
						{
							// Send a nanite request to prepare for visibility in CVarHLODWarmupNumFrames frames
							PrepareNaniteRequests(NaniteRequests, SMC, CVarHLODWarmupNumFrames.GetValueOnGameThread());
						}
						else if (HLODWarmupState.WarmupCallsUntilReady == 0)
						{
							// We expect HLOD to be visible at any moment (likely waiting for server visibility ack)
							PrepareNaniteRequests(NaniteRequests, SMC, 1);
						}
					}
				});

#if ENABLE_DRAW_DEBUG
				if (CVarHLODWarmupDebugDraw.GetValueOnAnyThread())
				{
					DrawDebugBox(HLODActor->GetWorld(), BoundsOrigin, BoundsExtent, FColor::Yellow, /*bPersistentLine*/ false, /*Lifetime*/ 1.0f);
				}
#endif
			}
		}

		if (!HLODActor)
		{
			// HLOD actor has been unloaded, stop tracking it's warmup state
			HLODActorWarmupStateIt.RemoveCurrent();
		}
	}

	MakeHLODRenderResourcesResident(VTRequests, NaniteRequests, InViewFamily);
}

#if WITH_EDITOR

bool UWorldPartitionHLODRuntimeSubsystem::WriteHLODStatsCSV(UWorld* InWorld, const FString& InFilename)
{
	UWorldPartition* WorldPartition = InWorld ? InWorld->GetWorldPartition() : nullptr;
	if (!WorldPartition)
	{
		return false;
	}
	
	typedef TFunction<FString(FWorldPartitionActorDescInstance* InActorDescInstance, const FHLODActorDesc&)> FGetStatFunc;

	auto GetHLODStat = [](FName InStatName)
	{
		return TPair<FName, FGetStatFunc>(InStatName, [InStatName](FWorldPartitionActorDescInstance* InActorDescInstance, const FHLODActorDesc& InActorDesc)
		{
			return FString::Printf(TEXT("%lld"), InActorDesc.GetStat(InStatName));
		});
	};

	const UDataLayerManager* DataLayerManager = WorldPartition->GetDataLayerManager();

	auto GetDataLayerShortName = [DataLayerManager](FName DataLayerInstanceName)
	{ 
		const UDataLayerInstance* DataLayerInstance = DataLayerManager ? DataLayerManager->GetDataLayerInstance(DataLayerInstanceName) : nullptr;
		return DataLayerInstance ? DataLayerInstance->GetDataLayerShortName() : DataLayerInstanceName.ToString();
	};

	TArray<TPair<FName, FGetStatFunc>> StatsToWrite =
	{	
		{ "WorldPackage",		[InWorld](FWorldPartitionActorDescInstance* InActorDescInstance, const FHLODActorDesc& InActorDesc) { return InWorld->GetPackage()->GetName(); } },
		{ "Name",				[](FWorldPartitionActorDescInstance* InActorDescInstance, const FHLODActorDesc& InActorDesc) { return InActorDescInstance->GetActorLabel().ToString(); } },
		{ "HLODLayer",			[](FWorldPartitionActorDescInstance* InActorDescInstance, const FHLODActorDesc& InActorDesc) { return InActorDesc.GetSourceHLODLayer().GetAssetName().ToString(); }},
		{ "SpatiallyLoaded",	[](FWorldPartitionActorDescInstance* InActorDescInstance, const FHLODActorDesc& InActorDesc) { return InActorDescInstance->GetIsSpatiallyLoaded() ? TEXT("true") : TEXT("false"); } },
		{ "DataLayers",			[&GetDataLayerShortName](FWorldPartitionActorDescInstance* InActorDescInstance, const FHLODActorDesc& InActorDesc) { return FString::JoinBy(InActorDescInstance->GetDataLayerInstanceNames().ToArray(), TEXT(" | "), GetDataLayerShortName); }},

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

	FStringOutputDevice Output;

	// Write header if file doesn't exist
	if (!IFileManager::Get().FileExists(*InFilename))
	{
		const FString StatHeader = FString::JoinBy(StatsToWrite, TEXT(","), [](const TPair<FName, FGetStatFunc>& Pair) { return Pair.Key.ToString(); });
		Output.Logf(TEXT("%s" LINE_TERMINATOR_ANSI), *StatHeader);
	}

	// Write one line per HLOD actor desc
	for (FActorDescContainerInstanceCollection::TIterator<AWorldPartitionHLOD> HLODIterator(WorldPartition); HLODIterator; ++HLODIterator)
	{
		const FString StatLine = FString::JoinBy(StatsToWrite, TEXT(","), [&HLODIterator](const TPair<FName, FGetStatFunc>& Pair) 
		{ 
			const FHLODActorDesc& HLODActorDesc = *(FHLODActorDesc*)HLODIterator->GetActorDesc();
			return Pair.Value(*HLODIterator, HLODActorDesc); 
		});
		Output.Logf(TEXT("%s" LINE_TERMINATOR_ANSI), *StatLine);
	}

	// Write to file
	return FFileHelper::SaveStringToFile(Output, *InFilename, FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), EFileWrite::FILEWRITE_Append);
}

FAutoConsoleCommand HLODDumpStats(
	TEXT("wp.Editor.HLOD.DumpStats"),
	TEXT("Write various HLOD stats to a CSV formatted file."),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		const FString HLODStatsOutputFilename = FPaths::ProjectLogDir() / TEXT("WorldPartition") / FString::Printf(TEXT("HLODStats-%08x-%s.csv"), FPlatformProcess::GetCurrentProcessId(), *FDateTime::Now().ToString());

		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (UWorld* World = Context.World())
			{
				UWorldPartitionHLODRuntimeSubsystem::WriteHLODStatsCSV(World, HLODStatsOutputFilename);
			}
		}
	})
);

#endif // #if WITH_EDITOR

#undef LOCTEXT_NAMESPACE

