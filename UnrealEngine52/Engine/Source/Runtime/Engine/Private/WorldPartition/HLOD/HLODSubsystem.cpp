// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/HLOD/HLODSubsystem.h"
#include "Engine/Level.h"
#include "WorldPartition/HLOD/HLODActor.h"
#include "Engine/LevelStreaming.h"
#include "WorldPartition/HLOD/HLODActorDesc.h"
#include "Materials/MaterialInterface.h"
#include "WorldPartition/DataLayer/DataLayerSubsystem.h"
#include "Misc/Paths.h"
#include "WorldPartition/WorldPartition.h"
#include "RenderUtils.h"
#include "WorldPartition/WorldPartitionSubsystem.h"
#include "SceneInterface.h"
#include "SceneView.h"
#include "WorldPartition/WorldPartitionRuntimeHash.h"
#include "SceneManagement.h"
#include "WorldPartition/WorldPartitionLevelStreamingDynamic.h"
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

#include UE_INLINE_GENERATED_CPP_BY_NAME(HLODSubsystem)

#define LOCTEXT_NAMESPACE "HLODSubsystem"

DEFINE_LOG_CATEGORY_STATIC(LogHLODSubsystem, Log, All);

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

static void HLODSubsystemCVarSinkFunction()
{
	for (UWorld* World : TObjectRange<UWorld>(RF_ClassDefaultObject | RF_ArchetypeObject, true, EInternalObjectFlags::Garbage))
	{
		if (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE)
		{
			if (UHLODSubsystem* HLODSubsystem = World->GetSubsystem<UHLODSubsystem>())
			{
				HLODSubsystem->OnCVarsChanged();
			}
		}
	}
}

static FAutoConsoleVariableSink CVarHLODSink(FConsoleCommandDelegate::CreateStatic(&HLODSubsystemCVarSinkFunction));

namespace FHLODSubsystem
{
    static const UWorldPartitionRuntimeCell* GetActorRuntimeCell(AActor* InActor)
    {
	    const ULevel* ActorLevel = InActor->GetLevel();
	    const ULevelStreaming* LevelStreaming = FLevelUtils::FindStreamingLevel(ActorLevel);
	    const UWorldPartitionLevelStreamingDynamic* LevelStreamingDynamic = Cast<UWorldPartitionLevelStreamingDynamic>(LevelStreaming);
	    return LevelStreamingDynamic ? LevelStreamingDynamic->GetWorldPartitionRuntimeCell() : nullptr;
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
		    return RuntimeCell->GetCellOwner()->GetOuterWorld()->GetWorldPartition();
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
		GetWorld()->GetSubsystem<UHLODSubsystem>()->OnBeginRenderViews(InViewFamily);
	}
};


UHLODSubsystem::UHLODSubsystem()
	: UWorldSubsystem()
	, bCachedShouldPerformWarmup(true)
{
}

UHLODSubsystem::~UHLODSubsystem()
{
}

void UHLODSubsystem::SetHLODAlwaysLoadedCullDistance(int32 InCullDistance)
{
	const float MaxDrawDistance = FMath::Max(InCullDistance, 0);
	const bool bNeverDistanceCull = InCullDistance <= 0;

	for (AWorldPartitionHLOD* HLODActor : AlwaysLoadedHLODActors)
	{
		HLODActor->ForEachComponent<UPrimitiveComponent>(false, [MaxDrawDistance, bNeverDistanceCull](UPrimitiveComponent* HLODComponent)
		{
			HLODComponent->bNeverDistanceCull = bNeverDistanceCull;
			HLODComponent->SetCachedMaxDrawDistance(MaxDrawDistance);

			// Enable per instance culling to avoid ISM components dissapearing as a whole
			if (UInstancedStaticMeshComponent* ISMComponent = Cast<UInstancedStaticMeshComponent>(HLODComponent))
			{
				ISMComponent->SetCullDistances(0, MaxDrawDistance);
			}
		});
	}
}

void UHLODSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	check(InWorld.IsGameWorld());

	AlwaysLoadedHLODActors.Reset();

	// Cache always loaded HLODs
	for (AActor* Actor : InWorld.PersistentLevel->Actors)
	{
		if (AWorldPartitionHLOD* HLODActor = Cast<AWorldPartitionHLOD>(Actor))
		{
			AlwaysLoadedHLODActors.Add(HLODActor);
		}
	}
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

	bCachedShouldPerformWarmup = ShouldPerformWarmup();

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
				WorldPartitionHLODRuntimeData.CellsData.Emplace(Cell->GetGuid());
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

const UHLODSubsystem::FCellData* UHLODSubsystem::GetCellData(const UWorldPartitionRuntimeCell* InCell) const
{
	return const_cast<UHLODSubsystem*>(this)->GetCellData(InCell);
}

