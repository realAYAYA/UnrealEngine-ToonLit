// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkInputDevice.h"

#define LOCTEXT_NAMESPACE "LiveLinkInputDeviceModule"

DEFINE_LOG_CATEGORY(LogLiveLinkInputDevice);

void FLiveLinkInputDeviceModule::StartupModule()
{
}

void FLiveLinkInputDeviceModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FLiveLinkInputDeviceModule, LiveLinkInputDevice)