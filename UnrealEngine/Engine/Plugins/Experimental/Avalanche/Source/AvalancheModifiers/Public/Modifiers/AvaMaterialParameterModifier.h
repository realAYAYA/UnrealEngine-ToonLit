// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaArrangeBaseModifier.h"
#include "UObject/ObjectPtr.h"
#include "AvaMaterialParameterModifier.generated.h"

class UMaterialInstanceDynamic;
class UTexture;

USTRUCT(BlueprintType)
struct FAvaMaterialParameterMap
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MaterialParameter")
	TMap<FName, float> ScalarParameters;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MaterialParameter")
	TMap<FName, FLinearColor> VectorParameters;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MaterialParameter")
	TMap<FName, TObjectPtr<UTexture>> TextureParameters;

	/** Matches the input parameter key map and removes all unused keys, does not touch current values */
	void MatchKeys(const FAvaMaterialParameterMap& InParameterMap);

	/** Apply those parameters on this Material Designer Instance */
	void Set(UMaterialInstanceDynamic* InMaterial);

	/** Read those parameters from this Material Designer Instance and save them */
	void Get(UMaterialInstanceDynamic* InMaterial);
};

/** This modifier sets specified dynamic materials parameters on an actor and its children */
UCLASS(MinimalAPI, BlueprintType)
class UAvaMaterialParameterModifier : public UAvaArrangeBaseModifier
{
	GENERATED_BODY()

public:
	UAvaMaterialParameterModifier();

	AVALANCHEMODIFIERS_API void SetMaterialParameters(const FAvaMaterialParameterMap& InParameterMap);
	const FAvaMaterialParameterMap& GetMaterialParameters() const
	{
		return MaterialParameters;
	}

	AVALANCHEMODIFIERS_API void SetUpdateChildren(bool bInUpdateChildren);
	bool GetUpdateChildren() const
	{
		return bUpdateChildren;
	}

protected:
	//~ Begin UObject
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject

	//~ Begin UActorModifierCoreBase
	virtual void OnModifierCDOSetup(FActorModifierCoreMetadata& InMetadata) override;
	virtual void OnModifierEnabled(EActorModifierCoreEnableReason InReason) override;
	virtual void OnModifierDisabled(EActorModifierCoreDisableReason InReason) override;
	virtual void OnModifiedActorTransformed() override;
	virtual void RestorePreState() override;
	virtual void Apply() override;
	//~ End UActorModifierCoreBase

	//~ Begin IAvaSceneTreeUpdateModifierExtension
	virtual void OnSceneTreeTrackedActorChildrenChanged(int32 InIdx, const TSet<TWeakObjectPtr<AActor>>& InPreviousChildrenActors, const TSet<TWeakObjectPtr<AActor>>& InNewChildrenActors) override;
	virtual void OnSceneTreeTrackedActorDirectChildrenChanged(int32 InIdx, const TArray<TWeakObjectPtr<AActor>>& InPreviousChildrenActors, const TArray<TWeakObjectPtr<AActor>>& InNewChildrenActors) override;
	//~ End IAvaSceneTreeUpdateModifierExtension

	/** Read and save original values */
	void SaveMaterialParameters();
	/** Write and restore original values */
	void RestoreMaterialParameters();

#if WITH_EDITOR
	/** Called when a property changes, used to detect material changes */
	virtual void OnActorPropertyChanged(UObject* InObject, FPropertyChangedEvent& InChangeEvent);
#endif

	void ScanActorMaterials();

	void OnMaterialParametersChanged();
	void OnUpdateChildrenChanged();

	virtual void OnActorMaterialAdded(UMaterialInstanceDynamic* InAdded) {}
	virtual void OnActorMaterialRemoved(UMaterialInstanceDynamic* InRemoved) {}

	/** Checks if this actor has a Material Designer Instance or that we already track one */
	bool IsActorSupported(const AActor* InActor) const;

	/** Retrieves all Material Designer Instance from a primitive component */
	TSet<UMaterialInstanceDynamic*> GetComponentDynamicMaterials(const UPrimitiveComponent* InComponent) const;

	/** Which parameters should we set on the Material Designer Instance,
	 * use EditCondition="bShowMaterialParameters && bShowMaterialParameters" otherwise edit inline boolean appear in details when it should not */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetMaterialParameters", Getter="GetMaterialParameters", Category="MaterialParameter", meta=(EditCondition="bShowMaterialParameters && bShowMaterialParameters", EditConditionHides, AllowPrivateAccess="true"))
	FAvaMaterialParameterMap MaterialParameters;

	/** Used to restore Material Designer Instance parameters to their original state */
	UPROPERTY()
	TMap<TWeakObjectPtr<UMaterialInstanceDynamic>, FAvaMaterialParameterMap> SavedMaterialParameters;

	/** Filter material type for child modifiers */
	UPROPERTY(Transient)
	TSubclassOf<UMaterialInstanceDynamic> MaterialClass;

	/** Will also look into attached children actors */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetUpdateChildren", Getter="GetUpdateChildren", Category="MaterialParameter", meta=(AllowPrivateAccess="true"))
	bool bUpdateChildren = true;

#if WITH_EDITORONLY_DATA
	/** Used by child classes to override MaterialParameters */
	UPROPERTY(Transient)
	bool bShowMaterialParameters = true;
#endif
};
