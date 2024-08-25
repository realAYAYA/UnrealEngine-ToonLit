// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Model/IDMMaterialBuildStateInterface.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Model/DMMaterialBuildUtils.h"

class UDMMaterialLayerObject;
class UDMMaterialSlot;
class UDMMaterialStage;
class UDMMaterialStageInput;
class UDMMaterialStageSource;
class UDMMaterialStageThroughput;
class UDynamicMaterialModel;
class UMaterial;
class UMaterialExpression;
class UMaterialExpressionAppendVector;
class UMaterialExpressionComponentMask;
struct FDMExpressionInput;
struct FDMMaterialBuildUtils;
struct FDMMaterialStageConnection;

/**
 * BuildState is a class that stores the current state of a material that is being built.
 * It stores useful lists of UMaterialExpressions relating to various object within the builder, such a Stages or Sources.
 * It is an entirely transient object. It is not meant to be saved outside of the material building processs.
 * It also provides some helper functions for creating UMaterialExpressions.
 */
struct DYNAMICMATERIALEDITOR_API FDMMaterialBuildState : public IDMMaterialBuildStateInterface, public TSharedFromThis<FDMMaterialBuildState>
{
	FDMMaterialBuildState(UMaterial* InDynamicMaterial, UDynamicMaterialModel* InMaterialModel, bool bInDirtyAssets = true);

	virtual ~FDMMaterialBuildState() override;

	virtual UMaterial* GetDynamicMaterial() const override;

	virtual UDynamicMaterialModel* GetMaterialModel() const override;

	bool ShouldDirtyAssets() const { return bDirtyAssets; }

	void SetIgnoreUVs();

	bool IsIgnoringUVs() const { return bIgnoreUVs; }

	void SetPreviewMaterial();

	bool IsPreviewMaterial() const { return bIsPreviewMaterial; }

	virtual IDMMaterialBuildUtilsInterface& GetBuildUtils() const override;

	FExpressionInput* GetMaterialProperty(EDMMaterialPropertyType InProperty) const;

	/** Slots */
	bool HasSlot(const UDMMaterialSlot* InSlot) const;

	const TArray<UMaterialExpression*>& GetSlotExpressions(const UDMMaterialSlot* InSlot) const;

	UMaterialExpression* GetLastSlotExpression(const UDMMaterialSlot* InSlot) const;

	void AddSlotExpressions(const UDMMaterialSlot* InSlot, const TArray<UMaterialExpression*>& InSlotExpressions);

	bool HasSlotProperties(const UDMMaterialSlot* InSlot) const;

	void AddSlotPropertyExpressions(const UDMMaterialSlot* InSlot, const TMap<EDMMaterialPropertyType, 
		TArray<UMaterialExpression*>>& InSlotPropertyExpressions);

	const TMap<EDMMaterialPropertyType, TArray<UMaterialExpression*>>& GetSlotPropertyExpressions(const UDMMaterialSlot* InSlot);

	UMaterialExpression* GetLastSlotPropertyExpression(const UDMMaterialSlot* InSlot, EDMMaterialPropertyType InMaterialProperty) const;

	TArray<const UDMMaterialSlot*> GetSlots() const;

	const TMap<const UDMMaterialSlot*, TArray<UMaterialExpression*>>& GetSlotMap() const;

	/** Layers */
	bool HasLayer(const UDMMaterialLayerObject* InLayer) const;

	const TArray<UMaterialExpression*>& GetLayerExpressions(const UDMMaterialLayerObject* InLayer) const;

	UMaterialExpression* GetLastLayerExpression(const UDMMaterialLayerObject* InLayer) const;

	void AddLayerExpressions(const UDMMaterialLayerObject* InLayer, const TArray<UMaterialExpression*>& InLayerExpressions);

	TArray<const UDMMaterialLayerObject*> GetLayers() const;

	const TMap<const UDMMaterialLayerObject*, TArray<UMaterialExpression*>>& GetLayerMap() const;

