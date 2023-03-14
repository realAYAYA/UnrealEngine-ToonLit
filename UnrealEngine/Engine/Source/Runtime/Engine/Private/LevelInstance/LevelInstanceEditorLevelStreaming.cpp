// Copyright Epic Games, Inc. All Rights Reserved.

#include "LevelInstance/LevelInstanceEditorLevelStreaming.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LevelInstanceEditorLevelStreaming)

#if WITH_EDITOR
#include "LevelInstance/LevelInstanceInterface.h"
#include "LevelInstance/LevelInstanceSubsystem.h"
#include "LevelInstance/LevelInstanceEditorPivot.h"
#include "EditorLevelUtils.h"
#include "Editor.h"
#include "Engine/LevelBounds.h"
#include "GameFramework/WorldSettings.h"
#include "PackageTools.h"
#endif

#if WITH_EDITOR
static FLevelInstanceID EditLevelInstanceID;
#endif

ULevelStreamingLevelInstanceEditor::ULevelStreamingLevelInstanceEditor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
#if WITH_EDITOR
	, LevelInstanceID(EditLevelInstanceID)
#endif
{
#if WITH_EDITOR
	SetShouldBeVisibleInEditor(true);

	if (!IsTemplate() && !GetWorld()->IsGameWorld())
	{
		GEngine->OnLevelActorAdded().AddUObject(this, &ULevelStreamingLevelInstanceEditor::OnLevelActorAdded);
	}
#endif
}

#if WITH_EDITOR
TOptional<FFolder::FRootObject> ULevelStreamingLevelInstanceEditor::GetFolderRootObject() const
{
	if (ILevelInstanceInterface* LevelInstance = GetLevelInstance())
	{
		if (AActor* Actor = CastChecked<AActor>(LevelInstance))
		{
			return FFolder::FRootObject(Actor);
		}
	}

	if (!LevelInstanceID.IsValid())
	{
		// When creating a new level instance, we have an invalid LevelInstanceID (until it gets loaded)
		// Return the world as root object.
		return FFolder::GetWorldRootFolder(GetWorld()).GetRootObject();
	}

	return TOptional<FFolder::FRootObject>();
}

ILevelInstanceInterface* ULevelStreamingLevelInstanceEditor::GetLevelInstance() const
{
	if (ULevelInstanceSubsystem* LevelInstanceSubsystem = GetWorld()->GetSubsystem<ULevelInstanceSubsystem>())
	{
		return LevelInstanceSubsystem->GetLevelInstance(LevelInstanceID);
	}

	return nullptr;
}

