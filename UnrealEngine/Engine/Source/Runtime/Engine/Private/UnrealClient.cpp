// Copyright Epic Games, Inc. All Rights Reserved.


#include "UnrealClient.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/App.h"
#include "EngineStats.h"
#include "EngineGlobals.h"
#include "RenderingThread.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Misc/ConfigCacheIni.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "UnrealEngine.h"
#include "Components/PostProcessComponent.h"
#include "HighResScreenshot.h"
#include "GameFramework/GameUserSettings.h"
#include "HModel.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Engine/PostProcessVolume.h"
#include "RendererInterface.h"
#include "EngineModule.h"
#include "Performance/EnginePerformanceTargets.h"
#include "Templates/UniquePtr.h"
#include "Elements/Framework/TypedElementList.h"
#include "EngineUtils.h"
#include "RenderGraphUtils.h"
#include "DynamicResolutionState.h"

DEFINE_LOG_CATEGORY_STATIC(LogClient, Log, All);

UE_IMPLEMENT_STRUCT("/Script/Engine", PostProcessSettings);

bool FViewport::bIsGameRenderingEnabled = true;
int32 FViewport::PresentAndStopMovieDelay = 0;

static const FName NAME_DummyViewport = FName(TEXT("DummyViewport"));

/**
* Reads the viewport's displayed pixels into a preallocated color buffer.
* @param OutImageData - RGBA8 values will be stored in this buffer
* @param TopLeftX - Top left X pixel to capture
* @param TopLeftY - Top left Y pixel to capture
* @param Width - Width of image in pixels to capture
* @param Height - Height of image in pixels to capture
* @return True if the read succeeded.
*/
bool FRenderTarget::ReadPixels(TArray< FColor >& OutImageData, FReadSurfaceDataFlags InFlags, FIntRect InRect)
{
	if(InRect == FIntRect(0, 0, 0, 0))
	{
		InRect = FIntRect(0, 0, GetSizeXY().X, GetSizeXY().Y);
	}

	// Read the render target surface data back.	
	struct FReadSurfaceContext
	{
		FRenderTarget* SrcRenderTarget;
		TArray<FColor>* OutData;
		FIntRect Rect;
		FReadSurfaceDataFlags Flags;
	};

	OutImageData.Reset();
	FReadSurfaceContext Context =
	{
		this,
		&OutImageData,
		InRect,
		InFlags
	};

	ENQUEUE_RENDER_COMMAND(ReadSurfaceCommand)(
		[Context](FRHICommandListImmediate& RHICmdList)
		{
			RHICmdList.ReadSurfaceData(
				Context.SrcRenderTarget->GetRenderTargetTexture(),
				Context.Rect,
				*Context.OutData,
				Context.Flags
				);
		});
	FlushRenderingCommands();

	return OutImageData.Num() > 0;
}


/**
* Reads the viewport's displayed pixels into a preallocated color buffer.
* @param OutputBuffer - RGBA8 values will be stored in this buffer
* @return True if the read succeeded.
*/
bool FRenderTarget::ReadPixelsPtr(FColor* OutImageBytes, FReadSurfaceDataFlags InFlags, FIntRect InRect)
{
	TArray<FColor> SurfaceData;

	bool bResult = ReadPixels( SurfaceData, InFlags, InRect );
	if( bResult )
	{
		FMemory::Memcpy( OutImageBytes, &SurfaceData[ 0 ], SurfaceData.Num() * sizeof(FColor) );
	}

	return bResult;
}

/**
 * Reads the viewport's displayed pixels into a preallocated color buffer.
 * @param OutImageBytes - RGBA16F values will be stored in this buffer.  Buffer must be preallocated with the correct size!
 * @param CubeFace - optional cube face for when reading from a cube render target
 * @return True if the read succeeded.
 */
bool FRenderTarget::ReadFloat16Pixels(FFloat16Color* OutImageData,ECubeFace CubeFace)
{
	// Read the render target surface data back.	
	struct FReadSurfaceFloatContext
	{
		FRenderTarget* SrcRenderTarget;
		TArray<FFloat16Color>* OutData;
		FIntRect Rect;
		ECubeFace CubeFace;
	};
	
	TArray<FFloat16Color> SurfaceData;
	FReadSurfaceFloatContext Context =
	{
		this,
		&SurfaceData,
		FIntRect(0, 0, GetSizeXY().X, GetSizeXY().Y),
		CubeFace	
	};

	ENQUEUE_RENDER_COMMAND(ReadSurfaceFloatCommand)(
		[Context](FRHICommandListImmediate& RHICmdList)
		{
			RHICmdList.ReadSurfaceFloatData(
				Context.SrcRenderTarget->GetRenderTargetTexture(),
				Context.Rect,
				*Context.OutData,
				Context.CubeFace,
				0,
				0
				);
		});
	FlushRenderingCommands();

	// Copy the surface data into the output array.
	FFloat16Color* OutImageColors = reinterpret_cast< FFloat16Color* >(OutImageData);

	// Cache width and height as its very expensive to call these virtuals in inner loop (never inlined)
	const int32 ImageWidth = GetSizeXY().X;
	const int32 ImageHeight = GetSizeXY().Y;
	for (int32 Y = 0; Y < ImageHeight; Y++)
	{
		FFloat16Color* SourceData = (FFloat16Color*)SurfaceData.GetData() + Y * ImageWidth;
		for (int32 X = 0; X < ImageWidth; X++)
		{
			OutImageColors[ Y * ImageWidth + X ] = SourceData[X];
		}
	}

	return true;
}

/**
 * Reads the viewport's displayed pixels into the given color buffer.
 * @param OutputBuffer - RGBA16F values will be stored in this buffer
 * @param CubeFace - optional cube face for when reading from a cube render target
 * @return True if the read succeeded.
 */
bool FRenderTarget::ReadFloat16Pixels(TArray<FFloat16Color>& OutputBuffer,ECubeFace CubeFace)
{
	// Copy the surface data into the output array.
	OutputBuffer.SetNumUninitialized(GetSizeXY().X * GetSizeXY().Y, true);
	return ReadFloat16Pixels((FFloat16Color*)&(OutputBuffer[0]), CubeFace);
}

/**
* Reads the viewport's displayed pixels into a preallocated color buffer.
* @param OutImageData - LinearColor array to fill!
* @param CubeFace - optional cube face for when reading from a cube render target
* @return True if the read succeeded.
*/
bool FRenderTarget::ReadLinearColorPixels(TArray<FLinearColor> &OutImageData, FReadSurfaceDataFlags InFlags, FIntRect InRect)
{
	if (InRect == FIntRect(0, 0, 0, 0))
	{
		InRect = FIntRect(0, 0, GetSizeXY().X, GetSizeXY().Y);
	}

	// Read the render target surface data back.	
	struct FReadSurfaceContext
	{
		FRenderTarget* SrcRenderTarget;
		TArray<FLinearColor>* OutData;
		FIntRect Rect;
		FReadSurfaceDataFlags Flags;
	};

	OutImageData.Reset();
	FReadSurfaceContext Context =
	{
		this,
		&OutImageData,
		InRect,
		InFlags
	};

	ENQUEUE_RENDER_COMMAND(ReadSurfaceCommand)(
		[Context](FRHICommandListImmediate& RHICmdList)
		{
			RHICmdList.ReadSurfaceData(
			Context.SrcRenderTarget->GetRenderTargetTexture(),
				Context.Rect,
				*Context.OutData,
				Context.Flags
				);
		});
	FlushRenderingCommands();

	return OutImageData.Num() > 0;
}

/**
* Reads the viewport's displayed pixels into a preallocated color buffer.
* @param OutputBuffer - RGBA8 values will be stored in this buffer
* @return True if the read succeeded.
*/
bool FRenderTarget::ReadLinearColorPixelsPtr(FLinearColor* OutImageBytes, FReadSurfaceDataFlags InFlags, FIntRect InRect)
{
	TArray<FLinearColor> SurfaceData;

	bool bResult = ReadLinearColorPixels(SurfaceData, InFlags, InRect);
	if (bResult)
	{
		check(SurfaceData.Num() != 0);
		FMemory::Memcpy(OutImageBytes, &SurfaceData[0], SurfaceData.Num() * sizeof(FLinearColor));
	}

	return bResult;
}

/** 
* @return display gamma expected for rendering to this render target 
*/
float FRenderTarget::GetDisplayGamma() const
{
	if (GEngine == NULL)
	{
		return 2.2f;
	}
	else
	{
		if (FMath::Abs(GEngine->DisplayGamma) <= 0.0f)
		{
			UE_LOG(LogClient, Error, TEXT("Invalid DisplayGamma! Resetting to the default of 2.2"));
			GEngine->DisplayGamma = 2.2f;
		}
		return GEngine->DisplayGamma;
	}
}

/**
* Accessor for the surface RHI when setting this render target
* @return render target surface RHI resource
*/
const FTextureRHIRef& FRenderTarget::GetRenderTargetTexture() const
{
	return RenderTargetTextureRHI;
}

FRDGTextureRef FRenderTarget::GetRenderTargetTexture(FRDGBuilder& GraphBuilder) const
{
	return RegisterExternalTexture(GraphBuilder, GetRenderTargetTexture(), TEXT("RenderTarget"));
}

FUnorderedAccessViewRHIRef FRenderTarget::GetRenderTargetUAV() const
{
	return FUnorderedAccessViewRHIRef();
}

void FScreenshotRequest::RequestScreenshot(bool bInShowUI)
{
	// empty string means we'll later pick the name
	RequestScreenshot(TEXT(""), bInShowUI, true);
}

void FScreenshotRequest::RequestScreenshot(const FString& InFilename, bool bInShowUI, bool bAddUniqueSuffix)
{
	FString GeneratedFilename = InFilename;
	CreateViewportScreenShotFilename(GeneratedFilename);

	if (bAddUniqueSuffix)
	{
		const bool bRemovePath = false;
		GeneratedFilename = FPaths::GetBaseFilename(GeneratedFilename, bRemovePath);
		if (GetHighResScreenshotConfig().bDateTimeBasedNaming)
		{
			FFileHelper::GenerateDateTimeBasedBitmapFilename(GeneratedFilename, TEXT("png"), Filename);
		}
		else
		{
			FFileHelper::GenerateNextBitmapFilename(GeneratedFilename, TEXT("png"), Filename);
		}
	}
	else
	{
		Filename = GeneratedFilename;
		if (FPaths::GetExtension(Filename).Len() == 0)
		{
			Filename += TEXT(".png");
		}
	}

	// Register the screenshot
	if (!Filename.IsEmpty())
	{
		bShowUI = bInShowUI;
		bIsScreenshotRequested = true;
	}

	GScreenMessagesRestoreState = GAreScreenMessagesEnabled;

	// Disable Screen Messages when the screenshot is requested without UI.
	if (bInShowUI == false)
	{
		GAreScreenMessagesEnabled = false;
	}
}


void FScreenshotRequest::Reset()
{
	bIsScreenshotRequested = false;
	Filename.Empty();
	bShowUI = false;
}

void FScreenshotRequest::CreateViewportScreenShotFilename(FString& InOutFilename)
{
	FString TypeName;

	if(GIsDumpingMovie)
	{
		TypeName = TEXT("MovieFrame");

		if(GIsDumpingMovie > 0)
		{
			// <=0:off (default), <0:remains on, >0:remains on for n frames (n is the number specified)
			--GIsDumpingMovie;
		}
	}
	else if(GIsHighResScreenshot)
	{
		FString FilenameOverride = GetHighResScreenshotConfig().FilenameOverride;
		TypeName = FilenameOverride.IsEmpty() ? TEXT("HighresScreenshot") : FilenameOverride;
	}
	else
	{
		TypeName = InOutFilename.IsEmpty() ? TEXT("ScreenShot") : InOutFilename;
	}
	check(!TypeName.IsEmpty());

	//default to using the path that is given
	InOutFilename = TypeName;
	if (!TypeName.Contains(TEXT("/")))
	{
		InOutFilename = GetDefault<UEngine>()->GameScreenshotSaveDirectory.Path / TypeName;
	}
}

TArray<FColor>* FScreenshotRequest::GetHighresScreenshotMaskColorArray()
{
	return &HighresScreenshotMaskColorArray;
}

FIntPoint& FScreenshotRequest::GetHighresScreenshotMaskExtents()
{
	return HighresScreenshotMaskExtents;
}

// @param bAutoType true: automatically choose GB/MB/KB/... false: always use MB for easier comparisons
FString GetMemoryString( const double Value, const bool bAutoType )
{
	if (bAutoType)
	{
		if (Value > 1024.0 * 1024.0 * 1024.0)
		{
			return FString::Printf( TEXT( "%.2f GB" ), float( Value / (1024.0 * 1024.0 * 1024.0) ) );
		}
		if (Value > 1024.0 * 1024.0)
		{
			return FString::Printf( TEXT( "%.2f MB" ), float( Value / (1024.0 * 1024.0) ) );
		}
		if (Value > 1024.0)
		{
			return FString::Printf( TEXT( "%.2f KB" ), float( Value / (1024.0) ) );
		}
		return FString::Printf( TEXT( "%.2f B" ), float( Value ) );
	}
	
	return FString::Printf( TEXT( "%.2f MB" ), float( Value / (1024.0 * 1024.0) ) );
}

