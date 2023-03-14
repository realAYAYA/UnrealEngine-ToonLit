// Copyright Epic Games, Inc. All Rights Reserved.

#include "AndroidEOSSDKManager.h"
#include "HAL/FileManager.h"

#if WITH_EOS_SDK

#include "eos_android.h"

EOS_EResult FAndroidEOSSDKManager::EOSInitialize(EOS_InitializeOptions& Options)
{
	EOS_Android_InitializeOptions SystemInitializeOptions = { 0 };
	SystemInitializeOptions.ApiVersion = EOS_ANDROID_INITIALIZEOPTIONS_API_LATEST;
	static_assert(EOS_ANDROID_INITIALIZEOPTIONS_API_LATEST == 2, "EOS_Android_InitializeOptions updated, check new fields");
	SystemInitializeOptions.Reserved = nullptr;
	SystemInitializeOptions.OptionalInternalDirectory = nullptr;
	SystemInitializeOptions.OptionalExternalDirectory = nullptr;

	Options.SystemInitializeOptions = &SystemInitializeOptions;

	return FEOSSDKManager::EOSInitialize(Options);
}

FString FAndroidEOSSDKManager::GetCacheDirBase() const
{
	FString BaseCacheDirBase = FEOSSDKManager::GetCacheDirBase();
	return IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*BaseCacheDirBase);
};

#endif // WITH_EOS_SDK