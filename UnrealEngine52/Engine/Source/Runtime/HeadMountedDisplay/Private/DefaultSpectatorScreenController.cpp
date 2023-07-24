// Copyright Epic Games, Inc. All Rights Reserved.

#include "DefaultSpectatorScreenController.h"
#include "HeadMountedDisplayTypes.h"
#include "PipelineStateCache.h"
#include "ClearQuad.h"
#include "ScreenRendering.h"
#include "TextureResource.h"
#include "GlobalRenderResources.h"
#include "Misc/CoreDelegates.h"
#include "Engine/Texture.h"
#include "HeadMountedDisplayBase.h"
#include "SceneUtils.h" // for SCOPED_DRAW_EVENT()
#include "IStereoLayers.h"


FDefaultSpectatorScreenController::FDefaultSpectatorScreenController(FHeadMountedDisplayBase* InHMDDevice)
	: HMDDevice(InHMDDevice)
{
}


ESpectatorScreenMode FDefaultSpectatorScreenController::GetSpectatorScreenMode() const
{
	if (IsInRenderingThread())
	{
		return SpectatorScreenMode_RenderThread;
	}
	else
	{
		FScopeLock Lock(&NewSpectatorScreenModeLock);
		return NewSpectatorScreenMode;
	}
}

void FDefaultSpectatorScreenController::SetSpectatorScreenMode(ESpectatorScreenMode Mode)
{
	UE_LOG(LogHMD, Log, TEXT("SetSpectatorScreenMode(%i)."), static_cast<int32>(Mode));

	FScopeLock FrameLock(&NewSpectatorScreenModeLock);
	NewSpectatorScreenMode = Mode;
}

void FDefaultSpectatorScreenController::SetSpectatorScreenTexture(UTexture* SrcTexture)
{
	SpectatorScreenTexture = SrcTexture;
}

UTexture* FDefaultSpectatorScreenController::GetSpectatorScreenTexture() const
{
	if (SpectatorScreenTexture.IsValid())
	{
		return SpectatorScreenTexture.Get();
	}
	return nullptr;
}

void FDefaultSpectatorScreenController::SetSpectatorScreenModeTexturePlusEyeLayout(const FSpectatorScreenModeTexturePlusEyeLayout& Layout)
{
	if (Layout.IsValid())
	{
		SetSpectatorScreenModeTexturePlusEyeLayoutRenderCommand(Layout);
	}
	else
	{
		UE_LOG(LogHMD, Warning, TEXT("SetSpectatorScreenModeTexturePlusEyeLayout called with invalid Layout.  Ignoring it.  See warnings above."))
	}
}

void FDefaultSpectatorScreenController::QueueDebugCanvasLayerID(int32 LayerID)
{
	DebugCanvasLayerIDs.Add(LayerID);
}

FSpectatorScreenRenderDelegate* FDefaultSpectatorScreenController::GetSpectatorScreenRenderDelegate_RenderThread()
{
	return &SpectatorScreenDelegate_RenderThread;
}

FRHICOMMAND_MACRO(FRHISetSpectatorScreenTexture)
{
	FDefaultSpectatorScreenController* SpectatorScreenController;
	FTexture2DRHIRef Texture;

	FORCEINLINE_DEBUGGABLE FRHISetSpectatorScreenTexture(FDefaultSpectatorScreenController* InSpectatorScreenController, const FTexture2DRHIRef& InTexture)
		: SpectatorScreenController(InSpectatorScreenController)
		, Texture(InTexture)
	{
	}

	HEADMOUNTEDDISPLAY_API void Execute(FRHICommandListBase& CmdList)
	{
		SpectatorScreenController->SetSpectatorScreenTexture_RenderThread(Texture);
	}
};

