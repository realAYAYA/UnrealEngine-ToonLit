// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapeSubsystem.h"
#include "Engine/Engine.h"
#include "UObject/UObjectGlobals.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"
#include "Modules/ModuleManager.h"
#include "ContentStreaming.h"
#include "HAL/IConsoleManager.h"
#include "Landscape.h"
#include "LandscapeEditTypes.h"
#include "LandscapeProxy.h"
#include "LandscapeStreamingProxy.h"
#include "LandscapeInfo.h"
#include "LandscapeInfoMap.h"
#include "LandscapeModule.h"
#include "LandscapeRender.h"
#include "LandscapePrivate.h"
#include "LandscapeSettings.h"
#include "LandscapeGrassMapsBuilder.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "WorldPartition/WorldPartitionSubsystem.h"
#include "ActorPartition/ActorPartitionSubsystem.h"
#include "Engine/World.h"
#include "Math/IntRect.h"
#include "LandscapeNotification.h"
#include "LandscapeConfigHelper.h"
#include "Engine/Canvas.h"
#include "EngineUtils.h"
#include "Misc/ScopedSlowTask.h"
#include "Algo/Transform.h"
#include "Algo/RemoveIf.h"
#include "Algo/Unique.h"
#include "Misc/App.h"
#include "Misc/DateTime.h"
#include "AssetCompilingManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LandscapeSubsystem)

#if WITH_EDITOR
#include "ActionableMessageSubsystem.h"
#include "FileHelpers.h"
#include "Editor.h"
#endif

static int32 GUseStreamingManagerForCameras = 1;
static FAutoConsoleVariableRef CVarUseStreamingManagerForCameras(
	TEXT("grass.UseStreamingManagerForCameras"),
	GUseStreamingManagerForCameras,
	TEXT("1: Use Streaming Manager; 0: Use ViewLocationsRenderedLastFrame"));


static FAutoConsoleVariable CVarMaxAsyncNaniteProxiesPerSecond(
	TEXT("landscape.Nanite.MaxAsyncProxyBuildsPerSecond"),
	6.0f,
	TEXT("Number of Async nanite proxies to dispatch per second"));

static bool GUpdateProxyActorRenderMethodOnTickAtRuntime = false;
static FAutoConsoleVariableRef CVarUpdateProxyActorRenderMethodUpdateOnTickAtRuntime(
	TEXT("landscape.UpdateProxyActorRenderMethodOnTickAtRuntime"),
	GUpdateProxyActorRenderMethodOnTickAtRuntime,
	TEXT("Update landscape proxy's rendering method (nanite enabled) when ticked. Always enabled in editor."));

extern int32 GGrassMapUseRuntimeGeneration;

DECLARE_CYCLE_STAT(TEXT("LandscapeSubsystem Tick"), STAT_LandscapeSubsystemTick, STATGROUP_Landscape);

#define LOCTEXT_NAMESPACE "LandscapeSubsystem"

namespace UE::Landscape
{
#if WITH_EDITOR
	void MarkModifiedLandscapesAsDirty()
	{
		if (UWorld* World = GEditor->GetEditorWorldContext().World())
		{
			if (ULandscapeSubsystem* LandscapeSubsystem = World->GetSubsystem<ULandscapeSubsystem>())
			{
				LandscapeSubsystem->MarkModifiedLandscapesAsDirty();
			}
		}
	}
	
	void BuildGrassMaps()
	{
		if (UWorld* World = GEditor->GetEditorWorldContext().World())
		{
			if (ULandscapeSubsystem* LandscapeSubsystem = World->GetSubsystem<ULandscapeSubsystem>())
			{
				LandscapeSubsystem->BuildGrassMaps();
			}
		}
	}

	void BuildPhysicalMaterial()
	{
		if (UWorld* World = GEditor->GetEditorWorldContext().World())
		{
			if (ULandscapeSubsystem* LandscapeSubsystem = World->GetSubsystem<ULandscapeSubsystem>())
			{
				LandscapeSubsystem->BuildPhysicalMaterial();
			}
		}
	}

	void BuildNanite()
	{
		if (UWorld* World = GEditor->GetEditorWorldContext().World())
		{
			if (ULandscapeSubsystem* LandscapeSubsystem = World->GetSubsystem<ULandscapeSubsystem>())
			{
				LandscapeSubsystem->BuildNanite();
			}
		}
	}

	void BuildAll()
	{
		if (UWorld* World = GEditor->GetEditorWorldContext().World())
		{
			if (ULandscapeSubsystem* LandscapeSubsystem = World->GetSubsystem<ULandscapeSubsystem>())
			{
				LandscapeSubsystem->BuildAll();
			}
		}
	}
#endif //WITH_EDITOR
}

ULandscapeSubsystem::ULandscapeSubsystem()
{
}

ULandscapeSubsystem::~ULandscapeSubsystem()
{
}

void ULandscapeSubsystem::RegisterActor(ALandscapeProxy* Proxy)
{
	Proxies.AddUnique(TWeakObjectPtr<ALandscapeProxy>(Proxy));
	
	if (ALandscape* LandscapeActor = Cast<ALandscape>(Proxy))
	{
		LandscapeActors.AddUnique(TWeakObjectPtr<ALandscape>(LandscapeActor));
	}
}

