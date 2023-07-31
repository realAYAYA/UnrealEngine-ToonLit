// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BaseBehaviors/BehaviorTargetInterfaces.h"
#include "BaseBehaviors/InputBehaviorModifierStates.h"
#include "CoreMinimal.h"
#include "InputBehavior.h"
#include "InputState.h"
#include "InteractiveTool.h"
#include "Templates/Function.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "MouseHoverBehavior.generated.h"

class UObject;



/**
 * Trivial InputBehavior that forwards InputBehavior hover events to a Target object via
 * the IHoverBehaviorTarget interface.
 */
UCLASS()
class INTERACTIVETOOLSFRAMEWORK_API UMouseHoverBehavior : public UInputBehavior
{
	GENERATED_BODY()

public:
	UMouseHoverBehavior();

	/**
	 * The modifier set for this behavior
	 */
	FInputBehaviorModifierStates Modifiers;

	virtual void Initialize(IHoverBehaviorTarget* Target);

	// UInputBehavior hover implementation

	virtual EInputDevices GetSupportedDevices() override;

	virtual bool WantsHoverEvents() override;
	virtual FInputCaptureRequest WantsHoverCapture(const FInputDeviceState& InputState) override;
	virtual FInputCaptureUpdate BeginHoverCapture(const FInputDeviceState& InputState, EInputCaptureSide eSide) override;
	virtual FInputCaptureUpdate UpdateHoverCapture(const FInputDeviceState& InputState) override;
	virtual void EndHoverCapture() override;

protected:
	IHoverBehaviorTarget* Target;
};


/**
 * An implementation of UMouseHoverBehavior that also implements IHoverBehaviorTarget directly, via a set 
 * of local lambda functions. To use/customize this class, the client replaces the lambda functions with their own.
 * This avoids having to create a separate IHoverBehaviorTarget implementation for trivial use-cases.
 */
UCLASS()
class INTERACTIVETOOLSFRAMEWORK_API ULocalMouseHoverBehavior : public UMouseHoverBehavior, public IHoverBehaviorTarget
{
	GENERATED_BODY()
protected:
	using UMouseHoverBehavior::Initialize;

public:
	/** Call this to initialize the class */
	virtual void Initialize()
	{
		this->Initialize(this);
	}

	/** lambda implementation of BeginHoverSequenceHitTest */
	TUniqueFunction<FInputRayHit(const FInputDeviceRay& PressPos)> BeginHitTestFunc = [](const FInputDeviceRay&) { return FInputRayHit(); };

	/** lambda implementation of OnBeginHover */
	TUniqueFunction<void(const FInputDeviceRay& PressPos)> OnBeginHoverFunc = [](const FInputDeviceRay&) {};

	/** lambda implementation of OnUpdateHover */
	TUniqueFunction<bool(const FInputDeviceRay& PressPos)> OnUpdateHoverFunc = [](const FInputDeviceRay&) { return false; };

	/** lambda implementation of OnEndHover */
	TUniqueFunction<void()> OnEndHoverFunc = []() {};

public:
	// IHoverBehaviorTarget implementation
	virtual FInputRayHit BeginHoverSequenceHitTest(const FInputDeviceRay& PressPos) override
	{
		return BeginHitTestFunc(PressPos);
	}

	virtual void OnBeginHover(const FInputDeviceRay& DevicePos) override
	{
		OnBeginHoverFunc(DevicePos);
	}

	virtual bool OnUpdateHover(const FInputDeviceRay& DevicePos) override
	{
		return OnUpdateHoverFunc(DevicePos);
	}

	virtual void OnEndHover() override
	{
		OnEndHoverFunc();
	}
};
