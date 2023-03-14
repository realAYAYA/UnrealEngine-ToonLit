// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "UObject/SoftObjectPtr.h"
#include "LevelInstance/LevelInstanceTypes.h"
#include "LevelInstanceInterface.generated.h"

class ULevelInstanceComponent;
class ULevelStreamingLevelInstance;

UINTERFACE()
class ENGINE_API ULevelInstanceInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

/**
 * Interface to be implemented by Actor classes to implement support for LevelInstances
 *
 * Interface exists to allow integration of the LevelInstance workflow on existing Actor class hierarchies.
 * 
 * @see FLevelInstanceActorImpl to use as a member or base class member of the implementing class. It provides most of the boiler plate code so that the implementing class behaves properly in editor (Undo/Redo/Delete/Selection/Imporrt/etc)
 */
class ENGINE_API ILevelInstanceInterface
{
	GENERATED_IINTERFACE_BODY()
			
	// Pure Interface Start
	virtual const FLevelInstanceID& GetLevelInstanceID() const = 0;

	virtual bool HasValidLevelInstanceID() const = 0;

	virtual const FGuid& GetLevelInstanceGuid() const = 0;

	virtual const TSoftObjectPtr<UWorld>& GetWorldAsset() const = 0;

	virtual bool IsLoadingEnabled() const = 0;

#if WITH_EDITOR
	virtual bool SetWorldAsset(TSoftObjectPtr<UWorld> WorldAsset) = 0;

	virtual ULevelInstanceComponent* GetLevelInstanceComponent() const = 0;

	virtual ELevelInstanceRuntimeBehavior GetDesiredRuntimeBehavior() const = 0;

	virtual ELevelInstanceRuntimeBehavior GetDefaultRuntimeBehavior() const = 0;

	virtual TSubclassOf<AActor> GetEditorPivotClass() const { return nullptr; }
#endif
	// Pure Interface End
		
	virtual void OnLevelInstanceLoaded() {}
		
	FString GetWorldAssetPackage() const { return GetWorldAsset().GetUniqueID().GetLongPackageName(); }

	bool IsWorldAssetValid() const { return GetWorldAsset().GetUniqueID().IsValid(); }

	virtual ULevelInstanceSubsystem* GetLevelInstanceSubsystem() const;

	virtual bool IsLoaded() const;
			
	virtual void LoadLevelInstance();

	virtual void UnloadLevelInstance();

	virtual bool IsInitiallyVisible() const { return true; }

	virtual ULevelStreamingLevelInstance* GetLevelStreaming() const;

#if WITH_EDITOR
	virtual ULevel* GetLoadedLevel() const;

	virtual void UpdateLevelInstanceFromWorldAsset();

	virtual void OnEdit();

	virtual void OnEditChild() {}

	virtual void OnCommit(bool bChanged);

	virtual void OnCommitChild(bool bChanged) {}
	
	virtual bool IsEditing() const;
	
	virtual bool HasChildEdit() const;
	
	virtual bool IsDirty() const;
	
	virtual bool HasDirtyChildren() const;
	
	virtual bool CanEnterEdit(FText* OutReason = nullptr) const;
	
	virtual bool EnterEdit(AActor* ContextActor = nullptr);
	
	virtual bool CanExitEdit(bool bDiscardEdits = false, FText* OutReason = nullptr) const;
	
	virtual bool ExitEdit(bool bDiscardEdits = false);
	
	virtual bool SetCurrent();
	
	virtual bool MoveActorsTo(const TArray<AActor*>& ActorsToMove);
#endif
};