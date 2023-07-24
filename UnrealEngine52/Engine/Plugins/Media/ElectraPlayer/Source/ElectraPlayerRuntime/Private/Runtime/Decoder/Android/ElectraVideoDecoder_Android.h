// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

#if PLATFORM_ANDROID

#include "PlayerCore.h"
#include "MediaVideoDecoderOutputAndroid.h"
#include "Renderer/RendererVideo.h"


namespace Electra
{

class FElectraPlayerVideoDecoderOutputAndroid : public FVideoDecoderOutputAndroid
{
public:
	FElectraPlayerVideoDecoderOutputAndroid() = default;
	~FElectraPlayerVideoDecoderOutputAndroid() = default;

	static void ReleaseToSurface(uint32 NativeDecoderID, const FDecoderTimeStamp& Time);

	void Initialize(EOutputType InOutputType, int32 InBufferIndex, int32 InValidCount, const TFunction<void(uint32, const FDecoderTimeStamp&)>& InSurfaceReleaseFN, uint32 InNativeDecoderID, FParamDict* InParamDict);

	void ReleaseToSurface() const override;

	void SetOwner(const TSharedPtr<IDecoderOutputOwner, ESPMode::ThreadSafe>& InOwningRenderer) override;

	void ShutdownPoolable() override;

	virtual EOutputType GetOutputType() const override;

private:
	// Decoder specific surface release function
	TFunction<void(uint32, const FDecoderTimeStamp&)> SurfaceReleaseFN;

	// Decoder output type
	EOutputType OutputType = EOutputType::Unknown;

	// Decoder output buffer index associated with this sample (Surface type)
	int32 BufferIndex = -1;

	// Valid count to check for decoder Android changes within a single logical decoder (Surface type)
	int32 ValidCount = -1;

	// ID of native decoder
	uint32 NativeDecoderID = 0;

	// We hold a weak reference to the video renderer. During destruction the video renderer could be destroyed while samples are still out there..
	TWeakPtr<IDecoderOutputOwner, ESPMode::ThreadSafe> OwningRenderer;
};

}

#endif