FOnScreenshotRequestProcessed FScreenshotRequest::ScreenshotProcessedDelegate;
FOnScreenshotCaptured FScreenshotRequest::ScreenshotCapturedDelegate;
bool FScreenshotRequest::bIsScreenshotRequested = false;
FString FScreenshotRequest::Filename;
FString FScreenshotRequest::NextScreenshotName;
bool FScreenshotRequest::bShowUI = false;
TArray<FColor> FScreenshotRequest::HighresScreenshotMaskColorArray;
FIntPoint FScreenshotRequest::HighresScreenshotMaskExtents;

static TAutoConsoleVariable<int32> CVarFullSizeUnitGraph(
	TEXT("FullSizeUnitGraph"),
	0,
	TEXT("If true, the unit graph is the old full size, full brightness version."));


int32 FStatUnitData::DrawStat(FViewport* InViewport, FCanvas* InCanvas, int32 InX, int32 InY)
{
	float DiffTime;
	if (FApp::IsBenchmarking() || FApp::UseFixedTimeStep())
	{
		/** If we're in fixed time step mode, FApp::GetCurrentTime() will be incorrect for benchmarking */
		const double CurrentTime = FPlatformTime::Seconds();
		if (LastTime == 0)
		{
			LastTime = CurrentTime;
		}
		DiffTime = CurrentTime - LastTime;
		LastTime = CurrentTime;
	}
	else
	{
		/** Use the DiffTime we computed last frame, because it correctly handles the end of frame idling and corresponds better to the other unit times. */
		DiffTime = FApp::GetCurrentTime() - FApp::GetLastTime();
	}

	RawFrameTime = DiffTime * 1000.0f;
	FrameTime = 0.9 * FrameTime + 0.1 * RawFrameTime;

	/** Number of milliseconds the gamethread was used last frame. */
	RawGameThreadTime = FPlatformTime::ToMilliseconds(GGameThreadTime);
	GameThreadTime = 0.9 * GameThreadTime + 0.1 * RawGameThreadTime;

	/** Number of milliseconds the renderthread was used last frame. */
	RawRenderThreadTime = FPlatformTime::ToMilliseconds(GRenderThreadTime);
	RenderThreadTime = 0.9 * RenderThreadTime + 0.1 * RawRenderThreadTime;

	RawRHITTime = FPlatformTime::ToMilliseconds(GRHIThreadTime);
	RHITTime = 0.9 * RHITTime + 0.1 * RawRHITTime;

	RawInputLatencyTime = FPlatformTime::ToMilliseconds64(GInputLatencyTime);
	InputLatencyTime = 0.9 * InputLatencyTime + 0.1 * RawInputLatencyTime;

	FDynamicResolutionStateInfos DynamicResolutionStateInfos;
	GEngine->GetDynamicResolutionCurrentStateInfos(/* out */ DynamicResolutionStateInfos);

	for (uint32 GPUIndex : FRHIGPUMask::All())
	{
		/** Number of milliseconds the GPU was busy last frame. */
		const uint32 GPUCycles = RHIGetGPUFrameCycles(GPUIndex);
		RawGPUFrameTime[GPUIndex] = FPlatformTime::ToMilliseconds(GPUCycles);
		GPUFrameTime[GPUIndex] = 0.9 * GPUFrameTime[GPUIndex] + 0.1 * RawGPUFrameTime[GPUIndex];
	}

	SET_FLOAT_STAT(STAT_UnitFrame, FrameTime);
	SET_FLOAT_STAT(STAT_UnitRender, RenderThreadTime);
	SET_FLOAT_STAT(STAT_UnitRHIT, RHITTime);
	SET_FLOAT_STAT(STAT_UnitGame, GameThreadTime);
	SET_FLOAT_STAT(STAT_UnitGPU, GPUFrameTime[0]);
	SET_FLOAT_STAT(STAT_InputLatencyTime, InputLatencyTime);

	GEngine->SetAverageUnitTimes(FrameTime, RenderThreadTime, GameThreadTime, GPUFrameTime[0], RHITTime);

	float Max_RenderThreadTime = 0.0f;
	float Max_GameThreadTime = 0.0f;
	float Max_GPUFrameTime[MAX_NUM_GPUS] = { 0.0f };
	float Max_FrameTime = 0.0f;
	float Max_RHITTime = 0.0f;
	float Max_InputLatencyTime = 0.0f;

	const bool bShowUnitMaxTimes = InViewport->GetClient() ? InViewport->GetClient()->IsStatEnabled(TEXT("UnitMax")) : false;
#if !UE_BUILD_SHIPPING
	const bool bShowRawUnitTimes = InViewport->GetClient() ? InViewport->GetClient()->IsStatEnabled(TEXT("Raw")) : false;
	RenderThreadTimes[CurrentIndex] = bShowRawUnitTimes ? RawRenderThreadTime : RenderThreadTime;
	GameThreadTimes[CurrentIndex] = bShowRawUnitTimes ? RawGameThreadTime : GameThreadTime;
	for (uint32 GPUIndex : FRHIGPUMask::All())
	{
		GPUFrameTimes[GPUIndex][CurrentIndex] = bShowRawUnitTimes ? RawGPUFrameTime[GPUIndex] : GPUFrameTime[GPUIndex];
	}
	FrameTimes[CurrentIndex] = bShowRawUnitTimes ? RawFrameTime : FrameTime;
	RHITTimes[CurrentIndex] = bShowRawUnitTimes ? RawRHITTime : RHITTime;
	InputLatencyTimes[CurrentIndex] = bShowRawUnitTimes ? RawInputLatencyTime : InputLatencyTime;
	for (TLinkedList<DynamicRenderScaling::FBudget*>::TIterator BudgetIt(DynamicRenderScaling::FBudget::GetGlobalList()); BudgetIt; BudgetIt.Next())
	{
		const DynamicRenderScaling::FBudget& Budget = **BudgetIt;
		ResolutionFractions[Budget][CurrentIndex] = DynamicResolutionStateInfos.ResolutionFractionApproximations[Budget];
	}
	CurrentIndex++;
	if (CurrentIndex == NumberOfSamples)
	{
		CurrentIndex = 0;
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (bShowUnitMaxTimes)
	{
		for (int32 MaxIndex = 0; MaxIndex < NumberOfSamples; MaxIndex++)
		{
			if (Max_RenderThreadTime < RenderThreadTimes[MaxIndex])
			{
				Max_RenderThreadTime = RenderThreadTimes[MaxIndex];
			}
			if (Max_GameThreadTime < GameThreadTimes[MaxIndex])
			{
				Max_GameThreadTime = GameThreadTimes[MaxIndex];
			}
			for (uint32 GPUIndex : FRHIGPUMask::All())
			{
				if (Max_GPUFrameTime[GPUIndex] < GPUFrameTimes[GPUIndex][MaxIndex])
				{
					Max_GPUFrameTime[GPUIndex] = GPUFrameTimes[GPUIndex][MaxIndex];
				}
			}
			if (Max_FrameTime < FrameTimes[MaxIndex])
			{
				Max_FrameTime = FrameTimes[MaxIndex];
			}
			if (Max_RHITTime < RHITTimes[MaxIndex])
			{
				Max_RHITTime = RHITTimes[MaxIndex];
			}
			if (Max_InputLatencyTime < InputLatencyTimes[MaxIndex])
			{
				Max_InputLatencyTime = InputLatencyTimes[MaxIndex];
			}
		}
	}
#endif // #if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
#endif // #if !UE_BUILD_SHIPPING

	FColor StatRed(233, 109, 99);		// (Salmon) Red that survives video compression
	FColor StatGreen(127, 202, 159);	// (De York) Green that survives video compression
	FColor StatOrange(244, 186, 112);	// Orange that survives video compression
	FColor StatMagenda(204, 153, 204);	// (Deep Pink) Magenta that survives video compression

	// Render CPU thread and GPU frame times.
	const bool bStereoRendering = GEngine->IsStereoscopic3D(InViewport);
	UFont* Font = (!FPlatformProperties::SupportsWindowedMode() && GEngine->GetMediumFont()) ? GEngine->GetMediumFont() : GEngine->GetSmallFont();

	const bool bShowUnitTimeGraph = InViewport->GetClient() ? InViewport->GetClient()->IsStatEnabled(TEXT("UnitGraph")) : false;
	bool bHaveGPUData[MAX_NUM_GPUS] = { false };
	for (uint32 GPUIndex : FRHIGPUMask::All())
	{
		bHaveGPUData[GPUIndex] = RawGPUFrameTime[GPUIndex] > 0;
	}
	const bool bHaveInputLatencyData = InputLatencyTime > 0;

	const float AlertResolutionFraction = 0.70f; // Truncation of sqrt(0.5) for easier remembering.

	// Draw unit.
	{
		int32 X3 = InX * (bStereoRendering ? 0.5f : 1.0f);
		if (bShowUnitMaxTimes)
		{
			X3 -= (int32)((float)Font->GetStringSize(TEXT(" 000.00 ms ")));
		}

		int32 X2 = bShowUnitMaxTimes ? X3 - (int32)((float)Font->GetStringSize(TEXT(" 000.00 ms "))) : X3;
		const int32 RowHeight = FMath::TruncToInt(Font->GetMaxCharHeight() * 1.1f);

		auto DrawTitleString = [&](const TCHAR* Title, const FColor& UnitGraphColor)
		{
			FString FullTitle = FString::Printf(TEXT("%s:   "), Title);
			int32 TitleSize = Font->GetStringSize(*FullTitle);
			InCanvas->DrawShadowedString(X2 - TitleSize, InY, *FullTitle, Font, bShowUnitTimeGraph ? UnitGraphColor : FColor::White);
		};

		{
			const FColor FrameTimeAverageColor = GEngine->GetFrameTimeDisplayColor(FrameTime);
			DrawTitleString(TEXT("Frame"), /* UnitGraphColor = */ FColor(100, 255, 100));
			InCanvas->DrawShadowedString(X2, InY, *FString::Printf(TEXT("%3.2f ms"), FrameTime), Font, FrameTimeAverageColor);
			if (bShowUnitMaxTimes)
			{
				const FColor MaxFrameTimeColor = GEngine->GetFrameTimeDisplayColor(Max_FrameTime);
				InCanvas->DrawShadowedString(X3, InY, *FString::Printf(TEXT("%4.2f ms"), Max_FrameTime), Font, MaxFrameTimeColor);
			}
			InY += RowHeight;
		}

		{
			const FColor GameThreadAverageColor = GEngine->GetFrameTimeDisplayColor(GameThreadTime);
			DrawTitleString(TEXT("Game"), /* UnitGraphColor = */ FColor(255, 100, 100));
			InCanvas->DrawShadowedString(X2, InY, *FString::Printf(TEXT("%3.2f ms"), GameThreadTime), Font, GameThreadAverageColor);
			if (bShowUnitMaxTimes)
			{
				const FColor GameThreadMaxColor = GEngine->GetFrameTimeDisplayColor(Max_GameThreadTime);
				InCanvas->DrawShadowedString(X3, InY, *FString::Printf(TEXT("%4.2f ms"), Max_GameThreadTime), Font, GameThreadMaxColor);
			}
			InY += RowHeight;
		}

		{
			const FColor RenderThreadAverageColor = GEngine->GetFrameTimeDisplayColor(RenderThreadTime);
			DrawTitleString(TEXT("Draw"), /* UnitGraphColor = */ FColor(100, 100, 255));
			InCanvas->DrawShadowedString(X2, InY, *FString::Printf(TEXT("%3.2f ms"), RenderThreadTime), Font, RenderThreadAverageColor);
			if (bShowUnitMaxTimes)
			{
				const FColor RenderThreadMaxColor = GEngine->GetFrameTimeDisplayColor(Max_RenderThreadTime);
				InCanvas->DrawShadowedString(X3, InY, *FString::Printf(TEXT("%4.2f ms"), Max_RenderThreadTime), Font, RenderThreadMaxColor);
			}
			InY += RowHeight;
		}

		for (uint32 GPUIndex : FRHIGPUMask::All())
		{
			if (bHaveGPUData[GPUIndex])
			{
				const FColor GPUAverageColor = GEngine->GetFrameTimeDisplayColor(GPUFrameTime[GPUIndex]);
				FString GPUString = GNumExplicitGPUsForRendering > 1 ? FString::Printf(TEXT("GPU%u"), GPUIndex) : TEXT("GPU");
				DrawTitleString(*GPUString, /* UnitGraphColor = */ FColor(255, 255, 100));
				InCanvas->DrawShadowedString(X2, InY, *FString::Printf(TEXT("%3.2f ms"), GPUFrameTime[GPUIndex]), Font, GPUAverageColor);
				if (bShowUnitMaxTimes)
				{
					const FColor GPUMaxColor = GEngine->GetFrameTimeDisplayColor(Max_GPUFrameTime[GPUIndex]);
					InCanvas->DrawShadowedString(X3, InY, *FString::Printf(TEXT("%4.2f ms"), Max_GPUFrameTime[GPUIndex]), Font, GPUMaxColor);
				}
				InY += RowHeight;
			}
		}
		if (IsRunningRHIInSeparateThread())
		{
			const FColor RenderThreadAverageColor = GEngine->GetFrameTimeDisplayColor(RHITTime);
			DrawTitleString(TEXT("RHIT"), /* UnitGraphColor = */ FColor(255, 100, 255));
			InCanvas->DrawShadowedString(X2, InY, *FString::Printf(TEXT("%3.2f ms"), RHITTime), Font, RenderThreadAverageColor);
			if (bShowUnitMaxTimes)
			{
				const FColor RenderThreadMaxColor = GEngine->GetFrameTimeDisplayColor(Max_RHITTime);
				InCanvas->DrawShadowedString(X3, InY, *FString::Printf(TEXT("%4.2f ms"), Max_RHITTime), Font, RenderThreadMaxColor);
			}
			InY += RowHeight;
		}
		if (bHaveInputLatencyData)
		{
			const float ReasonableInputLatencyFactor = 2.5f;
			const FColor InputLatencyAverageColor = GEngine->GetFrameTimeDisplayColor(InputLatencyTime / ReasonableInputLatencyFactor);
			DrawTitleString(TEXT("Input"), /* UnitGraphColor = */ FColor(255, 255, 100));
			InCanvas->DrawShadowedString(X2, InY, *FString::Printf(TEXT("%3.2f ms"), InputLatencyTime), Font, InputLatencyAverageColor);
			if (bShowUnitMaxTimes)
			{
				const FColor InputLatencyMaxColor = GEngine->GetFrameTimeDisplayColor(Max_InputLatencyTime / ReasonableInputLatencyFactor);
				InCanvas->DrawShadowedString(X3, InY, *FString::Printf(TEXT("%4.2f ms"), Max_InputLatencyTime), Font, InputLatencyMaxColor);
			}
			InY += RowHeight;
		}
		{
			if (bShowUnitMaxTimes)
			{
				FPlatformMemoryStats Stats = FPlatformMemory::GetStats();

				DrawTitleString(TEXT("Mem"), /* UnitGraphColor = */ FColor(100, 100, 255));
				InCanvas->DrawShadowedString(X2, InY, *GetMemoryString(Stats.UsedPhysical), Font, StatGreen);
				InCanvas->DrawShadowedString(X3, InY, *GetMemoryString(Stats.PeakUsedPhysical), Font, StatGreen);
				InY += RowHeight;
				
				DrawTitleString(TEXT("VMem"), /* UnitGraphColor = */ FColor(100, 100, 255));
				InCanvas->DrawShadowedString(X2, InY, *GetMemoryString(Stats.UsedVirtual), Font, StatGreen);
				InCanvas->DrawShadowedString(X3, InY, *GetMemoryString(Stats.PeakUsedVirtual), Font, StatGreen);
				InY += RowHeight;
			}
			else
			{
				uint64 MemoryUsed = FPlatformMemory::GetMemoryUsedFast();
				if (MemoryUsed > 0)
				{
					// print out currently used memory
					DrawTitleString(TEXT("Mem"), /* UnitGraphColor = */ FColor(100, 100, 255));
					InCanvas->DrawShadowedString(X2, InY, *GetMemoryString(MemoryUsed), Font, StatGreen);
					InY += RowHeight;
				}
			}
		}

		// Dynamic resolution
		{
			float ResolutionFraction = DynamicResolutionStateInfos.ResolutionFractionApproximations[GDynamicPrimaryResolutionFraction];
			float ScreenPercentage = ResolutionFraction * 100.0f;

			DrawTitleString(TEXT("DynRes"), /* UnitGraphColor = */ FColor(255, 160, 100));
			if (DynamicResolutionStateInfos.Status == EDynamicResolutionStatus::Enabled)
			{
				FColor Color = (ResolutionFraction < AlertResolutionFraction) ? StatRed : ((ResolutionFraction < FMath::Min(ResolutionFraction * 0.97f, 1.0f)) ? StatOrange : StatGreen);
				InCanvas->DrawShadowedString(X2, InY, *FString::Printf(TEXT("%3.1f%% x %3.1f%%"), ScreenPercentage, ScreenPercentage), Font, Color);
			}
			else if (DynamicResolutionStateInfos.Status == EDynamicResolutionStatus::DebugForceEnabled)
			{
				InCanvas->DrawShadowedString(X2, InY, *FString::Printf(TEXT("%3.1f%% x %3.1f%%"), ScreenPercentage, ScreenPercentage), Font, StatMagenda);
			}
			else if (DynamicResolutionStateInfos.Status == EDynamicResolutionStatus::Paused)
			{
				InCanvas->DrawShadowedString(X2, InY, TEXT("Paused"), Font, StatMagenda);
			}
			else if (DynamicResolutionStateInfos.Status == EDynamicResolutionStatus::Disabled)
			{
				InCanvas->DrawShadowedString(X2, InY, TEXT("OFF"), Font, FColor(160, 160, 160));
			}
			else if (DynamicResolutionStateInfos.Status == EDynamicResolutionStatus::Unsupported)
			{
				InCanvas->DrawShadowedString(X2, InY, TEXT("Unsupported"), Font, FColor(160, 160, 160));
			}
			else
			{
				check(0);
			}
			InY += RowHeight;
		}

		// Other dynamic render scalings
		if (DynamicResolutionStateInfos.Status == EDynamicResolutionStatus::Enabled && DynamicRenderScaling::IsSupported())
		{
			for (TLinkedList<DynamicRenderScaling::FBudget*>::TIterator BudgetIt(DynamicRenderScaling::FBudget::GetGlobalList()); BudgetIt; BudgetIt.Next())
			{
				const DynamicRenderScaling::FBudget& Budget = **BudgetIt;
				const DynamicRenderScaling::FHeuristicSettings& HeuristicSettings = Budget.GetSettings();
				if (Budget == GDynamicPrimaryResolutionFraction || !HeuristicSettings.IsEnabled())
				{
					continue;
				}

				float ResolutionFraction = DynamicResolutionStateInfos.ResolutionFractionApproximations[Budget];
				float ScreenPercentage = ResolutionFraction * 100.0f;

				FString DisplayName = Budget.GetName();
				DisplayName.ReplaceInline(TEXT("Dynamic"), TEXT("Dyn"));
				DisplayName.ReplaceInline(TEXT("Resolution"), TEXT("Res"));

				FColor Color = (ResolutionFraction < AlertResolutionFraction) ? StatRed : ((ResolutionFraction < FMath::Min(ResolutionFraction * 0.97f, 1.0f)) ? StatOrange : StatGreen);

				DrawTitleString(*DisplayName, /* UnitGraphColor = */ FColor::White);
				if (HeuristicSettings.Model == DynamicRenderScaling::EHeuristicModel::Quadratic)
				{
					InCanvas->DrawShadowedString(X2, InY, *FString::Printf(TEXT("%3.1f%% x %3.1f%%"), ScreenPercentage, ScreenPercentage), Font, Color);
				}
				else
				{
					InCanvas->DrawShadowedString(X2, InY, *FString::Printf(TEXT("%3.1f%%"), ScreenPercentage), Font, Color);
				}
				InY += RowHeight;
			}
		}

		// Draw calls
		{
			// Assume we don't have more than 1 GPU in mobile.
			int32 NumDrawCalls = GNumDrawCallsRHI[0];
			DrawTitleString(TEXT("Draws"), /* UnitGraphColor = */ FColor(100, 100, 255));
			InCanvas->DrawShadowedString(X2, InY, *FString::Printf(TEXT("%d"), NumDrawCalls), Font, StatGreen);
			InY += RowHeight;
		}
			
		// Primitives
		{
			// Assume we don't have more than 1 GPU in mobile.
			int32 NumPrimitives = GNumPrimitivesDrawnRHI[0];
			DrawTitleString(TEXT("Prims"), /* UnitGraphColor = */ FColor(100, 100, 255));
			if (NumPrimitives < 10000)
			{
				InCanvas->DrawShadowedString(X2, InY, *FString::Printf(TEXT("%d"), NumPrimitives), Font, StatGreen);
			}
			else
			{
				float NumPrimitivesK = NumPrimitives/1000.f;
				InCanvas->DrawShadowedString(X2, InY, *FString::Printf(TEXT("%.1fK"), NumPrimitivesK), Font, StatGreen);
			}
				
			InY += RowHeight;
		}
	}

#if !UE_BUILD_SHIPPING
	// Draw simple unit time graph
	if (bShowUnitTimeGraph)
	{
		const bool bSmallGraph = !CVarFullSizeUnitGraph.GetValueOnGameThread();

		UFont* SmallFont = GEngine->GetSmallFont();
		check(SmallFont);
		int32 AlertPrintWidth = SmallFont->GetStringSize(TEXT("000.0"));
		int32 AlertPrintHeight = SmallFont->GetStringHeightSize(TEXT("000.0"));

		// For each type of statistic that we want to graph (0=Render, 1=Game, 2=GPU, 3=Frame)
		enum EGraphStats
		{
			EGS_Render = 0,
			EGS_Game,
			EGS_GPU,
			EGS_Frame,
			EGS_RHIT,
			EGS_UnboundedHighValueCount,

			EGS_DynRes = EGS_UnboundedHighValueCount,
			EGS_Count
		};

		// The vertical axis is time in milliseconds
		// The horizontal axis is the frame number (NOT time!!!)

		// Threshold where graph lines will pulsate for slow frames
		extern TAutoConsoleVariable<float> GTargetFrameTimeThresholdCVar;
		const float TargetTimeMS = GTargetFrameTimeThresholdCVar.GetValueOnGameThread();

		const float AlertTimeMS = TargetTimeMS;

		// Graph layout
		const float GraphHeight = (bSmallGraph ? 120.0f : 350.0f);
#if PLATFORM_ANDROID || PLATFORM_IOS
		const float GraphLeftXPos = 20.0f;
		const float GraphBottomYPos = GraphHeight + 80.0f;
#else
		const float GraphLeftXPos = 80.0f;
		const float GraphBottomYPos = InCanvas->GetRenderTarget()->GetSizeXY().Y / InCanvas->GetDPIScale() - 50.0f;
#endif

		const float GraphBackgroundMarginSize = 8.0f;
		const float GraphHorizPixelsPerFrame = (bSmallGraph ? 1.0f : 2.0f);

		const float TargetTimeMSHeight = GraphHeight * 0.85f;
		const float	MaxDynresTargetTimeMSHeight = TargetTimeMSHeight * 0.75f;

		const float OutOfBudgetMarginHeight = (bSmallGraph ? 1 : 3);

		const float GraphTotalWidth = GraphHorizPixelsPerFrame * NumberOfSamples;
		const float GraphTotalHeight = TargetTimeMSHeight + (OutOfBudgetMarginHeight + (float)EGS_UnboundedHighValueCount) * AlertPrintHeight;

		// Scale MS axis so that TargetTimeMS stays at fixed ordinate.
		const float GraphVerticalPixelsPerMS = TargetTimeMSHeight / TargetTimeMS;

		// Scale dyn res so that RawMaxResolutionFraction is at MaxDynresTargetTimeMSHeight or below.
		const float GraphVerticalPixelsPerResolutionFraction = FMath::Min(100.0f, MaxDynresTargetTimeMSHeight / GDynamicPrimaryResolutionFraction.GetSettings().EstimateCostScale(DynamicResolutionStateInfos.ResolutionFractionUpperBounds[GDynamicPrimaryResolutionFraction]));

		// Compute pulse effect for lines above alert threshold
		const float AlertPulseFreq = 8.0f;
		const float AlertPulse = 0.5f + 0.5f * FMath::Sin((0.25f * UE_PI * 2.0) + (FApp::GetCurrentTime() * UE_PI * 2.0) * AlertPulseFreq);

		// Draw background.
		{
			FLinearColor BackgroundColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.7f);
			FCanvasTileItem BackgroundTile(
				FVector2D(GraphLeftXPos - GraphBackgroundMarginSize, GraphBottomYPos - GraphTotalHeight - GraphBackgroundMarginSize),
				FVector2D(GraphTotalWidth + 2 * GraphBackgroundMarginSize, GraphTotalHeight + 2 * GraphBackgroundMarginSize),
				BackgroundColor);

			BackgroundTile.BlendMode = SE_BLEND_AlphaBlend;

			InCanvas->DrawItem(BackgroundTile);
		}


		FBatchedElements* BatchedElements = InCanvas->GetBatchedElements(FCanvas::ET_Line);
		FHitProxyId HitProxyId = InCanvas->GetHitProxyId();

		// Reserve line vertices (2 border lines, 4 reference lines, then up to the maximum number of graph lines)
		BatchedElements->AddReserveLines(2 + 4 + EGS_Count * NumberOfSamples);

		// Draw timing graph frame.
		{
			const FLinearColor GraphBorderColor(0.1f, 0.1f, 0.1f);

			// Left
			BatchedElements->AddLine(
				FVector(GraphLeftXPos - 1.0f, GraphBottomYPos - GraphTotalHeight - GraphBackgroundMarginSize, 0.0f),
				FVector(GraphLeftXPos - 1.0f, GraphBottomYPos - 1.0f, 0.0f),
				GraphBorderColor,
				HitProxyId);

			// Bottom
			BatchedElements->AddLine(
				FVector(GraphLeftXPos - 1.0f, GraphBottomYPos - 1.0f, 0.0f),
				FVector(GraphLeftXPos + GraphHorizPixelsPerFrame * NumberOfSamples + GraphBackgroundMarginSize, GraphBottomYPos - 1.0f, 0.0f),
				GraphBorderColor,
				HitProxyId);

			InCanvas->DrawShadowedString(
				GraphLeftXPos - GraphBackgroundMarginSize,
				GraphBottomYPos - GraphTotalHeight - GraphBackgroundMarginSize - AlertPrintHeight - 2.0f,
				bShowRawUnitTimes ? TEXT("(Raw timings)") : TEXT("(Filtered timings)"), SmallFont, GraphBorderColor);
		}

		// Timing alert line
		{
			const FLinearColor LineColor(0.2f, 0.06f, 0.06f);
			FVector StartPos(
				GraphLeftXPos - 1.0f,
				GraphBottomYPos - AlertTimeMS * GraphVerticalPixelsPerMS,
				0.0f);
			FVector EndPos(
				GraphLeftXPos + GraphHorizPixelsPerFrame * NumberOfSamples + GraphBackgroundMarginSize,
				StartPos.Y,
				0.0f);

			BatchedElements->AddLine(
				StartPos,
				EndPos,
				LineColor,
				HitProxyId);

			InCanvas->DrawShadowedString(EndPos.X + 4.0f, EndPos.Y - AlertPrintHeight / 2, *FString::Printf(TEXT("%3.1f ms (budget)"), AlertTimeMS), SmallFont, LineColor);
		}

		// Screen percentage upper bound line
		{
			const FLinearColor LineColor(0.2f, 0.1f, 0.02f);
			FVector StartPos(
				GraphLeftXPos - 1.0f,
				GraphBottomYPos - GraphVerticalPixelsPerResolutionFraction * GDynamicPrimaryResolutionFraction.GetSettings().EstimateCostScale(DynamicResolutionStateInfos.ResolutionFractionUpperBounds[GDynamicPrimaryResolutionFraction]),
				0.0f);
			FVector EndPos(
				GraphLeftXPos + GraphHorizPixelsPerFrame * NumberOfSamples + GraphBackgroundMarginSize,
				StartPos.Y,
				0.0f);

			BatchedElements->AddLine(
				StartPos,
				EndPos,
				LineColor,
				HitProxyId);

			float MaxScreenPercentage = DynamicResolutionStateInfos.ResolutionFractionUpperBounds[GDynamicPrimaryResolutionFraction] * 100.0f;
			InCanvas->DrawShadowedString(
				EndPos.X + 4.0f,
				EndPos.Y - AlertPrintHeight / 2,
				*FString::Printf(TEXT("%3.1f%% x %3.1f%% (max)"), MaxScreenPercentage, MaxScreenPercentage), SmallFont, LineColor);
		}

		// Screen percentage = 100% native line
		if (DynamicResolutionStateInfos.ResolutionFractionUpperBounds[GDynamicPrimaryResolutionFraction] > 1.0f)
		{
			const FLinearColor LineColor(0.2f, 0.1f, 0.02f);
			FVector StartPos(
				GraphLeftXPos - 1.0f,
				GraphBottomYPos - GraphVerticalPixelsPerResolutionFraction,
				0.0f);
			FVector EndPos(
				GraphLeftXPos + GraphHorizPixelsPerFrame * NumberOfSamples + GraphBackgroundMarginSize,
				StartPos.Y,
				0.0f);

			BatchedElements->AddLine(
				StartPos,
				EndPos,
				LineColor,
				HitProxyId);

			if (GraphVerticalPixelsPerResolutionFraction * (GDynamicPrimaryResolutionFraction.GetSettings().EstimateCostScale(DynamicResolutionStateInfos.ResolutionFractionUpperBounds[GDynamicPrimaryResolutionFraction]) - 1.0f) >= AlertPrintHeight)
			{
				InCanvas->DrawShadowedString(EndPos.X + 4.0f, EndPos.Y - AlertPrintHeight / 2, TEXT("100.0% x 100.0% (native)"), SmallFont, LineColor);
			}
		}

		// Screen percentage = AlertResolutionFraction * 100 line
		{
			const FLinearColor LineColor(0.2f, 0.1f, 0.02f);
			FVector StartPos(
				GraphLeftXPos - 1.0f,
				GraphBottomYPos - GraphVerticalPixelsPerResolutionFraction * AlertResolutionFraction * AlertResolutionFraction,
				0.0f);
			FVector EndPos(
				GraphLeftXPos + GraphHorizPixelsPerFrame * NumberOfSamples + GraphBackgroundMarginSize,
				StartPos.Y,
				0.0f);

			BatchedElements->AddLine(
				StartPos,
				EndPos,
				LineColor,
				HitProxyId);

			float AlertScreenPercentage = AlertResolutionFraction * 100.0f;
			InCanvas->DrawShadowedString(EndPos.X + 4.0f, EndPos.Y - AlertPrintHeight / 2,
				*FString::Printf(TEXT("%3.1f%% x %3.1f%% (alert)"), AlertScreenPercentage, AlertScreenPercentage), SmallFont, LineColor);
		}

		int32 AlertPrintY = GraphBottomYPos - AlertTimeMS * GraphVerticalPixelsPerMS - OutOfBudgetMarginHeight * AlertPrintHeight;

		const bool bShowFrameTimeInUnitGraph = InViewport->GetClient() ? InViewport->GetClient()->IsStatEnabled(TEXT("UnitTime")) : false;
		for (int32 StatIndex = 0; StatIndex < EGS_Count; ++StatIndex)
		{
			int32 LastPrintX = 0xFFFFFFFF;
			AlertPrintY -= AlertPrintHeight;

			// If we don't have GPU data to display, then skip this line
			if ((StatIndex == EGS_GPU && !bHaveGPUData[0])
				|| (StatIndex == EGS_Frame && bShowFrameTimeInUnitGraph == false && bHaveGPUData[0])
				|| (StatIndex == EGS_RHIT && !IsRunningRHIInSeparateThread()))
			{
				continue;
			}

			FLinearColor StatColor;
			float* Values = NULL;
			float GraphVerticalPixelPerValue = 1.0f;
			float AbsoluteAlertValueThreshold = -1.0f;
			float RelativeAlertValueThreshold = -1.0f;
			int32 DisplayPow = 1;
			float DisplayMultiplier = 1.0f;
			bool HigherIsBest = false;
			if (StatIndex == EGS_Render)
			{
				AbsoluteAlertValueThreshold = AlertTimeMS;
				Values = RenderThreadTimes.GetData();
				GraphVerticalPixelPerValue = GraphVerticalPixelsPerMS;
				StatColor = FLinearColor(0.1f, 0.1f, 1.0f);		// Blue
			}
			else if (StatIndex == EGS_Game)
			{
				AbsoluteAlertValueThreshold = AlertTimeMS;
				Values = GameThreadTimes.GetData();
				GraphVerticalPixelPerValue = GraphVerticalPixelsPerMS;
				StatColor = FLinearColor(1.0f, 0.1f, 0.1f);		// Red
			}
			else if (StatIndex == EGS_GPU)
			{
				AbsoluteAlertValueThreshold = AlertTimeMS;
				// Multi-GPU support : We don't support more than 1 GPU in stat unitgraph yet.
				Values = GPUFrameTimes[0].GetData();
				GraphVerticalPixelPerValue = GraphVerticalPixelsPerMS;
				StatColor = FLinearColor(1.0f, 1.0f, 0.1f);		// Yellow
			}
			else if (StatIndex == EGS_Frame)
			{
				AbsoluteAlertValueThreshold = AlertTimeMS;
				Values = FrameTimes.GetData();
				GraphVerticalPixelPerValue = GraphVerticalPixelsPerMS;
				StatColor = FLinearColor(0.1f, 1.0f, 0.1f);		// Green
			}
			else if (StatIndex == EGS_RHIT)
			{
				AbsoluteAlertValueThreshold = AlertTimeMS;
				Values = RHITTimes.GetData();
				GraphVerticalPixelPerValue = GraphVerticalPixelsPerMS;
				StatColor = FLinearColor(1.0f, 0.1f, 1.0f);		// Green
			}
			else if (StatIndex == EGS_DynRes)
			{
				const DynamicRenderScaling::FBudget& Budget = GDynamicPrimaryResolutionFraction;

				AbsoluteAlertValueThreshold = AlertResolutionFraction;
				RelativeAlertValueThreshold = 0.05;
				Values = ResolutionFractions[GDynamicPrimaryResolutionFraction].GetData();
				GraphVerticalPixelPerValue = GraphVerticalPixelsPerResolutionFraction;
				StatColor = FLinearColor(1.0f, 0.5f, 0.1f);
				DisplayPow = Budget.GetSettings().Model == DynamicRenderScaling::EHeuristicModel::Quadratic ? 2 : 1;
				DisplayMultiplier = 100.0f;
				HigherIsBest = true;
				AlertPrintY = GraphBottomYPos - AlertResolutionFraction * AlertResolutionFraction * GraphVerticalPixelsPerResolutionFraction + AlertPrintHeight;
			}
			else
			{
				unimplemented();
			}


			// For each sample in our data set
			for (int32 CurFrameIndex = 0; CurFrameIndex < NumberOfSamples; ++CurFrameIndex)
			{
				const int32 PrevFrameIndex = FMath::Max(0, CurFrameIndex - 1);
				const int32 NextFrameIndex = FMath::Min(NumberOfSamples - 1, CurFrameIndex + 1);
				int32 PrevUnitIndex = (CurrentIndex + PrevFrameIndex) % NumberOfSamples;
				int32 CurUnitIndex = (CurrentIndex + CurFrameIndex) % NumberOfSamples;
				int32 NextUnitIndex = (CurrentIndex + NextFrameIndex) % NumberOfSamples;

				const float PrevValue = Values[PrevUnitIndex];
				const float CurValue = Values[CurUnitIndex];
				const float NextValue = Values[NextUnitIndex];

				if (CurValue < 0.0f || PrevValue < 0.0f)
				{
					continue;
				}

				const float MaxClampingY = GraphTotalHeight - 2 * StatIndex;

				const FVector LineStart(
					GraphLeftXPos + (float)PrevFrameIndex * GraphHorizPixelsPerFrame,
					GraphBottomYPos - FMath::Min(PrevValue * (DisplayPow == 2 ? PrevValue : 1.0f) * GraphVerticalPixelPerValue, MaxClampingY),
					0.0f);

				const FVector LineEnd(
					GraphLeftXPos + (float)CurFrameIndex * GraphHorizPixelsPerFrame,
					GraphBottomYPos - FMath::Min(CurValue * (DisplayPow == 2 ? CurValue : 1.0f) * GraphVerticalPixelPerValue, MaxClampingY),
					0.0f);

				BatchedElements->AddLine(LineStart, LineEnd, StatColor, HitProxyId);

				if (AbsoluteAlertValueThreshold < 0.0f)
				{
					continue;
				}

				// Absolute alert detection.
				bool Alert = (
					(!HigherIsBest && CurValue > AbsoluteAlertValueThreshold && (CurFrameIndex == 0 || PrevValue <= AbsoluteAlertValueThreshold)) ||
					(HigherIsBest && CurValue < AbsoluteAlertValueThreshold && (CurFrameIndex == 0 || PrevValue >= AbsoluteAlertValueThreshold)));
				float AlertValue = AbsoluteAlertValueThreshold;

				// If not absolute alert detection, look for relative alert.
				if (!Alert && RelativeAlertValueThreshold > 0.0f)
				{
					AlertValue = PrevValue * (1 - RelativeAlertValueThreshold);
					Alert = NextUnitIndex > 0 && (
						(!HigherIsBest && CurValue > AlertValue && CurValue >= NextValue) ||
						(HigherIsBest && CurValue < AlertValue && CurValue <= NextValue));
				}

				if (Alert)
				{
					const int32 AlertPadding = 1;
					float MaxValue = CurValue;
					int32 MinCheckFrames = FMath::Min<int32>(FPlatformMath::CeilToInt((float)AlertPrintWidth / GraphHorizPixelsPerFrame) + 10, NumberOfSamples);
					int32 CheckIndex = CurUnitIndex + 1;
					for (; CheckIndex < MinCheckFrames; ++CheckIndex)
					{
						MaxValue = HigherIsBest ? FMath::Min(MaxValue, Values[CheckIndex]) : FMath::Max(MaxValue, Values[CheckIndex]);
					}
					for (; CheckIndex < NumberOfSamples; ++CheckIndex)
					{
						if ((!HigherIsBest && Values[CheckIndex] <= AlertValue) ||
							(HigherIsBest && Values[CheckIndex] >= AlertValue))
						{
							break;
						}
						MaxValue = HigherIsBest ? FMath::Min(MaxValue, Values[CheckIndex]) : FMath::Max(MaxValue, Values[CheckIndex]);
					}

					int32 StartX = GraphLeftXPos + (float)PrevFrameIndex * GraphHorizPixelsPerFrame - AlertPrintWidth;
					if (StartX > LastPrintX)
					{

						InCanvas->DrawShadowedString(
							StartX,
							AlertValue != AbsoluteAlertValueThreshold ? LineEnd.Y : AlertPrintY,
							*FString::Printf(TEXT("%3.1f"), CurValue * DisplayMultiplier), SmallFont, StatColor);
						LastPrintX = StartX + AlertPrintWidth + AlertPadding;
					}
				}
			}
		}
	}
#endif	// !UE_BUILD_SHIPPING

	return InY;
}

