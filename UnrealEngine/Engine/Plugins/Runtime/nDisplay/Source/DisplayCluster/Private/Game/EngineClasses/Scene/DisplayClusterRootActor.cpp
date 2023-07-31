// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterRootActor.h"

#include "Components/SceneComponent.h"
#include "Components/DisplayClusterOriginComponent.h"
#include "Components/DisplayClusterCameraComponent.h"
#include "Components/DisplayClusterScreenComponent.h"
#include "Components/DisplayClusterSceneComponentSyncParent.h"
#include "Components/DisplayClusterPreviewComponent.h"
#include "Components/DisplayClusterSyncTickComponent.h"
#include "Components/DisplayClusterICVFXCameraComponent.h"
#include "Components/DisplayClusterSceneComponentSyncThis.h"
#include "CineCameraComponent.h"
#include "ProceduralMeshComponent.h"

#include "DisplayClusterLightCardActor.h"

#include "Config/IPDisplayClusterConfigManager.h"
#include "DisplayClusterConfigurationStrings.h"

#include "IDisplayClusterConfiguration.h"
#include "DisplayClusterConfigurationTypes.h"

#include "DisplayClusterPlayerInput.h"
#include "Blueprints/DisplayClusterBlueprint.h"
#include "Blueprints/DisplayClusterBlueprintGeneratedClass.h"

#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

#include "HAL/IConsoleManager.h"

#include "Misc/DisplayClusterAppExit.h"
#include "Misc/DisplayClusterGlobals.h"
#include "Misc/DisplayClusterHelpers.h"
#include "Misc/DisplayClusterLog.h"
#include "Misc/DisplayClusterStrings.h"

#include "Render/Viewport/DisplayClusterViewportManager.h"

#include "Game/EngineClasses/Scene/DisplayClusterRootActorInitializer.h"

#include "Algo/MaxElement.h"

#if WITH_EDITOR
#include "IConcertSyncClientModule.h"
#include "IConcertClientWorkspace.h"
#include "IConcertSyncClient.h"
#endif

static TAutoConsoleVariable<int32> CVarShowVisualizationComponents(
	TEXT("nDisplay.render.show.visualizationcomponents"),
	1,
	TEXT("Whether to show or hide visualization mesh components \n")
	TEXT("0 : Hides visualization components \n")
	TEXT("1 : Shows visualization components\n")
);

ADisplayClusterRootActor::ADisplayClusterRootActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, OperationMode(EDisplayClusterOperationMode::Disabled)
{
	/*
	 * Origin component
	 *
	 * We HAVE to store the origin (root) component in our own UPROPERTY marked visible.
	 * Live link has a property which maintains a component reference. Live link sets this
	 * through their details panel automatically, which unreal validates in
	 * FComponentReferenceCustomization::IsComponentReferenceValid.
	 *
	 * Unreal won't allow native components that don't have CPF_Edit to be set. Luckily
	 * they search the owning class for a property containing the component.
	 */
	{
		DisplayClusterRootComponent = CreateDefaultSubobject<UDisplayClusterOriginComponent>(TEXT("RootComponent"));
		SetRootComponent(DisplayClusterRootComponent);
	}

	// A helper component to trigger nDisplay Tick() during Tick phase
	SyncTickComponent = CreateDefaultSubobject<UDisplayClusterSyncTickComponent>(TEXT("DisplayClusterSyncTick"));

	// Default nDisplay camera
	DefaultViewPoint = CreateDefaultSubobject<UDisplayClusterCameraComponent>(TEXT("DefaultViewPoint"));
	DefaultViewPoint->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	DefaultViewPoint->SetRelativeLocation(FVector(0.f, 0.f, 50.f));

	ViewportManager = MakeUnique<FDisplayClusterViewportManager>();

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = ETickingGroup::TG_PostUpdateWork;

	bFindCameraComponentWhenViewTarget = false;
	bReplicates = false;
	SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

#if WITH_EDITOR
	Constructor_Editor();
#endif
}

ADisplayClusterRootActor::~ADisplayClusterRootActor()
{
#if WITH_EDITOR
	Destructor_Editor();
#endif
}

bool ADisplayClusterRootActor::IsRunningGameOrPIE() const
{
	if (!IsRunningGame())
	{
#if WITH_EDITOR
		const UWorld* World = GetWorld();
		return World && World->IsPlayInEditor();
#else
		return true;
#endif
	}

	return true;
}

const FDisplayClusterConfigurationICVFX_StageSettings& ADisplayClusterRootActor::GetStageSettings() const
{
	check(CurrentConfigData);

	return CurrentConfigData->StageSettings;
}

const FDisplayClusterConfigurationRenderFrame& ADisplayClusterRootActor::GetRenderFrameSettings() const 
{ 
	check(CurrentConfigData);

	return CurrentConfigData->RenderFrameSettings;
}

void ADisplayClusterRootActor::InitializeFromConfig(UDisplayClusterConfigurationData* ConfigData)
{
	if (ConfigData)
	{
		// Store new config data object
		UpdateConfigDataInstance(ConfigData, true);

		BuildHierarchy();

#if WITH_EDITOR
		if (GIsEditor && GetWorld())
		{
			UpdatePreviewComponents();
		}
#endif
	}
}

