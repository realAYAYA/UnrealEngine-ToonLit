// Copyright Epic Games, Inc. All Rights Reserved.
#include "ProfilingDebugging/TraceScreenshot.h"

#include "Containers/Map.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "ImageUtils.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformTLS.h"
#include "HAL/TlsAutoCleanup.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"
#include "ProfilingDebugging/MiscTrace.h"
#include "Tasks/Task.h"
#include "Trace/Trace.inl"

static void TraceScreenshotCommandCallback(const TArray<FString>& Args)
{
	FString Name;
	if (Args.Num() > 0)
	{
		Name = Args[0];
	}

	FTraceScreenshot::RequestScreenshot(Name, LogConsoleResponse);
}

static FAutoConsoleCommand TraceScreenshotCmd(
	TEXT("Trace.Screenshot"),
	TEXT("[Name] Takes a screenshot and saves it in the trace."
		" [Name] is the name of the screenshot."
	),
	FConsoleCommandWithArgsDelegate::CreateStatic(TraceScreenshotCommandCallback)
);

bool FTraceScreenshot::bSuppressWritingToFile = false;

void FTraceScreenshot::RequestScreenshot(FString Name, const FLogCategoryAlias& LogCategory)
{
	if (!SHOULD_TRACE_SCREENSHOT())
	{
		UE_LOG_REF(LogCategory, Error, TEXT("Could not trace screenshot because the screenshot trace channel is off. Turn it on using \"Trace.Enable Screenshot\"."));
		return;
	}

	bSuppressWritingToFile = true;

	if (Name.IsEmpty())
	{
		Name = FDateTime::Now().ToString(TEXT("Screenshot_%Y%m%d_%H%M%S"));
	}
	constexpr bool bShowUI = false;
	constexpr bool bAddUniqueSuffix = false;
	FScreenshotRequest::RequestScreenshot(Name, bShowUI, bAddUniqueSuffix);
}

void FTraceScreenshot::TraceScreenshot(int32 InSizeX, int32 InSizeY, const TArray<FColor>& InImageData, const FString& InScreenshotName, int32 DesiredX)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ScreenshotTracing_Prepare);
	FString ScreenshotName = FPaths::GetBaseFilename(InScreenshotName);

	if (ScreenshotName.IsEmpty())
	{
		ScreenshotName = FDateTime::Now().ToString(TEXT("Screenshot_%Y%m%d_%H%M%S"));
	}

	UE_LOG(LogCore, Display, TEXT("Tracing Screenshot \"%s\" taken with size: %d x %d"), *ScreenshotName, InSizeX, InSizeY);
	
	uint64 Cycles = FPlatformTime::Cycles64();
	TArray<FColor> *ImageCopy = new TArray<FColor>(InImageData);
	
	UE::Tasks::Launch(UE_SOURCE_LOCATION, 
		[ImageCopy, Cycles, ScreenshotName, InSizeX, InSizeY, DesiredX]
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(ScreenshotTracing_Execute);

			// Set full alpha on the bitmap
			for (FColor& Pixel : *ImageCopy)
			{
				Pixel.A = 255;
			}

			TArray64<uint8> CompressedBitmap;
			if (DesiredX > 0 && InSizeX != DesiredX)
			{
				int32 ResizedX = FMath::Min(640, InSizeX);
				int32 ResizedY = (InSizeY * ResizedX) / InSizeX;

				TArray<FColor> ResizedImage;
				ResizedImage.SetNum(ResizedX * ResizedY);
				FImageUtils::ImageResize(InSizeX, InSizeY, *ImageCopy, ResizedX, ResizedY, ResizedImage, false);

				FImageUtils::PNGCompressImageArray(ResizedX, ResizedY, TArrayView64<const FColor>(ResizedImage.GetData(), ResizedImage.Num()), CompressedBitmap);
				TRACE_SCREENSHOT(*ScreenshotName, Cycles, ResizedX, ResizedY, CompressedBitmap);
			}
			else
			{
				FImageUtils::PNGCompressImageArray(InSizeX, InSizeY, TArrayView64<const FColor>(ImageCopy->GetData(), ImageCopy->Num()), CompressedBitmap);
				TRACE_SCREENSHOT(*ScreenshotName, Cycles, InSizeX, InSizeY, CompressedBitmap);
			}

			delete ImageCopy;
		});

	Reset();
}

void FTraceScreenshot::Reset()
{
	bSuppressWritingToFile = false;
}
