// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "VCamInputTypes.h"

#include "VCamInputSubsystem.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogVCamInput, Log, All);

class FVCamInputProcessor;

// Currently only used for the placeholder Editor Input System
// This subsystem will be removed once the new input system is ready
UCLASS()
class VCAMINPUT_API UVCamInputSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()
public:

	UVCamInputSubsystem();
	~UVCamInputSubsystem();

	// By default the editor will use gamepads to control the editor camera
	// Setting this to true will prevent this
	UFUNCTION(BlueprintCallable, Category="Input")
	void SetShouldConsumeGamepadInput(const bool bInShouldConsumeGamepadInput);

	UFUNCTION(BlueprintPure, Category="Input")
    bool GetShouldConsumeGamepadInput() const;

	UFUNCTION(BlueprintCallable, Category="Input")
	void BindKeyDownEvent(const FKey Key, FKeyInputDelegate Delegate);

	UFUNCTION(BlueprintCallable, Category="Input")
    void BindKeyUpEvent(const FKey Key, FKeyInputDelegate Delegate);

	UFUNCTION(BlueprintCallable, Category="Input")
    void BindAnalogEvent(const FKey Key, FAnalogInputDelegate Delegate);

	UFUNCTION(BlueprintCallable, Category="Input")
    void BindMouseMoveEvent(FPointerInputDelegate Delegate);

	UFUNCTION(BlueprintCallable, Category="Input")
    void BindMouseButtonDownEvent(const FKey Key, FPointerInputDelegate Delegate);

	UFUNCTION(BlueprintCallable, Category="Input")
    void BindMouseButtonUpEvent(const FKey Key, FPointerInputDelegate Delegate);

	UFUNCTION(BlueprintCallable, Category="Input")
    void BindMouseDoubleClickEvent(const FKey Key, FPointerInputDelegate Delegate);

	UFUNCTION(BlueprintCallable, Category="Input")
    void BindMouseWheelEvent(FPointerInputDelegate Delegate);

private:
	// Whether the input processor was successfully registered
	bool bIsRegisterd = false;

	TSharedPtr<FVCamInputProcessor> InputProcessor;
};

