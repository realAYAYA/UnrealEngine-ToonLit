// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFeatureAction_WorldActionBase.h"
#include "GameplayTagContainer.h"
#include "UObject/WeakObjectPtr.h"
#include "GameFeatureAction_AddInputConfig.generated.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS

struct FMappableConfigPair;

class APawn;
struct FComponentRequestHandle;

/**
 * Registers a Player Mappable Input config to the Game User Settings
 * 
 * Expects that local players are set up to use the EnhancedInput system.
 */
UCLASS(meta = (DisplayName = "Add Input Config"))
class UE_DEPRECATED(5.3, "UGameFeatureAction_AddInputConfig has been deprecated. Please use UGameFeatureAction_AddInputContextMapping instead.") LYRAGAME_API UGameFeatureAction_AddInputConfig : public UGameFeatureAction_WorldActionBase
{
	GENERATED_BODY()
	
public:
	//~UObject UGameFeatureAction
	virtual void OnGameFeatureRegistering() override;
	virtual void OnGameFeatureActivating(FGameFeatureActivatingContext& Context) override;
	virtual void OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context) override;
	virtual void OnGameFeatureUnregistering() override;
	//~End of UGameFeatureAction interface

	//~UObject interface
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
	//~End of UObject interface
	
private:
	/** A way for us to keep references to any delegate handles that are needed and track the pawns that have been modified */
	struct FPerContextData
	{
		TArray<TSharedPtr<FComponentRequestHandle>> ExtensionRequestHandles;
		TArray<TWeakObjectPtr<APawn>> PawnsAddedTo;
	};
	
	/** The "active data" that is used with this game feature's context changes. */
	TMap<FGameFeatureStateChangeContext, FPerContextData> ContextData;
	
	//~ Begin UGameFeatureAction_WorldActionBase interface
	virtual void AddToWorld(const FWorldContext& WorldContext, const FGameFeatureStateChangeContext& ChangeContext) override;
	//~ End UGameFeatureAction_WorldActionBase interface
	
	/** Reset the active data on this game feature, clearing references to any pawns and delegate handles. */
	void Reset(FPerContextData& ActiveData);

	/** Callback for the UGameFrameworkComponentManager when a pawn has been added */
	void HandlePawnExtension(AActor* Actor, FName EventName, FGameFeatureStateChangeContext ChangeContext);

	/** Add all the InputConfigs that are marked to activate automatically to the given pawn */
	void AddInputConfig(APawn* Pawn, FPerContextData& ActiveData);
	
	/** Remove all the InputConfigs from the given pawn and take them out of the given context data */
	void RemoveInputConfig(APawn* Pawn, FPerContextData& ActiveData);

	/** The player mappable configs to register for user with this config */
	UPROPERTY(EditAnywhere)
	TArray<FMappableConfigPair> InputConfigs;
};

PRAGMA_ENABLE_DEPRECATION_WARNINGS