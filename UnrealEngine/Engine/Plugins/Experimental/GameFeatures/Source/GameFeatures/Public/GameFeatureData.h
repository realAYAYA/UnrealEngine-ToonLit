// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/AssetManagerTypes.h"
#include "GameFeatureAction.h"

#include "GameFeatureData.generated.h"

/** Data related to a game feature, a collection of code and content that adds a separable discrete feature to the game */
UCLASS()
class GAMEFEATURES_API UGameFeatureData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Method to get where the primary assets should scanned from in the plugin hierarchy */
	const TArray<FPrimaryAssetTypeInfo>& GetPrimaryAssetTypesToScan() const { return PrimaryAssetTypesToScan; }

#if WITH_EDITOR
	TArray<FPrimaryAssetTypeInfo>& GetPrimaryAssetTypesToScan() { return PrimaryAssetTypesToScan; }
#endif //if WITH_EDITOR

	/** Method to process the base ini file for the plugin during loading */
	void InitializeBasePluginIniFile(const FString& PluginInstalledFilename) const;

	/** Method to process ini files for the plugin during activation */
	void InitializeHierarchicalPluginIniFiles(const FString& PluginInstalledFilename) const;

public:
	//~UPrimaryDataAsset interface
#if WITH_EDITORONLY_DATA
	virtual void UpdateAssetBundleData() override;
#endif
	//~End of UPrimaryDataAsset interface

	//~UObject interface
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(TArray<FText>& ValidationErrors) override;
#endif
	//~End of UObject interface

	const TArray<UGameFeatureAction*>& GetActions() const { return Actions; }

private:
	/** Internal helper function to reload config data on objects as a result of a plugin INI being loaded */
	void ReloadConfigs(FConfigFile& PluginConfig) const;

protected:

	/** List of actions to perform as this game feature is loaded/activated/deactivated/unloaded */
	UPROPERTY(EditDefaultsOnly, Instanced, Category="Actions")
	TArray<TObjectPtr<UGameFeatureAction>> Actions;

	/** List of asset types to scan at startup */
	UPROPERTY(EditAnywhere, Category="Game Feature | Asset Manager", meta=(TitleProperty="PrimaryAssetType"))
	TArray<FPrimaryAssetTypeInfo> PrimaryAssetTypesToScan;
};