UHLODSubsystem::FCellData* UHLODSubsystem::GetCellData(const UWorldPartitionRuntimeCell* InCell)
{
	// @todo_ow ContentBundles do not support Hlods
	if (InCell->GetContentBundleID().IsValid())
	{
		return nullptr;
	}

	const UWorldPartition* WorldPartition = InCell->GetCellOwner()->GetOuterWorld()->GetWorldPartition();
	check(WorldPartition);
	FWorldPartitionHLODRuntimeData* WorldPartitionHLODRuntimeData = WorldPartitionsHLODRuntimeData.Find(WorldPartition);
	if (WorldPartitionHLODRuntimeData)
	{
		check(WorldPartition->IsStreamingEnabled());
		return WorldPartitionHLODRuntimeData->CellsData.Find(InCell->GetGuid());
	}

	return nullptr;
}

UHLODSubsystem::FCellData* UHLODSubsystem::GetCellData(AWorldPartitionHLOD* InWorldPartitionHLOD)
{
	const UWorldPartition* WorldPartition = FHLODSubsystem::GetWorldPartition(InWorldPartitionHLOD);
	check(WorldPartition);
	FWorldPartitionHLODRuntimeData* WorldPartitionHLODRuntimeData = WorldPartitionsHLODRuntimeData.Find(WorldPartition);
	if (WorldPartitionHLODRuntimeData)
	{
		check(WorldPartition->IsStreamingEnabled());
		const FGuid CellGuid = InWorldPartitionHLOD->GetSourceCellGuid();
		return WorldPartitionHLODRuntimeData->CellsData.Find(CellGuid);
	}

	return nullptr;		
}

const TArray<AWorldPartitionHLOD*>& UHLODSubsystem::GetHLODActorsForCell(const UWorldPartitionRuntimeCell* InCell) const
{
	if (const FCellData* CellData = GetCellData(InCell))
	{
		return CellData->LoadedHLODs;
	}

	// No HLOD found for the given cell, return a dummy array
	static const TArray<AWorldPartitionHLOD*> DummyArray;
	return DummyArray;
}

void UHLODSubsystem::RegisterHLODActor(AWorldPartitionHLOD* InWorldPartitionHLOD)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UHLODSubsystem::RegisterHLODActor);
	
	if (FCellData* CellData = GetCellData(InWorldPartitionHLOD))
	{
#if WITH_EDITOR
		UE_LOG(LogHLODSubsystem, Verbose, TEXT("Registering HLOD %s (%s) for cell %s"), *InWorldPartitionHLOD->GetActorLabel(), *InWorldPartitionHLOD->GetActorGuid().ToString(), *InWorldPartitionHLOD->GetSourceCellGuid().ToString());
#endif
		CellData->LoadedHLODs.Add(InWorldPartitionHLOD);
		InWorldPartitionHLOD->SetVisibility(UHLODSubsystem::WorldPartitionHLODEnabled && !CellData->bIsCellVisible);
	}
	else
	{
		UE_LOG(LogHLODSubsystem, Verbose, TEXT("Found HLOD referencing nonexistent cell '%s'"), *InWorldPartitionHLOD->GetSourceCellGuid().ToString());
		InWorldPartitionHLOD->SetVisibility(false);
	}

	HLODActorsToWarmup.Remove(InWorldPartitionHLOD);
	HLODActorRegisteredEvent.Broadcast(InWorldPartitionHLOD);
}

void UHLODSubsystem::UnregisterHLODActor(AWorldPartitionHLOD* InWorldPartitionHLOD)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UHLODSubsystem::UnregisterHLODActor);

	if (FCellData* CellData = GetCellData(InWorldPartitionHLOD))
	{
#if WITH_EDITOR
		UE_LOG(LogHLODSubsystem, Verbose, TEXT("Unregistering HLOD %s (%s) for cell %s"), *InWorldPartitionHLOD->GetActorLabel(), *InWorldPartitionHLOD->GetActorGuid().ToString(), *InWorldPartitionHLOD->GetSourceCellGuid().ToString());
#endif

		int32 NumRemoved = CellData->LoadedHLODs.Remove(InWorldPartitionHLOD);
		check(NumRemoved == 1);
	}

	HLODActorUnregisteredEvent.Broadcast(InWorldPartitionHLOD);
}

void UHLODSubsystem::OnCellShown(const UWorldPartitionRuntimeCell* InCell)
{
	if (FCellData* CellData = GetCellData(InCell))
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

void UHLODSubsystem::OnCellHidden(const UWorldPartitionRuntimeCell* InCell)
{
	if (FCellData* CellData = GetCellData(InCell))
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
			HLODActorsToWarmup.Remove(HLODActor);
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
		InOutNaniteRequests.Add(&StaticMesh->GetRenderData()->NaniteResources);
	}
}