void ADisplayClusterRootActor::OverrideFromConfig(UDisplayClusterConfigurationData* ConfigData)
{
	check(ConfigData);
	check(ConfigData->Scene);
	check(ConfigData->Cluster);

	// Override base types and structures
	CurrentConfigData->Meta = ConfigData->Meta;
	CurrentConfigData->Info = ConfigData->Info;
	CurrentConfigData->CustomParameters = ConfigData->CustomParameters;
	CurrentConfigData->Diagnostics = ConfigData->Diagnostics;
	CurrentConfigData->bFollowLocalPlayerCamera = ConfigData->bFollowLocalPlayerCamera;
	CurrentConfigData->bExitOnEsc = ConfigData->bExitOnEsc;

	// Override Scene, but without changing its name to avoid the Editor not being able to control it via MultiUser.
	{
		FName SceneName = NAME_None;

		if (CurrentConfigData->Scene)
		{
			SceneName = CurrentConfigData->Scene->GetFName();

			const FName DeadName = MakeUniqueObjectName(CurrentConfigData, UDisplayClusterConfigurationScene::StaticClass(), "DEAD_DisplayClusterConfigurationScene");
			CurrentConfigData->Scene->Rename(*DeadName.ToString());
		}

		CurrentConfigData->Scene = DuplicateObject(ConfigData->Scene, CurrentConfigData, SceneName);
	}

	// Override Cluster
	if (CurrentConfigData->Cluster)
	{
		CurrentConfigData->Cluster->PrimaryNode = ConfigData->Cluster->PrimaryNode;
		CurrentConfigData->Cluster->Sync        = ConfigData->Cluster->Sync;
		CurrentConfigData->Cluster->Network     = ConfigData->Cluster->Network;
		CurrentConfigData->Cluster->Failover    = ConfigData->Cluster->Failover;

		// Remove nodes in current config that are not in the new config

		for (auto CurrentNodeIt = CurrentConfigData->Cluster->Nodes.CreateIterator(); CurrentNodeIt; ++CurrentNodeIt)
		{
			if (!ConfigData->Cluster->Nodes.Find(CurrentNodeIt->Key))
			{
				CurrentNodeIt.RemoveCurrent();
			}
		}

		// Go over new nodes and override the current ones (or add new ones that didn't exist)

		for (auto NewNodeIt = ConfigData->Cluster->Nodes.CreateConstIterator(); NewNodeIt; ++NewNodeIt)
		{
			TObjectPtr<UDisplayClusterConfigurationClusterNode>* CurrentNodePtr = CurrentConfigData->Cluster->Nodes.Find(NewNodeIt->Key);

			// Add the node if it doesn't exist
			if (!CurrentNodePtr)
			{
				CurrentConfigData->Cluster->Nodes.Add(
					NewNodeIt->Key,
					DuplicateObject(NewNodeIt->Value, CurrentConfigData->Cluster, NewNodeIt->Value->GetFName())
				);

				continue;
			}

			UDisplayClusterConfigurationClusterNode* CurrentNode = *CurrentNodePtr;
			check(CurrentNode);

			const UDisplayClusterConfigurationClusterNode* NewNode = NewNodeIt->Value;
			check(NewNode);

			// Override Node settings

			CurrentNode->Host = NewNode->Host;
			CurrentNode->bIsSoundEnabled = NewNode->bIsSoundEnabled;
			CurrentNode->bIsFullscreen = NewNode->bIsFullscreen;
			CurrentNode->WindowRect = NewNode->WindowRect;
			CurrentNode->Postprocess = NewNode->Postprocess;
			CurrentNode->OutputRemap = NewNode->OutputRemap;

			if (ConfigData->bOverrideViewportsFromExternalConfig)
			{
				// Remove viewports in current node that are not in the new node
				for (auto CurrentViewportIt = CurrentNode->Viewports.CreateIterator(); CurrentViewportIt; ++CurrentViewportIt)
				{
					if (!NewNode->Viewports.Find(CurrentViewportIt->Key))
					{
						CurrentViewportIt.RemoveCurrent();
					}
				}

				// Go over viewport and override the current ones (or add new ones that didn't exist)
				for (auto NewViewportIt = NewNode->Viewports.CreateConstIterator(); NewViewportIt; ++NewViewportIt)
				{
					TObjectPtr<UDisplayClusterConfigurationViewport>* CurrentViewportPtr = CurrentNode->Viewports.Find(NewViewportIt->Key);

					// Add the viewport if it doesn't exist
					if (!CurrentViewportPtr)
					{
						CurrentNode->Viewports.Add(
							NewViewportIt->Key,
							DuplicateObject(NewViewportIt->Value, CurrentNode, NewViewportIt->Value->GetFName())
						);
						continue;
					}

					UDisplayClusterConfigurationViewport* CurrentViewport = *CurrentViewportPtr;
					check(CurrentViewport);

					const UDisplayClusterConfigurationViewport* NewViewport = NewViewportIt->Value;
					check(NewViewport);

					// Override viewport settings

					CurrentViewport->Camera = NewViewport->Camera;
					CurrentViewport->RenderSettings.BufferRatio = NewViewport->RenderSettings.BufferRatio;
					CurrentViewport->GPUIndex = NewViewport->GPUIndex;
					CurrentViewport->Region = NewViewport->Region;
					CurrentViewport->ProjectionPolicy = NewViewport->ProjectionPolicy;
				}
			}
		}
	}

	// There is no sense to call BuildHierarchy because it works for non-BP root actors.
	// On the other hand, OverwriteFromConfig method is called for BP root actors only by nature.

	// And update preview stuff in Editor
#if WITH_EDITOR
	if (GIsEditor && GetWorld())
	{
		UpdatePreviewComponents();
	}
#endif
}