	/** Stages */
	bool HasStage(const UDMMaterialStage* InStage) const;

	const TArray<UMaterialExpression*>& GetStageExpressions(const UDMMaterialStage* InStage) const;

	UMaterialExpression* GetLastStageExpression(const UDMMaterialStage* InStage) const;

	void AddStageExpressions(const UDMMaterialStage* InStage, const TArray<UMaterialExpression*>& InStageExpressions);

	TArray<const UDMMaterialStage*> GetStages() const;

	const TMap<const UDMMaterialStage*, TArray<UMaterialExpression*>>& GetStageMap() const;

	/** Stage Sources */
	bool HasStageSource(const UDMMaterialStageSource* InStageSource) const;

	const TArray<UMaterialExpression*>& GetStageSourceExpressions(const UDMMaterialStageSource* InStageSource) const;

	UMaterialExpression* GetLastStageSourceExpression(const UDMMaterialStageSource* InStageSource) const;

	void AddStageSourceExpressions(const UDMMaterialStageSource* InStageSource, 
		const TArray<UMaterialExpression*>& InStageSourceExpressions);

	TArray<const UDMMaterialStageSource*> GetStageSources() const;

	const TMap<const UDMMaterialStageSource*, TArray<UMaterialExpression*>>& GetStageSourceMap() const;

	/** Material Values */
	virtual bool HasValue(const UDMMaterialValue* InValue) const override;

	virtual const TArray<UMaterialExpression*>& GetValueExpressions(const UDMMaterialValue* InValue) const override;

	virtual UMaterialExpression* GetLastValueExpression(const UDMMaterialValue* InValue) const override;

	virtual void AddValueExpressions(const UDMMaterialValue* InValue, const TArray<UMaterialExpression*>& InValueExpressions) override;

	virtual TArray<const UDMMaterialValue*> GetValues() const override;

	virtual const TMap<const UDMMaterialValue*, TArray<UMaterialExpression*>>& GetValueMap() const override;

	/** Callbacks */
	bool HasCallback(const UDMMaterialStageSource* InCallback) const;

	const TArray<UDMMaterialStageSource*>& GetCallbackExpressions(const UDMMaterialStageSource* InCallback) const;

	void AddCallbackExpressions(const UDMMaterialStageSource* InCallback, const TArray<UDMMaterialStageSource*>& InCallbackExpressions);

	TArray<const UDMMaterialStageSource*> GetCallbacks() const;

	const TMap<const UDMMaterialStageSource*, TArray<UDMMaterialStageSource*>>& GetCallbackMap() const;

	/** Other */
	virtual void AddOtherExpressions(const TArray<UMaterialExpression*>& InOtherExpressions) override;

	virtual const TSet<UMaterialExpression*>& GetOtherExpressions() override;

private:
	UMaterial* DynamicMaterial = nullptr;
	UDynamicMaterialModel* MaterialModel = nullptr;
	bool bDirtyAssets;
	bool bIgnoreUVs;
	bool bIsPreviewMaterial;
	TSharedRef<FDMMaterialBuildUtils> Utils;

	TMap<const UDMMaterialValue*, TArray<UMaterialExpression*>> Values;
	TMap<const UDMMaterialSlot*, TArray<UMaterialExpression*>> Slots;
	TMap<const UDMMaterialSlot*, TMap<EDMMaterialPropertyType, TArray<UMaterialExpression*>>> SlotProperties;
	TMap<const UDMMaterialLayerObject*, TArray<UMaterialExpression*>> Layers;
	TMap<const UDMMaterialStage*, TArray<UMaterialExpression*>> Stages;
	TMap<const UDMMaterialStageSource*, TArray<UMaterialExpression*>> StageSources;
	TMap<const UDMMaterialStageSource*, TArray<UDMMaterialStageSource*>> Callbacks;
	TSet<UMaterialExpression*> OtherExpressions;
};