void FDefaultSpectatorScreenController::SetSpectatorScreenTextureRenderCommand(UTexture* SrcTexture)
{
	check(IsInGameThread());

	if (!SrcTexture)
	{
		return;
	}

	FTexture2DRHIRef Texture2DRHIRef;
	FTextureResource* TextureResource = SrcTexture->GetResource();
	if (TextureResource && TextureResource->TextureRHI)
	{
		Texture2DRHIRef = TextureResource->TextureRHI->GetTexture2D();
	}

	// setting the texture must be done on the thread that's executing RHI commandlists.
	FDefaultSpectatorScreenController* SpectatorScreenController = this;
	ENQUEUE_RENDER_COMMAND(SetSpectatorScreenTexture)(
		[SpectatorScreenController, Texture2DRHIRef](FRHICommandListImmediate& RHICmdList)
		{
			if (RHICmdList.Bypass())
			{
				FRHISetSpectatorScreenTexture Command(SpectatorScreenController, Texture2DRHIRef);
				Command.Execute(RHICmdList);
				return;
			}
			ALLOC_COMMAND_CL(RHICmdList, FRHISetSpectatorScreenTexture)(SpectatorScreenController, Texture2DRHIRef);
		}
	);
}

void FDefaultSpectatorScreenController::SetSpectatorScreenTexture_RenderThread(FTexture2DRHIRef& InTexture)
{
	SpectatorScreenTexture_RenderThread = InTexture;
}


FRHICOMMAND_MACRO(FRHISetSpectatorScreenModeTexturePlusEyeLayout)
{
	FDefaultSpectatorScreenController* SpectatorScreenController;
	FSpectatorScreenModeTexturePlusEyeLayout Layout;

	FORCEINLINE_DEBUGGABLE FRHISetSpectatorScreenModeTexturePlusEyeLayout(FDefaultSpectatorScreenController* InSpectatorScreenController, const FSpectatorScreenModeTexturePlusEyeLayout& InLayout)
		: SpectatorScreenController(InSpectatorScreenController)
		, Layout(InLayout)
	{
	}

	HEADMOUNTEDDISPLAY_API void Execute(FRHICommandListBase& CmdList)
	{
		SpectatorScreenController->SetSpectatorScreenModeTexturePlusEyeLayout_RenderThread(Layout);
	}
};

void FDefaultSpectatorScreenController::SetSpectatorScreenModeTexturePlusEyeLayoutRenderCommand(const FSpectatorScreenModeTexturePlusEyeLayout& NewLayout)
{
	check(IsInGameThread());

	// setting the layout must be done on the thread that's executing RHI commandlists.
	FDefaultSpectatorScreenController* SpectatorScreenController = this;
	ENQUEUE_RENDER_COMMAND(SetSpectatorScreenTexture)(
		[SpectatorScreenController, NewLayout](FRHICommandListImmediate& RHICmdList)
		{
			if (RHICmdList.Bypass())
			{
				FRHISetSpectatorScreenModeTexturePlusEyeLayout Command(SpectatorScreenController, NewLayout);
				Command.Execute(RHICmdList);
				return;
			}
			ALLOC_COMMAND_CL(RHICmdList, FRHISetSpectatorScreenModeTexturePlusEyeLayout)(SpectatorScreenController, NewLayout);
		}
	);
}

void FDefaultSpectatorScreenController::SetSpectatorScreenModeTexturePlusEyeLayout_RenderThread(const FSpectatorScreenModeTexturePlusEyeLayout& Layout)
{
	SpectatorScreenModeTexturePlusEyeLayout_RenderThread = Layout;
}

void FDefaultSpectatorScreenController::BeginRenderViewFamily()
{
	check(IsInGameThread());

	SetSpectatorScreenTextureRenderCommand(SpectatorScreenTexture.Get());
}

