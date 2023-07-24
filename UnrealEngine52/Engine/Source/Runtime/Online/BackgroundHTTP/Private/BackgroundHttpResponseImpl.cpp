// Copyright Epic Games, Inc. All Rights Reserved.

#include "BackgroundHttpResponseImpl.h"

#include "Interfaces/IHttpResponse.h"

#include "HAL/PlatformApplicationMisc.h"
#include "Misc/Paths.h"

FBackgroundHttpResponseImpl::FBackgroundHttpResponseImpl()
	: TempContentFilePath()
	, ResponseCode(EHttpResponseCodes::Unknown)
{
}

int32 FBackgroundHttpResponseImpl::GetResponseCode() const
{
	return ResponseCode;
}

const FString& FBackgroundHttpResponseImpl::GetTempContentFilePath() const
{
	return TempContentFilePath;
}