// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
Level.cpp: Level-related functions
=============================================================================*/

#include "Engine/Level.h"

#include "EngineLogs.h"
#include "Algo/Copy.h"
#include "Algo/ForEach.h"
#include "Algo/Transform.h"
#include "Misc/ScopedSlowTask.h"
#include "HAL/PlatformFile.h"
#include "UObject/UE5MainStreamObjectVersion.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "UObject/FortniteMainBranchObjectVersion.h"
#include "UObject/ReleaseObjectVersion.h"
#include "EngineStats.h"
#include "RenderingThread.h"
#include "GameFramework/Pawn.h"
#include "SceneInterface.h"
#include "PrecomputedLightVolume.h"
#include "PrecomputedVolumetricLightmap.h"
#include "Engine/MapBuildDataRegistry.h"
#include "Components/LightComponent.h"
#include "Model.h"
#include "Containers/TransArray.h"
#include "UObject/MetaData.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/UObjectIterator.h"
#include "UObject/Package.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/Texture2D.h"
#include "ContentStreaming.h"
#include "Engine/AssetUserData.h"
#include "Engine/LevelScriptBlueprint.h"
#include "Engine/LevelScriptActor.h"
#include "Engine/NetDriver.h"
#include "Engine/WorldComposition.h"
#include "StaticLighting.h"
#include "TickTaskManagerInterface.h"
#include "PhysicsEngine/BodySetup.h"
#include "Engine/LevelBounds.h"
#include "Async/ParallelFor.h"
#include "Misc/ArchiveMD5.h"
#if WITH_EDITOR
#include "StaticMeshCompiler.h"
#include "ActorDeferredScriptManager.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "PieFixupSerializer.h"
#include "Editor.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Settings/LevelEditorMiscSettings.h"
#include "ExternalPackageHelper.h"
#include "ActorFolder.h"
#include "Misc/MessageDialog.h"
#include "EditorActorFolders.h"
#include "UObject/MetaData.h"
#include "WorldPartition/WorldPartitionHelpers.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/PathViews.h"
#include "Selection.h"
#include "Framework/Notifications/NotificationManager.h"
#endif
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionLog.h"
#include "WorldPartition/WorldPartitionSubsystem.h"
#include "WorldPartition/WorldPartitionActorDescInstance.h"
#include "Engine/LevelStreaming.h"
#include "LevelUtils.h"
#include "Components/ModelComponent.h"
#include "Engine/LevelActorContainer.h"
#include "ObjectTrace.h"
#include "UObject/MetaData.h"
#include "WorldPartition/WorldPartitionRuntimeCell.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(Level)

#define LOCTEXT_NAMESPACE "ULevel"
DEFINE_LOG_CATEGORY(LogLevel);

int32 GActorClusteringEnabled = 1;
static FAutoConsoleVariableRef CVarActorClusteringEnabled(
	TEXT("gc.ActorClusteringEnabled"),
	GActorClusteringEnabled,
	TEXT("Whether to allow levels to create actor clusters for GC."),
	ECVF_Default
);

int32 GOptimizeActorRegistration = 1;
static FAutoConsoleVariableRef CVarOptimizeActorRegistration(
	TEXT("s.OptimizeActorRegistration"),
	GOptimizeActorRegistration,
	TEXT("Enables optimizations to actor component registration functions like PostRegisterAllComponents\n")
	TEXT(" 0: Disable optimizations for legacy code that depends on redundant calls to registration functions\n")
	TEXT(" 1: Enables optimizations and assumes the code is working properly\n")
	TEXT(" 2: Enables optimization logic, but ensures it is working properly in non shipping builds"),
	ECVF_Default
);

// RouteActorInit for a single actor generally takes about half the time to complete compared to component initialization
float GRouteActorInitializationWorkUnitWeighting = 0.5f;
static FAutoConsoleVariableRef CVarRouteActorInitializationWorkUnitWeighting(
	TEXT("s.RouteActorInitializationWorkUnitWeighting"),
	GRouteActorInitializationWorkUnitWeighting,
	TEXT("Weighting to apply to RouteActorInitialization when computing work units (relative to component init)"),
	ECVF_Default
);

#if WITH_EDITOR

TArray<ULevel::FLevelExternalActorsPathsProviderDelegate> ULevel::LevelExternalActorsPathsProviders;
TArray<ULevel::FLevelMountPointResolverDelegate> ULevel::LevelMountPointResolvers;

void FLevelActorFoldersHelper::SetUseActorFolders(ULevel* InLevel, bool bInEnabled)
{
	InLevel->SetUseActorFoldersInternal(bInEnabled);
}

void FLevelActorFoldersHelper::AddActorFolder(ULevel* InLevel, UActorFolder* InActorFolder, bool bInShouldDirtyLevel, bool bInShouldBroadcast)
{
	InLevel->Modify(bInShouldDirtyLevel);
	check(InActorFolder->GetGuid().IsValid());
	InLevel->AddActorFolder(InActorFolder);

	if (bInShouldBroadcast)
	{
		GEngine->BroadcastActorFolderAdded(InActorFolder);
	}
}

void FLevelActorFoldersHelper::RenameFolder(ULevel* InLevel, const FFolder& InOldFolder, const FFolder& InNewFolder)
{
	check(InLevel);
	// This implementation can be called both if FActorFolders is or isn't initialized.
	if (FActorFolders::Get().IsInitializedForWorld(*InLevel->GetWorld()))
	{
		FActorFolders::Get().RenameFolderInWorld(*InLevel->GetWorld(), InOldFolder, InNewFolder);
	}
	else
	{
		UActorFolder* ActorFolder = InOldFolder.GetActorFolder();
		check(::IsValid(ActorFolder));
		UActorFolder* FoundFolder = InNewFolder.GetActorFolder();
		check(!::IsValid(FoundFolder) || !FoundFolder->GetPath().IsEqual(InNewFolder.GetPath(), ENameCase::CaseSensitive));

		UActorFolder* ParentActorFolder = InNewFolder.GetParent().GetActorFolder();
		ActorFolder->SetParent(ParentActorFolder);
		const FString FolderLabel = InNewFolder.GetLeafName().ToString();
		ActorFolder->SetLabel(FolderLabel);
		check(ActorFolder->GetPath().IsEqual(InNewFolder.GetPath(), ENameCase::CaseSensitive));
	}
}

static bool GAllowCleanupActorFolders = true;

void FLevelActorFoldersHelper::DeleteFolder(ULevel* InLevel, const FFolder& InFolder)
{
	TGuardValue<bool> GuardValue(GAllowCleanupActorFolders, false);
	check(InLevel);
	// This implementation can be called both if FActorFolders is or isn't initialized.
	if (FActorFolders::Get().IsInitializedForWorld(*InLevel->GetWorld()))
	{
		FActorFolders::Get().DeleteFolder(*InLevel->GetWorld(), InFolder);
	}
	else
	{
		UActorFolder* ActorFolder = InFolder.GetActorFolder();
		if (::IsValid(ActorFolder))
		{
			ActorFolder->MarkAsDeleted();
		}
	}
}

#endif

/*-----------------------------------------------------------------------------
FActorFolderSet implementation.
-----------------------------------------------------------------------------*/
#if WITH_EDITOR
static bool GIsFixingActorFolders = false;
#endif

void FActorFolderSet::Add(UActorFolder* InActorFolder)
{
	check(InActorFolder);

#if WITH_EDITOR
	if (!IsEmpty() && !GIsFixingActorFolders)
	{
		const FName AddedFolderPath = InActorFolder->GetPath();
		for (const TObjectPtr<UActorFolder>& Folder : ActorFolders)
		{
			if (AddedFolderPath.IsEqual(Folder->GetPath(), ENameCase::IgnoreCase))
			{
				UE_LOG(LogLevel, Error, TEXT("Adding duplicate actor folder %s with path %s."), *InActorFolder->GetName(), *AddedFolderPath.ToString());
				break;
			}
		}
	}
#endif

	ActorFolders.Add(InActorFolder);
}

/*-----------------------------------------------------------------------------
FPendingAutoReceiveInputActor implementation.
-----------------------------------------------------------------------------*/

FPendingAutoReceiveInputActor::FPendingAutoReceiveInputActor(AActor* InActor, const int32 InPlayerIndex)
	: Actor(InActor)
	, PlayerIndex(InPlayerIndex)
{
}

FPendingAutoReceiveInputActor::~FPendingAutoReceiveInputActor() = default;

/*-----------------------------------------------------------------------------
ULevel implementation.
-----------------------------------------------------------------------------*/

/** Called when a level package has been dirtied. */
FSimpleMulticastDelegate ULevel::LevelDirtiedEvent;

int32 FPrecomputedVisibilityHandler::NextId = 0;

/** Updates visibility stats. */
void FPrecomputedVisibilityHandler::UpdateVisibilityStats(bool bAllocating) const
{
	if (bAllocating)
	{
		INC_DWORD_STAT_BY(STAT_PrecomputedVisibilityMemory, PrecomputedVisibilityCellBuckets.GetAllocatedSize());
		for (int32 BucketIndex = 0; BucketIndex < PrecomputedVisibilityCellBuckets.Num(); BucketIndex++)
		{
			INC_DWORD_STAT_BY(STAT_PrecomputedVisibilityMemory, PrecomputedVisibilityCellBuckets[BucketIndex].Cells.GetAllocatedSize());
			INC_DWORD_STAT_BY(STAT_PrecomputedVisibilityMemory, PrecomputedVisibilityCellBuckets[BucketIndex].CellDataChunks.GetAllocatedSize());
			for (int32 ChunkIndex = 0; ChunkIndex < PrecomputedVisibilityCellBuckets[BucketIndex].CellDataChunks.Num(); ChunkIndex++)
			{
				INC_DWORD_STAT_BY(STAT_PrecomputedVisibilityMemory, PrecomputedVisibilityCellBuckets[BucketIndex].CellDataChunks[ChunkIndex].Data.GetAllocatedSize());
			}
		}
	}
	else
	{ //-V523 disabling identical branch warning because PVS-Studio does not understate the stat system in all configurations:
		DEC_DWORD_STAT_BY(STAT_PrecomputedVisibilityMemory, PrecomputedVisibilityCellBuckets.GetAllocatedSize());
		for (int32 BucketIndex = 0; BucketIndex < PrecomputedVisibilityCellBuckets.Num(); BucketIndex++)
		{
			DEC_DWORD_STAT_BY(STAT_PrecomputedVisibilityMemory, PrecomputedVisibilityCellBuckets[BucketIndex].Cells.GetAllocatedSize());
			DEC_DWORD_STAT_BY(STAT_PrecomputedVisibilityMemory, PrecomputedVisibilityCellBuckets[BucketIndex].CellDataChunks.GetAllocatedSize());
			for (int32 ChunkIndex = 0; ChunkIndex < PrecomputedVisibilityCellBuckets[BucketIndex].CellDataChunks.Num(); ChunkIndex++)
			{
				DEC_DWORD_STAT_BY(STAT_PrecomputedVisibilityMemory, PrecomputedVisibilityCellBuckets[BucketIndex].CellDataChunks[ChunkIndex].Data.GetAllocatedSize());
			}
		}
	}
}

/** Sets this visibility handler to be actively used by the rendering scene. */
void FPrecomputedVisibilityHandler::UpdateScene(FSceneInterface* Scene) const
{
	if (Scene && PrecomputedVisibilityCellBuckets.Num() > 0)
	{
		Scene->SetPrecomputedVisibility(this);
	}
}

/** Invalidates the level's precomputed visibility and frees any memory used by the handler. */
void FPrecomputedVisibilityHandler::Invalidate(FSceneInterface* Scene)
{
	Scene->SetPrecomputedVisibility(NULL);
	// Block until the renderer no longer references this FPrecomputedVisibilityHandler so we can delete its data
	FlushRenderingCommands();
	UpdateVisibilityStats(false);
	PrecomputedVisibilityCellBucketOriginXY = FVector2D(0,0);
	PrecomputedVisibilityCellSizeXY = 0;
	PrecomputedVisibilityCellSizeZ = 0;
	PrecomputedVisibilityCellBucketSizeXY = 0;
	PrecomputedVisibilityNumCellBuckets = 0;
	PrecomputedVisibilityCellBuckets.Empty();
	// Bump the Id so FSceneViewState will know to discard its cached visibility data
	Id = NextId;
	NextId++;
}

void FPrecomputedVisibilityHandler::ApplyWorldOffset(const FVector& InOffset)
{
	PrecomputedVisibilityCellBucketOriginXY-= FVector2D(InOffset.X, InOffset.Y);
	for (FPrecomputedVisibilityBucket& Bucket : PrecomputedVisibilityCellBuckets)
	{
		for (FPrecomputedVisibilityCell& Cell : Bucket.Cells)
		{
			Cell.Min+= InOffset;
		}
	}
}

FArchive& operator<<( FArchive& Ar, FPrecomputedVisibilityHandler& D )
{
	Ar << D.PrecomputedVisibilityCellBucketOriginXY;
	Ar << D.PrecomputedVisibilityCellSizeXY;
	Ar << D.PrecomputedVisibilityCellSizeZ;
	Ar << D.PrecomputedVisibilityCellBucketSizeXY;
	Ar << D.PrecomputedVisibilityNumCellBuckets;
	Ar << D.PrecomputedVisibilityCellBuckets;
	if (Ar.IsLoading())
	{
		D.UpdateVisibilityStats(true);
	}
	return Ar;
}


/** Sets this volume distance field to be actively used by the rendering scene. */
void FPrecomputedVolumeDistanceField::UpdateScene(FSceneInterface* Scene) const
{
	if (Scene && Data.Num() > 0)
	{
		Scene->SetPrecomputedVolumeDistanceField(this);
	}
}

/** Invalidates the level's volume distance field and frees any memory used by it. */
void FPrecomputedVolumeDistanceField::Invalidate(FSceneInterface* Scene)
{
	if (Scene && Data.Num() > 0)
	{
		Scene->SetPrecomputedVolumeDistanceField(NULL);
		// Block until the renderer no longer references this FPrecomputedVolumeDistanceField so we can delete its data
		FlushRenderingCommands();
		Data.Empty();
	}
}

FArchive& operator<<( FArchive& Ar, FPrecomputedVolumeDistanceField& D )
{
	Ar << D.VolumeMaxDistance;
	Ar << D.VolumeBox;
	Ar << D.VolumeSizeX;
	Ar << D.VolumeSizeY;
	Ar << D.VolumeSizeZ;
	Ar << D.Data;

	return Ar;
}

FLevelSimplificationDetails::FLevelSimplificationDetails()
 : bCreatePackagePerAsset(true)
 , DetailsPercentage(70.0f)
 , bOverrideLandscapeExportLOD(false)
 , LandscapeExportLOD(7)
 , bBakeFoliageToLandscape(false)
 , bBakeGrassToLandscape(false)
{
}

bool FLevelSimplificationDetails::operator == (const FLevelSimplificationDetails& Other) const
{
	return bCreatePackagePerAsset == Other.bCreatePackagePerAsset
		&& DetailsPercentage == Other.DetailsPercentage
		&& StaticMeshMaterialSettings == Other.StaticMeshMaterialSettings
		&& bOverrideLandscapeExportLOD == Other.bOverrideLandscapeExportLOD
		&& LandscapeExportLOD == Other.LandscapeExportLOD
		&& LandscapeMaterialSettings == Other.LandscapeMaterialSettings
		&& bBakeFoliageToLandscape == Other.bBakeFoliageToLandscape
		&& bBakeGrassToLandscape == Other.bBakeGrassToLandscape;
}

static bool IsActorFolderObjectsFeatureAvailable()
{
#if WITH_EDITOR
	return !IsRunningCookCommandlet() && !IsRunningGame() && GIsEditor;
#else
	return false;
#endif
}

TMap<FName, TWeakObjectPtr<UWorld> > ULevel::StreamedLevelsOwningWorld;

#if WITH_EDITOR
const FName ULevel::LoadAllExternalObjectsTag(TEXT("LoadAllExternalObjectsTag"));
const FName ULevel::DontLoadExternalObjectsTag(TEXT("DontLoadExternalObjectsTag"));
const FName ULevel::DontLoadExternalFoldersTag(TEXT("DontLoadExternalFoldersTag"));
#endif

ULevel::ULevel( const FObjectInitializer& ObjectInitializer )
	:	UObject( ObjectInitializer )
	,	Actors()
	,	OwningWorld(NULL)
	,	TickTaskLevel(FTickTaskManagerInterface::Get().AllocateTickTaskLevel())
	,	PrecomputedLightVolume(new FPrecomputedLightVolume())
	,	PrecomputedVolumetricLightmap(new FPrecomputedVolumetricLightmap())
	,	RouteActorInitializationState(ERouteActorInitializationState::Preinitialize)
	,	RouteActorInitializationIndex(0)
{
#if WITH_EDITORONLY_DATA
	LevelColor = FLinearColor::White;
	FixupOverrideVertexColorsTimeMS = 0;
	FixupOverrideVertexColorsCount = 0;
	bUseExternalActors = false;
	bContainsStableActorGUIDs = true;
	PlayFromHereActor = nullptr;
	ActorPackagingScheme = EActorPackagingScheme::Reduced;
	bPromptWhenAddingToLevelBeforeCheckout = true;
	bPromptWhenAddingToLevelOutsideBounds = true;
	bUseActorFolders = false;
	bFixupActorFoldersAtLoad = IsActorFolderObjectsFeatureAvailable();
	bForceCantReuseUnloadedButStillAround = false;
#endif	
	bActorClusterCreated = false;
	bGarbageCollectionClusteringEnabled = true;
	bIsPartitioned = false;
	bStaticComponentsRegisteredInStreamingManager = false;
	IncrementalComponentState = EIncrementalComponentState::Init;
}

void ULevel::Initialize(const FURL& InURL)
{
	URL = InURL;
}

ULevel::~ULevel()
{
	FTickTaskManagerInterface::Get().FreeTickTaskLevel(TickTaskLevel);
	TickTaskLevel = NULL;
}

void ULevel::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	ULevel* This = CastChecked<ULevel>(InThis);

	// Let GC know that we're referencing some AActor objects
	if (FPlatformProperties::RequiresCookedData() && GActorClusteringEnabled && This->bGarbageCollectionClusteringEnabled && This->bActorClusterCreated)
	{
		Collector.AddStableReferenceArray(&This->ActorsForGC);
	}
	else
	{
		Collector.AddStableReferenceArray(&This->Actors);
	}

	Super::AddReferencedObjects( This, Collector );
}

void ULevel::CleanupLevel(bool bCleanupResources, bool bUnloadFromEditor)
{
	OnCleanupLevel.Broadcast();

	if (bCleanupResources)
	{
		if (UWorldPartition* WorldPartition = GetWorldPartition(); WorldPartition && WorldPartition->IsInitialized())
		{
			WorldPartition->Uninitialize();
		}
	}

	const bool bTrashPackage = !ULevelStreaming::ShouldReuseUnloadedButStillAroundLevels(this);
	TSet<UPackage*> ProcessedPackages;
	auto ProcessPackage = [&ProcessedPackages, bTrashPackage](UPackage* InPackage, bool bInClearStandaloneFlag = false)
	{
		bool bWasAlreadyInSet;
		ProcessedPackages.Add(InPackage, &bWasAlreadyInSet);
		if (!bWasAlreadyInSet)
		{
#if WITH_EDITOR
			if (bInClearStandaloneFlag || bTrashPackage)
			{
				// Clear RF_Standalone flag on objects in package
				ForEachObjectWithPackage(InPackage, [](UObject* Object) { Object->ClearFlags(RF_Standalone); return true; }, false);
			}
#endif
			if (bTrashPackage && (InPackage != GetTransientPackage()))
			{
				// Rename package to make sure it won't be reused
				FName NewPackageName = MakeUniqueObjectName(nullptr, UPackage::StaticClass(), FName(*FString::Printf(TEXT("%s_Trashed"), *InPackage->GetName())));
				InPackage->Rename(*NewPackageName.ToString(), nullptr, REN_ForceNoResetLoaders | REN_DontCreateRedirectors | REN_NonTransactional | REN_DoNotDirty);
			}
		}
	};

	UPackage* LevelPackage = GetPackage();
	if (bTrashPackage)
	{
		ProcessPackage(LevelPackage);
	}

#if WITH_EDITOR
	// Process objects outered to this level but in a different package (currently only possible in editor)
	check(!bUnloadFromEditor || bCleanupResources);
	if (bUnloadFromEditor || bTrashPackage)
	{
		ForEachObjectWithOuter(this, [LevelPackage, ProcessPackage](UObject* InObject)
		{
			if (UPackage* ObjectPackage = InObject->GetPackage(); ObjectPackage && LevelPackage != ObjectPackage)
			{
				ProcessPackage(ObjectPackage, true);
			}
		}, /*bIncludeNestedObjects*/ true);
	}
#endif
}

void ULevel::CleanupReferences()
{
	// Make sure MetaData is not marked as standalone as this will prevent the level from being GC'd
	if (GetOutermost()->HasMetaData())
	{
		GetOutermost()->GetMetaData()->ClearFlags(RF_Standalone);
	}
}

void ULevel::PostInitProperties()
{
	Super::PostInitProperties();

	// Initialize LevelBuildDataId to something unique, in case this is a new ULevel
	LevelBuildDataId = FGuid::NewGuid();
}