void ADisplayClusterRootActor::UpdateConfigDataInstance(UDisplayClusterConfigurationData* ConfigDataTemplate, bool bForceRecreate)
{
	if (ConfigDataTemplate == nullptr)
	{
		CurrentConfigData = nullptr;
		ConfigDataName = TEXT("");
	}
	else
	{
		if (CurrentConfigData == nullptr)
		{
			// Only create config data once. Do not create in constructor as default sub objects or individual properties won't sync
			// properly with instanced values.

			const EObjectFlags CommonFlags = RF_Public | RF_Transactional;
			
			CurrentConfigData = NewObject<UDisplayClusterConfigurationData>(
				this,
				UDisplayClusterConfigurationData::StaticClass(),
				NAME_None,
				IsTemplate() ? RF_ArchetypeObject | CommonFlags : CommonFlags,
				ConfigDataTemplate);
			
			if (CurrentConfigData->Cluster == nullptr)
			{
				CurrentConfigData->Cluster = NewObject<UDisplayClusterConfigurationCluster>(
					CurrentConfigData,
					UDisplayClusterConfigurationCluster::StaticClass(),
					NAME_None,
					IsTemplate() ? RF_ArchetypeObject | CommonFlags : CommonFlags);
			}

			if (CurrentConfigData->Scene == nullptr)
			{
				CurrentConfigData->Scene = NewObject<UDisplayClusterConfigurationScene>(
					CurrentConfigData,
					UDisplayClusterConfigurationScene::StaticClass(),
					NAME_None,
					IsTemplate() ? RF_ArchetypeObject | CommonFlags : CommonFlags);
			}
		}
		else if (bForceRecreate)
		{
			UEngine::FCopyPropertiesForUnrelatedObjectsParams Params;
			PRAGMA_DISABLE_DEPRECATION_WARNINGS
			// Leaving this enabled for now for the purposes of the aggressive replacement auditing
			Params.bAggressiveDefaultSubobjectReplacement = true;
			PRAGMA_ENABLE_DEPRECATION_WARNINGS
			Params.bNotifyObjectReplacement = false;
			Params.bDoDelta = false;
			UEngine::CopyPropertiesForUnrelatedObjects(ConfigDataTemplate, CurrentConfigData, Params);
		}

		ConfigDataName = CurrentConfigData->GetFName();
	}
}

UDisplayClusterConfigurationData* ADisplayClusterRootActor::GetDefaultConfigDataFromAsset() const
{
	UClass* CurrentClass = GetClass();
	while (CurrentClass)
	{
		if (UObject* FoundTemplate = Cast<UObject>(CurrentClass->GetDefaultSubobjectByName(ConfigDataName)))
		{
			return Cast<UDisplayClusterConfigurationData>(FoundTemplate);
		}
		CurrentClass = CurrentClass->GetSuperClass();
	}

	return nullptr;
}

UDisplayClusterConfigurationData* ADisplayClusterRootActor::GetConfigData() const
{
	return CurrentConfigData;
}

bool ADisplayClusterRootActor::IsInnerFrustumEnabled(const FString& InnerFrustumID) const
{
	// add more GUI rules here
	// Inner Frustum Enabled
	//  Camera_1  [ ]
	//  Camera_2  [X]
	//  Camera_3  [X]

	return true;
}

int ADisplayClusterRootActor::GetInnerFrustumPriority(const FString& InnerFrustumID) const
{
	int Order = 100000;
	for (const FDisplayClusterComponentRef& It : InnerFrustumPriority)
	{
		if (It.Name.Compare(InnerFrustumID, ESearchCase::IgnoreCase) == 0)
		{
			return Order;
		}
		Order--;
	}

	return -1;
}

template <typename TComp>
void ImplCollectChildVisualizationOrHiddenComponents(TSet<FPrimitiveComponentId>& OutPrimitives, TComp* pComp, bool bCollectVisualizationComponents = true, bool bCollectHiddenComponents = true)
{
#if WITH_EDITOR

	if (!bCollectVisualizationComponents && !bCollectHiddenComponents)
	{
		return;
	}

	USceneComponent* SceneComp = Cast<USceneComponent>(pComp);
	if (SceneComp)
	{
		TArray<USceneComponent*> Childrens;
		SceneComp->GetChildrenComponents(false, Childrens);
		for (USceneComponent* ChildIt : Childrens)
		{
			if ((bCollectVisualizationComponents && ChildIt->IsVisualizationComponent()) 
				|| (bCollectHiddenComponents && ChildIt->bHiddenInGame))
			{
				UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(ChildIt);
				if (PrimComp)
				{
					OutPrimitives.Add(PrimComp->ComponentId);
				}
			}
		}
	}
#endif
}

