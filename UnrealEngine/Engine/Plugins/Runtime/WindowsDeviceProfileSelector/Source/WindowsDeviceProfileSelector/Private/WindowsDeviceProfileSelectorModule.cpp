// Copyright Epic Games, Inc. All Rights Reserved.

#include "WindowsDeviceProfileSelectorModule.h"
#include "Modules/ModuleManager.h"
#include "RHI.h"
#include "RHICommandList.h"
#include "DynamicRHI.h"
#include "Misc/ConfigCacheIni.h"

IMPLEMENT_MODULE(FWindowsDeviceProfileSelectorModule, WindowsDeviceProfileSelector);


void FWindowsDeviceProfileSelectorModule::StartupModule()
{
}


void FWindowsDeviceProfileSelectorModule::ShutdownModule()
{
}

const FString FWindowsDeviceProfileSelectorModule::GetRuntimeDeviceProfileName()
{
#if defined(WINDOWS_OVERRIDE_DEVICEPROFILE_NAME)
	// overridden device profile name, typically for Windows-based platform extensions
	FString ProfileName = WINDOWS_OVERRIDE_DEVICEPROFILE_NAME;
#elif UE_IS_COOKED_EDITOR
	// some heuristics to determine a cooked editor
	FString ProfileName = TEXT("WindowsCookedEditor");
#else
	// Windows, WindowsEditor, WindowsClient, or WindowsServer
	FString ProfileName = FPlatformProperties::PlatformName();
#endif

	if (FApp::CanEverRender())
	{
		TArray<FString> AvailableProfiles;
		GConfig->GetSectionNames(GDeviceProfilesIni, AvailableProfiles);

		FString TmpProfileName = ProfileName + TCHAR('_') + GetSelectedDynamicRHIModuleName(false);
		if (AvailableProfiles.Contains(FString::Printf(TEXT("%s DeviceProfile"), *TmpProfileName)))
		{
			// Use RHI specific device profile if exist
			ProfileName = MoveTemp(TmpProfileName);
		}
	}

	UE_LOG(LogInit, Log, TEXT("Selected Device Profile: [%s]"), *ProfileName);
	return ProfileName;
}
