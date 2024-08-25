// Copyright Epic Games, Inc. All Rights Reserved.

#include "GenericPlatform/GenericApplication.h"
#include "GenericPlatform/Accessibility/GenericAccessibleInterfaces.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CoreDelegates.h"
#include "Misc/CommandLine.h"

const FGamepadKeyNames::Type FGamepadKeyNames::Invalid(NAME_None);

// Ensure that the GamepadKeyNames match those in InputCoreTypes.cpp
const FGamepadKeyNames::Type FGamepadKeyNames::LeftAnalogX("Gamepad_LeftX");
const FGamepadKeyNames::Type FGamepadKeyNames::LeftAnalogY("Gamepad_LeftY");
const FGamepadKeyNames::Type FGamepadKeyNames::RightAnalogX("Gamepad_RightX");
const FGamepadKeyNames::Type FGamepadKeyNames::RightAnalogY("Gamepad_RightY");
const FGamepadKeyNames::Type FGamepadKeyNames::LeftTriggerAnalog("Gamepad_LeftTriggerAxis");
const FGamepadKeyNames::Type FGamepadKeyNames::RightTriggerAnalog("Gamepad_RightTriggerAxis");

const FGamepadKeyNames::Type FGamepadKeyNames::LeftThumb("Gamepad_LeftThumbstick");
const FGamepadKeyNames::Type FGamepadKeyNames::RightThumb("Gamepad_RightThumbstick");
const FGamepadKeyNames::Type FGamepadKeyNames::SpecialLeft("Gamepad_Special_Left");
const FGamepadKeyNames::Type FGamepadKeyNames::SpecialLeft_X("Gamepad_Special_Left_X");
const FGamepadKeyNames::Type FGamepadKeyNames::SpecialLeft_Y("Gamepad_Special_Left_Y");
const FGamepadKeyNames::Type FGamepadKeyNames::SpecialRight("Gamepad_Special_Right");
const FGamepadKeyNames::Type FGamepadKeyNames::FaceButtonBottom("Gamepad_FaceButton_Bottom");
const FGamepadKeyNames::Type FGamepadKeyNames::FaceButtonRight("Gamepad_FaceButton_Right");
const FGamepadKeyNames::Type FGamepadKeyNames::FaceButtonLeft("Gamepad_FaceButton_Left");
const FGamepadKeyNames::Type FGamepadKeyNames::FaceButtonTop("Gamepad_FaceButton_Top");
const FGamepadKeyNames::Type FGamepadKeyNames::LeftShoulder("Gamepad_LeftShoulder");
const FGamepadKeyNames::Type FGamepadKeyNames::RightShoulder("Gamepad_RightShoulder");
const FGamepadKeyNames::Type FGamepadKeyNames::LeftTriggerThreshold("Gamepad_LeftTrigger");
const FGamepadKeyNames::Type FGamepadKeyNames::RightTriggerThreshold("Gamepad_RightTrigger");
const FGamepadKeyNames::Type FGamepadKeyNames::DPadUp("Gamepad_DPad_Up");
const FGamepadKeyNames::Type FGamepadKeyNames::DPadDown("Gamepad_DPad_Down");
const FGamepadKeyNames::Type FGamepadKeyNames::DPadRight("Gamepad_DPad_Right");
const FGamepadKeyNames::Type FGamepadKeyNames::DPadLeft("Gamepad_DPad_Left");

const FGamepadKeyNames::Type FGamepadKeyNames::LeftStickUp("Gamepad_LeftStick_Up");
const FGamepadKeyNames::Type FGamepadKeyNames::LeftStickDown("Gamepad_LeftStick_Down");
const FGamepadKeyNames::Type FGamepadKeyNames::LeftStickRight("Gamepad_LeftStick_Right");
const FGamepadKeyNames::Type FGamepadKeyNames::LeftStickLeft("Gamepad_LeftStick_Left");

const FGamepadKeyNames::Type FGamepadKeyNames::RightStickUp("Gamepad_RightStick_Up");
const FGamepadKeyNames::Type FGamepadKeyNames::RightStickDown("Gamepad_RightStick_Down");
const FGamepadKeyNames::Type FGamepadKeyNames::RightStickRight("Gamepad_RightStick_Right");
const FGamepadKeyNames::Type FGamepadKeyNames::RightStickLeft("Gamepad_RightStick_Left");

namespace UE::InputDeviceScope::Private
{
TArray<FInputDeviceScope*>& GetScopeStack()
{
	static thread_local TArray<FInputDeviceScope*> ScopeStack;
	return ScopeStack;
}
}

