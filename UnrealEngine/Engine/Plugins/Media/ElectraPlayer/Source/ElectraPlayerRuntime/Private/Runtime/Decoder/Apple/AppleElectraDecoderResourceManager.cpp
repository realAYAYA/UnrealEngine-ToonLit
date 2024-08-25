// Copyright Epic Games, Inc. All Rights Reserved.

#include "AppleElectraDecoderResourceManager.h"

#if PLATFORM_MAC || PLATFORM_IOS || PLATFORM_TVOS

#include "IElectraDecoderOutputVideo.h"
#include "ElectraDecodersUtils.h"
#include "Renderer/RendererBase.h"
#include "ElectraVideoDecoder_Apple.h"

#include "ElectraPlayerMisc.h"

#include <VideoToolbox/VideoToolbox.h>


namespace Electra
{

namespace AppleDecoderResources
{
	static FElectraDecoderResourceManagerApple::FCallbacks Callbacks;
}


bool FElectraDecoderResourceManagerApple::Startup(const FCallbacks& InCallbacks)
{
	AppleDecoderResources::Callbacks = InCallbacks;
	return AppleDecoderResources::Callbacks.GetMetalDevice ? true : false;
}

void FElectraDecoderResourceManagerApple::Shutdown()
{
}

TSharedPtr<IElectraDecoderResourceDelegate, ESPMode::ThreadSafe> FElectraDecoderResourceManagerApple::GetDelegate()
{
	static TSharedPtr<IElectraDecoderResourceDelegate, ESPMode::ThreadSafe> Self = MakeShared<FElectraDecoderResourceManagerApple, ESPMode::ThreadSafe>();
	return Self;
}


FElectraDecoderResourceManagerApple::~FElectraDecoderResourceManagerApple()
{
}

class FElectraDecoderResourceManagerApple::FInstanceVars : public FElectraDecoderResourceManagerApple::IDecoderPlatformResource
{
public:
	uint32 Codec4CC = 0;
};



IElectraDecoderResourceDelegateApple::IDecoderPlatformResource* FElectraDecoderResourceManagerApple::CreatePlatformResource(void* InOwnerHandle, EDecoderPlatformResourceType InDecoderResourceType, const TMap<FString, FVariant> InOptions)
{
	FInstanceVars* Vars = new FInstanceVars;
	check(Vars);
	if (Vars)
	{
		uint32 Codec4CC = (uint32)ElectraDecodersUtil::GetVariantValueSafeU64(InOptions, TEXT("codec_4cc"), 0);
		Vars->Codec4CC = Codec4CC;
	}
	return Vars;
}

void FElectraDecoderResourceManagerApple::ReleasePlatformResource(void* InOwnerHandle, IDecoderPlatformResource* InHandleToDestroy)
{
	FInstanceVars* Vars = static_cast<FInstanceVars*>(InHandleToDestroy);
	if (Vars)
	{
		delete Vars;
	}
}


bool FElectraDecoderResourceManagerApple::GetMetalDevice(void **OutMetalDevice)
{
	check(OutMetalDevice);
	if (AppleDecoderResources::Callbacks.GetMetalDevice)
	{
		return AppleDecoderResources::Callbacks.GetMetalDevice(OutMetalDevice, AppleDecoderResources::Callbacks.UserValue);
	}
	return false;
}


bool FElectraDecoderResourceManagerApple::SetupRenderBufferFromDecoderOutput(IMediaRenderer::IBuffer* InOutBufferToSetup, TSharedPtr<FParamDict, ESPMode::ThreadSafe> InOutBufferPropertes, TSharedPtr<IElectraDecoderVideoOutput, ESPMode::ThreadSafe> InDecoderOutput, FElectraDecoderResourceManagerApple::IDecoderPlatformResource* InPlatformSpecificResource)
{
	check(InOutBufferToSetup);
	check(InOutBufferPropertes.IsValid());
	check(InDecoderOutput.IsValid());

	TSharedPtr<FElectraPlayerVideoDecoderOutputApple, ESPMode::ThreadSafe> DecoderOutput = InOutBufferToSetup->GetBufferProperties().GetValue(RenderOptionKeys::Texture).GetSharedPointer<FElectraPlayerVideoDecoderOutputApple>();
	FInstanceVars* Vars = static_cast<FInstanceVars*>(InPlatformSpecificResource);

	if (DecoderOutput.IsValid())
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
		InOutBufferPropertes->Set(IDecoderOutputOptionNames::BitsPerComponent, FVariantValue((int64)InDecoderOutput->GetNumberOfBits()));

		int32 Width = InDecoderOutput->GetDecodedWidth();
		int32 Height = InDecoderOutput->GetDecodedHeight();
		//
		// Image buffers?
		//
		IElectraDecoderVideoOutputImageBuffers* ImageBuffers = reinterpret_cast<IElectraDecoderVideoOutputImageBuffers*>(InDecoderOutput->GetPlatformOutputHandle(EElectraDecoderPlatformOutputHandleType::ImageBuffers));
		if (ImageBuffers != nullptr)
		{
			int32 NumImageBuffers = ImageBuffers->GetNumberOfBuffers();
			check(NumImageBuffers == 1 || NumImageBuffers == 2);

			// Color buffer

			EElectraDecoderPlatformPixelFormat PixFmt = ImageBuffers->GetBufferFormatByIndex(0);
			EElectraDecoderPlatformPixelEncoding PixEnc = ImageBuffers->GetBufferEncodingByIndex(0);

			EPixelFormat RHIPixFmt;
			switch (PixFmt)
			{
				case EElectraDecoderPlatformPixelFormat::R8G8B8A8:		RHIPixFmt = EPixelFormat::PF_R8G8B8A8; break;
				case EElectraDecoderPlatformPixelFormat::A8R8G8B8:		RHIPixFmt = EPixelFormat::PF_A8R8G8B8; break;
				case EElectraDecoderPlatformPixelFormat::B8G8R8A8:		RHIPixFmt = EPixelFormat::PF_B8G8R8A8; break;
				case EElectraDecoderPlatformPixelFormat::R16G16B16A16:	RHIPixFmt = EPixelFormat::PF_R16G16B16A16_UNORM; break;
				case EElectraDecoderPlatformPixelFormat::A16B16G16R16:	RHIPixFmt = EPixelFormat::PF_A16B16G16R16; break;
				case EElectraDecoderPlatformPixelFormat::A32B32G32R32F:	RHIPixFmt = EPixelFormat::PF_A32B32G32R32F; break;
				case EElectraDecoderPlatformPixelFormat::A2B10G10R10:	RHIPixFmt = EPixelFormat::PF_A2B10G10R10; break;
				case EElectraDecoderPlatformPixelFormat::DXT1:			RHIPixFmt = EPixelFormat::PF_DXT1; break;
				case EElectraDecoderPlatformPixelFormat::DXT5:			RHIPixFmt = EPixelFormat::PF_DXT5; break;
				case EElectraDecoderPlatformPixelFormat::BC4:			RHIPixFmt = EPixelFormat::PF_BC4; break;
				case EElectraDecoderPlatformPixelFormat::NV12:			RHIPixFmt = EPixelFormat::PF_NV12; break;
				case EElectraDecoderPlatformPixelFormat::P010:			RHIPixFmt = EPixelFormat::PF_P010; break;
				default: RHIPixFmt = EPixelFormat::PF_Unknown; break;
			}
			check(RHIPixFmt != EPixelFormat::PF_Unknown);

			EVideoDecoderPixelEncoding DecPixEnc;
			switch (PixEnc)
			{
				case EElectraDecoderPlatformPixelEncoding::Native:			DecPixEnc = EVideoDecoderPixelEncoding::Native; break;
				case EElectraDecoderPlatformPixelEncoding::RGB:				DecPixEnc = EVideoDecoderPixelEncoding::RGB; break;
				case EElectraDecoderPlatformPixelEncoding::RGBA:			DecPixEnc = EVideoDecoderPixelEncoding::RGBA; break;
				case EElectraDecoderPlatformPixelEncoding::YCbCr:			DecPixEnc = EVideoDecoderPixelEncoding::YCbCr; break;
				case EElectraDecoderPlatformPixelEncoding::YCbCr_Alpha:		DecPixEnc = EVideoDecoderPixelEncoding::YCbCr_Alpha; break;
				case EElectraDecoderPlatformPixelEncoding::YCoCg:			DecPixEnc = EVideoDecoderPixelEncoding::YCoCg; break;
				case EElectraDecoderPlatformPixelEncoding::YCoCg_Alpha:		DecPixEnc = EVideoDecoderPixelEncoding::YCoCg_Alpha; break;
				case EElectraDecoderPlatformPixelEncoding::CbY0CrY1:		DecPixEnc = EVideoDecoderPixelEncoding::CbY0CrY1; break;
				case EElectraDecoderPlatformPixelEncoding::Y0CbY1Cr:		DecPixEnc = EVideoDecoderPixelEncoding::Y0CbY1Cr; break;
				case EElectraDecoderPlatformPixelEncoding::ARGB_BigEndian:	DecPixEnc = EVideoDecoderPixelEncoding::ARGB_BigEndian; break;
				default: DecPixEnc = DecPixEnc = EVideoDecoderPixelEncoding::Native; break;
			}

			int32 Pitch = ImageBuffers->GetBufferPitchByIndex(0);

			InOutBufferPropertes->Set(IDecoderOutputOptionNames::PixelFormat, FVariantValue((int64)RHIPixFmt));
			InOutBufferPropertes->Set(IDecoderOutputOptionNames::PixelEncoding, FVariantValue((int64)DecPixEnc));
			InOutBufferPropertes->Set(IDecoderOutputOptionNames::Pitch, FVariantValue((int64)Pitch));

// [...] ALPHA BUFFER -- HOW DO WE PASS IT ON!?
// [...] ANY CS/HDR INFO FROM THE DECODER? -- PASSING IT ON WOULD BE EASY, BUT RIGHT NOW WE ALWAYS READ IT FROM THE CONTAINER (in code further up the chain)

			TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> ColorBuffer = ImageBuffers->GetBufferDataByIndex(0);
			if (ColorBuffer.IsValid() && ColorBuffer->Num())
			{
				//
				// CPU side buffer
				//
				DecoderOutput->InitializeWithBuffer(ColorBuffer,
					Pitch,							// Buffer stride
					FIntPoint(Width, Height),		// Buffer dimensions
					InOutBufferPropertes);
				return true;
			}
			else
			{
				// Note: we assume a DX11 texture at this point, but this could also be interpreted as IUknown and then either DX11 or DX12 resources being derived from it...
				CVImageBufferRef ImageBuffer = static_cast<CVImageBufferRef>(ImageBuffers->GetBufferTextureByIndex(0));
				if (ImageBuffer != nullptr)
				{
					//
					// GPU texture (CVImageBuffer)
					//
					DecoderOutput->Initialize(ImageBuffer, InOutBufferPropertes);
					return true;
				}
			}

		}
	}
	return false;
}

} // namespace Electra

#endif
