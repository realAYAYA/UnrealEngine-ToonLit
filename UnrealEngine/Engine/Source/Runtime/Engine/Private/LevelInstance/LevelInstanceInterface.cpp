// Copyright Epic Games, Inc. All Rights Reserved.

#include "LevelInstance/LevelInstanceInterface.h"
#include "LevelInstance/LevelInstanceSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LevelInstanceInterface)

#if WITH_EDITOR
#include "LevelInstance/LevelInstanceEditorInstanceActor.h"
#include "LevelInstance/LevelInstanceComponent.h"
#endif

ULevelInstanceInterface::ULevelInstanceInterface(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{}

ULevelInstanceSubsystem* ILevelInstanceInterface::GetLevelInstanceSubsystem() const
{
	const AActor* Actor = CastChecked<AActor>(this);
	return Actor->GetWorld() ? Actor->GetWorld()->GetSubsystem<ULevelInstanceSubsystem>() : nullptr;
}

bool ILevelInstanceInterface::IsLoaded() const
{
	if (ULevelInstanceSubsystem* LevelInstanceSubsystem = GetLevelInstanceSubsystem())
	{
		return LevelInstanceSubsystem->IsLoaded(this);
	}

	return false;
}

void ILevelInstanceInterface::LoadLevelInstance()
{
	if (IsLoadingEnabled())
	{
		if (ULevelInstanceSubsystem* LevelInstanceSubsystem = GetLevelInstanceSubsystem())
		{
			bool bForce = false;
#if WITH_EDITOR
			// When reinstancing or world wasn't ticked between changes. Avoid reloading the level but if the package changed, force the load
			bForce = IsLoaded() && LevelInstanceSubsystem->GetLevelInstanceLevel(this)->GetPackage()->GetLoadedPath() != FPackagePath::FromPackageNameChecked(GetWorldAssetPackage());
#endif
			LevelInstanceSubsystem->RequestLoadLevelInstance(this, bForce);
		}
	}
}

void ILevelInstanceInterface::UnloadLevelInstance()
{
	if (IsLoadingEnabled())
	{
		if (ULevelInstanceSubsystem* LevelInstanceSubsystem = GetLevelInstanceSubsystem())
		{
#if WITH_EDITOR
			check(!HasDirtyChildren());
#endif
			LevelInstanceSubsystem->RequestUnloadLevelInstance(this);
		}
	}
}

ULevelStreamingLevelInstance* ILevelInstanceInterface::GetLevelStreaming() const
{
	if (ULevelInstanceSubsystem* LevelInstanceSubsystem = GetLevelInstanceSubsystem())
	{
		return LevelInstanceSubsystem->GetLevelInstanceLevelStreaming(this);
	}

	return nullptr;
}

#if WITH_EDITOR
ULevel* ILevelInstanceInterface::GetLoadedLevel() const
{
	if (ULevelInstanceSubsystem* LevelInstanceSubsystem = GetLevelInstanceSubsystem())
	{
		return LevelInstanceSubsystem->GetLevelInstanceLevel(this);
	}

	return nullptr;
}

void ILevelInstanceInterface::UpdateLevelInstanceFromWorldAsset()
{
	if (HasValidLevelInstanceID())
	{
		if (ULevelInstanceSubsystem* LevelInstanceSubsystem = GetLevelInstanceSubsystem())
		{
			if (IsWorldAssetValid() && IsLoadingEnabled())
			{
				const bool bForceUpdate = true;
				LevelInstanceSubsystem->RequestLoadLevelInstance(this, bForceUpdate);
			}
			else if (IsLoaded())
			{
				UnloadLevelInstance();
			}
		}
	}
}

bool ILevelInstanceInterface::IsEditing() const
{
	if (HasValidLevelInstanceID())
	{
		if (ULevelInstanceSubsystem* LevelInstanceSubsystem = GetLevelInstanceSubsystem())
		{
			return LevelInstanceSubsystem->IsEditingLevelInstance(this);
		}
	}

	return false;
}

bool ILevelInstanceInterface::HasChildEdit() const
{
	if (HasValidLevelInstanceID())
	{
		if (ULevelInstanceSubsystem* LevelInstanceSubsystem = GetLevelInstanceSubsystem())
		{
			return LevelInstanceSubsystem->HasChildEdit(this);
		}
	}

	return false;
}

bool ILevelInstanceInterface::IsDirty() const
{
	if (HasValidLevelInstanceID())
	{
		if (ULevelInstanceSubsystem* LevelInstanceSubsystem = GetLevelInstanceSubsystem())
		{
			return LevelInstanceSubsystem->IsEditingLevelInstanceDirty(this);
		}
	}

	return false;
}

bool ILevelInstanceInterface::HasDirtyChildren() const
{
	if (HasValidLevelInstanceID())
	{
		if (ULevelInstanceSubsystem* LevelInstanceSubsystem = GetLevelInstanceSubsystem())
		{
			return LevelInstanceSubsystem->HasDirtyChildrenLevelInstances(this);
		}
	}

	return false;
}

bool ILevelInstanceInterface::CanEnterEdit(FText* OutReason) const
{
	if (HasValidLevelInstanceID())
	{
		if (ULevelInstanceSubsystem* LevelInstanceSubsystem = GetLevelInstanceSubsystem())
		{
			return LevelInstanceSubsystem->CanEditLevelInstance(this, OutReason);
		}
	}

	return false;
}

bool ILevelInstanceInterface::EnterEdit(AActor* ContextActor)
{
	ULevelInstanceSubsystem* LevelInstanceSubsystem = GetLevelInstanceSubsystem();
	check(LevelInstanceSubsystem);
	LevelInstanceSubsystem->EditLevelInstance(this, ContextActor);
	return true;
}

void ILevelInstanceInterface::OnEdit()
{
	if (ULevelInstanceComponent* LevelInstanceComponent = GetLevelInstanceComponent())
	{
		LevelInstanceComponent->OnEdit();
	}
}

bool ILevelInstanceInterface::CanExitEdit(bool bDiscardEdits, FText* OutReason) const
{
	if (HasValidLevelInstanceID())
	{
		if (ULevelInstanceSubsystem* LevelInstanceSubsystem = GetLevelInstanceSubsystem())
		{
			return LevelInstanceSubsystem->CanCommitLevelInstance(this, bDiscardEdits, OutReason);
		}
	}

	return false;
}

bool ILevelInstanceInterface::ExitEdit(bool bDiscardEdits)
{
	ULevelInstanceSubsystem* LevelInstanceSubsystem = GetLevelInstanceSubsystem();
	check(LevelInstanceSubsystem);
	return LevelInstanceSubsystem->CommitLevelInstance(this, bDiscardEdits);
}

void ILevelInstanceInterface::OnCommit(bool bChanged)
{
	if (ULevelInstanceComponent* LevelInstanceComponent = GetLevelInstanceComponent())
	{
		LevelInstanceComponent->OnCommit();
	}
}

bool ILevelInstanceInterface::SetCurrent()
{
	if (ULevelInstanceSubsystem* LevelInstanceSubsystem = GetLevelInstanceSubsystem())
	{
		return LevelInstanceSubsystem->SetCurrent(this);
	}

	return false;
}

bool ILevelInstanceInterface::MoveActorsTo(const TArray<AActor*>& ActorsToMove)
{
	if (ULevelInstanceSubsystem* LevelInstanceSubsystem = GetLevelInstanceSubsystem())
	{
		return LevelInstanceSubsystem->MoveActorsTo(this, ActorsToMove);
	}

	return false;
}

#endif


