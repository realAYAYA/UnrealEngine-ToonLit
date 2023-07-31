// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if ELECTRA_HTTPSTREAM_APPLE

namespace ElectraHTTPStreamApple
{
	FString GetErrorMessage(NSError* ErrorCode);
	void LogError(const FString& Message);
}

#endif
