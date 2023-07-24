// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "IIdentifiableXRDevice.h" // for FXRDeviceId
#include "XRAssetFunctionLibrary.generated.h"

class AActor;
class UPrimitiveComponent;
class IXRSystemAssets;

/* UXRAssetFunctionLibrary
 *****************************************************************************/

UCLASS()
class UXRAssetFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

public:
	/**
	 * Spawns a render component for the specified XR device.
	 *
	 * NOTE: The associated XR system backend has to provide a model for this to
	 *       work - if one is not available for the specific device, then this
	 *       will fail and return an invalid (null) object.
	 *
	 * @param  Target				The intended owner for the component to attach to.
	 * @param  XRDeviceId			Specifies the device you're wanting a model for.
	 * @param  bManualAttachment	If set, will leave the component unattached (mirror's the same option on the generic AddComponent node). When unset the component will attach to the actor's root.
	 * @param  RelativeTransform	Specifies the component initial transform (relative to its attach parent).
	 *
	 * @return A new component representing the specified device (invalid/null if a model for the device doesn't exist).
	 */
	UFUNCTION(BlueprintCallable, Category = "XR|Devices", meta = (DefaultToSelf = "Target"))
	static UPrimitiveComponent* AddDeviceVisualizationComponentBlocking(AActor* Target, const FXRDeviceId& XRDeviceId, bool bManualAttachment, const FTransform& RelativeTransform);

	/**
	 * Spawns a render component for the specified XR device.
	 *
	 * NOTE: The associated XR system backend has to provide a model for this to
	 *       work - if one is not available for the specific device, then this
	 *       will fail and return an invalid (null) object.
	 *
	 * @param  Target				The intended owner for the component to attach to.
	 * @param  SystemName			(optional) Targets a specific XR system (i.e. 'Oculus', 'SteamVR', etc.). If left as 'None', then the first system found that can render the device will be used.
	 * @param  DeviceName			Source name of the specific device - expect the same names that the MotionControllerComponent's "MotionSource" field uses ('Left', 'Right', etc.).
	 * @param  bManualAttachment	If set, will leave the component unattached (mirror's the same option on the generic AddComponent node). When unset the component will attach to the actor's root.
	 * @param  RelativeTransform	Specifies the component initial transform (relative to its attach parent).
	 *
	 * @return A new component representing the specified device (invalid/null if a model for the device doesn't exist).
	 */
	UFUNCTION(BlueprintCallable, Category = "XR|Devices", meta = (DefaultToSelf = "Target"))
	static UPrimitiveComponent* AddNamedDeviceVisualizationComponentBlocking(AActor* Target, const FName SystemName, const FName DeviceName, bool bManualAttachment, const FTransform& RelativeTransform, FXRDeviceId& XRDeviceId);
};

/* UAsyncTask_LoadXRDeviceVisComponent
 *****************************************************************************/

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDeviceModelLoadedDelegate, const UPrimitiveComponent*, LoadedComponent);

UCLASS()
class UAsyncTask_LoadXRDeviceVisComponent : public UBlueprintAsyncActionBase
{
	GENERATED_UCLASS_BODY()

public:

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", DefaultToSelf = "Target"), Category = "XR|Devices")
	static UAsyncTask_LoadXRDeviceVisComponent* AddNamedDeviceVisualizationComponentAsync(AActor* Target, const FName SystemName, const FName DeviceName, bool bManualAttachment, const FTransform& RelativeTransform, FXRDeviceId& XRDeviceId, UPrimitiveComponent*& NewComponent);

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", DefaultToSelf = "Target"), Category = "XR|Devices")
	static UAsyncTask_LoadXRDeviceVisComponent* AddDeviceVisualizationComponentAsync(AActor* Target, const FXRDeviceId& XRDeviceId, bool bManualAttachment, const FTransform& RelativeTransform, UPrimitiveComponent*& NewComponent);

	UPROPERTY(BlueprintAssignable)
	FDeviceModelLoadedDelegate OnModelLoaded;

	UPROPERTY(BlueprintAssignable)
	FDeviceModelLoadedDelegate OnLoadFailure;

public:
	//~ UBlueprintAsyncActionBase interface
	virtual void Activate() override;

private:
	void OnLoadComplete(bool bSuccess);

	enum class ELoadStatus : uint8
	{
		Pending,
		LoadSuccess,
		LoadFailure
	};
	ELoadStatus LoadStatus = ELoadStatus::Pending;

	UPROPERTY()
	TObjectPtr<UPrimitiveComponent> SpawnedComponent;
};