int32 FStatHitchesData::DrawStat(FViewport* InViewport, FCanvas* InCanvas, int32 InX, int32 InY)
{
	const double CurrentTime = FPlatformTime::Seconds();
	if (LastTime > 0)
	{
		const float HitchThresholdSecs = FEnginePerformanceTargets::GetHitchFrameTimeThresholdMS() * 0.001f;

		const float DeltaSeconds = CurrentTime - LastTime;
		if (DeltaSeconds > HitchThresholdSecs)
		{
			Hitches[OverwriteIndex] = DeltaSeconds;
			When[OverwriteIndex] = CurrentTime;
			OverwriteIndex = (OverwriteIndex + 1) % NumHitches;

			UE_LOG(LogClient, Warning, TEXT("HITCH %d              running cnt = %5d"), int32(DeltaSeconds * 1000), Count++);
		}

		const int32 MaxY = InCanvas->GetRenderTarget()->GetSizeXY().Y;
		static const double TravelTime = 4.2;
		for (int32 i = 0; i < NumHitches; i++)
		{
			if (When[i] > 0 && When[i] <= CurrentTime && When[i] >= CurrentTime - TravelTime)
			{
				const float MyHitchSecs = Hitches[i];
				const float MyHitchMS = MyHitchSecs * 1000.0f;

				// Scale the time before passing in so that hitches aren't all red
				const FColor MyColor = GEngine->GetFrameTimeDisplayColor(MyHitchMS * 0.25f);

				const int32 MyY = InY + int32(float(MaxY - InY) * float((CurrentTime - When[i]) / TravelTime));
				const FString Hitch = FString::Printf(TEXT("%5d"), int32(MyHitchMS));
				InCanvas->DrawShadowedString(InX, MyY, *Hitch, GEngine->GetSmallFont(), MyColor);
			}
		}
	}
	LastTime = CurrentTime;
	return InY;
}


