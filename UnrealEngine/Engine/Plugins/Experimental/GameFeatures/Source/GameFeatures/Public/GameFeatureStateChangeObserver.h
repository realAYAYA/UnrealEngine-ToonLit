// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameFeatureStateChangeObserver.generated.h"

class UGameFeatureData;
struct FGameFeatureDeactivatingContext;
struct FGameFeaturePauseStateChangeContext;

UINTERFACE(MinimalAPI)
class UGameFeatureStateChangeObserver : public UInterface
{
	GENERATED_BODY()
};

/**
 * This class is meant to be overridden in your game to handle game-specific reactions to game feature plugins
 * being mounted or unmounted
 *
 * Generally you should prefer to use UGameFeatureAction instances on your game feature data asset instead of
 * this, especially if any data is involved
 *
 * If you do use these, create them in your UGameFeaturesProjectPolicies subclass and register them via
 * AddObserver / RemoveObserver on UGameFeaturesSubsystem
 */
class GAMEFEATURES_API IGameFeatureStateChangeObserver
{
	GENERATED_BODY()

public:

	virtual void OnGameFeatureCheckingStatus(const FString& PluginURL) {}

	virtual void OnGameFeatureTerminating(const FString& PluginURL) {}

	virtual void OnGameFeatureRegistering(const UGameFeatureData* GameFeatureData, const FString& PluginName, const FString& PluginURL) {}

	virtual void OnGameFeatureUnregistering(const UGameFeatureData* GameFeatureData, const FString& PluginName, const FString& PluginURL) {}

	virtual void OnGameFeatureLoading(const UGameFeatureData* GameFeatureData, const FString& PluginURL) {}

	virtual void OnGameFeatureActivating(const UGameFeatureData* GameFeatureData, const FString& PluginURL) {}

	virtual void OnGameFeatureDeactivating(const UGameFeatureData* GameFeatureData, FGameFeatureDeactivatingContext& Context, const FString& PluginURL) {}

	/** Called whenever a GameFeature State either pauses or resumes work without transitioning out of that state.
		EX: Downloading paused due to a users cellular data settings or the user taking a pause action. We
		may not yet transition to a download error, but want a way to observe this behavior. */
	virtual void OnGameFeaturePauseChange(const FString& PluginURL, const FString& PluginName, FGameFeaturePauseStateChangeContext& Context) {}
};
