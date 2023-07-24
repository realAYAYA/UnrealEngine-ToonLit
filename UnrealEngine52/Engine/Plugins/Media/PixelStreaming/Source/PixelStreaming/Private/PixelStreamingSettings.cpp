// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixelStreamingSettings.h"
#include "Settings.h"

FPixelStreamingSimulcastParameters FPixelStreamingSettings::GetSimulcastParameters() 
{
	return UE::PixelStreaming::Settings::SimulcastParameters;
}

bool FPixelStreamingSettings::GetCaptureUseFence() 
{
	return UE::PixelStreaming::Settings::CVarPixelStreamingCaptureUseFence.GetValueOnAnyThread();
}

bool FPixelStreamingSettings::GetVPXUseCompute() 
{
	return UE::PixelStreaming::Settings::CVarPixelStreamingVPXUseCompute.GetValueOnAnyThread();
}