/*=============================================================================
//
// FViewport implementation.
//
=============================================================================*/

/** Send when a viewport is resized */
FViewport::FOnViewportResized FViewport::ViewportResizedEvent;

FViewport::FViewport(FViewportClient* InViewportClient):
	ViewportClient(InViewportClient),
	InitialPositionX(0),
	InitialPositionY(0),
	SizeX(0),
	SizeY(0),
	WindowMode(IsRunningGame() ? GSystemResolution.WindowMode : EWindowMode::Windowed),
	bHitProxiesCached(false),
	bHasRequestedToggleFreeze(false),
	bIsSlateViewport(false),
	bIsHDR(false),
	ViewportType(NAME_None),
	bTakeHighResScreenShot(false)
{
	//initialize the hit proxy kernel
	HitProxySize = 5;
	if (GIsEditor) 
	{
		GConfig->GetInt( TEXT("UnrealEd.HitProxy"), TEXT("HitProxySize"), (int32&)HitProxySize, GEditorIni );
		HitProxySize = FMath::Clamp( HitProxySize, (uint32)1, (uint32)MAX_HITPROXYSIZE );
	}

	// Cache the viewport client's hit proxy storage requirement.
	bRequiresHitProxyStorage = ViewportClient && ViewportClient->RequiresHitProxyStorage();
#if !WITH_EDITOR && !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if ( bRequiresHitProxyStorage )
	{
		UE_LOG(LogClient, Warning, TEXT("Consoles don't need hitproxy storage - wasting memory!?") );
	}
#endif

	AppVersionString = FString::Printf( TEXT( "Build: %s" ), FApp::GetBuildVersion() );

	bIsPlayInEditorViewport = false;
}