template <typename TComp>
void ADisplayClusterRootActor::GetTypedPrimitives(TSet<FPrimitiveComponentId>& OutPrimitives, const TArray<FString>* InCompNames, bool bCollectChildrenVisualizationComponent) const
{
	TArray<TComp*> TypedComponents;
	GetComponents<TComp>(TypedComponents, true);

	for (TComp*& CompIt : TypedComponents)
	{
		if (CompIt)
		{
			if (InCompNames != nullptr)
			{
				// add only comp from names list
				for (const FString& NameIt : (*InCompNames))
				{
					if (CompIt->GetName() == NameIt)
					{
						UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(CompIt);
						if (PrimComp)
						{
							OutPrimitives.Add(PrimComp->ComponentId);
						}

						if (bCollectChildrenVisualizationComponent)
						{
							ImplCollectChildVisualizationOrHiddenComponents(OutPrimitives, CompIt);
						}
						break;
					}
				}
			}
			else
			{
				UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(CompIt);
				if (PrimComp)
				{
					OutPrimitives.Add(PrimComp->ComponentId);
				}

				if (bCollectChildrenVisualizationComponent)
				{
					ImplCollectChildVisualizationOrHiddenComponents(OutPrimitives, CompIt);
				}
			}

		}
	}
}

bool ADisplayClusterRootActor::FindPrimitivesByName(const TArray<FString>& InNames, TSet<FPrimitiveComponentId>& OutPrimitives)
{
	GetTypedPrimitives<UActorComponent>(OutPrimitives, &InNames, false);

	return true;
}

// Gather components not rendered in game
bool ADisplayClusterRootActor::GetHiddenInGamePrimitives(TSet<FPrimitiveComponentId>& OutPrimitives)
{
	check(IsInGameThread());

	OutPrimitives.Empty();

	if (CurrentConfigData)
	{
		// Add warp meshes assigned in the configuration into hide lists
		TArray<FString> WarpMeshNames;
		CurrentConfigData->GetReferencedMeshNames(WarpMeshNames);
		if (WarpMeshNames.Num() > 0)
		{
			GetTypedPrimitives<UStaticMeshComponent>(OutPrimitives, &WarpMeshNames);
			GetTypedPrimitives<UProceduralMeshComponent>(OutPrimitives, &WarpMeshNames);
		}
	}

	// Always hide DC Screen component in game
	GetTypedPrimitives<UDisplayClusterScreenComponent>(OutPrimitives);

#if WITH_EDITOR

	const bool bHideVisualizationComponents = !CVarShowVisualizationComponents.GetValueOnGameThread();

	// Hide visualization and hidden components from RootActor
	{
		TArray<UPrimitiveComponent*> PrimitiveComponents;
		GetComponents(PrimitiveComponents);

		for (UPrimitiveComponent* CompIt : PrimitiveComponents)
		{
			if ((bHideVisualizationComponents && CompIt->IsVisualizationComponent()) || CompIt->bHiddenInGame)
			{
				OutPrimitives.Add(CompIt->ComponentId);
			}

			ImplCollectChildVisualizationOrHiddenComponents(OutPrimitives, CompIt, bHideVisualizationComponents);
		}
	}

	// Hide visualization and hidden components from scene
	if (const UWorld* CurrentWorld = GetWorld())
	{
		// Iterate over all actors, looking for editor components.
		for (const TWeakObjectPtr<AActor>& WeakActor : FActorRange(CurrentWorld))
		{
			if (AActor* Actor = WeakActor.Get())
			{
				// do not render hidden in game actors
				const bool bActorHideInGame = Actor->IsHidden();

				TArray<UPrimitiveComponent*> PrimitiveComponents;
				Actor->GetComponents(PrimitiveComponents);

				for (UPrimitiveComponent* PrimComp : PrimitiveComponents)
				{
					if ((bHideVisualizationComponents && PrimComp->IsVisualizationComponent()) 
						|| ((bActorHideInGame || PrimComp->bHiddenInGame) && !PrimComp->bCastHiddenShadow))
					{
						OutPrimitives.Add(PrimComp->ComponentId);
					}

					ImplCollectChildVisualizationOrHiddenComponents(OutPrimitives, PrimComp, bHideVisualizationComponents);
				}
			}
		}
	}

#endif

	return OutPrimitives.Num() > 0;
}

void ADisplayClusterRootActor::InitializeRootActor()
{
	const UWorld* World = GetWorld();

	if (!World)
	{
		return;
	}
	
	bool bIsPIE = false;

	if (!CurrentConfigData && !ConfigDataName.IsNone())
	{
		// Attempt load from embedded data.
		UpdateConfigDataInstance(GetDefaultConfigDataFromAsset());
	}

	if (ViewportManager.IsValid() == false)
	{
		ViewportManager = MakeUnique<FDisplayClusterViewportManager>();
	}

	// Packaged, PIE and -game runtime
	if (IsRunningGameOrPIE())
	{
		if (CurrentConfigData)
		{
			BuildHierarchy();

#if WITH_EDITOR
			UpdatePreviewComponents();
#endif
			return;
		}
	}
#if WITH_EDITOR
	// Initialize from file property by default in Editor
	else
	{
		if (CurrentConfigData)
		{
			BuildHierarchy();
			UpdatePreviewComponents();
			return;
		}
	}
#endif
}

void ADisplayClusterRootActor::UpdateProceduralMeshComponentData(const UProceduralMeshComponent* InProceduralMeshComponent)
{
	check(IsInGameThread());

	if (ViewportManager.IsValid())
	{
		FName ProceduralComponentName = (InProceduralMeshComponent==nullptr) ? NAME_None : InProceduralMeshComponent->GetFName();

		// Support for all hidden internal refs
		ViewportManager->MarkComponentGeometryDirty(ProceduralComponentName);
	}
}

