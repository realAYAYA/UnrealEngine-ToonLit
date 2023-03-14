// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "Templates/Function.h"

typedef TFunction<void(int32 HttpStatus)> FDownloadComplete;
typedef TFunction<void(int32 BytesReceived)> FDownloadProgress;
typedef TFunction<void(void)> FDownloadCancel;

extern FDownloadCancel PlatformStreamDownload(const FString& Url, const FString& TargetFile, const FDownloadProgress& Progress, const FDownloadComplete& Callback);
