// Copyright Epic Games, Inc. All Rights Reserved.

#include "LevelInstance/LevelInstanceLevelStreaming.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/LevelStreaming.h"
#include "LevelInstance/LevelInstanceInterface.h"
#include "LevelInstance/LevelInstancePrivate.h"
#include "LevelInstance/LevelInstanceSubsystem.h"
#include "Engine/LevelBounds.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/Paths.h"
#include "ProfilingDebugging/ScopedTimers.h"
#include "UObject/Linker.h"
#include "UObject/Package.h"
#include "WorldPartition/WorldPartition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LevelInstanceLevelStreaming)

#if WITH_EDITOR
#include "LevelInstance/LevelInstanceEditorInstanceActor.h"
#include "LevelUtils.h"
#include "ActorFolder.h"
#include "Misc/LazySingleton.h"
#include "Misc/ScopeExit.h"
#include "UObject/LinkerLoad.h"
#include "Engine/GameEngine.h"
#include "Streaming/LevelStreamingDelegates.h"

static bool GDisableLevelInstanceEditorPartialLoading = false;
FAutoConsoleVariableRef CVarDisableLevelInstanceEditorPartialLoading(
	TEXT("wp.Editor.DisableLevelInstanceEditorPartialLoading"),
	GDisableLevelInstanceEditorPartialLoading,
	TEXT("Allow disabling partial loading of level instances in the editor."),
	ECVF_Default);

static bool GForceEditorWorldMode = false;
FAutoConsoleVariableRef CVarForceEditorWorldMode(
	TEXT("LevelInstance.ForceEditorWorldMode"),
	GForceEditorWorldMode,
	TEXT("Allow -game instances to behave like an editor with temporary root object attached to instance. This will prevent HLOD from working in -game. This feature is only supported on non WP worlds."),
	ECVF_Default);


namespace FLevelInstanceLevelStreamingUtils
{
	static void MarkObjectsInPackageAsTransientAndNonTransactional(UPackage* InPackage)
	{
		InPackage->ClearFlags(RF_Transactional);
		InPackage->SetFlags(RF_Transient);
		ForEachObjectWithPackage(InPackage, [](UObject* Obj)
			{
				Obj->SetFlags(RF_Transient);
				Obj->ClearFlags(RF_Transactional);
				return true;
			}, true);
	}
}

#endif

ULevelStreamingLevelInstance::ULevelStreamingLevelInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
#if WITH_EDITOR
	, CachedBounds(ForceInit)
	, bResetLoadersCalled(false)
#endif
{
#if WITH_EDITOR
	SetShouldBeVisibleInEditor(true);
#endif
}

ILevelInstanceInterface* ULevelStreamingLevelInstance::GetLevelInstance() const
{
	if (ULevelInstanceSubsystem* LevelInstanceSubsystem = GetWorld()->GetSubsystem<ULevelInstanceSubsystem>())
	{
		return LevelInstanceSubsystem->GetLevelInstance(LevelInstanceID);
	}

	return nullptr;
}

#if WITH_EDITOR

bool ULevelStreamingLevelInstance::IsEditorWorldMode() const
{
	bool bCanSupportForceEditorWorldMode = !GIsEditor;
	if (GForceEditorWorldMode && !GIsEditor)
	{
		UWorld* OwningWorld = GetWorld();
		check(OwningWorld);
		// We do not support GForceEditorWorldMode in WP worlds.
		if (OwningWorld->GetWorldPartition() != nullptr)
		{
			bCanSupportForceEditorWorldMode = false;
		}
	}
	return (GForceEditorWorldMode && bCanSupportForceEditorWorldMode) || !GetWorld()->IsGameWorld();
}

TOptional<FFolder::FRootObject> ULevelStreamingLevelInstance::GetFolderRootObject() const
{
	if (ILevelInstanceInterface* LevelInstance = GetLevelInstance())
	{
		if (AActor* Actor = CastChecked<AActor>(LevelInstance))
		{
			return FFolder::FRootObject(Actor);
		}
	}
	// When LevelInstance is null, it's because the level instance is being streamed-out. 
	// Return the world as root object.
	return FFolder::GetWorldRootFolder(GetWorld()).GetRootObject();
}

