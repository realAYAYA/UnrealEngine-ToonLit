// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

#if PLATFORM_MAC || PLATFORM_IOS || PLATFORM_TVOS

#include "PlayerCore.h"
#include "MediaVideoDecoderOutputApple.h"
#include "Renderer/RendererVideo.h"


namespace Electra
{

class FElectraPlayerVideoDecoderOutputApple : public FVideoDecoderOutputApple
{
public:
	FElectraPlayerVideoDecoderOutputApple();

	~FElectraPlayerVideoDecoderOutputApple();

    void Initialize(CVImageBufferRef InImageBufferRef, FParamDict* InParamDict);

	void SetOwner(const TSharedPtr<IDecoderOutputOwner, ESPMode::ThreadSafe>& InOwningRenderer) override;

	void ShutdownPoolable() override;

	uint32 GetStride() const override;

	CVImageBufferRef GetImageBuffer() const override;

private:
	uint32 Stride;

	CVImageBufferRef ImageBufferRef;

	// We hold a weak reference to the video renderer. During destruction the video renderer could be destroyed while samples are still out there..
	TWeakPtr<IDecoderOutputOwner, ESPMode::ThreadSafe> OwningRenderer;
};


} // namespace Electra

#endif

