// Copyright Epic Games, Inc. All Rights Reserved.

#include "PreprocessRenderInputTextureProxy.h"

#include "Engine/Texture.h"


namespace UE::DMXPixelMapping::Rendering::Preprocess::Private
{
	FPreprocessRenderInputTextureProxy::FPreprocessRenderInputTextureProxy(UTexture* InTexture)
		:  WeakInputTexture(InTexture)
	{
		if (InTexture)
		{
			InTexture->WaitForStreaming();
		}
	}

	void FPreprocessRenderInputTextureProxy::Render()
	{
		// No rendering required
	}

	UTexture* FPreprocessRenderInputTextureProxy::GetRenderedTexture() const
	{
		return WeakInputTexture.Get();
	}

	FVector2D FPreprocessRenderInputTextureProxy::GetSize2D() const
	{
		return WeakInputTexture.IsValid() ?
			FVector2D(WeakInputTexture->GetSurfaceWidth(), WeakInputTexture->GetSurfaceHeight()) :
			FVector2D{ 0.0, 0.0 };
	}
}