FInputDeviceScope::FInputDeviceScope(IInputDevice* InInputDevice, FName InInputDeviceName, int32 InHardwareDeviceHandle, FString InHardwareDeviceIdentifier)
	: InputDevice(InInputDevice)
	, InputDeviceName(InInputDeviceName)
	, HardwareDeviceHandle(InHardwareDeviceHandle)
	, HardwareDeviceIdentifier(InHardwareDeviceIdentifier)
{
	// Add to scope stack
	UE::InputDeviceScope::Private::GetScopeStack().Add(this);
}

FInputDeviceScope::~FInputDeviceScope()
{
	TArray<FInputDeviceScope*>& ScopeStack = UE::InputDeviceScope::Private::GetScopeStack();

	// This should always be the top of the stack
	ensureMsgf((ScopeStack.Num() > 0 && ScopeStack.Last() == this), TEXT("FInputDeviceScope was not destroyed in correct order!"));
	ScopeStack.Remove(this);
}

const FInputDeviceScope* FInputDeviceScope::GetCurrent()
{
	TArray<FInputDeviceScope*>& ScopeStack = UE::InputDeviceScope::Private::GetScopeStack();
	if (ScopeStack.Num() > 0)
	{
		return ScopeStack.Last();
	}
	return nullptr;
}

float GDebugSafeZoneRatio = 1.0f;
float GDebugActionZoneRatio = 1.0f;

struct FSafeZoneConsoleVariables
{
	FAutoConsoleVariableRef DebugSafeZoneRatioCVar;
	FAutoConsoleVariableRef DebugActionZoneRatioCVar;

	FSafeZoneConsoleVariables()
		: DebugSafeZoneRatioCVar(
			TEXT("r.DebugSafeZone.TitleRatio"),
			GDebugSafeZoneRatio,
			TEXT("The safe zone ratio that will be returned by FDisplayMetrics::GetDisplayMetrics on platforms that don't have a defined safe zone (0..1)\n")
			TEXT(" default: 1.0"))
		, DebugActionZoneRatioCVar(
			TEXT("r.DebugActionZone.ActionRatio"),
			GDebugActionZoneRatio,
			TEXT("The action zone ratio that will be returned by FDisplayMetrics::GetDisplayMetrics on platforms that don't have a defined safe zone (0..1)\n")
			TEXT(" default: 1.0"))
	{
		DebugSafeZoneRatioCVar->SetOnChangedCallback(FConsoleVariableDelegate::CreateRaw(this, &FSafeZoneConsoleVariables::OnDebugSafeZoneChanged));
		DebugActionZoneRatioCVar->SetOnChangedCallback(FConsoleVariableDelegate::CreateRaw(this, &FSafeZoneConsoleVariables::OnDebugSafeZoneChanged));
	}

	void OnDebugSafeZoneChanged(IConsoleVariable* Var)
	{
		FCoreDelegates::OnSafeFrameChangedEvent.Broadcast();
	}
};

FSafeZoneConsoleVariables GSafeZoneConsoleVariables;

FPlatformRect FDisplayMetrics::GetMonitorWorkAreaFromPoint(const FVector2D& Point) const
{
	for (const FMonitorInfo& Info : MonitorInfo)
	{
		// The point may not actually be inside the work area (for example on Windows taskbar or Mac menu bar), so we use DisplayRect to find the monitor
		if (Point.X >= Info.DisplayRect.Left && Point.X < Info.DisplayRect.Right && Point.Y >= Info.DisplayRect.Top && Point.Y < Info.DisplayRect.Bottom)
		{
			return Info.WorkArea;
		}
	}

	return FPlatformRect(0, 0, 0, 0);
}

float FDisplayMetrics::GetDebugTitleSafeZoneRatio()
{
	return GDebugSafeZoneRatio;
}

float FDisplayMetrics::GetDebugActionSafeZoneRatio()
{
	return GDebugActionZoneRatio;
}

