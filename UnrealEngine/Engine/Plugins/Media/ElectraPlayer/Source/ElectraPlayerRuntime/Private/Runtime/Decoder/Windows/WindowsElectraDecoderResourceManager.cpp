// Copyright Epic Games, Inc. All Rights Reserved.

#include "WindowsElectraDecoderResourceManager.h"
#include "Templates/AlignmentTemplates.h"
#include "Misc/ScopeLock.h"

#include "IElectraDecoderOutputVideo.h"
#include "ElectraDecodersUtils.h"

#include "Renderer/RendererBase.h"
#include "ElectraVideoDecoder_PC.h"
#include "VideoDecoderResourceDelegate.h"

#include "ElectraPlayerMisc.h"

#include COMPILED_PLATFORM_HEADER(ElectraDecoderGPUBufferHelpers.h)

/***************************************************************************************************************************************************/
/***************************************************************************************************************************************************/
/***************************************************************************************************************************************************/
namespace Electra
{

namespace WindowsDecoderResources
{
	static FElectraDecoderResourceManagerWindows::FCallbacks Callbacks;
	static bool bDidInitializeMF = false;
}

bool FElectraDecoderResourceManagerWindows::Startup(const FCallbacks& InCallbacks)
{
	// Windows 8 or better?
	if (!FPlatformMisc::VerifyWindowsVersion(6, 2))
	{
		UE_LOG(LogElectraPlayer, Log, TEXT("Electra is incompatible with Windows prior to 8.0 version: %s"), *FPlatformMisc::GetOSVersion());
		return false;
	}
	if (!(FPlatformProcess::GetDllHandle(TEXT("mf.dll"))
		&& FPlatformProcess::GetDllHandle(TEXT("mfplat.dll"))
		&& FPlatformProcess::GetDllHandle(TEXT("msmpeg2vdec.dll"))
		&& FPlatformProcess::GetDllHandle(TEXT("MSAudDecMFT.dll"))))
	{
		UE_LOG(LogElectraPlayer, Error, TEXT("Electra can't load Media Foundation, %s"), *FPlatformMisc::GetOSVersion());
		return false;
	}
	HRESULT Result = MFStartup(MF_VERSION);
	if (FAILED(Result))
	{
		UE_LOG(LogElectraPlayer, Error, TEXT("MFStartup failed with 0x%08x"), Result);
		return false;
	}
	WindowsDecoderResources::bDidInitializeMF = true;
	WindowsDecoderResources::Callbacks = InCallbacks;
	return WindowsDecoderResources::Callbacks.GetD3DDevice ? true : false;
}

void FElectraDecoderResourceManagerWindows::Shutdown()
{
	if (WindowsDecoderResources::bDidInitializeMF)
	{
		MFShutdown();
		WindowsDecoderResources::bDidInitializeMF = false;
	}
}

TSharedPtr<IElectraDecoderResourceDelegate, ESPMode::ThreadSafe> FElectraDecoderResourceManagerWindows::GetDelegate()
{
	static TSharedPtr<IElectraDecoderResourceDelegate, ESPMode::ThreadSafe> Self = MakeShared<FElectraDecoderResourceManagerWindows, ESPMode::ThreadSafe>();
	return Self;
}


FElectraDecoderResourceManagerWindows::~FElectraDecoderResourceManagerWindows()
{
}


class FElectraDecoderResourceManagerWindows::FInstanceVars : public IElectraDecoderResourceDelegateWindows::IDecoderPlatformResource
{
public:
	uint32 Codec4CC = 0;
	uint32 MaxWidth = 0;
	uint32 MaxHeight = 0;
	uint32 MaxOutputBuffers = 0;
	TWeakPtr<IVideoDecoderResourceDelegate, ESPMode::ThreadSafe> VideoDecoderResourceDelegate;
	TSharedPtr<FElectraMediaDecoderOutputBufferPool_DX12> D3D12ResourcePool;
};


IElectraDecoderResourceDelegateWindows::IDecoderPlatformResource* FElectraDecoderResourceManagerWindows::CreatePlatformResource(void* InOwnerHandle, EDecoderPlatformResourceType InDecoderResourceType, const TMap<FString, FVariant> InOptions)
{
	IVideoDecoderResourceDelegate* Delegate = reinterpret_cast<IVideoDecoderResourceDelegate*>(ElectraDecodersUtil::GetVariantValueSafeU64(InOptions, TEXT("VideoResourceDelegate"), 0));
	if (Delegate)
	{
		TSharedPtr<IVideoDecoderResourceDelegate, ESPMode::ThreadSafe> VideoDecoderResourceDelegate = Delegate->AsShared();
		FInstanceVars* Vars = new FInstanceVars();
		check(Vars);
		if (Vars)
		{
			Vars->VideoDecoderResourceDelegate = VideoDecoderResourceDelegate;
			Vars->Codec4CC = (uint32)ElectraDecodersUtil::GetVariantValueSafeU64(InOptions, TEXT("codec_4cc"), 0);
			Vars->MaxWidth = (uint32)ElectraDecodersUtil::GetVariantValueSafeU64(InOptions, TEXT("max_width"), 1920);
			Vars->MaxHeight = (uint32)ElectraDecodersUtil::GetVariantValueSafeU64(InOptions, TEXT("max_height"), 1080);
			Vars->MaxOutputBuffers = (uint32)ElectraDecodersUtil::GetVariantValueSafeU64(InOptions, TEXT("max_output_buffers"), 5);
		}
		return Vars;
	}
	return nullptr;
}


void FElectraDecoderResourceManagerWindows::ReleasePlatformResource(void* InOwnerHandle, IDecoderPlatformResource* InHandleToDestroy)
{
	FInstanceVars* Vars = static_cast<FInstanceVars*>(InHandleToDestroy);
	if (Vars)
	{
		delete Vars;
	}
}


bool FElectraDecoderResourceManagerWindows::GetD3DDevice(void **OutD3DDevice, int32* OutD3DVersionTimes1000)
{
	check(OutD3DDevice);
	check(OutD3DVersionTimes1000);
	if (WindowsDecoderResources::Callbacks.GetD3DDevice)
	{
		return WindowsDecoderResources::Callbacks.GetD3DDevice(OutD3DDevice, OutD3DVersionTimes1000, WindowsDecoderResources::Callbacks.UserValue);
	}
	return false;
}

TSharedPtr<IElectraDecoderResourceDelegateBase::IAsyncConsecutiveTaskSync, ESPMode::ThreadSafe> FElectraDecoderResourceManagerWindows::CreateAsyncConsecutiveTaskSync()
{
	if (WindowsDecoderResources::Callbacks.CreateAsyncConsecutiveTaskSync)
	{
		return WindowsDecoderResources::Callbacks.CreateAsyncConsecutiveTaskSync();
	}
	return nullptr;
}

bool FElectraDecoderResourceManagerWindows::RunCodeAsync(TFunction<void()>&& CodeToRun, IAsyncConsecutiveTaskSync* TaskSync)
{
	if (WindowsDecoderResources::Callbacks.RunCodeAsync)
	{
		return WindowsDecoderResources::Callbacks.RunCodeAsync(MoveTemp(CodeToRun), TaskSync);
	}
	return false;
}


bool FElectraDecoderResourceManagerWindows::SetupRenderBufferFromDecoderOutputFromMFSample(IMediaRenderer::IBuffer* InOutBufferToSetup, TSharedPtr<FParamDict, ESPMode::ThreadSafe> InOutBufferPropertes, TSharedPtr<IElectraDecoderVideoOutput, ESPMode::ThreadSafe> InDecoderOutput, FElectraDecoderResourceManagerWindows::IDecoderPlatformResource* InPlatformSpecificResource)
{
	TSharedPtr<FElectraPlayerVideoDecoderOutputPC, ESPMode::ThreadSafe> DecoderOutput = InOutBufferToSetup->GetBufferProperties().GetValue(RenderOptionKeys::Texture).GetSharedPointer<FElectraPlayerVideoDecoderOutputPC>();
	FInstanceVars* Vars = static_cast<FInstanceVars*>(InPlatformSpecificResource);

	if (DecoderOutput.IsValid() && Vars != nullptr)
	{
		TSharedPtr<IVideoDecoderResourceDelegate, ESPMode::ThreadSafe> PinnedResourceDelegate = Vars->VideoDecoderResourceDelegate.Pin();

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

		// What type of decoder output do we have here?
		TMap<FString, FVariant> ExtraValues;
		InDecoderOutput->GetExtraValues(ExtraValues);

		// Presently we handle only decoder output from a DirectX decoder transform.
		check(ElectraDecodersUtil::GetVariantValueFString(ExtraValues, TEXT("platform")).Equals(TEXT("dx")));

		int32 DXVersion = (int32)ElectraDecodersUtil::GetVariantValueSafeI64(ExtraValues, TEXT("dxversion"), 0);
		bool bIsSW = !!ElectraDecodersUtil::GetVariantValueSafeI64(ExtraValues, TEXT("sw"), 0);

		HRESULT Result;
		// DX11, DX12 or non-DX & HW accelerated?
		if ((DXVersion == 0 || DXVersion >= 11000) && !bIsSW)
		{
			TRefCountPtr<IMFSample> DecodedOutputSample = reinterpret_cast<IMFSample*>(InDecoderOutput->GetPlatformOutputHandle(EElectraDecoderPlatformOutputHandleType::MFSample));

			DWORD BuffersNum = 0;
			if ((Result = DecodedOutputSample->GetBufferCount(&BuffersNum)) != S_OK)
			{
				UE_LOG(LogElectraPlayer, Error, TEXT("IMFSample::GetBufferCount() failed with %08x"), Result);
				return false;
			}
			if (BuffersNum != 1)
			{
				UE_LOG(LogElectraPlayer, Error, TEXT("IMFSample::GetBufferCount() returned %u buffers instead of 1"), Result);
				return false;
			}

			TRefCountPtr<IMFMediaBuffer> Buffer;
			if ((Result = DecodedOutputSample->GetBufferByIndex(0, Buffer.GetInitReference())) != S_OK)
			{
				UE_LOG(LogElectraPlayer, Error, TEXT("IMFSample::GetBufferByIndex() failed with %08x"), Result);
				return false;
			}

			TRefCountPtr<IMFDXGIBuffer> DXGIBuffer;
			if ((Result = Buffer->QueryInterface(__uuidof(IMFDXGIBuffer), (void**)DXGIBuffer.GetInitReference())) != S_OK)
			{
				UE_LOG(LogElectraPlayer, Error, TEXT("IMFMediaBuffer::QueryInterface(IMFDXGIBuffer) failed with %08x"), Result);
				return false;
			}

			// No, continue with SW or DX11 texture output
			TRefCountPtr<ID3D11Texture2D> Texture2D;
			if ((Result = DXGIBuffer->GetResource(IID_PPV_ARGS(Texture2D.GetInitReference()))) != S_OK)
			{
				UE_LOG(LogElectraPlayer, Error, TEXT("IMFDXGIBuffer::GetResource(ID3D11Texture2D) failed with %08x"), Result);
				return false;
			}
			D3D11_TEXTURE2D_DESC TextureDesc;
			Texture2D->GetDesc(&TextureDesc);
			if (TextureDesc.Format != DXGI_FORMAT_NV12 && TextureDesc.Format != DXGI_FORMAT_P010)
			{
				UE_LOG(LogElectraPlayer, Error, TEXT("ID3D11Texture2D::GetDesc() did not return NV12 or P010 format as expected"));
				return false;
			}

			InOutBufferPropertes->Set(IDecoderOutputOptionNames::PixelFormat, FVariantValue((int64)(TextureDesc.Format == DXGI_FORMAT_NV12 ? EPixelFormat::PF_NV12 : EPixelFormat::PF_P010)));
			InOutBufferPropertes->Set(IDecoderOutputOptionNames::BitsPerComponent, FVariantValue((int64)(TextureDesc.Format == DXGI_FORMAT_NV12 ? 8 : 10)));

			if (DXVersion == 0 || DXVersion >= 12000)
			{
				//
				// DX12 (with DX11 decode device) & non-DX
				// 
				// (access buffer for CPU use)
				//
				TRefCountPtr<IMF2DBuffer> Buffer2D;
				if ((Result = Buffer->QueryInterface(__uuidof(IMF2DBuffer), (void**)Buffer2D.GetInitReference())) != S_OK)
				{
					UE_LOG(LogElectraPlayer, Error, TEXT("IMFDXGIBuffer::QueryInterface(IMF2DBuffer) failed with %08x"), Result);
					return false;
				}

				uint8* Data = nullptr;
				LONG Pitch = 0;
				if ((Result = Buffer2D->Lock2D(&Data, &Pitch)) != S_OK)
				{
					UE_LOG(LogElectraPlayer, Error, TEXT("IMF2DBuffer::Lock2D() failed with %08x"), Result);
					return false;
				}

				InOutBufferPropertes->Set(IDecoderOutputOptionNames::Pitch, FVariantValue((int64)Pitch));

				// Get decoded with (e.g. featuring any height adjustments for CPU buffer usage of NV12 etc.)
				int32 Width = InDecoderOutput->GetDecodedWidth();
				int32 Height = InDecoderOutput->GetDecodedHeight();

				DecoderOutput->InitializeWithBuffer(Data, Pitch * Height,
					Pitch,						// Buffer stride
					FIntPoint(Width, Height),	// Buffer dimensions
					InOutBufferPropertes);

				if ((Result = Buffer2D->Unlock2D()) != S_OK)
				{
					UE_LOG(LogElectraPlayer, Error, TEXT("IMF2DBuffer::Unlock2D() failed with %08x"), Result);
					return false;
				}
			}
			else
			{
				//
				// DX11
				//
				check(DXVersion >= 11000);

				// Notes:
				// - No need to apply a *1.5 factor to the height in this case. DX11 can access sub-resources in both NV12 & P010 to get to the CbCr data
				// - No need to specify a pitch as this is all directly handled on the GPU side of things
				//
				ID3D11Device* Device = reinterpret_cast<ID3D11Device*>(InDecoderOutput->GetPlatformOutputHandle(EElectraDecoderPlatformOutputHandleType::DXDevice));
				ID3D11DeviceContext* DeviceContext = reinterpret_cast<ID3D11DeviceContext*>(InDecoderOutput->GetPlatformOutputHandle(EElectraDecoderPlatformOutputHandleType::DXDeviceContext));
				if (!Device || !DeviceContext)
				{
					return false;
				}

				uint32 ViewIndex = 0;
				if (DXGIBuffer->GetSubresourceIndex(&ViewIndex) != S_OK)
				{
					return false;
				}
				DecoderOutput->InitializeWithSharedTexture(Device, DeviceContext, Texture2D, ViewIndex, FIntPoint(InDecoderOutput->GetWidth(), InDecoderOutput->GetHeight()), InOutBufferPropertes);
				if (!DecoderOutput->GetTexture())
				{
					UE_LOG(LogElectraPlayer, Error, TEXT("ID3D11Device::CreateTexture2D() failed!"));
					return false;
				}
			}
			return true;
		}
		else if (bIsSW)
		{
			//
			// "Software" case
			//
			TRefCountPtr<IMFSample> DecodedOutputSample = reinterpret_cast<IMFSample*>(InDecoderOutput->GetPlatformOutputHandle(EElectraDecoderPlatformOutputHandleType::MFSample));

			DWORD BuffersNum = 0;
			if ((Result = DecodedOutputSample->GetBufferCount(&BuffersNum)) != S_OK)
			{
				UE_LOG(LogElectraPlayer, Error, TEXT("IMFSample::GetBufferCount() failed with %08x"), Result);
				return false;
			}
			if (BuffersNum != 1)
			{
				UE_LOG(LogElectraPlayer, Error, TEXT("IMFSample::GetBufferCount() returned %u buffers instead of 1"), Result);
				return false;
			}
			TRefCountPtr<IMFMediaBuffer> Buffer;
			if ((Result = DecodedOutputSample->GetBufferByIndex(0, Buffer.GetInitReference())) != S_OK)
			{
				UE_LOG(LogElectraPlayer, Error, TEXT("IMFSample::GetBufferByIndex() failed with %08x"), Result);
				return false;
			}

			int32 Width = InDecoderOutput->GetDecodedWidth();
			int32 Height = InDecoderOutput->GetDecodedHeight();

			// With software decode we cannot query any DXGI/DirectX data types, so we query the decoder extra data...
			EElectraDecoderPlatformPixelFormat SamplePixFmt = static_cast<EElectraDecoderPlatformPixelFormat>(ElectraDecodersUtil::GetVariantValueSafeI64(ExtraValues, TEXT("pixfmt"), (int64)EElectraDecoderPlatformPixelFormat::NV12));

			EPixelFormat PixFmt = (SamplePixFmt == EElectraDecoderPlatformPixelFormat::NV12) ? EPixelFormat::PF_NV12 : EPixelFormat::PF_P010;

			InOutBufferPropertes->Set(IDecoderOutputOptionNames::PixelFormat, FVariantValue((int64)PixFmt));
			InOutBufferPropertes->Set(IDecoderOutputOptionNames::BitsPerComponent, FVariantValue((int64)((PixFmt == EPixelFormat::PF_NV12) ? 8 : 10)));

			TRefCountPtr<IMF2DBuffer> Buffer2D;
			if ((Result = Buffer->QueryInterface(__uuidof(IMF2DBuffer), (void**)Buffer2D.GetInitReference())) == S_OK)
			{
				uint8* Data = nullptr;
				LONG Pitch = 0;
				if ((Result = Buffer2D->Lock2D(&Data, &Pitch)) != S_OK)
				{
					UE_LOG(LogElectraPlayer, Error, TEXT("IMF2DBuffer::Lock2D() failed with %08x"), Result);
					return false;
				}

				InOutBufferPropertes->Set(IDecoderOutputOptionNames::Pitch, FVariantValue((int64)Pitch));

				DecoderOutput->InitializeWithBuffer(Data, Pitch * Height,
					Pitch,						// Buffer stride
					FIntPoint(Width, Height),	// Buffer dimensions
					InOutBufferPropertes);

				if ((Result = Buffer2D->Unlock2D()) != S_OK)
				{
					UE_LOG(LogElectraPlayer, Error, TEXT("IMF2DBuffer::Unlock2D() failed with %08x"), Result);
					return false;
				}
			}
			else
			{
				DWORD BufferSize = 0;
				uint8* Data = nullptr;
				if ((Result = Buffer->GetCurrentLength(&BufferSize)) != S_OK)
				{
					UE_LOG(LogElectraPlayer, Error, TEXT("IMFMediaBuffer::GetCurrentLength() failed with %08x"), Result);
					return false;
				}
				if ((Result = Buffer->Lock(&Data, NULL, NULL)) != S_OK)
				{
					UE_LOG(LogElectraPlayer, Error, TEXT("IMFMediaBuffer::Lock() failed with %08x"), Result);
					return false;
				}

				int32 Pitch = Width * ((PixFmt == EPixelFormat::PF_NV12) ? 1 : 2);

				InOutBufferPropertes->Set(IDecoderOutputOptionNames::Pitch, FVariantValue((int64)Pitch));

				DecoderOutput->InitializeWithBuffer(Data, BufferSize,
					Pitch,						// Buffer stride
					FIntPoint(Width, Height),	// Buffer dimensions
					InOutBufferPropertes);

				if ((Result = Buffer->Unlock()) != S_OK)
				{
					UE_LOG(LogElectraPlayer, Error, TEXT("IMFMediaBuffer::Unlock() failed with %08x"), Result);
					return false;
				}
			}
			return true;
		}
	}
	return false;
}


bool FElectraDecoderResourceManagerWindows::SetupRenderBufferFromDecoderOutput(IMediaRenderer::IBuffer* InOutBufferToSetup, TSharedPtr<FParamDict, ESPMode::ThreadSafe> InOutBufferPropertes, TSharedPtr<IElectraDecoderVideoOutput, ESPMode::ThreadSafe> InDecoderOutput, FElectraDecoderResourceManagerWindows::IDecoderPlatformResource* InPlatformSpecificResource)
{
	check(InOutBufferToSetup);
	check(InOutBufferPropertes.IsValid());
	check(InDecoderOutput.IsValid());

	TSharedPtr<FElectraPlayerVideoDecoderOutputPC, ESPMode::ThreadSafe> DecoderOutput = InOutBufferToSetup->GetBufferProperties().GetValue(RenderOptionKeys::Texture).GetSharedPointer<FElectraPlayerVideoDecoderOutputPC>();
	FInstanceVars* Vars = static_cast<FInstanceVars*>(InPlatformSpecificResource);
	if (DecoderOutput.IsValid())
	{
		//
		// Image buffers?
		//
		IElectraDecoderVideoOutputImageBuffers* ImageBuffers = reinterpret_cast<IElectraDecoderVideoOutputImageBuffers*>(InDecoderOutput->GetPlatformOutputHandle(EElectraDecoderPlatformOutputHandleType::ImageBuffers));
		if (ImageBuffers != nullptr)
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

			int32 Width = InDecoderOutput->GetDecodedWidth();
			int32 Height = InDecoderOutput->GetDecodedHeight();

			InOutBufferPropertes->Set(IDecoderOutputOptionNames::BitsPerComponent, FVariantValue((int64)InDecoderOutput->GetNumberOfBits()));

			int32 NumImageBuffers = ImageBuffers->GetNumberOfBuffers();
			check(NumImageBuffers == 1 || NumImageBuffers == 2);

			// Color buffer
			EElectraDecoderPlatformPixelFormat PixFmt = ImageBuffers->GetBufferFormatByIndex(0);
			EElectraDecoderPlatformPixelEncoding PixEnc = ImageBuffers->GetBufferEncodingByIndex(0);

			EPixelFormat RHIPixFmt;
			switch(PixFmt)
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
				default: DecPixEnc = EVideoDecoderPixelEncoding::Native; break;
			}
			
			int32 Pitch = ImageBuffers->GetBufferPitchByIndex(0);

			InOutBufferPropertes->Set(IDecoderOutputOptionNames::PixelFormat, FVariantValue((int64)RHIPixFmt));
			InOutBufferPropertes->Set(IDecoderOutputOptionNames::PixelEncoding, FVariantValue((int64)DecPixEnc));
			InOutBufferPropertes->Set(IDecoderOutputOptionNames::Pitch, FVariantValue((int64)Pitch));

			TMap<FString, FVariant> ExtraValues;
			InDecoderOutput->GetExtraValues(ExtraValues);

			InOutBufferPropertes->Set(IDecoderOutputOptionNames::PixelDataScale, FVariantValue((double)ElectraDecodersUtil::GetVariantValueSafeDouble(ExtraValues, TEXT("pix_datascale"), 1.0)));

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
				TRefCountPtr<IUnknown> TextureCommon(static_cast<IUnknown*>(ImageBuffers->GetBufferTextureByIndex(0)));

				if (TextureCommon.IsValid())
				{
					HRESULT Res;

					void* DDevice;
					int32 DVersion;
					check(WindowsDecoderResources::Callbacks.GetD3DDevice);
					WindowsDecoderResources::Callbacks.GetD3DDevice(&DDevice, &DVersion, WindowsDecoderResources::Callbacks.UserValue);

					if (DVersion >= 12000)
					{
						TRefCountPtr<ID3D12Device> D3D12Device(static_cast<ID3D12Device*>(DDevice));

						// Can we get a DX12 texture? (this will only work if the transform is associated with a DX12 device earlier on; hence no need for a SDK version guard here)
						TRefCountPtr<ID3D12Resource> Resource;
						Res = TextureCommon->QueryInterface(__uuidof(ID3D12Resource), (void**)Resource.GetInitReference());
						if (Res == S_OK)
						{
							//
							// DX12 texture / buffer
							//

							// We might also have a fence / sync we need to use before accessing the data on the texture...
							FElectraDecoderOutputSync OutputSync;
							ImageBuffers->GetBufferTextureSyncByIndex(0, OutputSync);

							DecoderOutput->InitializeWithResource(D3D12Device, Resource, Pitch, OutputSync, FIntPoint(InDecoderOutput->GetDecodedWidth(), InDecoderOutput->GetDecodedHeight()), InOutBufferPropertes, Vars->VideoDecoderResourceDelegate,
																  Vars->D3D12ResourcePool, Vars->MaxWidth, Vars->MaxHeight, Vars->MaxOutputBuffers);
							return true;
						}
					}

					TRefCountPtr<ID3D11Texture2D> Texture;
					Res = TextureCommon->QueryInterface(__uuidof(ID3D11Texture2D), (void**)Texture.GetInitReference());
					if (Res == S_OK)
					{
						//
						// DX11 texture
						//
						ID3D11Device* Device = reinterpret_cast<ID3D11Device*>(InDecoderOutput->GetPlatformOutputHandle(EElectraDecoderPlatformOutputHandleType::DXDevice));
						ID3D11DeviceContext* DeviceContext = reinterpret_cast<ID3D11DeviceContext*>(InDecoderOutput->GetPlatformOutputHandle(EElectraDecoderPlatformOutputHandleType::DXDeviceContext));
						if (Device && DeviceContext)
						{
							uint32 ViewIndex = 0;
							DecoderOutput->InitializeWithSharedTexture(Device, DeviceContext, Texture.GetReference(), ViewIndex, FIntPoint(InDecoderOutput->GetWidth(), InDecoderOutput->GetHeight()), InOutBufferPropertes);
							return true;
						}
					}
				}
			}
		}
		else
		{
			//
			// IMF sample?
			//
			if (InDecoderOutput->GetPlatformOutputHandle(EElectraDecoderPlatformOutputHandleType::MFSample) != nullptr)
			{
				return SetupRenderBufferFromDecoderOutputFromMFSample(InOutBufferToSetup, InOutBufferPropertes, InDecoderOutput, InPlatformSpecificResource);
			}
		}
	}
	return false;
}


} // namespace Electra