FBox ULevelStreamingLevelInstance::GetBounds() const
{
	check(GetLoadedLevel());
	FTransform LevelInstanceTransform = Cast<AActor>(GetLevelInstance())->GetTransform();
	if (!CachedBounds.IsValid || !CachedTransform.Equals(LevelInstanceTransform))
	{
		CachedTransform = LevelInstanceTransform;
		CachedBounds = ALevelBounds::CalculateLevelBounds(GetLoadedLevel());
		
		// Possible if Level has no bounds relevant actors
		if (!CachedBounds.IsValid)
		{
			CachedBounds = FBox(CachedTransform.GetLocation(), CachedTransform.GetLocation());
		}
	}
	check(CachedBounds.IsValid);
	return CachedBounds;
}

void ULevelStreamingLevelInstance::OnLoadedActorsAddedToLevelPreEvent(const TArray<AActor*>& InActors)
{
	if (IsEditorWorldMode())
	{
		if (ILevelInstanceInterface* LevelInstance = GetLevelInstance())
		{
			for (AActor* Actor : InActors)
			{
				if (IsValid(Actor))
				{
					if (Actor->IsPackageExternal())
					{
						if (bResetLoadersCalled)
						{
							ResetLoaders(Actor->GetExternalPackage());
							ForEachObjectWithOuter(Actor, [](UObject* InObject)
							{
								if (InObject && InObject->IsPackageExternal())
								{
									ResetLoaders(InObject->GetExternalPackage());
								}
							}, /*bIncludeNestedObjects*/ true);
						}

						FLevelInstanceLevelStreamingUtils::MarkObjectsInPackageAsTransientAndNonTransactional(Actor->GetExternalPackage());
					}

					// Must happen before the actors are registered with the world, which is the case for this delegate.
					const FActorContainerID& ContainerID = LevelInstance->GetLevelInstanceID().GetContainerID();
					FSetActorInstanceGuid SetActorInstanceGuid(Actor, ContainerID.GetActorGuid(Actor->GetActorGuid()));

					FSetActorIsInLevelInstance SetIsInLevelInstance(Actor);
				}
			}
		}
	}
}

void ULevelStreamingLevelInstance::OnLoadedActorsAddedToLevelPostEvent(const TArray<AActor*>& InActors)
{
	if (IsEditorWorldMode())
	{
		if (GetLoadedLevel()->bAreComponentsCurrentlyRegistered)
		{
			if (ILevelInstanceInterface* LevelInstance = GetLevelInstance())
			{
				for (AActor* Actor : InActors)
				{
					if (IsValid(Actor))
					{
						Actor->PushSelectionToProxies();
						if (LevelInstance)
						{
							Actor->PushLevelInstanceEditingStateToProxies(CastChecked<AActor>(LevelInstance)->IsInEditLevelInstanceHierarchy());
						}

						if (LevelInstanceEditorInstanceActor.IsValid())
						{
							if (Actor->GetAttachParentActor() == nullptr && !Actor->IsChildActor())
							{
								Actor->AttachToActor(LevelInstanceEditorInstanceActor.Get(), FAttachmentTransformRules::KeepWorldTransform);
							}
						}
					}
				}
			}
		}
	}
}

void ULevelStreamingLevelInstance::ResetLevelInstanceLoaders()
{
	// @todo_ow: Ideally at some point it is no longer needed to ResetLoaders at all and Linker would not lock the package files preventing saves.
	if (bResetLoadersCalled)
	{
		return;
	}

	if (UWorld* OuterWorld = LoadedLevel ? LoadedLevel->GetTypedOuter<UWorld>() : nullptr)
	{
		UE_SCOPED_TIMER(*FString::Printf(TEXT("ULevelStreamingLevelInstance::ResetLevelInstanceLoaders(%s)"), *FPaths::GetBaseFilename(OuterWorld->GetPackage()->GetName())), LogLevelInstance, Log);

		FName PackageName = OuterWorld->GetPackage()->GetLoadedPath().GetPackageFName();
		if (!ULevel::GetIsLevelPartitionedFromPackage(PackageName))
		{
			ResetLoaders(OuterWorld->GetPackage());
		}
		else if(FLinkerLoad* LinkerLoad = OuterWorld->GetPackage()->GetLinker())
		{
			// Resetting the loader will prevent any OFPA package to properly re-load since their import level will fail to resolve.
			// DetachLoader allows releasing the lock on the file handle so level package can be saved.
			LinkerLoad->DetachLoader();
		}

		ForEachObjectWithOuter(LoadedLevel, [](UObject* InObject)
		{
			if (InObject && InObject->IsPackageExternal())
			{
				ResetLoaders(InObject->GetExternalPackage());
			}
		}, /*bIncludeNestedObjects*/ true);

		bResetLoadersCalled = true;
	}
}