bool ADisplayClusterRootActor::BuildHierarchy()
{
	check(CurrentConfigData);
	check(IsInGameThread());

	if (!IsBlueprint())
	{
		// Temporary solution. The whole initialization stuff has been moved to a separate initialization class. Since
		// it won't be possible to configure any components in a config file, and the proper asset initialization will
		// be performed on the configurator side, the DCRA won't need to have any custom logic around the components.
		TUniquePtr<FDisplayClusterRootActorInitializer> Initializer = MakeUnique<FDisplayClusterRootActorInitializer>();
		Initializer->Initialize(this, CurrentConfigData);
	}

	return true;
}

void ADisplayClusterRootActor::UpdateLightCardPositions()
{
	// Find a view origin to use as the transform "anchor" of each light card. At the moment, assume the first view origin component
	// found is the correct view origin (the same assumption is made in the light card editor). If no view origin is found, use the root component
	USceneComponent* ViewOriginComponent = GetRootComponent();

	TArray<UDisplayClusterCameraComponent*> ViewOriginComponents;
	GetComponents(ViewOriginComponents);

	if (ViewOriginComponents.Num())
	{
		ViewOriginComponent = ViewOriginComponents[0];
	}

	const FVector Location = ViewOriginComponent ? ViewOriginComponent->GetComponentLocation() : GetActorLocation();
	const FRotator Rotation = GetActorRotation();

	// Iterate over all the light cards referenced in the root actor's config data and update their location and rotation to match the view origin
	if (UDisplayClusterConfigurationData* CurrentData = GetConfigData())
	{
		FDisplayClusterConfigurationICVFX_VisibilityList& LightCards = CurrentData->StageSettings.Lightcard.ShowOnlyList;

		for (const TSoftObjectPtr<AActor>& Actor : LightCards.Actors)
		{
			ADisplayClusterLightCardActor* LightCardActor = Actor.IsValid() ? Cast<ADisplayClusterLightCardActor>(Actor.Get()) : nullptr;
			if (LightCardActor)
			{
				// Set the owner so the light card actor can look the root actor up in certain situations, like adjusting labels
				// when running as -game
				LightCardActor->SetRootActorOwner(this);
				if (LightCardActor->bLockToOwningRootActor)
				{
					LightCardActor->SetActorLocation(Location);
					LightCardActor->SetActorRotation(Rotation);
				}
			}
		}

		if (LightCards.ActorLayers.Num())
		{
			if (UWorld* World = GetWorld())
			{
				for (TActorIterator<ADisplayClusterLightCardActor> Iter(World); Iter; ++Iter)
				{
					ADisplayClusterLightCardActor* LightCardActor = *Iter;
					for (const FActorLayer& ActorLayer : LightCards.ActorLayers)
					{
						if (LightCardActor->Layers.Contains(ActorLayer.Name))
						{
							LightCardActor->SetRootActorOwner(this);
							if (LightCardActor->bLockToOwningRootActor)
							{
								LightCardActor->SetActorLocation(Location);
								LightCardActor->SetActorRotation(Rotation);
							}
							break;
						}
					}
				}
			}
		}
	}
}

bool ADisplayClusterRootActor::IsBlueprint() const
{
	for (UClass* Class = GetClass(); Class; Class = Class->GetSuperClass())
	{
		if (Cast<UBlueprintGeneratedClass>(Class) != nullptr)
		{
			return true;
		}
	}
	
	return false;
}

void ADisplayClusterRootActor::BeginPlay()
{
	Super::BeginPlay();

	// Store current operation mode
	OperationMode = GDisplayCluster->GetOperationMode();
	if (OperationMode == EDisplayClusterOperationMode::Cluster)
	{
		FString SyncPolicyType;

		// Read native input synchronization settings
		IPDisplayClusterConfigManager* const ConfigMgr = GDisplayCluster->GetPrivateConfigMgr();
		if (ConfigMgr)
		{
			UDisplayClusterConfigurationData* ConfigData = ConfigMgr->GetConfig();
			if (ConfigData)
			{
				SyncPolicyType = ConfigData->Cluster->Sync.InputSyncPolicy.Type;
				UE_LOG(LogDisplayClusterGame, Log, TEXT("Native input sync policy: %s"), *SyncPolicyType);
			}
		}

		// Optionally activate native input synchronization
		if (SyncPolicyType.Equals(DisplayClusterConfigurationStrings::config::cluster::input_sync::InputSyncPolicyReplicatePrimary, ESearchCase::IgnoreCase))
		{
			APlayerController* const PlayerController = GetWorld()->GetFirstPlayerController();
			if (PlayerController)
			{
				PlayerController->PlayerInput = NewObject<UDisplayClusterPlayerInput>(PlayerController);
			}
		}
	}

	InitializeRootActor();
}