void ULandscapeSubsystem::UnregisterActor(ALandscapeProxy* Proxy)
{
	Proxies.Remove(TWeakObjectPtr<ALandscapeProxy>(Proxy));

	if (ALandscape* LandscapeActor = Cast<ALandscape>(Proxy))
	{
		LandscapeActors.Remove(TWeakObjectPtr<ALandscape>(LandscapeActor));
	}
}

void ULandscapeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (UWorld* World = GetWorld())
	{
		if (AWorldSettings* WorldSettings = World->GetWorldSettings())
		{
			OnNaniteWorldSettingsChangedHandle = WorldSettings->OnNaniteSettingsChanged.AddUObject(this, &ULandscapeSubsystem::OnNaniteWorldSettingsChanged);
		}
	}

	static IConsoleVariable* NaniteEnabledCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Nanite"));
	if (NaniteEnabledCVar && !NaniteEnabledCVar->OnChangedDelegate().IsBoundToObject(this))
	{
		NaniteEnabledCVar->OnChangedDelegate().AddUObject(this, &ULandscapeSubsystem::OnNaniteEnabledChanged);
	}

	static IConsoleVariable* LandscapeNaniteEnabledCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("landscape.RenderNanite"));
	if (LandscapeNaniteEnabledCVar && !LandscapeNaniteEnabledCVar->OnChangedDelegate().IsBoundToObject(this))
	{
		LandscapeNaniteEnabledCVar->OnChangedDelegate().AddUObject(this, &ULandscapeSubsystem::OnNaniteEnabledChanged);
	}

	TextureStreamingManager = new FLandscapeTextureStreamingManager();
	check(TextureStreamingManager);

	GrassMapsBuilder = new FLandscapeGrassMapsBuilder(GetWorld(), *TextureStreamingManager);

#if WITH_EDITOR
	PhysicalMaterialBuilder = new FLandscapePhysicalMaterialBuilder(GetWorld());

	if (!IsRunningCommandlet())
	{
		NotificationManager = new FLandscapeNotificationManager();
	}

#endif
	
	OnScalabilityChangedHandle = Scalability::OnScalabilitySettingsChanged.AddLambda([](const Scalability::FQualityLevels& QualityLevels)
    {
    	for (auto* LandscapeComponent : TObjectRange<ULandscapeComponent>(RF_ClassDefaultObject | RF_ArchetypeObject, true, EInternalObjectFlags::Garbage))
    	{
    		LandscapeComponent->MarkRenderStateDirty();
    	}
    });
    	
	FCoreUObjectDelegates::GetPostGarbageCollect().AddUObject(this, &ULandscapeSubsystem::HandlePostGarbageCollect);
}

void ULandscapeSubsystem::Deinitialize()
{
	FCoreUObjectDelegates::GetPostGarbageCollect().RemoveAll(this);

	if (OnNaniteWorldSettingsChangedHandle.IsValid())
	{
		UWorld* World = GetWorld();
		check(World != nullptr);

		AWorldSettings* WorldSettings = World->GetWorldSettings();
		check(WorldSettings != nullptr);

		WorldSettings->OnNaniteSettingsChanged.Remove(OnNaniteWorldSettingsChangedHandle);
		OnNaniteWorldSettingsChangedHandle.Reset();
	}

	static IConsoleVariable* NaniteEnabledCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Nanite"));
	if (NaniteEnabledCVar)
	{
		NaniteEnabledCVar->OnChangedDelegate().RemoveAll(this);
	}

	static IConsoleVariable* LandscapeNaniteEnabledCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("landscape.RenderNanite"));
	if (LandscapeNaniteEnabledCVar)
	{
		LandscapeNaniteEnabledCVar->OnChangedDelegate().RemoveAll(this);
	}

	Scalability::OnScalabilitySettingsChanged.Remove(OnScalabilityChangedHandle);
	
#if WITH_EDITOR
	
	while (NaniteBuildsInFlight != 0)
	{
		ENamedThreads::Type CurrentThread = FTaskGraphInterface::Get().GetCurrentThreadIfKnown();
		FTaskGraphInterface::Get().ProcessThreadUntilIdle(CurrentThread);
		FAssetCompilingManager::Get().ProcessAsyncTasks();
	}

	delete GrassMapsBuilder;
	delete PhysicalMaterialBuilder;
	delete NotificationManager;
#endif
	Proxies.Empty();
	LandscapeActors.Empty();

	Super::Deinitialize();
}

void ULandscapeSubsystem::HandlePostGarbageCollect()
{
	ALandscapeProxy::RemoveInvalidExclusionBoxes();
	GetTextureStreamingManager()->CleanupInvalidEntries();
}

TStatId ULandscapeSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULandscapeSubsystem, STATGROUP_Tickables);
}

void ULandscapeSubsystem::RegisterComponent(ULandscapeComponent* Component)
{
	GetGrassMapBuilder()->RegisterComponent(Component);
}

void ULandscapeSubsystem::UnregisterComponent(ULandscapeComponent* Component)
{
	GetGrassMapBuilder()->UnregisterComponent(Component);
}