void ULevelStreamingLevelInstance::OnLoadedActorsRemovedFromLevelPostEvent(const TArray<AActor*>& InActors)
{
	// Detach actor or else it will keep it alive (attachement keeps a reference to the actor)
	check(LevelInstanceEditorInstanceActor.IsValid());

	for (AActor* Actor : InActors)
	{
		if (IsValid(Actor))
		{
			if (Actor->GetAttachParentActor() == LevelInstanceEditorInstanceActor.Get())
			{
				Actor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
			}
		}
	}
}

void ULevelStreamingLevelInstance::OnPreInitializeContainerInstance(UActorDescContainerInstance::FInitializeParams& InInitParams, UActorDescContainerInstance* InContainerInstance)
{
	AActor* LevelInstanceActor = Cast<AActor>(GetLevelInstance());
	UWorldPartition* OwningWorldPartition = LevelInstanceActor->GetLevel()->GetWorldPartition();
	
	// In Editor it is possible to have a non WP parent world in which case we pass in null to the SetParent method, this will ensure that in editor the Level Instance container ID won't be a IsMainContainer() and will properly handle IsMainWorldOnly actors
	InInitParams.SetParent(OwningWorldPartition ? OwningWorldPartition->GetActorDescContainerInstance() : nullptr, LevelInstanceActor->GetActorGuid());
}

#endif

ULevelStreamingLevelInstance* ULevelStreamingLevelInstance::LoadInstance(ILevelInstanceInterface* LevelInstance)
{
	AActor* LevelInstanceActor = CastChecked<AActor>(LevelInstance);
#if WITH_EDITOR
	if (!ULevelInstanceSubsystem::CheckForLoop(LevelInstance))
	{
		UE_LOG(LogLevelInstance, Error, TEXT("Failed to load LevelInstance Actor '%s' because that would cause a loop. Run Map Check for more details."), *LevelInstanceActor->GetPathName());
		return nullptr;
	}

	FPackagePath WorldAssetPath;
	if (!FPackagePath::TryFromPackageName(LevelInstance->GetWorldAssetPackage(), WorldAssetPath) || !FPackageName::DoesPackageExist(WorldAssetPath))
	{
		UE_LOG(LogLevelInstance, Error, TEXT("Failed to load LevelInstance Actor '%s' because it refers to an invalid package ('%s'). Run Map Check for more details."), *LevelInstanceActor->GetPathName(), *LevelInstance->GetWorldAsset().GetLongPackageName());
		return nullptr;
	}
#endif

	bool bOutSuccess = false;

	const FString ShortPackageName = FPackageName::GetShortName(LevelInstance->GetWorldAsset().GetLongPackageName());
	// Build a unique and deterministic LevelInstance level instance name by using LevelInstanceID. 
	// Distinguish game from editor since we don't want to duplicate for PIE already loaded editor instances (not yet supported).
	const FString Suffix = FString::Printf(TEXT("%s_LevelInstance_%016llx_%d"), *ShortPackageName, LevelInstance->GetLevelInstanceID().GetHash(), LevelInstanceActor->GetWorld()->IsGameWorld() ? 1 : 0);
	
	UWorld* World = LevelInstanceActor->GetWorld();

	FLoadLevelInstanceParams Params(World, LevelInstance->GetWorldAssetPackage(), LevelInstanceActor->GetActorTransform());
	Params.OptionalLevelNameOverride = &Suffix;
	Params.OptionalLevelStreamingClass = LevelInstance->GetLevelStreamingClass();
	Params.bLoadAsTempPackage = true;
#if WITH_EDITOR
	Params.EditorPathOwner = LevelInstanceActor;
#endif
	
	if (World->IsGameWorld())
	{
		Params.bInitiallyVisible = LevelInstance->IsInitiallyVisible();
		Params.bAllowReuseExitingLevelStreaming = true;
	}

	const FString LevelInstancePackageName = ULevelStreamingDynamic::GetLevelInstancePackageName(Params);

	ULevelStreamingLevelInstance* LevelStreaming = Cast<ULevelStreamingLevelInstance>(ULevelStreamingDynamic::LoadLevelInstance(Params, bOutSuccess));
	if (bOutSuccess)
	{
		LevelStreaming->LevelInstanceID = LevelInstance->GetLevelInstanceID();
		LevelStreaming->LevelColor = FLinearColor::MakeRandomSeededColor(GetTypeHash(LevelInstance->GetLevelInstanceID()));

#if WITH_EDITOR
		if (!World->IsGameWorld())
		{
			GEngine->BlockTillLevelStreamingCompleted(LevelInstanceActor->GetWorld());
		}
#endif
		return LevelStreaming;
	}

	return nullptr;
}

