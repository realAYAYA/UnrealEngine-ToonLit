// Copyright Epic Games, Inc. All Rights Reserved.

#include "LevelInstance/LevelInstanceLevelStreaming.h"
#include "LevelInstance/LevelInstanceInterface.h"
#include "LevelInstance/LevelInstancePrivate.h"
#include "LevelInstance/LevelInstanceSubsystem.h"
#include "Engine/LevelBounds.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"
#include "GameFramework/WorldSettings.h"
#include "ProfilingDebugging/ScopedTimers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LevelInstanceLevelStreaming)

#if WITH_EDITOR
#include "LevelInstance/LevelInstanceEditorInstanceActor.h"
#include "Editor.h"
#include "EditorLevelUtils.h"
#include "LevelUtils.h"
#include "ActorFolder.h"
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

void ULevelStreamingLevelInstance::OnLoadedActorAddedToLevel(AActor& InActor)
{
	check(LevelInstanceEditorInstanceActor.IsValid());
	PrepareLevelInstanceLoadedActor(InActor, GetLevelInstance(), true);
}

void ULevelStreamingLevelInstance::ResetLevelInstanceLoaders()
{
	// @todo_ow: Resetting the load will prevent any OFPA package to properly load since their import level will fail to resolve.
	//           This is a temporary workaround. The downside of this is that the level can't be saved. 
	//           Most of the changes will only affect OFPA packages. One problematic use case is when changing the pivot of a Level Instance.

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

		for (AActor* Actor : LoadedLevel->Actors)
		{
			if (Actor && Actor->IsPackageExternal())
			{
				ResetLoaders(Actor->GetExternalPackage());
			}
		}

		LoadedLevel->ForEachActorFolder([](UActorFolder* ActorFolder)
		{
			if (ActorFolder->IsPackageExternal())
			{
				ResetLoaders(ActorFolder->GetExternalPackage());
			}
			return true;
		});

		bResetLoadersCalled = true;
	}
}

void ULevelStreamingLevelInstance::PrepareLevelInstanceLoadedActor(AActor& InActor, ILevelInstanceInterface* InLevelInstance, bool bResetLoaders)
{
	InActor.ClearFlags(RF_Transactional);
	InActor.SetFlags(RF_Transient);

	if (InActor.IsPackageExternal())
	{
		if (bResetLoaders)
		{
			ResetLoaders(InActor.GetExternalPackage());
		}

		InActor.GetPackage()->SetFlags(RF_Transient);
	}

	InActor.PushSelectionToProxies();
	if (InLevelInstance)
	{
		InActor.PushLevelInstanceEditingStateToProxies(CastChecked<AActor>(InLevelInstance)->IsInEditingLevelInstance());
	}

	if (LevelInstanceEditorInstanceActor.IsValid())
	{
		if (InActor.GetAttachParentActor() == nullptr && !InActor.IsChildActor())
		{
			// Detect if LevelInstance has moved since it was loaded/created and move the child actor accordingly
			if (!LevelTransform.Equals(LevelInstanceEditorInstanceActor->GetTransform()))
			{
				FTransform TransformToApply = LevelTransform.Inverse() * LevelInstanceEditorInstanceActor->GetTransform();
				FLevelUtils::FApplyLevelTransformParams TransformParams(InActor.GetLevel(), TransformToApply);
				TransformParams.Actor = &InActor;
				TransformParams.bDoPostEditMove = true;
				FLevelUtils::ApplyLevelTransform(TransformParams);
			}

			InActor.AttachToActor(LevelInstanceEditorInstanceActor.Get(), FAttachmentTransformRules::KeepWorldTransform);
		}
	}
}