void ULandscapeSubsystem::RemoveGrassInstances(const TSet<ULandscapeComponent*>* ComponentsToRemoveGrassInstances)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ULandscapeSubsystem::RemoveGrassInstances);
	for (TWeakObjectPtr<ALandscapeProxy> ProxyPtr : Proxies)
	{
		if (ALandscapeProxy* Proxy = ProxyPtr.Get())
		{
			Proxy->FlushGrassComponents(ComponentsToRemoveGrassInstances, /*bFlushGrassMaps = */false);
		}
	}
}

void ULandscapeSubsystem::RegenerateGrass(bool bInFlushGrass, bool bInForceSync, TOptional<TArrayView<FVector>> InOptionalCameraLocations)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ULandscapeSubsystem::RegenerateGrass);

	if (Proxies.IsEmpty())
	{
		return;
	}

	UWorld* World = GetWorld();

	if (bInFlushGrass)
	{
		RemoveGrassInstances();
	}

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UpdateGrass);

		TArray<FVector> CameraLocations;
		if (InOptionalCameraLocations.IsSet())
		{
			CameraLocations = *InOptionalCameraLocations;
		}
		else
		{
			if (GUseStreamingManagerForCameras == 0)
			{
				CameraLocations = World->ViewLocationsRenderedLastFrame;
			}
			else if (int32 Num = IStreamingManager::Get().GetNumViews())
			{
				CameraLocations.Reserve(Num);
				for (int32 Index = 0; Index < Num; Index++)
				{
					const FStreamingViewInfo& ViewInfo = IStreamingManager::Get().GetViewInformation(Index);
					CameraLocations.Add(ViewInfo.ViewOrigin);
				}
			}
		}

		// Update the grass near the specified location(s) : 
		for (TWeakObjectPtr<ALandscapeProxy> ProxyPtr : Proxies)
		{
			if (ALandscapeProxy* Proxy = ProxyPtr.Get())
			{
				Proxy->UpdateGrass(CameraLocations, bInForceSync);
			}
		}
	}
}

ETickableTickType ULandscapeSubsystem::GetTickableTickType() const
{
	return HasAnyFlags(RF_ClassDefaultObject) || !GetWorld() || GetWorld()->IsNetMode(NM_DedicatedServer) ? ETickableTickType::Never : ETickableTickType::Always;
}

bool ULandscapeSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// we also support inactive worlds -- they are used when the world is already saved, but SaveAs renames it:
	// then it duplicates the world (producing an inactive world), which we then need to update Landscapes in during OnPreSave()
	return Super::DoesSupportWorldType(WorldType) || WorldType == EWorldType::Inactive;
}

void ULandscapeSubsystem::Tick(float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_LandscapeSubsystemTick);
	TRACE_CPUPROFILER_EVENT_SCOPE(ULandscapeSubsystem::Tick);
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(Landscape);
	LLM_SCOPE(ELLMTag::Landscape);

	Super::Tick(DeltaTime);

	UWorld* World = GetWorld();
	bool bIsGameWorld = World->IsGameWorld();

#if WITH_EDITOR
	AppCurrentDateTime = FDateTime::Now();
	uint32 FrameNumber = World->Scene->GetFrameNumber();
	const bool bIsTimeOnlyTick = (FrameNumber == LastTickFrameNumber);

	ILandscapeModule& LandscapeModule = FModuleManager::GetModuleChecked<ILandscapeModule>("Landscape");
	// Check if we need to start or stop creating Collision SceneProxies. Don't do this on time-only ticks as the viewport (therefore the scenes) are not drawn in that case, which would lead to wrongly
	//  assume that no view needed collision this frame
	if (!bIsTimeOnlyTick)
	{
		int32 NumViewsWithShowCollision = LandscapeModule.GetLandscapeSceneViewExtension()->GetNumViewsWithShowCollision();
		const bool bNewShowCollisions = NumViewsWithShowCollision > 0;
		const bool bShowCollisionChanged = (bNewShowCollisions != bAnyViewShowCollisions);
		bAnyViewShowCollisions = bNewShowCollisions;
        
		if (bShowCollisionChanged)
		{
			for (ULandscapeHeightfieldCollisionComponent* LandscapeHeightfieldCollisionComponent : TObjectRange<ULandscapeHeightfieldCollisionComponent>(RF_ClassDefaultObject | RF_ArchetypeObject, true, EInternalObjectFlags::Garbage))
			{
				LandscapeHeightfieldCollisionComponent->MarkRenderStateDirty();
			}
		}
	}
#endif // WITH_EDITOR
	
	// Double check requested textures are in the right state
	TextureStreamingManager->CheckRequestedTextures();

	static TArray<FVector> OldCameras;
	TArray<FVector>* Cameras = nullptr;
	if (GUseStreamingManagerForCameras == 0)
	{
		if (OldCameras.Num() || World->ViewLocationsRenderedLastFrame.Num())
		{
			// there is a bug here, which often leaves us with no cameras in the editor -- try to fall back to previous camera position(s)
			if (World->ViewLocationsRenderedLastFrame.Num())
			{
				check(IsInGameThread());
				OldCameras = World->ViewLocationsRenderedLastFrame;
			}
			Cameras = &OldCameras;
		}
	}
	else
	{
		int32 Num = IStreamingManager::Get().GetNumViews();
		if (Num)
		{
			OldCameras.Reset(Num);
			for (int32 Index = 0; Index < Num; Index++)
			{
				auto& ViewInfo = IStreamingManager::Get().GetViewInformation(Index);
				OldCameras.Add(ViewInfo.ViewOrigin);
			}
			Cameras = &OldCameras;
		}
	}

	// dereference and filter TWeakObjectPtr proxies once, up front, to get the list of active proxies
	// also run early update on Proxies, and determine if all of the proxies are ready for grass generation to start
	// TODO [chris.tchou] We should drop the usage of TWeakObjectPtr to reduce the dereference cost, can rely on register/unregister instead
	bool bAllProxiesReadyForGrassMapGeneration = true;
	bool bAllProxiesRuntimeGrassMapsDisabled = true;

