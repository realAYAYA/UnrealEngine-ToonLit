// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFeatureAction.generated.h"

class UGameFeatureData;
struct FGameFeatureActivatingContext;
struct FGameFeatureDeactivatingContext;
struct FAssetBundleData;

/** Represents an action to be taken when a game feature is activated */
UCLASS(DefaultToInstanced, EditInlineNew, Abstract)
class GAMEFEATURES_API UGameFeatureAction : public UObject
{
	GENERATED_BODY()

public:
	virtual UGameFeatureData* GetGameFeatureData() const;

	/** Called when the object owning the action is registered for possible activation, this is called even if a feature never activates */
	virtual void OnGameFeatureRegistering() {}

	/** Called to unregister an action, it will not be activated again without being registered again */
	virtual void OnGameFeatureUnregistering() {}
	
	/** Called to indicate that a feature is being loaded for activation in the near future */
	virtual void OnGameFeatureLoading() {}

	/** Called to indicate that a feature is being unloaded */
	virtual void OnGameFeatureUnloading() {}

	/** Called when the feature is actually applied */
	virtual void OnGameFeatureActivating(FGameFeatureActivatingContext& Context);

	/** Older-style activation function with no context, called by base class if context version is not overridden */
	virtual void OnGameFeatureActivating() {}

	/** Called when game feature is deactivated, it may be activated again in the near future */
	virtual void OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context) {}

	/** Returns whether the action game feature plugin is registered or not. */
	bool IsGameFeaturePluginRegistered() const;

	/** Returns whether the action game feature plugin is active or not. */
	bool IsGameFeaturePluginActive() const;

#if WITH_EDITORONLY_DATA
	virtual void AddAdditionalAssetBundleData(FAssetBundleData& AssetBundleData) {}
#endif
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