void FDisplayMetrics::ApplyDefaultSafeZones()
{
	// Allow safe zones to be overridden by the command line. Used by mobile PIE.
	TitleSafePaddingSize = FVector4(0.0f, 0.0f, 0.0f, 0.0f);
	bool bSetByCommandLine;
	bSetByCommandLine = FParse::Value(FCommandLine::Get(), TEXT("SafeZonePaddingLeft="),   TitleSafePaddingSize.X);
	bSetByCommandLine = FParse::Value(FCommandLine::Get(), TEXT("SafeZonePaddingTop="),  TitleSafePaddingSize.Y) || bSetByCommandLine;
	bSetByCommandLine = FParse::Value(FCommandLine::Get(), TEXT("SafeZonePaddingRight="),    TitleSafePaddingSize.Z) || bSetByCommandLine;
	bSetByCommandLine = FParse::Value(FCommandLine::Get(), TEXT("SafeZonePaddingBottom="), TitleSafePaddingSize.W) || bSetByCommandLine;

	if (!bSetByCommandLine)
	{
		const float SafeZoneRatio = GetDebugTitleSafeZoneRatio();
		if (SafeZoneRatio < 1.0f)
		{
			const float HalfUnsafeRatio = (1.0f - SafeZoneRatio) * 0.5f;
			TitleSafePaddingSize = FVector4((float)PrimaryDisplayWidth * HalfUnsafeRatio, (float)PrimaryDisplayHeight * HalfUnsafeRatio, (float)PrimaryDisplayWidth * HalfUnsafeRatio, (float)PrimaryDisplayHeight * HalfUnsafeRatio);
		}
	}

	const float ActionSafeZoneRatio = GetDebugActionSafeZoneRatio();
	if (ActionSafeZoneRatio < 1.0f)
	{
		const float HalfUnsafeRatio = (1.0f - ActionSafeZoneRatio) * 0.5f;
		ActionSafePaddingSize = FVector4((float)PrimaryDisplayWidth * HalfUnsafeRatio, (float)PrimaryDisplayHeight * HalfUnsafeRatio, (float)PrimaryDisplayWidth * HalfUnsafeRatio, (float)PrimaryDisplayHeight * HalfUnsafeRatio);
	}
}

void FDisplayMetrics::PrintToLog() const
{
	UE_LOG(LogInit, Log, TEXT("Display metrics:"));
	UE_LOG(LogInit, Log, TEXT("  PrimaryDisplayWidth: %d"), PrimaryDisplayWidth);
	UE_LOG(LogInit, Log, TEXT("  PrimaryDisplayHeight: %d"), PrimaryDisplayHeight);
	UE_LOG(LogInit, Log, TEXT("  PrimaryDisplayWorkAreaRect:"));
	UE_LOG(LogInit, Log, TEXT("    Left=%d, Top=%d, Right=%d, Bottom=%d"), 
		PrimaryDisplayWorkAreaRect.Left, PrimaryDisplayWorkAreaRect.Top, 
		PrimaryDisplayWorkAreaRect.Right, PrimaryDisplayWorkAreaRect.Bottom);

	UE_LOG(LogInit, Log, TEXT("  VirtualDisplayRect:"));
	UE_LOG(LogInit, Log, TEXT("    Left=%d, Top=%d, Right=%d, Bottom=%d"), 
		VirtualDisplayRect.Left, VirtualDisplayRect.Top, 
		VirtualDisplayRect.Right, VirtualDisplayRect.Bottom);

	UE_LOG(LogInit, Log, TEXT("  TitleSafePaddingSize: %s"), *TitleSafePaddingSize.ToString());
	UE_LOG(LogInit, Log, TEXT("  ActionSafePaddingSize: %s"), *ActionSafePaddingSize.ToString());

	const int NumMonitors = MonitorInfo.Num();
	UE_LOG(LogInit, Log, TEXT("  Number of monitors: %d"), NumMonitors);

	for (int MonitorIdx = 0; MonitorIdx < NumMonitors; ++MonitorIdx)
	{
		const FMonitorInfo & Info = MonitorInfo[MonitorIdx];
		UE_LOG(LogInit, Log, TEXT("    Monitor %d"), MonitorIdx);
		UE_LOG(LogInit, Log, TEXT("      Name: %s"), *Info.Name);
		UE_LOG(LogInit, Log, TEXT("      ID: %s"), *Info.ID);
		UE_LOG(LogInit, Log, TEXT("      NativeWidth: %d"), Info.NativeWidth);
		UE_LOG(LogInit, Log, TEXT("      NativeHeight: %d"), Info.NativeHeight);
		UE_LOG(LogInit, Log, TEXT("      bIsPrimary: %s"), Info.bIsPrimary ? TEXT("true") : TEXT("false"));
	}
}

GenericApplication::GenericApplication(const TSharedPtr< ICursor >& InCursor)
	: Cursor(InCursor)
	, MessageHandler(MakeShareable(new FGenericApplicationMessageHandler()))
#if WITH_ACCESSIBILITY
	, AccessibleMessageHandler(MakeShareable(new FGenericAccessibleMessageHandler()))
#endif
{

}
GenericApplication::~GenericApplication() = default;

#if WITH_ACCESSIBILITY
void GenericApplication::SetAccessibleMessageHandler(const TSharedRef<FGenericAccessibleMessageHandler>& InAccessibleMessageHandler)
{
	AccessibleMessageHandler = InAccessibleMessageHandler;
}

TSharedRef<FGenericAccessibleMessageHandler> GenericApplication::GetAccessibleMessageHandler() const
{
	return AccessibleMessageHandler;
}
#endif