void ULevel::Serialize( FArchive& Ar )
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("ULevel::Serialize"), STAT_Level_Serialize, STATGROUP_LoadTime);

	Super::Serialize( Ar );

	Ar.UsingCustomVersion(FReleaseObjectVersion::GUID);
	Ar.UsingCustomVersion(FRenderingObjectVersion::GUID);
	Ar.UsingCustomVersion(FFortniteMainBranchObjectVersion::GUID);
	Ar.UsingCustomVersion(FUE5MainStreamObjectVersion::GUID);
	Ar.UsingCustomVersion(FUE5ReleaseStreamObjectVersion::GUID);

	if (Ar.IsLoading())
	{
		if (Ar.CustomVer(FReleaseObjectVersion::GUID) < FReleaseObjectVersion::LevelTransArrayConvertedToTArray)
		{
			TTransArray<AActor*> OldActors(this);
			Ar << OldActors;
			Actors.Reserve(OldActors.Num());
			for (AActor* Actor : OldActors)
			{
				Actors.Push(Actor);
			}
		}
		else
		{
			Ar << Actors;
		}

#if WITH_EDITORONLY_DATA
		bContainsStableActorGUIDs = Ar.CustomVer(FFortniteMainBranchObjectVersion::GUID) >= FFortniteMainBranchObjectVersion::ContainsStableActorGUIDs;
#endif
	}
	else if (Ar.IsPersistent() && (Ar.IsSaving() || Ar.IsObjectReferenceCollector()))
	{
		UPackage* LevelPackage = GetOutermost();
		TArray<AActor*> EmbeddedActors;
		EmbeddedActors.Reserve(Actors.Num());

		Algo::CopyIf(Actors, EmbeddedActors, [&](AActor* Actor)
		{
			if (!Actor)
			{
				return false;
			}

			check(Actor->GetLevel() == this);

			if (Actor->HasAnyFlags(RF_Transient))
			{
				return false;
			}

#if WITH_EDITOR
			if (IsUsingExternalActors() && Actor->IsPackageExternal())
			{
				// Some actors needs to be referenced by the level (world settings, default brush, etc...)
				if (Actor->ShouldLevelKeepRefIfExternal())
				{
					return true;
				}
				// Don't filter out external actors if duplicating the world to get the actors properly duplicated.
				else if (!(Ar.GetPortFlags() & PPF_Duplicate))
				{
					return false;
				}
				// When the world is partitioned and duplicating for PIE, we only duplicate external actors that were explicitely marked (see IsForceExternalActorLevelReferenceForPIE).
				// All the other actors will be streamed by the runtime grids.
				else if (bIsPartitioned && (Ar.GetPortFlags() & PPF_DuplicateForPIE))
				{
					return Actor->IsForceExternalActorLevelReferenceForPIE();
				}
			}
#endif
			return true;
		});

		Ar << EmbeddedActors;

#if WITH_EDITORONLY_DATA
		bContainsStableActorGUIDs = true;
#endif
	}
	else
	{
		Ar << Actors;
	}

	Ar << URL;

	Ar << Model;

	Ar << ModelComponents;

	if(!Ar.IsFilterEditorOnly() || (Ar.UEVer() < VER_UE4_EDITORONLY_BLUEPRINTS) )
	{
#if WITH_EDITORONLY_DATA
		// Skip serializing the LSBP if this is a world duplication for PIE/SIE, as it is not needed, and it causes overhead in startup times
		if( (Ar.GetPortFlags() & PPF_DuplicateForPIE ) == 0 )
		{
			Ar << LevelScriptBlueprint;
		}
		else
#endif //WITH_EDITORONLY_DATA
		{
			UObject* DummyBP = NULL;
			Ar << DummyBP;
		}
	}

	if( !Ar.IsTransacting() )
	{
		Ar << LevelScriptActor;
	}

	// Stop serializing deprecated classes with new versions
	if ( Ar.IsLoading() && Ar.CustomVer(FRenderingObjectVersion::GUID) < FRenderingObjectVersion::RemovedTextureStreamingLevelData )
	{
		// Strip for unsupported platforms
		TMap< UTexture2D*, TArray< FStreamableTextureInstance > >		Dummy0;
		TMap< UPrimitiveComponent*, TArray< FDynamicTextureInstance > >	Dummy1;
		bool Dummy2;
		Ar << Dummy0;
		Ar << Dummy1;
		Ar << Dummy2;

		//@todo legacy, useless
		if (Ar.IsLoading())
		{
			uint32 Size;
			Ar << Size;
			Ar.Seek(Ar.Tell() + Size);
		}
		else if (Ar.IsSaving())
		{
			uint32 Len = 0;
			Ar << Len;
		}

		if(Ar.UEVer() < VER_UE4_REMOVE_LEVELBODYSETUP)
		{
			UBodySetup* DummySetup;
			Ar << DummySetup;
		}

		TMap< UTexture2D*, bool > Dummy3;
		Ar << Dummy3;
	}

#if WITH_EDITOR
	if ( IsUsingExternalActors() && Ar.IsLoading() )
	{
		if (Ar.CustomVer(FUE5MainStreamObjectVersion::GUID) < FUE5MainStreamObjectVersion::FixForceExternalActorLevelReferenceDuplicates)
		{
			TSet<AActor*> DupActors;
			for (int ActorIndex = 0; ActorIndex < Actors.Num(); ++ActorIndex)
			{
				if (AActor * Actor = Actors[ActorIndex])
				{
					bool bAlreadyInSet = false;
					DupActors.Add(Actor, &bAlreadyInSet);

					if (bAlreadyInSet)
					{
						Actors[ActorIndex] = nullptr;
					}
				}
			}
		}
		
		if (Ar.CustomVer(FUE5ReleaseStreamObjectVersion::GUID) < FUE5ReleaseStreamObjectVersion::AddLevelActorPackagingScheme)
		{
			ActorPackagingScheme = EActorPackagingScheme::Original;
		}
	}
#endif

	// Mark archive and package as containing a map if we're serializing to disk.
	if( !HasAnyFlags( RF_ClassDefaultObject ) && Ar.IsPersistent() )
	{
		Ar.ThisContainsMap();
		GetOutermost()->ThisContainsMap();
	}

	// serialize the nav list
	Ar << NavListStart;
	Ar << NavListEnd;

	if (Ar.IsLoading() && Ar.CustomVer(FRenderingObjectVersion::GUID) < FRenderingObjectVersion::MapBuildDataSeparatePackage)
	{
		FPrecomputedLightVolumeData* LegacyData = new FPrecomputedLightVolumeData();
		Ar << (*LegacyData);

		FLevelLegacyMapBuildData LegacyLevelData;
		LegacyLevelData.Id = LevelBuildDataId;
		LegacyLevelData.Data = LegacyData;
		GLevelsWithLegacyBuildData.AddAnnotation(this, MoveTemp(LegacyLevelData));
	}

	Ar << PrecomputedVisibilityHandler;
	Ar << PrecomputedVolumeDistanceField;

	if (Ar.UEVer() >= VER_UE4_WORLD_LEVEL_INFO &&
		Ar.UEVer() < VER_UE4_WORLD_LEVEL_INFO_UPDATED)
	{
		FWorldTileInfo Info;
		Ar << Info;
	}

#if WITH_EDITORONLY_DATA
	if (Ar.CustomVer(FUE5ReleaseStreamObjectVersion::GUID) >= FUE5ReleaseStreamObjectVersion::AddLevelActorFolders)
	{
		if (IsUsingActorFolders())
		{
			// When archive is persistent, serialize ActorFolders array only
			// if actor folder objects are not stored in their external packages OR if we a duplicating the level
			if ((!IsUsingExternalObjects() || (Ar.GetPortFlags() & PPF_Duplicate)) && Ar.IsPersistent() && !Ar.IsCooking())
			{
				Ar << ActorFolders;
			}
		}
	}

	if (Ar.GetPortFlags() & PPF_DuplicateForPIE)
	{
		Ar << PlayFromHereActor;
		if (Ar.IsSaving())
		{
			PlayFromHereActor = nullptr;
		}
	}
#endif

	if (Ar.IsLoading())
	{
		if (Ar.GetPortFlags() & PPF_DuplicateForPIE)
		{
			bWasDuplicatedForPIE = true;
		}

		if (bWasDuplicatedForPIE || (Ar.GetPortFlags() & PPF_Duplicate))
		{
			bWasDuplicated = true;
		}
	}
}

void ULevel::CreateReplicatedDestructionInfo(AActor* const Actor)
{
	if ((Actor == nullptr) || GIsReconstructingBlueprintInstances || UE::Net::ShouldIgnoreStaticActorDestruction())
	{
		return;
	}
	
	// mimic the checks the package map will do before assigning a guid
	const bool bIsActorStatic = Actor->IsFullNameStableForNetworking() && Actor->IsSupportedForNetworking();
	const bool bActorHasRole = Actor->GetRemoteRole() != ROLE_None;
	const bool bShouldCreateDestructionInfo = bIsActorStatic && bActorHasRole
#if WITH_EDITOR
		&& !GIsReinstancing
#endif
		;

	if (bShouldCreateDestructionInfo)
	{
		FReplicatedStaticActorDestructionInfo NewInfo;
		NewInfo.PathName = Actor->GetFName();
		NewInfo.FullName = Actor->GetFullName();
		NewInfo.DestroyedPosition = Actor->GetActorLocation();
		NewInfo.ObjOuter = Actor->GetOuter();
		NewInfo.ObjClass = Actor->GetClass();

		DestroyedReplicatedStaticActors.Add(NewInfo);
	}
}

const TArray<FReplicatedStaticActorDestructionInfo>& ULevel::GetDestroyedReplicatedStaticActors() const
{
	return DestroyedReplicatedStaticActors;
}

bool ULevel::IsNetActor(const AActor* Actor)
{
	if (Actor == nullptr)
	{
		return false;
	}

	// If this is a server, use RemoteRole.
	// If this is a client, use Role.
	const ENetRole NetRole = (!Actor->IsNetMode(NM_Client)) ? Actor->GetRemoteRole() : (ENetRole)Actor->GetLocalRole();

	// This test will return true on clients for actors with ROLE_Authority, which might be counterintuitive,
	// but clients will need to consider these actors in some cases, such as if their bTearOff flag is true.
	return NetRole > ROLE_None;
}

#if WITH_EDITOR
void ULevel::AddLoadedActor(AActor* Actor, const FTransform* TransformToApply)
{
	AddLoadedActors({ Actor }, TransformToApply);
}

void ULevel::AddLoadedActors(const TArray<AActor*>& ActorList, const FTransform* TransformToApply)
{
	// Actors set used to accelerate lookups for actors already in the level's actor list. This can happen for newly added
	// actors, as spawning them will add them to the actors array, and grabbing an actor descriptor reference on them will trigger
	// loading, even if the actor already exists.
	TSet<AActor*> ActorsSet(ObjectPtrDecay(Actors));

	// Actors queue that will be added to the level, filtered for actors already in the level's actor list.
	TArray<AActor*> ActorsQueue;
	ActorsQueue.Reserve(ActorList.Num());

	TFunction<void(AActor* Actor)> QueueActor = [this, &ActorsSet, &ActorsQueue, &QueueActor](AActor* Actor)
	{
		check(IsValidChecked(Actor));
		check(Actor->GetLevel() == this);		

		if (!ActorsSet.Contains(Actor))
		{
			Actors.Add(Actor);
			ActorsForGC.Add(Actor);
			ActorsQueue.Add(Actor);

			// Handle child actors
			Actor->ForEachComponent<UChildActorComponent>(false, [this, &QueueActor](UChildActorComponent* ChildActorComponent)
			{
				if (AActor* ChildActor = ChildActorComponent->GetChildActor())
				{
					QueueActor(ChildActor);
				}
			});
		}
	};

	for (AActor* Actor : ActorList)
	{
		QueueActor(Actor);
	}

	FScopedSlowTask SlowTask(ActorsQueue.Num() * 3, LOCTEXT("RegisteringActors", "Registering actors..."));
	SlowTask.MakeDialogDelayed(1.0f);

	OnLoadedActorAddedToLevelPreEvent.Broadcast(ActorsQueue);

	// Register all components
	for (AActor* Actor : ActorsQueue)
	{
		// RegisterAllComponents can destroy child actors
		if (IsValid(Actor))
		{
			if (TransformToApply)
			{
				FLevelUtils::FApplyLevelTransformParams TransformParams(this, *TransformToApply);
				TransformParams.Actor = Actor;
				TransformParams.bDoPostEditMove = true;
				FLevelUtils::ApplyLevelTransform(TransformParams);
			}

			if (bAreComponentsCurrentlyRegistered)
			{
				Actor->RegisterAllComponents();
			}
		}

		SlowTask.EnterProgressFrame(1);
	}

	// Rerun construction scripts
	for (AActor* Actor : ActorsQueue)
	{
		// RegisterAllComponents/RerunConstructionScripts can destroy child actors
		if (IsValid(Actor))
		{
			if (bAreComponentsCurrentlyRegistered)
			{
				Actor->RerunConstructionScripts();
			}
		}

		SlowTask.EnterProgressFrame(1);
	}

	// Finalize actors
	for (AActor* Actor : ActorsQueue)
	{
		// RegisterAllComponents/RerunConstructionScripts can destroy child actors
		if (IsValid(Actor))
		{
			if (bAreComponentsCurrentlyRegistered)
			{
				GetWorld()->UpdateCullDistanceVolumes(Actor);
				Actor->MarkComponentsRenderStateDirty();
			}

			if (IsUsingActorFolders() && bFixupActorFoldersAtLoad)
			{
				Actor->FixupActorFolder();
			}

			OnLoadedActorAddedToLevelEvent.Broadcast(*Actor);
		}

		SlowTask.EnterProgressFrame(1);
	}

	TArray<AActor*> ValidActorsQueue;
	ValidActorsQueue.Reserve(ActorsQueue.Num());
	Algo::CopyIf(ActorsQueue, ValidActorsQueue, [&](AActor* Actor) { return IsValid(Actor); });

	OnLoadedActorAddedToLevelPostEvent.Broadcast(ValidActorsQueue);
}

void ULevel::RemoveLoadedActor(AActor* Actor, const FTransform* TransformToRemove)
{
	RemoveLoadedActors({ Actor }, TransformToRemove);
}

void ULevel::RemoveLoadedActors(const TArray<AActor*>& ActorList, const FTransform* TransformToRemove)
{
	// Build an actor pointer to pair of indices into Actors and ActorsForGC to accelerate removal lookups. This doesn't make much sense
	// when removing a single actor, but most of the time we are removing actors in large batches and it makes a huge difference in removal time.
	TMap<AActor*, TPair<int32, int32>> ActorsIndices;
	{
		ActorsIndices.Reserve(Actors.Num());

		int32 ActorIndex = 0;
		Algo::ForEach(Actors, [&ActorsIndices, &ActorIndex](AActor* Actor)
		{
			if (Actor)
			{
				ActorsIndices.Add(Actor, TPair<int32, int32>(INDEX_NONE, INDEX_NONE)).Key = ActorIndex;
			}
			ActorIndex++;
		});
	
		int32 ActorForGCIndex = 0;
		Algo::ForEach(ActorsForGC, [&ActorsIndices, &ActorForGCIndex](AActor* Actor)
		{
			if (Actor)
			{
				ActorsIndices.FindOrAdd(Actor, TPair<int32, int32>(INDEX_NONE, INDEX_NONE)).Value = ActorForGCIndex;
			}
			ActorForGCIndex++;
		});
	}

	// Build the actual actor list that needs removal.
	TArray<AActor*> ActorsQueue;
	ActorsQueue.Reserve(ActorList.Num());

	TFunction<void(AActor* Actor)> QueueActor = [this, &ActorsIndices, &ActorsQueue, &QueueActor](AActor* Actor)
	{
		check(IsValidChecked(Actor));
		check(Actor->GetLevel() == this);		

		// Handle child actors
		Actor->ForEachComponent<UChildActorComponent>(false, [this, &QueueActor](UChildActorComponent* ChildActorComponent)
		{
			if (AActor* ChildActor = ChildActorComponent->GetChildActor())
			{
				QueueActor(ChildActor);
			}
		});

		if (const TPair<int32, int32>* ActorIndices = ActorsIndices.Find(Actor))
		{
			if (ActorIndices->Key != INDEX_NONE)
			{
				check(Actors[ActorIndices->Key] == Actor);
				Actors[ActorIndices->Key] = nullptr;
			}

			if (ActorIndices->Value != INDEX_NONE)
			{
				check(ActorsForGC[ActorIndices->Value] == Actor);
				ActorsForGC[ActorIndices->Value] = nullptr;
			}

			ActorsQueue.Add(Actor);
		}
	};

	for (AActor* Actor : ActorList)
	{
		QueueActor(Actor);
	}

	FScopedSlowTask SlowTask(ActorsQueue.Num(), LOCTEXT("UnregisteringActors", "Unregistering actors..."));
	SlowTask.MakeDialogDelayed(1.0f);

	OnLoadedActorRemovedFromLevelPreEvent.Broadcast(ActorsQueue);

	for (AActor* Actor : ActorsQueue)
	{
		// UnregisterAllComponents can destroy child actors
		if (IsValid(Actor))
		{
			Actor->UnregisterAllComponents();
			Actor->RegisterAllActorTickFunctions(false, true);

			OnLoadedActorRemovedFromLevelEvent.Broadcast(*Actor);

			if (TransformToRemove)
			{
				const FTransform TransformToRemoveInverse = TransformToRemove->Inverse();
				FLevelUtils::FApplyLevelTransformParams TransformParams(this, TransformToRemoveInverse);
				TransformParams.Actor = Actor;
				TransformParams.bDoPostEditMove = true;
				FLevelUtils::ApplyLevelTransform(TransformParams);
			}
		}

		SlowTask.EnterProgressFrame(1);
	}

	Actors.Remove(nullptr);
	ActorsForGC.Remove(nullptr);

	TArray<AActor*> ValidActorsQueue;
	ValidActorsQueue.Reserve(ActorsQueue.Num());
	Algo::CopyIf(ActorsQueue, ValidActorsQueue, [&](AActor* Actor) { return IsValid(Actor); });

	FDeselectedActorsEvent DeselectedActorsEvent(ValidActorsQueue);
	OnLoadedActorRemovedFromLevelPostEvent.Broadcast(ValidActorsQueue);
}
#endif

void ULevel::SortActorList()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_Level_SortActorList);
	if (Actors.Num() == 0)
	{
		// No need to sort an empty list
		return;
	}
	LLM_REALLOC_SCOPE(Actors.GetData());
	UE_MEMSCOPE_PTR(Actors.GetData());

	TArray<AActor*> NewActors;
	TArray<AActor*> NewNetActors;
	NewActors.Reserve(Actors.Num());
	NewNetActors.Reserve(Actors.Num());

	if (WorldSettings)
	{
		// The WorldSettings tries to stay at index 0
		NewActors.Add(WorldSettings);

		if (OwningWorld != nullptr)
		{
			OwningWorld->AddNetworkActor(WorldSettings);
		}
	}

	// Add non-net actors to the NewActors immediately, cache off the net actors to Append after
	for (AActor* Actor : Actors)
	{
		if (IsValid(Actor) && Actor != WorldSettings)
		{
			if (IsNetActor(Actor))
			{
				NewNetActors.Add(Actor);
				if (OwningWorld != nullptr)
				{
					OwningWorld->AddNetworkActor(Actor);
				}
			}
			else
			{
				NewActors.Add(Actor);
			}
		}
	}

	NewActors.Append(MoveTemp(NewNetActors));

	// Replace with sorted list.
	Actors = ObjectPtrWrap(MoveTemp(NewActors));
}

void ULevel::PreSave(const class ITargetPlatform* TargetPlatform)
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS;
	Super::PreSave(TargetPlatform);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS;
}

void ULevel::PreSave(FObjectPreSaveContext ObjectSaveContext)
{
	Super::PreSave(ObjectSaveContext);

#if WITH_EDITOR
	if( !IsTemplate() )
	{
		// Clear out any crosslevel references
		for( int32 ActorIdx = 0; ActorIdx < Actors.Num(); ActorIdx++ )
		{
			AActor *Actor = Actors[ActorIdx];
			if( Actor != NULL )
			{
				Actor->ClearCrossLevelReferences();
			}
		}

		if (ObjectSaveContext.IsCooking())
		{
			BuildLevelTextureStreamingComponentDataFromActors(this);
		}
	}
#endif // WITH_EDITOR
}

void ULevel::PostLoad()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ULevel::PostLoad);

	Super::PostLoad();

	// Ensure that the level is pointed to the owning world.  For streamed levels, this will be the world of the P map
	// they are streamed in to which we cached when the package loading was invoked
	// We also need to do this before doing additional loading to avoid reinstantiation of BP actors
	// assigned to this level to be without a World.
	OwningWorld = ULevel::StreamedLevelsOwningWorld.FindRef(GetOutermost()->GetFName()).Get();
	if (OwningWorld == nullptr)
	{
		OwningWorld = CastChecked<UWorld>(GetOuter());
	}
	else
	{
		// This entry will not be used anymore, remove it
		ULevel::StreamedLevelsOwningWorld.Remove(GetOutermost()->GetFName());
	}

