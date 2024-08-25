// Copyright Epic Games, Inc. All Rights Reserved.

#include "AndroidElectraDecoderResourceManager.h"

#include "IElectraDecoderOutputVideo.h"
#include "ElectraDecodersUtils.h"
#include "Renderer/RendererBase.h"
#include "ElectraVideoDecoder_Android.h"

#include "VideoDecoderResourceDelegate.h"

//#include "Renderer/RendererVideo.h"
//#include "MediaDecoderOutput.h"

/***************************************************************************************************************************************************/
/***************************************************************************************************************************************************/
/***************************************************************************************************************************************************/
namespace Electra
{

bool FElectraDecoderResourceManagerAndroid::Startup()
{
	return true;
}

void FElectraDecoderResourceManagerAndroid::Shutdown()
{
}

TSharedPtr<IElectraDecoderResourceDelegate, ESPMode::ThreadSafe> FElectraDecoderResourceManagerAndroid::GetDelegate()
{
	static TSharedPtr<IElectraDecoderResourceDelegate, ESPMode::ThreadSafe> Self = MakeShared<FElectraDecoderResourceManagerAndroid, ESPMode::ThreadSafe>();
	return Self;
}


FElectraDecoderResourceManagerAndroid::~FElectraDecoderResourceManagerAndroid()
{
}


class FElectraDecoderResourceManagerAndroid::FInstanceVars : public IElectraDecoderResourceDelegateAndroid::IDecoderPlatformResourceAndroid
{
public:
	virtual ~FInstanceVars() {}
	void RequestSurface(TWeakPtr<ISurfaceRequestCallback, ESPMode::ThreadSafe> InRequestCallback) override;
	ESurfaceChangeResult VerifySurfaceView(void*& OutNewSurfaceView, void* InCurrentSurfaceView) override;
	void SetBufferReleaseCallback(TWeakPtr<IBufferReleaseCallback, ESPMode::ThreadSafe> InBufferReleaseCallback) override;

	TWeakPtr<IVideoDecoderResourceDelegate, ESPMode::ThreadSafe> VideoDecoderResourceDelegate;
	TWeakPtr<ISurfaceRequestCallback, ESPMode::ThreadSafe> SurfaceRequestCallback;
	TWeakPtr<IBufferReleaseCallback, ESPMode::ThreadSafe> BufferReleaseCallback;
	
	IDecoderPlatformResourceAndroid::ISurfaceRequestCallback::ESurfaceType SurfaceType = IDecoderPlatformResourceAndroid::ISurfaceRequestCallback::ESurfaceType::Error;
	volatile uint32 CurrentID = 0;

