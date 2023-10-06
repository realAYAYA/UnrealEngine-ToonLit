// Copyright Epic Games, Inc. All Rights Reserved.
#include "PixelStreamingVideoInputRenderTarget.h"
#include "PixelStreamingVideoInputRHI.h"
#include "TextureResource.h"
#include "Engine/TextureRenderTarget2D.h"
#include "PixelCaptureInputFrameRHI.h"

TSharedPtr<FPixelStreamingVideoInputRenderTarget> FPixelStreamingVideoInputRenderTarget::Create(UTextureRenderTarget2D* Target)
{
	return TSharedPtr<FPixelStreamingVideoInputRenderTarget>(new FPixelStreamingVideoInputRenderTarget(Target));
}

FPixelStreamingVideoInputRenderTarget::~FPixelStreamingVideoInputRenderTarget()
{
	FCoreDelegates::OnEndFrameRT.Remove(DelegateHandle);
}

FPixelStreamingVideoInputRenderTarget::FPixelStreamingVideoInputRenderTarget(UTextureRenderTarget2D* InTarget)
	: Target(InTarget)
	, DelegateHandle(FCoreDelegates::OnEndFrameRT.AddRaw(this, &FPixelStreamingVideoInputRenderTarget::OnEndFrameRenderThread))
{
}

void FPixelStreamingVideoInputRenderTarget::OnEndFrameRenderThread()
{
	if (Target)
	{
		if (FTexture2DRHIRef Texture = Target->GetResource()->GetTexture2DRHI())
		{
			OnFrame(FPixelCaptureInputFrameRHI(Texture));
		}
	}
}

FString FPixelStreamingVideoInputRenderTarget::ToString()
{
	return TEXT("a Render Target");
}