// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/DMMaterialComponent.h"
#include "DMEDefs.h"
#include "UObject/StrongObjectPtr.h"
#include "DMMaterialStageSource.generated.h"

class UDMMaterialSlot;
class UDMMaterialStageInputTextureUV;
class UMaterialExpression;
struct FDMMaterialBuildState;

/**
 * A node which produces an output.
 */
UCLASS(Abstract, BlueprintType, ClassGroup = "Material Designer", meta = (DisplayName = "Material Designer Stage Source"))
class DYNAMICMATERIALEDITOR_API UDMMaterialStageSource : public UDMMaterialComponent
{
	GENERATED_BODY()

	friend class UDMMaterialStage;

public:
	static const TArray<TStrongObjectPtr<UClass>>& GetAvailableSourceClasses();

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UDMMaterialStage* GetStage() const;

	/* Returns a description of the stage for which this is the source. */
	UFUNCTION(BlueprintPure, Category = "Material Designer")
	virtual FText GetStageDescription() const { return GetComponentDescription(); }

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	const TArray<FDMMaterialStageConnector>& GetOutputConnectors() const { return OutputConnectors; }

	virtual void GetMaskAlphaBlendNode(const TSharedRef<FDMMaterialBuildState>& InBuildState, UMaterialExpression*& OutExpression, 
		int32& OutOutputIndex, int32& OutOutputChannel) const;

	virtual void GenerateExpressions(const TSharedRef<FDMMaterialBuildState>& InBuildState) const
		PURE_VIRTUAL(UDMMaterialStageSource::GenerateExpressions)

	virtual void AddExpressionProperties(const TArray<UMaterialExpression*>& Expressions) const {}

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UMaterial* GetPreviewMaterial();

	void UpdateOntoPreviewMaterial(UMaterial* ExternalPreviewMaterial);

	/** 
	 * Returns the output index (channel WHOLE_CHANNEL) if this expression has pre-masked outputs.
	 * Returns INDEX_NONE if it is not supported.
	 */
	virtual int32 GetInnateMaskOutput(int32 OutputIndex, int32 OutputChannels) const;

	/**
	 * Given an output index, may return an override for output channels on that output.
	 * E.g. The texture sample alpha output may override to FOURTH_CHANNEL.
	 * Returns INDEX_NONE with no override.
	 */
	virtual int32 GetOutputChannelOverride(int32 InOutputIndex) const { return INDEX_NONE; }

	virtual bool UpdateStagePreviewMaterial(UDMMaterialStage* InStage, UMaterial* InPreviewMaterial, UMaterialExpression*& OutMaterialExpression, 
		int32& OutputIndex);

	//~ Begin FNotifyHook
	virtual void NotifyPostChange(const FPropertyChangedEvent& InPropertyChangedEvent, class FEditPropertyChain* InPropertyThatChanged) override;
	//~ End FNotifyHook

	//~ Begin UDMMaterialComponent
	virtual void Update(EDMUpdateType InUpdateType) override;
	virtual void DoClean() override;
	virtual UDMMaterialComponent* GetParentComponent() const override;
	virtual void PostEditorDuplicate(UDynamicMaterialModel* InMaterialModel, UDMMaterialComponent* InParent) override;
	//~ End UDMMaterialComponent

	//~ Begin UObject
	virtual void PostEditUndo() override;
	//~ End UObject

protected:
	static TArray<TStrongObjectPtr<UClass>> SourceClasses;

	static void GenerateClassList();

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Material Designer")
	TArray<FDMMaterialStageConnector> OutputConnectors;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, DuplicateTransient, TextExportTransient, Category = "Material Designer")
	TObjectPtr<UMaterial> PreviewMaterial;

	UDMMaterialStageSource();

	void CreatePreviewMaterial();
	virtual void UpdatePreviewMaterial(UMaterial* InPreviewMaterial = nullptr);

	//~ Begin UDMMaterialComponent
	virtual void OnComponentAdded() override;
	//~ End UDMMaterialComponent
};