	static std::atomic<uint32> NextID;
};
std::atomic<uint32> FElectraDecoderResourceManagerAndroid::FInstanceVars::NextID { 0 };


void FElectraDecoderResourceManagerAndroid::FInstanceVars::RequestSurface(TWeakPtr<IElectraDecoderResourceDelegateAndroid::IDecoderPlatformResourceAndroid::ISurfaceRequestCallback, ESPMode::ThreadSafe> InRequestCallback)
{
	SurfaceRequestCallback = MoveTemp(InRequestCallback);

	auto Notify = [Callback=SurfaceRequestCallback](IDecoderPlatformResourceAndroid::ISurfaceRequestCallback::ESurfaceType InType, void* InSurface) -> void
	{
		TSharedPtr<ISurfaceRequestCallback, ESPMode::ThreadSafe> cb = Callback.Pin();
		if (cb.IsValid())
		{
			cb->OnNewSurface(InType, InSurface);
		}
	};

	TSharedPtr<IVideoDecoderResourceDelegate, ESPMode::ThreadSafe> rd = VideoDecoderResourceDelegate.Pin();
	if (!rd.IsValid())
	{
		Notify(IDecoderPlatformResourceAndroid::ISurfaceRequestCallback::ESurfaceType::Error, nullptr);
		return;
	}

	/**
		Note: The surface returned (if any) must be an _additional_ globalref, not the actual surface.
		      This is because the actual surface might go away at any time, leaving us with an otherwise stale
			  reference that will result in a JNI crash when used.
			  The decoder implementation will pass along this globalref to the decoder and then release it,
			  so this must not be the actual surface handle!
	 */
	void* Surface = rd->VideoDecoderResourceDelegate_GetCodecSurface();
	SurfaceType = IDecoderPlatformResourceAndroid::ISurfaceRequestCallback::ESurfaceType::Surface;
	Notify(SurfaceType, Surface);
}

FElectraDecoderResourceManagerAndroid::FInstanceVars::ESurfaceChangeResult FElectraDecoderResourceManagerAndroid::FInstanceVars::VerifySurfaceView(void*& OutNewSurfaceView, void* InCurrentSurfaceView)
{
	check(!"This should not be called since we are not supporting SurfaceViews at the moment!");
	OutNewSurfaceView = nullptr;
	return ESurfaceChangeResult::Error;
}


void FElectraDecoderResourceManagerAndroid::FInstanceVars::SetBufferReleaseCallback(TWeakPtr<IBufferReleaseCallback, ESPMode::ThreadSafe> InBufferReleaseCallback)
{
	CurrentID = ++NextID;
	BufferReleaseCallback = MoveTemp(InBufferReleaseCallback);
}



void FElectraDecoderResourceManagerAndroid::ReleaseToSurface(uint32 InID, const FDecoderTimeStamp& Time)
{
	TSharedPtr<FElectraDecoderResourceManagerAndroid, ESPMode::ThreadSafe> Dlg = StaticCastSharedPtr<FElectraDecoderResourceManagerAndroid>(GetDelegate());
	if (Dlg.IsValid())
	{
		FScopeLock lock(&Dlg->Lock);
		for(auto &Inst : Dlg->InstanceVars)
		{
			if (Inst->CurrentID == InID)
			{
				TSharedPtr<IElectraDecoderResourceDelegateAndroid::IDecoderPlatformResourceAndroid::IBufferReleaseCallback, ESPMode::ThreadSafe> cb = Inst->BufferReleaseCallback.Pin();
				if (cb.IsValid())
				{
//					cb->OnReleaseBuffer();
				}
			}
		}
	}
}



IElectraDecoderResourceDelegateAndroid::IDecoderPlatformResource* FElectraDecoderResourceManagerAndroid::CreatePlatformResource(void* InOwnerHandle, IElectraDecoderResourceDelegateAndroid::EDecoderPlatformResourceType InDecoderResourceType, const TMap<FString, FVariant> InOptions)
{
/*
	if (InDecoderResourceType != IElectraDecoderResourceDelegateAndroid::EDecoderPlatformResourceType::Video)
	{
		return nullptr;
	}
*/
	IVideoDecoderResourceDelegate* Delegate = reinterpret_cast<IVideoDecoderResourceDelegate*>(ElectraDecodersUtil::GetVariantValueSafeU64(InOptions, TEXT("VideoResourceDelegate"), 0));
	if (Delegate)
	{
		TSharedPtr<IVideoDecoderResourceDelegate, ESPMode::ThreadSafe> VideoDecoderResourceDelegate = Delegate->AsShared();
		FInstanceVars* Inst = new FInstanceVars;
		Inst->VideoDecoderResourceDelegate = VideoDecoderResourceDelegate;
		FScopeLock lock(&Lock);
		InstanceVars.Emplace(Inst);
		return Inst;
	}
	return nullptr;
}

void FElectraDecoderResourceManagerAndroid::ReleasePlatformResource(void* InOwnerHandle, IDecoderPlatformResource* InHandleToDestroy)
{
	FInstanceVars* InInst = static_cast<FInstanceVars*>(InHandleToDestroy);
	if (InInst)
	{
		FScopeLock lock(&Lock);
		check(InstanceVars.Contains(InInst));
		if (InstanceVars.Contains(InInst))
		{
			InstanceVars.Remove(InInst);
			delete InInst;
		}
	}
}


class FElectraDecoderVideoOutputCopyResources : public IElectraDecoderVideoOutputCopyResources
{
public:
	virtual ~FElectraDecoderVideoOutputCopyResources() {}
	void SetBufferIndex(int32 InIndex) override
	{ BufferIndex = InIndex; }
	void SetValidCount(int32 InValidCount) override
	{ ValidCount = InValidCount; }
	bool ShouldReleaseBufferImmediately() override
	{ return bReleaseImmediately; }