#if WITH_EDITOR
	const FLinkerLoad* Linker = GetLinker();
	check(Linker || bWasDuplicated);

	if ((IsUsingExternalActors() || IsUsingExternalObjects())
		&& OwningWorld
		&& OwningWorld->WorldType == EWorldType::Editor)
	{
		if (Linker && Linker->IsPackageRelocated())
		{
			const FString LevelPackageName = GetPackage()->GetName();
			UE_LOG(LogLevel, Error, TEXT("The level %s was moved on disk without the editor knowing it. This might cause some loading issues and some saving issue like an actor potentialy stomping the file of another actor."), *LevelPackageName);

			if (GIsEditor)
			{
				FFormatNamedArguments Args;
				Args.Add(TEXT("PackageName"), FText::FromString(LevelPackageName));
				FNotificationInfo Info(FText::Format(LOCTEXT("UnsupportedRelocatedLevel", "The level ({PackageName}) was relocated.\nLevels that use one file per actor cannot be moved on disk without using the editor or the migration tool."), Args));
				Info.ExpireDuration = 10.0f;

				FSlateNotificationManager::Get().AddNotification(Info);
			}
		}
	}

	if (IsUsingActorFolders() && IsUsingExternalObjects() && IsActorFolderObjectsFeatureAvailable())
	{
		if (!bWasDuplicated && !Linker->GetInstancingContext().HasTag(ULevel::DontLoadExternalFoldersTag))
		{
			// Load all folders for this level
			FExternalPackageHelper::LoadObjectsFromExternalPackages<UActorFolder>(this, [this](UActorFolder* LoadedFolder)
			{
				check(IsValid(LoadedFolder));
				LoadedExternalActorFolders.Add(LoadedFolder);
			});
		}
	}

	// if we use external actors, load dynamic actors here
	if (IsUsingExternalActors())
	{
		if (!bWasDuplicated &&
			(!bIsPartitioned || Linker->GetInstancingContext().HasTag(ULevel::LoadAllExternalObjectsTag)) &&
			!Linker->GetInstancingContext().HasTag(ULevel::DontLoadExternalObjectsTag))
		{
			UPackage* LevelPackage = GetPackage();
			bool bPackageForPIE = LevelPackage->HasAnyPackageFlags(PKG_PlayInEditor);
			FName PackageResourceName = LevelPackage->GetLoadedPath().GetPackageFName();
			bool bInstanced = !PackageResourceName.IsNone() && (PackageResourceName != LevelPackage->GetFName());

			// if the level is instanced, create an instancing context for remapping the actor imports
			FLinkerInstancingContext InstancingContext;
			if (bInstanced)
			{
				InstancingContext.AddPackageMapping(PackageResourceName, LevelPackage->GetFName());
			}

			TArray<FString> ActorPackageNames = GetOnDiskExternalActorPackages(/*bTryUsingPackageLoadedPath*/ true);
			TArray<FString> InstancePackageNames;
			for (const FString& ActorPackageName : ActorPackageNames)
			{
				if (bInstanced)
				{
					const FString InstancedName = GetExternalActorPackageInstanceName(LevelPackage->GetName(), ActorPackageName);
					InstancePackageNames.Add(InstancedName);

					InstancingContext.AddPackageMapping(FName(*ActorPackageName), FName(*InstancedName));
				}
			}

			TSet<AActor*> ActorsSet(ObjectPtrDecay(Actors));
			for (int32 i=0; i < ActorPackageNames.Num(); i++)
			{
				const FString& ActorPackageName = ActorPackageNames[i];

				// Propagate RF_Transient from the Level Package in case we are loading an instanced level
				UPackage* ActorPackage = bInstanced ? CreatePackage( *InstancePackageNames[i]) : nullptr;
				if (ActorPackage && LevelPackage->HasAnyFlags(RF_Transient))
				{
					ActorPackage->SetFlags(RF_Transient);
				}

				bool bFoundActor = false;
				ActorPackage = LoadPackage(ActorPackage, *ActorPackageName, bPackageForPIE ? LOAD_PackageForPIE : LOAD_None, nullptr, &InstancingContext);
				
				ForEachObjectWithPackage(ActorPackage, [this, &ActorsSet, &bFoundActor, bPackageForPIE](UObject* PackageObject)
				{
					// There might be multiple actors per package in the case where an actor as a child actor component as we put child actor in the same package as their parent
					if (PackageObject->IsA<AActor>() && !PackageObject->IsTemplate())
					{
						AActor* Actor = (AActor*)PackageObject;
						// Verity that the actor is not already in the array (this is valid if, during last save, the actor returned true in AActor::ShouldLevelKeepRefIfExternal)
						if (!ActorsSet.Contains(Actor))
						{
							Actors.Add(Actor);
						}
						bFoundActor = true;
					}
					// In PIE, we make sure to clear RF_Standalone flag on objects in external packages (UMetaData) 
					// This guarantees that external packages of actors that are destroyed during the PIE session will
					// properly get GC'ed and will allow future edits/modifications of OFPA actors.
					if (bPackageForPIE)
					{
						PackageObject->ClearFlags(RF_Standalone);
					}
					return true;
				}, false);

				UE_CLOG(!bFoundActor, LogLevel, Error, TEXT("Failed to load Actor for External Actor Package %s"), *ActorPackageName);
			}
		}
	}
#endif

	UWorldComposition::OnLevelPostLoad(this);
		
#if WITH_EDITOR
	Actors.Remove(nullptr);
#endif

	if (WorldSettings == nullptr)
	{
		WorldSettings = Cast<AWorldSettings>(Actors[0]);
	}

	// in the Editor, sort Actor list immediately (at runtime we wait for the level to be added to the world so that it can be delayed in the level streaming case)
	if (GIsEditor)
	{
		SortActorList();
	}

	// Validate navigable geometry
	if (Model == NULL || Model->NumUniqueVertices == 0)
	{
		StaticNavigableGeometry.Empty();
	}

#if WITH_EDITOR
	if (!GetOutermost()->HasAnyPackageFlags(PKG_PlayInEditor))
	{
		// Rename the LevelScriptBlueprint after the outer world.
		UWorld* OuterWorld = Cast<UWorld>(GetOuter());
		if (LevelScriptBlueprint && OuterWorld && LevelScriptBlueprint->GetFName() != OuterWorld->GetFName())
		{
			// The level blueprint must be named the same as the level/world.
			// If there is already something there with that name, rename it to something else.
			if (UObject* ExistingObject = StaticFindObject(nullptr, LevelScriptBlueprint->GetOuter(), *OuterWorld->GetName()))
			{
				ExistingObject->Rename(nullptr, nullptr, REN_DoNotDirty | REN_DontCreateRedirectors | REN_ForceNoResetLoaders | REN_NonTransactional);
			}

			// Use LevelScriptBlueprint->GetOuter() instead of NULL to make sure the generated top level objects are moved appropriately
			LevelScriptBlueprint->Rename(*OuterWorld->GetName(), LevelScriptBlueprint->GetOuter(), REN_DoNotDirty | REN_DontCreateRedirectors | REN_ForceNoResetLoaders | REN_NonTransactional | REN_SkipGeneratedClasses);
		}
	}

	RepairLevelScript();
#endif
}

bool ULevel::CanBeClusterRoot() const
{
	// We don't want to create the cluster for levels in the same place as other clusters (after PostLoad)
	// because at this point some of the assets referenced by levels may still haven't created clusters themselves.
	return false;
}

void ULevel::CreateCluster()
{
	// ULevels are not cluster roots themselves, instead they create a special actor container
	// that holds a reference to all actors that are to be clustered. This is because only
	// specific actor types can be clustered so the remaining actors that are not clustered
	// need to be referenced through the level.
	// Also, we don't want the level to reference the actors that are clusters because that would
	// make things work even slower (references to clustered objects are expensive). That's why
	// we keep a separate array for referencing unclustered actors (ActorsForGC).
	if (FPlatformProperties::RequiresCookedData() && GCreateGCClusters && GActorClusteringEnabled && bGarbageCollectionClusteringEnabled && !bActorClusterCreated)
	{
		ActorsForGC.Reset();

		TArray<AActor*> ClusterActors;

		for (int32 ActorIndex = Actors.Num() - 1; ActorIndex >= 0; --ActorIndex)
		{
			AActor* Actor = Actors[ActorIndex];
			if (Actor && Actor->CanBeInCluster())
			{
				ClusterActors.Add(Actor);
			}
			else
			{
				ActorsForGC.Add(Actor);
			}
		}
		if (ClusterActors.Num())
		{
			ActorCluster = NewObject<ULevelActorContainer>(this, TEXT("ActorCluster"), RF_Transient);
			ActorCluster->Actors = MoveTemp(ClusterActors);
			ActorCluster->CreateCluster();
		}
		bActorClusterCreated = true;
	}
}

void ULevel::PreDuplicate(FObjectDuplicationParameters& DupParams)
{
	Super::PreDuplicate(DupParams);

#if WITH_EDITOR
	if (DupParams.DuplicateMode == EDuplicateMode::PIE)
	{
		AActor::FDuplicationSeedInterface DuplicationSeedInterface(DupParams.DuplicationSeed);
		for (AActor* Actor : Actors)
		{
			if (Actor != nullptr)
			{
				Actor->PopulatePIEDuplicationSeed(DuplicationSeedInterface);
			}
		}
	}

	if (DupParams.DuplicateMode != EDuplicateMode::PIE && DupParams.bAssignExternalPackages)
	{
		UPackage* SrcPackage = GetPackage();
		UPackage* DstPackage = DupParams.DestOuter->GetPackage();

		FString ReplaceFrom = FPaths::GetBaseFilename(*SrcPackage->GetName());
		ReplaceFrom = FString::Printf(TEXT("%s.%s:"), *ReplaceFrom, *ReplaceFrom);

		FString ReplaceTo = FPaths::GetBaseFilename(*DstPackage->GetName());
		ReplaceTo = FString::Printf(TEXT("%s.%s:"), *ReplaceTo, *ReplaceTo);

		ForEachObjectWithOuter(this, [this, &SrcPackage, &DstPackage, &ReplaceFrom, &ReplaceTo, &DupParams](UObject* Object)
		{
			if (UPackage* Package = Object ? Object->GetExternalPackage() : nullptr)
			{
				FString Path = Object->GetPathName();
				if (DstPackage != SrcPackage)
				{
					Path = Path.Replace(*ReplaceFrom, *ReplaceTo);
				}
				UPackage* DupPackage = Object->IsA<AActor>() ? ULevel::CreateActorPackage(DstPackage, GetActorPackagingScheme(), Path, Object) : FExternalPackageHelper::CreateExternalPackage(DstPackage, Path);
				DupPackage->MarkAsFullyLoaded();
				DupPackage->MarkPackageDirty();
				DupParams.DuplicationSeed.Add(Package, DupPackage);
			}
		}, /*bIncludeNestedObjects*/ true);
	}
#endif
}

UWorld* ULevel::GetWorld() const
{
	return OwningWorld;
}

void ULevel::ClearLevelComponents()
{
	bAreComponentsCurrentlyRegistered = false;

	// Remove the model components from the scene.
	for (UModelComponent* ModelComponent : ModelComponents)
	{
		if (ModelComponent && ModelComponent->IsRegistered())
		{
			ModelComponent->UnregisterComponent();
		}
	}

	// Remove the actors' components from the scene and build a list of relevant worlds
	// In theory (though it is a terrible idea), users could spawn Actors from an OnUnregister event so don't use ranged-for
	for (int32 ActorIndex = 0; ActorIndex < Actors.Num(); ++ActorIndex)
	{
		AActor* Actor = Actors[ActorIndex];
		if (Actor)
		{
			Actor->UnregisterAllComponents();
		}
	}
}

void ULevel::BeginDestroy()
{
	ULevelStreaming::RemoveLevelAnnotation(this);
	
	if (!IStreamingManager::HasShutdown())
	{
		// At this time, referenced UTexture2Ds are still in memory.
		IStreamingManager::Get().RemoveLevel( this );
	}

	Super::BeginDestroy();

	// Remove this level from its OwningWorld's collection
	if (CachedLevelCollection)
	{
		CachedLevelCollection->RemoveLevel(this);
	}

	if (OwningWorld && IsPersistentLevel() && OwningWorld->Scene)
	{
		OwningWorld->Scene->SetPrecomputedVisibility(NULL);
		OwningWorld->Scene->SetPrecomputedVolumeDistanceField(NULL);
	}

	ReleaseRenderingResources();

	RemoveFromSceneFence.BeginFence();
}

bool ULevel::IsReadyForFinishDestroy()
{
	const bool bReady = Super::IsReadyForFinishDestroy();
	return bReady && RemoveFromSceneFence.IsFenceComplete();
}

void ULevel::FinishDestroy()
{
	delete PrecomputedLightVolume;
	PrecomputedLightVolume = NULL;

	delete PrecomputedVolumetricLightmap;
	PrecomputedVolumetricLightmap = NULL;

	Super::FinishDestroy();
}

/**
* A TMap key type used to sort BSP nodes by locality and zone.
*/
struct FModelComponentKey
{
	uint32	X;
	uint32	Y;
	uint32	Z;
	uint32	MaskedPolyFlags;

	friend bool operator==(const FModelComponentKey& A,const FModelComponentKey& B)
	{
		return	A.X == B.X 
			&&	A.Y == B.Y 
			&&	A.Z == B.Z 
			&&	A.MaskedPolyFlags == B.MaskedPolyFlags;
	}

	friend uint32 GetTypeHash(const FModelComponentKey& Key)
	{
		return FCrc::MemCrc_DEPRECATED(&Key,sizeof(Key));
	}
};

void ULevel::UpdateLevelComponents(bool bRerunConstructionScripts, FRegisterComponentContext* Context)
{
	// Update all components in one swoop.
	IncrementalUpdateComponents( 0, bRerunConstructionScripts, Context);
}

/**
 *	Sorts actors such that parent actors will appear before children actors in the list
 *	Stable sort
 */
static void SortActorsHierarchy(TArray<TObjectPtr<AActor>>& Actors, ULevel* Level)
{
	const double StartTime = FPlatformTime::Seconds();

	TMap<AActor*, int32> DepthMap;
	TArray<AActor*, TInlineAllocator<10>> VisitedActors;

	DepthMap.Reserve(Actors.Num());

	bool bFoundWorldSettings = false;

	// This imitates the internals of GetDefaultBrush() without the sometimes problematic checks
	ABrush* DefaultBrush = (Actors.Num() >= 2 ? Cast<ABrush>(Actors[1]) : nullptr); 
	TFunction<int32(AActor*)> CalcAttachDepth = [&DepthMap, &VisitedActors, &CalcAttachDepth, &bFoundWorldSettings, DefaultBrush](AActor* Actor)
	{
		int32 Depth = 0;
		if (int32* FoundDepth = DepthMap.Find(Actor))
		{
			Depth = *FoundDepth;
		}
		else
		{
			// WorldSettings is expected to be the first element in the sorted Actors array
			// To accomodate for the known issue where two world settings can exist, we only sort the
			// first one we find to the 0 index
			if (Actor->IsA<AWorldSettings>())
			{
				if (!bFoundWorldSettings)
				{
					Depth = TNumericLimits<int32>::Lowest();
					bFoundWorldSettings = true;
				}
				else
				{
					UE_LOG(LogLevel, Warning, TEXT("Detected duplicate WorldSettings actor - UE-62934"));
				}
			}
			// The default brush is expected to be the second element in the sorted Actors array
			else if (Actor == DefaultBrush)
			{
				Depth = TNumericLimits<int32>::Lowest() + 1;
			}
			else if (AActor* ParentActor = Actor->GetAttachParentActor())
			{
				if (VisitedActors.Contains(ParentActor))
				{
					FString VisitedActorLoop;
					for (AActor* VisitedActor : VisitedActors)
					{
						VisitedActorLoop += VisitedActor->GetName() + TEXT(" -> ");
					}
					VisitedActorLoop += Actor->GetName();

					UE_LOG(LogLevel, Warning, TEXT("Found loop in attachment hierarchy: %s"), *VisitedActorLoop);
					// Once we find a loop, depth is mostly meaningless, so we'll treat the "end" of the loop as 0
				}
				else
				{
					VisitedActors.Add(Actor);

					// Actors attached to a ChildActor have to be registered first or else
					// they will become detached due to the AttachChildren not yet being populated
					// and thus not recorded in the ComponentInstanceDataCache
					if (ParentActor->IsChildActor())
					{
						Depth = CalcAttachDepth(ParentActor) - 1;
					}
					else
					{
						Depth = CalcAttachDepth(ParentActor) + 1;
					}
				}
			}
			DepthMap.Add(Actor, Depth);
		}
		return Depth;
	};

	for (AActor* Actor : Actors)
	{
		if (Actor)
		{
			CalcAttachDepth(Actor);
			VisitedActors.Reset();
		}
	}

	const double CalcAttachDepthTime = FPlatformTime::Seconds() - StartTime;

	auto DepthSorter = [&DepthMap](AActor* A, AActor* B)
	{
		const int32 DepthA = A ? DepthMap.FindRef(A) : MAX_int32;
		const int32 DepthB = B ? DepthMap.FindRef(B) : MAX_int32;
		return DepthA < DepthB;
	};

	const double StableSortStartTime = FPlatformTime::Seconds();
	StableSortInternal(Actors.GetData(), Actors.Num(), DepthSorter);
	const double StableSortTime = FPlatformTime::Seconds() - StableSortStartTime;

	const double ElapsedTime = FPlatformTime::Seconds() - StartTime;
	if (ElapsedTime > 1.0 && !FApp::IsUnattended())
	{
		UE_LOG(LogLevel, Warning, TEXT("SortActorsHierarchy(%s) took %f seconds (CalcAttachDepth: %f StableSort: %f)"), Level ? *GetNameSafe(Level->GetOutermost()) : TEXT("??"), ElapsedTime, CalcAttachDepthTime, StableSortTime);
	}

	// Since all the null entries got sorted to the end, lop them off right now
	int32 RemoveAtIndex = Actors.Num();
	while (RemoveAtIndex > 0 && Actors[RemoveAtIndex - 1] == nullptr)
	{
		--RemoveAtIndex;
	}

	if (RemoveAtIndex < Actors.Num())
	{
		Actors.RemoveAt(RemoveAtIndex, Actors.Num() - RemoveAtIndex);
	}
}

DECLARE_CYCLE_STAT(TEXT("Deferred Init Bodies"), STAT_DeferredUpdateBodies, STATGROUP_Physics);
void ULevel::IncrementalUpdateComponents(int32 NumComponentsToUpdate, bool bRerunConstructionScripts, FRegisterComponentContext* Context)
{
#if !WITH_EDITOR
	ensure(!bRerunConstructionScripts);
#endif

	TRACE_CPUPROFILER_EVENT_SCOPE(ULevel::IncrementalUpdateComponents);

	// A value of 0 means that we want to update all components.
	if (NumComponentsToUpdate != 0)
	{
		// Only the game can use incremental update functionality.
		checkf(OwningWorld->IsGameWorld(), TEXT("Cannot call IncrementalUpdateComponents with non 0 argument in the Editor/ commandlets."));
	}

	bool bFullyUpdateComponents = NumComponentsToUpdate == 0;

	// The editor is never allowed to incrementally update components.  Make sure to pass in a value of zero for NumActorsToUpdate.
	check(bFullyUpdateComponents || OwningWorld->IsGameWorld());

	do
	{
		switch (IncrementalComponentState)
		{
		case EIncrementalComponentState::Init:
			// Do BSP on the first pass.
			UpdateModelComponents();
			// Sort actors to ensure that parent actors will be registered before child actors
			SortActorsHierarchy(Actors, this);
			IncrementalComponentState = EIncrementalComponentState::RegisterInitialComponents;

			//pass through
		case EIncrementalComponentState::RegisterInitialComponents:
			if (IncrementalRegisterComponents(true, NumComponentsToUpdate, Context))
			{
#if WITH_EDITOR
				const bool bShouldRunConstructionScripts = !bHasRerunConstructionScripts && bRerunConstructionScripts && !IsTemplate();
				IncrementalComponentState = bShouldRunConstructionScripts ? EIncrementalComponentState::RunConstructionScripts : EIncrementalComponentState::Finalize;
#else
				IncrementalComponentState = EIncrementalComponentState::Finalize;
#endif
			}
			break;

#if WITH_EDITOR
		case EIncrementalComponentState::RunConstructionScripts:
			if (IncrementalRunConstructionScripts(bFullyUpdateComponents))
			{
				IncrementalComponentState = EIncrementalComponentState::Finalize;
			}
			break;
#endif

		case EIncrementalComponentState::Finalize:
			IncrementalComponentState = EIncrementalComponentState::Init;
			CurrentActorIndexForIncrementalUpdate = 0;
			bHasCurrentActorCalledPreRegister = false;
			bAreComponentsCurrentlyRegistered = true;
			CreateCluster();
			break;
		}
	}while(bFullyUpdateComponents && !bAreComponentsCurrentlyRegistered);
	

	{
		SCOPE_CYCLE_COUNTER(STAT_DeferredUpdateBodies);

		FPhysScene* PhysScene = OwningWorld->GetPhysicsScene();
		if (PhysScene)
		{
			PhysScene->ProcessDeferredCreatePhysicsState();
		}
	}
}

bool ULevel::IncrementalRegisterComponents(bool bPreRegisterComponents, int32 NumComponentsToUpdate, FRegisterComponentContext* Context)
{
	// Find next valid actor to process components registration

	if (OwningWorld)
	{
		OwningWorld->SetAllowDeferredPhysicsStateCreation(true);
	}

	while (CurrentActorIndexForIncrementalUpdate < Actors.Num())
	{
		AActor* Actor = Actors[CurrentActorIndexForIncrementalUpdate];
		bool bAllComponentsRegistered = true;
		if (IsValid(Actor))
		{
			if (!Actor->HasActorRegisteredAllComponents() || GOptimizeActorRegistration == 0)
			{
#if PERF_TRACK_DETAILED_ASYNC_STATS
				FScopeCycleCounterUObject ContextScope(Actor);
#endif
				if (bPreRegisterComponents && !bHasCurrentActorCalledPreRegister)
				{
					Actor->PreRegisterAllComponents();
					bHasCurrentActorCalledPreRegister = true;
				}
				bAllComponentsRegistered = Actor->IncrementalRegisterComponents(NumComponentsToUpdate, Context);
			}
#if !UE_BUILD_SHIPPING
			else if (GOptimizeActorRegistration == 2)
			{
				// Verify that there aren't any leftover components
				Actor->ForEachComponent(false, [](UActorComponent* Component)
				{
					ensureMsgf(Component->IsRegistered() || !Component->bAutoRegister, TEXT("Component %s should be registered!"), *Component->GetPathName());
				});
			}
#endif
		}

		if (bAllComponentsRegistered)
		{
			// All components have been registered for this actor, move to a next one
			CurrentActorIndexForIncrementalUpdate++;
			bHasCurrentActorCalledPreRegister = false;
		}

		// If we do an incremental registration return to outer loop after each processed actor 
		// so outer loop can decide whether we want to continue processing this frame
		if (NumComponentsToUpdate != 0)
		{
			break;
		}
	}

	if (CurrentActorIndexForIncrementalUpdate >= Actors.Num())
	{
		// We need to process pending adds prior to rerunning the construction scripts, which may internally
		// perform removals / adds themselves.
		if (Context)
		{
			Context->Process();
		}
		CurrentActorIndexForIncrementalUpdate = 0;
		return true;
	}

	return false;
}

#if WITH_EDITOR
bool ULevel::IncrementalRunConstructionScripts(bool bProcessAllActors)
{
	// Find next valid actor to process components registration
	while (CurrentActorIndexForIncrementalUpdate < Actors.Num())
	{
		AActor* Actor = Actors[CurrentActorIndexForIncrementalUpdate];
		CurrentActorIndexForIncrementalUpdate++;

		if (Actor == nullptr || !IsValidChecked(Actor) || Actor->IsChildActor())
		{
			continue;
		}

		// Child actors have already been built and initialized up by their parent and they should not be reconstructed again
		if (Actor->IsChildActor())
		{
			continue;
		}

#if WITH_EDITOR
		if (DeferRunningConstructionScripts(Actor))
		{
			continue;
		}
#endif

#if PERF_TRACK_DETAILED_ASYNC_STATS
		FScopeCycleCounterUObject ContextScope(Actor);
#endif
		
		// TODO - Ideally we could pull out component registration from this call, allowing for even better distribution of the 
		// expensive RegisterComponentWithWorld calls, but there are a few systems that would need to be re-factored in order to do this
		// safely.
		Actor->RerunConstructionScripts();

		// If we do an incremental registration return to outer loop after each processed actor 
		// so outer loop can decide whether we want to continue processing this frame
		if (!bProcessAllActors)
		{
			break;
		}
	}

	if (OwningWorld)
	{
		OwningWorld->SetAllowDeferredPhysicsStateCreation(false);
	}

	if (CurrentActorIndexForIncrementalUpdate >= Actors.Num())
	{
		CurrentActorIndexForIncrementalUpdate = 0;
		bCachedHasStaticMeshCompilationPending.Reset();
		bHasRerunConstructionScripts = true;
		return true;
	}
	return false;
}
#endif