// It is imporant that this function be called early in the render frame, ie in PreRenderViewFamily_RenderThread so that
// SpectatorScreenMode_RenderThread is set before other render frame work is done.
void FDefaultSpectatorScreenController::UpdateSpectatorScreenMode_RenderThread()
{
	check(IsInRenderingThread());

	ESpectatorScreenMode NewMode;
	{
		FScopeLock FrameLock(&NewSpectatorScreenModeLock);
		NewMode = NewSpectatorScreenMode;
	}

	if (NewMode == SpectatorScreenMode_RenderThread)
	{
		return;
	}

	FSpectatorScreenRenderDelegate* RenderDelegate = GetSpectatorScreenRenderDelegate_RenderThread();
	check(RenderDelegate);

	RenderDelegate->Unbind();

	SpectatorScreenMode_RenderThread = NewMode;

	switch (NewMode)
	{
	case ESpectatorScreenMode::Disabled:
		break;
	case ESpectatorScreenMode::SingleEyeLetterboxed:
		RenderDelegate->BindRaw(this, &FDefaultSpectatorScreenController::RenderSpectatorModeSingleEyeLetterboxed);
		break;
	case ESpectatorScreenMode::Undistorted:
		RenderDelegate->BindRaw(this, &FDefaultSpectatorScreenController::RenderSpectatorModeUndistorted);
		break;
	case ESpectatorScreenMode::Distorted:
		RenderDelegate->BindRaw(this, &FDefaultSpectatorScreenController::RenderSpectatorModeDistorted);
		break;
	case ESpectatorScreenMode::SingleEye:
		RenderDelegate->BindRaw(this, &FDefaultSpectatorScreenController::RenderSpectatorModeSingleEye);
		break;
	case ESpectatorScreenMode::Texture:
		RenderDelegate->BindRaw(this, &FDefaultSpectatorScreenController::RenderSpectatorModeTexture);
		break;
	case ESpectatorScreenMode::TexturePlusEye:
		RenderDelegate->BindRaw(this, &FDefaultSpectatorScreenController::RenderSpectatorModeMirrorAndTexture);
		break;
	default:
		RenderDelegate->BindRaw(this, &FDefaultSpectatorScreenController::RenderSpectatorModeSingleEyeCroppedToFill);
		break;
	}
}

void FDefaultSpectatorScreenController::RenderSpectatorScreen_RenderThread(FRHICommandListImmediate& RHICmdList, FRHITexture2D* BackBuffer, FTexture2DRHIRef SrcTexture, FVector2D WindowSize)
{
	RenderSpectatorScreen_RenderThread(RHICmdList, BackBuffer, SrcTexture, nullptr, WindowSize);
}

void FDefaultSpectatorScreenController::RenderSpectatorScreen_RenderThread(FRHICommandListImmediate& RHICmdList, FRHITexture2D* BackBuffer, FTexture2DRHIRef SrcTexture, FTexture2DRHIRef LayersTexture, FVector2D WindowSize)
{
	SCOPED_NAMED_EVENT_TEXT("RenderSocialScreen_RenderThread()", FColor::Magenta);

	check(IsInRenderingThread());

	StereoLayersTexture = LayersTexture;

	if (SpectatorScreenDelegate_RenderThread.IsBound())
	{
		SCOPED_DRAW_EVENT(RHICmdList, SpectatorScreen)
		SpectatorScreenDelegate_RenderThread.Execute(RHICmdList, BackBuffer, SrcTexture, SpectatorScreenTexture_RenderThread, WindowSize);
	}

	// Apply the debug canvas layer.
	IStereoLayers* StereoLayers = HMDDevice->GetStereoLayers();
	if (StereoLayers && !LayersTexture)
	{
		const FIntRect DstRect(0, 0, BackBuffer->GetSizeX(), BackBuffer->GetSizeY());

		for (int32 LayerID : DebugCanvasLayerIDs)
		{
			FTextureRHIRef LayerTexture = nullptr, HMDNull = nullptr;
			StereoLayers->GetAllocatedTexture(LayerID, LayerTexture, HMDNull);
			if (LayerTexture)
			{
				FTexture2DRHIRef LayerTexture2D = LayerTexture->GetTexture2D();
				check(LayerTexture2D.IsValid());  // Debug canvas layer should be a 2d layer
				const FIntRect LayerRect(0, 0, LayerTexture2D->GetSizeX(), LayerTexture2D->GetSizeY());
				const FIntRect DstRectLetterboxed = Helpers::GetLetterboxedDestRect(LayerRect, DstRect);
				HMDDevice->CopyTexture_RenderThread(RHICmdList, LayerTexture2D, LayerRect, BackBuffer, DstRectLetterboxed, false, false);
			}
		}
		DebugCanvasLayerIDs.Empty();
	}
}

FIntRect FDefaultSpectatorScreenController::GetFullFlatEyeRect_RenderThread(FTexture2DRHIRef EyeTexture)
{
	return HMDDevice->GetFullFlatEyeRect_RenderThread(EyeTexture);
}

void FDefaultSpectatorScreenController::CopyEmulatedLayers(FRHICommandListImmediate& RHICmdList, FTexture2DRHIRef TargetTexture, const FIntRect SrcRect, const FIntRect DstRect)
{
	if (StereoLayersTexture)
	{
		HMDDevice->CopyTexture_RenderThread(RHICmdList, StereoLayersTexture, SrcRect, TargetTexture, DstRect, false, false); 
	}
	StereoLayersTexture = nullptr;
}

