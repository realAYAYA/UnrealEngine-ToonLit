// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BackgroundHttpResponseImpl.h"
#include "Containers/UnrealString.h"
#include "HAL/Platform.h"
#include "Interfaces/IHttpRequest.h"

/**
 * Contains implementation of some common functions that don't vary between implementation
 */
class FGenericPlatformBackgroundHttpResponse
	: public FBackgroundHttpResponseImpl
{
public:
	//Provide constructor to create an already 
	FGenericPlatformBackgroundHttpResponse(int32 ResponseCode, const FString& TempContentFilePath);

	//Provide constuctor to create a request from underlying HttpRequest information
	FGenericPlatformBackgroundHttpResponse(FHttpRequestPtr HttpRequestIn, FHttpResponsePtr HttpResponse, bool bSuccess);
};