bool ULevel::IncrementalUnregisterComponents(int32 NumComponentsToUnregister)
{
	// A value of 0 means that we want to unregister all components.
	if (NumComponentsToUnregister != 0)
	{
		// Only the game can use incremental update functionality.
		checkf(OwningWorld->IsGameWorld(), TEXT("Cannot call IncrementalUnregisterComponents with non 0 argument in the Editor/ commandlets."));
	}

	// Find next valid actor to process components unregistration
	int32 NumComponentsUnregistered = 0;
	while (CurrentActorIndexForUnregisterComponents < Actors.Num())
	{
		AActor* Actor = Actors[CurrentActorIndexForUnregisterComponents];
		if (Actor)
		{
			int32 NumComponents = Actor->GetComponents().Num();
			NumComponentsUnregistered += NumComponents;
			Actor->UnregisterAllComponents();
		}
		CurrentActorIndexForUnregisterComponents++;
		if (NumComponentsUnregistered > NumComponentsToUnregister )
		{
			break;
		}
	}

	if (CurrentActorIndexForUnregisterComponents >= Actors.Num())
	{
		CurrentActorIndexForUnregisterComponents = 0;
		return true;
	}
	return false;
}

void ULevel::MarkLevelComponentsRenderStateDirty()
{
	for (UModelComponent* ModelComponent : ModelComponents)
	{
		if (ModelComponent)
		{
			ModelComponent->MarkRenderStateDirty();
		}
	}

	for (AActor* Actor : Actors)
	{
		if (Actor)
		{
			Actor->MarkComponentsRenderStateDirty();
		}
	}
}

#if WITH_EDITOR

bool ULevel::HasStaticMeshCompilationPending()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ULevel::HasStaticMeshCompilationPending);

	bool bHasStaticMeshCompilationPending = false;
	ForEachObjectWithOuterBreakable(
		this,
		[&bHasStaticMeshCompilationPending](UObject* InObject)
		{
			if (UStaticMeshComponent* Component = Cast<UStaticMeshComponent>(InObject))
			{
				if (Component->IsCompiling())
				{
					bHasStaticMeshCompilationPending = false;
					return false;
				}
			}

			return true;
		}
	);

	return bHasStaticMeshCompilationPending;
}

bool ULevel::DeferRunningConstructionScripts(AActor* InActor)
{
	// if there are outstanding asset (static meshes) compilation when loading actors
	// Defer the running of non trivial construction scripts
	// This is done to ensure that user construction scripts that runs line trace
	// Would not get an improper result from inflight static mesh compilation for example.
	if (FStaticMeshCompilingManager::Get().GetNumRemainingMeshes() &&
		InActor->HasNonTrivialUserConstructionScript())
	{
		if (!bCachedHasStaticMeshCompilationPending.IsSet())
		{
			bCachedHasStaticMeshCompilationPending = HasStaticMeshCompilationPending();
		}

		if (bCachedHasStaticMeshCompilationPending)
		{
			FActorDeferredScriptManager::Get().AddActor(InActor);
			return true;
		}
	}
	return false;
}

void ULevel::CreateModelComponents()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ULevel::CreateModelComponents)

	FScopedSlowTask SlowTask(10);
	SlowTask.MakeDialogDelayed(3.0f);

	SlowTask.EnterProgressFrame(4);

	Model->InvalidSurfaces = false;
	
	// It is possible that the BSP model has existing buffers from an undo/redo operation
	if (Model->MaterialIndexBuffers.Num())
	{
		// Make sure model resources are released which only happens on the rendering thread
		FlushRenderingCommands();

		// Clear the model index buffers.
		Model->MaterialIndexBuffers.Empty();
	}

	struct FNodeIndices
	{
		TArray<uint16> Nodes;
		TSet<uint16> UniqueNodes;

		FNodeIndices()
		{
			Nodes.Reserve(16);
			UniqueNodes.Reserve(16);
		}

		void AddUnique(uint16 Index)
		{
			if (!UniqueNodes.Contains(Index))
			{
				Nodes.Add(Index);
				UniqueNodes.Add(Index);
			}
		}
	};

	TMap< FModelComponentKey, FNodeIndices > ModelComponentMap;

	{
		FScopedSlowTask InnerTask(Model->Nodes.Num());
		InnerTask.MakeDialogDelayed(3.0f);

		// Sort the nodes by zone, grid cell and masked poly flags.
		for (int32 NodeIndex = 0; NodeIndex < Model->Nodes.Num(); NodeIndex++)
		{
			InnerTask.EnterProgressFrame(1);

			FBspNode& Node = Model->Nodes[NodeIndex];
			FBspSurf& Surf = Model->Surfs[Node.iSurf];

			if (Node.NumVertices > 0)
			{
				// Calculate the bounding box of this node.
				FBox NodeBounds(ForceInit);
				for (int32 VertexIndex = 0; VertexIndex < Node.NumVertices; VertexIndex++)
				{
					NodeBounds += (FVector)Model->Points[Model->Verts[Node.iVertPool + VertexIndex].pVertex];
				}

				// Create a sort key for this node using the grid cell containing the center of the node's bounding box.
#define MODEL_GRID_SIZE_XY	2048.0f
#define MODEL_GRID_SIZE_Z	4096.0f
				FModelComponentKey Key;
				check(OwningWorld);
				if (OwningWorld->GetWorldSettings()->bMinimizeBSPSections)
				{
					Key.X = 0;
					Key.Y = 0;
					Key.Z = 0;
				}
				else
				{
					Key.X = FMath::FloorToInt(NodeBounds.GetCenter().X / MODEL_GRID_SIZE_XY);
					Key.Y = FMath::FloorToInt(NodeBounds.GetCenter().Y / MODEL_GRID_SIZE_XY);
					Key.Z = FMath::FloorToInt(NodeBounds.GetCenter().Z / MODEL_GRID_SIZE_Z);
				}

				Key.MaskedPolyFlags = 0;

				// Find an existing node list for the grid cell.
				FNodeIndices* ComponentNodes = ModelComponentMap.Find(Key);
				if (!ComponentNodes)
				{
					// This is the first node we found in this grid cell, create a new node list for the grid cell.
					ComponentNodes = &ModelComponentMap.Add(Key);
				}

				// Add the node to the grid cell's node list.
				ComponentNodes->AddUnique(NodeIndex);
			}
			else
			{
				// Put it in component 0 until a rebuild occurs.
				Node.ComponentIndex = 0;
			}
		}
	}

	// Create a UModelComponent for each grid cell's node list.
	for (TMap< FModelComponentKey, FNodeIndices >::TConstIterator It(ModelComponentMap); It; ++It)
	{
		const FModelComponentKey&		Key		= It.Key();
		const TArray<uint16>&			Nodes	= It.Value().Nodes;

		for(int32 NodeIndex = 0;NodeIndex < Nodes.Num();NodeIndex++)
		{
			Model->Nodes[Nodes[NodeIndex]].ComponentIndex = ModelComponents.Num();							
			Model->Nodes[Nodes[NodeIndex]].ComponentNodeIndex = NodeIndex;
		}

		UModelComponent* ModelComponent = NewObject<UModelComponent>(this);
		ModelComponent->InitializeModelComponent(Model, ModelComponents.Num(), Key.MaskedPolyFlags, Nodes);
		ModelComponents.Add(ModelComponent);

		for(int32 NodeIndex = 0;NodeIndex < Nodes.Num();NodeIndex++)
		{
			Model->Nodes[Nodes[NodeIndex]].ComponentElementIndex = INDEX_NONE;

			const uint16								Node	 = Nodes[NodeIndex];
			const TIndirectArray<FModelElement>&	Elements = ModelComponent->GetElements();
			for( int32 ElementIndex=0; ElementIndex<Elements.Num(); ElementIndex++ )
			{
				if( Elements[ElementIndex].Nodes.Find( Node ) != INDEX_NONE )
				{
					Model->Nodes[Nodes[NodeIndex]].ComponentElementIndex = ElementIndex;
					break;
				}
			}
		}
	}

	// Clear old cached data in case we don't regenerate it below, e.g. after removing all BSP from a level.
	Model->NumIncompleteNodeGroups = 0;
	Model->CachedMappings.Empty();

	SlowTask.EnterProgressFrame(4);

	// Work only needed if we actually have BSP in the level.
	if( ModelComponents.Num() )
	{
		check( OwningWorld );
		// Build the static lighting vertices!
		/** The lights in the world which the system is building. */
		TArray<ULightComponentBase*> Lights;
		// Prepare lights for rebuild.
		for(TObjectIterator<ULightComponent> LightIt;LightIt;++LightIt)
		{
			ULightComponent* const Light = *LightIt;
			const bool bLightIsInWorld = IsValid(Light->GetOwner()) && OwningWorld->ContainsActor(Light->GetOwner());
			if (bLightIsInWorld && (Light->HasStaticLighting() || Light->HasStaticShadowing()))
			{
				// Make sure the light GUIDs and volumes are up-to-date.
				Light->ValidateLightGUIDs();

				// Add the light to the system's list of lights in the world.
				Lights.Add(Light);
			}
		}

		// For BSP, we aren't Component-centric, so we can't use the GetStaticLightingInfo 
		// function effectively. Instead, we look across all nodes in the Level's model and
		// generate NodeGroups - which are groups of nodes that are coplanar, adjacent, and 
		// have the same lightmap resolution (henceforth known as being "conodes"). Each 
		// NodeGroup will get a mapping created for it

		// create all NodeGroups
		Model->GroupAllNodes(this, Lights);

		// now we need to make the mappings/meshes
		for (TMap<int32, FNodeGroup*>::TIterator It(Model->NodeGroups); It; ++It)
		{
			FNodeGroup* NodeGroup = It.Value();

			if (NodeGroup->Nodes.Num())
			{
				// get one of the surfaces/components from the NodeGroup
				// @todo: Remove need for GetSurfaceLightMapResolution to take a surfaceindex, or a ModelComponent :)
				UModelComponent* SomeModelComponent = ModelComponents[Model->Nodes[NodeGroup->Nodes[0]].ComponentIndex];
				int32 SurfaceIndex = Model->Nodes[NodeGroup->Nodes[0]].iSurf;

				// fill out the NodeGroup/mapping, as UModelComponent::GetStaticLightingInfo did
				SomeModelComponent->GetSurfaceLightMapResolution(SurfaceIndex, true, NodeGroup->SizeX, NodeGroup->SizeY, NodeGroup->WorldToMap, &NodeGroup->Nodes);
				NodeGroup->MapToWorld = NodeGroup->WorldToMap.InverseFast();

				// Cache the surface's vertices and triangles.
				NodeGroup->BoundingBox.Init();

				for(int32 NodeIndex = 0;NodeIndex < NodeGroup->Nodes.Num();NodeIndex++)
				{
					const FBspNode& Node = Model->Nodes[NodeGroup->Nodes[NodeIndex]];
					const FBspSurf& NodeSurf = Model->Surfs[Node.iSurf];
					const FVector& TextureBase = (FVector)Model->Points[NodeSurf.pBase];
					const FVector& TextureX = (FVector)Model->Vectors[NodeSurf.vTextureU];
					const FVector& TextureY = (FVector)Model->Vectors[NodeSurf.vTextureV];
					const int32 BaseVertexIndex = NodeGroup->Vertices.Num();
					// Compute the surface's tangent basis.
					FVector NodeTangentX = (FVector)Model->Vectors[NodeSurf.vTextureU].GetSafeNormal();
					FVector NodeTangentY = (FVector)Model->Vectors[NodeSurf.vTextureV].GetSafeNormal();
					FVector NodeTangentZ = (FVector)Model->Vectors[NodeSurf.vNormal].GetSafeNormal();

					// Generate the node's vertices.
					for(uint32 VertexIndex = 0;VertexIndex < Node.NumVertices;VertexIndex++)
					{
						/*const*/ FVert& Vert = Model->Verts[Node.iVertPool + VertexIndex];
						const FVector VertexWorldPosition = (FVector)Model->Points[Vert.pVertex];

						FStaticLightingVertex* DestVertex = new(NodeGroup->Vertices) FStaticLightingVertex;
						DestVertex->WorldPosition = VertexWorldPosition;
						DestVertex->TextureCoordinates[0].X = ((VertexWorldPosition - TextureBase) | TextureX) / 128.0f;
						DestVertex->TextureCoordinates[0].Y = ((VertexWorldPosition - TextureBase) | TextureY) / 128.0f;
						DestVertex->TextureCoordinates[1].X = NodeGroup->WorldToMap.TransformPosition(VertexWorldPosition).X;
						DestVertex->TextureCoordinates[1].Y = NodeGroup->WorldToMap.TransformPosition(VertexWorldPosition).Y;
						DestVertex->WorldTangentX = NodeTangentX;
						DestVertex->WorldTangentY = NodeTangentY;
						DestVertex->WorldTangentZ = NodeTangentZ;

						// TEMP - Will be overridden when lighting is build!
						Vert.ShadowTexCoord = FVector2f(DestVertex->TextureCoordinates[1]);

						// Include the vertex in the surface's bounding box.
						NodeGroup->BoundingBox += VertexWorldPosition;
					}

					// Generate the node's vertex indices.
					for(uint32 VertexIndex = 2;VertexIndex < Node.NumVertices;VertexIndex++)
					{
						NodeGroup->TriangleVertexIndices.Add(BaseVertexIndex + 0);
						NodeGroup->TriangleVertexIndices.Add(BaseVertexIndex + VertexIndex);
						NodeGroup->TriangleVertexIndices.Add(BaseVertexIndex + VertexIndex - 1);

						// track the source surface for each triangle
						NodeGroup->TriangleSurfaceMap.Add(Node.iSurf);
					}
				}
			}
		}
	}
	Model->UpdateVertices();

	SlowTask.EnterProgressFrame(2);

	for (int32 UpdateCompIdx = 0; UpdateCompIdx < ModelComponents.Num(); UpdateCompIdx++)
	{
		UModelComponent* ModelComp = ModelComponents[UpdateCompIdx];
		ModelComp->GenerateElements(true);
		ModelComp->InvalidateCollisionData();
	}
}

void ULevel::InitializeTextureStreamingContainer(uint32 InPackedTextureStreamingQualityLevelFeatureLevel)
{
	PackedTextureStreamingQualityLevelFeatureLevel = InPackedTextureStreamingQualityLevelFeatureLevel;
	bTextureStreamingRotationChanged = false;
	StreamingTextureGuids.Empty();
	StreamingTextures.Empty();
	TextureStreamingResourceGuids.Empty();
	NumTextureStreamingDirtyResources = 0; // This is persistent in order to be able to notify if a rebuild is required when running a cooked build.
}

uint16 ULevel::RegisterStreamableTexture(UTexture* InTexture)
{
	if (InTexture->LevelIndex == INDEX_NONE)
	{
		// If this is the first time this texture gets processed in the packing process, encode it.
		int32 ExistingTextureIndex = StreamingTextureGuids.Find(InTexture->GetLightingGuid());
		if (ExistingTextureIndex != INDEX_NONE)
		{
			check(StreamingTextures.IsValidIndex(ExistingTextureIndex));
			// Detect that another texture with the same guid was registered
			int32 Index = StreamingTextures.Find(*InTexture->GetPathName());
			check(Index == INDEX_NONE);
			UE_LOG(LogLevel, Warning, TEXT("Another streamable texture %s was already registered with the same guid. Texture %s needs to be resaved."), *StreamingTextures[ExistingTextureIndex].ToString(), *InTexture->GetPathName());
			InTexture->Modify();
			InTexture->SetLightingGuid();
		}

		uint16 RegisteredIndex = RegisterStreamableTexture(InTexture->GetPathName(), InTexture->GetLightingGuid());
		check(RegisteredIndex != InvalidRegisteredStreamableTexture);
		InTexture->LevelIndex = (int32)RegisteredIndex;
	}
	check(StreamingTextureGuids.IsValidIndex(InTexture->LevelIndex));
	check(StreamingTextureGuids[InTexture->LevelIndex] == InTexture->GetLightingGuid());
	return (uint16)InTexture->LevelIndex;
}

uint16 ULevel::RegisterStreamableTexture(const FString& InTextureName, const FGuid& InTextureGuid)
{
	check(StreamingTextures.Num() == StreamingTextureGuids.Num());
	const int32 TextureNameIndex = StreamingTextures.Find(FName(InTextureName));
	const int32 TextureGuidIndex = StreamingTextureGuids.Find(InTextureGuid);
	if (TextureNameIndex != TextureGuidIndex)
	{
		UE_CLOG(TextureNameIndex != INDEX_NONE, LogLevel, Warning, TEXT("Failed to register streamable texture Name = %s Guid = %s: An entry already exists for this Name with a different Guid = %s. Consider rebuilding texture streaming."), *InTextureName, *InTextureGuid.ToString(), *StreamingTextureGuids[TextureNameIndex].ToString());
		UE_CLOG(TextureGuidIndex != INDEX_NONE, LogLevel, Warning, TEXT("Failed to register streamable texture Name = %s Guid = %s: An entry already exists for this Guid with a different Name = %s. Consider modifying & resaving one of these textures (will change its guid) and rebuiling texture streaming."), *InTextureName, *InTextureGuid.ToString(), *StreamingTextures[TextureGuidIndex].ToString());
		return InvalidRegisteredStreamableTexture;
	}
	else if (TextureNameIndex != INDEX_NONE)
	{
		return TextureNameIndex;
	}
	else
	{
		uint16 Index = StreamingTextureGuids.Add(InTextureGuid);
		StreamingTextures.Add(FName(*InTextureName));
		check(StreamingTextures.Num() == StreamingTextureGuids.Num());
		return Index;
	}
}

#endif

void ULevel::UpdateModelComponents()
{
	// Create/update the level's BSP model components.
	if(!ModelComponents.Num())
	{
#if WITH_EDITOR
		CreateModelComponents();
#endif // WITH_EDITOR
	}
	else
	{
		for(int32 ComponentIndex = 0;ComponentIndex < ModelComponents.Num();ComponentIndex++)
		{
			if(ModelComponents[ComponentIndex] && ModelComponents[ComponentIndex]->IsRegistered())
			{
				ModelComponents[ComponentIndex]->UnregisterComponent();
			}
		}
	}

	// Initialize the model's index buffers.
	for(TMap<UMaterialInterface*,TUniquePtr<FRawIndexBuffer16or32> >::TIterator IndexBufferIt(Model->MaterialIndexBuffers);
		IndexBufferIt;
		++IndexBufferIt)
	{
		BeginInitResource(IndexBufferIt->Value.Get());
	}

	if (ModelComponents.Num() > 0)
	{
		check( OwningWorld );
		// Update model components.
		for(int32 ComponentIndex = 0;ComponentIndex < ModelComponents.Num();ComponentIndex++)
		{
			if(ModelComponents[ComponentIndex])
			{
				ModelComponents[ComponentIndex]->RegisterComponentWithWorld(OwningWorld);
			}
		}
	}

	Model->bInvalidForStaticLighting = true;
}

#if WITH_EDITOR
void ULevel::PreEditUndo()
{
	// if we are using external actors do not call into the parent `PreEditUndo` which in the end just calls Modify and dirties the level, which we want to avoid
	// Unfortunately we cannot determine here if the properties modified through the undo are actually related to external actors...
	if (!IsUsingExternalActors())
	{
		Super::PreEditUndo();
		// Since package don't record their package flag in transaction, sync the level package dynamic import flag
		GetPackage()->ClearPackageFlags(PKG_DynamicImports);
	}
	else
	{
		// Since package don't record their package flag in transaction, sync the level package dynamic import flag
		GetPackage()->SetPackageFlags(PKG_DynamicImports);
	}

	// Detach existing model components.  These are left in the array, so they are saved for undoing the undo.
	for(int32 ComponentIndex = 0;ComponentIndex < ModelComponents.Num();ComponentIndex++)
	{
		if(ModelComponents[ComponentIndex])
		{
			ModelComponents[ComponentIndex]->UnregisterComponent();
		}
	}

	// Release the model's resources.
	Model->BeginReleaseResources();
	Model->ReleaseResourcesFence.Wait();

	ReleaseRenderingResources();

	// Wait for the components to be detached.
	FlushRenderingCommands();

	ABrush::GGeometryRebuildCause = TEXT("Undo");
}


void ULevel::PostEditUndo()
{
	Super::PostEditUndo();

	Model->UpdateVertices();
	// Update model components that were detached earlier
	UpdateModelComponents();

	ABrush::GGeometryRebuildCause = nullptr;

	// If it's a streaming level and was not visible, don't init rendering resources
	if (OwningWorld)
	{
		bool bIsStreamingLevelVisible = false;
		if (OwningWorld->PersistentLevel == this)
		{
			bIsStreamingLevelVisible = FLevelUtils::IsLevelVisible(OwningWorld->PersistentLevel);
		}
		else
		{
			if(const ULevelStreaming* StreamingLevel = ULevelStreaming::FindStreamingLevel(this))
			{
				bIsStreamingLevelVisible = FLevelUtils::IsStreamingLevelVisibleInEditor(StreamingLevel);
			}
		}

		if (bIsStreamingLevelVisible)
		{
			InitializeRenderingResources();

			// Hack: FScene::AddPrecomputedVolumetricLightmap does not cause static draw lists to be updated - force an update so the correct base pass shader is selected in ProcessBasePassMesh.  
			// With the normal load order, the level rendering resources are always initialized before the components that are in the level, so this is not an issue. 
			// During undo, PostEditUndo on the component and ULevel are called in an arbitrary order.
			MarkLevelComponentsRenderStateDirty();
		}
	}

	// Non-transactional actors may disappear from the actors list but still exist, so we need to re-add them
	// Likewise they won't get recreated if we undo to before they were deleted, so we'll have nulls in the actors list to remove
	TSet<AActor*> ActorsSet(ObjectPtrDecay(Actors));
	ForEachObjectWithOuter(this, [&ActorsSet, this](UObject* InnerObject)
	{
		AActor* InnerActor = Cast<AActor>(InnerObject);
		if (InnerActor && !ActorsSet.Contains(InnerActor))
		{
			Actors.Add(InnerActor);
		}
	}, /*bIncludeNestedObjects*/ false, /*ExclusionFlags*/ RF_NoFlags, /* InternalExclusionFlags */ EInternalObjectFlags::Garbage);

	MarkLevelBoundsDirty();
}

