// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameFeaturesProjectPolicies.h"
#include "GameFeaturesSubsystem.h"
#include "GameFeaturesSubsystemSettings.h"
#include "Misc/CoreMisc.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameFeaturesProjectPolicies)

void UDefaultGameFeaturesProjectPolicies::InitGameFeatureManager()
{
	UE_LOG(LogGameFeatures, Log, TEXT("Scanning for built-in game feature plugins"));

	auto AdditionalFilter = [&](const FString& PluginFilename, const FGameFeaturePluginDetails& PluginDetails, FBuiltInGameFeaturePluginBehaviorOptions& OutOptions) -> bool
	{
		return true;
	};

	UGameFeaturesSubsystem::Get().LoadBuiltInGameFeaturePlugins(AdditionalFilter);
}

void UDefaultGameFeaturesProjectPolicies::GetGameFeatureLoadingMode(bool& bLoadClientData, bool& bLoadServerData) const
{
	// By default, load both unless we are a dedicated server or client only cooked build
	bLoadClientData = !IsRunningDedicatedServer();
	bLoadServerData = !IsRunningClientOnly();
}

const TArray<FName> UDefaultGameFeaturesProjectPolicies::GetPreloadBundleStateForGameFeature() const
{
	// By default, use the bundles corresponding to loading mode
	bool bLoadClientData, bLoadServerData;
	GetGameFeatureLoadingMode(bLoadClientData, bLoadServerData);

	TArray<FName> FeatureBundles;
	if (bLoadClientData)
	{
		FeatureBundles.Add(UGameFeaturesSubsystemSettings::LoadStateClient);
	}
	if (bLoadServerData)
	{
		FeatureBundles.Add(UGameFeaturesSubsystemSettings::LoadStateServer);
	}
	return FeatureBundles;
}

void UGameFeaturesProjectPolicies::ExplicitLoadGameFeaturePlugin(const FString& PluginURL, const FGameFeaturePluginLoadComplete& CompleteDelegate, const bool bActivateGameFeatures)
{
	if (bActivateGameFeatures)
	{
		UGameFeaturesSubsystem::Get().LoadAndActivateGameFeaturePlugin(PluginURL, CompleteDelegate);
	}
	else
	{
		UGameFeaturesSubsystem::Get().LoadGameFeaturePlugin(PluginURL, CompleteDelegate);
	}
}