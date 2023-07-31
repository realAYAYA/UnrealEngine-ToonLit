// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputBehavior.h"
#include "InputState.h"
#include "Math/Ray.h"
#include "Math/Vector2D.h"
#include "Templates/Function.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "AnyButtonInputBehavior.generated.h"

class UObject;

/**
 * UAnyButtonInputBehavior is a base behavior that provides a generic
 * interface to a TargetButton on a physical Input Device. You can subclass
 * UAnyButtonInputBehavior to write InputBehaviors that can work independent
 * of a particular device type or button, by using the UAnyButtonInputBehavior functions below.
 * 
 * The target device button is selected using the .ButtonNumber property, or you can
 * override the relevant GetXButtonState() function if you need more control.
 * 
 *  @todo spatial controllers
 *  @todo support tablet fingers
 *  @todo support gamepad?
 */
UCLASS()
class INTERACTIVETOOLSFRAMEWORK_API UAnyButtonInputBehavior : public UInputBehavior
{
	GENERATED_BODY()

public:
	UAnyButtonInputBehavior();

	/** Return set of devices supported by this behavior */
	virtual EInputDevices GetSupportedDevices() override;

	/** @return true if Target Button has been pressed this frame */
	virtual bool IsPressed(const FInputDeviceState& input);

	/** @return true if Target Button is currently held down */
	virtual bool IsDown(const FInputDeviceState& input);

	/** @return true if Target Button was released this frame */
	virtual bool IsReleased(const FInputDeviceState& input);
	
	/** @return current 2D position of Target Device, or zero if device does not have 2D position */
	virtual FVector2D GetClickPoint(const FInputDeviceState& input);

	/** @return current 3D world ray for Target Device position */
	virtual FRay GetWorldRay(const FInputDeviceState& input);
	
	/** @return current 3D world ray and optional 2D position for Target Device */
	virtual FInputDeviceRay GetDeviceRay(const FInputDeviceState& input);

	/** @return last-active supported Device */
	EInputDevices GetActiveDevice() const;


public:
	/** Configure the target Mouse button to be the left button */
	virtual void SetUseLeftMouseButton();
	/** Configure the target Mouse button to be the middle button */
	virtual void SetUseMiddleMouseButton();
	/** Configure the target Mouse button to be the right button */
	virtual void SetUseRightMouseButton();
	/** Configure a custom target mouse button */
	virtual void SetUseCustomMouseButton(TUniqueFunction<FDeviceButtonState(const FInputDeviceState& Input)>);


protected:
	/** Which device is currently active */
	EInputDevices ActiveDevice;

	/** Returns FDeviceButtonState for target mouse button */
	TUniqueFunction<FDeviceButtonState(const FInputDeviceState& Input)> GetMouseButtonStateFunc;

	/** Returns FDeviceButtonState for target active device button */
	FDeviceButtonState GetButtonStateFunc(const FInputDeviceState& Input);
};