void ULevel::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FProperty* PropertyThatChanged = PropertyChangedEvent.MemberProperty;
	const FString PropertyName = PropertyThatChanged ? PropertyThatChanged->GetName() : TEXT("");

	if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(ULevel, MapBuildData))
	{
		// MapBuildData is not editable but can be modified by the editor's Force Delete
		ReleaseRenderingResources();
		InitializeRenderingResources();
	}

	for (UAssetUserData* Datum : AssetUserData)
	{
		if (Datum != nullptr)
		{
			Datum->PostEditChangeOwner();
		}
	}
} 

#endif // WITH_EDITOR

void ULevel::MarkLevelBoundsDirty()
{
#if WITH_EDITOR
	if (LevelBoundsActor.IsValid())
	{
		LevelBoundsActor->MarkLevelBoundsDirty();
	}
#endif// WITH_EDITOR
}

#if WITH_EDITOR
namespace LevelAssetRegistryHelper
{
	static bool GetLevelInfoFromAssetRegistry(FName LevelPackage, TFunctionRef<bool(const FAssetData & Asset)> Func)
	{
		IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

		// Always ask for scan in case no initial scan was done (Commandlets, IsRunningGame) or if AssetRegistry isn't done scanning
		AssetRegistry.ScanFilesSynchronous({ LevelPackage.ToString()});
		
		TArray<FAssetData> LevelPackageAssets;
		AssetRegistry.GetAssetsByPackageName(LevelPackage, LevelPackageAssets, true);

		for (const FAssetData& Asset : LevelPackageAssets)
		{
			static const FTopLevelAssetPath WorldClassPathName(TEXT("/Script/Engine"), TEXT("World"));
			if (Asset.AssetClassPath == WorldClassPathName)
			{
				return Func(Asset);
			}
		}

		return false;
	}

	static void ScanLevelAssets(const FString& LevelPackage)
	{
		IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		AssetRegistry.ScanModifiedAssetFiles({ LevelPackage });

		if (ULevel::GetIsLevelUsingExternalActorsFromPackage(FName(*LevelPackage)))
		{
			AssetRegistry.ScanPathsSynchronous( ULevel::GetExternalObjectsPaths(LevelPackage), /*bForceRescan=*/true);
		}
	}
}

bool ULevel::GetIsLevelPartitionedFromAsset(const FAssetData& Asset)
{
	FString LevelIsPartitionedStr;
	static const FName NAME_LevelIsPartitioned(TEXT("LevelIsPartitioned"));
	if (Asset.GetTagValue(NAME_LevelIsPartitioned, LevelIsPartitionedStr))
	{
		check(LevelIsPartitionedStr == TEXT("1"));
		return true;
	}
	return false;
}

bool ULevel::GetIsLevelPartitionedFromPackage(FName LevelPackage)
{
	return LevelPackage.IsNone() ? false : LevelAssetRegistryHelper::GetLevelInfoFromAssetRegistry(LevelPackage, [](const FAssetData& Asset)
	{
		return GetIsLevelPartitionedFromAsset(Asset);
	});
}

bool ULevel::GetIsLevelUsingExternalActorsFromAsset(const FAssetData& Asset)
{
	FString LevelIsUsingExternalActorsStr;
	static const FName NAME_LevelIsUsingExternalActors(TEXT("LevelIsUsingExternalActors"));
	if (Asset.GetTagValue(NAME_LevelIsUsingExternalActors, LevelIsUsingExternalActorsStr))
	{
		check(LevelIsUsingExternalActorsStr == TEXT("1"));
		return true;
	}
	return false;
}

bool ULevel::GetIsLevelUsingExternalActorsFromPackage(FName LevelPackage)
{
	return LevelAssetRegistryHelper::GetLevelInfoFromAssetRegistry(LevelPackage, [](const FAssetData& Asset)
	{
		return GetIsLevelUsingExternalActorsFromAsset(Asset);
	});
}

bool ULevel::GetIsUsingActorFoldersFromAsset(const FAssetData& Asset)
{
	FString LevelIsUsingActorFoldersStr;
	static const FName NAME_LevelIsUsingActorFolders(TEXT("LevelIsUsingActorFolders"));
	if (Asset.GetTagValue(NAME_LevelIsUsingActorFolders, LevelIsUsingActorFoldersStr))
	{
		check(LevelIsUsingActorFoldersStr == TEXT("1"));
		return true;
	}
	return false;
}

bool ULevel::GetIsUsingActorFoldersFromPackage(FName LevelPackage)
{
	return LevelAssetRegistryHelper::GetLevelInfoFromAssetRegistry(LevelPackage, [](const FAssetData& Asset)
	{
		return GetIsUsingActorFoldersFromAsset(Asset);
	});
}

bool ULevel::GetIsStreamingDisabledFromAsset(const FAssetData& Asset)
{
	FString LevelHasStreamingDisabledStr;
	static const FName NAME_LevelHasStreamingDisabled(TEXT("LevelHasStreamingDisabled"));
	if (Asset.GetTagValue(NAME_LevelHasStreamingDisabled, LevelHasStreamingDisabledStr))
	{
		check(LevelHasStreamingDisabledStr == TEXT("1"));
		return true;
	}
	return false;
}

bool ULevel::GetWorldExternalActorsReferencesFromAsset(const FAssetData& Asset, TArray<FGuid>& OutWorldExternalActorsReferences)
{
	FString WorldExternalActorsReferencesStr;
	static const FName NAME_WorldExternalActorsReferences(TEXT("WorldExternalActorsReferences"));
	if (Asset.GetTagValue(NAME_WorldExternalActorsReferences, WorldExternalActorsReferencesStr))
	{
		TArray<FString> WorldReferencesStr;
		if (WorldExternalActorsReferencesStr.ParseIntoArray(WorldReferencesStr, TEXT(",")))
		{
			Algo::Transform(WorldReferencesStr, OutWorldExternalActorsReferences, [](const FString& GuidStr) { return FGuid(GuidStr); });
			return true;
		}
	}
	return false;
}

bool ULevel::GetIsStreamingDisabledFromPackage(FName LevelPackage)
{
	return LevelAssetRegistryHelper::GetLevelInfoFromAssetRegistry(LevelPackage, [](const FAssetData& Asset)
	{
		return GetIsStreamingDisabledFromAsset(Asset);
	});
}

bool ULevel::GetWorldExternalActorsReferencesFromPackage(FName LevelPackage, TArray<FGuid>& OutWorldExternalActorsReferences)
{
	return LevelAssetRegistryHelper::GetLevelInfoFromAssetRegistry(LevelPackage, [&OutWorldExternalActorsReferences](const FAssetData& Asset)
	{
		return GetWorldExternalActorsReferencesFromAsset(Asset, OutWorldExternalActorsReferences);
	});
}

bool ULevel::GetLevelBoundsFromAsset(const FAssetData& Asset, FBox& OutLevelBounds)
{
	FString LevelBoundsLocationStr;
	static const FName NAME_LevelBoundsLocation(TEXT("LevelBoundsLocation"));

	FString LevelBoundsExtentStr;
	static const FName NAME_LevelBoundsExtent(TEXT("LevelBoundsExtent"));

	if (Asset.GetTagValue(NAME_LevelBoundsLocation, LevelBoundsLocationStr) &&
		Asset.GetTagValue(NAME_LevelBoundsExtent, LevelBoundsExtentStr))
	{
		FVector LevelBoundsLocation;
		FVector LevelBoundsExtent;
		if (LevelBoundsLocation.InitFromCompactString(LevelBoundsLocationStr) &&
			LevelBoundsExtent.InitFromCompactString(LevelBoundsExtentStr))
		{
			OutLevelBounds = FBox(LevelBoundsLocation - LevelBoundsExtent, LevelBoundsLocation + LevelBoundsExtent);
			return true;
		}
	}
	// Invalid bounds
	return false;
}

bool ULevel::GetLevelBoundsFromPackage(FName LevelPackage, FBox& OutLevelBounds)
{
	return LevelAssetRegistryHelper::GetLevelInfoFromAssetRegistry(LevelPackage, [&OutLevelBounds](const FAssetData& Asset)
	{
		return GetLevelBoundsFromAsset(Asset, OutLevelBounds);
	});
}

FVector ULevel::GetLevelInstancePivotOffsetFromAsset(const FAssetData& Asset)
{
	static const FName NAME_LevelInstancePivotOffset(TEXT("LevelInstancePivotOffset"));
	FString LevelInstancePivotOffsetStr;
	if (Asset.GetTagValue(NAME_LevelInstancePivotOffset, LevelInstancePivotOffsetStr))
	{
		FVector LevelInstancePivotOffset;
		if (LevelInstancePivotOffset.InitFromCompactString(LevelInstancePivotOffsetStr))
		{
			return LevelInstancePivotOffset;
		}
	}
	return FVector::ZeroVector;
}

FVector ULevel::GetLevelInstancePivotOffsetFromPackage(FName LevelPackage)
{
	FVector LevelInstancePivot = FVector::ZeroVector;
	LevelAssetRegistryHelper::GetLevelInfoFromAssetRegistry(LevelPackage, [&LevelInstancePivot](const FAssetData& Asset)
	{
		LevelInstancePivot = GetLevelInstancePivotOffsetFromAsset(Asset);
		return true;
	});
	return LevelInstancePivot;
}

bool ULevel::GetPromptWhenAddingToLevelOutsideBounds() const
{
	return !bIsPartitioned && bPromptWhenAddingToLevelOutsideBounds && GetDefault<ULevelEditorMiscSettings>()->bPromptWhenAddingToLevelOutsideBounds;
}

bool ULevel::GetPromptWhenAddingToLevelBeforeCheckout() const
{
	return !bIsPartitioned && bPromptWhenAddingToLevelBeforeCheckout && GetDefault<ULevelEditorMiscSettings>()->bPromptWhenAddingToLevelBeforeCheckout;
}
#endif

void ULevel::InvalidateModelGeometry()
{
	// Save the level/model state for transactions.
	Model->Modify(true);
	Modify();

	// Remove existing model components.
	for(int32 ComponentIndex = 0;ComponentIndex < ModelComponents.Num();ComponentIndex++)
	{
		if(ModelComponents[ComponentIndex])
		{
			ModelComponents[ComponentIndex]->Modify();
			ModelComponents[ComponentIndex]->UnregisterComponent();
		}
	}
	ModelComponents.Empty();

	// Begin releasing the model's resources.
	Model->BeginReleaseResources();
}


void ULevel::InvalidateModelSurface()
{
	Model->InvalidSurfaces = true;
}

void ULevel::CommitModelSurfaces()
{
	if(Model->InvalidSurfaces)
	{
		if (!Model->bOnlyRebuildMaterialIndexBuffers)
		{
			// Unregister model components
			for (int32 ComponentIndex = 0; ComponentIndex < ModelComponents.Num(); ComponentIndex++)
			{
				if (ModelComponents[ComponentIndex] && ModelComponents[ComponentIndex]->IsRegistered())
				{
					ModelComponents[ComponentIndex]->UnregisterComponent();
				}
			}
		}

		// Begin releasing the model's resources.
		Model->BeginReleaseResources();

		// Wait for the model's resources to be released.
		FlushRenderingCommands();

		// Clear the model index buffers.
		Model->MaterialIndexBuffers.Empty();

		// Update the model vertices.
		Model->UpdateVertices();

		// Update the model components.
		for(int32 ComponentIndex = 0;ComponentIndex < ModelComponents.Num();ComponentIndex++)
		{
			if(ModelComponents[ComponentIndex])
			{
				ModelComponents[ComponentIndex]->CommitSurfaces();
			}
		}
		Model->InvalidSurfaces = false;

		// Initialize the model's index buffers.
		for(TMap<UMaterialInterface*,TUniquePtr<FRawIndexBuffer16or32> >::TIterator IndexBufferIt(Model->MaterialIndexBuffers);
			IndexBufferIt;
			++IndexBufferIt)
		{
			BeginInitResource(IndexBufferIt->Value.Get());
		}

		// Register model components before init'ing index buffer so collision has access to index buffer data
		// This matches the order of operation in ULevel::UpdateModelComponents
		if (ModelComponents.Num() > 0)
		{
			check( OwningWorld );
			// Update model components.
			for(int32 ComponentIndex = 0;ComponentIndex < ModelComponents.Num();ComponentIndex++)
			{
				if(ModelComponents[ComponentIndex])
				{
					if (Model->bOnlyRebuildMaterialIndexBuffers)
					{
						// This is intentionally updated immediately. We just re-created vertex and index buffers
						// without invalidating static meshes. Re-create all static meshes now so that mesh draw
						// commands are refreshed.
						ModelComponents[ComponentIndex]->RecreateRenderState_Concurrent();
					}
					else
					{
						ModelComponents[ComponentIndex]->RegisterComponentWithWorld(OwningWorld);
					}
				}
			}
		}

		Model->bOnlyRebuildMaterialIndexBuffers = false;
	}
}

#if WITH_EDITOR
void ULevel::AddActorFolder(UActorFolder* InActorFolder)
{
	Modify(false);
	check(InActorFolder);
	const FGuid& FolderGuid = InActorFolder->GetGuid();
	check(FolderGuid.IsValid());
	check(!ActorFolders.Contains(FolderGuid));
	ActorFolders.Add(FolderGuid, InActorFolder);
	if (!InActorFolder->IsMarkedAsDeleted())
	{
		FActorFolderSet& Folders = FolderLabelToActorFolders.FindOrAdd(InActorFolder->GetLabel());
		Folders.Add(InActorFolder);
	}
}

void ULevel::RemoveActorFolder(UActorFolder* InActorFolder)
{
	Modify(false);
	check(InActorFolder);
	check(InActorFolder->IsMarkedAsDeleted());
	check(GAllowCleanupActorFolders);

	TObjectPtr<UActorFolder> FoundActorFolder;
	if (ensure(ActorFolders.RemoveAndCopyValue(InActorFolder->GetGuid(), FoundActorFolder)))
	{
		check(FoundActorFolder == InActorFolder);
		FActorFolderSet* Folders = FolderLabelToActorFolders.Find(FoundActorFolder->GetLabel());
		check(!Folders || !Folders->GetActorFolders().Contains(InActorFolder))

		GEngine->BroadcastActorFolderRemoved(InActorFolder);
		
		InActorFolder->Modify();
		InActorFolder->MarkAsGarbage();
		check(InActorFolder->GetPackage()->IsDirty());
	}
}

void ULevel::OnFolderMarkAsDeleted(UActorFolder* InActorFolder)
{
	check(InActorFolder);
	check(InActorFolder->IsMarkedAsDeleted());
	if (ensure(ActorFolders.Contains(InActorFolder->GetGuid())))
	{
		Modify(false);
		FActorFolderSet& Folders = FolderLabelToActorFolders.FindChecked(InActorFolder->GetLabel());
		verify(Folders.Remove(InActorFolder) || GIsFixingActorFolders);
		if (Folders.IsEmpty())
		{
			FolderLabelToActorFolders.Remove(InActorFolder->GetLabel());
		}
		if (GAllowCleanupActorFolders)
		{
			// Get all unreferenced folders or folders that resolves to the root ("/")
			for (const FGuid& GuidFolder : GetDeletedAndUnreferencedActorFolders())
			{
				// Remove our folder if it's part of these folders
				if (GuidFolder == InActorFolder->GetGuid())
				{
					RemoveActorFolder(InActorFolder);
				}
				// Also allow to remove other folders of this list only if their external package is writable
				else if (UActorFolder* ActorFolder = GetActorFolder(GuidFolder, false))
				{
					if (!ActorFolder->GetParent())
					{
						if (UPackage* Package = ActorFolder->GetExternalPackage())
						{
							FPackagePath PackagePath(Package->GetLoadedPath());
							if (FPackageName::DoesPackageExist(PackagePath, &PackagePath))
							{
								FString FilePath = PackagePath.GetLocalFullPath();
								const bool bReadOnly = FPlatformFileManager::Get().GetPlatformFile().IsReadOnly(*FilePath);
								if (!bReadOnly)
								{
									RemoveActorFolder(ActorFolder);
								}
							}
						}
					}
				}
			}
		}
	}
}

void ULevel::OnFolderLabelChanged(UActorFolder* InActorFolder, const FString& InOldFolderLabel)
{
	check(InActorFolder);
	if (InOldFolderLabel.IsEmpty())
	{
		// We are in the process of creating the actor folder
		check(!ActorFolders.Contains(InActorFolder->GetGuid()));
		return;
	}
	if (ensure(ActorFolders.Contains(InActorFolder->GetGuid())))
	{
		Modify(false);
		FActorFolderSet& OldLabelFolders = FolderLabelToActorFolders.FindChecked(InOldFolderLabel);
		verify(OldLabelFolders.Remove(InActorFolder) || GIsFixingActorFolders);
		if (OldLabelFolders.IsEmpty())
		{
			FolderLabelToActorFolders.Remove(InOldFolderLabel);
		}
		FActorFolderSet& NewLabelFolders = FolderLabelToActorFolders.FindOrAdd(InActorFolder->GetLabel());
		NewLabelFolders.Add(InActorFolder);
	}
}

void ULevel::FixupActorFolders()
{
	auto BuildFolderLabelToActorFolders = [this]()
	{
		ForEachActorFolder([this](UActorFolder* ActorFolder)
		{
			FActorFolderSet& Folders = FolderLabelToActorFolders.FindOrAdd(ActorFolder->GetLabel());
			Folders.Add(ActorFolder);
			return true;
		}, /*bSkipDeleted*/ true);
	};

	if (!IsActorFolderObjectsFeatureAvailable())
	{
		return;
	}

	if (IsUsingActorFolders())
	{
		TGuardValue<bool> FixingActorFolders(GIsFixingActorFolders, true);

		if (IsUsingExternalObjects())
		{
			// At this point, LoadedExternalActorFolders are fully loaded, transfer them to the ActorFolders list.
			for (UActorFolder* LoadedActorFolder : LoadedExternalActorFolders)
			{
				check(LoadedActorFolder->GetGuid().IsValid());
				AddActorFolder(LoadedActorFolder);
			}
			LoadedExternalActorFolders.Empty();

			if (bWasDuplicated)
			{
				// Duplicated folders won't call AddActorFolder, build FolderLabelToActorFolders
				BuildFolderLabelToActorFolders();
			}
		}
		else
		{
			// Build FolderLabelToActorFolders for non-externalized ActorFolders
			BuildFolderLabelToActorFolders();
		}

		ForEachActorFolder([](UActorFolder* ActorFolder)
		{
			// Detects and clears invalid parent folder
			ActorFolder->FixupParentFolder();
			return true;
		}, /*bSkipDeleted*/ true);

		const bool bFixDuplicateFolders = !IsRunningCommandlet();
		if (bFixDuplicateFolders)
		{
			// Build a sorted list of duplicate paths and a map of duplicate path to corresponding actor folders
			TArray<FName> SortedDuplicatePaths;
			TMap<FName, TArray<UActorFolder*>> PathToFolders;
			ForEachActorFolder([&SortedDuplicatePaths, &PathToFolders](UActorFolder* ActorFolder)
			{
				FName Path = ActorFolder->GetPath();
				if (PathToFolders.FindOrAdd(ActorFolder->GetPath()).Add(ActorFolder) == 1)
				{
					SortedDuplicatePaths.Add(Path);
				}
				return true;
			}, /*bSkipDeleted*/ true);

			SortedDuplicatePaths.Sort([](const FName& FolderPathA, const FName& FolderPathB)
			{
				return FolderPathA.LexicalLess(FolderPathB);
			});

			TSet<UActorFolder*> FoldersToDelete;
			auto HasParentToDelete = [&FoldersToDelete](UActorFolder* InFolder)
			{
				UActorFolder* Parent = InFolder->GetParent();
				while (Parent)
				{
					if (FoldersToDelete.Contains(Parent))
					{
						return true;
					}
					Parent = Parent->GetParent();
				}
				return false;
			};

			TSet<UActorFolder*> FoldersToKeep;
			TMap<UActorFolder*, UActorFolder*> DuplicateFolders;

			for (FName& DuplicatePath : SortedDuplicatePaths)
			{
				// Choose to keep the folder that has no parent to delete
				TArray<UActorFolder*>& Folders = PathToFolders.FindChecked(DuplicatePath);
				UActorFolder* FolderToKeep = nullptr;
				for (UActorFolder* Folder : Folders)
				{
					if (!HasParentToDelete(Folder))
					{
						FolderToKeep = Folder;
						break;
					}
				}

				// Validation
				check(FolderToKeep);
				bool bAlreadyExists = false;
				FoldersToKeep.Add(FolderToKeep, &bAlreadyExists);
				check(!bAlreadyExists);

				// All other folders are considered duplicated and will be marked as deleted
				for (UActorFolder* Folder : Folders)
				{
					if (Folder != FolderToKeep)
					{
						FoldersToDelete.Add(Folder);

						// Cleanup FolderLabelToActorFolders from duplicate folder as we don't want GetActorFolder(Path) to resolve to a duplicate ActorFolder
						FActorFolderSet* FoundSet = FolderLabelToActorFolders.Find(Folder->GetLabel());
						// We should have at least 2 elements in the set (the one we keep and one or multiple duplicates)
						check(FoundSet && FoundSet->GetActorFolders().Num() > 1 && FoundSet->GetActorFolders().Contains(Folder));
						verify(FoundSet->Remove(Folder));
						check(!FoundSet->IsEmpty());

						// Keep a map that will be used to retrieve the folder we keep for each duplicate/deleted folder
						DuplicateFolders.Add(Folder, FolderToKeep);
					}
				}
			}

			// Sort in descending order so children will be deleted before parents
			TArray<UActorFolder*> SortedFoldersToDelete = FoldersToDelete.Array();
			SortedFoldersToDelete.Sort([](const UActorFolder& FolderA, const UActorFolder& FolderB)
			{
				return FolderB.GetPath().LexicalLess(FolderA.GetPath());
			});

			for (UActorFolder* FolderToDelete : SortedFoldersToDelete)
			{
				UActorFolder* NewParent = DuplicateFolders.FindChecked(FolderToDelete);
				// First move duplicate folder under the single folder we keep. Use a unique name to avoid dealing with name clash
				const FFolder OldFolder = FolderToDelete->GetFolder();
				UE_LOG(LogLevel, Log, TEXT("Merging duplicate actor folder %s."), *OldFolder.GetPath().ToString());

				// Since we can't rename to a dest with the same parent hierarchy, do it in 2 passes.
				// For example: If we want to rename A/B to A/B/B_Dup123, we need to :
				// 1- Rename A/B to A/B_Dup123
				// 2- Rename A/B_Dup123 to A/B/B_Dup123
				// Then we mark for delete A/B/B_Dup123, so that child actors and folders of A/B/B_Dup123 will be parented back to in A/B.
				const FString NewPath = FString::Printf(TEXT("%s_%s"), *NewParent->GetPath().ToString(), *FGuid::NewGuid().ToString());
				const FFolder NewFolder = FFolder(OldFolder.GetRootObject(), FName(NewPath));
				FLevelActorFoldersHelper::RenameFolder(this, OldFolder, NewFolder);
				const FString NewPath2 = FString::Printf(TEXT("%s/DuplicateFolder_%s"), *NewParent->GetPath().ToString(), *FolderToDelete->GetLabel(), *FGuid::NewGuid().ToString());
				const FFolder NewFolder2 = FFolder(OldFolder.GetRootObject(), FName(NewPath2));
				FLevelActorFoldersHelper::RenameFolder(this, NewFolder, NewFolder2);
				// Then delete (mark as deleted) this folder
				FLevelActorFoldersHelper::DeleteFolder(this, FolderToDelete->GetFolder());
			}
		}
	}
	
	// Fixup actors actor folder
	if (bFixupActorFoldersAtLoad)
	{
		for (AActor* Actor : Actors)
		{
			if (IsValid(Actor))
			{
				Actor->FixupActorFolder();
			}
		}
	}
	
	if (IsUsingActorFolders())
	{
		ForEachActorFolder([](UActorFolder* ActorFolder)
		{
			GEngine->BroadcastActorFolderAdded(ActorFolder);
			return true;
		}, /*bSkipDeleted*/ true);
	}
}
#endif

