// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace ElectraHTTPStreamWinHttp
{
	FString GetSecurityErrorMessage(uint32 Flags);
	FString GetApiName(uint32 ApiNameCode);
	FString GetErrorMessage(uint32 ErrorCode);
	FString GetErrorLogMessage(const TCHAR* const Method, uint32 ErrorCode);
	void LogError(const TCHAR* const Method, uint32 ErrorCode);
	void LogError(const FString& Message);
}