void ADisplayClusterRootActor::Tick(float DeltaSeconds)
{
	// Update saved DeltaSeconds for root actor
	LastDeltaSecondsValue = DeltaSeconds;

	if (OperationMode == EDisplayClusterOperationMode::Cluster ||
		OperationMode == EDisplayClusterOperationMode::Editor)
	{
		UWorld* const CurWorld = GetWorld();
		if (CurWorld && CurrentConfigData)
		{
			APlayerController* const CurPlayerController = CurWorld->GetFirstPlayerController();
			if (CurPlayerController)
			{
				// Depending on the flag state the DCRA follows or not the current player's camera
				if (CurrentConfigData->bFollowLocalPlayerCamera)
				{
					APlayerCameraManager* const CurPlayerCameraManager = CurPlayerController->PlayerCameraManager;
					if (CurPlayerCameraManager)
					{
						SetActorLocationAndRotation(CurPlayerCameraManager->GetCameraLocation(), CurPlayerCameraManager->GetCameraRotation());
					}
				}

				if (CurrentConfigData->bExitOnEsc)
				{
					if (CurPlayerController->WasInputKeyJustPressed(EKeys::Escape))
					{
						FDisplayClusterAppExit::ExitApplication(FString("Exit on ESC requested"));
					}
				}
			}
		}
	}

	// Show 'not supported' warning if instanced stereo is used
	if (OperationMode != EDisplayClusterOperationMode::Disabled)
	{
		static const TConsoleVariableData<int32>* const InstancedStereoCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("vr.InstancedStereo"));
		if (InstancedStereoCVar)
		{
			const bool bIsInstancedStereoRequested = (InstancedStereoCVar->GetValueOnGameThread() != 0);
			if (bIsInstancedStereoRequested)
			{
				UE_LOG(LogDisplayClusterGame, Error, TEXT("Instanced stereo was requested. nDisplay doesn't support instanced stereo so far."));
				GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::Red, TEXT("nDisplay doesn't support instanced stereo"));
			}
		}
	}

#if WITH_EDITOR
	// Tick editor preview
	Tick_Editor(DeltaSeconds);
#endif

	if (UDisplayClusterConfigurationData* CurrentData = GetConfigData())
	{
		if (CurrentData->StageSettings.Lightcard.bEnable)
		{
			UpdateLightCardPositions();
		}
	}

	Super::Tick(DeltaSeconds);
}

void ADisplayClusterRootActor::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	PostLoad_Editor();
#endif

	InitializeRootActor();
}

void ADisplayClusterRootActor::PostActorCreated()
{
	Super::PostActorCreated();

#if WITH_EDITOR
	PostActorCreated_Editor();
#endif

	InitializeRootActor();
}

void ADisplayClusterRootActor::BeginDestroy()
{
#if WITH_EDITOR
	BeginDestroy_Editor();
#endif

	ViewportManager.Reset();

	Super::BeginDestroy();
}