void ULevel::OnLevelLoaded()
{
#if WITH_EDITOR
	FixupActorFolders();
#endif

	// Find associated level streaming for this level
	const ULevelStreaming* LevelStreaming = FLevelUtils::FindStreamingLevel(this);

	// Set level's associated WorldPartitionRuntimeCell for dynamically injected cells
	if (LevelStreaming && !WorldPartitionRuntimeCell.GetUniqueID().IsValid())
	{
		WorldPartitionRuntimeCell = Cast<const UWorldPartitionRuntimeCell>(LevelStreaming->GetWorldPartitionCell());
	}

	// 1. Cook commandlet does it's own UWorldPartition::Initialize call in FWorldPartitionCookPackageSplitter::GetGenerateList
	// 2. Do not Initialize if World doesn't have a UWorldPartitionSubsystem
	if (!IsRunningCookCommandlet() && OwningWorld->HasSubsystem<UWorldPartitionSubsystem>())
	{
		if (UWorldPartition* WorldPartition = GetWorldPartition())
		{
			//
			// When do we need to initialize the associated world partition object?
			//
			//	- When the level is the main world persistent level
			//	- When the sublevel is streamed in the editor (mainly for data layers)
			//	- When the sublevel is streamed in game and the main world is not partitioned
			//
			const bool bIsOwningWorldGameWorld = OwningWorld->IsGameWorld();
			const bool bIsOwningWorldPartitioned = OwningWorld->IsPartitionedWorld();
			const bool bIsMainWorldLevel = OwningWorld->PersistentLevel == this;
			const bool bInitializeForEditor = !bIsOwningWorldGameWorld;
			const bool bInitializeForGame = bIsOwningWorldGameWorld;

			UE_LOG(LogWorldPartition, Log, TEXT("ULevel::OnLevelLoaded(%s)(bIsOwningWorldGameWorld=%d, bIsOwningWorldPartitioned=%d, InitializeForMainWorld=%d, InitializeForEditor=%d, InitializeForGame=%d)"), 
				*GetTypedOuter<UWorld>()->GetName(), bIsOwningWorldGameWorld ? 1 : 0, bIsOwningWorldPartitioned ? 1 : 0, bIsMainWorldLevel ? 1 : 0, bInitializeForEditor ? 1 : 0, bInitializeForGame ? 1 : 0);

			if (bIsMainWorldLevel || bInitializeForEditor)
			{
				if (!WorldPartition->IsInitialized())
				{
					FTransform Transform = LevelStreaming ? LevelStreaming->LevelTransform : FTransform::Identity;
					WorldPartition->Initialize(OwningWorld, Transform);
				}
			}
		}
	}
}

void ULevel::BuildStreamingData(UWorld* World, ULevel* TargetLevel/*=NULL*/, UTexture2D* UpdateSpecificTextureOnly/*=NULL*/)
{
#if WITH_EDITORONLY_DATA
	double StartTime = FPlatformTime::Seconds();


	TArray<ULevel* > LevelsToCheck;
	if ( TargetLevel )
	{
		LevelsToCheck.Add(TargetLevel);
	}
	else if ( World )
	{
		for ( int32 LevelIndex=0; LevelIndex < World->GetNumLevels(); LevelIndex++ )
		{
			ULevel* Level = World->GetLevel(LevelIndex);
			LevelsToCheck.Add(Level);
		}
	}
	else
	{
		for (TObjectIterator<ULevel> It; It; ++It)
		{
			ULevel* Level = *It;
			LevelsToCheck.Add(Level);
		}
	}

	for ( int32 LevelIndex=0; LevelIndex < LevelsToCheck.Num(); LevelIndex++ )
	{
		ULevel* Level = LevelsToCheck[LevelIndex];
		if (!Level) continue;

		if (Level->bIsVisible || Level->IsPersistentLevel())
		{
			IStreamingManager::Get().AddLevel(Level);
		}
		//@todo : handle UpdateSpecificTextureOnly
	}

	UE_LOG(LogLevel, Verbose, TEXT("ULevel::BuildStreamingData took %.3f seconds."), FPlatformTime::Seconds() - StartTime);
#else
	UE_LOG(LogLevel, Fatal,TEXT("ULevel::BuildStreamingData should not be called on a console"));
#endif
}

ABrush* ULevel::GetDefaultBrush() const
{
	ABrush* DefaultBrush = nullptr;
	if (Actors.Num() >= 2)
	{
		// If the builder brush exists then it will be the 2nd actor in the actors array.
		DefaultBrush = Cast<ABrush>(Actors[1]);
		// If the second actor is not a brush then it certainly cannot be the builder brush.
		if (DefaultBrush != nullptr)
		{
			checkf(DefaultBrush->GetBrushComponent(), TEXT("%s"), *GetPathName());
			checkf(DefaultBrush->Brush != nullptr, TEXT("%s"), *GetPathName());
		}
	}
	return DefaultBrush;
}


AWorldSettings* ULevel::GetWorldSettings(bool bChecked) const
{
	if (bChecked)
	{
		checkf( WorldSettings != nullptr, TEXT("%s"), *GetPathName() );
	}
	return WorldSettings;
}

void ULevel::SetWorldSettings(AWorldSettings* NewWorldSettings)
{
	check(NewWorldSettings); // Doesn't make sense to be clearing a world settings object
	if (WorldSettings != NewWorldSettings)
	{
		// We'll generally endeavor to keep the world settings at its traditional index 0
		const int32 NewWorldSettingsIndex = Actors.FindLast( NewWorldSettings );
		if (NewWorldSettingsIndex != 0)
		{
			if (Actors[0] == nullptr || Actors[0]->IsA<AWorldSettings>())
			{
				Exchange(Actors[0],Actors[NewWorldSettingsIndex]);
			}
			else
			{
				Actors[NewWorldSettingsIndex] = nullptr;
				Actors.Insert(NewWorldSettings,0);
			}
		}

		// Assign the new world settings before destroying the old ones
		// since level will prevent destruction of the world settings if it matches the cached value
		WorldSettings = NewWorldSettings;

		// Makes no sense to have several WorldSettings so destroy existing ones
		for (int32 ActorIndex=1; ActorIndex<Actors.Num(); ActorIndex++)
		{
			if (AActor* Actor = Actors[ActorIndex])
			{
				if (AWorldSettings* ExistingWorldSettings = Cast<AWorldSettings>(Actor))
				{
					check(ExistingWorldSettings != WorldSettings);
					ExistingWorldSettings->Destroy();
				}
			}
		}
	}
}

AWorldDataLayers* ULevel::GetWorldDataLayers() const
{
	return WorldDataLayers;
}

void ULevel::SetWorldDataLayers(AWorldDataLayers* NewWorldDataLayers)
{
	check(!WorldDataLayers || WorldDataLayers == NewWorldDataLayers);
	WorldDataLayers = NewWorldDataLayers;
}

const IWorldPartitionCell* ULevel::GetWorldPartitionRuntimeCell() const
{
	return WorldPartitionRuntimeCell.Get();
}

UWorldPartition* ULevel::GetWorldPartition() const
{
	return bIsPartitioned ? GetWorldSettings()->GetWorldPartition() : nullptr;
}

ALevelScriptActor* ULevel::GetLevelScriptActor() const
{
	return LevelScriptActor;
}


void ULevel::InitializeNetworkActors()
{
	check( OwningWorld );

	const bool bIsClient = OwningWorld->IsNetMode(NM_Client);

	// Kill non relevant client actors and set net roles correctly
	for( int32 ActorIndex=0; ActorIndex<Actors.Num(); ActorIndex++ )
	{
		AActor* Actor = Actors[ActorIndex];
		if( Actor )
		{
			// Kill off actors that aren't interesting to the client.
			if( !Actor->IsActorInitialized() && !Actor->bActorSeamlessTraveled )
			{
				// Add to startup list
				if (Actor->bNetLoadOnClient)
				{
					Actor->bNetStartup = true;

					for (UActorComponent* Component : Actor->GetComponents())
					{
						if (Component)
						{
							Component->SetIsNetStartupComponent(true);
						}
					}
				}

				if (bIsClient)
				{
					if (!Actor->bNetLoadOnClient)
					{
						Actor->Destroy(true);
					}
					else
					{
						// Exchange the roles if:
						//	-We are a client
						//  -This is bNetLoadOnClient=true
						//  -RemoteRole != ROLE_None
						Actor->ExchangeNetRoles(true);
					}
				}				
			}

			Actor->bActorSeamlessTraveled = false;
		}
	}

	bAlreadyClearedActorsSeamlessTravelFlag = true;
	bAlreadyInitializedNetworkActors = true;
}

void ULevel::ClearActorsSeamlessTraveledFlag()
{
	for (AActor* Actor : Actors)
	{
		if (Actor)
		{
			Actor->bActorSeamlessTraveled = false;
		}
	}

	bAlreadyClearedActorsSeamlessTravelFlag = true;
}

void ULevel::InitializeRenderingResources()
{
	// OwningWorld can be NULL when InitializeRenderingResources is called during undo, where a transient ULevel is created to allow undoing level move operations
	// At the point at which Pre/PostEditChange is called on that transient ULevel, it is not part of any world and therefore should not have its rendering resources initialized
	if (OwningWorld && bIsVisible && FApp::CanEverRender())
	{
		ULevel* ActiveLightingScenario = OwningWorld->GetActiveLightingScenario();
		UMapBuildDataRegistry* EffectiveMapBuildData = MapBuildData;

		if (ActiveLightingScenario && ActiveLightingScenario->MapBuildData)
		{
			EffectiveMapBuildData = ActiveLightingScenario->MapBuildData;
		}

		if (!PrecomputedLightVolume->IsAddedToScene())
		{
			PrecomputedLightVolume->AddToScene(OwningWorld->Scene, EffectiveMapBuildData, LevelBuildDataId);
		}

		if (!PrecomputedVolumetricLightmap->IsAddedToScene())
		{
			PrecomputedVolumetricLightmap->AddToScene(OwningWorld->Scene, EffectiveMapBuildData, LevelBuildDataId, IsPersistentLevel());
		}

		if (OwningWorld->Scene && EffectiveMapBuildData)
		{
			EffectiveMapBuildData->InitializeClusterRenderingResources(OwningWorld->Scene->GetFeatureLevel());
		}
	}
}

void ULevel::ReleaseRenderingResources()
{
	if (OwningWorld && FApp::CanEverRender())
	{
		if (PrecomputedLightVolume)
		{
			PrecomputedLightVolume->RemoveFromScene(OwningWorld->Scene);
		}

		if (PrecomputedVolumetricLightmap)
		{
			PrecomputedVolumetricLightmap->RemoveFromScene(OwningWorld->Scene);
		}
	}
}

void ULevel::ResetRouteActorInitializationState()
{
	 RouteActorInitializationState = ERouteActorInitializationState::Preinitialize;
	 RouteActorInitializationIndex = 0;
}

int32 ULevel::GetEstimatedAddToWorldWorkUnitsTotal() const
{
	const int32 ActorCount = Actors.Num();
	return ActorCount + ActorCount * GRouteActorInitializationWorkUnitWeighting;
}

int32 ULevel::GetEstimatedAddToWorldWorkUnitsRemaining() const
{
	// We count "work units" as the number of actors requiring component update, plus the number of actors requiring initialization
	if (bIsVisible)
	{
		return 0;
	}
	const int32 ActorCount = Actors.Num();
	int32 TotalWorkUnits = 0;
	if (!bAlreadyUpdatedComponents && IncrementalComponentState != EIncrementalComponentState::Finalize)
	{
		TotalWorkUnits += ActorCount - CurrentActorIndexForIncrementalUpdate;
	}
	if (!IsFinishedRouteActorInitialization())
	{
		// Count remaining work items from the current state, plus work items from the states that follow
		const int32 NumStates = (int32)ERouteActorInitializationState::Finished;
		int32 ActorInitWorkItemCount = ActorCount - RouteActorInitializationIndex;

		int32 CompleteStatesRemaining = (int32)ERouteActorInitializationState::Finished - (int32)RouteActorInitializationState - 1;
		ActorInitWorkItemCount += ActorCount * CompleteStatesRemaining;

		TotalWorkUnits += ActorInitWorkItemCount * GRouteActorInitializationWorkUnitWeighting / NumStates;
	}
	return TotalWorkUnits;
}

void ULevel::RouteActorInitialize(int32 NumActorsToProcess)
{
	TRACE_OBJECT_EVENT(this, RouteActorInitialize);

	const bool bFullProcessing = (NumActorsToProcess <= 0);
	switch (RouteActorInitializationState)
	{
		case ERouteActorInitializationState::Preinitialize:
		{
			// Actor pre-initialization may spawn new actors so we need to incrementally process until actor count stabilizes
			while (RouteActorInitializationIndex < Actors.Num())
			{
				AActor* const Actor = Actors[RouteActorInitializationIndex];
				if (Actor && !Actor->IsActorInitialized())
				{
					Actor->PreInitializeComponents();
				}

				++RouteActorInitializationIndex;
				if (!bFullProcessing && (--NumActorsToProcess == 0))
				{
					return;
				}
			}

			RouteActorInitializationIndex = 0;
			RouteActorInitializationState = ERouteActorInitializationState::Initialize;
		}

		// Intentional fall-through, proceeding if we haven't expired our actor count budget
		case ERouteActorInitializationState::Initialize:
		{
			while (RouteActorInitializationIndex < Actors.Num())
			{
				AActor* const Actor = Actors[RouteActorInitializationIndex];
				if (Actor)
				{
					if (!Actor->IsActorInitialized())
					{
						Actor->InitializeComponents();
						Actor->PostInitializeComponents();
						if (!Actor->IsActorInitialized() && IsValidChecked(Actor))
						{
							UE_LOG(LogActor, Fatal, TEXT("%s failed to route PostInitializeComponents. Please call Super::PostInitializeComponents() in your <className>::PostInitializeComponents() function."), *Actor->GetFullName());
						}
					}
				}

				++RouteActorInitializationIndex;
				if (!bFullProcessing && (--NumActorsToProcess == 0))
				{
					return;
				}
			}

			RouteActorInitializationIndex = 0;
			RouteActorInitializationState = ERouteActorInitializationState::BeginPlay;
		}

		// Intentional fall-through, proceeding if we haven't expired our actor count budget
		case ERouteActorInitializationState::BeginPlay:
		{
			if (OwningWorld->HasBegunPlay())
			{
				while (RouteActorInitializationIndex < Actors.Num())
				{
					// Child actors have play begun explicitly by their parents
					AActor* const Actor = Actors[RouteActorInitializationIndex];
					if (Actor && !Actor->IsChildActor())
					{
						// This will no-op if the actor has already begun play
						SCOPE_CYCLE_COUNTER(STAT_ActorBeginPlay);
						const bool bFromLevelStreaming = true;
						Actor->DispatchBeginPlay(bFromLevelStreaming);
					}

					++RouteActorInitializationIndex;
					if (!bFullProcessing && (--NumActorsToProcess == 0))
					{
						return;
					}
				}
			}

			RouteActorInitializationState = ERouteActorInitializationState::Finished;
		}

		// Intentional fall-through if we're done
		case ERouteActorInitializationState::Finished:
		{
			break;
		}
	}
}

UPackage* ULevel::CreateMapBuildDataPackage() const
{
	FString PackageName = GetOutermost()->GetName() + TEXT("_BuiltData");
	UPackage* BuiltDataPackage = CreatePackage( *PackageName);
	// PKG_ContainsMapData required so FEditorFileUtils::GetDirtyContentPackages can treat this as a map package
	BuiltDataPackage->SetPackageFlags(PKG_ContainsMapData);
	return BuiltDataPackage;
}

UMapBuildDataRegistry* ULevel::GetOrCreateMapBuildData()
{
	if (!MapBuildData 
		// If MapBuildData is in the level package we need to create a new one, see CreateRegistryForLegacyMap
		|| MapBuildData->IsLegacyBuildData()
		|| !MapBuildData->HasAllFlags(RF_Public | RF_Standalone))
	{
		if (MapBuildData)
		{
			// Release rendering data depending on MapBuildData, before we destroy MapBuildData
			MapBuildData->InvalidateStaticLighting(GetWorld(), true, nullptr);

			// Allow the legacy registry to be GC'ed
			MapBuildData->ClearFlags(RF_Standalone);
		}

		UPackage* BuiltDataPackage = CreateMapBuildDataPackage();

		FName ShortPackageName = FPackageName::GetShortFName(BuiltDataPackage->GetFName());
		// Top level UObjects have to have both RF_Standalone and RF_Public to be saved into packages
		MapBuildData = NewObject<UMapBuildDataRegistry>(BuiltDataPackage, ShortPackageName, RF_Standalone | RF_Public);
		MarkPackageDirty();
	}

	return MapBuildData;
}

void ULevel::SetLightingScenario(bool bNewIsLightingScenario)
{
	bIsLightingScenario = bNewIsLightingScenario;

	OwningWorld->PropagateLightingScenarioChange();
}

bool ULevel::HasAnyActorsOfType(UClass *SearchType)
{
	// just search the actors array
	for (int32 Idx = 0; Idx < Actors.Num(); Idx++)
	{
		AActor *Actor = Actors[Idx];
		// if valid, not pending kill, and
		// of the correct type
		if (IsValid(Actor) &&
			Actor->IsA(SearchType))
		{
			return true;
		}
	}
	return false;
}

#if WITH_EDITOR
FString ULevel::ResolveRootPath(const FString& LevelPackageName, const UObject* InLevelMountPointContext)
{
	TOptional<FString> ResolvedLevelMountPoint;
	if (InLevelMountPointContext)
	{
		for (const FLevelMountPointResolverDelegate& Resolver : ULevel::LevelMountPointResolvers)
		{
			FString Result;
			if (Resolver.Execute(LevelPackageName, InLevelMountPointContext, Result) && !Result.IsEmpty())
			{
				ResolvedLevelMountPoint = Result;
				break;
			}
		}
	}

	// If ResolvedLevelMountPoint is not set, fallback on LevelPackageName
	const FString LevelRootPath = ResolvedLevelMountPoint.Get(LevelPackageName);
	return LevelRootPath;
}

FString ULevel::GetActorPackageName(UPackage* InLevelPackage, EActorPackagingScheme ActorPackagingScheme, const FString& InActorPath, const UObject* InLevelMountPointContext)
{
	check(InLevelPackage);
	const FString LevelRootPath = ULevel::ResolveRootPath(InLevelPackage->GetName(), InLevelMountPointContext);
	return ULevel::GetActorPackageName(ULevel::GetExternalActorsPath(LevelRootPath), ActorPackagingScheme, InActorPath);
}

FString ULevel::GetActorPackageName(const FString& InBaseDir, EActorPackagingScheme ActorPackagingScheme, const FString& InActorPath)
{
	// Convert the actor path to lowercase to make sure we get the same hash for case insensitive file systems
	FString ActorPath = InActorPath.ToLower();

	FArchiveMD5 ArMD5;
	ArMD5 << ActorPath;

	FGuid PackageGuid = ArMD5.GetGuidFromHash();
	check(PackageGuid.IsValid());

	FString GuidBase36 = PackageGuid.ToString(EGuidFormats::Base36Encoded);
	check(GuidBase36.Len());

	TStringBuilderWithBuffer<TCHAR, NAME_SIZE> ActorPackageName;

	uint32 FilenameOffset = 0;
	ActorPackageName.Append(InBaseDir);
	ActorPackageName.Append(TEXT("/"));

	switch (ActorPackagingScheme)
	{
	case EActorPackagingScheme::Original:
		ActorPackageName.Append(*GuidBase36, 2);
		FilenameOffset = 2;
		break;
	case EActorPackagingScheme::Reduced:
		ActorPackageName.Append(*GuidBase36, 1);
		FilenameOffset = 1;
		break;
	}

	ActorPackageName.Append(TEXT("/"));
	ActorPackageName.Append(*GuidBase36 + FilenameOffset, 2);
	ActorPackageName.Append(TEXT("/"));
	ActorPackageName.Append(*GuidBase36 + FilenameOffset + 2);

	return ActorPackageName.ToString();
}