#if WITH_EDITOR
	for (TWeakObjectPtr<ALandscape> ActorPtr : LandscapeActors)
	{
		if (ALandscape* Landscape = ActorPtr.Get())
		{
			// if either of these things are true, then we wait for them to complete before running ANY grass map updates..
			bool bLandscapeToolIsModifyingLandscape = !Landscape->bGrassUpdateEnabled;
			bool bLandscapeLayerMergeWillBePerformedSoon = !Landscape->IsUpToDate() && Landscape->GetLandscapeInfo()->SupportsLandscapeEditing();
			if (bLandscapeToolIsModifyingLandscape || bLandscapeLayerMergeWillBePerformedSoon)
			{
				bAllProxiesReadyForGrassMapGeneration = false;
			}
		}
	}
#endif // WITH_EDITOR

	static TArray<ALandscapeProxy*> ActiveProxies;
	ActiveProxies.Reset(Proxies.Num());
	for (TWeakObjectPtr<ALandscapeProxy> ProxyPtr : Proxies)
	{
		if (ALandscapeProxy* Proxy = ProxyPtr.Get())
		{
			ActiveProxies.Add(Proxy);
			
			// Update the proxies proxy
			{
				if (!Proxy->GetDisableRuntimeGrassMapGeneration())
				{
					bAllProxiesRuntimeGrassMapsDisabled = false;
				}

#if WITH_EDITOR
				if (!bIsGameWorld)
				{
					// in editor, automatically update component grass types if the material changes
					for (ULandscapeComponent* Component : Proxy->LandscapeComponents)
					{
						Component->UpdateGrassTypes();
					}
				}
#endif // WITH_EDITOR

				// Update the grass type summary if necessary
				if (!Proxy->IsGrassTypeSummaryValid())
				{
					Proxy->UpdateGrassTypeSummary();
				}
			}
		}
	}

	bool bGrassMapGenerationDisabled = bAllProxiesRuntimeGrassMapsDisabled;
#if WITH_EDITOR
	if (GIsEditor && !bIsGameWorld)
	{
		bGrassMapGenerationDisabled = false;
	}
#endif // WITH_EDITOR

	bool bAllowStartGrassMapGeneration = bAllProxiesReadyForGrassMapGeneration && !bGrassMapGenerationDisabled && (!bIsGameWorld || GGrassMapUseRuntimeGeneration);

	GrassMapsBuilder->AmortizedUpdateGrassMaps(Cameras ? *Cameras : TArray<FVector>(), bIsGrassCreationPrioritized, bAllowStartGrassMapGeneration);

	int32 InOutNumComponentsCreated = 0;
#if WITH_EDITOR
	int32 NumProxiesUpdated = 0;
	int32 NumMeshesToUpdate = 0;
	NumNaniteMeshUpdatesAvailable += CVarMaxAsyncNaniteProxiesPerSecond->GetFloat() * DeltaTime;
	if (NumNaniteMeshUpdatesAvailable > 1.0f)
	{
		NumMeshesToUpdate = NumNaniteMeshUpdatesAvailable;
		NumNaniteMeshUpdatesAvailable -= NumMeshesToUpdate;
	}
#endif // WITH_EDITOR
	for (ALandscapeProxy* Proxy : ActiveProxies)
	{
#if WITH_EDITOR
		if (GIsEditor)
		{
			if (ALandscape* Landscape = Cast<ALandscape>(Proxy))
			{
				Landscape->TickLayers(DeltaTime);
			}

			// editor-only
			if (!World->IsPlayInEditor())
			{
				Proxy->UpdatePhysicalMaterialTasks();
			}
		}

		Proxy->GetAsyncWorkMonitor().Tick(DeltaTime);

		if (IsLiveNaniteRebuildEnabled())
		{
			if (NumProxiesUpdated < NumMeshesToUpdate && Proxy->GetAsyncWorkMonitor().CheckIfUpdateTriggeredAndClear(FAsyncWorkMonitor::EAsyncWorkType::BuildNaniteMeshes))
			{
				NumProxiesUpdated++;
				Proxy->UpdateNaniteRepresentation(/* const ITargetPlatform* = */nullptr);
			}
		}
#endif //WITH_EDITOR
		// TODO [chris.tchou] : this stops all async task processing if cameras go away, which might leave tasks dangling
		if (Cameras && Proxy->ShouldTickGrass())
		{
			Proxy->TickGrass(*Cameras, InOutNumComponentsCreated);
		}

#if !WITH_EDITOR
		if (GUpdateProxyActorRenderMethodOnTickAtRuntime)
#endif // WITH_EDITOR
		{
			Proxy->UpdateRenderingMethod();
		}
	}

	ActiveProxies.Reset();
	
	ALandscapeProxy::DebugDrawExclusionBoxes(World);

