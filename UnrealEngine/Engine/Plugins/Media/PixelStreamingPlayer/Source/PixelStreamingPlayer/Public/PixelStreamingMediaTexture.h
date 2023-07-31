// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/Texture2DDynamic.h"
#include "RenderTargetPool.h"
#include "api/media_stream_interface.h"
#include "PixelStreamingMediaTexture.generated.h"

class FPixelStreamingMediaTextureResource;

/**
 * A Texture Object that can be used in materials etc. that takes updates from webrtc frames.
 */
UCLASS(NotBlueprintType, NotBlueprintable, HideDropdown, HideCategories = (ImportSettings, Compression, Texture, Adjustments, Compositing, LevelOfDetail, Object), META = (DisplayName = "PixelStreaming Media Texture"))
class PIXELSTREAMINGPLAYER_API UPixelStreamingMediaTexture : public UTexture2DDynamic, public rtc::VideoSinkInterface<webrtc::VideoFrame>
{
	GENERATED_UCLASS_BODY()

protected:
	// UObject overrides.
	virtual void BeginDestroy() override;
	virtual void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) override;

	// UTexture implementation
	virtual FTextureResource* CreateResource() override;

	// from rtc::VideoSinkInterface<webrtc::VideoFrame>
	virtual void OnFrame(const webrtc::VideoFrame& frame) override;

private:
	void InitializeResources();

	// updates the internal texture resource after each frame.
	void UpdateTextureReference(FRHICommandList& RHICmdList, FTexture2DRHIRef Reference);

	TArray<uint8_t> Buffer;

	FCriticalSection RenderSyncContext;
	FTextureRHIRef SourceTexture;
	FPooledRenderTargetDesc RenderTargetDescriptor;
	TRefCountPtr<IPooledRenderTarget> RenderTarget;
	FPixelStreamingMediaTextureResource* CurrentResource;
};
