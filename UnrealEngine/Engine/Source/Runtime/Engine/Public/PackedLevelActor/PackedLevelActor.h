// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "LevelInstance/LevelInstanceActor.h"

#if WITH_EDITOR
#include "Kismet2/ComponentEditorUtils.h"
#endif

#include "PackedLevelActor.generated.h"

class UInstancedStaticMeshComponent;
class UBlueprint;

/**
 * APackedLevelActor is the result of packing the source level (WorldAsset base class property) into a single actor. See FPackedLevelActorBuilder.
 * 
 * 
 * Other components are unsupported and will result in an incomplete APackedLevelActor. In this case using a regular ALevelInstance is recommended.
 */
UCLASS()
class ENGINE_API APackedLevelActor : public ALevelInstance
{
	GENERATED_BODY()

public:
	APackedLevelActor();

	virtual bool IsLoadingEnabled() const override;

	virtual void Serialize(FArchive& Ar) override;
#if WITH_EDITOR
	virtual TUniquePtr<class FWorldPartitionActorDesc> CreateClassActorDesc() const override;
	
	static bool CreateOrUpdateBlueprint(ALevelInstance* InLevelInstance, TSoftObjectPtr<UBlueprint> InBlueprintAsset, bool bCheckoutAndSave = true, bool bPromptForSave = true);
	static bool CreateOrUpdateBlueprint(TSoftObjectPtr<UWorld> InWorldAsset, TSoftObjectPtr<UBlueprint> InBlueprintAsset, bool bCheckoutAndSave = true, bool bPromptForSave = true);

	static FName GetPackedComponentTag();

	virtual void PostLoad() override;
	virtual void UpdateLevelInstanceFromWorldAsset() override;
	virtual void OnCommit(bool bChanged) override;
	virtual void OnCommitChild(bool bChanged) override;
	virtual void OnEdit() override;
	virtual void OnEditChild() override;

	virtual bool CanEditChange(const FProperty* InProperty) const override;
		
	void SetPackedVersion(const FGuid& Version) { PackedVersion = Version; }

	virtual bool IsHiddenEd() const override;
	virtual bool IsHLODRelevant() const override;

	void DestroyPackedComponents();
	void GetPackedComponents(TArray<UActorComponent*>& OutPackedComponents) const;

	virtual ELevelInstanceRuntimeBehavior GetDefaultRuntimeBehavior() const override { return ELevelInstanceRuntimeBehavior::None; }

	virtual void RerunConstructionScripts() override;

	template<class T>
	T* AddPackedComponent(TSubclassOf<T> ComponentClass)
	{
		Modify();
		FName NewComponentName = *FComponentEditorUtils::GenerateValidVariableName(ComponentClass, this);
		T* NewComponent = NewObject<T>(this, ComponentClass, NewComponentName, RF_Transactional);
		AddInstanceComponent(NewComponent);
		NewComponent->ComponentTags.Add(GetPackedComponentTag());
		return NewComponent;
	}
#endif

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TArray<TSoftObjectPtr<UBlueprint>> PackedBPDependencies;

private:
	bool bChildChanged;

	UPROPERTY()
	FGuid PackedVersion;
#endif
};


DEFINE_ACTORDESC_TYPE(APackedLevelActor, FWorldPartitionActorDesc);