// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameFeaturesProjectPolicies.h"
#include "GameFeaturesSubsystemSettings.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameFeaturesProjectPolicies)

void UDefaultGameFeaturesProjectPolicies::InitGameFeatureManager()
{
	UE_LOG(LogGameFeatures, Log, TEXT("Scanning for built-in game feature plugins"));

	auto AdditionalFilter = [&](const FString& PluginFilename, const FGameFeaturePluginDetails& PluginDetails, FBuiltInGameFeaturePluginBehaviorOptions& OutOptions) -> bool
	{
		// By default, force all initially loaded plugins to synchronously load, this overrides the behavior of GameFeaturePlugin.AsyncLoad which will be used for later loads
		OutOptions.bForceSyncLoading = true;

		// By default, no plugins are filtered so we expect all built-in dependencies to be created before their parent GFPs
		OutOptions.bLogWarningOnForcedDependencyCreation = true;

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

bool UGameFeaturesProjectPolicies::WillPluginBeCooked(const FString& PluginFilename, const FGameFeaturePluginDetails& PluginDetails) const
{
	return true;
}

TValueOrError<FString, FString> UGameFeaturesProjectPolicies::ResolvePluginDependency(const FString& PluginURL, const FString& DependencyName) const
{
	FString DependencyURL;
	bool bResolvedDependency = false;

	// Check if UGameFeaturesSubsystem is already aware of it
	if (UGameFeaturesSubsystem::Get().GetPluginURLByName(DependencyName, DependencyURL))
	{
		bResolvedDependency = true;
	}
	// Check if the dependency plugin exists yet (should be true for all built-in plugins)
	else if (TSharedPtr<IPlugin> DependencyPlugin = IPluginManager::Get().FindPlugin(DependencyName))
	{
		// It could still be a GFP, but state machine may not have been created for it yet
		// Check if it is a built-in GFP
		if (!DependencyPlugin->GetDescriptorFileName().IsEmpty() &&
			GetDefault<UGameFeaturesSubsystemSettings>()->IsValidGameFeaturePlugin(FPaths::ConvertRelativePathToFull(DependencyPlugin->GetDescriptorFileName())))
		{
			DependencyURL = UGameFeaturesSubsystem::GetPluginURL_FileProtocol(DependencyPlugin->GetDescriptorFileName());
		}

		bResolvedDependency = true;
	}

	if (bResolvedDependency)
	{
		return MakeValue(MoveTemp(DependencyURL));
	}

	return MakeError(TEXT("NotFound"));
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