TArray<FString> ULevel::GetExternalActorsPaths(const FString& InLevelPackageName, const FString& InPackageShortName)
{
	TArray<FString> Result;
	Result.Add(ULevel::GetExternalActorsPath(InLevelPackageName, InPackageShortName));
	for (const FLevelExternalActorsPathsProviderDelegate& Provider : ULevel::LevelExternalActorsPathsProviders)
	{
		Provider.ExecuteIfBound(InLevelPackageName, InPackageShortName, Result);
	}
	return Result;
}

FDelegateHandle ULevel::RegisterLevelExternalActorsPathsProvider(const FLevelExternalActorsPathsProviderDelegate& Provider)
{
	LevelExternalActorsPathsProviders.Add(Provider);
	return LevelExternalActorsPathsProviders.Last().GetHandle();
}

void ULevel::UnregisterLevelExternalActorsPathsProvider(const FDelegateHandle& ProviderDelegateHandle)
{
	LevelExternalActorsPathsProviders.RemoveAll([ProviderDelegateHandle](const FLevelExternalActorsPathsProviderDelegate& Delegate)
	{
		return Delegate.GetHandle() == ProviderDelegateHandle;
	});
}

FDelegateHandle ULevel::RegisterLevelMountPointResolver(const FLevelMountPointResolverDelegate& Resolver)
{
	LevelMountPointResolvers.Add(Resolver);
	return LevelMountPointResolvers.Last().GetHandle();
}

void ULevel::UnregisterLevelMountPointResolver(const FDelegateHandle& ResolverDelegateHandle)
{
	LevelMountPointResolvers.RemoveAll([ResolverDelegateHandle](const FLevelMountPointResolverDelegate& Delegate)
	{
		return Delegate.GetHandle() == ResolverDelegateHandle;
	});
}
FString ULevel::GetExternalActorsPath(const FString& InLevelPackageName, const FString& InPackageShortName)
{
	// Strip the temp prefix if found
	FString ExternalActorsPath;

	auto TrySplitLongPackageName = [&InPackageShortName](const FString& InLevelPackageName, FString& OutExternalActorsPath)
	{
		FString MountPoint, PackagePath, ShortName;
		if (FPackageName::SplitLongPackageName(InLevelPackageName, MountPoint, PackagePath, ShortName))
		{
			OutExternalActorsPath = FString::Printf(TEXT("%s%s/%s%s"), *MountPoint, GetExternalActorsFolderName(), *PackagePath, InPackageShortName.IsEmpty() ? *ShortName : *InPackageShortName);
			return true;
		}
		return false;
	};

	// This exists to support the Fortnite Foundation level streaming and Level Instances which prefix a valid package with /Temp (/Temp/Game/...)
	// Unsaved worlds also have a /Temp prefix but no other mount point in their paths and they should fallback to not stripping the prefix. (first call to SplitLongPackageName will fail and second will succeed)
	if (InLevelPackageName.StartsWith(TEXT("/Temp")))
	{
		FString BaseLevelPackageName = InLevelPackageName.Mid(5);
		if (TrySplitLongPackageName(BaseLevelPackageName, ExternalActorsPath))
		{
			return ExternalActorsPath;
		}
	}

	if (TrySplitLongPackageName(InLevelPackageName, ExternalActorsPath))
	{
		return ExternalActorsPath;
	}

	return FString();
}

FString ULevel::GetExternalActorsPath(UPackage* InLevelPackage, const FString& InPackageShortName)
{
	check(InLevelPackage);

	// We can't use the Package->FileName here because it might be a duplicated a package
	// We can't use the package short name directly in some cases either (PIE, instanced load) as it may contain pie prefix or not reflect the real actor location
	return ULevel::GetExternalActorsPath(InLevelPackage->GetName(), InPackageShortName);
}

EActorPackagingScheme ULevel::GetActorPackagingSchemeFromActorPackageName(const FStringView InActorPackageName)
{
	// Use the fact that the end of an actor package path is a GUID that is encoded in a base36 and those are always 25 character long to determine the PackagingScheme.
	FStringView ActorBaseFilename = FPathViews::GetBaseFilename(InActorPackageName);

	if (ActorBaseFilename.Len() == 22)
	{
		return EActorPackagingScheme::Reduced;
	}

	check(ActorBaseFilename.Len() == 21);

	return EActorPackagingScheme::Original;
}

void ULevel::ScanLevelAssets(const FString& InLevelPackageName)
{
	LevelAssetRegistryHelper::ScanLevelAssets(InLevelPackageName);
}

const TCHAR* ULevel::GetExternalActorsFolderName()
{
	return FPackagePath::GetExternalActorsFolderName();
}

bool ULevel::IsUsingExternalActors() const
{
	return bUseExternalActors;
}

void ULevel::SetUseExternalActors(bool bEnable)
{
	bUseExternalActors = bEnable;
	UPackage* LevelPackage = GetPackage();
	if (bEnable)
	{
		LevelPackage->SetPackageFlags(PKG_DynamicImports);
	}
	else
	{
		LevelPackage->ClearPackageFlags(PKG_DynamicImports);
	}
	CreateOrUpdateActorFolders();
}

TArray<FString> ULevel::GetExternalObjectsPaths(const FString& InLevelPackageName, const FString& InPackageShortName)
{
	TArray<FString> Paths;
	Paths.Append(ULevel::GetExternalActorsPaths(InLevelPackageName, InPackageShortName));
	Paths.Add(FExternalPackageHelper::GetExternalObjectsPath(InLevelPackageName, InPackageShortName));
	return Paths;
}

bool ULevel::IsUsingExternalObjects() const
{
	return IsUsingExternalActors();
}

static UActorFolder* FindNextFolder(const TMap<FString, FActorFolderSet>& InFolderLabelToActorFolders, const TArray<FString>& InFolderLabels, int32 Index, UActorFolder* ParentFolder)
{
	if (const FActorFolderSet* FoundSet = InFolderLabelToActorFolders.Find(InFolderLabels[Index]))
	{
		for (const TObjectPtr<UActorFolder>& ActorFolder : FoundSet->GetActorFolders())
		{
			if (ActorFolder->GetParent() == ParentFolder)
			{
				int32 NextIndex = Index + 1;
				check(NextIndex <= InFolderLabels.Num());

				if (NextIndex == InFolderLabels.Num())
				{
					return ActorFolder;
				}
				if (UActorFolder* Found = FindNextFolder(InFolderLabelToActorFolders, InFolderLabels, NextIndex, ActorFolder))
				{
					return Found;
				}
			}
		}
	}
	return nullptr;
}

UActorFolder* ULevel::GetActorFolder(const FName& InPath) const
{
	if (!InPath.IsNone() && IsUsingActorFolders())
	{
		TArray<FString> FolderLabels;
		FString Path = FPaths::RemoveDuplicateSlashes(InPath.ToString());
		Path.ParseIntoArray(FolderLabels, TEXT("/"));
		UActorFolder* CurrentFolder = (FolderLabels.Num() > 0) ? FindNextFolder(FolderLabelToActorFolders, FolderLabels, 0, nullptr) : nullptr;
		return CurrentFolder;
	}
	return nullptr;
}

UActorFolder* ULevel::GetActorFolder(const FGuid& InGuid, bool bSkipDeleted) const
{
	if (IsUsingActorFolders() && InGuid.IsValid())
	{
		if (const TObjectPtr<UActorFolder>* FoundActorFolder = ActorFolders.Find(InGuid))
		{
			UActorFolder* ActorFolder = *FoundActorFolder;
			if (bSkipDeleted && ActorFolder->IsMarkedAsDeleted())
			{
				return ActorFolder->GetParent();
			}
			return ActorFolder;
		}
	}
	return nullptr;
}

bool ULevel::IsUsingActorFolders() const
{
	return bUseActorFolders;
}

bool ULevel::SetUseActorFolders(bool bInEnabled, bool bInInteractiveMode)
{
	if (bUseActorFolders == bInEnabled)
	{
		return false;
	}

	const bool bInteractiveMode = bInInteractiveMode && !IsRunningCommandlet();

	if (!bInEnabled)
	{
		UE_LOG(LogLevel, Warning, TEXT("Disabling actor folder objects is not supported."));
		if (bInteractiveMode)
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("DisableActorFoldersNotSupported", "Disabling actor folder objects is not supported."));
		}
		return false;
	}

	if (GetWorldPartition())
	{
		UClass* WorldPartitionBuilderCommandletClass = FindObject<UClass>(nullptr, TEXT("/Script/UnrealEd.WorldPartitionBuilderCommandlet"), true);
		const bool bIsRunningWorldPartitionBuilderCommandlet = WorldPartitionBuilderCommandletClass && GetRunningCommandletClass() && GetRunningCommandletClass()->IsChildOf(WorldPartitionBuilderCommandletClass);
		if (!bIsRunningWorldPartitionBuilderCommandlet)
		{
			if (bInteractiveMode)
			{
				FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("UseCommandletForActorFoldersOnWorldPartition", "Enabling actor folder objects on a partitioned world requires using the WorldPartitionBuilderCommandlet with the WorldPartitionResaveActorsBuilder (see log for details)."));
			}
			UE_LOG(LogLevel, Warning, TEXT("To enable actor folder objects, run WorldPartitionBuilderCommandlet with these options: '-Builder=WorldPartitionResaveActorsBuilder -EnableActorFolders'"));
			return false;
		}
	}

	// Validate we have a saved map
	UPackage* LevelPackage = GetOutermost();
	if (LevelPackage == GetTransientPackage()
		|| LevelPackage->HasAnyFlags(RF_Transient)
		|| !FPackageName::IsValidLongPackageName(LevelPackage->GetName()))
	{
		if (bInteractiveMode)
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("UseActorFoldersSaveMap", "You need to save the level before enabling the `Use Actor Folder Objects` option."));
		}
		UE_LOG(LogLevel, Warning, TEXT("You need to save the level before enabling the `Use Actor Folder Objects` option."));
		return false;
	}

	if (bInteractiveMode)
	{
		FText MessageTitle(LOCTEXT("ConvertActorFolderDialog", "Convert Actor Folders"));
		FText Message(LOCTEXT("ConvertActorFoldersToActorFoldersMsg", "Do you want to convert all actors with a folder to use a actor folder objects?"));
		EAppReturnType::Type ConvertAnswer = FMessageDialog::Open(EAppMsgType::YesNo, Message, MessageTitle);
		if (ConvertAnswer != EAppReturnType::Yes)
		{
			return false;
		}
	}

	SetUseActorFoldersInternal(bInEnabled);
	// Operation cannot be undone
	GEditor->ResetTransaction(LOCTEXT("LevelUseActorFolderObjectsResetTrans", "Level Use Actor Folder Objects"));

	return true;
}

void ULevel::SetUseActorFoldersInternal(bool bInEnabled)
{
	Modify();
	bUseActorFolders = bInEnabled;
	CreateOrUpdateActorFolders();
}

void ULevel::CreateOrUpdateActorFolders()
{
	if (!IsUsingActorFolders())
	{
		return;
	}

	const bool bHadActorFolders = !ActorFolders.IsEmpty();

	// Here we also do a cleanup of all actor folders marked as deleted.
	// Find and fixup actors and folders that are referencing them.
	// Also update actor folders packaging mode.
	TArray<UActorFolder*> ActorFoldersToDelete;
	const bool bShouldUsePackageExternal = IsUsingExternalObjects();
	ForEachActorFolder([&ActorFoldersToDelete, bShouldUsePackageExternal](UActorFolder* ActorFolder)
	{
		ActorFolder->Fixup();
		if (ActorFolder->IsMarkedAsDeleted())
		{
			ActorFoldersToDelete.Add(ActorFolder);
		}
		// When embedding, still call SetPackageExternal(false) on actor folder marked as deleted to empty the package
		if (!ActorFolder->IsMarkedAsDeleted() || !bShouldUsePackageExternal)
		{
			ActorFolder->SetPackageExternal(bShouldUsePackageExternal);
		}
		return true;
	});

	// Create necessary actor folders
	for (AActor* Actor : Actors)
	{
		if (IsValid(Actor))
		{
			Actor->CreateOrUpdateActorFolder();
		}
	}

	// Remove actor folders marked as deleted
	for (UActorFolder* ActorFolderToDelete : ActorFoldersToDelete)
	{
		RemoveActorFolder(ActorFolderToDelete);
	}

	// Avoid broadcasting if no actor folder were/are part of this level
	const bool bHasActorFolders = !ActorFolders.IsEmpty();
	if (bHadActorFolders || bHasActorFolders)
	{
		GEngine->BroadcastActorFoldersUpdated(this);
	}
}

TSet<FGuid> ULevel::GetDeletedAndUnreferencedActorFolders() const
{
	TSet<FGuid> FoldersToDelete;

	if (!IsUsingActorFolders())
	{
		return FoldersToDelete;
	}

	TSet<FGuid> RootFoldersToDelete;
	for (const auto& Pair : ActorFolders)
	{
		UActorFolder* ActorFolder = Pair.Value;
		if (ActorFolder->IsMarkedAsDeleted())
		{
			if (!ActorFolder->GetParent())
			{
				RootFoldersToDelete.Add(ActorFolder->GetGuid());
			}
			else
			{
				FoldersToDelete.Add(ActorFolder->GetGuid());
			}
		}
	}

	// Remove all other folders that are still referenced by Actors or other Folders
	for (const auto& Pair : ActorFolders)
	{
		UActorFolder* ActorFolder = Pair.Value;
		UActorFolder* ParentFolder = ActorFolder->GetParent(false);
		if (ParentFolder)
		{
			FoldersToDelete.Remove(ParentFolder->GetGuid());
			if (FoldersToDelete.IsEmpty())
			{
				break;
			}
		}
	}

	if (UWorldPartition* WorldPartition = GetWorldPartition())
	{
		FWorldPartitionHelpers::ForEachActorDescInstance(WorldPartition, [&FoldersToDelete](const FWorldPartitionActorDescInstance* ActorDescInstance)
		{
			AActor* Actor = ActorDescInstance->GetActor();
			FoldersToDelete.Remove(Actor ? Actor->GetFolderGuid() : ActorDescInstance->GetFolderGuid());
			return !FoldersToDelete.IsEmpty();
		});
	}

	// Also loop through all loaded level actors
	for (AActor* Actor : Actors)
	{
		if (IsValid(Actor))
		{
			FoldersToDelete.Remove(Actor->GetFolderGuid());
			if (FoldersToDelete.IsEmpty())
			{
				break;
			}
		}
	}

	// Allow folders that don't have parent folders to be deleted as it won't affect child actors/folders (they are already at the root)
	FoldersToDelete.Append(RootFoldersToDelete);
	return FoldersToDelete;
}

void ULevel::CleanupDeletedAndUnreferencedActorFolders()
{
	// Remove actor folders marked as deleted
	for (const FGuid& GuidFolderToDelete : GetDeletedAndUnreferencedActorFolders())
	{
		if (UActorFolder* ActorFolderToDelete = GetActorFolder(GuidFolderToDelete, false /*bSkipDeleted*/))
		{
			RemoveActorFolder(ActorFolderToDelete);
		}
	}
}

void ULevel::ForEachActorFolder(TFunctionRef<bool(UActorFolder*)> Operation, bool bSkipDeleted)
{
	if (!IsUsingActorFolders())
	{
		return;
	}

	for (const auto& Pair : ActorFolders)
	{
		UActorFolder* ActorFolder = Pair.Value;
		if (!bSkipDeleted || !ActorFolder->IsMarkedAsDeleted())
		{
			if (!Operation(ActorFolder))
			{
				break;
			}
		}
	}
}

bool ULevel::ShouldCreateNewExternalActors() const
{
	return IsUsingExternalActors() && !GetPackage()->HasAnyPackageFlags(PKG_PlayInEditor);
}

void ULevel::ConvertAllActorsToPackaging(bool bExternal)
{
	SetUseExternalActors(bExternal);

	if (bExternal)
	{
		// always force levels to use the reduced packaging scheme
		ActorPackagingScheme = EActorPackagingScheme::Reduced;
	}

	// Make a copy of the current actor lists since packaging conversion may modify the actor list as a side effect
	TArray<AActor*> CurrentActors = ObjectPtrDecay(Actors);
	for (AActor* Actor : CurrentActors)
	{
		if (Actor && Actor->SupportsExternalPackaging())
		{
			check(Actor->GetLevel() == this);
			Actor->SetPackageExternal(bExternal);
		}
	}
}

FString ULevel::GetExternalActorPackageInstanceName(const FString& LevelPackageName, const FString& ActorPackageName)
{
	return FLinkerInstancingContext::GetInstancedPackageName(LevelPackageName, ActorPackageName);
}

TArray<FString> ULevel::GetOnDiskExternalActorPackages(const FString& ExternalActorsPath)
{
	TArray<FString> ActorPackageNames;
	if (!ExternalActorsPath.IsEmpty())
	{
		FARFilter Filter;
		Filter.bIncludeOnlyOnDiskAssets = true;
		Filter.PackagePaths.Add(*ExternalActorsPath);
		Filter.bRecursivePaths = true;
	
		TArray<FAssetData> ActorAssets;
		IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		AssetRegistry.ScanSynchronous({ ExternalActorsPath }, TArray<FString>());
		FExternalPackageHelper::GetSortedAssets(Filter, ActorAssets);

		ActorPackageNames.Reserve(ActorAssets.Num());
		Algo::Transform(ActorAssets, ActorPackageNames, [](const FAssetData& ActorAssetData) { return ActorAssetData.PackageName.ToString(); });
	}

	return ActorPackageNames;
}

TArray<FString> ULevel::GetOnDiskExternalActorPackages(bool bTryUsingPackageLoadedPath) const
{
	UWorld* World = GetTypedOuter<UWorld>();
	FString LevelPackageName;
	FString LevelPackageShortName;
	if (bTryUsingPackageLoadedPath && !World->GetPackage()->GetLoadedPath().IsEmpty())
	{
		LevelPackageName = World->GetPackage()->GetLoadedPath().GetPackageName();
	}
	else
	{
		LevelPackageName = World->GetPackage()->GetName();
		LevelPackageShortName = !World->OriginalWorldName.IsNone() ? World->OriginalWorldName.ToString() : World->GetName();
	}
	TArray<FString> ActorPackagePaths;
	for (const FString& ExternalActorsPath : ULevel::GetExternalActorsPaths(LevelPackageName, LevelPackageShortName))
	{
		ActorPackagePaths.Append(GetOnDiskExternalActorPackages(ExternalActorsPath));
	}
	return ActorPackagePaths;
}

TArray<UPackage*> ULevel::GetLoadedExternalObjectPackages() const
{
	// We also need to provide empty packages (for example, actors that were converted to non-external)
	UWorld* World = GetTypedOuter<UWorld>();
	if (!ensure(IsValid(World)))
	{
		return {};
	}

	TSet<UPackage*> ExternalObjectPackages;

	// Get external packages (including deleted actors)
	ExternalObjectPackages.Append(GetPackage()->GetExternalPackages());

	// Make sure we filter out similar external folders
	auto SanitizeExternalPath = [](const FString& InPath)
	{
		if (!InPath.IsEmpty())
		{
			if (!InPath.EndsWith("/"))
			{
				return InPath + "/";
			}
		}
		return InPath;
	};

	TSet<FString> ExternalObjectsPathSet;
	const FString LevelPackageShortName = !World->OriginalWorldName.IsNone() ? World->OriginalWorldName.ToString() : World->GetName();
	ExternalObjectsPathSet.Add(SanitizeExternalPath(ULevel::GetExternalActorsPath(World->GetPackage(), LevelPackageShortName)));
	ExternalObjectsPathSet.Add(SanitizeExternalPath(FExternalPackageHelper::GetExternalObjectsPath(World->GetPackage(), LevelPackageShortName)));

	if (UWorldPartition* WorldPartition = GetWorldPartition())
	{
		WorldPartition->ForEachActorDescContainerInstance([&SanitizeExternalPath, &ExternalObjectsPathSet](UActorDescContainerInstance* ActorDescContainerInstance)
		{
			ExternalObjectsPathSet.Add(SanitizeExternalPath(ActorDescContainerInstance->GetExternalActorPath()));
			ExternalObjectsPathSet.Add(SanitizeExternalPath(ActorDescContainerInstance->GetExternalObjectPath()));
		});
	}

	TArray<FString> ExternalObjectsPaths = ExternalObjectsPathSet.Array().FilterByPredicate([](const FString& Path) {
			FString Filename;
			if (!Path.IsEmpty() && FPackageName::TryConvertLongPackageNameToFilename(Path, Filename))
			{
				return FPaths::DirectoryExists(Filename);
			}
			return false;
		});

	if (!ExternalObjectsPaths.IsEmpty())
	{
		TArray<UObject*> Packages;
		GetObjectsOfClass(UPackage::StaticClass(), Packages, /*bIncludeDerivedClasses =*/ true, /*EObjectFlags ExcludeFlags =*/ RF_ClassDefaultObject,/*EInternalObjectFlags ExclusionInternalFlags =*/ EInternalObjectFlags::None);

		TArray<bool> PackageIsInExternalObjectsPath;
		PackageIsInExternalObjectsPath.InsertUninitialized(0, Packages.Num());

		ParallelFor(Packages.Num(), [&Packages = std::as_const(Packages), &ExternalObjectsPaths = std::as_const(ExternalObjectsPaths), &PackageIsInExternalObjectsPath](int32 Index) {
			UPackage* Package = static_cast<UPackage*>(Packages[Index]);

			TStringBuilder<256> PackageName;
			Package->GetLoadedPath().AppendPackageName(PackageName);
			FStringView PackageNameStringView(PackageName);
			bool bIsInExternalObjectsPath = false;
			for (const FString& ExternalObjectsPath : ExternalObjectsPaths)
			{
				bIsInExternalObjectsPath = PackageNameStringView.Contains(ExternalObjectsPath);
				if (bIsInExternalObjectsPath)
				{
					break;
				}
			}
			PackageIsInExternalObjectsPath[Index] = bIsInExternalObjectsPath;
		});

		for (int Index=0; Index<PackageIsInExternalObjectsPath.Num(); Index++)
		{
			if (PackageIsInExternalObjectsPath[Index])
			{
				ExternalObjectPackages.Add(static_cast<UPackage*>(Packages[Index]));
			}
		}
	}
	return ExternalObjectPackages.Array();
}

