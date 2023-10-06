// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "CurlElectra.h"

namespace ElectraHTTPStreamLibCurl
{
	FString GetErrorMessage(CURLcode ErrorCode);
	FString GetErrorMessage(CURLMcode ErrorCode);
	void LogError(const FString& Message);
}