#if WITH_EDITOR
void ULevelStreamingLevelInstance::OnLevelStreamingStateChanged(UWorld* InWorld, const ULevelStreaming* InLevelStreaming, ULevel* InLevelIfLoaded, ELevelStreamingState InPrevState, ELevelStreamingState InNewState)
{
	if (InNewState == ELevelStreamingState::LoadedVisible && InLevelStreaming == this)
	{
		ILevelInstanceInterface* LevelInstance = GetLevelInstance();

		ULevel* Level = GetLoadedLevel();
		check(GetLevelStreamingState() == ELevelStreamingState::LoadedVisible);

		// Flag all Level Objects as non RF_Transactional & RF_Transient so that they can't be added to the Transaction Buffer and will be allowed to be unloaded / reloaded without clearing the Transaction Buffer
		FLevelInstanceLevelStreamingUtils::MarkObjectsInPackageAsTransientAndNonTransactional(Level->GetPackage());

		ForEachObjectWithOuter(Level, [](UObject* InObject)
		{
			// Skip actors as they are already handled in OnLoadedActorsAddedToLevelPreEvent
			if (InObject && InObject->IsPackageExternal() && !InObject->IsA<AActor>())
			{
				FLevelInstanceLevelStreamingUtils::MarkObjectsInPackageAsTransientAndNonTransactional(InObject->GetPackage());
			}
		}, /*bIncludeNestedObjects*/ true);

		OnLoadedActorsAddedToLevelPreEvent(Level->Actors);

		Level->OnLoadedActorAddedToLevelPreEvent.AddUObject(this, &ULevelStreamingLevelInstance::OnLoadedActorsAddedToLevelPreEvent);
		Level->OnLoadedActorAddedToLevelPostEvent.AddUObject(this, &ULevelStreamingLevelInstance::OnLoadedActorsAddedToLevelPostEvent);
		Level->OnLoadedActorRemovedFromLevelPreEvent.AddUObject(this, &ULevelStreamingLevelInstance::OnLoadedActorsRemovedFromLevelPostEvent);

		// Create special actor that will handle selection and transform
		LevelInstanceEditorInstanceActor = ALevelInstanceEditorInstanceActor::Create(LevelInstance, Level);

		// Push editing state to child actors
		AActor* LevelInstanceActor = CastChecked<AActor>(LevelInstance);
		LevelInstanceActor->PushLevelInstanceEditingStateToProxies(LevelInstanceActor->IsInEditLevelInstanceHierarchy());

		// Unregister
		FLevelStreamingDelegates::OnLevelStreamingStateChanged.RemoveAll(this);
	}
}
#endif

