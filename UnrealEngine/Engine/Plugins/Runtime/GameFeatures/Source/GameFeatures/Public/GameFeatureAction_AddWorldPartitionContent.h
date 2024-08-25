// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFeatureAction.h"
#include "GameFeatureAction_AddWorldPartitionContent.generated.h"

class UExternalDataLayerAsset;

/**
 *
 */
UCLASS(meta = (DisplayName = "Add World Partition Content"))
class GAMEFEATURES_API UGameFeatureAction_AddWorldPartitionContent : public UGameFeatureAction
{
	GENERATED_UCLASS_BODY()

public:
	//~ Begin UGameFeatureAction interface
	virtual void OnGameFeatureRegistering() override;
	virtual void OnGameFeatureUnregistering() override;
	virtual void OnGameFeatureActivating() override;
	virtual void OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context) override;
	//~ End UGameFeatureAction interface

#if WITH_EDITOR
	//~ Begin UObject interface
	virtual void PreEditChange(FProperty* PropertyThatWillChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PreEditUndo() override;
	virtual void PostEditUndo() override;
	//~ End UObject interface

	const TObjectPtr<const UExternalDataLayerAsset>& GetExternalDataLayerAsset() const { return ExternalDataLayerAsset; }
#endif

private:
#if WITH_EDITOR
	void OnExternalDataLayerAssetChanged(const UExternalDataLayerAsset* OldAsset, const UExternalDataLayerAsset* NewAsset);
#endif

	/** Used to detect changes on the Data Layer Asset in the action. */
	TWeakObjectPtr<const UExternalDataLayerAsset> PreEditChangeExternalDataLayerAsset;
	TWeakObjectPtr<const UExternalDataLayerAsset> PreEditUndoExternalDataLayerAsset;

	/** External Data Layer used by this action. */
	UPROPERTY(EditAnywhere, Category = DataLayer)
	TObjectPtr<const UExternalDataLayerAsset> ExternalDataLayerAsset;

	friend class UGameFeatureActionConvertContentBundleWorldPartitionBuilder;
};