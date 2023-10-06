// Copyright Epic Games, Inc. All Rights Reserved.
#include "IOSEOSSDKManager.h"

#if WITH_EOS_SDK

#include "Misc/CoreDelegates.h"

FIOSEOSSDKManager::FIOSEOSSDKManager()
{
	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddRaw(this, &FIOSEOSSDKManager::OnApplicationStatusChanged, EOS_EApplicationStatus::EOS_AS_Foreground);
	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddRaw(this, &FIOSEOSSDKManager::OnApplicationStatusChanged, EOS_EApplicationStatus::EOS_AS_BackgroundSuspended);
}

FIOSEOSSDKManager::~FIOSEOSSDKManager()
{	
	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.RemoveAll(this);
	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.RemoveAll(this);
}

FString FIOSEOSSDKManager::GetCacheDirBase() const
{
	NSString* BundleIdentifier = [[NSBundle mainBundle]bundleIdentifier];
	NSString* CacheDirectory = NSTemporaryDirectory(); // Potentially use NSCachesDirectory
	CacheDirectory = [CacheDirectory stringByAppendingPathComponent : BundleIdentifier];

	const char* CStrCacheDirectory = [CacheDirectory UTF8String];
	return FString(UTF8_TO_TCHAR(CStrCacheDirectory));
};

#endif // WITH_EOS_SDK