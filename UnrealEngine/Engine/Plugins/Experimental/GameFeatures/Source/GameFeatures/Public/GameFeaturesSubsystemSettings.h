// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameFeatureData.h"
#include "GameFeaturesSubsystemSettings.generated.h"

/** Settings for the Game Features framework */
UCLASS(config=Game, defaultconfig, meta = (DisplayName = "Game Features"))
class GAMEFEATURES_API UGameFeaturesSubsystemSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UGameFeaturesSubsystemSettings();

	/** State/Bundle to always load on clients */
	static const FName LoadStateClient;

	/** State/Bundle to always load on dedicated server */
	static const FName LoadStateServer;

	/** Name of a singleton class to spawn as the game feature project policy. If empty, it will spawn the default one (UDefaultGameFeaturesProjectPolicies) */
	UPROPERTY(config, EditAnywhere, Category=DefaultClasses, meta=(MetaClass="/Script/GameFeatures.GameFeaturesProjectPolicies", DisplayName="Game Feature Project Policy Class", ConfigRestartRequired=true))
	FSoftClassPath GameFeaturesManagerClassName;

	/** List of plugins that are forcibly disabled (e.g., via a hotfix) */
	UPROPERTY(config, EditAnywhere, Category=GameFeatures)
	TArray<FString> DisabledPlugins;

	/** List of metadata (additional keys) to try parsing from the .uplugin to provide to FGameFeaturePluginDetails */
	UPROPERTY(config, EditAnywhere, Category=GameFeatures)
	TArray<FString> AdditionalPluginMetadataKeys;

	UE_DEPRECATED(5.0, "Use IsValidGameFeaturePlugin() instead")
	FString BuiltInGameFeaturePluginsFolder;

private:
	// Cached list of all GameFeature plugin directories (including extension versions)
	mutable TArray<FString> BuiltInGameFeaturePluginsFolders;

public:
	// Returns true if the specified (fully qualified) path is a game feature plugin
	bool IsValidGameFeaturePlugin(const FString& PluginDescriptorFilename) const;

};