#if WITH_EDITOR
	if (GIsEditor && !World->IsPlayInEditor())
	{
		LandscapePhysicalMaterial::GarbageCollectTasks();

		if (NotificationManager)
		{
			NotificationManager->Tick();
		}
	}

	NaniteMeshBuildEvents.RemoveAllSwap([](const FGraphEventRef& Ref) -> bool { return Ref->IsComplete(); });

	LastTickFrameNumber = FrameNumber;
	
	if (UActionableMessageSubsystem* ActionableMessageSubsystem = World->GetSubsystem<UActionableMessageSubsystem>())
	{
		FActionableMessage ActionableMessage;
		const FName LandscapeMessageProvider = TEXT("Landscape");

		if (GetActionableMessage(ActionableMessage))
		{
			ActionableMessageSubsystem->SetActionableMessage(LandscapeMessageProvider, ActionableMessage);
		}
		else
		{
			ActionableMessageSubsystem->ClearActionableMessage(LandscapeMessageProvider);
		}
	}


#endif // WITH_EDITOR
}

void ULandscapeSubsystem::ForEachLandscapeInfo(TFunctionRef<bool(ULandscapeInfo*)> ForEachLandscapeInfoFunc) const
{
	if (ULandscapeInfoMap* LandscapeInfoMap = ULandscapeInfoMap::FindLandscapeInfoMap(GetWorld()))
	{
		for (const auto& Pair : LandscapeInfoMap->Map)
		{
			if (ULandscapeInfo* LandscapeInfo = Pair.Value)
			{
				if (!ForEachLandscapeInfoFunc(LandscapeInfo))
				{
					return;
				}
			}
		}
	}
}

void ULandscapeSubsystem::OnNaniteEnabledChanged(IConsoleVariable*)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_Landscape_OnNaniteEnabledChanged);

	for (TWeakObjectPtr<ALandscapeProxy>& ProxyPtr : Proxies)
	{
		if (ALandscapeProxy* Proxy = ProxyPtr.Get())
		{
			Proxy->UpdateRenderingMethod();
		}
	}
}

#if WITH_EDITOR
void ULandscapeSubsystem::BuildAll()
{
	// This is a deliberate action, make sure to flush all packages that are 'pending dirty' :
	MarkModifiedLandscapesAsDirty();

	BuildGrassMaps();
	BuildPhysicalMaterial();
	BuildNanite();
}

void ULandscapeSubsystem::BuildGrassMaps()
{
	GrassMapsBuilder->Build();
}

void ULandscapeSubsystem::BuildPhysicalMaterial()
{
	PhysicalMaterialBuilder->Rebuild();
}

TArray<ALandscapeProxy*> ULandscapeSubsystem::GetOutdatedProxies(UE::Landscape::EOutdatedDataFlags InMatchingOutdatedDataFlags, bool bInMustMatchAllFlags) const
{
	UWorld* World = GetWorld();
	if (!World || World->IsGameWorld())
	{
		return {};
	}

	TArray<ALandscapeProxy*> FinalProxiesToBuild;
	Algo::TransformIf(Proxies, FinalProxiesToBuild, 
		[InMatchingOutdatedDataFlags, bInMustMatchAllFlags](const TWeakObjectPtr<ALandscapeProxy>& InProxyPtr)
		{ 
			UE::Landscape::EOutdatedDataFlags ProxyOutdatedDataFlags = InProxyPtr->GetOutdatedDataFlags();
			return bInMustMatchAllFlags
				? EnumHasAllFlags(ProxyOutdatedDataFlags, InMatchingOutdatedDataFlags)
				: EnumHasAnyFlags(ProxyOutdatedDataFlags, InMatchingOutdatedDataFlags);
		}, 
		[](const TWeakObjectPtr<ALandscapeProxy>& InProxyPtr) { return InProxyPtr.Get(); });

	return FinalProxiesToBuild;
}

int32 ULandscapeSubsystem::GetOutdatedGrassMapCount()
{
	return GrassMapsBuilder->GetOutdatedGrassMapCount(/*bInForceUpdate*/false);
}

int32 ULandscapeSubsystem::GetOudatedPhysicalMaterialComponentsCount()
{
	return PhysicalMaterialBuilder->GetOudatedPhysicalMaterialComponentsCount();
}