void ULevelStreamingLevelInstance::OnLoadedActorRemovedFromLevel(AActor& InActor)
{
	// Detach actor or else it will keep it alive (attachement keeps a reference to the actor)
	check(LevelInstanceEditorInstanceActor.IsValid());
	if (InActor.GetAttachParentActor() == LevelInstanceEditorInstanceActor.Get())
	{
		InActor.DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	}
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
	Params.OptionalLevelStreamingClass = ULevelStreamingLevelInstance::StaticClass();
	Params.bLoadAsTempPackage = true;
	
	if (World->IsGameWorld())
	{
		Params.bInitiallyVisible = LevelInstance->IsInitiallyVisible();
	}
	
	ULevelStreamingLevelInstance* LevelStreaming = Cast<ULevelStreamingLevelInstance>(ULevelStreamingDynamic::LoadLevelInstance(Params, bOutSuccess));
	if (bOutSuccess)
	{
		LevelStreaming->LevelInstanceID = LevelInstance->GetLevelInstanceID();
		
#if WITH_EDITOR
		if (!World->IsGameWorld())
		{
			GEngine->BlockTillLevelStreamingCompleted(LevelInstanceActor->GetWorld());

			// Most of the code here is meant to allow partial support for undo/redo of LevelInstance Instance Loading:
			// by setting the objects RF_Transient and !RF_Transactional we can check when unloading if those flags
			// have been changed and figure out if we need to clear the transaction buffer or not.
			// It might not be the final solution to support Undo/Redo in LevelInstances but it handles most of the non-editing part
			if (ULevel* Level = LevelStreaming->GetLoadedLevel())
			{
				check(LevelStreaming->GetCurrentState() == ULevelStreaming::ECurrentState::LoadedVisible);

				Level->OnLoadedActorAddedToLevelEvent.AddUObject(LevelStreaming, &ULevelStreamingLevelInstance::OnLoadedActorAddedToLevel);
				Level->OnLoadedActorRemovedFromLevelEvent.AddUObject(LevelStreaming, &ULevelStreamingLevelInstance::OnLoadedActorRemovedFromLevel);

				UWorld* OuterWorld = Level->GetTypedOuter<UWorld>();
				OuterWorld->ClearFlags(RF_Transactional);
				OuterWorld->SetFlags(RF_Transient);
			
				OuterWorld->GetPackage()->ClearFlags(RF_Transactional);
				OuterWorld->GetPackage()->SetFlags(RF_Transient);

				ForEachObjectWithOuter(OuterWorld, [&](UObject* Obj)
				{
					Obj->ClearFlags(RF_Transactional);
					Obj->SetFlags(RF_Transient);
				}, true);

				for (AActor* LevelActor : Level->Actors)
				{
					if (LevelActor)
					{
						LevelStreaming->PrepareLevelInstanceLoadedActor(*LevelActor, LevelInstance, false);
					}
				}

				Level->ForEachActorFolder([](UActorFolder* ActorFolder)
				{
					if (ActorFolder->IsPackageExternal())
					{
						ActorFolder->GetPackage()->SetFlags(RF_Transient);
					}
					return true;
				});

				// Create special actor that will handle selection and transform
				LevelStreaming->LevelInstanceEditorInstanceActor = ALevelInstanceEditorInstanceActor::Create(LevelInstance, Level);
			}
			else
			{
				// Failed to load package
				return nullptr;
			}
		}
#endif
		return LevelStreaming;
	}

	return nullptr;
}

void ULevelStreamingLevelInstance::UnloadInstance(ULevelStreamingLevelInstance* LevelStreaming)
{
	if (LevelStreaming->GetWorld()->IsGameWorld())
	{
		LevelStreaming->SetShouldBeLoaded(false);
		LevelStreaming->SetShouldBeVisible(false);
		LevelStreaming->SetIsRequestingUnloadAndRemoval(true);
	}
#if WITH_EDITOR
	else
	{
		ULevel* LoadedLevel = LevelStreaming->GetLoadedLevel();
		LoadedLevel->OnLoadedActorAddedToLevelEvent.RemoveAll(LevelStreaming);
		LoadedLevel->OnLoadedActorRemovedFromLevelEvent.RemoveAll(LevelStreaming);
		LevelStreaming->LevelInstanceEditorInstanceActor.Reset();

		// Check if we need to flush the Trans buffer...
		UWorld* OuterWorld = LoadedLevel->GetTypedOuter<UWorld>();
		bool bResetTrans = false;
		ForEachObjectWithOuterBreakable(OuterWorld, [&bResetTrans](UObject* Obj)
		{
			if(Obj->HasAnyFlags(RF_Transactional))
			{
				bResetTrans = true;
				return false;
			}
			return true;
		}, true);

		LevelStreaming->GetWorld()->GetSubsystem<ULevelInstanceSubsystem>()->RemoveLevelsFromWorld({ LevelStreaming->GetLoadedLevel() }, bResetTrans);
	}
#endif 
}

void ULevelStreamingLevelInstance::OnLevelLoadedChanged(ULevel* InLevel)
{
	Super::OnLevelLoadedChanged(InLevel);

	if (ULevel* NewLoadedLevel = GetLoadedLevel())
	{
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
		}
	}
}

