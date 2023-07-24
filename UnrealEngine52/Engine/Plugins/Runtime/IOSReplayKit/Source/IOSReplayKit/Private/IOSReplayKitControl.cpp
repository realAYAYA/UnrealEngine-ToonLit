// Copyright Epic Games, Inc. All Rights Reserved.

#include "IOSReplayKitControl.h"
#include "IOSReplayKit.h"

void UIOSReplayKitControl::StartRecording(bool bMicrophoneEnabled)
{
    IIOSReplayKitModuleInterface::Get().Initialize(bMicrophoneEnabled, false);
    IIOSReplayKitModuleInterface::Get().StartRecording();
	
    UE_LOG(LogIOSReplayKit, Log, TEXT("Starting recording!"));
}

void UIOSReplayKitControl::StopRecording()
{
    IIOSReplayKitModuleInterface::Get().StopRecording();
    UE_LOG(LogIOSReplayKit, Log, TEXT("Stopping recording!"));
}

void UIOSReplayKitControl::StartCaptureToFile(bool bMicrophoneEnabled)
{
    IIOSReplayKitModuleInterface::Get().Initialize(bMicrophoneEnabled, false);
    IIOSReplayKitModuleInterface::Get().StartCaptureToFile();
}

void UIOSReplayKitControl::StopCapture()
{
    IIOSReplayKitModuleInterface::Get().StopCapture();
}