void ULandscapeSubsystem::BuildNanite(TArrayView<ALandscapeProxy*> InProxiesToBuild, bool bForceRebuild)
{
	TRACE_BOOKMARK(TEXT("ULandscapeSubsystem::BuildNanite"));

	UWorld* World = GetWorld();
	if (!World || World->IsGameWorld())
	{
		return;
	}

	if (InProxiesToBuild.IsEmpty() && Proxies.IsEmpty())
	{
		return;
	}

	TArray<ALandscapeProxy*> FinalProxiesToBuild;
	if (InProxiesToBuild.IsEmpty())
	{
		Algo::Transform(Proxies, FinalProxiesToBuild, [](const TWeakObjectPtr<ALandscapeProxy>& InProxyPtr) { return InProxyPtr.Get(); });
	}
	else 
	{
		for (ALandscapeProxy* ProxyToBuild : InProxiesToBuild)
		{
			FinalProxiesToBuild.Add(ProxyToBuild);
			// Build all streaming proxies in the case of a ALandscape :
			if (ALandscape* Landscape = Cast<ALandscape>(ProxyToBuild))
			{
				ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
				if (LandscapeInfo != nullptr)
				{
					Algo::Transform(LandscapeInfo->StreamingProxies, FinalProxiesToBuild, [](const TWeakObjectPtr<ALandscapeStreamingProxy>& InStreamingProxy) { return InStreamingProxy.Get(); });
				}
			}
		}
	}

	// Only keep unique copies : 
	FinalProxiesToBuild.Sort();
	FinalProxiesToBuild.SetNum(Algo::Unique(FinalProxiesToBuild));

	// Don't keep those that are null or already up to date :
	FinalProxiesToBuild.SetNum(Algo::RemoveIf(FinalProxiesToBuild, [bForceRebuild](ALandscapeProxy* InProxy) { return (InProxy == nullptr) || (!bForceRebuild && InProxy->IsNaniteMeshUpToDate()); }));

	FGraphEventArray AsyncEvents;
	for (ALandscapeProxy* LandscapeProxy : FinalProxiesToBuild)
	{
		// reset the nanite content guid so we force rebuild nanite
		if (ULandscapeNaniteComponent* NaniteComponent = LandscapeProxy->GetComponentByClass<ULandscapeNaniteComponent>(); NaniteComponent != nullptr && bForceRebuild)
		{
			UE_LOG(LogLandscape, Log, TEXT("Reset proxy: '%s'"), *LandscapeProxy->GetActorNameOrLabel());
			NaniteComponent->SetProxyContentId(FGuid());
		}

		if (LandscapeProxy->IsNaniteMeshUpToDate())
		{
			continue;
		}
		AsyncEvents.Add(LandscapeProxy->UpdateNaniteRepresentationAsync(nullptr));
	}

	FGraphEventRef WaitForAllNaniteUpdates = FFunctionGraphTask::CreateAndDispatchWhenReady([]() {},
		TStatId(),
		&AsyncEvents,
		ENamedThreads::GameThread);
	
	int32 LastRemainingMeshes = NaniteBuildsInFlight.load();
	int32 TotalMeshes = LastRemainingMeshes;
	FScopedSlowTask SlowTask(TotalMeshes, (LOCTEXT("Landscape_BuildNanite", "Building Nanite Landscape Meshes")));
	// todo [don.boogert] - need mechanism to abort all the current work here
	const bool bShowCancelButton = false;
	SlowTask.MakeDialog(bShowCancelButton);

	// we have to drain the game thread tasks and static mesh builds
	while (NaniteBuildsInFlight != 0)
	{
		int32 Remaining = NaniteBuildsInFlight.load();
		int32 MeshesProcessed = LastRemainingMeshes - Remaining;
		LastRemainingMeshes = Remaining;

		ENamedThreads::Type CurrentThread = FTaskGraphInterface::Get().GetCurrentThreadIfKnown();
		FTaskGraphInterface::Get().ProcessThreadUntilIdle(CurrentThread);
		FAssetCompilingManager::Get().ProcessAsyncTasks();
		SlowTask.EnterProgressFrame(MeshesProcessed, FText::Format(LOCTEXT("Landscape_BuildNaniteProgress", "Building Nanite Landscape Mesh ({0} of {1})"), FText::AsNumber(TotalMeshes - LastRemainingMeshes), FText::AsNumber(SlowTask.TotalAmountOfWork)));
	}
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
bool ULandscapeSubsystem::IsDirtyOnlyInModeEnabled()
{
	return ULandscapeInfo::IsDirtyOnlyInModeEnabled();
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

bool ULandscapeSubsystem::GetDirtyOnlyInMode() const
{
	const ULandscapeSettings* Settings = GetDefault<ULandscapeSettings>();
	return (Settings->LandscapeDirtyingMode == ELandscapeDirtyingMode::InLandscapeModeOnly) 
		|| (Settings->LandscapeDirtyingMode == ELandscapeDirtyingMode::InLandscapeModeAndUserTriggeredChanges);
}

void ULandscapeSubsystem::SaveModifiedLandscapes()
{	
	TSet<UPackage*> SetDirtyPackages;
	TSet<FName> PackagesToSave;

	const bool bSkipDirty = false;

	// Gather list of packages to save and make them dirty so they are considered by FEditorFileUtils::SaveDirtyPackages.
	ForEachLandscapeInfo([&](ULandscapeInfo* LandscapeInfo)
	{
		for(UPackage* ModifiedPackage : LandscapeInfo->GetModifiedPackages())
		{
			PackagesToSave.Add(ModifiedPackage->GetFName());
			if (!ModifiedPackage->IsDirty())
			{
				SetDirtyPackages.Add(ModifiedPackage);
				ModifiedPackage->SetDirtyFlag(true);
			}
		}
		return true;
	});

	const bool bPromptUserToSave = true;
	const bool bSaveMapPackages = true;
	const bool bSaveContentPackages = true;
	const bool bFastSave = false;
	const bool bNotifyNoPackagesSaved = false;
	const bool bCanBeDeclined = true;

	FEditorFileUtils::SaveDirtyPackages(bPromptUserToSave, bSaveMapPackages, bSaveContentPackages, bFastSave, bNotifyNoPackagesSaved, bCanBeDeclined, nullptr,
		[PackagesToSave](UPackage* DirtyPackage)
		{
			if (PackagesToSave.Contains(DirtyPackage->GetFName()))
			{
				return false;
			}
			return true;
		});

	// If Package wasn't saved it is still in the LandscapeInfo ModifiedPackage list, set its dirty flag back to false.
	ForEachLandscapeInfo([&](ULandscapeInfo* LandscapeInfo)
	{
		for (UPackage* ModifiedPackage : LandscapeInfo->GetModifiedPackages())
		{
			if (SetDirtyPackages.Contains(ModifiedPackage))
			{
				ModifiedPackage->SetDirtyFlag(false);
			}
		}
		return true;
	});
}

void ULandscapeSubsystem::MarkModifiedLandscapesAsDirty()
{
	// Flush all packages that are pending mark for dirty : 
	ForEachLandscapeInfo([&](ULandscapeInfo* LandscapeInfo)
	{
		LandscapeInfo->MarkModifiedPackagesAsDirty();
		return true;
	});
}

bool ULandscapeSubsystem::HasModifiedLandscapes() const
{
	bool bHasModifiedLandscapes = false;
	ForEachLandscapeInfo([&](ULandscapeInfo* LandscapeInfo)
	{
		if (LandscapeInfo->GetModifiedPackageCount() > 0)
		{
			bHasModifiedLandscapes = true;
			return false;
		}
		return true;
	});
	
	return bHasModifiedLandscapes;
}

bool ULandscapeSubsystem::IsGridBased() const
{
	return UWorld::IsPartitionedWorld(GetWorld());
}

void ULandscapeSubsystem::ChangeGridSize(ULandscapeInfo* LandscapeInfo, uint32 GridSizeInComponents)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ULandscapeSubsystem::ChangeGridSize);
	
	if (!IsGridBased())
	{
		return;
	}

	TSet<AActor*> ActorsToDelete;
	FLandscapeConfigHelper::ChangeGridSize(LandscapeInfo, GridSizeInComponents, ActorsToDelete);
	// This code path is used for converting a non grid based Landscape to a gridbased so it shouldn't delete any actors
	check(!ActorsToDelete.Num());
}