bool UHLODSubsystem::PrepareToWarmup(const UWorldPartitionRuntimeCell* InCell, AWorldPartitionHLOD* InHLODActor)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UHLODSubsystem::PrepareToWarmup)

	bool bHLODActorNeedsWarmUp = false;

	if (InHLODActor->DoesRequireWarmup())
	{
		FWorldPartitionHLODWarmupState& WarmupState = HLODActorsToWarmup.FindOrAdd(InHLODActor);

		// In case a previous request to unload was aborted and the cell never unloaded... assume warmup requests are expired after a given amount of frames.
		const uint32 WarmupExpiredFrames = 30;
		const uint32 CurrentFrameNumber = GetWorld()->Scene->GetFrameNumber();

		// Trigger warmup on the first request to unload, or if a warmup request expired
		const bool bInitiateWarmup = WarmupState.WarmupEndFrame == INDEX_NONE || CurrentFrameNumber > (WarmupState.WarmupEndFrame + WarmupExpiredFrames);
		const bool bWarmupCompleted = !bInitiateWarmup && CurrentFrameNumber >= WarmupState.WarmupEndFrame;
		
		if (bInitiateWarmup)
		{
			// Warmup will be triggered in the next BeginRenderView() call, at which point the frame number will have been incremented.
			WarmupState.WarmupStartFrame = CurrentFrameNumber + 1;
			WarmupState.WarmupEndFrame = WarmupState.WarmupStartFrame + CVarHLODWarmupNumFrames.GetValueOnGameThread();
			WarmupState.Location = InCell->GetCellBounds().GetCenter();
		}

		bHLODActorNeedsWarmUp = !bWarmupCompleted;
	}

	return bHLODActorNeedsWarmUp;
}

void UHLODSubsystem::OnCVarsChanged()
{
	bCachedShouldPerformWarmup = ShouldPerformWarmup();
}

bool UHLODSubsystem::ShouldPerformWarmup() const
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

bool UHLODSubsystem::ShouldPerformWarmupForCell(const UWorldPartitionRuntimeCell* InCell) const
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

bool UHLODSubsystem::CanMakeVisible(const UWorldPartitionRuntimeCell* InCell)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UHLODSubsystem::CanMakeVisible)

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

bool UHLODSubsystem::CanMakeInvisible(const UWorldPartitionRuntimeCell* InCell)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UHLODSubsystem::CanMakeInvisible)

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

static void MakeHLODRenderResourcesResident(TMap<UMaterialInterface*, float>& VTRequests, TSet<Nanite::FResources*>& NaniteRequests, const FSceneViewFamily& InViewFamily)
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

			const uint32 NumFramesBeforeRender = CVarHLODWarmupNumFrames.GetValueOnRenderThread();
			for (const Nanite::FResources* Resource : NaniteRequests)
			{
				GetRendererModule().PrefetchNaniteResource(Resource, NumFramesBeforeRender);
			}
		});
	}
}

void UHLODSubsystem::OnBeginRenderViews(const FSceneViewFamily& InViewFamily)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UHLODSubsystem::OnBeginRenderViews)

	TMap<UMaterialInterface*, float> VTRequests;
	TSet<Nanite::FResources*> NaniteRequests;

	const bool bWarmupNanite = CVarHLODWarmupNanite.GetValueOnGameThread() != 0;
	const bool bWarmupVT = CVarHLODWarmupVT.GetValueOnGameThread() != 0;

	for (FHLODWarmupStateMap::TIterator HLODActorWarmupStateIt(HLODActorsToWarmup); HLODActorWarmupStateIt; ++HLODActorWarmupStateIt)
	{
		const FObjectKey& HLODActorObjectKey = HLODActorWarmupStateIt.Key();
		const FWorldPartitionHLODWarmupState& HLODWarmupState = HLODActorWarmupStateIt.Value();
		const AWorldPartitionHLOD* HLODActor = Cast<AWorldPartitionHLOD>(HLODActorObjectKey.ResolveObjectPtr());
		
		if (HLODActor && InViewFamily.FrameNumber < HLODWarmupState.WarmupEndFrame)
		{
			HLODActor->ForEachComponent<UStaticMeshComponent>(false, [&](UStaticMeshComponent* SMC)
			{
				// Assume ISM HLOD don't need warmup, as they are actually found in the source level
				if (SMC->IsA<UInstancedStaticMeshComponent>() || !SMC->GetStaticMesh())
				{
					return;
				}

				// Retrieve this component's bound - we must support getting the bounds before the component is even registered.
				FVector BoundsOrigin = SMC->GetStaticMesh()->GetBounds().Origin + SMC->GetRelativeLocation();
				FVector BoundsExtent = SMC->GetStaticMesh()->GetBounds().BoxExtent;
				
				float ScreenSizePixels = 0;
				if (IsInView(BoundsOrigin, BoundsExtent, InViewFamily, bWarmupVT, ScreenSizePixels))
				{
					if (bWarmupVT)
					{
						PrepareVTRequests(VTRequests, SMC, ScreenSizePixels);
					}

					if (bWarmupNanite)
					{
						// Only issue Nanite requests on the first warmup frame
						if (HLODWarmupState.WarmupStartFrame == InViewFamily.FrameNumber)
						{
							PrepareNaniteRequests(NaniteRequests, SMC);
						}
					}

#if ENABLE_DRAW_DEBUG
					if (CVarHLODWarmupDebugDraw.GetValueOnAnyThread())
					{
						DrawDebugBox(HLODActor->GetWorld(), BoundsOrigin, BoundsExtent, FColor::Yellow, /*bPersistentLine*/ false, /*Lifetime*/ 1.0f);
					}
#endif
				}
			});
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