FViewport::~FViewport()
{
}

bool FViewport::TakeHighResScreenShot()
{
	if( GScreenshotResolutionX == 0 && GScreenshotResolutionY == 0 )
	{
		GScreenshotResolutionX = SizeX * GetHighResScreenshotConfig().ResolutionMultiplier;
		GScreenshotResolutionY = SizeY * GetHighResScreenshotConfig().ResolutionMultiplier;
	}

	uint32 MaxTextureDimension = GetMax2DTextureDimension();

	// Check that we can actually create a destination texture of this size
	if (GScreenshotResolutionX > MaxTextureDimension || GScreenshotResolutionY > MaxTextureDimension)
	{
		// Send a notification to tell the user the screenshot has failed
		auto Message = NSLOCTEXT("UnrealClient", "HighResScreenshotTooBig", "The high resolution screenshot multiplier is too large for your system. Please try again with a smaller value!");
		FNotificationInfo Info(Message);
		Info.bFireAndForget = true;
		Info.ExpireDuration = 5.0f;
		Info.bUseSuccessFailIcons = false;
		Info.bUseLargeFont = false;
		FSlateNotificationManager::Get().AddNotification(Info); 

		UE_LOG(LogClient, Warning, TEXT("The specified multiplier for high resolution screenshot is too large for your system (requested size %ux%u, max size %ux%u)! Please try again with a smaller value."), GScreenshotResolutionX, GScreenshotResolutionY, MaxTextureDimension, MaxTextureDimension);

		GIsHighResScreenshot = false;
		return false;
	}
	else
	{
		// Everything is OK. Take the shot.
		bTakeHighResScreenShot = true;

		//Force a redraw.
		Invalidate();	

		return true;
	}
}

static void HighResScreenshotBeginFrame(FDummyViewport* DummyViewport)
{
	GFrameCounter++;
	ENQUEUE_RENDER_COMMAND(BeginFrameCommand)(
		[DummyViewport, CurrentFrameCounter = GFrameCounter](FRHICommandListImmediate& RHICmdList)
	{
		GFrameCounterRenderThread = CurrentFrameCounter;
		GFrameNumberRenderThread++;
		GPU_STATS_BEGINFRAME(RHICmdList);
		RHICmdList.BeginFrame();
		FCoreDelegates::OnBeginFrameRT.Broadcast();
		if (DummyViewport)
		{
			DummyViewport->BeginRenderFrame(RHICmdList);
		}
	});
}

static void HighResScreenshotEndFrame(FDummyViewport* DummyViewport)
{
	ENQUEUE_RENDER_COMMAND(EndFrameCommand)(
		[DummyViewport](FRHICommandListImmediate& RHICmdList)
	{
		if (DummyViewport)
		{
			DummyViewport->EndRenderFrame(RHICmdList, false, false);
		}
		FCoreDelegates::OnEndFrameRT.Broadcast();
		RHICmdList.EndFrame();
		GPU_STATS_ENDFRAME(RHICmdList);
	});
}

void FViewport::HighResScreenshot()
{
	if (!ViewportClient->GetEngineShowFlags())
	{
		return;
	}

	// We need to cache this as FScreenshotRequest is a global and the filename is
	// cleared out before we use it below
	const FString CachedScreenshotName = FScreenshotRequest::GetFilename();

	FDummyViewport* DummyViewport = new FDummyViewport(ViewportClient);

	DummyViewport->SizeX = (GScreenshotResolutionX > 0) ? GScreenshotResolutionX : SizeX;
	DummyViewport->SizeY = (GScreenshotResolutionY > 0) ? GScreenshotResolutionY : SizeY;

	BeginInitResource(DummyViewport);

	bool MaskShowFlagBackup = ViewportClient->GetEngineShowFlags()->HighResScreenshotMask;
	const auto MotionBlurShowFlagBackup = ViewportClient->GetEngineShowFlags()->MotionBlur;

	ViewportClient->GetEngineShowFlags()->SetHighResScreenshotMask(GetHighResScreenshotConfig().bMaskEnabled);
	ViewportClient->GetEngineShowFlags()->SetMotionBlur(false);

	// Forcing 128-bit rendering pipeline
	static IConsoleVariable* SceneColorFormatVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.SceneColorFormat"));
	static IConsoleVariable* PostColorFormatVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.PostProcessingColorFormat"));
	static IConsoleVariable* ForceLODVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ForceLOD"));

	check(SceneColorFormatVar && PostColorFormatVar);
	const int32 OldSceneColorFormat = SceneColorFormatVar->GetInt();
	const int32 OldPostColorFormat = PostColorFormatVar->GetInt();
	const int32 OldForceLOD = ForceLODVar ? ForceLODVar->GetInt() : -1;

	if (GetHighResScreenshotConfig().bForce128BitRendering)
	{
		SceneColorFormatVar->Set(5, ECVF_SetByCode);
		PostColorFormatVar->Set(1, ECVF_SetByCode);
	}

	if (ForceLODVar)
	{
		// Force highest LOD
		ForceLODVar->Set(0, ECVF_SetByCode);
	}

	// Render the requested number of frames (at least once)
	static const auto HighResScreenshotDelay = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.HighResScreenshotDelay"));
	const uint32 DefaultScreenshotDelay = 4;
	uint32 FrameDelay = HighResScreenshotDelay ? FMath::Max(HighResScreenshotDelay->GetValueOnGameThread(), 1) : DefaultScreenshotDelay;

	// End the frame that was started before HighResScreenshot() was called. Pass nullptr for the viewport because there's no need to
	// call EndRenderFrame on the real viewport, as BeginRenderFrame hasn't been called yet.
	HighResScreenshotEndFrame(nullptr);

	// Perform run-up.
	while (FrameDelay)
	{
		HighResScreenshotBeginFrame(DummyViewport);

		FCanvas Canvas(DummyViewport, NULL, ViewportClient->GetWorld(), ViewportClient->GetWorld()->FeatureLevel);
		{
			ViewportClient->Draw(DummyViewport, &Canvas);
		}
		Canvas.Flush_GameThread();

		// Draw the debug canvas
		DummyViewport->GetDebugCanvas()->SetAllowedModes(FCanvas::Allow_DeleteOnRender);
		DummyViewport->GetDebugCanvas()->Flush_GameThread(true);

		HighResScreenshotEndFrame(DummyViewport);
		
		FlushRenderingCommands();

		--FrameDelay;
	}

	ViewportClient->GetEngineShowFlags()->SetHighResScreenshotMask(MaskShowFlagBackup);
	ViewportClient->GetEngineShowFlags()->MotionBlur = MotionBlurShowFlagBackup;
	bool bIsScreenshotSaved = ViewportClient->ProcessScreenShots(DummyViewport);

	SceneColorFormatVar->Set(OldSceneColorFormat, ECVF_SetByCode);
	PostColorFormatVar->Set(OldPostColorFormat, ECVF_SetByCode);
	if (ForceLODVar)
	{
		ForceLODVar->Set(OldForceLOD, ECVF_SetByCode);
	}

	BeginReleaseResource(DummyViewport);
	FlushRenderingCommands();
	delete DummyViewport;

	// once the screenshot is done we disable the feature to get only one frame
	GIsHighResScreenshot = false;
	bTakeHighResScreenShot = false;

	// Notification of a successful screenshot
	if ((GIsEditor || !IsFullscreen()) && !GIsAutomationTesting && bIsScreenshotSaved)
	{
		auto Message = NSLOCTEXT("UnrealClient", "HighResScreenshotSavedAs", "High resolution screenshot saved as");
		FNotificationInfo Info(Message);
		Info.bFireAndForget = true;
		Info.ExpireDuration = 5.0f;
		Info.bUseSuccessFailIcons = false;
		Info.bUseLargeFont = false;
		
		const FString HyperLinkText = FPaths::ConvertRelativePathToFull(CachedScreenshotName);
		Info.Hyperlink = FSimpleDelegate::CreateStatic([](FString SourceFilePath) 
		{
			FPlatformProcess::ExploreFolder(*(FPaths::GetPath(SourceFilePath)));
		}, HyperLinkText);
		Info.HyperlinkText = FText::FromString(HyperLinkText);
		
		FSlateNotificationManager::Get().AddNotification(Info);
		UE_LOG(LogClient, Log, TEXT("%s %s"), *Message.ToString(), *HyperLinkText);
	}

	// Start a new frame for the real viewport. Same as above, pass nullptr because BeginRenderFrame will be called
	// after this function returns.
	HighResScreenshotBeginFrame(nullptr);
}

struct FEndDrawingCommandParams
{
	FViewport* Viewport;
	uint32 bLockToVsync : 1;
	uint32 bShouldTriggerTimerEvent : 1;
	uint32 bShouldPresent : 1;
};

/**
 * Helper function used in ENQUEUE_RENDER_COMMAND below. Needed to be split out due to
 * use of macro and former already being one.
 *
 * @param Parameters	Parameters passed from the gamethread to the renderthread command.
 */
static void ViewportEndDrawing(FRHICommandListImmediate& RHICmdList, FEndDrawingCommandParams Parameters)
{	
	GInputLatencyTimer.RenderThreadTrigger = Parameters.bShouldTriggerTimerEvent;
	Parameters.Viewport->EndRenderFrame(RHICmdList, Parameters.bShouldPresent, Parameters.bLockToVsync);
}

/** Starts a new rendering frame. Called from the rendering thread. */
void FViewport::BeginRenderFrame(FRHICommandListImmediate& RHICmdList)
{
	check( IsInRenderingThread() );
	RHICmdList.BeginDrawingViewport(GetViewportRHI(), FTextureRHIRef());
	UpdateRenderTargetSurfaceRHIToCurrentBackBuffer();
}

/**
 *	Ends a rendering frame. Called from the rendering thread.
 *	@param bPresent		Whether the frame should be presented to the screen
 *	@param bLockToVsync	Whether the GPU should block until VSYNC before presenting
 */
void FViewport::EndRenderFrame(FRHICommandListImmediate& RHICmdList, bool bPresent, bool bLockToVsync)
{
	check( IsInRenderingThread() );

	RHICmdList.EnqueueLambda([CurrentFrameCounter = GFrameCounterRenderThread](FRHICommandListImmediate& InRHICmdList)
	{
		GEngine->SetPresentLatencyMarkerStart(CurrentFrameCounter);
	});

	uint32 StartTime = FPlatformTime::Cycles();
	RHICmdList.EndDrawingViewport(GetViewportRHI(), bPresent, bLockToVsync);
	uint32 EndTime = FPlatformTime::Cycles();

	RHICmdList.EnqueueLambda([CurrentFrameCounter = GFrameCounterRenderThread](FRHICommandListImmediate& InRHICmdList)
	{
		GEngine->SetPresentLatencyMarkerEnd(CurrentFrameCounter);
	});

	GRenderThreadIdle[ERenderThreadIdleTypes::WaitingForGPUPresent] += EndTime - StartTime;
	GRenderThreadNumIdle[ERenderThreadIdleTypes::WaitingForGPUPresent]++;
}

FRHIGPUMask FViewport::GetGPUMask(FRHICommandListImmediate& RHICmdList) const
{
	return FRHIGPUMask::FromIndex(RHICmdList.GetViewportNextPresentGPUIndex(GetViewportRHI()));
}

void APostProcessVolume::PostUnregisterAllComponents()
{
	// Route clear to super first.
	Super::PostUnregisterAllComponents();
	// World will be NULL during exit purge.
	if (GetWorld())
	{
		GetWorld()->RemovePostProcessVolume(this);
		GetWorld()->PostProcessVolumes.RemoveSingle(this);
	}
}

void APostProcessVolume::PostRegisterAllComponents()
{
	// Route update to super first.
	Super::PostRegisterAllComponents();
	GetWorld()->InsertPostProcessVolume(this);
}

void UPostProcessComponent::OnRegister()
{
	Super::OnRegister();
	GetWorld()->InsertPostProcessVolume(this);
}

void UPostProcessComponent::OnUnregister()
{
	Super::OnUnregister();
	GetWorld()->RemovePostProcessVolume(this);
}

void UPostProcessComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if(Ar.IsLoading())
	{
#if WITH_EDITORONLY_DATA
		Settings.OnAfterLoad();
#endif
	}
}

/**
*	Starts a new rendering frame. Called from the game thread thread.
*/
void FViewport::EnqueueBeginRenderFrame(const bool bShouldPresent)
{
	FViewport* Viewport = this;
	ENQUEUE_RENDER_COMMAND(BeginDrawingCommand)(
		[Viewport](FRHICommandListImmediate& RHICmdList)
		{
			Viewport->BeginRenderFrame(RHICmdList);
		});
}


void FViewport::EnqueueEndRenderFrame(const bool bLockToVsync, const bool bShouldPresent)
{
	FEndDrawingCommandParams Params = { this, (uint32)bLockToVsync, (uint32)GInputLatencyTimer.GameThreadTrigger, (uint32)(PresentAndStopMovieDelay > 0 ? 0 : bShouldPresent) };
	ENQUEUE_RENDER_COMMAND(EndDrawingCommand)(
		[Params](FRHICommandListImmediate& RHICmdList)
		{
			ViewportEndDrawing(RHICmdList, Params);
		});
}

void FViewport::Draw( bool bShouldPresent /*= true */)
{
	SCOPED_NAMED_EVENT(FViewport_Draw, FColor::Red);
	UWorld* World = GetClient()->GetWorld();

	// Ignore reentrant draw calls, since we can only redraw one viewport at a time.
	static bool bReentrant = false;
	if(!bReentrant)
	{
		// See what screenshot related features are required
		static const auto CVarDumpFrames = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.BufferVisualizationDumpFrames"));
		GIsHighResScreenshot = GIsHighResScreenshot || bTakeHighResScreenShot;
		bool bAnyScreenshotsRequired = FScreenshotRequest::IsScreenshotRequested() || GIsHighResScreenshot || GIsDumpingMovie;
		bool bBufferVisualizationDumpingRequired = bAnyScreenshotsRequired && CVarDumpFrames && CVarDumpFrames->GetValueOnGameThread();

		// if this is a game viewport, and game rendering is disabled, then we don't want to actually draw anything
		if ( World && World->IsGameWorld() && !bIsGameRenderingEnabled)
		{
			// since we aren't drawing the viewport, we still need to update streaming
			World->UpdateLevelStreaming();
		}
		else
		{
			if( GIsHighResScreenshot )
			{
				const bool bShowUI = false;
				const bool bAddFilenameSuffix = GetHighResScreenshotConfig().FilenameOverride.IsEmpty();
				FScreenshotRequest::RequestScreenshot( FString(), bShowUI, bAddFilenameSuffix );
				HighResScreenshot();
			}
			else if(bAnyScreenshotsRequired && bBufferVisualizationDumpingRequired)
			{
				// request the screenshot early so we have the name setup that BufferVisualization can dump it's content
				const bool bShowUI = false;
				const bool bAddFilenameSuffix = true;
				FScreenshotRequest::RequestScreenshot( FString(), bShowUI, bAddFilenameSuffix );
			}
	
			if( SizeX > 0 && SizeY > 0 )
			{
				static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.VSync"));
				bool bLockToVsync = CVar->GetValueOnGameThread() != 0;
				ULocalPlayer* Player = (GEngine && World) ? GEngine->GetFirstGamePlayer(World) : NULL;
				if ( Player )
				{
					bLockToVsync |= (Player && Player->PlayerController && Player->PlayerController->bCinematicMode);
				}
	 			EnqueueBeginRenderFrame(bShouldPresent);

				// Calculate gamethread time (excluding idle time)
				{
					static uint64 LastFrameUpdated = MAX_uint64;
					if (GFrameCounter != LastFrameUpdated)
					{
						static uint32 Lastimestamp = 0;
						static bool bStarted = false;
						uint32 CurrentTime	= FPlatformTime::Cycles();
						FThreadIdleStats& GameThread = FThreadIdleStats::Get();
						if (bStarted)
						{
							uint32 ThreadTime	= CurrentTime - Lastimestamp;
							// add any stalls via sleep or fevent
							GGameThreadTime		= (ThreadTime > GameThread.Waits) ? (ThreadTime - GameThread.Waits) : ThreadTime;
						}
						else
						{
							bStarted = true;
						}

						LastFrameUpdated = GFrameCounter;
						Lastimestamp		= CurrentTime;
						GameThread.Reset();
					}
				}

				UWorld* ViewportWorld = ViewportClient->GetWorld();
				FCanvas Canvas(this, nullptr, ViewportWorld, ViewportWorld ? ViewportWorld->FeatureLevel.GetValue() : GMaxRHIFeatureLevel, FCanvas::CDM_DeferDrawing, ViewportClient->ShouldDPIScaleSceneCanvas() ? ViewportClient->GetDPIScale() : 1.0f);
				Canvas.SetRenderTargetRect(FIntRect(0, 0, SizeX, SizeY));
				{
					ViewportClient->Draw(this, &Canvas);
				}
				Canvas.Flush_GameThread();
				
				UGameViewportClient::OnViewportRendered().Broadcast(this);
				
				ViewportClient->ProcessScreenShots(this);
	
				// Slate doesn't present immediately. Tag the viewport as requiring vsync so that it happens.
				SetRequiresVsync(bLockToVsync);
				EnqueueEndRenderFrame(bLockToVsync, bShouldPresent);

				GInputLatencyTimer.GameThreadTrigger = false;
			}
		}

		// Reset the camera cut flags if we are in a viewport that has a world
		if (World)
		{
			for( FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator )
			{
				APlayerController* PlayerController = Iterator->Get();
				if (PlayerController && PlayerController->PlayerCameraManager)
				{
					PlayerController->PlayerCameraManager->bGameCameraCutThisFrame = false;
				}
			}
		}

		// countdown the present delay, and then stop the movie at the end
		// this doesn't need to be on rendering thread as long as we have a long enough delay (2 or 3 frames), because
		// the rendering thread will never be more than one frame behind
		if (PresentAndStopMovieDelay > 0)
		{
			PresentAndStopMovieDelay--;
			// stop any playing movie
			if (PresentAndStopMovieDelay == 0)
			{
				// Enable game rendering again if it isn't already.
				bIsGameRenderingEnabled = true;
			}
		}
	}
}


void FViewport::InvalidateHitProxy()
{
	bHitProxiesCached = false;
	HitProxyMap.Invalidate();
	
	FCanvas* DebugCanvas = GetDebugCanvas();
	if (DebugCanvas)
	{
		DebugCanvas->SetHitProxy(nullptr);
	}
}



void FViewport::Invalidate()
{
	DeferInvalidateHitProxy();
	InvalidateDisplay();
}


void FViewport::DeferInvalidateHitProxy()
{
	// Default implementation does not defer.  Overridden implementations may.
	InvalidateHitProxy();
}

const TArray<FColor>& FViewport::GetRawHitProxyData(FIntRect InRect)
{
	FScopedConditionalWorldSwitcher WorldSwitcher(ViewportClient);

	const bool bIsRenderingStereo = GEngine->IsStereoscopic3D( this );

	bool bFetchHitProxyBytes = !bIsRenderingStereo && ( !bHitProxiesCached || (SizeY*SizeX) != CachedHitProxyData.Num() );

	if (bIsRenderingStereo)
	{
		// Stereo viewports don't support hit proxies, and we don't want to update them because it will adversely
		// affect performance.
		CachedHitProxyData.SetNumZeroed( SizeY * SizeX );
	}
	// If the hit proxy map isn't up to date, render the viewport client's hit proxies to it.
	else if (!bHitProxiesCached)
	{
		EnqueueBeginRenderFrame(false);

		FViewport* Viewport = this;
		ENQUEUE_RENDER_COMMAND(BeginDrawingCommandHitProxy)(
			[Viewport](FRHICommandListImmediate& RHICmdList)
			{
				// Set the hit proxy map's render target.
				// Clear the hit proxy map to white, which is overloaded to mean no hit proxy.
				FRHITexture* RenderTarget = Viewport->HitProxyMap.GetRenderTargetTexture();
				RHICmdList.Transition(FRHITransitionInfo(RenderTarget, ERHIAccess::Unknown, ERHIAccess::RTV));
				FRHIRenderPassInfo RPInfo(RenderTarget, ERenderTargetActions::Clear_Store);
				RHICmdList.BeginRenderPass(RPInfo, TEXT("ClearHitProxyMap"));
				RHICmdList.EndRenderPass();
			});

		// Let the viewport client draw its hit proxies.
		FCanvas Canvas(&HitProxyMap, &HitProxyMap, ViewportClient->GetWorld(), GetFeatureLevel(), FCanvas::CDM_DeferDrawing, ViewportClient->ShouldDPIScaleSceneCanvas() ? ViewportClient->GetDPIScale() : 1.0f);
		{
			ViewportClient->Draw(this, &Canvas);
		}
		Canvas.Flush_GameThread();

		//Resolve surface to texture.
		FHitProxyMap* HitProxyMapPtr = &HitProxyMap;
		ENQUEUE_RENDER_COMMAND(UpdateHitProxyRTCommand)(
			[HitProxyMapPtr](FRHICommandListImmediate& RHICmdList)
			{
				TransitionAndCopyTexture(RHICmdList, HitProxyMapPtr->GetRenderTargetTexture(), HitProxyMapPtr->GetHitProxyCPUTexture(), {});
			});

		ENQUEUE_RENDER_COMMAND(EndDrawingCommand)(
			[Viewport](FRHICommandListImmediate& RHICmdList)
			{
				Viewport->EndRenderFrame(RHICmdList, false, false);
			});

		// Cache the hit proxies for the next GetHitProxyMap call.
		bHitProxiesCached = true;
	}

	if (bFetchHitProxyBytes)
	{
		// Read the hit proxy map surface data back.
		FIntRect ViewportRect(0, 0, SizeX, SizeY);
		struct FReadSurfaceContext
		{
			FViewport* Viewport;
			TArray<FColor>* OutData;
			FIntRect Rect;
		};
		FReadSurfaceContext Context =
		{
			this,
			&CachedHitProxyData,
			ViewportRect
		};

		ENQUEUE_RENDER_COMMAND(ReadSurfaceCommand)(
			[Context](FRHICommandListImmediate& RHICmdList)
			{
				RHICmdList.ReadSurfaceData(
				Context.Viewport->HitProxyMap.GetHitProxyCPUTexture(),
				Context.Rect,
				*Context.OutData,
				FReadSurfaceDataFlags()
				);
			});
		FlushRenderingCommands();
	}

	return CachedHitProxyData;
}

void FViewport::GetHitProxyMap(FIntRect InRect,TArray<HHitProxy*>& OutMap)
{
	const TArray<FColor>& CachedData = GetRawHitProxyData(InRect);
	if (CachedData.Num()==0)
	{
		return;
	}
	
	// Map the hit proxy map surface data to hit proxies.
	OutMap.Empty(InRect.Width() * InRect.Height());
	for(int32 Y = InRect.Min.Y; Y < InRect.Max.Y; Y++)
	{
		if (!CachedData.IsValidIndex(int32(Y * SizeX)))
		{
			break;
		}

		const FColor* SourceData = &CachedData[Y * SizeX];
		for(int32 X = InRect.Min.X ;X < InRect.Max.X; X++)
		{
			FHitProxyId HitProxyId(SourceData[X]);
			OutMap.Add(GetHitProxyById(HitProxyId));
		}
	}
}

HHitProxy* FViewport::GetHitProxy(int32 X,int32 Y)
{
	// Compute a HitProxySize x HitProxySize test region with the center at (X,Y).
	int32	MinX = X - HitProxySize,
			MinY = Y - HitProxySize,
			MaxX = X + HitProxySize,
			MaxY = Y + HitProxySize;

	FIntPoint VPSize = GetSizeXY();
	
	// Clip the region to the viewport bounds.
	MinX = FMath::Clamp(MinX, 0, VPSize.X - 1);
	MinY = FMath::Clamp(MinY, 0, VPSize.Y - 1);
	MaxX = FMath::Clamp(MaxX, 0, VPSize.X - 1);
	MaxY = FMath::Clamp(MaxY, 0, VPSize.Y - 1);

	int32 TestSizeX	= MaxX - MinX + 1;
	int32 TestSizeY	= MaxY - MinY + 1;
	HHitProxy* HitProxy = NULL;

	if(TestSizeX > 0 && TestSizeY > 0)
	{
		// Read the hit proxy map from the device.
		TArray<HHitProxy*>	ProxyMap;
		GetHitProxyMap(FIntRect(MinX, MinY, MaxX + 1, MaxY + 1),ProxyMap);
		check(ProxyMap.Num() == TestSizeX * TestSizeY);

		// Find the hit proxy in the test region with the highest order.
		int32 ProxyIndex = TestSizeY/2 * TestSizeX + TestSizeX/2;
		check(ProxyIndex<ProxyMap.Num());
		HitProxy = ProxyMap[ProxyIndex];
	
		bool bIsOrtho = GetClient()->IsOrtho();

		for(int32 TestY = 0;TestY < TestSizeY;TestY++)
		{
			for(int32 TestX = 0;TestX < TestSizeX;TestX++)
			{
				HHitProxy* TestProxy = ProxyMap[TestY * TestSizeX + TestX];
				if(TestProxy && (!HitProxy || (bIsOrtho ? TestProxy->OrthoPriority : TestProxy->Priority) > (bIsOrtho ? HitProxy->OrthoPriority : HitProxy->Priority)))
				{
					HitProxy = TestProxy;
				}
			}
		}
	}

	return HitProxy;
}

void FViewport::GetActorsAndModelsInHitProxy(FIntRect InRect, TSet<AActor*>& OutActors, TSet<UModel*>& OutModels)
{
	OutActors.Empty();
	OutModels.Empty();

	EnumerateHitProxiesInRect(InRect, [&OutActors, &OutModels](HHitProxy* HitProxy)
			{
				if( HitProxy->IsA(HActor::StaticGetType()) )
				{
					AActor* Actor = ((HActor*)HitProxy)->Actor;
					if (Actor)
					{
						OutActors.Add(Actor);
					}
				}
				else if( HitProxy->IsA(HModel::StaticGetType()) )
				{
					OutModels.Add( ((HModel*)HitProxy)->GetModel() );
				}
				else if( HitProxy->IsA(HBSPBrushVert::StaticGetType()) )
				{
					HBSPBrushVert* HitBSPBrushVert = ((HBSPBrushVert*)HitProxy);
					if( HitBSPBrushVert->Brush.IsValid() )
					{
						OutActors.Add( HitBSPBrushVert->Brush.Get() );
					}
				}
		return true;
	});
}

FTypedElementHandle FViewport::GetElementHandleAtPoint(int32 X, int32 Y)
{
	if (HHitProxy* HitProxy = GetHitProxy(X, Y))
	{
		return HitProxy->GetElementHandle();
	}
	return FTypedElementHandle();
}

void FViewport::GetElementHandlesInRect(FIntRect InRect, FTypedElementListRef OutElementHandles)
{
	OutElementHandles->Reset();

	EnumerateHitProxiesInRect(InRect, [&OutElementHandles](HHitProxy* HitProxy)
	{
		if (FTypedElementHandle ElementHandle = HitProxy->GetElementHandle())
		{
			OutElementHandles->Add(MoveTemp(ElementHandle));
		}
		return true;
	});
}

void FViewport::EnumerateHitProxiesInRect(FIntRect InRect, TFunctionRef<bool(HHitProxy*)> InCallback)
{
	const TArray<FColor>& RawHitProxyData = GetRawHitProxyData(InRect);

	// Lower the resolution with massive box selects
	const int32 Step = (InRect.Width() > 500 && InRect.Height() > 500) ? 4 : 1;

	for (int32 Y = InRect.Min.Y; Y < InRect.Max.Y; Y = Y < InRect.Max.Y-1 ? FMath::Min(InRect.Max.Y-1, Y+Step) : ++Y)
	{
		const FColor* SourceData = &RawHitProxyData[Y * SizeX];
		for (int32 X = InRect.Min.X; X < InRect.Max.X; X = X < InRect.Max.X-1 ? FMath::Min(InRect.Max.X-1, X+Step) : ++X)
		{
			FHitProxyId HitProxyId(SourceData[X]);
			HHitProxy* HitProxy = GetHitProxyById(HitProxyId);

			if (HitProxy && !InCallback(HitProxy))
			{
				return;
			}
		}
	}
}

void FViewport::UpdateViewportRHI(bool bDestroyed, uint32 NewSizeX, uint32 NewSizeY, EWindowMode::Type NewWindowMode, EPixelFormat PreferredPixelFormat)
{
	{
		// Temporarily stop rendering thread.
		FlushRenderingCommands();

		// Update the viewport attributes.
		// This is done AFTER the command flush done by UpdateViewportRHI, to avoid disrupting rendering thread accesses to the old viewport size.
		SizeX = NewSizeX;
		SizeY = NewSizeY;
		WindowMode = NewWindowMode;

		// Release the viewport's resources.
		BeginReleaseResource(this);

		// Don't reinitialize the viewport RHI if the viewport has been destroyed.
		if(bDestroyed)
		{
			if(IsValidRef(ViewportRHI))
			{
				// If the viewport RHI has already been initialized, release it.
				ViewportRHI.SafeRelease();
			}
		}
		else
		{
			if(IsValidRef(ViewportRHI))
			{
				// If the viewport RHI has already been initialized, resize it.
				RHIResizeViewport(
					ViewportRHI,
					SizeX,
					SizeY,
					IsFullscreen(),
					PreferredPixelFormat
					);
			}
			else
			{
				// Initialize the viewport RHI with the new viewport state.
				ViewportRHI = RHICreateViewport(
					GetWindow(),
					SizeX,
					SizeY,
					IsFullscreen(),
					EPixelFormat::PF_Unknown /* ie, use default format */);
			}
		
			// Initialize the viewport's resources.
			BeginInitResource(this);
		}
	}

	if ( !bDestroyed )
	{
		// send a notification that the viewport has been resized
		ViewportResizedEvent.Broadcast(this, 0);
	}
}

FIntRect FViewport::CalculateViewExtents(float AspectRatio, const FIntRect& ViewRect)
{
	FIntRect Result = ViewRect;

	const float CurrentSizeX = ViewRect.Width();
	const float CurrentSizeY = ViewRect.Height();

	// the viewport's SizeX/SizeY may not always match the GetDesiredAspectRatio(), so adjust the requested AspectRatio to compensate
	const float AdjustedAspectRatio = AspectRatio / (GetDesiredAspectRatio() / ((float)GetSizeXY().X / (float)GetSizeXY().Y));

	// If desired, enforce a particular aspect ratio for the render of the scene. 
	// Results in black bars at top/bottom etc.
	const float AspectRatioDifference = AdjustedAspectRatio - (CurrentSizeX / CurrentSizeY);

	if( FMath::Abs( AspectRatioDifference ) > 0.01f )
	{
		// If desired aspect ratio is bigger than current - we need black bars on top and bottom.
		if( AspectRatioDifference > 0.0f )
		{
			// Calculate desired Y size.
			const int32 NewSizeY = FMath::Max(1, FMath::RoundToInt( CurrentSizeX / AdjustedAspectRatio ) );
			Result.Min.Y = FMath::RoundToInt( 0.5f * (CurrentSizeY - NewSizeY) );
			Result.Max.Y = Result.Min.Y + NewSizeY;
			Result.Min.Y += ViewRect.Min.Y;
			Result.Max.Y += ViewRect.Min.Y;
		}
		// Otherwise - will place bars on the sides.
		else
		{
			const int32 NewSizeX = FMath::Max(1, FMath::RoundToInt( CurrentSizeY * AdjustedAspectRatio ) );
			Result.Min.X = FMath::RoundToInt( 0.5f * (CurrentSizeX - NewSizeX) );
			Result.Max.X = Result.Min.X + NewSizeX;
			Result.Min.X += ViewRect.Min.X;
			Result.Max.X += ViewRect.Min.X;
		}
	}

	return Result;
}

/**
 *	Sets a viewport client if one wasn't provided at construction time.
 *	@param InViewportClient	- The viewport client to set.
 **/
void FViewport::SetViewportClient( FViewportClient* InViewportClient )
{
	ViewportClient = InViewportClient;
}

void FViewport::InitDynamicRHI()
{
	UpdateRenderTargetSurfaceRHIToCurrentBackBuffer();

	if(bRequiresHitProxyStorage)
	{
		// Initialize the hit proxy map.
		HitProxyMap.Init(SizeX,SizeY);
	}
}

void FViewport::ReleaseDynamicRHI()
{
	HitProxyMap.Release();
	RenderTargetTextureRHI.SafeRelease();
}

void FViewport::ReleaseRHI()
{
	FlushRenderingCommands();
	ViewportRHI.SafeRelease();
}

void FViewport::InitRHI()
{
	FlushRenderingCommands();
	if(!IsValidRef(ViewportRHI))
	{
		ViewportRHI = RHICreateViewport(
			GetWindow(),
			SizeX,
			SizeY,
			IsFullscreen(),
			EPixelFormat::PF_Unknown
			);
		UpdateRenderTargetSurfaceRHIToCurrentBackBuffer();
	}
}

ENGINE_API bool IsCtrlDown(FViewport* Viewport) { return (Viewport->KeyState(EKeys::LeftControl) || Viewport->KeyState(EKeys::RightControl)); }
ENGINE_API bool IsShiftDown(FViewport* Viewport) { return (Viewport->KeyState(EKeys::LeftShift) || Viewport->KeyState(EKeys::RightShift)); }
ENGINE_API bool IsAltDown(FViewport* Viewport) { return (Viewport->KeyState(EKeys::LeftAlt) || Viewport->KeyState(EKeys::RightAlt)); }


/** Constructor */
FViewport::FHitProxyMap::FHitProxyMap()
{
}


/** Destructor */
FViewport::FHitProxyMap::~FHitProxyMap()
{
}


void FViewport::FHitProxyMap::Init(uint32 NewSizeX,uint32 NewSizeY)
{
	SizeX = NewSizeX;
	SizeY = NewSizeY;

	// Create a render target to store the hit proxy map.
	{
		const FRHITextureCreateDesc Desc =
			FRHITextureCreateDesc::Create2D(TEXT("HitProxyTexture"))
			.SetExtent(SizeX, SizeY)
			.SetFormat(PF_B8G8R8A8)
			.SetFlags(ETextureCreateFlags::RenderTargetable | ETextureCreateFlags::ShaderResource)
			.SetClearValue(FClearValueBinding::White)
			.SetInitialState(ERHIAccess::SRVMask);

		RenderTargetTextureRHI = RHICreateTexture(Desc);
	}
	{
		const FRHITextureCreateDesc Desc =
			FRHITextureCreateDesc::Create2D(TEXT("HitProxyCPUTexture"))
			.SetExtent(SizeX, SizeY)
			.SetFormat(PF_B8G8R8A8)
			.SetFlags(ETextureCreateFlags::CPUReadback);

		HitProxyCPUTexture = RHICreateTexture(Desc);
	}
}

void FViewport::FHitProxyMap::Release()
{
	HitProxyCPUTexture.SafeRelease();
	RenderTargetTextureRHI.SafeRelease();
}

void FViewport::FHitProxyMap::Invalidate()
{
	HitProxies.Empty();
}

void FViewport::FHitProxyMap::AddHitProxy(HHitProxy* HitProxy)
{
	HitProxies.Add(HitProxy);
}


/** FGCObject: add UObject references to GC */
void FViewport::FHitProxyMap::AddReferencedObjects( FReferenceCollector& Collector )
{
	// Allow all of our hit proxy objects to serialize their references
	for( int32 CurProxyIndex = 0; CurProxyIndex < HitProxies.Num(); ++CurProxyIndex )
	{
		HHitProxy* CurProxy = HitProxies[ CurProxyIndex ];
		if( CurProxy != NULL )
		{
			CurProxy->AddReferencedObjects( Collector );
		}
	}
}

FString FViewport::FHitProxyMap::GetReferencerName() const
{
	return TEXT("FViewport::FHitProxyMap");
}

/**
 * Globally enables/disables rendering
 *
 * @param bIsEnabled true if drawing should occur
 * @param PresentAndStopMovieDelay Number of frames to delay before enabling bPresent in RHIEndDrawingViewport, and before stopping the movie
 */
void FViewport::SetGameRenderingEnabled(bool bIsEnabled, int32 InPresentAndStopMovieDelay)
{
	bIsGameRenderingEnabled = bIsEnabled;
	PresentAndStopMovieDelay = InPresentAndStopMovieDelay;
}

/**
 * Handles freezing/unfreezing of rendering
 */
void FViewport::ProcessToggleFreezeCommand()
{
	bHasRequestedToggleFreeze = 1;
}

/**
 * Returns if there is a command to toggle freezing
 */
bool FViewport::HasToggleFreezeCommand()
{
	// save the current command
	bool ReturnVal = bHasRequestedToggleFreeze;
	
	// make sure that we no longer have the command, as we are now passing off "ownership"
	// of the command
	bHasRequestedToggleFreeze = false;

	// return what it was
	return ReturnVal;
}

/**
 * Update the render target surface RHI to the current back buffer 
 */
void FViewport::UpdateRenderTargetSurfaceRHIToCurrentBackBuffer()
{
	if(IsValidRef(ViewportRHI))
	{
		RenderTargetTextureRHI = RHIGetViewportBackBuffer(ViewportRHI);
	}
}

void FViewport::SetInitialSize( FIntPoint InitialSizeXY )
{
	// Initial size only works if the viewport has not yet been resized
	if( GetSizeXY() == FIntPoint::ZeroValue )
	{
		UpdateViewportRHI( false, InitialSizeXY.X, InitialSizeXY.Y, EWindowMode::Windowed, PF_Unknown );
	}
}


ENGINE_API bool GetViewportScreenShot(FViewport* Viewport, TArray<FColor>& Bitmap, const FIntRect& ViewRect /*= FIntRect()*/)
{
	// Read the contents of the viewport into an array.
	if (Viewport->ReadPixels(Bitmap, FReadSurfaceDataFlags(), ViewRect))
	{
		check(Bitmap.Num() == ViewRect.Area() || (Bitmap.Num() == Viewport->GetSizeXY().X * Viewport->GetSizeXY().Y));
		return true;
	}

	return false;
}

extern bool ParseResolution( const TCHAR* InResolution, uint32& OutX, uint32& OutY, int32& WindowMode );

ENGINE_API bool GetHighResScreenShotInput(const TCHAR* Cmd, FOutputDevice& Ar, uint32& OutXRes, uint32& OutYRes, float& OutResMult, FIntRect& OutCaptureRegion, bool& OutShouldEnableMask, bool& OutDumpBufferVisualizationTargets, bool& OutCaptureHDR, FString& OutFilenameOverride, bool& OutUseDateTimeAsFileName)
{
	TArray<FString> Arguments;
	const FString FilenameSearchString = TEXT("filename=");

	// FParse::Value has better handling of escape characters than FParse::Token
	if (!FParse::Value(Cmd, *FilenameSearchString, OutFilenameOverride))
	{
		OutFilenameOverride.Reset();
	}

	FString Arg;
	while (FParse::Token(Cmd, Arg, true))
	{
		// Now skip filename since we already processed it
		if (!Arg.StartsWith(FilenameSearchString))
		{
			Arguments.Add(Arg);
		}
	}

	int32 NumArguments = Arguments.Num();

	if (NumArguments >= 1)
	{
		int32 WindowModeDummy;
		if( !ParseResolution( *Arguments[0], OutXRes, OutYRes, WindowModeDummy ) )
		{
			//If Cmd is valid and it's not a resolution then the input must be a multiplier.
			float Mult = FCString::Atof(*Arguments[0]);

			if( Mult > 0.0f && Arguments[0].IsNumeric() )
			{
				OutResMult = Mult;
			}
			else
			{
				Ar.Logf(TEXT("Error: Bad input. Input should be in either the form \"HighResShot 1920x1080\" or \"HighResShot 2\""));
				return false;
			}
		}
		else if( OutXRes <= 0 || OutYRes <= 0  )
		{
			Ar.Logf(TEXT("Error: Values must be greater than 0 in both dimensions"));
			return false;
		}
		else if( OutXRes > GetMax2DTextureDimension() || OutYRes > GetMax2DTextureDimension()  )
		{
			Ar.Logf(TEXT("Error: Screenshot size exceeds the maximum allowed texture size (%d x %d)"), GetMax2DTextureDimension(), GetMax2DTextureDimension());
			return false;
		}

		// Try and extract capture region from string
		int32 CaptureRegionX = NumArguments > 1 ? FCString::Atoi(*Arguments[1]) : 0;
		int32 CaptureRegionY = NumArguments > 2 ? FCString::Atoi(*Arguments[2]) : 0;
		int32 CaptureRegionWidth = NumArguments > 3 ? FCString::Atoi(*Arguments[3]) : 0;
		int32 CaptureRegionHeight = NumArguments > 4 ? FCString::Atoi(*Arguments[4]) : 0;
		OutCaptureRegion = FIntRect(CaptureRegionX, CaptureRegionY, CaptureRegionX + CaptureRegionWidth, CaptureRegionY + CaptureRegionHeight);

		OutShouldEnableMask = NumArguments > 5 ? FCString::Atoi(*Arguments[5]) != 0 : false;
		OutDumpBufferVisualizationTargets = NumArguments > 6 ? FCString::Atoi(*Arguments[6]) != 0 : false;
		OutCaptureHDR = NumArguments > 7 ? FCString::Atoi(*Arguments[7]) != 0 : false;
		OutUseDateTimeAsFileName = NumArguments > 8 ? FCString::Atoi(*Arguments[8]) != 0 : false;


		return true;
	}
	else
	{
		Ar.Logf(TEXT("Error: Bad input. Input should be in either the form \"HighResShot 1920x1080\" or \"HighResShot 2\""));
	}

	return false;
}

/** Tracks the viewport client that should process the stat command, can be NULL */
FCommonViewportClient* GStatProcessingViewportClient = NULL;


float FCommonViewportClient::GetDPIScale() const
{
	if (bShouldUpdateDPIScale)
	{
		CachedDPIScale = UpdateViewportClientWindowDPIScale();

		bShouldUpdateDPIScale = false;
	}

	return CachedDPIScale;
}

void FCommonViewportClient::DrawHighResScreenshotCaptureRegion(FCanvas& Canvas)
{
	const FLinearColor BoxColor = FLinearColor::Red;
	FHighResScreenshotConfig& Config = GetHighResScreenshotConfig();

	FCanvasLineItem LineItem;

	// Draw the line a line in X and Y extending out from the center.
	LineItem.SetColor( BoxColor );
	LineItem.Draw( &Canvas, FVector2D(Config.UnscaledCaptureRegion.Min.X, Config.UnscaledCaptureRegion.Min.Y), FVector2D(Config.UnscaledCaptureRegion.Max.X, Config.UnscaledCaptureRegion.Min.Y) );
	LineItem.Draw( &Canvas, FVector2D(Config.UnscaledCaptureRegion.Max.X, Config.UnscaledCaptureRegion.Min.Y), FVector2D(Config.UnscaledCaptureRegion.Max.X, Config.UnscaledCaptureRegion.Max.Y));
	LineItem.Draw( &Canvas, FVector2D(Config.UnscaledCaptureRegion.Max.X, Config.UnscaledCaptureRegion.Max.Y), FVector2D(Config.UnscaledCaptureRegion.Min.X, Config.UnscaledCaptureRegion.Max.Y));
	LineItem.Draw( &Canvas, FVector2D(Config.UnscaledCaptureRegion.Min.X, Config.UnscaledCaptureRegion.Max.Y), FVector2D(Config.UnscaledCaptureRegion.Min.X, Config.UnscaledCaptureRegion.Min.Y));
}

void FCommonViewportClient::RequestUpdateDPIScale()
{
	bShouldUpdateDPIScale = true;
}

float FCommonViewportClient::GetDPIDerivedResolutionFraction() const
{
	#if WITH_EDITOR
	if (GIsEditor)
	{
		// When in high res screenshot do not modify screen percentage based on dpi scale
		if (GIsHighResScreenshot)
		{
			return 1.0f;
		}

		static auto CVarEditorViewportHighDPIPtr = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Editor.Viewport.HighDPI"));

		if (CVarEditorViewportHighDPIPtr && CVarEditorViewportHighDPIPtr->GetInt() == 0)
		{
			return FMath::Min(1.0f / GetDPIScale(), 1.0f);
		}
	}
	#endif

	return 1.0f;
}

/**
 * FDummyViewport
 */

FDummyViewport::FDummyViewport(FViewportClient* InViewportClient)
	: FViewport(InViewportClient)
	, DebugCanvas(NULL)
{
	ViewportType = NAME_DummyViewport;
	UWorld* CurWorld = (InViewportClient != NULL ? InViewportClient->GetWorld() : NULL);
	DebugCanvas = new FCanvas(this, NULL, CurWorld, (CurWorld != NULL ? CurWorld->FeatureLevel.GetValue() : GMaxRHIFeatureLevel));
		
	DebugCanvas->SetAllowedModes(0);
}

FDummyViewport::~FDummyViewport()
{
	if (DebugCanvas != NULL)
	{
		delete DebugCanvas;
		DebugCanvas = NULL;
	}
}

void FDummyViewport::InitDynamicRHI()
{
	const FRHITextureCreateDesc Desc =
		FRHITextureCreateDesc::Create2D(TEXT("FDummyViewport"))
		.SetExtent(SizeX, SizeY)
		.SetFormat(PF_A2B10G10R10)
		.SetFlags(ETextureCreateFlags::RenderTargetable | ETextureCreateFlags::ShaderResource)
		.SetInitialState(ERHIAccess::SRVMask);

	RenderTargetTextureRHI = RHICreateTexture(Desc);
}
