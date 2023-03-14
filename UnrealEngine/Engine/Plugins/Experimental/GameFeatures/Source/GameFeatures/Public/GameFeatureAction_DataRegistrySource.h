// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFeatureAction.h"
#include "Engine/CurveTable.h"
#include "Engine/DataTable.h"
#include "GameFeatureAction_DataRegistrySource.generated.h"

/** Defines which source assets to add and conditions for adding */
USTRUCT()
struct FDataRegistrySourceToAdd
{
	GENERATED_BODY()

	FDataRegistrySourceToAdd()
		: AssetPriority(0)
		, bClientSource(false)
		, bServerSource(false)
	{}

	/** Name of the registry to add to */
	UPROPERTY(EditAnywhere, Category="Registry Data")
	FName RegistryToAddTo;

	/** Priority to use when adding to the registry.  Higher priorities are searched first */
	UPROPERTY(EditAnywhere, Category = "Registry Data")
	int32 AssetPriority;

	/** Should this component be added for clients */
	UPROPERTY(EditAnywhere, Category = "Registry Data")
	uint8 bClientSource : 1;

	/** Should this component be added on servers */
	UPROPERTY(EditAnywhere, Category = "Registry Data")
	uint8 bServerSource : 1;

	/** Link to the data table to add to the registry */
	UPROPERTY(EditAnywhere, Category = "Registry Data")
	TSoftObjectPtr<UDataTable> DataTableToAdd;

	/** Link to the curve table to add to the registry */
	UPROPERTY(EditAnywhere, Category = "Registry Data")
	TSoftObjectPtr<UCurveTable> CurveTableToAdd;
};

/** Specifies a list of source assets to add to Data Registries when this feature is activated */
UCLASS(MinimalAPI, meta = (DisplayName = "Add Data Registry Source"))
class UGameFeatureAction_DataRegistrySource : public UGameFeatureAction
{
	GENERATED_BODY()

public:
	virtual void OnGameFeatureRegistering() override;
	virtual void OnGameFeatureUnregistering() override;
	virtual void OnGameFeatureActivating() override;
	virtual void OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context) override;

	/** If true, we should load the sources at registration time instead of activation time */
	virtual bool ShouldPreloadAtRegistration();

#if WITH_EDITORONLY_DATA
	virtual void AddAdditionalAssetBundleData(FAssetBundleData& AssetBundleData) override;
#endif

	//~UObject interface
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(TArray<FText>& ValidationErrors) override;
#endif
	//~End of UObject interface

private:
	/** List of sources to add when this feature is activated */
	UPROPERTY(EditAnywhere, Category = "Registry Data")
	TArray<FDataRegistrySourceToAdd> SourcesToAdd;

	/** If true, this will preload the sources when the feature is registered in the editor to support the editor pickers */
	UPROPERTY(EditAnywhere, Category = "Registry Data")
	bool bPreloadInEditor;
};