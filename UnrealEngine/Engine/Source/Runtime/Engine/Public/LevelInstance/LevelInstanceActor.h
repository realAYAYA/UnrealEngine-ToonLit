// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Actor.h"
#include "Containers/Array.h"
#include "LevelInstance/LevelInstanceTypes.h"
#include "LevelInstance/LevelInstanceInterface.h"
#include "LevelInstance/LevelInstanceActorImpl.h"
#include "LevelInstance/LevelInstanceActorGuid.h"
#include "WorldPartition/WorldPartitionActorDesc.h"
#include "LevelInstanceActor.generated.h"

UCLASS()
class ENGINE_API ALevelInstance : public AActor, public ILevelInstanceInterface
{
	GENERATED_BODY()

public:
	ALevelInstance();

protected:
#if WITH_EDITORONLY_DATA
	/** Level LevelInstance */
	UPROPERTY(EditAnywhere, Category = Level, meta = (NoCreate, DisplayName="Level"))
	TSoftObjectPtr<UWorld> WorldAsset;
#endif

	UPROPERTY()
	TSoftObjectPtr<UWorld> CookedWorldAsset;

	UPROPERTY(Transient, ReplicatedUsing=OnRep_LevelInstanceSpawnGuid)
	FGuid LevelInstanceSpawnGuid;

private:
	FLevelInstanceActorGuid LevelInstanceActorGuid;
	FLevelInstanceActorImpl LevelInstanceActorImpl;
public:

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	ELevelInstanceRuntimeBehavior DesiredRuntimeBehavior;
#endif

	// Begin ILevelInstanceInterface
	const FLevelInstanceID& GetLevelInstanceID() const override;
	bool HasValidLevelInstanceID() const override;
	virtual const FGuid& GetLevelInstanceGuid() const override;
	virtual const TSoftObjectPtr<UWorld>& GetWorldAsset() const override;
	
	virtual void OnLevelInstanceLoaded() override;
	virtual bool IsLoadingEnabled() const override;
	// End ILevelInstanceInterface
			
	virtual void PostRegisterAllComponents() override;
	virtual void PostUnregisterAllComponents() override;
	virtual void Serialize(FArchive& Ar) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
		
	UFUNCTION()
	void OnRep_LevelInstanceSpawnGuid();

#if WITH_EDITOR
	// Begin ILevelInstanceInterface
	virtual ULevelInstanceComponent* GetLevelInstanceComponent() const override;
	virtual bool SetWorldAsset(TSoftObjectPtr<UWorld> WorldAsset) override;
	virtual ELevelInstanceRuntimeBehavior GetDesiredRuntimeBehavior() const override { return DesiredRuntimeBehavior; }
	virtual ELevelInstanceRuntimeBehavior GetDefaultRuntimeBehavior() const override { return ELevelInstanceRuntimeBehavior::Partitioned; }
	virtual TSubclassOf<AActor> GetEditorPivotClass() const override;
	// End ILevelInstanceInterface
			
	// UObject overrides
	virtual void PreEditUndo() override;
	virtual void PostEditUndo() override;
	virtual void PostEditUndo(TSharedPtr<ITransactionObjectAnnotation> TransactionAnnotation) override;
	virtual void PostLoad() override;
	virtual bool CanEditChange(const FProperty* InProperty) const override;
	virtual void PreEditChange(FProperty* PropertyThatWillChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditImport() override;
	virtual void PreSave(FObjectPreSaveContext ObjectSaveContext) override;

	// AActor overrides
	virtual void CheckForErrors() override;
	virtual TUniquePtr<class FWorldPartitionActorDesc> CreateClassActorDesc() const override;
	virtual bool CanDeleteSelectedActor(FText& OutReason) const override;
	virtual void SetIsTemporarilyHiddenInEditor(bool bIsHidden) override;
	virtual bool SetIsHiddenEdLayer(bool bIsHiddenEdLayer) override;
	virtual void EditorGetUnderlyingActors(TSet<AActor*>& OutUnderlyingActors) const override;
	virtual void PushSelectionToProxies() override;
	virtual void PushLevelInstanceEditingStateToProxies(bool bInEditingState) override;
	virtual FBox GetComponentsBoundingBox(bool bNonColliding = false, bool bIncludeFromChildActors = false) const override;
	virtual FBox GetStreamingBounds() const override;
	virtual bool IsLockLocation() const override;
	virtual bool GetReferencedContentObjects(TArray<UObject*>& Objects) const override;
	virtual bool GetSoftReferencedContentObjects(TArray<FSoftObjectPath>& SoftObjects) const override;
	virtual bool OpenAssetEditor() override;
	virtual bool EditorCanAttachFrom(const AActor* InChild, FText& OutReason) const override;
	// End of AActor interface

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnLevelInstanceActorPostLoad, ALevelInstance*);
	static FOnLevelInstanceActorPostLoad OnLevelInstanceActorPostLoad;

private:
	void ResetUnsupportedWorldAsset();
#endif
};

DEFINE_ACTORDESC_TYPE(ALevelInstance, FLevelInstanceActorDesc);
