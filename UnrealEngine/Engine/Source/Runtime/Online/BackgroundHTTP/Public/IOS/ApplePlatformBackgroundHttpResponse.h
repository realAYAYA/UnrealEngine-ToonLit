// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BackgroundHttpResponseImpl.h"
#include "Interfaces/IBackgroundHttpRequest.h"

/**
 * Contains implementation of some common functions that don't vary between implementation
 */
class FApplePlatformBackgroundHttpResponse
	: public FBackgroundHttpResponseImpl
{
public:
    //Provide constructor to create an already
    FApplePlatformBackgroundHttpResponse(int32 ResponseCode, const FString& TempContentFilePath);
};
