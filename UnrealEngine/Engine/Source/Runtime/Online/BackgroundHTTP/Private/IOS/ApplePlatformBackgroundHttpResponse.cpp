// Copyright Epic Games, Inc. All Rights Reserved.

#include "IOS/ApplePlatformBackgroundHttpResponse.h"

FApplePlatformBackgroundHttpResponse::FApplePlatformBackgroundHttpResponse(int32 ResponseCodeIn, const FString& TempContentFilePathIn)
{
    TempContentFilePath = TempContentFilePathIn;
    ResponseCode = ResponseCodeIn;
}
