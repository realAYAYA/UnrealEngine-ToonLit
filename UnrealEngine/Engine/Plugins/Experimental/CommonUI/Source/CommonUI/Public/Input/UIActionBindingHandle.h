// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"

class UWidget;
enum class ECommonInputMode : uint8;

struct COMMONUI_API FUIActionBindingHandle
{
public:
	bool IsValid() const;
	void Unregister();

	FName GetActionName() const;

	FText GetDisplayName() const;

	/** Should not be called often as broadcasts UCommonUIActionRouterBase::OnBoundActionsUpdated event */
	void SetDisplayName(const FText& DisplayName);

	const UWidget* GetBoundWidget() const;

	FUIActionBindingHandle() {}
	bool operator==(const FUIActionBindingHandle& Other) const { return RegistrationId == Other.RegistrationId; }
	bool operator!=(const FUIActionBindingHandle& Other) const { return !operator==(Other); }

	friend uint32 GetTypeHash(const FUIActionBindingHandle& Handle)
	{
		return ::GetTypeHash(Handle.RegistrationId);
	}

private:
	friend struct FUIActionBinding;
	
#if !UE_BUILD_SHIPPING
	// Using FString since the FName visualizer gets confused after live coding atm
	FString CachedDebugActionName;
#endif

	FUIActionBindingHandle(int32 InRegistrationId)
		: RegistrationId(InRegistrationId)
	{}

	int32 RegistrationId = INDEX_NONE;
};

struct FUICameraConfig
{
	COMMONUI_API FUICameraConfig(){}
	FUICameraConfig(uint8 InCamera) : DesiredCamera(InCamera) {}

	TOptional<uint8> GetDesiredCamera() const { return DesiredCamera; }

private:
	TOptional<uint8> DesiredCamera;
};

struct FUIInputConfig
{
	ECommonInputMode GetInputMode() const { return InputMode; }
	EMouseCaptureMode GetMouseCaptureMode() const { return MouseCaptureMode; }
	bool HideCursorDuringViewportCapture() const { return bHideCursorDuringViewportCapture; }

	COMMONUI_API FUIInputConfig();
	FUIInputConfig(ECommonInputMode InInputMode, EMouseCaptureMode InMouseCaptureMode, bool bInHideCursorDuringViewportCapture = true)
		: InputMode(InInputMode)
		, MouseCaptureMode(InMouseCaptureMode)
		, bHideCursorDuringViewportCapture(bInHideCursorDuringViewportCapture)
	{}

	bool operator==(const FUIInputConfig& Other) const
	{
		return InputMode == Other.InputMode
			&& MouseCaptureMode == Other.MouseCaptureMode
			&& bHideCursorDuringViewportCapture == Other.bHideCursorDuringViewportCapture;
	}

	bool operator!=(const FUIInputConfig& Other) const
	{
		return !operator==(Other);
	}

	bool bIgnoreMoveInput = false;
	bool bIgnoreLookInput = false;

private:
	ECommonInputMode InputMode;
	EMouseCaptureMode MouseCaptureMode;
	bool bHideCursorDuringViewportCapture = true;
};