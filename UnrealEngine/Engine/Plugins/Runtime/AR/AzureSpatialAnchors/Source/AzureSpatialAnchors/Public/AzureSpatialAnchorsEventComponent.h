// Copyright Epic Games, Inc. All Rights Reserved.
// AzureSpatialAnchorsEventComponent.h: Component to handle receiving notifications from AzureSpatialAnchors

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/ActorComponent.h"
#include "IAzureSpatialAnchors.h"
#include "Delegates/Delegate.h"
#include "Delegates/DelegateCombinations.h"
#include "ARPin.h"
#include "AzureCloudSpatialAnchor.h"
#include "AzureSpatialAnchorsTypes.h"
#include "AzureSpatialAnchorsEventComponent.generated.h"

UCLASS(Blueprintable, meta = (BlueprintSpawnableComponent))
class AZURESPATIALANCHORS_API UAzureSpatialAnchorsEventComponent : public UActorComponent
{
	GENERATED_UCLASS_BODY()

public:

	/** Delegates that will be cast by the ASA platform implementations. */

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FASAAnchorLocatedDelegate, int32, WatcherIdentifier, EAzureSpatialAnchorsLocateAnchorStatus, Status, UAzureCloudSpatialAnchor*, CloudSpatialAnchor);
	UPROPERTY(BlueprintAssignable)
	FASAAnchorLocatedDelegate ASAAnchorLocatedDelegate;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FASALocateAnchorsCompletedDelegate, int32, WatcherIdentifier, bool, WasCanceled);
	UPROPERTY(BlueprintAssignable)
	FASALocateAnchorsCompletedDelegate ASALocateAnchorsCompleteDelegate;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FASASessionUpdatedDelegate, float, ReadyForCreateProgress, float, RecommendedForCreateProgress, int, SessionCreateHash, int, SessionLocateHash, EAzureSpatialAnchorsSessionUserFeedback, Feedback);
	UPROPERTY(BlueprintAssignable)
	FASASessionUpdatedDelegate ASASessionUpdatedDelegate;

	void OnRegister() override;
	void OnUnregister() override;

private:
	/** Native handlers that get registered with the actual FCoreDelegates, and then proceed to broadcast to the delegates above */
	void ASAAnchorLocatedDelegate_Handler(int32 WatcherIdentifier, EAzureSpatialAnchorsLocateAnchorStatus Status, UAzureCloudSpatialAnchor* CloudSpatialAnchor)	{ ASAAnchorLocatedDelegate.Broadcast(WatcherIdentifier, Status, CloudSpatialAnchor); }
	void ASALocateAnchorsCompleteDelegate_Handler(int32 WatcherIdentifier, bool WasCanceled) { ASALocateAnchorsCompleteDelegate.Broadcast(WatcherIdentifier, WasCanceled); }
	void ASASessionUpdatedDelegate_Handler(float ReadyForCreateProgress, float RecommendedForCreateProgress, int SessionCreateHash, int SessionLocateHash, EAzureSpatialAnchorsSessionUserFeedback Feedback) { ASASessionUpdatedDelegate.Broadcast(ReadyForCreateProgress, RecommendedForCreateProgress, SessionCreateHash, SessionLocateHash, Feedback); }
};



