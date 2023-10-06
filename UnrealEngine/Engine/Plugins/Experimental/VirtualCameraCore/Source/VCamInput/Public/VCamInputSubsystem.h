// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "VCamInputTypes.h"
#include "VCamInputSubsystem.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogVCamInput, Log, All);

class FVCamInputProcessor;

UCLASS(Deprecated)
class UE_DEPRECATED(5.2, "This class has been deprecated and its functionality superseded by UInputVCamSubsystem.") VCAMINPUT_API UDEPRECATED_VCamInputSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()
public:

	UDEPRECATED_VCamInputSubsystem();
	~UDEPRECATED_VCamInputSubsystem();

	// By default the editor will use gamepads to control the editor camera
	// Setting this to true will prevent this
	UE_DEPRECATED(5.2, "Use UInputVCamSubsystem::SetShouldConsumeGamepadInput instead.")
	UFUNCTION(BlueprintCallable, Category="Input", meta = (DeprecatedFunction, DeprecationMessage = "Use UInputVCamSubsystem::SetShouldConsumeGamepadInput instead."))
	void SetShouldConsumeGamepadInput(const bool bInShouldConsumeGamepadInput);

	UE_DEPRECATED(5.2, "This function is deprecated.")
	UFUNCTION(BlueprintPure, Category="Input", meta = (DeprecatedFunction, DeprecationMessage = "This function is deprecated."))
    bool GetShouldConsumeGamepadInput() const;

	UE_DEPRECATED(5.2, "This function is deprecated.")
	UFUNCTION(BlueprintCallable, Category="Input", meta = (DeprecatedFunction, DeprecationMessage = "This function is deprecated."))
	void BindKeyDownEvent(const FKey Key, FKeyInputDelegate Delegate);

	UE_DEPRECATED(5.2, "This function is deprecated.")
	UFUNCTION(BlueprintCallable, Category="Input", meta = (DeprecatedFunction, DeprecationMessage = "This function is deprecated."))
    void BindKeyUpEvent(const FKey Key, FKeyInputDelegate Delegate);

	UE_DEPRECATED(5.2, "This function is deprecated.")
	UFUNCTION(BlueprintCallable, Category="Input", meta = (DeprecatedFunction, DeprecationMessage = "This function is deprecated."))
    void BindAnalogEvent(const FKey Key, FAnalogInputDelegate Delegate);

	UE_DEPRECATED(5.2, "This function is deprecated.")
	UFUNCTION(BlueprintCallable, Category="Input", meta = (DeprecatedFunction, DeprecationMessage = "This function is deprecated."))
    void BindMouseMoveEvent(FPointerInputDelegate Delegate);

	UE_DEPRECATED(5.2, "This function is deprecated.")
	UFUNCTION(BlueprintCallable, Category="Input", meta = (DeprecatedFunction, DeprecationMessage = "This function is deprecated."))
    void BindMouseButtonDownEvent(const FKey Key, FPointerInputDelegate Delegate);

	UE_DEPRECATED(5.2, "This function is deprecated.")
	UFUNCTION(BlueprintCallable, Category="Input", meta = (DeprecatedFunction, DeprecationMessage = "This function is deprecated."))
    void BindMouseButtonUpEvent(const FKey Key, FPointerInputDelegate Delegate);

	UE_DEPRECATED(5.2, "This function is deprecated.")
	UFUNCTION(BlueprintCallable, Category="Input", meta = (DeprecatedFunction, DeprecationMessage = "This function is deprecated."))
    void BindMouseDoubleClickEvent(const FKey Key, FPointerInputDelegate Delegate);

	UE_DEPRECATED(5.2, "This function is deprecated.")
	UFUNCTION(BlueprintCallable, Category="Input", meta = (DeprecatedFunction, DeprecationMessage = "This function is deprecated."))
    void BindMouseWheelEvent(FPointerInputDelegate Delegate);
};

