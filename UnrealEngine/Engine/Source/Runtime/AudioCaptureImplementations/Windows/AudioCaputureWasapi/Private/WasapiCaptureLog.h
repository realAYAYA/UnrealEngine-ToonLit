// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AudioCaptureCoreLog.h"

#define WASAPI_CAPTURE_LOG_RESULT(FunctionName, Result) \
	{ \
		FString ErrorString = FString::Printf(TEXT("%s -> 0x%X (line: %d)"), TEXT( FunctionName ), Result, __LINE__); \
		UE_LOG(LogAudioCaptureCore, Error, TEXT("WasapiCapture Error: %s"), *ErrorString);				  \
	}
