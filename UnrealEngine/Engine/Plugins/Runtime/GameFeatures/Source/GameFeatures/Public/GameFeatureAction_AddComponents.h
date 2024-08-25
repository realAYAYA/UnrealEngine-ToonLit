// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFeatureAction.h"
#include "GameFeaturesSubsystem.h"

#include "GameFeatureAction_AddComponents.generated.h"

class AActor;
class UActorComponent;
class UGameFrameworkComponentManager;
class UGameInstance;
struct FComponentRequestHandle;
struct FWorldContext;

enum class EGameFrameworkAddComponentFlags : uint8;

// Description of a component to add to a type of actor when this game feature is enabled
// (the actor class must be game feature aware, it does not happen magically)
//@TODO: Write more documentation here about how to make an actor game feature / modular gameplay aware
USTRUCT()
struct GAMEFEATURES_API FGameFeatureComponentEntry
{
	GENERATED_BODY()

	FGameFeatureComponentEntry();

	// The base actor class to add a component to
	UPROPERTY(EditAnywhere, Category="Components", meta=(AllowAbstract="True"))
	TSoftClassPtr<AActor> ActorClass;

	// The component class to add to the specified type of actor
	UPROPERTY(EditAnywhere, Category="Components")
	TSoftClassPtr<UActorComponent> ComponentClass;
	
	// Should this component be added for clients
	UPROPERTY(EditAnywhere, Category="Components")
	uint8 bClientComponent:1;

	// Should this component be added on servers
	UPROPERTY(EditAnywhere, Category="Components")
	uint8 bServerComponent:1;

	// Observe these rules when adding the component, if any
	UPROPERTY(EditAnywhere, Category = "Components", meta = (Bitmask, BitmaskEnum = "/Script/ModularGameplay.EGameFrameworkAddComponentFlags"))
	uint8 AdditionFlags;
};	

//////////////////////////////////////////////////////////////////////
// UGameFeatureAction_AddComponents

/**
 * Adds actor<->component spawn requests to the component manager
 *
 * @see UGameFrameworkComponentManager
 */
UCLASS(MinimalAPI, meta = (DisplayName = "Add Components"))
class UGameFeatureAction_AddComponents final : public UGameFeatureAction
{
	GENERATED_BODY()

public:
	//~UGameFeatureAction interface
	virtual void OnGameFeatureActivating(FGameFeatureActivatingContext& Context) override;
	virtual void OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context) override;
#if WITH_EDITORONLY_DATA
	virtual void AddAdditionalAssetBundleData(FAssetBundleData& AssetBundleData) override;
#endif
	//~End of UGameFeatureAction interface

	//~UObject interface
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
	//~End of UObject interface

	/** List of components to add to gameplay actors when this game feature is enabled */
	UPROPERTY(EditAnywhere, Category="Components", meta=(TitleProperty="{ActorClass} -> {ComponentClass}"))
	TArray<FGameFeatureComponentEntry> ComponentList;

private:
	struct FContextHandles
	{
		FDelegateHandle GameInstanceStartHandle;
		TArray<TSharedPtr<FComponentRequestHandle>> ComponentRequestHandles;
	};

	void AddToWorld(const FWorldContext& WorldContext, FContextHandles& Handles);

	void HandleGameInstanceStart(UGameInstance* GameInstance, FGameFeatureStateChangeContext ChangeContext);

	TMap<FGameFeatureStateChangeContext, FContextHandles> ContextHandles;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