ULevelStreamingLevelInstanceEditor* ULevelStreamingLevelInstanceEditor::Load(ILevelInstanceInterface* LevelInstance)
{
	AActor* LevelInstanceActor = CastChecked<AActor>(LevelInstance);
	UWorld* CurrentWorld = LevelInstanceActor->GetWorld();

	// Make sure the level is not already loaded
	// For instance, drag and drop a Level into a Level Instance, then Edit the Level Instance will
	// result in the level at the wrong location (level transform wasn't applied)
	UPackage* LevelPackage = FindObject<UPackage>(nullptr, *LevelInstance->GetWorldAssetPackage());
	UWorld* LevelWorld = LevelPackage ? UWorld::FindWorldInPackage(LevelPackage) : nullptr;
	if (LevelWorld && LevelWorld->bIsWorldInitialized && !LevelWorld->GetPackage()->HasAnyPackageFlags(PKG_NewlyCreated))
	{
		UPackageTools::UnloadPackages({ LevelPackage });
	}

	TGuardValue<FLevelInstanceID> GuardEditLevelInstanceID(EditLevelInstanceID, LevelInstance->GetLevelInstanceID());
	ULevelStreamingLevelInstanceEditor* LevelStreaming = nullptr;
	// If Asset is null we can create a level here
	if (LevelInstance->GetWorldAsset().IsNull())
	{
		LevelStreaming = Cast<ULevelStreamingLevelInstanceEditor>(EditorLevelUtils::CreateNewStreamingLevelForWorld(*CurrentWorld, ULevelStreamingLevelInstanceEditor::StaticClass(), TEXT(""), false, nullptr, true, TFunction<void(ULevel*)>(), LevelInstanceActor->GetTransform()));
		if (LevelStreaming)
		{
			LevelInstanceActor->Modify();
			LevelInstance->SetWorldAsset(LevelStreaming->GetLoadedLevel()->GetTypedOuter<UWorld>());
		}
	}
	else
	{
		LevelStreaming = Cast<ULevelStreamingLevelInstanceEditor>(EditorLevelUtils::AddLevelToWorld(CurrentWorld, *LevelInstance->GetWorldAssetPackage(), ULevelStreamingLevelInstanceEditor::StaticClass(), LevelInstanceActor->GetTransform()));
	}
		
	if (LevelStreaming)
	{
		check(LevelStreaming);
		check(LevelStreaming->LevelInstanceID == LevelInstance->GetLevelInstanceID());

		GEngine->BlockTillLevelStreamingCompleted(LevelInstanceActor->GetWorld());

		if (ULevel* LoadedLevel = LevelStreaming->GetLoadedLevel())
		{
			LoadedLevel->OnLoadedActorAddedToLevelEvent.AddUObject(LevelStreaming, &ULevelStreamingLevelInstanceEditor::OnLoadedActorAddedToLevel);
		}

		// Create special actor that will handle changing the pivot of this level
		FLevelInstanceEditorPivotHelper::Create(LevelInstance, LevelStreaming);

		return LevelStreaming;
	}

	return nullptr;
}

void ULevelStreamingLevelInstanceEditor::Unload(ULevelStreamingLevelInstanceEditor* LevelStreaming)
{
	if (ULevelInstanceSubsystem* LevelInstanceSubsystem = LevelStreaming->GetWorld()->GetSubsystem<ULevelInstanceSubsystem>())
	{
		if (ULevel* LoadedLevel = LevelStreaming->GetLoadedLevel())
		{
			LoadedLevel->OnLoadedActorAddedToLevelEvent.RemoveAll(LevelStreaming);
			LevelInstanceSubsystem->RemoveLevelsFromWorld({ LoadedLevel });
		}
	}
}

void ULevelStreamingLevelInstanceEditor::OnLoadedActorAddedToLevel(AActor& InActor)
{
	OnLevelActorAdded(&InActor);
}

void ULevelStreamingLevelInstanceEditor::OnLevelActorAdded(AActor* InActor)
{
	if (InActor && InActor->GetLevel() == LoadedLevel)
	{
		InActor->PushLevelInstanceEditingStateToProxies(true);
	}
}

void ULevelStreamingLevelInstanceEditor::OnLevelLoadedChanged(ULevel* InLevel)
{
	Super::OnLevelLoadedChanged(InLevel);

	if (ULevel* NewLoadedLevel = GetLoadedLevel())
	{
		check(InLevel == NewLoadedLevel);

		// Avoid prompts for Level Instance editing
		NewLoadedLevel->bPromptWhenAddingToLevelBeforeCheckout = false;
		NewLoadedLevel->bPromptWhenAddingToLevelOutsideBounds = false;

		check(!NewLoadedLevel->bAlreadyMovedActors);
		if (AWorldSettings* WorldSettings = NewLoadedLevel->GetWorldSettings())
		{
			LevelTransform = FTransform(WorldSettings->LevelInstancePivotOffset) * LevelTransform;
		}

		if (ULevelInstanceSubsystem* LevelInstanceSubsystem = GetWorld()->GetSubsystem<ULevelInstanceSubsystem>())
		{
			LevelInstanceSubsystem->RegisterLoadedLevelStreamingLevelInstanceEditor(this);
		}
	}
}

FBox ULevelStreamingLevelInstanceEditor::GetBounds() const
{
	check(GetLoadedLevel());
	return ALevelBounds::CalculateLevelBounds(GetLoadedLevel());
}

#endif