ALandscapeProxy* ULandscapeSubsystem::FindOrAddLandscapeProxy(ULandscapeInfo* LandscapeInfo, const FIntPoint& SectionBase)
{
	if (!IsGridBased())
	{
		return LandscapeInfo->GetCurrentLevelLandscapeProxy(true);
	}

	return FLandscapeConfigHelper::FindOrAddLandscapeStreamingProxy(LandscapeInfo, SectionBase);
}

void ULandscapeSubsystem::DisplayMessages(FCanvas* Canvas, float& XPos, float& YPos)
{
	
}

bool ULandscapeSubsystem::GetActionableMessage(FActionableMessage& OutActionableMessage)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ULandscapeSubsystem::GetActionableMessage);

	const FText DefaultMessage = LOCTEXT("DefaultLandscapeModified.Message", "Landscape is not up to date");
	const FText DefaultTooltip = LOCTEXT("DefaultLandscapeModified.ToolTip", "Assets that affect the Landscape have changed. Rebuild the Landscape to see the results.");
	const FText DefaultActionMessage = LOCTEXT("DefaultLandscapeModified.Action", "Rebuild");

	const TArray<ALandscapeProxy*> OutdatedGrassMapProxies = GetOutdatedProxies(UE::Landscape::EOutdatedDataFlags::GrassMaps, /*bInMustMatchAllFlags = */false);
	const TArray<ALandscapeProxy*> OutdatedPhysicalMaterialProxies = GetOutdatedProxies(UE::Landscape::EOutdatedDataFlags::PhysicalMaterials, /*bInMustMatchAllFlags = */false);
	const TArray<ALandscapeProxy*> OutdatedNaniteProxies = GetOutdatedProxies(UE::Landscape::EOutdatedDataFlags::NaniteMeshes, /*bInMustMatchAllFlags = */false);
	
	const int32 OutdatedGrassmapProxiesCount = OutdatedGrassMapProxies.Num();
	const int32 OutdatedPhysicalMaterialProxiesCount = OutdatedPhysicalMaterialProxies.Num();
	const int32 OutdatedNaniteProxiesCount = OutdatedNaniteProxies.Num();

	const int32 OutdatedFlags = ((OutdatedGrassmapProxiesCount > 0) ? 1 : 0) + ((OutdatedPhysicalMaterialProxiesCount > 0) ? 1 : 0) + ((OutdatedNaniteProxiesCount > 0) ? 1 : 0);

	if (HasModifiedLandscapes())
	{
		OutActionableMessage.Message = LOCTEXT("LandscapeModified.Message", "Landscape assets are out of date");
		OutActionableMessage.Tooltip = LOCTEXT("LandscapeModified.Tooltip", "The Landscape actors visible in your level have been modified as a result of changes to other assets.\nThese changes need to be applied to the Landscape assets.");
		OutActionableMessage.ActionMessage = LOCTEXT("LandscapeModified.Action", "Update");
		OutActionableMessage.ActionCallback = UE::Landscape::MarkModifiedLandscapesAsDirty;

		return true;
	}
	
	if (OutdatedFlags > 1)
	{
		OutActionableMessage.Message = DefaultMessage;
		OutActionableMessage.Tooltip = DefaultTooltip;
		OutActionableMessage.ActionMessage = DefaultActionMessage;
		OutActionableMessage.ActionCallback = UE::Landscape::BuildAll;

		return true;
	}
	
	if (OutdatedGrassmapProxiesCount > 0)
	{
		OutActionableMessage.Message = FText::Format(LOCTEXT("GRASS_MAPS_NEED_TO_BE_REBUILT_FMT", "{0} Landscape {0}|plural(one=actor,other=actors) with grass maps {0}|plural(one=needs,other=need) to be rebuilt"), OutdatedGrassmapProxiesCount);
		OutActionableMessage.Tooltip = DefaultTooltip;
		OutActionableMessage.ActionMessage = DefaultActionMessage;
		OutActionableMessage.ActionCallback = UE::Landscape::BuildGrassMaps;

		return true;
	}
	
	if (OutdatedPhysicalMaterialProxiesCount > 0)
	{
		OutActionableMessage.Message = FText::Format(LOCTEXT("LANDSCAPE_PHYSICALMATERIAL_NEED_TO_BE_REBUILT_FMT", "{0} Landscape {0}|plural(one=actor,other=actors) with physical materials {0}|plural(one=needs,other=need) to be rebuilt"), OutdatedPhysicalMaterialProxiesCount);
		OutActionableMessage.Tooltip = DefaultTooltip;
		OutActionableMessage.ActionMessage = DefaultActionMessage;
		OutActionableMessage.ActionCallback = UE::Landscape::BuildPhysicalMaterial;

		return true;
	}

	if (OutdatedNaniteProxiesCount > 0)
	{
		OutActionableMessage.Message = FText::Format(LOCTEXT("LANDSCAPE_NANITE_MESHES_NEED_TO_BE_REBUILT_FMT", "{0} Landscape {0}|plural(one=actor,other=actors) with Nanite meshes {0}|plural(one=needs,other=need) to be rebuilt"), OutdatedNaniteProxiesCount);
		OutActionableMessage.Tooltip = DefaultTooltip;
		OutActionableMessage.ActionMessage = DefaultActionMessage;
		OutActionableMessage.ActionCallback = UE::Landscape::BuildNanite;

		return true;
	}

	return false;
}

