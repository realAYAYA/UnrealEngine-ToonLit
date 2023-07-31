/*
Copyright 2019 Valve Corporation under https://opensource.org/licenses/BSD-3-Clause
This code includes modifications by Epic Games.  Modifications (c) Epic Games, Inc.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
   may be used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

#include "Runtime/Engine/Classes/Engine/StaticMesh.h"
#include "Components/ActorComponent.h"
#include "Components/StaticMeshComponent.h"
#include "SteamVRTrackingRefComponent.generated.h"

// Delegates
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FComponentTrackingActivatedSignature, int32, DeviceID, FName, DeviceClass, FString, DeviceModel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FComponentTrackingDeactivatedSignature, int32, DeviceID, FName, DeviceClass, FString, DeviceModel);

UCLASS( ClassGroup=(Custom), deprecated, meta=(BlueprintSpawnableComponent, DeprecationMessage = "SteamVR plugin is deprecated; please use the OpenXR plugin.") )
class STEAMVRINPUTDEVICE_API UDEPRECATED_USteamVRTrackingReferences : public UActorComponent
{
	GENERATED_BODY()

public:	
	UDEPRECATED_USteamVRTrackingReferences();

	/** Blueprint event - When a new active device is recognized */
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(BlueprintAssignable, Category = "VR", meta = (DeprecatedProperty))
	FComponentTrackingActivatedSignature OnTrackedDeviceActivated;

	/** When an active device gets deactivated */
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(BlueprintAssignable, Category = "VR", meta = (DeprecatedProperty))
	FComponentTrackingDeactivatedSignature OnTrackedDeviceDeactivated;

	// TODO: Set default mesh to SteamVR provided render model. Must be backwards compatible to UE4.15

	/** Display Tracking References in-world */
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UFUNCTION(BlueprintCallable, Category = "SteamVR Input", meta = (DeprecatedFunction, DeprecationMessage = "The SteamVR plugin is deprecated."))
	bool ShowTrackingReferences(UStaticMesh* TrackingReferenceMesh);

	/** Remove Tracking References in-world */
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UFUNCTION(BlueprintCallable, Category = "SteamVR Input", meta = (DeprecatedFunction, DeprecationMessage = "The SteamVR plugin is deprecated."))
	void HideTrackingReferences();

	/** Scale to apply to the tracking reference mesh */
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SteamVR Input", meta = (DeprecatedProperty))
	float ActiveDevicePollFrequency = 1.f;

	/** Scale to apply to the tracking reference mesh */
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SteamVR Input", meta = (DeprecatedProperty))
	FVector TrackingReferenceScale = FVector(1.f);

	/** Currently displayed Tracking References in-world */
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(BlueprintReadOnly, Category = "SteamVR Input", meta = (DeprecatedProperty))
	TArray<TObjectPtr<UStaticMeshComponent>> TrackingReferences;

private:
	/** Represents a tracked device with a flag on whether its activated or not */
	struct FActiveTrackedDevice
	{
		unsigned int		id;		// The SteamVR id of this device
		bool		bActivated;		// Whether or not this device has been activated

		FActiveTrackedDevice(unsigned int inId, bool inActivated)
			: id(inId)
			, bActivated(inActivated)
		{}

	};

	/** Cache for current delta time */
	float CurrentDeltaTime = 0.f;

	/** Cache to hold tracked devices registered in SteamVR */
	TArray<FActiveTrackedDevice> ActiveTrackingDevices;

protected:
	virtual void BeginPlay() override;	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	FName GetDeviceClass(unsigned int id);
	bool FindTrackedDevice(unsigned int id);
};
