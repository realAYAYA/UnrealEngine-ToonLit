// Copyright Epic Games, Inc. All Rights Reserved.

#include "RewindDebuggerSettings.h"
#include "Misc/CoreDelegates.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RewindDebuggerSettings)

URewindDebuggerSettings::URewindDebuggerSettings() : CameraMode(ERewindDebuggerCameraMode::Replay), /*bShouldAutoDetach(false),*/ bShouldAutoRecordOnPIE(false)
{
	FCoreDelegates::OnPreExit.AddLambda([]()
	{
		Get().SaveConfig();
	});
}

#if WITH_EDITOR

FText URewindDebuggerSettings::GetSectionText() const
{
	return NSLOCTEXT("RewindDebugger", "RewindDebuggerSettingsName", "Rewind Debugger");
}

FText URewindDebuggerSettings::GetSectionDescription() const
{
	return NSLOCTEXT("RewindDebugger", "RewindDebuggerSettingsDescription", "Configure options for the Rewind Debugger.");
}

#endif

FName URewindDebuggerSettings::GetCategoryName() const
{
	return FName(TEXT("Plugins"));
}

URewindDebuggerSettings& URewindDebuggerSettings::Get()
{
	URewindDebuggerSettings* MutableCDO = GetMutableDefault<URewindDebuggerSettings>();
	check(MutableCDO != nullptr)
	
	return *MutableCDO;
}