/**
* Propagate map changes from the DefaultsOwner to the InstanceOwner. The map must be <FString, UObject*>.
* Each instanced object value will also be setup to use default propagation.
*
* If the DefaultsOwner has added a key it will be added to all instances with a new UObject value templated from the
* default value.
* 
* If the DefaultsOwner has removed a key it will be removed from all instances.
* 
* All other elements will be left alone and element values will always work through the default propagation system.
*
* This is necessary in the event the instance has modified individual element value properties. Default container
* propagation will treat the entire container as dirty and not add or remove elements to the instance.
* 
* For nDisplay the instance can only edit element properties, not the container size.
* This allows us to safely propagate size changes from the default map through a custom propagation system.
*
* @param MapProperty The map property to sync.
* @param DefaultsOwner The direct object owning the map property with the default values.
* @param InstanceOwner The direct object owning the map property with the instance values.
*/
static void PropagateDefaultMapToInstancedMap(const FMapProperty* MapProperty, UObject* DefaultsOwner, UObject* InstanceOwner)
{
	check(DefaultsOwner);
	check(InstanceOwner);
	
	const void* MapContainerDefaults = MapProperty->ContainerPtrToValuePtr<void*>(DefaultsOwner);
	const void* MapContainerInstance = MapProperty->ContainerPtrToValuePtr<void*>(InstanceOwner);
	FScriptMapHelper MapDefaultsHelper(MapProperty, MapContainerDefaults);
	FScriptMapHelper MapInstanceHelper(MapProperty, MapContainerInstance);

	bool bHasChanged = false;

	auto AddKeyWithInstancedObject = [&](const FString* Key, UObject* ArchetypeToUse)
	{
		if (const uint8* ExistingPair = MapInstanceHelper.FindMapPairPtrFromHash(Key))
		{
			MapInstanceHelper.RemovePair(ExistingPair);
		}
		
		// Existing objects should only occur in the case of delete and undo.
		UObject* ObjectToAdd = FindObject<UObject>(InstanceOwner, *ArchetypeToUse->GetName());
		if (ObjectToAdd == nullptr)
		{
			// Create the instance to assign to the map. Provide the archetype as a template with
			// the same name so properties are propagated automatically.
			ObjectToAdd = NewObject<UObject>(InstanceOwner, ArchetypeToUse->GetClass(),
			ArchetypeToUse->GetFName(), RF_Transactional, ArchetypeToUse);
		}
		
		MapInstanceHelper.AddPair(Key, &ObjectToAdd);
		
		bHasChanged = true;
	};

	// Look for elements that should be added.
	for (FScriptMapHelper::FIterator DefaultIt = MapDefaultsHelper.CreateIterator(); DefaultIt; ++DefaultIt)
	{
		uint8* DefaultPairPtr = MapDefaultsHelper.GetPairPtr(*DefaultIt);
		check(DefaultPairPtr);
		
		FString* Key = MapProperty->KeyProp->ContainerPtrToValuePtr<FString>(DefaultPairPtr);
		check(Key);
		
		UObject** DefaultObjectPtr =  MapProperty->ValueProp->ContainerPtrToValuePtr<UObject*>(DefaultPairPtr);
		check(DefaultObjectPtr);
		
		UObject* DefaultObject = *DefaultObjectPtr;
		check(DefaultObject);
		
		if (UObject** InstancedObjectPtr = (UObject**)MapInstanceHelper.FindValueFromHash(Key))
		{
			if (const UObject* InstancedObject = *InstancedObjectPtr)
			{
				const bool bArchetypeCorrect = DefaultObject == InstancedObject->GetArchetype();
				
				// The archetype should always match the new default. There are edge cases with MU
				// where the instance may be updated prior to the CDO and should be corrected once
				// the BP is saved. If this occurs outside of MU there could be a serious problem.
				if (!bArchetypeCorrect)
				{
					const ADisplayClusterRootActor* RootActor =
					CastChecked<ADisplayClusterRootActor>(InstancedObject->GetTypedOuter(ADisplayClusterRootActor::StaticClass()));
#if WITH_EDITOR
					if (GEditor)
					{
						bool bIsMultiUserSession = false;
						TSharedPtr<IConcertSyncClient> ConcertSyncClient = IConcertSyncClientModule::Get().GetClient(TEXT("MultiUser"));
						if (ConcertSyncClient.IsValid())
						{
							TSharedPtr<IConcertClientWorkspace> Workspace = ConcertSyncClient->GetWorkspace();
							bIsMultiUserSession = Workspace.IsValid();
						}

						ensure(bArchetypeCorrect || bIsMultiUserSession);
					}
#endif
					UE_LOG(LogDisplayClusterBlueprint, Warning, TEXT("Archetype mismatch on nDisplay config %s. Make sure the config is compiled and saved. Property: %s, Instance: %s, Archetype: %s, Default %s."),
						*RootActor->GetName(), *MapProperty->GetName(), *InstancedObject->GetName(), *InstancedObject->GetArchetype()->GetName(), *DefaultObject->GetName());
				}
				continue;
			}
		}
		
		AddKeyWithInstancedObject(Key, DefaultObject);
	}

	// Look for elements that should be removed.
	for (FScriptMapHelper::FIterator InstanceIt = MapInstanceHelper.CreateIterator(); InstanceIt;)
	{
		uint8* InstancePairPtr = MapInstanceHelper.GetPairPtr(*InstanceIt);
		check(InstancePairPtr);
		
		FString* Key = MapProperty->KeyProp->ContainerPtrToValuePtr<FString>(InstancePairPtr);
		check(Key);
		
		if (!MapDefaultsHelper.FindValueFromHash(Key))
		{
			if (UObject** InstanceObjectPtr = MapProperty->ValueProp->ContainerPtrToValuePtr<UObject*>(InstancePairPtr))
			{
				if (UObject* InstanceObject = *InstanceObjectPtr)
				{
					// Trash the object -- default propagation won't handle this any more.
					// RemoveAt below will remove the reference to it. This transaction can still be undone.
					// Rename to transient package now so the same name is available immediately.
					// It's possible a new object needs to be created with this outer using the same name.
					InstanceObject->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors);
					InstanceObject->SetFlags(RF_Transient);
				}
			}
			
			MapInstanceHelper.RemoveAt(*InstanceIt);
			bHasChanged = true;
		}
		else
		{
			++InstanceIt;
		}
	}
	
	if (bHasChanged)
	{
		MapInstanceHelper.Rehash();
#if WITH_EDITOR
		InstanceOwner->PostEditChange();
#endif
	}
}

/**
 * Syncs default config data changes to a config instance.
 *
 * @param InDefaultConfigData The class default config data object.
 * @param InInstanceConfigData An instance config data object.
 */
static void PropagateDataFromDefaultConfig(UDisplayClusterConfigurationData* InDefaultConfigData, UDisplayClusterConfigurationData* InInstanceConfigData)
{
	check(InDefaultConfigData);
	check(InInstanceConfigData);
	
	const FMapProperty* ClusterNodesMapProperty = FindFieldChecked<FMapProperty>(UDisplayClusterConfigurationCluster::StaticClass(),
																				GET_MEMBER_NAME_CHECKED(UDisplayClusterConfigurationCluster, Nodes));
	const FMapProperty* ViewportsMapProperty = FindFieldChecked<FMapProperty>(UDisplayClusterConfigurationClusterNode::StaticClass(),
																			GET_MEMBER_NAME_CHECKED(UDisplayClusterConfigurationClusterNode, Viewports));
	
	PropagateDefaultMapToInstancedMap(ClusterNodesMapProperty, InDefaultConfigData->Cluster, InInstanceConfigData->Cluster);
	
	for (const TTuple<FString, TObjectPtr<UDisplayClusterConfigurationClusterNode>>& ClusterKeyVal : InDefaultConfigData->Cluster->Nodes)
	{
		UDisplayClusterConfigurationClusterNode* DestinationValue = InInstanceConfigData->Cluster->Nodes.FindChecked(
			ClusterKeyVal.Key);
		PropagateDefaultMapToInstancedMap(ViewportsMapProperty, ClusterKeyVal.Value, DestinationValue);
	}
}