static UPackage* CreateActorPackageInternal(const FString& InPackageName, const FString& InActorPath)
{
	if (UPackage* ExistingActorPackage = FindObject<UPackage>(nullptr, *InPackageName))
	{
		check(ExistingActorPackage->HasAllPackagesFlags(PKG_EditorOnly | PKG_ContainsMapData));
		return ExistingActorPackage;
	}

	UPackage* ActorPackage = CreatePackage(*InPackageName);
	ActorPackage->SetPackageFlags(PKG_EditorOnly | PKG_ContainsMapData | PKG_NewlyCreated);

	return ActorPackage;
};

UPackage* ULevel::CreateActorPackage(UPackage* InLevelPackage, EActorPackagingScheme InActorPackagingScheme, const FString& InActorPath, const UObject* InTargetContextObject)
{
	const FString PackageName = ULevel::GetActorPackageName(InLevelPackage, InActorPackagingScheme, InActorPath, InTargetContextObject);
	UPackage* ActorPackage = CreateActorPackageInternal(PackageName, InActorPath);
	// Should be prevented upstream but we propagate the flag to prevent issues in asset enumeration
	if (!ensureMsgf(!(InLevelPackage->GetPackageFlags() & PKG_PlayInEditor), TEXT("Actor packages should not be created on PlayInEditor levels")))
	{
		ActorPackage->SetPackageFlags(PKG_PlayInEditor);
	}
	return ActorPackage;
}

void ULevel::DetachAttachAllActorsPackages(bool bReattach)
{
	if (bReattach)
	{
		for (AActor* Actor : Actors)
		{
			if (Actor)
			{
				Actor->ReattachExternalPackage();
			}
		}

		// Reouter objects previously found in the actors packages to their original packages
		for (const TPair<TObjectPtr<UObject>, TObjectPtr<UPackage>>& ObjectToPackage : ObjectsToExternalPackages)
		{
			UObject* Object = ObjectToPackage.Key;
			UPackage* Package = ObjectToPackage.Value;

			Object->Rename(nullptr, Package, REN_ForceNoResetLoaders);
		}

		ObjectsToExternalPackages.Empty();
	}
	else
	{
		UPackage* LevelPackage = GetPackage();

		check(ObjectsToExternalPackages.IsEmpty());

		for (AActor* Actor : Actors)
		{
			if (Actor)
			{
				if (UPackage* ActorExternalPackage = Actor->GetExternalPackage())
				{
					Actor->DetachExternalPackage();

					// Process objects found in the source actor package
					TArray<UObject*> Objects;
					const bool bIncludeNestedSubobjects = false;

					GetObjectsWithPackage(ActorExternalPackage, Objects, bIncludeNestedSubobjects, RF_NoFlags, EInternalObjectFlags::Garbage);
					for (UObject* Object : Objects)
					{
						if (Object != Actor && Object->GetFName() != NAME_PackageMetaData)
						{
							// Move objects in the destination level package
							Object->Rename(nullptr, LevelPackage, REN_ForceNoResetLoaders);

							// Keep track of which package this object really belongs to
							ObjectsToExternalPackages.Emplace(Object, ActorExternalPackage);
						}
					}
				}
			}
		}
	}
}

void ULevel::OnApplyNewLightingData(bool bLightingSuccessful)
{
	// Store level offset that was used during static light data build
	// This will be used to find correct world position of precomputed lighting samples during origin rebasing
	LightBuildLevelOffset = FIntVector::ZeroValue;
	if (bLightingSuccessful && OwningWorld && OwningWorld->WorldComposition)
	{
		LightBuildLevelOffset = OwningWorld->WorldComposition->GetLevelOffset(this);
	}
}

TArray<UBlueprint*> ULevel::GetLevelBlueprints() const
{
	TArray<UBlueprint*> LevelBlueprints;

	ForEachObjectWithOuter(this, [&LevelBlueprints](UObject* LevelChild)
	{
		UBlueprint* LevelChildBP = Cast<UBlueprint>(LevelChild);
		if (LevelChildBP)
		{
			LevelBlueprints.Add(LevelChildBP);
		}
	}, false, RF_NoFlags, EInternalObjectFlags::Garbage);

	return LevelBlueprints;
}

ULevelScriptBlueprint* ULevel::GetLevelScriptBlueprint(bool bDontCreate)
{
	const FString LevelScriptName = ULevelScriptBlueprint::CreateLevelScriptNameFromLevel(this);
	if( !LevelScriptBlueprint && !bDontCreate)
	{
		// The level blueprint must be named the same as the level/world.
		// If there is already something there with that name, rename it to something else.
		if (UObject* ExistingObject = StaticFindObject(nullptr, this, *LevelScriptName))
		{
			ExistingObject->Rename(nullptr, nullptr, REN_DoNotDirty | REN_DontCreateRedirectors | REN_ForceNoResetLoaders | REN_NonTransactional);
		}

		// If no blueprint is found, create one. 
		LevelScriptBlueprint = Cast<ULevelScriptBlueprint>(FKismetEditorUtilities::CreateBlueprint(GEngine->LevelScriptActorClass, this, FName(*LevelScriptName), BPTYPE_LevelScript, ULevelScriptBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass()));

		// LevelScript blueprints should not be standalone
		LevelScriptBlueprint->ClearFlags(RF_Standalone);
		ULevel::LevelDirtiedEvent.Broadcast();
		// Refresh level script actions
		FWorldDelegates::RefreshLevelScriptActions.Broadcast(OwningWorld);
	}

	// Ensure that friendly name is always up-to-date
	if (LevelScriptBlueprint)
	{
		LevelScriptBlueprint->FriendlyName = LevelScriptName;
	}

	return LevelScriptBlueprint;
}

void ULevel::CleanupLevelScriptBlueprint()
{
	if( LevelScriptBlueprint )
	{
		if( LevelScriptBlueprint->SkeletonGeneratedClass )
		{
			LevelScriptBlueprint->SkeletonGeneratedClass->ClassGeneratedBy = nullptr; 
		}

		if( LevelScriptBlueprint->GeneratedClass )
		{
			LevelScriptBlueprint->GeneratedClass->ClassGeneratedBy = nullptr; 
		}
	}
}

void ULevel::OnLevelScriptBlueprintChanged(ULevelScriptBlueprint* InBlueprint)
{
	if (!InBlueprint->bIsRegeneratingOnLoad &&
		ensureMsgf(InBlueprint == LevelScriptBlueprint, TEXT("Level ('%s') received OnLevelScriptBlueprintChanged   for the wrong Blueprint ('%s')."), LevelScriptBlueprint ? *LevelScriptBlueprint->GetPathName() : TEXT("NULL"), *InBlueprint->GetPathName()))
	{	    
		RegenerateLevelScriptActor();
	}
}	

void ULevel::BeginCacheForCookedPlatformData(const ITargetPlatform *TargetPlatform)
{
	Super::BeginCacheForCookedPlatformData(TargetPlatform);

	// Cook the level blueprint.
	if (ULevelScriptBlueprint* LevelBlueprint = GetLevelScriptBlueprint(true))
	{
		LevelBlueprint->BeginCacheForCookedPlatformData(TargetPlatform);
	}
}

void ULevel::FixupForPIE(int32 InPIEInstanceID, TFunctionRef<void(int32, FSoftObjectPath&)> InCustomFixupFunction)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ULevel::FixupForPIE);
	FPIEFixupSerializer FixupSerializer(this, InPIEInstanceID, InCustomFixupFunction);
	Serialize(FixupSerializer);
}

void ULevel::FixupForPIE(int32 InPIEInstanceID)
{
	FixupForPIE(InPIEInstanceID, [](int32, FSoftObjectPath&) {});
}

#endif	//WITH_EDITOR

bool ULevel::ResolveSubobject(const TCHAR* SubObjectPath, UObject*& OutObject, bool bLoadIfExists)
{
	// First check if we can find the sub object through relative path of this level
	OutObject = StaticFindObject(nullptr, this, SubObjectPath);
	if (OutObject)
	{
		return true;
	}

	// Then check if we can resolve through a top level actor (Editor Path)
	FString SubObjectName;
	FString SubObjectContext(SubObjectPath);
	if (FString(SubObjectPath).Split(TEXT("."), &SubObjectContext, &SubObjectName))
	{
		if (UObject* SubObject = StaticFindObject(nullptr, this, *SubObjectContext))
		{
			return SubObject->ResolveSubobject(*SubObjectName, OutObject, bLoadIfExists);
		}
	}
	
	// Lastly forward to world partition
	if (UWorldPartition* WorldPartition = GetWorldPartition())
	{
		return WorldPartition->ResolveSubobject(SubObjectPath, OutObject, bLoadIfExists);
	}
	
	return false;
}

bool ULevel::IsPersistentLevel() const
{
	bool bIsPersistent = false;
	if( OwningWorld )
	{
		bIsPersistent = (this == OwningWorld->PersistentLevel);
	}
	return bIsPersistent;
}

bool ULevel::IsCurrentLevel() const
{
	bool bIsCurrent = false;
	if( OwningWorld )
	{
		bIsCurrent = (this == OwningWorld->GetCurrentLevel());
	}
	return bIsCurrent;
}

bool ULevel::IsInstancedLevel() const
{
	const UWorld* OuterWorld = GetTypedOuter<UWorld>();
	return OuterWorld && OuterWorld->IsInstanced();
}

void ULevel::ApplyWorldOffset(const FVector& InWorldOffset, bool bWorldShift)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_ULevel_ApplyWorldOffset);

	// Move precomputed light samples
	if (PrecomputedLightVolume && !InWorldOffset.IsZero())
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_ULevel_ApplyWorldOffset_PrecomputedLightVolume);
		
		if (!PrecomputedLightVolume->IsAddedToScene())
		{
			// When we add level to world, move precomputed lighting data taking into account position of level at time when lighting was built  
			if (bIsAssociatingLevel)
			{
				FVector PrecomputedLightVolumeOffset = InWorldOffset - FVector(LightBuildLevelOffset);
				PrecomputedLightVolume->ApplyWorldOffset(PrecomputedLightVolumeOffset);
			}
		}
		// At world origin rebasing all registered volumes will be moved during FScene shifting
		// Otherwise we need to send a command to move just this volume
		else if (!bWorldShift) 
		{
			FPrecomputedLightVolume* InPrecomputedLightVolume = PrecomputedLightVolume;
			ENQUEUE_RENDER_COMMAND(ApplyWorldOffset_PLV)(
				[InPrecomputedLightVolume, InWorldOffset](FRHICommandListImmediate& RHICmdList)
	 			{
					InPrecomputedLightVolume->ApplyWorldOffset(InWorldOffset);
 				});
		}
	}

	if (PrecomputedVolumetricLightmap && !InWorldOffset.IsZero())
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_ULevel_ApplyWorldOffset_PrecomputedLightVolume);
		
		if (!PrecomputedVolumetricLightmap->IsAddedToScene())
		{
			// When we add level to world, move precomputed lighting data taking into account position of level at time when lighting was built  
			if (bIsAssociatingLevel)
			{
				FVector PrecomputedVolumetricLightmapOffset = InWorldOffset - FVector(LightBuildLevelOffset);
				PrecomputedVolumetricLightmap->ApplyWorldOffset(PrecomputedVolumetricLightmapOffset);
			}
		}
		// At world origin rebasing all registered volumes will be moved during FScene shifting
		// Otherwise we need to send a command to move just this volume
		else if (!bWorldShift) 
		{
			FPrecomputedVolumetricLightmap* InPrecomputedVolumetricLightmap = PrecomputedVolumetricLightmap;
			ENQUEUE_RENDER_COMMAND(ApplyWorldOffset_PLV)(
				[InPrecomputedVolumetricLightmap, InWorldOffset](FRHICommandListImmediate& RHICmdList)
	 			{
					InPrecomputedVolumetricLightmap->ApplyWorldOffset(InWorldOffset);
 				});
		}
	}

	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_ULevel_ApplyWorldOffset_Actors);
		// Iterate over all actors in the level and move them
		for (int32 ActorIndex = 0; ActorIndex < Actors.Num(); ActorIndex++)
		{
			AActor* Actor = Actors[ActorIndex];
			if (Actor)
			{
				const FVector Offset = (bWorldShift && Actor->bIgnoresOriginShifting) ? FVector::ZeroVector : InWorldOffset;
				FScopeCycleCounterUObject Context(Actor);
				Actor->ApplyWorldOffset(Offset, bWorldShift);
			}
		}
	}
	
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_ULevel_ApplyWorldOffset_Model);
		// Move model geometry
		for (int32 CompIdx = 0; CompIdx < ModelComponents.Num(); ++CompIdx)
		{
			ModelComponents[CompIdx]->ApplyWorldOffset(InWorldOffset, bWorldShift);
		}
	}

	if (!InWorldOffset.IsZero()) 
	{
		// Notify streaming managers that level primitives were shifted
		IStreamingManager::Get().NotifyLevelOffset(this, InWorldOffset);		
	}
	
	FWorldDelegates::PostApplyLevelOffset.Broadcast(this, OwningWorld, InWorldOffset, bWorldShift);
}

void ULevel::RegisterActorForAutoReceiveInput(AActor* Actor, const int32 PlayerIndex)
{
	PendingAutoReceiveInputActors.Add(FPendingAutoReceiveInputActor(Actor, PlayerIndex));
}

void ULevel::PushPendingAutoReceiveInput(APlayerController* InPlayerController)
{
	check( InPlayerController );
	int32 PlayerIndex = -1;
	int32 Index = 0;
	for( FConstPlayerControllerIterator Iterator = InPlayerController->GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator )
	{
		APlayerController* PlayerController = Iterator->Get();
		if (InPlayerController == PlayerController)
		{
			PlayerIndex = Index;
			break;
		}
		Index++;
	}

	if (PlayerIndex >= 0)
	{
		TArray<AActor*> ActorsToAdd;
		for (int32 PendingIndex = PendingAutoReceiveInputActors.Num() - 1; PendingIndex >= 0; --PendingIndex)
		{
			FPendingAutoReceiveInputActor& PendingActor = PendingAutoReceiveInputActors[PendingIndex];
			if (PendingActor.PlayerIndex == PlayerIndex)
			{
				if (PendingActor.Actor.IsValid())
				{
					ActorsToAdd.Add(PendingActor.Actor.Get());
				}
				PendingAutoReceiveInputActors.RemoveAtSwap(PendingIndex);
			}
		}
		for (int32 ToAddIndex = ActorsToAdd.Num() - 1; ToAddIndex >= 0; --ToAddIndex)
		{
			APawn* PawnToPossess = Cast<APawn>(ActorsToAdd[ToAddIndex]);
			if (PawnToPossess)
			{
				InPlayerController->Possess(PawnToPossess);
			}
			else
			{
				ActorsToAdd[ToAddIndex]->EnableInput(InPlayerController);
			}
		}
	}
}

void ULevel::AddAssetUserData(UAssetUserData* InUserData)
{
	if(InUserData != NULL)
	{
		UAssetUserData* ExistingData = GetAssetUserDataOfClass(InUserData->GetClass());
		if(ExistingData != NULL)
		{
			AssetUserData.Remove(ExistingData);
		}
		AssetUserData.Add(InUserData);
	}
}

UAssetUserData* ULevel::GetAssetUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass)
{
	for(int32 DataIdx=0; DataIdx<AssetUserData.Num(); DataIdx++)
	{
		UAssetUserData* Datum = AssetUserData[DataIdx];
		if(Datum != NULL && Datum->IsA(InUserDataClass))
		{
			return Datum;
		}
	}
	return NULL;
}

void ULevel::RemoveUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass)
{
	for(int32 DataIdx=0; DataIdx<AssetUserData.Num(); DataIdx++)
	{
		UAssetUserData* Datum = AssetUserData[DataIdx];
		if(Datum != NULL && Datum->IsA(InUserDataClass))
		{
			AssetUserData.RemoveAt(DataIdx);
			return;
		}
	}
}

bool ULevel::HasVisibilityChangeRequestPending() const
{
	return (OwningWorld && ( this == OwningWorld->GetCurrentLevelPendingVisibility() || this == OwningWorld->GetCurrentLevelPendingInvisibility() ) );
}

#if WITH_EDITOR
void ULevel::RepairLevelScript()
{
	// Ensure bound events point to the level script actor
	if (LevelScriptActor)
	{
		if (ULevelScriptBlueprint* LevelBlueprint = Cast<ULevelScriptBlueprint>(LevelScriptActor->GetClass()->ClassGeneratedBy))
		{
			FBlueprintEditorUtils::FixLevelScriptActorBindings(LevelScriptActor, LevelBlueprint);
		}
	}

	// Show a toast notification to the user if there are multiple LSAs in this level (only for levels being actively edited)
	if (LevelScriptActor && !MultipleLSAsNotification.IsValid() &&
		OwningWorld && OwningWorld->WorldType == EWorldType::Editor)
	{
		TArray<ALevelScriptActor*> SiblingLSAs = LevelScriptActor->FindSiblingLevelScriptActors();

		if (!SiblingLSAs.IsEmpty())
		{
			UE_LOG(LogLevel, Error, TEXT("Detected more than one LevelScriptActor in map '%s'. This can lead to duplicate level blueprint operations during play."), *GetPathName());

			FNotificationInfo Info(LOCTEXT("MultipleLSAsPopupTitle", "Map Corruption: Multiple Level Script Actors"));
			Info.ExpireDuration = 10.0f;
			Info.bFireAndForget = true;
			Info.Image = FCoreStyle::Get().GetBrush(TEXT("MessageLog.Error"));

			Info.ButtonDetails.Add(FNotificationButtonInfo(LOCTEXT("MultipleLSAsPopupBeginRepair", "Repair Map"), LOCTEXT("MultipleLSAsBeginRepairTT", "Repairing the map by deleting extra LevelScriptActors"), FSimpleDelegate::CreateUObject(this, &ULevel::OnMultipleLSAsPopupClicked)));
			Info.ButtonDetails.Add(FNotificationButtonInfo(LOCTEXT("MultipleLSAsPopupDismiss", "Dismiss"), LOCTEXT("MultipleLSAsPopupDismissTT", "Dismiss this notification"), FSimpleDelegate::CreateUObject(this, &ULevel::OnMultipleLSAsPopupDismissed)));

			MultipleLSAsNotification = FSlateNotificationManager::Get().AddNotification(Info);
			MultipleLSAsNotification.Pin()->SetCompletionState(SNotificationItem::CS_Pending);
		}
	}

	// Catch the edge case where we have a level blueprint but have never created the LevelScriptActor based on it.
	//    This could happen if a new level is saved before the level blueprint is compiled.
	if (!LevelScriptActor && LevelScriptBlueprint && !LevelScriptBlueprint->bIsRegeneratingOnLoad &&
		OwningWorld && OwningWorld->IsEditorWorld() && !OwningWorld->IsGameWorld())
	{
		RegenerateLevelScriptActor();
	}
}

void ULevel::RegenerateLevelScriptActor()
{
	check(LevelScriptBlueprint);

	bool bResetDebugObject = false;

	UClass* SpawnClass = (LevelScriptBlueprint->GeneratedClass) ? LevelScriptBlueprint->GeneratedClass : LevelScriptBlueprint->SkeletonGeneratedClass;

	if (SpawnClass)
	{
		// Get rid of the old LevelScriptActor
		if (LevelScriptActor)
		{
			// Clear the current debug object and indicate that it needs to be reset (below).
			if (LevelScriptBlueprint->GetObjectBeingDebugged() == LevelScriptActor)
			{
				bResetDebugObject = true;
				LevelScriptBlueprint->SetObjectBeingDebugged(nullptr);
			}

			LevelScriptActor->MarkAsGarbage();
			LevelScriptActor = nullptr;
		}

		check(OwningWorld);
		// Create the new one
		FActorSpawnParameters SpawnInfo;
		SpawnInfo.OverrideLevel = this;
		LevelScriptActor = OwningWorld->SpawnActor<ALevelScriptActor>(SpawnClass, SpawnInfo);

		if (LevelScriptActor)
		{
			// Reset the current debug object to the new instance if it was previously set to the old instance.
			if (bResetDebugObject)
			{
				LevelScriptBlueprint->SetObjectBeingDebugged(LevelScriptActor);
			}

			LevelScriptActor->ClearFlags(RF_Transactional);
			check(LevelScriptActor->GetLevel() == this);
			// Finally, fixup all the bound events to point to their new LSA
			FBlueprintEditorUtils::FixLevelScriptActorBindings(LevelScriptActor, LevelScriptBlueprint);
		}
	}
	else
	{
		UE_LOG(LogLevel, Error, TEXT("Skipped regeneration of LevelScriptActor due to blueprint '%s' having no spawnable class. A probable cause is that the blueprint is deriving from an invalid class, and may not function."), *LevelScriptBlueprint->GetName());
	}
}

void ULevel::RemoveExtraLevelScriptActors()
{
	if (LevelScriptActor)
	{
		TArray<ALevelScriptActor*> ExtraLSAs = LevelScriptActor->FindSiblingLevelScriptActors();

		if (ExtraLSAs.Num() > 0)
		{
			if (UEditorActorSubsystem* EditorActorSubsystem = GEditor->GetEditorSubsystem<UEditorActorSubsystem>())
			{
				for (ALevelScriptActor* LSA : ExtraLSAs)
				{
					UE_LOG(LogLevel, Log, TEXT("Deleting extra LevelScriptActor: %s"), *LSA->GetPathName());
					EditorActorSubsystem->DestroyActor(LSA);
				}
			}
		}
	}
}

void ULevel::OnMultipleLSAsPopupClicked()
{
	RemoveExtraLevelScriptActors();
	OnMultipleLSAsPopupDismissed();
}

void ULevel::OnMultipleLSAsPopupDismissed()
{
	if (MultipleLSAsNotification.IsValid())
	{
		TSharedPtr<SNotificationItem> NotifPopup = MultipleLSAsNotification.Pin();

		NotifPopup->SetCompletionState(SNotificationItem::CS_None);
		NotifPopup->SetExpireDuration(0.0f);
		NotifPopup->SetFadeOutDuration(0.0f);
		NotifPopup->ExpireAndFadeout();

		MultipleLSAsNotification.Reset();
	}
}

#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE