// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/DMMaterialComponent.h"
#include "DMEDefs.h"
#include "DMMaterialSlot.generated.h"

class FScopedTransaction;
class UDMMaterialLayerObject;
class UDMMaterialSlot;
class UDMMaterialStage;
class UDynamicMaterialModelEditorOnlyData;
class UMaterial;
struct FDMMaterialBuildState;
struct FDMMaterialLayer;

DECLARE_MULTICAST_DELEGATE_OneParam(FDMOnMaterialSlotConnectorsUpdated, UDMMaterialSlot*);
DECLARE_MULTICAST_DELEGATE_OneParam(FDMOnMaterialSlotPropertiesUpdated, UDMMaterialSlot*);
DECLARE_MULTICAST_DELEGATE_OneParam(FDMOnMaterialSlotLayersUpdated, UDMMaterialSlot*);

/**
 * A list of operations/inputs daisy chained together to produce an output.
 */
UCLASS(BlueprintType, ClassGroup = "Material Designer", Meta = (DisplayName = "Material Designer Slot"))
class DYNAMICMATERIALEDITOR_API UDMMaterialSlot : public UDMMaterialComponent
{
	friend class SDMMaterialSlot;
	friend class SDMSlot;

	GENERATED_BODY()

public:
	static const FString LayersPathToken;

	UDMMaterialSlot();

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UDynamicMaterialModelEditorOnlyData* GetMaterialModelEditorOnlyData() const;

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	int32 GetIndex() const { return Index; }

	void SetIndex(int32 InNewIndex) { Index = InNewIndex; }

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	FText GetDescription() const;

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	const TArray<EDMValueType>& GetOutputConnectorTypesForMaterialProperty(EDMMaterialPropertyType InMaterialProperty) const;

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	TSet<EDMValueType> GetAllOutputConnectorTypes() const;

	UFUNCTION(BlueprintPure, Category = "Material Designer", meta = (DisplayName = "Get Layer"))
	UDMMaterialLayerObject* GetLayer(int32 InLayerIndex) const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	UDMMaterialLayerObject* FindLayer(const UDMMaterialStage* InBaseOrMask) const;

	UFUNCTION(BlueprintPure, Category = "Material Designer", meta = (DisplayName = "Get Layers"))
	TArray<UDMMaterialLayerObject*> BP_GetLayers() const;

	const TArray<TObjectPtr<UDMMaterialLayerObject>>& GetLayers() const { return LayerObjects; }

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	UDMMaterialLayerObject* AddLayer(EDMMaterialPropertyType InMaterialProperty, UDMMaterialStage* InNewBase);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	UDMMaterialLayerObject* AddLayerWithMask(EDMMaterialPropertyType InMaterialProperty, UDMMaterialStage* InNewBase, UDMMaterialStage* InNewMask);

	// Adds the specified layer to the end of the layer list
	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	bool PasteLayer(UDMMaterialLayerObject* InLayer);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	bool RemoveLayer(UDMMaterialLayerObject* InLayer);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	bool MoveLayer(UDMMaterialLayerObject* InLayer, int32 InNewIndex);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	bool MoveLayerBefore(UDMMaterialLayerObject* InLayer, UDMMaterialLayerObject* InBeforeLayer = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	bool MoveLayerAfter(UDMMaterialLayerObject* InLayer, UDMMaterialLayerObject* InAfterLayer = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	UDMMaterialLayerObject* GetLastLayerForMaterialProperty(EDMMaterialPropertyType InMaterialProperty) const;

	void UpdateOutputConnectorTypes();
	void UpdateMaterialProperties();

	FDMOnMaterialSlotConnectorsUpdated& GetOnConnectorsUpdateDelegate() { return OnConnectorsUpdateDelegate; }
	FDMOnMaterialSlotPropertiesUpdated& GetOnPropertiesUpdateDelegate() { return OnPropertiesUpdateDelegate; }
	FDMOnMaterialSlotLayersUpdated& GetOnLayersUpdateDelegate() { return OnLayersUpdateDelegate; }

	void GenerateExpressions(const TSharedRef<FDMMaterialBuildState>& InBuildState) const;

	const TMap<TWeakObjectPtr<UDMMaterialSlot>, int32>& GetSlotsReferencedBy() const { return SlotsReferencedBy; }

	UFUNCTION(BlueprintCallable, Category = "Material Designer", Meta = (DisplayName="Get Slots Referenced By"))
	TArray<UDMMaterialSlot*> K2_GetSlotsReferencedBy() const;

	/** Returns true if a new associated is created */
	bool ReferencedBySlot(UDMMaterialSlot* InOtherSlot);

	/** Returns true if all associations have been removed */
	bool UnreferencedBySlot(UDMMaterialSlot* InOtherSlot);

	bool IsEditingLayers() const { return bIsEditingLayers; }
	void SetEditingLayers(bool bInIsEditing) { bIsEditingLayers = bInIsEditing; }

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UMaterial* GetPreviewMaterial(EDMMaterialLayerStage InLayerStage);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	bool SetLayerMaterialPropertyAndReplaceOthers(UDMMaterialLayerObject* InLayer, EDMMaterialPropertyType InMaterialProperty, 
		EDMMaterialPropertyType InReplaceWithProperty);

	//~ Begin UDMMaterialComponent
	virtual void Update(EDMUpdateType InUpdateType) override;
	virtual void DoClean() override;
	virtual bool IsRootComponent() const override { return true; }
	virtual FString GetComponentPathComponent() const override;
	virtual UDMMaterialComponent* GetParentComponent() const override { return nullptr; }
	virtual void PostEditorDuplicate(UDynamicMaterialModel* InMaterialModel, UDMMaterialComponent* InParent) override;
	//~ End UDMMaterialComponent

	//~ Begin UObject
	virtual bool Modify(bool bInAlwaysMarkDirty = true) override;
	virtual void PostEditUndo() override;
	virtual void PostLoad() override;
	//~ End UObject

protected:
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Material Designer")
	int32 Index;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Material Designer")
	TArray<TObjectPtr<UDMMaterialLayerObject>> LayerObjects;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Material Designer")
	TMap<EDMMaterialPropertyType, FDMMaterialSlotOutputConnectorTypes> OutputConnectorTypes;

	UPROPERTY()
	TMap<TWeakObjectPtr<UDMMaterialSlot>, int32> SlotsReferencedBy;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, DuplicateTransient, TextExportTransient, Category = "Material Designer")
	bool bIsEditingLayers;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, DuplicateTransient, TextExportTransient, Category = "Material Designer")
	TObjectPtr<UMaterial> BasePreviewMaterial;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, DuplicateTransient, TextExportTransient, Category = "Material Designer")
	TObjectPtr<UMaterial> MaskPreviewMaterial;

	FDMOnMaterialSlotConnectorsUpdated OnConnectorsUpdateDelegate;
	FDMOnMaterialSlotPropertiesUpdated OnPropertiesUpdateDelegate;
	FDMOnMaterialSlotLayersUpdated OnLayersUpdateDelegate;

	void CreatePreviewMaterial(EDMMaterialLayerStage InLayerStage);
	void UpdatePreviewMaterial(EDMMaterialLayerStage InLayerStage);
	void UpdateBasePreviewMaterial(const TSharedRef<FDMMaterialBuildState>& InBuildState);
	void UpdateBasePreviewMaterialProperty(const TSharedRef<FDMMaterialBuildState>& InBuildState, EDMMaterialPropertyType InBaseProperty);
	void UpdateBasePreviewMaterialFull(const TSharedRef<FDMMaterialBuildState>& InBuildState);
	void UpdateMaskPreviewMaterial(const TSharedRef<FDMMaterialBuildState>& InBuildState);
	void UpdateMaskPreviewMaterialProperty(const TSharedRef<FDMMaterialBuildState>& InBuildState, EDMMaterialPropertyType InMaskProperty);
	void UpdateMaskPreviewMaterialMaskCombination(const TSharedRef<FDMMaterialBuildState>& InBuildState, EDMMaterialPropertyType InMaskProperty);
	void UpdatePreviewMaterialProperty(const TSharedRef<FDMMaterialBuildState>& InBuildState, UMaterial* InPreviewMaterial, EDMMaterialPropertyType InProperty);

	//~ Begin UDMMaterialComponent
	virtual void OnComponentAdded() override;
	virtual void OnComponentRemoved() override;
	virtual UDMMaterialComponent* GetSubComponentByPath(FDMComponentPath& InPath, const FDMComponentPathSegment& InPathSegment) const override;
	//~ End UDMMaterialComponent

private:
	UE_DEPRECATED(5.4, "Promoted to full UObjects.")
	UPROPERTY()
	TArray<FDMMaterialLayer> Layers;

	void ConvertDeprecatedLayers(TArray<FDMMaterialLayer>& InLayers);
};
