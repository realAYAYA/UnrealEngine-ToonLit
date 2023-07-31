// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Logging/LogMacros.h"
#include "PixelCapturePrivate.h"

DEFINE_LOG_CATEGORY(LogPixelCapture);

class FPixelCaptureModule : public IModuleInterface
{
};

IMPLEMENT_MODULE(FPixelCaptureModule, PixelCapture)