	int32 BufferIndex = -1;
	int32 ValidCount = -1;
	bool bReleaseImmediately = false;
};


bool FElectraDecoderResourceManagerAndroid::SetupRenderBufferFromDecoderOutput(IMediaRenderer::IBuffer* InOutBufferToSetup, TSharedPtr<FParamDict, ESPMode::ThreadSafe> InOutBufferPropertes, TSharedPtr<IElectraDecoderVideoOutput, ESPMode::ThreadSafe> InDecoderOutput, IDecoderPlatformResource* InPlatformSpecificResource)
{
	check(InOutBufferToSetup);
	check(InOutBufferPropertes.IsValid());
	check(InDecoderOutput.IsValid());
	if (!InPlatformSpecificResource)
	{
		return false;
	}
	
	FInstanceVars* InInst = static_cast<FInstanceVars*>(InPlatformSpecificResource);
	TSharedPtr<IVideoDecoderResourceDelegate, ESPMode::ThreadSafe> PinnedResourceDelegate = InInst->VideoDecoderResourceDelegate.Pin();
	TSharedPtr<FElectraPlayerVideoDecoderOutputAndroid, ESPMode::ThreadSafe> DecoderOutput = InOutBufferToSetup->GetBufferProperties().GetValue(RenderOptionKeys::Texture).GetSharedPointer<FElectraPlayerVideoDecoderOutputAndroid>();
	if (DecoderOutput.IsValid() && PinnedResourceDelegate.IsValid())
	{
		FElectraVideoDecoderOutputCropValues Crop = InDecoderOutput->GetCropValues();
		InOutBufferPropertes->Set(IDecoderOutputOptionNames::Width, FVariantValue((int64)InDecoderOutput->GetWidth()));
		InOutBufferPropertes->Set(IDecoderOutputOptionNames::Height, FVariantValue((int64)InDecoderOutput->GetHeight()));
		InOutBufferPropertes->Set(IDecoderOutputOptionNames::CropLeft, FVariantValue((int64)Crop.Left));
		InOutBufferPropertes->Set(IDecoderOutputOptionNames::CropRight, FVariantValue((int64)Crop.Right));
		InOutBufferPropertes->Set(IDecoderOutputOptionNames::CropTop, FVariantValue((int64)Crop.Top));
		InOutBufferPropertes->Set(IDecoderOutputOptionNames::CropBottom, FVariantValue((int64)Crop.Bottom));
		InOutBufferPropertes->Set(IDecoderOutputOptionNames::AspectRatio, FVariantValue((double)InDecoderOutput->GetAspectRatioW() / (double)InDecoderOutput->GetAspectRatioH()));
		InOutBufferPropertes->Set(IDecoderOutputOptionNames::AspectW, FVariantValue((int64)InDecoderOutput->GetAspectRatioW()));
		InOutBufferPropertes->Set(IDecoderOutputOptionNames::AspectH, FVariantValue((int64)InDecoderOutput->GetAspectRatioH()));
		InOutBufferPropertes->Set(IDecoderOutputOptionNames::FPSNumerator, FVariantValue((int64)InDecoderOutput->GetFrameRateNumerator()));
		InOutBufferPropertes->Set(IDecoderOutputOptionNames::FPSDenominator, FVariantValue((int64)InDecoderOutput->GetFrameRateDenominator()));
		int32 NumBits = InDecoderOutput->GetNumberOfBits();
		InOutBufferPropertes->Set(IDecoderOutputOptionNames::PixelFormat, FVariantValue((int64)((NumBits > 8)  ? EPixelFormat::PF_A2B10G10R10 : EPixelFormat::PF_B8G8R8A8)));
		InOutBufferPropertes->Set(IDecoderOutputOptionNames::BitsPerComponent, FVariantValue((int64) NumBits));
		InOutBufferPropertes->Set(IDecoderOutputOptionNames::Pitch, FVariantValue((int64)InDecoderOutput->GetDecodedWidth() * ((NumBits > 8) ? 2 : 1)));

		FElectraDecoderVideoOutputCopyResources cr;
		cr.bReleaseImmediately = InInst->SurfaceType == IDecoderPlatformResourceAndroid::ISurfaceRequestCallback::ESurfaceType::Surface;
		IElectraDecoderVideoOutput::EImageCopyResult CopyResult = InDecoderOutput->CopyPlatformImage(&cr);
		if (CopyResult != IElectraDecoderVideoOutput::EImageCopyResult::Succeeded)
		{
			return false;
		}

		check(InInst->SurfaceType == IDecoderPlatformResourceAndroid::ISurfaceRequestCallback::ESurfaceType::Surface);

		DecoderOutput->Initialize(InInst->SurfaceType == IDecoderPlatformResourceAndroid::ISurfaceRequestCallback::ESurfaceType::Surface     ? FVideoDecoderOutputAndroid::EOutputType::DirectToSurfaceAsQueue :
								  InInst->SurfaceType == IDecoderPlatformResourceAndroid::ISurfaceRequestCallback::ESurfaceType::SurfaceView ? FVideoDecoderOutputAndroid::EOutputType::DirectToSurfaceAsView :
								  FVideoDecoderOutputAndroid::EOutputType::Unknown, cr.BufferIndex, cr.ValidCount, ReleaseToSurface, InInst->CurrentID, InOutBufferPropertes);
		return true;
#if 0
-----------------
		DecoderOutput->Initialize(bSurfaceIsView ? FVideoDecoderOutputAndroid::EOutputType::DirectToSurfaceAsView : FVideoDecoderOutputAndroid::EOutputType::DirectToSurfaceAsQueue, NextImage.OutputBufferInfo.BufferIndex, NextImage.OutputBufferInfo.ValidCount, ReleaseToSurface, NativeDecoderID, OutputBufferSampleProperties.Get());

		if (!bSurfaceIsView)
		{
			// Release the decoder output buffer, thereby enqueuing it on our output surface.
			// (we are issuing an RHI thread based update to our texture for each of these, so we should always have a 1:1 mapping - assuming we are fast enough
			//  to not make the surface drop a frame before we get to it)
			DecoderInstance->ReleaseOutputBuffer(NextImage.OutputBufferInfo.BufferIndex, NextImage.OutputBufferInfo.ValidCount, true, -1);
		}
		else
		{
			// We decode right into a Surface. Queue up output buffers until we are ready to show them
			ReadyOutputBuffersToSurface.Emplace(FOutputBufferInfo(FDecoderTimeStamp(NextImage.SourceInfo->AdjustedPTS.GetAsTimespan(), NextImage.SourceInfo->AdjustedPTS.GetSequenceIndex()), NextImage.OutputBufferInfo.BufferIndex, NextImage.OutputBufferInfo.ValidCount));
		}
		// Note: we are returning the buffer to the renderer before we are done getting data
		// (but: this will sync all up as the render command queues are all in order - and hence the async process will be done before MediaTextureResources sees this)
		Renderer->ReturnBuffer(RenderOutputBuffer, true, *OutputBufferSampleProperties);
----------------
#endif
	}
	return false;
}

} // namespace Electra
