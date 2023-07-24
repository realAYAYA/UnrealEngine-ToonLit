// Copyright Epic Games, Inc. All Rights Reserved.

#include "AndroidFetchPlatformBackgroundHttp.h"

#include "AndroidPlatformBackgroundHttpManager.h"
#include "AndroidPlatformBackgroundHttpRequest.h"
#include "AndroidPlatformBackgroundHttpResponse.h"

void FAndroidFetchPlatformBackgroundHttp::Initialize()
{
}

void FAndroidFetchPlatformBackgroundHttp::Shutdown()
{
}

FBackgroundHttpManagerPtr FAndroidFetchPlatformBackgroundHttp::CreatePlatformBackgroundHttpManager()
{
    return MakeShared<FAndroidPlatformBackgroundHttpManager, ESPMode::ThreadSafe>();
}

FBackgroundHttpRequestPtr FAndroidFetchPlatformBackgroundHttp::ConstructBackgroundRequest()
{
	return MakeShared<FAndroidPlatformBackgroundHttpRequest, ESPMode::ThreadSafe>();
}

FBackgroundHttpResponsePtr FAndroidFetchPlatformBackgroundHttp::ConstructBackgroundResponse(int32 ResponseCode, const FString& TempFilePath)
{
	return MakeShared<FAndroidPlatformBackgroundHttpResponse, ESPMode::ThreadSafe>(ResponseCode, TempFilePath);
}

bool FAndroidFetchPlatformBackgroundHttp::CheckRequirementsSupported()
{
	return FAndroidPlatformBackgroundHttpManager::HandleRequirementsCheck();
}