FDateTime ULandscapeSubsystem::GetAppCurrentDateTime()
{
	return AppCurrentDateTime;
}


void ULandscapeSubsystem::AddAsyncEvent(FGraphEventRef GraphEventRef)
{
	NaniteMeshBuildEvents.Add(GraphEventRef);
}

int32 LiveRebuildNaniteOnModification = 0;
static FAutoConsoleVariableRef CVarLiveRebuildNaniteOnModification(
	TEXT("landscape.Nanite.LiveRebuildOnModification"),
	LiveRebuildNaniteOnModification,
	TEXT("Trigger a rebuild of Nanite representation immediately when a modification is performed (World Partition Maps Only)"));

int32 LandscapeMultithreadNaniteBuild = 1;
static FAutoConsoleVariableRef CVarLandscapeMultithreadNaniteBuild(
	TEXT("landscape.Nanite.MultithreadBuild"),
	LandscapeMultithreadNaniteBuild,
	TEXT("Multithread nanite landscape build in (World Partition Maps Only)"));

int32 LandscapeMaxSimultaneousMultithreadNaniteBuilds = -1;
static FAutoConsoleVariableRef CVarLandscapeMaxSimultaneousMultithreadNaniteBuilds(
	TEXT("landscape.Nanite.MaxSimultaneousMultithreadBuilds"),
	LandscapeMaxSimultaneousMultithreadNaniteBuilds,
	TEXT("Max number of simultaneous Nanite static mesh tasks (-1 = unlimited )"));

bool ULandscapeSubsystem::IsMultithreadedNaniteBuildEnabled()
{
	return LandscapeMultithreadNaniteBuild > 0;
}

bool ULandscapeSubsystem::IsLiveNaniteRebuildEnabled()
{
	return LiveRebuildNaniteOnModification > 0;
}

bool ULandscapeSubsystem::AreNaniteBuildsInProgress() const
{
	return NaniteBuildsInFlight.load() > 0;
}

void ULandscapeSubsystem::IncNaniteBuild()
{
	NaniteBuildsInFlight++;
}

void ULandscapeSubsystem::DecNaniteBuild()
{
	NaniteBuildsInFlight--;
	NaniteStaticMeshesInFlight--;
}

void ULandscapeSubsystem::WaitLaunchNaniteBuild()
{
	ON_SCOPE_EXIT
	{
		++NaniteStaticMeshesInFlight;
	};
	
	if (LandscapeMultithreadNaniteBuild != 0 && LandscapeMaxSimultaneousMultithreadNaniteBuilds == -1)
	{
		return;
	}
	
	const int32 MaxNaniteBuilds  = LandscapeMultithreadNaniteBuild ? LandscapeMaxSimultaneousMultithreadNaniteBuilds : 1;

	if (MaxNaniteBuilds < 0)
	{
		return;
	}

	while(NaniteStaticMeshesInFlight >= MaxNaniteBuilds)
	{
		FPlatformProcess::Sleep(0.05f);
	}

	check((LandscapeMultithreadNaniteBuild != 0) || (NaniteStaticMeshesInFlight <= 1));
}

#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE
