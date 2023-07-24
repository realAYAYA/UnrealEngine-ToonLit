// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixelStreamingVideoInputBackBufferComposited.h"
#include "Settings.h"
#include "PixelStreamingEditorUtils.h"
#include "Utils.h"
#include "UtilsRender.h"
#include "ToStringExtensions.h"
#include "PixelStreamingPrivate.h"
#include "PixelCaptureInputFrameRHI.h"
#include "PixelCaptureCapturerRHI.h"
#include "PixelCaptureCapturerRHIToI420CPU.h"
#include "PixelCaptureCapturerRHIToI420Compute.h"
#include "PixelCaptureBufferFormat.h"
#include "Framework/Application/SlateApplication.h"
#include "GenericPlatform/GenericWindowDefinition.h"
#include "Application/SlateApplicationBase.h"
#include "Widgets/SWindow.h"

DECLARE_LOG_CATEGORY_EXTERN(LogPixelStreamingBackBufferComposited, Log, VeryVerbose);
DEFINE_LOG_CATEGORY(LogPixelStreamingBackBufferComposited);

TSharedPtr<FPixelStreamingVideoInputBackBufferComposited> FPixelStreamingVideoInputBackBufferComposited::Create()
{
	TSharedPtr<FPixelStreamingVideoInputBackBufferComposited> NewInput = TSharedPtr<FPixelStreamingVideoInputBackBufferComposited>(new FPixelStreamingVideoInputBackBufferComposited());
	TWeakPtr<FPixelStreamingVideoInputBackBufferComposited> WeakInput = NewInput;
	// Set up the callback on the game thread since FSlateApplication::Get() can only be used there
	UE::PixelStreaming::DoOnGameThread([WeakInput]() {
		if (TSharedPtr<FPixelStreamingVideoInputBackBufferComposited> Input = WeakInput.Pin())
		{
			FSlateRenderer* Renderer = FSlateApplication::Get().GetRenderer();
			Input->DelegateHandle = Renderer->OnBackBufferReadyToPresent().AddSP(Input.ToSharedRef(), &FPixelStreamingVideoInputBackBufferComposited::OnBackBufferReady);
		}
	});

	return NewInput;
}

FPixelStreamingVideoInputBackBufferComposited::FPixelStreamingVideoInputBackBufferComposited()
{
	SharedFrameRect = MakeShared<FIntRect>();
	UE::PixelStreaming::DoOnGameThread([this]() {
		FSlateApplication::Get().OnPreTick().AddLambda([this](float DeltaTime) {
			FScopeLock Lock(&TopLevelWindowsCriticalSection);
			TopLevelWindows.Empty();
			FSlateApplication::Get().GetAllVisibleWindowsOrdered(TopLevelWindows);
		});
	});
}

FPixelStreamingVideoInputBackBufferComposited::~FPixelStreamingVideoInputBackBufferComposited()
{
	if (!IsEngineExitRequested())
	{
		UE::PixelStreaming::DoOnGameThread([HandleCopy = DelegateHandle]() {
			FSlateApplication::Get().GetRenderer()->OnBackBufferReadyToPresent().Remove(HandleCopy);
		});
	}
}

void FPixelStreamingVideoInputBackBufferComposited::OnBackBufferReady(SWindow& SlateWindow, const FTexture2DRHIRef& FrameBuffer)
{
	{
		FScopeLock Lock(&TopLevelWindowsCriticalSection);
		if (TopLevelWindows.IsEmpty())
		{
			return;
		}
	}

	UE_LOG(LogPixelStreamingBackBufferComposited, Verbose, TEXT("=== Window Rendered ==="));
	UE_LOG(LogPixelStreamingBackBufferComposited, Verbose, TEXT("Type: %s"), UE::EditorPixelStreaming::ToString(SlateWindow.GetType()));

	FIntPoint FrameSize = FrameBuffer->GetSizeXY();
	FString Hash = UE::EditorPixelStreaming::HashWindow(SlateWindow, FrameBuffer);
	FTextureRHIRef StagingTexture = StagingTextures.FindRef(Hash);
	if (!StagingTexture.IsValid())
	{
		UE_LOG(LogPixelStreamingBackBufferComposited, Verbose, TEXT("Creating new staging texture: %dx%d"), FrameSize.X, FrameSize.Y);
		StagingTexture = UE::PixelStreaming::CreateRHITexture(FrameSize.X, FrameSize.Y);
		StagingTextures.Add(Hash, StagingTexture);
	}

	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
	// Re-render FrameBuffer to StagingTexture (ensure format match)
	UE::PixelStreaming::CopyTextureRDG(RHICmdList, FrameBuffer, StagingTexture);

	TopLevelWindowTextures.Add(&SlateWindow, StagingTexture);
	TopLevelWindowsCriticalSection.Lock();
	// Check that we have received a texture from every window in the TopLevelWindows array
	uint8 NumWindowsRendered = 0;
	for (TSharedRef<SWindow> Window : TopLevelWindows)
	{
		if (TopLevelWindowTextures.FindRef(&Window.Get()))
		{
			++NumWindowsRendered;
		}
	}

	if (NumWindowsRendered == TopLevelWindows.Num())
	{
		CompositeWindows();
	}
	TopLevelWindowsCriticalSection.Unlock();
}