void FDefaultSpectatorScreenController::RenderSpectatorModeUndistorted(FRHICommandListImmediate& RHICmdList, FTexture2DRHIRef TargetTexture, FTexture2DRHIRef EyeTexture, FTexture2DRHIRef OtherTexture, FVector2D WindowSize)
{
	const FIntRect SrcRect(0, 0, EyeTexture->GetSizeX(), EyeTexture->GetSizeY());
	const FIntRect DstRect(0, 0, TargetTexture->GetSizeX(), TargetTexture->GetSizeY());

	HMDDevice->CopyTexture_RenderThread(RHICmdList, EyeTexture, SrcRect, TargetTexture, DstRect, false, true);
	CopyEmulatedLayers(RHICmdList, TargetTexture, SrcRect, DstRect);
}

void FDefaultSpectatorScreenController::RenderSpectatorModeDistorted(FRHICommandListImmediate& RHICmdList, FTexture2DRHIRef TargetTexture, FTexture2DRHIRef EyeTexture, FTexture2DRHIRef OtherTexture, FVector2D WindowSize)
{
	// Note distorted mode is supported on only on oculus
	// The default implementation falls back to RenderSpectatorModeSingleEyeCroppedToFill.
	RenderSpectatorModeSingleEyeCroppedToFill(RHICmdList, TargetTexture, EyeTexture, OtherTexture, WindowSize);
}

void FDefaultSpectatorScreenController::RenderSpectatorModeSingleEye(FRHICommandListImmediate& RHICmdList, FTexture2DRHIRef TargetTexture, FTexture2DRHIRef EyeTexture, FTexture2DRHIRef OtherTexture, FVector2D WindowSize)
{
	const FIntRect SrcRect(0, 0, EyeTexture->GetSizeX() / 2, EyeTexture->GetSizeY());
	const FIntRect DstRect(0, 0, TargetTexture->GetSizeX(), TargetTexture->GetSizeY());

	HMDDevice->CopyTexture_RenderThread(RHICmdList, EyeTexture, SrcRect, TargetTexture, DstRect, false, true);
	CopyEmulatedLayers(RHICmdList, TargetTexture, SrcRect, DstRect);
}

void FDefaultSpectatorScreenController::RenderSpectatorModeSingleEyeLetterboxed(FRHICommandListImmediate& RHICmdList, FTexture2DRHIRef TargetTexture, FTexture2DRHIRef EyeTexture, FTexture2DRHIRef OtherTexture, FVector2D WindowSize)
{
	const FIntRect SrcRect = GetFullFlatEyeRect_RenderThread(EyeTexture);
	const FIntRect DstRect(0, 0, TargetTexture->GetSizeX(), TargetTexture->GetSizeY());
	const FIntRect DstRectLetterboxed = Helpers::GetLetterboxedDestRect(SrcRect, DstRect);

	HMDDevice->CopyTexture_RenderThread(RHICmdList, EyeTexture, SrcRect, TargetTexture, DstRectLetterboxed, true, true);
	CopyEmulatedLayers(RHICmdList, TargetTexture, SrcRect, DstRectLetterboxed);
}

void FDefaultSpectatorScreenController::RenderSpectatorModeSingleEyeCroppedToFill(FRHICommandListImmediate& RHICmdList, FTexture2DRHIRef TargetTexture, FTexture2DRHIRef EyeTexture, FTexture2DRHIRef OtherTexture, FVector2D WindowSize)
{
	const FIntRect SrcRect = GetFullFlatEyeRect_RenderThread(EyeTexture);
	const FIntRect DstRect(0, 0, TargetTexture->GetSizeX(), TargetTexture->GetSizeY());
	const FIntRect WindowRect(0, 0, static_cast<int32>(WindowSize.X), static_cast<int32>(WindowSize.Y));

	const FIntRect SrcCroppedToFitRect = Helpers::GetEyeCroppedToFitRect(HMDDevice->GetEyeCenterPoint_RenderThread(EStereoscopicEye::eSSE_LEFT_EYE), SrcRect, WindowRect);

	HMDDevice->CopyTexture_RenderThread(RHICmdList, EyeTexture, SrcCroppedToFitRect, TargetTexture, DstRect, false, true);
	CopyEmulatedLayers(RHICmdList, TargetTexture, SrcCroppedToFitRect, DstRect);
}