#if WITH_EDITOR
void ADisplayClusterRootActor::RerunConstructionScripts()
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("ADisplayClusterRootActor::RerunConstructionScripts"), STAT_RerunConstructionScripts, STATGROUP_NDisplay);
	
	const IDisplayClusterConfiguration& Config = IDisplayClusterConfiguration::Get();
	if (!Config.IsTransactingSnapshot())
	{
		Super::RerunConstructionScripts();
		
		if (!IsTemplate())
		{
			if (UDisplayClusterConfigurationData* CurrentData = GetConfigData())
			{
				const ADisplayClusterRootActor* CDO = CastChecked<ADisplayClusterRootActor>(GetClass()->GetDefaultObject());
				UDisplayClusterConfigurationData* DefaultData = CDO->GetConfigData();
				PropagateDataFromDefaultConfig(DefaultData, CurrentData);
			}
		}
		RerunConstructionScripts_Editor();
	}
}
#endif

UDisplayClusterCameraComponent* ADisplayClusterRootActor::GetDefaultCamera() const
{
	return DefaultViewPoint;
}

USceneComponent* ADisplayClusterRootActor::GetCommonViewPoint() const
{
	if (UDisplayClusterConfigurationData* ConfigData = GetConfigData())
	{
		if (ConfigData->Cluster)
		{
			TMap<FString, int32> ViewPointCounts;

			// Get all the camera names used by viewports in this cluster
			for (const TPair<FString, TObjectPtr<UDisplayClusterConfigurationClusterNode>>& NodePair : ConfigData->Cluster->Nodes)
			{
				if (!NodePair.Value)
				{
					continue;
				}

				for (const TPair<FString, TObjectPtr<UDisplayClusterConfigurationViewport>>& ViewportPair : NodePair.Value->Viewports)
				{
					if (!ViewportPair.Value)
					{
						continue;
					}

					ViewPointCounts.FindOrAdd(ViewportPair.Value->Camera)++;
				}
			}

			if (!ViewPointCounts.IsEmpty())
			{
				// Find the viewpoint with the most references
				TPair<FString, int32>* MostCommonViewPointCount = Algo::MaxElementBy(ViewPointCounts,
					[](const TPair<FString, int32>& Pair)
					{
						return Pair.Value;
					}
				);

				if (MostCommonViewPointCount && !MostCommonViewPointCount->Key.IsEmpty())
				{
					// Try to return the camera with the most common name
					if (USceneComponent* Camera = GetComponentByName<UDisplayClusterCameraComponent>(MostCommonViewPointCount->Key))
					{
						return Camera;
					}
				}
			}
		}
	}

	// We didn't find a common camera override (or it was empty), so fall back to the default camera
	if (USceneComponent* DefaultCamera = GetDefaultCamera())
	{
		return DefaultCamera;
	}

	// No default camera, so fall back to the cluster root
	return GetRootComponent();
}

bool ADisplayClusterRootActor::SetReplaceTextureFlagForAllViewports(bool bReplace)
{
	IDisplayCluster& Display = IDisplayCluster::Get();

	UDisplayClusterConfigurationData* ConfigData = GetConfigData();

	if (!ConfigData)
	{
		UE_LOG(LogDisplayClusterGame, Warning, TEXT("RootActor's ConfigData was null"));
		return false;
	}

	if (!ConfigData->Cluster)
	{
		UE_LOG(LogDisplayClusterGame, Warning, TEXT("ConfigData's Cluster was null"));
		return false;
	}

	// Only update the viewports of the current node. If there isn't one, which is normal in Editor, we update them all.

	const FString NodeId = Display.GetClusterMgr()->GetNodeId();
	TArray<typename decltype(UDisplayClusterConfigurationCluster::Nodes)::ValueType, TInlineAllocator<1>> Nodes;

	if (NodeId.IsEmpty())
	{
		ConfigData->Cluster->Nodes.GenerateValueArray(Nodes);
	}
	else
	{
		UDisplayClusterConfigurationClusterNode* Node = ConfigData->Cluster->GetNode(NodeId);

		if (!Node)
		{
			UE_LOG(LogDisplayClusterGame, Warning, TEXT("NodeId '%s' not found in ConfigData"), *NodeId);
			return false;
		}

		Nodes.Add(Node);
	}

	for (const UDisplayClusterConfigurationClusterNode* Node: Nodes)
	{
		check(Node != nullptr);

		for (const TPair<FString, TObjectPtr<UDisplayClusterConfigurationViewport>>& ViewportItem : Node->Viewports)
		{
			if (ViewportItem.Value)
			{
				ViewportItem.Value->RenderSettings.Replace.bAllowReplace = bReplace;
			}
		}

		for (const TPair<FString, TObjectPtr<UDisplayClusterConfigurationClusterNode>>& NodeItem : ConfigData->Cluster->Nodes)
		{
			if (!NodeItem.Value)
			{
				continue;
			}

			for (const TPair<FString, TObjectPtr<UDisplayClusterConfigurationViewport>>& ViewportItem : NodeItem.Value->Viewports)
			{
				if (ViewportItem.Value)
				{
					ViewportItem.Value->RenderSettings.Replace.bAllowReplace = bReplace;
				}
			}
		}
	}

	return true;
}

void ADisplayClusterRootActor::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	ADisplayClusterRootActor* This = CastChecked<ADisplayClusterRootActor>(InThis);

	if (This && This->ViewportManager.IsValid())
	{
		This->ViewportManager->AddReferencedObjects(Collector);
	}

	Super::AddReferencedObjects(InThis, Collector);
}
