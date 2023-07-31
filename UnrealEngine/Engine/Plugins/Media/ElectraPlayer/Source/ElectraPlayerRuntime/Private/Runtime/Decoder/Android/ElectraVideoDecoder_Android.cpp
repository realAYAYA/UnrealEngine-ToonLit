// Copyright Epic Games, Inc. All Rights Reserved.

#include "ElectraVideoDecoder_Android.h"

#if PLATFORM_ANDROID

namespace Electra
{

void FElectraPlayerVideoDecoderOutputAndroid::Initialize(EOutputType InOutputType, int32 InBufferIndex, int32 InValidCount, const TFunction<void(uint32, const FDecoderTimeStamp&)>& InSurfaceReleaseFN, uint32 InNativeDecoderID, FParamDict* InParamDict)
{
	FVideoDecoderOutputAndroid::Initialize(InParamDict);
	SurfaceReleaseFN = InSurfaceReleaseFN;
	OutputType = InOutputType;
	BufferIndex = InBufferIndex;
	ValidCount = InValidCount;
	NativeDecoderID = InNativeDecoderID;
}

void FElectraPlayerVideoDecoderOutputAndroid::ReleaseToSurface() const
{
	SurfaceReleaseFN(NativeDecoderID, GetTime());
}

void FElectraPlayerVideoDecoderOutputAndroid::SetOwner(const TSharedPtr<IDecoderOutputOwner, ESPMode::ThreadSafe>& InOwningRenderer)
{
	OwningRenderer = InOwningRenderer;
}

void FElectraPlayerVideoDecoderOutputAndroid::ShutdownPoolable()
{
	TSharedPtr<IDecoderOutputOwner, ESPMode::ThreadSafe> lockedVideoRenderer = OwningRenderer.Pin();
	if (lockedVideoRenderer.IsValid())
	{
		lockedVideoRenderer->SampleReleasedToPool(this);
	}
}

FElectraPlayerVideoDecoderOutputAndroid::EOutputType FElectraPlayerVideoDecoderOutputAndroid::GetOutputType() const
{
	return OutputType;
}


} // namespace Electra

// -----------------------------------------------------------------------------------------------------------------------------

FVideoDecoderOutput* FElectraPlayerPlatformVideoDecoderOutputFactory::Create()
{
	return new Electra::FElectraPlayerVideoDecoderOutputAndroid();
}

#endif
