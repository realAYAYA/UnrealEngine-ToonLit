// Copyright Epic Games, Inc. All Rights Reserved.

#include "LinuxDeviceProfileSelectorModule.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FLinuxDeviceProfileSelectorModule, LinuxDeviceProfileSelector);


void FLinuxDeviceProfileSelectorModule::StartupModule()
{
}


void FLinuxDeviceProfileSelectorModule::ShutdownModule()
{
}


FString const FLinuxDeviceProfileSelectorModule::GetRuntimeDeviceProfileName()
{
#if UE_IS_COOKED_EDITOR
	// some heuristics to determine a cooked editor
	FString ProfileName = TEXT("LinuxCookedEditor");
#else
	// [RCL] 2015-09-22 FIXME: support different environments
	FString ProfileName = FPlatformProperties::PlatformName();
#endif

	UE_LOG(LogLinux, Log, TEXT("Selected Device Profile: [%s]"), *ProfileName);
	return ProfileName;
}