void FPixelStreamingVideoInputBackBufferComposited::CompositeWindows()
{
	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();

	// Process all of the windows we will need to render. This processing step finds the extents of the
	// composited texture as well as the top-left most point
	FIntPoint TopLeft = FIntPoint(MAX_int32, MAX_int32);
	FIntPoint BottomRight = FIntPoint(MIN_int32, MIN_int32);
	for (uint8 i = 0; i < TopLevelWindows.Num(); i++)
	{
		TSharedRef<SWindow> CurrentWindow = TopLevelWindows[i];
		// Early out if we have an "invalid" window type
		if (CurrentWindow->GetOpacity() == 0.f || CurrentWindow->GetSizeInScreen() == FVector2f(0, 0))
		{
			continue;
		}

		FTextureRHIRef CurrentTexture = TopLevelWindowTextures.FindRef(&CurrentWindow.Get());
		FIntPoint TextureExtent = VectorMin(CurrentTexture->GetSizeXY(), CurrentWindow->GetSizeInScreen().IntPoint());

		FIntPoint WindowPosition = FIntPoint(CurrentWindow->GetPositionInScreen().X, CurrentWindow->GetPositionInScreen().Y);
		//
		TopLeft = VectorMin(TopLeft, WindowPosition);
		//
		BottomRight = VectorMax(BottomRight, (WindowPosition + TextureExtent));
	}

	FIntPoint Extent = BottomRight - TopLeft;
	FTextureRHIRef CompositedTexture = UE::PixelStreaming::CreateRHITexture(Extent.X, Extent.Y);
	for (uint8 i = 0; i < TopLevelWindows.Num(); i++)
	{
		TSharedRef<SWindow> CurrentWindow = TopLevelWindows[i];
		// Early out if we have an "invalid" window type
		if (CurrentWindow->GetOpacity() == 0.f || CurrentWindow->GetSizeInScreen() == FVector2f(0, 0))
		{
			continue;
		}
		FIntPoint WindowPosition = FIntPoint(CurrentWindow->GetPositionInScreen().X, CurrentWindow->GetPositionInScreen().Y) - TopLeft;

		FTextureRHIRef CurrentTexture = TopLevelWindowTextures.FindRef(&CurrentWindow.Get());
		// There is only ever one tooltip and as such UE keeps the same texture for each and just rerenders the content
		// this can lead to small tooltips having a large texture from a previously displayed long tooltip
		// so we use the tooltips window size which is guaranteed to be correct
		FIntPoint TextureExtent = VectorMin(CurrentTexture->GetSizeXY(), CurrentWindow->GetSizeInScreen().IntPoint());

		// Copy our new texture to the compositedframe texture
		FRHICopyTextureInfo CopyInfo;
		CopyInfo.Size.X = TextureExtent.X;
		CopyInfo.Size.Y = TextureExtent.Y;
		CopyInfo.Size.Z = 1;
		CopyInfo.DestPosition.X = WindowPosition.X;
		CopyInfo.DestPosition.Y = WindowPosition.Y;
		CopyInfo.DestPosition.Z = 0;
		// source and dest are the same. simple copy
		RHICmdList.Transition(FRHITransitionInfo(CurrentTexture, ERHIAccess::Unknown, ERHIAccess::CopySrc));
		RHICmdList.Transition(FRHITransitionInfo(CompositedTexture, ERHIAccess::Unknown, ERHIAccess::CopyDest));
		RHICmdList.CopyTexture(CurrentTexture, CompositedTexture, CopyInfo);
	}

	// Our composition is complete, send it to along the pipeline
	OnFrame(FPixelCaptureInputFrameRHI(CompositedTexture));
	// Update the default streamer to let it know our compositedframe size and position. This way it can correctly scale input from the browser
	*SharedFrameRect.Get() = FIntRect(TopLeft, BottomRight);
	OnFrameSizeChanged.Broadcast(SharedFrameRect);
}

FString FPixelStreamingVideoInputBackBufferComposited::ToString()
{
	return TEXT("the Editor");
}
