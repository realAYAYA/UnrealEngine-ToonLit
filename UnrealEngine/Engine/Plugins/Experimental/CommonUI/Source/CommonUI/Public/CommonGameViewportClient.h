// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "InputCoreTypes.h"
#include "Engine/GameViewportClient.h"
#include "CommonGameViewportClient.generated.h"

DECLARE_DELEGATE_FourParams(FOnRerouteInputDelegate, FInputDeviceId /* InputDeviceId */, FKey /* Key */, EInputEvent /* EventType */, FReply& /* Reply */);
DECLARE_DELEGATE_FourParams(FOnRerouteAxisDelegate, FInputDeviceId /* InputDeviceId */, FKey /* Key */, float /* Delta */, FReply& /* Reply */);
DECLARE_DELEGATE_FiveParams(FOnRerouteTouchDelegate, int32 /* ControllerId */, uint32 /* TouchId */, ETouchType::Type /* TouchType */, const FVector2D& /* TouchLocation */, FReply& /* Reply */);

/**
* CommonUI Viewport to reroute input to UI first. Needed to allow CommonUI to route / handle inputs.
*/
UCLASS(Within = Engine, transient, config = Engine)
class COMMONUI_API UCommonGameViewportClient : public UGameViewportClient
{
	GENERATED_BODY()

public:

	UCommonGameViewportClient(FVTableHelper& Helper);
	virtual ~UCommonGameViewportClient();

	// UGameViewportClient interface begin
	virtual bool InputKey(const FInputKeyEventArgs& EventArgs) override;
	virtual bool InputAxis(FViewport* InViewport, FInputDeviceId InputDevice, FKey Key, float Delta, float DeltaTime, int32 NumSamples, bool bGamepad) override;
	virtual bool InputTouch(FViewport* InViewport, int32 ControllerId, uint32 Handle, ETouchType::Type Type, const FVector2D& TouchLocation, float Force, FDateTime DeviceTimestamp, uint32 TouchpadIndex) override;
	// UGameViewportClient interface end

	FOnRerouteInputDelegate& OnRerouteInput() { return RerouteInput; }
	FOnRerouteAxisDelegate& OnRerouteAxis() { return RerouteAxis; }
	FOnRerouteTouchDelegate& OnRerouteTouch() { return RerouteTouch; }

	FOnRerouteInputDelegate& OnRerouteBlockedInput() { return RerouteBlockedInput; }

	/** Default Handler for Key input. */
	UE_DEPRECATED(5.1, "This version of HandleRerouteInput has been deprecated. Please use the version that takes an FInputDeviceId instead")
	virtual void HandleRerouteInput(int32 ControllerId, FKey Key, EInputEvent EventType, FReply& Reply);

	/** Default Handler for Key input. */
	virtual void HandleRerouteInput(FInputDeviceId DeviceId, FKey Key, EInputEvent EventType, FReply& Reply);

	/** Default Handler for Axis input. */
	UE_DEPRECATED(5.1, "This version of HandleRerouteAxis has been deprecated. Please use the version that takes an FInputDeviceId instead")
	virtual void HandleRerouteAxis(int32 ControllerId, FKey Key, float Delta, FReply& Reply);

	/** Default Handler for Axis input. */
	virtual void HandleRerouteAxis(FInputDeviceId DeviceId, FKey Key, float Delta, FReply& Reply);

	/** Default Handler for Touch input. */
	virtual void HandleRerouteTouch(int32 ControllerId, uint32 TouchId, ETouchType::Type TouchType, const FVector2D& TouchLocation, FReply& Reply);

protected:
	
	/** Console window & fullscreen shortcut have higher priority than UI */
	virtual bool IsKeyPriorityAboveUI(const FInputKeyEventArgs& EventArgs);

	FOnRerouteInputDelegate RerouteInput;
	FOnRerouteAxisDelegate RerouteAxis;
	FOnRerouteTouchDelegate RerouteTouch;

	FOnRerouteInputDelegate RerouteBlockedInput;
};
