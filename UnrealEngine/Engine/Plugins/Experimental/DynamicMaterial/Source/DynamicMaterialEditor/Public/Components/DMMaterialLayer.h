// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/DMMaterialComponent.h"
#include "DMDefs.h"
#include "Components/DMMaterialStage.h"
#include "DMMaterialLayer.generated.h"

class UDMMaterialEffectStack;
class UDMMaterialSlot;
class UDMMaterialStage;
class UMaterialExpression;

UCLASS(BlueprintType, ClassGroup = "Material Designer", Meta = (DisplayName = "Material Designer Layer"))
class DYNAMICMATERIALEDITOR_API UDMMaterialLayerObject : public UDMMaterialComponent
{
	GENERATED_BODY()

public:
	static const FString StagesPathToken;
	static const FString BasePathToken;
	static const FString MaskPathToken;
	static const FString EffectStackPathToken;

	using FStageCallbackFunc = TFunctionRef<void(UDMMaterialStage*)>;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	static UDMMaterialLayerObject* CreateLayer(UDMMaterialSlot* InSlot, EDMMaterialPropertyType InMaterialProperty, 
		const TArray<UDMMaterialStage*>& InStages);
	
	static UDMMaterialLayerObject* DeserializeFromString(UDMMaterialSlot* InOuter, const FString& InSerializedString);

	UDMMaterialLayerObject();

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UDMMaterialSlot* GetSlot() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	int32 FindIndex() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	const FText& GetLayerName() const { return LayerName; }

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetLayerName(const FText& InName);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	bool IsEnabled() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	bool SetEnabled(bool bInIsEnabled);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	EDMMaterialPropertyType GetMaterialProperty() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	bool SetMaterialProperty(EDMMaterialPropertyType InMaterialProperty);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	bool IsTextureUVLinkEnabled() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	bool SetTextureUVLinkEnabled(bool bInValue);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	bool ToggleTextureUVLinkEnabled();

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	UDMMaterialLayerObject* GetPreviousLayer(EDMMaterialPropertyType InUsingProperty, EDMMaterialLayerStage InSearchFor) const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	UDMMaterialLayerObject* GetNextLayer(EDMMaterialPropertyType InUsingProperty, EDMMaterialLayerStage InSearchFor) const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	bool CanMoveLayerAbove(UDMMaterialLayerObject* InLayer) const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	bool CanMoveLayerBelow(UDMMaterialLayerObject* InLayer) const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	UDMMaterialStage* GetStage(EDMMaterialLayerStage InStageType = EDMMaterialLayerStage::All, bool bInCheckEnabled = false) const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	TArray<UDMMaterialStage*> GetStages(EDMMaterialLayerStage InStageType = EDMMaterialLayerStage::All, bool bInCheckEnabled = false) const;

	const TArray<TObjectPtr<UDMMaterialStage>>& GetAllStages() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	EDMMaterialLayerStage GetStageType(const UDMMaterialStage* InStage) const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	UDMMaterialStage* GetFirstValidStage(EDMMaterialLayerStage InStageScope) const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	UDMMaterialStage* GetLastValidStage(EDMMaterialLayerStage InStageScope) const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	bool HasValidStage(const UDMMaterialStage* InStage) const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	bool HasValidStageOfType(EDMMaterialLayerStage InStageScope = EDMMaterialLayerStage::All) const;

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	bool IsStageEnabled(EDMMaterialLayerStage InStageScope = EDMMaterialLayerStage::All) const;

	/** Checks for the first enabled and valid stage. */
	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	UDMMaterialStage* GetFirstEnabledStage(EDMMaterialLayerStage InStageScope) const;

	/** Checks for the last enabled and valid stage. */
	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	UDMMaterialStage* GetLastEnabledStage(EDMMaterialLayerStage InStageScope) const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	bool SetStage(EDMMaterialLayerStage InStageType, UDMMaterialStage* InStage);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	bool AreAllStagesValid(EDMMaterialLayerStage InStageScope) const;

	/** Checks if both stages are enabled and valid */
	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	bool AreAllStagesEnabled(EDMMaterialLayerStage InStageScope) const;

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	bool IsStageBeingEdited(EDMMaterialLayerStage InStageScope = EDMMaterialLayerStage::All) const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	UDMMaterialStage* GetFirstStageBeingEdited(EDMMaterialLayerStage InStageScope) const;

	void ForEachValidStage(EDMMaterialLayerStage InStageScope, FStageCallbackFunc InCallback) const;

	void ForEachEnabledStage(EDMMaterialLayerStage InStageScope, FStageCallbackFunc InCallback) const;

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UDMMaterialEffectStack* GetEffectStack() const;

	FString SerializeToString() const;

	void GenerateExpressions(const TSharedRef<FDMMaterialBuildState>& InBuildState) const;

	bool ApplyEffects(const TSharedRef<FDMMaterialBuildState>& InBuildState, const UDMMaterialStage* InStage,
		TArray<UMaterialExpression*>& InOutStageExpressions, int32& InOutLastExpressionOutputChannel, int32& InOutLastExpressionOutputIndex) const;

	//~ Begin UDMMaterialComponent
	virtual UDMMaterialComponent* GetParentComponent() const override;
	virtual FString GetComponentPathComponent() const override;
	virtual FText GetComponentDescription() const override;
	virtual void Update(EDMUpdateType InUpdateType) override;
	virtual void PostEditorDuplicate(UDynamicMaterialModel* InMaterialModel, UDMMaterialComponent* InParent) override;
	//~ End UDMMaterialComponent

	//~ Begin UObject
	virtual bool Modify(bool bInAlwaysMarkDirty = true) override;
	virtual void PostEditUndo() override;
	//~ End UObject

protected:
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Material Designer")
	EDMMaterialPropertyType MaterialProperty = EDMMaterialPropertyType::None;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Material Designer")
	FText LayerName;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Material Designer")
	bool bEnabled;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Instanced, Category = "Material Designer")
	TArray<TObjectPtr<UDMMaterialStage>> Stages;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Instanced, Category = "Material Designer")
	TObjectPtr<UDMMaterialEffectStack> EffectStack;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Material Designer")
	bool bLinkedUVs;

	//~ Begin UDMMaterialComponent
	virtual UDMMaterialComponent* GetSubComponentByPath(FDMComponentPath& InPath, const FDMComponentPathSegment& InPathSegment) const override;
	virtual void OnComponentAdded() override;
	virtual void OnComponentRemoved() override;
	//~ End UDMMaterialComponent
};