void FDefaultSpectatorScreenController::RenderSpectatorModeTexture(FRHICommandListImmediate& RHICmdList, FTexture2DRHIRef TargetTexture, FTexture2DRHIRef EyeTexture, FTexture2DRHIRef OtherTexture, FVector2D WindowSize)
{
	FRHITexture2D* SrcTexture = OtherTexture;
	if (!SrcTexture)
	{
		SrcTexture = GetFallbackRHITexture();
	}

	const FIntRect SrcRect(0, 0, SrcTexture->GetSizeX(), SrcTexture->GetSizeY());
	const FIntRect DstRect(0, 0, TargetTexture->GetSizeX(), TargetTexture->GetSizeY());

	HMDDevice->CopyTexture_RenderThread(RHICmdList, SrcTexture, SrcRect, TargetTexture, DstRect, false, true);
}

void FDefaultSpectatorScreenController::RenderSpectatorModeMirrorAndTexture(FRHICommandListImmediate& RHICmdList, FTexture2DRHIRef TargetTexture, FTexture2DRHIRef EyeTexture, FTexture2DRHIRef OtherTexture, FVector2D WindowSize)
{
	FRHITexture2D* OtherTextureLocal = OtherTexture;
	if (!OtherTextureLocal)
	{
		OtherTextureLocal = GetFallbackRHITexture();
	}

	const FIntRect EyeDstRect = SpectatorScreenModeTexturePlusEyeLayout_RenderThread.GetScaledEyeRect(TargetTexture->GetSizeX(), TargetTexture->GetSizeY());
	const FIntRect EyeSrcRect = GetFullFlatEyeRect_RenderThread(EyeTexture);
	const FIntRect CroppedEyeSrcRect = Helpers::GetEyeCroppedToFitRect(HMDDevice->GetEyeCenterPoint_RenderThread(EStereoscopicEye::eSSE_LEFT_EYE), EyeSrcRect, EyeDstRect);

	const FIntRect OtherDstRect = SpectatorScreenModeTexturePlusEyeLayout_RenderThread.GetScaledTextureRect(TargetTexture->GetSizeX(), TargetTexture->GetSizeY());
	const FIntRect OtherSrcRect(0, 0, OtherTextureLocal->GetSizeX(), OtherTextureLocal->GetSizeY());

	const bool bClearBlack = SpectatorScreenModeTexturePlusEyeLayout_RenderThread.bClearBlack;

	if (SpectatorScreenModeTexturePlusEyeLayout_RenderThread.bDrawEyeFirst)
	{
		HMDDevice->CopyTexture_RenderThread(RHICmdList, EyeTexture, CroppedEyeSrcRect, TargetTexture, EyeDstRect, bClearBlack, true);
		CopyEmulatedLayers(RHICmdList, TargetTexture, CroppedEyeSrcRect, EyeDstRect);
		HMDDevice->CopyTexture_RenderThread(RHICmdList, OtherTextureLocal, OtherSrcRect, TargetTexture, OtherDstRect, false, !SpectatorScreenModeTexturePlusEyeLayout_RenderThread.bUseAlpha);
	}
	else
	{
		HMDDevice->CopyTexture_RenderThread(RHICmdList, OtherTextureLocal, OtherSrcRect, TargetTexture, OtherDstRect, bClearBlack, true);
		HMDDevice->CopyTexture_RenderThread(RHICmdList, EyeTexture, CroppedEyeSrcRect, TargetTexture, EyeDstRect, false, true);
		CopyEmulatedLayers(RHICmdList, TargetTexture, CroppedEyeSrcRect, EyeDstRect);
	}
}

FRHITexture2D* FDefaultSpectatorScreenController::GetFallbackRHITexture() const
{
	//return GWhiteTexture->TextureRHI->GetTexture2D();
	return GBlackTexture->TextureRHI->GetTexture2D();
}


