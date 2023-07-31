// Copyright Epic Games, Inc. All Rights Reserved.

#include "AndroidPlatformBackgroundHttpResponse.h"

FAndroidPlatformBackgroundHttpResponse::FAndroidPlatformBackgroundHttpResponse(int32 ResponseCodeIn, const FString& TempContentFilePathIn)
{
    TempContentFilePath = TempContentFilePathIn;
    ResponseCode = ResponseCodeIn;
}
