// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BackgroundHttpResponseImpl.h"
#include "Interfaces/IBackgroundHttpRequest.h"

/**
 * Contains implementation of some common functions that don't vary between implementation
 */
class FAndroidPlatformBackgroundHttpResponse
	: public FBackgroundHttpResponseImpl
{
public:
    //Provide constructor to create an already
    FAndroidPlatformBackgroundHttpResponse(int32 ResponseCode, const FString& TempContentFilePath);
};
