// Copyright Epic Games, Inc. All Rights Reserved.

#include "AndroidEOSSDKManager.h"

#if WITH_EOS_SDK

#include "HAL/FileManager.h"
#include "EOSShared.h"

#include "eos_android.h"

EOS_EResult FAndroidEOSSDKManager::EOSInitialize(EOS_InitializeOptions& Options)
{
	EOS_Android_InitializeOptions SystemInitializeOptions = { 0 };
	SystemInitializeOptions.ApiVersion = 2;
	UE_EOS_CHECK_API_MISMATCH(EOS_ANDROID_INITIALIZEOPTIONS_API_LATEST, 2);
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