FIntRect FDefaultSpectatorScreenController::Helpers::GetEyeCroppedToFitRect(FVector2D EyeCenterPoint, const FIntRect& SrcRect, const FIntRect& TargetRect)
{
	// Return a SubRect of EyeRect which has the same aspect ratio as TargetRect
	// such that drawing that SubRect of the eye texture into TargetRect of some other texture
	// will give a nice single eye cropped to fit view.

	// If EyeCenterPoint can be put in the center of the screen by shifting the crop up/down or left/right
	// shift it as far as we can without cropping further.  This means if we are cropping
	// vertically we can shift to a vertical center other than 0.5, and if we are cropping horizontally
	// we can shift to a horizontal center other than 0.5.

	// Eye rect is the subrect of the eye texture that we want to crop to fit TargetRect.
	// Eye rect should already have been cropped to only contain pixels we might want to show on TargetRect.
	// So it ought to be cropped to the reasonably flat-looking part of the rendered area.

	FIntRect OutRect = SrcRect;

	// Assuming neither rect is zero size in any dimension.
	check(SrcRect.Area() != 0);
	check(TargetRect.Area() != 0);

	const float SrcRectAspect = (float)SrcRect.Width() / (float)SrcRect.Height();
	const float TargetRectAspect = (float)TargetRect.Width() / (float)TargetRect.Height();

	if (SrcRectAspect < TargetRectAspect)
	{
		// Source is taller than destination
		// Crop top/bottom
		const float DesiredSrcHeight = SrcRect.Height() * (SrcRectAspect / TargetRectAspect);
		const int32 HalfHeightDiff = FMath::TruncToInt(((float)SrcRect.Height() - DesiredSrcHeight) * 0.5f);
		OutRect.Min.Y += HalfHeightDiff;
		OutRect.Max.Y -= HalfHeightDiff;
		const int32 DesiredCenterAdjustment = FMath::TruncToInt(((float)EyeCenterPoint.Y - 0.5f) * (float)SrcRect.Height());
		const int32 ActualCenterAdjustment = FMath::Clamp(DesiredCenterAdjustment, -HalfHeightDiff, HalfHeightDiff);
		OutRect.Min.Y += ActualCenterAdjustment;
		OutRect.Max.Y += ActualCenterAdjustment;
	}
	else
	{
		// Source is wider than destination
		// Crop left/right
		const float DesiredSrcWidth = SrcRect.Width() * (TargetRectAspect / SrcRectAspect);
		const int32 HalfWidthDiff = FMath::TruncToInt(((float)SrcRect.Width() - DesiredSrcWidth) * 0.5f);
		OutRect.Min.X += HalfWidthDiff;
		OutRect.Max.X -= HalfWidthDiff;
		const int32 DesiredCenterAdjustment = FMath::TruncToInt(((float)EyeCenterPoint.X - 0.5f) * (float)SrcRect.Width());
		const int32 ActualCenterAdjustment = FMath::Clamp(DesiredCenterAdjustment, -HalfWidthDiff, HalfWidthDiff);
		OutRect.Min.X += ActualCenterAdjustment;
		OutRect.Max.X += ActualCenterAdjustment;
	}

	return OutRect;
}

FIntRect FDefaultSpectatorScreenController::Helpers::GetLetterboxedDestRect(const FIntRect& SrcRect, const FIntRect& TargetRect)
{
	FIntRect OutRect = TargetRect;

	// Assuming neither rect is zero size in any dimension.
	check(SrcRect.Area() != 0);
	check(TargetRect.Area() != 0);

	const float SrcRectAspect = (float)SrcRect.Width() / (float)SrcRect.Height();
	const float TargetRectAspect = (float)TargetRect.Width() / (float)TargetRect.Height();

	if (SrcRectAspect < TargetRectAspect)
	{
		// Source is taller than destination
		// Column-boxing
		const float DesiredTgtWidth = TargetRect.Width() * (SrcRectAspect / TargetRectAspect);
		const int32 HalfWidthDiff = FMath::TruncToInt(((float)TargetRect.Width() - DesiredTgtWidth) * 0.5f);
		OutRect.Min.X += HalfWidthDiff;
		OutRect.Max.X -= HalfWidthDiff;
	}
	else
	{
		// Source is wider than destination
		// Letter-boxing
		const float DesiredTgtHeight = TargetRect.Height() * (TargetRectAspect / SrcRectAspect);
		const int32 HalfHeightDiff = FMath::TruncToInt(((float)TargetRect.Height() - DesiredTgtHeight) * 0.5f);
		OutRect.Min.Y += HalfHeightDiff;
		OutRect.Max.Y -= HalfHeightDiff;
	}

	return OutRect;
}