void ULevelStreamingLevelInstance::UnloadInstance(ULevelStreamingLevelInstance* LevelStreaming)
{
#if WITH_EDITOR
	if (LevelStreaming->IsEditorWorldMode())
	{
		FLevelStreamingDelegates::OnLevelStreamingStateChanged.RemoveAll(LevelStreaming);

		ULevel* LoadedLevel = LevelStreaming->GetLoadedLevel();
		LoadedLevel->OnLoadedActorAddedToLevelPreEvent.RemoveAll(LevelStreaming);
		LoadedLevel->OnLoadedActorAddedToLevelPostEvent.RemoveAll(LevelStreaming);
		LoadedLevel->OnLoadedActorRemovedFromLevelPreEvent.RemoveAll(LevelStreaming);
		LevelStreaming->LevelInstanceEditorInstanceActor.Reset();

		// Check if we need to flush the Trans buffer...
		UWorld* OuterWorld = LoadedLevel->GetTypedOuter<UWorld>();
		bool bResetTrans = false;
		ForEachObjectWithOuterBreakable(OuterWorld, [&bResetTrans](UObject* Obj)
		{
			if(Obj->HasAnyFlags(RF_Transactional))
			{
				bResetTrans = true;
				UE_LOG(LogLevelInstance, Warning, TEXT("Found RF_Transactional object '%s' while unloading Level Instance."), *Obj->GetPathName());
				return false;
			}
			return true;
		}, /*bIncludeNestedObjects*/ true);

		LevelStreaming->GetWorld()->GetSubsystem<ULevelInstanceSubsystem>()->RemoveLevelsFromWorld({ LevelStreaming->GetLoadedLevel() }, bResetTrans);
	}
	else
#endif
	if (LevelStreaming->GetWorld()->IsGameWorld())
	{
		LevelStreaming->SetShouldBeLoaded(false);
		LevelStreaming->SetShouldBeVisible(false);
		LevelStreaming->SetIsRequestingUnloadAndRemoval(true);
	}

}

void ULevelStreamingLevelInstance::OnLevelLoadedChanged(ULevel* InLevel)
{
	Super::OnLevelLoadedChanged(InLevel);

	if (ULevel* NewLoadedLevel = GetLoadedLevel())
	{
#if WITH_EDITOR
		if (IsEditorWorldMode())
		{
			FLevelStreamingDelegates::OnLevelStreamingStateChanged.AddUObject(this, &ULevelStreamingLevelInstance::OnLevelStreamingStateChanged);
		}
#endif

		check(InLevel == NewLoadedLevel);
		if (!NewLoadedLevel->bAlreadyMovedActors)
		{
			AWorldSettings* WorldSettings = NewLoadedLevel->GetWorldSettings();
			check(WorldSettings);
			LevelTransform = FTransform(WorldSettings->LevelInstancePivotOffset) * LevelTransform;
		}

		if (ULevelInstanceSubsystem* LevelInstanceSubsystem = GetWorld()->GetSubsystem<ULevelInstanceSubsystem>())
		{
			LevelInstanceSubsystem->RegisterLoadedLevelStreamingLevelInstance(this);

#if WITH_EDITOR
			if (UWorldPartition* OuterWorldPartition = NewLoadedLevel->GetWorldPartition())
			{
				check(!OuterWorldPartition->IsInitialized());
				
				// In Non-Editor worlds we want the container of a Level Instance to be considered the main container, it will do its own generate streaming if it is a World Partition
				// so we don't need to be called on Pre init
				if (IsEditorWorldMode())
				{
					OuterWorldPartition->OnActorDescContainerInstancePreInitialize.BindUObject(this, &ULevelStreamingLevelInstance::OnPreInitializeContainerInstance);
				}

				if (UWorldPartition* OwningWorldPartition = GetWorld()->GetWorldPartition(); OwningWorldPartition && OwningWorldPartition->IsStreamingEnabled())
				{
					if (GDisableLevelInstanceEditorPartialLoading)
					{
						OuterWorldPartition->bOverrideEnableStreamingInEditor = false;
					}
					else if (ILevelInstanceInterface* LevelInstance = LevelInstanceSubsystem->GetLevelInstance(LevelInstanceID))
					{
						OuterWorldPartition->bOverrideEnableStreamingInEditor = LevelInstance->SupportsPartialEditorLoading();
					}
				}
				else
				{
					// Do not enable Streaming in Editor if Level Instance is not part of a World Partition/Streaming world
					OuterWorldPartition->bOverrideEnableStreamingInEditor = false;
				}
			}
#endif
		}
	}
}
