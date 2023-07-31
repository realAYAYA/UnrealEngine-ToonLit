// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BaseBehaviors/AnyButtonInputBehavior.h"
#include "CoreMinimal.h"
#include "InputBehavior.h"
#include "InputBehaviorModifierStates.h"
#include "Templates/Function.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "MouseWheelBehavior.generated.h"

class IMouseWheelBehaviorTarget;
class UObject;
struct FInputDeviceState;

UCLASS()
class INTERACTIVETOOLSFRAMEWORK_API UMouseWheelInputBehavior : public UAnyButtonInputBehavior
{
	GENERATED_BODY()

public:
	UMouseWheelInputBehavior();

	/**
	 * Initialize this behavior with the given Target
	 * @param Target implementor of hit-test and on-clicked functions
	 */
	virtual void Initialize(IMouseWheelBehaviorTarget* Target);


	/**
	 * WantsCapture() will only return capture request if this function returns true (or is null)
	 */
	TFunction<bool(const FInputDeviceState&)> ModifierCheckFunc = nullptr;


	// UInputBehavior implementation
	virtual FInputCaptureRequest WantsCapture(const FInputDeviceState& Input) override;
	virtual FInputCaptureUpdate BeginCapture(const FInputDeviceState& Input, EInputCaptureSide eSide) override;
	virtual FInputCaptureUpdate UpdateCapture(const FInputDeviceState& Input, const FInputCaptureData& Data) override;
	virtual void ForceEndCapture(const FInputCaptureData& Data) override;

	/**
	 * The modifier set for this behavior
	 */
	FInputBehaviorModifierStates Modifiers;

protected:
	/**
	 *
	 */
	IMouseWheelBehaviorTarget* Target;

};