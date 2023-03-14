// Copyright Epic Games, Inc. All Rights Reserved.

#include "PlayerCore.h"
#include "ElectraPlayerPrivate.h"
#include "ElectraPlayerPrivate_Platform.h"
#include "StreamAccessUnitBuffer.h"
#include "Decoder/VideoDecoderH264.h"
#include "Renderer/RendererBase.h"
#include "Renderer/RendererVideo.h"
#include "Player/PlayerSessionServices.h"
#include "Utilities/StringHelpers.h"

#include "Decoder/DX/DecoderErrors_DX.h"
#include "MediaVideoDecoderOutputPC.h"

/*********************************************************************************************************************/
/*********************************************************************************************************************/
/*********************************************************************************************************************/

#include "ElectraVideoDecoder_PC.h"

#include "Decoder/DX/MediaFoundationGUIDs.h"
#include "Decoder/DX/VideoDecoderH264_DX.h"


#define ELECTRA_USE_IMF2DBUFFER	1		// set to 1 to use IMF2DBuffer interface rather then plain media buffer when retrieving HW decoded data for CPU use



const D3DFORMAT DX9_NV12_FORMAT = (D3DFORMAT)MAKEFOURCC('N', 'V', '1', '2');

/*********************************************************************************************************************/
/*********************************************************************************************************************/
/*********************************************************************************************************************/

#ifdef ELECTRA_ENABLE_SWDECODE
static TAutoConsoleVariable<int32> CVarElectraPCUseSoftwareDecoding(
	TEXT("Electra.PC.UseSoftwareDecoding"),
	0,
	TEXT("Use software decoding on PC even if hardware decoding is supported.\n")
	TEXT(" 0: use hardware decoding if supported (default); 1: use software decoding."),
	ECVF_Default);
#endif



namespace Electra
{

/*********************************************************************************************************************/
/*********************************************************************************************************************/
/*********************************************************************************************************************/

class FVideoDecoderH264_PC : public FVideoDecoderH264
{
public:
	enum
	{
		MaxHWDecodeW = 1920,
		MaxHWDecodeH = 1088
	};
private:
	virtual bool InternalDecoderCreate() override;
	virtual bool CreateDecoderOutputBuffer() override;
	virtual void PreInitDecodeOutputForSW(const FIntPoint& Dim);
	virtual bool SetupDecodeOutputData(const FIntPoint& ImageDim, const TRefCountPtr<IMFSample>& DecodedOutputSample, FParamDict* OutputBufferSampleProperties) override;

	bool CopyTexture(const TRefCountPtr<IMFSample>& DecodedSample, Electra::FParamDict* ParamDict, FIntPoint OutputDim);
};


/***************************************************************************************************************************************************/
/***************************************************************************************************************************************************/
/***************************************************************************************************************************************************/

IVideoDecoderH264* IVideoDecoderH264::Create()
{
	return new FVideoDecoderH264_PC();
}

//-----------------------------------------------------------------------------
/**
 *	Queries decoder support/capability for a stream with given properties.
 *	Can be called after Startup() but should be called only shortly before playback to ensure all relevant
 *	libraries are initialized.
 */
bool FVideoDecoderH264::GetStreamDecodeCapability(FStreamDecodeCapability& OutResult, const FStreamDecodeCapability& InStreamParameter)
{
	// Set the output same as the input. Modifications are made below.
	OutResult = InStreamParameter;
	OutResult.DecoderSupportType = FStreamDecodeCapability::ESupported::NotSupported;

#ifdef ELECTRA_ENABLE_SWDECODE
	// Software forced through CVar?
	if (CVarElectraPCUseSoftwareDecoding.GetValueOnAnyThread() > 0)
	{
		bIsHWSupported = false;
	}
	else
#endif
	{
		// Did we check the actual decoder for being HW supported yet?
		if (!bDidCheckHWSupport)
		{
			// Check for HW support by creating and configuring a decoder.
			FVideoDecoderH264* TestDecoder = new FVideoDecoderH264_PC;
			// Set width and height in the decoder configuration. These are used in creating the decoder.
			TestDecoder->Config.MaxFrameWidth = InStreamParameter.Width ? InStreamParameter.Width : FVideoDecoderH264_PC::MaxHWDecodeW;
			TestDecoder->Config.MaxFrameHeight = InStreamParameter.Height ? InStreamParameter.Height : FVideoDecoderH264_PC::MaxHWDecodeH;
			TestDecoder->TestHardwareDecoding();
			bIsHWSupported = TestDecoder->bIsHardwareAccelerated;
			delete TestDecoder;
			bDidCheckHWSupport = true;
		}
	}
	if (bIsHWSupported)
	{
		// FIXME: Should we check the resolution or profile level here?
		//        Right now we won't be going higher than 1080 so we should be fine.
		OutResult.DecoderSupportType = FStreamDecodeCapability::ESupported::HardwareOnly;
	}
	else
	{
		// Global query?
		if (InStreamParameter.Width <= 0 && InStreamParameter.Height <= 0)
		{
			// For the sake of argument we limit SW decoding to 720p@60.
			OutResult.Width = 1280;
			OutResult.Height = 720;
			OutResult.Profile = 100;
			OutResult.Level = 42;
			OutResult.FPS = 60.0;
			OutResult.DecoderSupportType = FStreamDecodeCapability::ESupported::SoftwareOnly;
		}
		else
		{
			// Calculate via number of macroblocks per second.
			int32 maxMB = (1280 / 16) * (720 / 16) * 60;
			int32 numMB = (int32) (((InStreamParameter.Width + 15) / 16) * ((InStreamParameter.Height + 15) / 16) * (InStreamParameter.FPS > 0.0 ? InStreamParameter.FPS : 30.0));
			OutResult.DecoderSupportType = numMB <= maxMB ? FStreamDecodeCapability::ESupported::SoftwareOnly : FStreamDecodeCapability::ESupported::NotSupported;
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
/**
 * Create a decoder instance.
 *
 * @return true if successful, false on error
 */
bool FVideoDecoderH264_PC::InternalDecoderCreate()
{
	TRefCountPtr<IMFAttributes>	Attributes;
	TRefCountPtr<IMFTransform>	Decoder;
	HRESULT					res;

	if (Electra::IsWindows8Plus())
	{
		// Check if there is any reason for a "device lost" - if not we know all is stil well; otherwise we bail without creating a decoder
		if (Electra::FDXDeviceInfo::s_DXDeviceInfo->DxDevice && (res = Electra::FDXDeviceInfo::s_DXDeviceInfo->DxDevice->GetDeviceRemovedReason()) != S_OK)
		{
			PostError(res, "Device lost detected.", ERRCODE_INTERNAL_COULD_NOT_SET_OUTPUT_DESIRED_MEDIA_TYPE);
			return false;
		}
	}

	VERIFY_HR(CoCreateInstance(MFTmsH264Decoder, nullptr, CLSCTX_INPROC_SERVER, IID_IMFTransform, reinterpret_cast<void**>(&Decoder)), "CoCreateInstance failed", ERRCODE_INTERNAL_COULD_NOT_CREATE_DECODER);
	VERIFY_HR(Decoder->GetAttributes(Attributes.GetInitReference()), "Failed to get video decoder transform attributes", ERRCODE_INTERNAL_COULD_GET_TRANSFORM_ATTRIBUTES);

	// Force SW decoding?
#ifdef ELECTRA_ENABLE_SWDECODE
	if (CVarElectraPCUseSoftwareDecoding.GetValueOnAnyThread() > 0)
	{
		FallbackToSwDecoding(FString::Printf(TEXT("Windows %s"), *FWindowsPlatformMisc::GetOSVersion()));
	}
	else
#endif
	{
		// Check if the transform is D3D aware
		if (Electra::IsWindows8Plus())
		{
			if (!Electra::FDXDeviceInfo::s_DXDeviceInfo->DxDeviceManager)
			{
				// Win8+ with no DX11. No need to check for any DX11 awareness...
				return true;
			}

			uint32 IsDX11Aware = 0;
			res = Attributes->GetUINT32(MF_SA_D3D11_AWARE, &IsDX11Aware);
			if (FAILED(res))
			{
				FallbackToSwDecoding(TEXT("Failed to get MF_SA_D3D11_AWARE"));
			}
			else if (IsDX11Aware == 0)
			{
				FallbackToSwDecoding(TEXT("Not MF_SA_D3D11_AWARE"));
			}
			else if (FAILED(res = Decoder->ProcessMessage(MFT_MESSAGE_SET_D3D_MANAGER, reinterpret_cast<ULONG_PTR>(Electra::FDXDeviceInfo::s_DXDeviceInfo->DxDeviceManager.GetReference()))))
			{
				FallbackToSwDecoding(FString::Printf(TEXT("Failed to set MFT_MESSAGE_SET_D3D_MANAGER: 0x%X %s"), res, *GetComErrorDescription(res)));
			}
		}
		else // Windows 7
		{
#ifdef ELECTRA_HAVE_DX9
			if (!Electra::FDXDeviceInfo::s_DXDeviceInfo->Dx9Device || !Electra::FDXDeviceInfo::s_DXDeviceInfo->Dx9DeviceManager)
			{
				FallbackToSwDecoding(TEXT("Failed to create DirectX 9 device / device manager"));
			}

			uint32 IsD3DAware = 0;
			HRESULT HRes = Attributes->GetUINT32(MF_SA_D3D_AWARE, &IsD3DAware);
			if (FAILED(HRes))
			{
				FallbackToSwDecoding(TEXT("Failed to get MF_SA_D3D_AWARE"));
			}
			else if (IsD3DAware == 0)
			{
				FallbackToSwDecoding(TEXT("Not MF_SA_D3D_AWARE"));
			}
			else if (FAILED(HRes = Decoder->ProcessMessage(MFT_MESSAGE_SET_D3D_MANAGER, reinterpret_cast<ULONG_PTR>(Electra::FDXDeviceInfo::s_DXDeviceInfo->Dx9DeviceManager.GetReference()))))
			{
				FallbackToSwDecoding(FString::Printf(TEXT("Failed to set MFT_MESSAGE_SET_D3D_MANAGER: 0x%X %s"), HRes, *GetComErrorDescription(HRes)));
			}
#else
			return false;
#endif
		}
	}

	// Sugar & spice
	if (FAILED(res = Attributes->SetUINT32(CODECAPI_AVLowLatencyMode, 1)))
	{
		// Not an error. If it doesn't work it just doesn't.
	}

	/*
	if (FAILED(res = Attributes->SetUINT32(AVDecVideoMaxCodedWidth, 1280 + 64)))
		return false;
	if (FAILED(res = Attributes->SetUINT32(AVDecVideoMaxCodedHeight, 720 + 64)))
		return false;
	*/

	// Create successful, take on the decoder.
	DecoderTransform = Decoder;

	return true;
}


bool FVideoDecoderH264_PC::CreateDecoderOutputBuffer()
{
	HRESULT									res;
	TUniquePtr<FDecoderOutputBuffer>		NewDecoderOutputBuffer(new FDecoderOutputBuffer());

	VERIFY_HR(DecoderTransform->GetOutputStreamInfo(0, &NewDecoderOutputBuffer->mOutputStreamInfo), "Failed to get video decoder output stream information", ERRCODE_INTERNAL_COULD_NOT_GET_OUTPUT_STREAM_INFO);
	// Do we need to provide the sample output buffer or does the decoder create it for us?
	if ((NewDecoderOutputBuffer->mOutputStreamInfo.dwFlags & (MFT_OUTPUT_STREAM_PROVIDES_SAMPLES | MFT_OUTPUT_STREAM_CAN_PROVIDE_SAMPLES)) == 0)
	{
#if ELECTRA_ENABLE_SWDECODE
		check(bIsHardwareAccelerated == false);
		if (CurrentRenderOutputBuffer)
		{
			TSharedPtr<FElectraPlayerVideoDecoderOutputPC, ESPMode::ThreadSafe> DecoderOutput = CurrentRenderOutputBuffer->GetBufferProperties().GetValue("texture").GetSharedPointer<FElectraPlayerVideoDecoderOutputPC>();
			check(DecoderOutput);
			// Get the sample which was created earlier
			NewDecoderOutputBuffer->mOutputBuffer.pSample = DecoderOutput->GetMFSample();
			// Destination is a plain old pointer, we need to ref manually
			if (NewDecoderOutputBuffer->mOutputBuffer.pSample)
			{
				NewDecoderOutputBuffer->mOutputBuffer.pSample->AddRef();
			}
			else
			{
				return false;
			}
		}
		else
#endif
		{
			return false;
		}
	}
	CurrentDecoderOutputBuffer = MoveTemp(NewDecoderOutputBuffer);
	return true;
}


void FVideoDecoderH264_PC::PreInitDecodeOutputForSW(const FIntPoint& Dim)
{
	TSharedPtr<FElectraPlayerVideoDecoderOutputPC, ESPMode::ThreadSafe> DecoderOutput = CurrentRenderOutputBuffer->GetBufferProperties().GetValue("texture").GetSharedPointer<FElectraPlayerVideoDecoderOutputPC>();
	check(DecoderOutput);

	if (DecoderTransform.IsValid())
	{
		MFT_OUTPUT_STREAM_INFO	OutputStreamInfo;
		HRESULT Result = DecoderTransform->GetOutputStreamInfo(0, &OutputStreamInfo);
		// Do we need to provide the sample output buffer or does the decoder create it for us?
		if (SUCCEEDED(Result) && (OutputStreamInfo.dwFlags & (MFT_OUTPUT_STREAM_PROVIDES_SAMPLES | MFT_OUTPUT_STREAM_CAN_PROVIDE_SAMPLES)) == 0)
		{
			DecoderOutput->SetSWDecodeTargetBufferSize(OutputStreamInfo.cbSize);
		}
	}

	DecoderOutput->PreInitForDecode(Dim, [this](int32 ApiReturnValue, const FString& Message, uint16 Code, UEMediaError Error) { PostError(ApiReturnValue, Message, Code, Error); });
}


bool FVideoDecoderH264_PC::SetupDecodeOutputData(const FIntPoint & ImageDim, const TRefCountPtr<IMFSample>& DecodedOutputSample, FParamDict* OutputBufferSampleProperties)
{
#ifdef ELECTRA_ENABLE_SWDECODE
	if (!bIsHardwareAccelerated)
	{
		TSharedPtr<FElectraPlayerVideoDecoderOutputPC, ESPMode::ThreadSafe> DecoderOutput = CurrentRenderOutputBuffer->GetBufferProperties().GetValue("texture").GetSharedPointer<FElectraPlayerVideoDecoderOutputPC>();
		check(DecoderOutput);
		DecoderOutput->ProcessDecodeOutput(ImageDim, OutputBufferSampleProperties);
	}
	else
#endif
	{
		bool bCopyResult = CopyTexture(DecodedOutputSample, OutputBufferSampleProperties, ImageDim);
		if (!bCopyResult)
		{
			// Failed for some reason. Let's best not render this then, but put in the buffers nonetheless to maintain the normal data flow.
			OutputBufferSampleProperties->Set("is_dummy", FVariantValue(true));
		}
	}
	return true;
}


bool FVideoDecoderH264_PC::CopyTexture(const TRefCountPtr<IMFSample>& Sample, Electra::FParamDict* ParamDict, FIntPoint OutputDim)
{
#if !UE_SERVER
	TSharedPtr<FElectraPlayerVideoDecoderOutputPC, ESPMode::ThreadSafe> DecoderOutput = CurrentRenderOutputBuffer->GetBufferProperties().GetValue("texture").GetSharedPointer<FElectraPlayerVideoDecoderOutputPC>();
	check(DecoderOutput);

	DWORD BuffersNum = 0;
	CHECK_HR(Sample->GetBufferCount(&BuffersNum));

	if (BuffersNum != 1)
	{
		return false;
	}

	if (FDXDeviceInfo::s_DXDeviceInfo->DxVersion == FDXDeviceInfo::ED3DVersion::Version11Win8)
	{
		check(FDXDeviceInfo::s_DXDeviceInfo->DxDevice);

		DecoderOutput->InitializeWithSharedTexture(
			FDXDeviceInfo::s_DXDeviceInfo->DxDevice,
			FDXDeviceInfo::s_DXDeviceInfo->DxDeviceContext,
			Sample,
			OutputDim,
			ParamDict);

		if (!DecoderOutput->GetTexture())
		{
			UE_LOG(LogElectraPlayer, Error, TEXT("FVideoDecoderH264: InD3D11Device->CreateTexture2D() failed!"));
			return false;
		}
	}
	else if (FDXDeviceInfo::s_DXDeviceInfo->DxVersion == FDXDeviceInfo::ED3DVersion::Version12Win10)
	{
		TRefCountPtr<IMFMediaBuffer> Buffer;
		CHECK_HR(Sample->GetBufferByIndex(0, Buffer.GetInitReference()));

		// Gather some info from the DX11 texture in the buffer...
		TRefCountPtr<IMFDXGIBuffer> DXGIBuffer;
		CHECK_HR(Buffer->QueryInterface(__uuidof(IMFDXGIBuffer), (void**)DXGIBuffer.GetInitReference()));

		TRefCountPtr<ID3D11Texture2D> Texture2D;
		CHECK_HR(DXGIBuffer->GetResource(IID_PPV_ARGS(Texture2D.GetInitReference())));
		D3D11_TEXTURE2D_DESC TextureDesc;
		Texture2D->GetDesc(&TextureDesc);
		if (TextureDesc.Format != DXGI_FORMAT_NV12)
		{
			LogMessage(IInfoLog::ELevel::Error, "FVideoDecoderH264::CopyTexture(): Decoded texture is not in NV12 format");
			return false;
		}

		// Retrieve frame data and store it in Buffer for rendering later
#if ELECTRA_USE_IMF2DBUFFER
		TRefCountPtr<IMF2DBuffer> Buffer2D;
		CHECK_HR(Buffer->QueryInterface(__uuidof(IMF2DBuffer), (void**)Buffer2D.GetInitReference()));

		uint8* Data = nullptr;
		LONG Pitch;
		CHECK_HR(Buffer2D->Lock2D(&Data, &Pitch));

		DecoderOutput->InitializeWithBuffer(Data, Pitch * (TextureDesc.Height * 3 / 2),
			Pitch,															// Buffer stride
			FIntPoint(TextureDesc.Width, TextureDesc.Height * 3 / 2),		// Buffer dimensions
			ParamDict);

		CHECK_HR(Buffer2D->Unlock2D());
#else
		DWORD BufferSize = 0;
		CHECK_HR(Buffer->GetCurrentLength(&BufferSize));
		// Not really trusting the size here. Seen crash reports where the returned buffer size is larger than what is contained,
		// as if the texture was wider (a multiple of 256 actually). The crash then happens on the memcpy() trying to _read_ more
		// data than was actually mapped to the buffer (access violation on exactly the start of the next page (4096 byte aligned)).
		// https://docs.microsoft.com/en-us/windows/win32/api/dxgiformat/ne-dxgiformat-dxgi_format stipulates the format to be aligned
		// to 'rowPitch' (whatever that is) and 'SysMemPitch' (https://docs.microsoft.com/en-us/windows/win32/api/d3d11/ns-d3d11-d3d11_subresource_data)
		// which as far as I can tell is a property set when the texture was created. Which we have NO control over since the video decoder creates this
		// texture for us. There does not seem to be any API to query this pitch at all.
		// NV12 format as described here https://docs.microsoft.com/en-us/windows/win32/medfound/recommended-8-bit-yuv-formats-for-video-rendering
		// does not suggest there to be any padding/alignment/pitch.
		// What makes the crashes so weird is that they do not happen all the time right on the first frame. It takes a bit before wrong values are returned.
		// For instance, for a 1152*656 texture 1259520 (1280*656) is returned when really it should be only 1133568,
		// or for a 864*480 texture instead of 622080 bytes we are told there'd be 737280 bytes (1024*480).
		// Since we are not using any "actual pitch" in the Initialize() call but the actual width of the texture we must only copy as much data anyway.
		// Even if there was padding in the texture that we would be copying over into our buffer it would not be skipped on rendering later on account
		// of us specifying the width for pitch.
		// It seems prudent at this point not to trust the returned buffer size but to calculate it from the actual dimensions ourselves.
		// NOTE: this value is probably dependent on the GPU and driver implementing the decoder and may therefore not be a general issue!
		BufferSize = TextureDesc.Width * (TextureDesc.Height * 3 / 2);

		uint8* Data = nullptr;
		CHECK_HR(Buffer->Lock(&Data, NULL, NULL));

		DecoderOutput->InitializeWithBuffer(Data, BufferSize,
			TextureDesc.Width,												// Buffer stride
			FIntPoint(TextureDesc.Width, TextureDesc.Height * 3 / 2),		// Buffer dimensions
			ParamDict);

		CHECK_HR(Buffer->Unlock());
#endif

		return true;
	}
#ifdef ELECTRA_HAVE_DX9
	else if (FDXDeviceInfo::s_DXDeviceInfo->DxVersion == FDXDeviceInfo::ED3DVersion::Version9Win7)
	{
		TRefCountPtr<IMFMediaBuffer> Buffer;
		CHECK_HR(Sample->GetBufferByIndex(0, Buffer.GetInitReference()));

		TRefCountPtr<IDirect3DSurface9> Dx9DecoderSurface;
		TRefCountPtr<IMFGetService> BufferService;
		CHECK_HR(Buffer->QueryInterface(IID_PPV_ARGS(BufferService.GetInitReference())));
		CHECK_HR(BufferService->GetService(MR_BUFFER_SERVICE, IID_PPV_ARGS(Dx9DecoderSurface.GetInitReference())));

		D3DSURFACE_DESC Dx9SurfaceDesc;
		CHECK_HR_DX9(Dx9DecoderSurface->GetDesc(&Dx9SurfaceDesc));
		check(Dx9SurfaceDesc.Format == DX9_NV12_FORMAT);

		if (Dx9SurfaceDesc.Format != DX9_NV12_FORMAT)
		{
			LogMessage(IInfoLog::ELevel::Error, "FVideoDecoderH264::CopyTexture(): Decoded DX9 surface is not in NV12 format");
		}

		// Read back DX9 surface data and pass it onto the texture sample
		D3DLOCKED_RECT Dx9DecoderTexLockedRect;
		CHECK_HR_DX9(Dx9DecoderSurface->LockRect(&Dx9DecoderTexLockedRect, nullptr, D3DLOCK_READONLY));
		check(Dx9DecoderTexLockedRect.pBits && Dx9DecoderTexLockedRect.Pitch > 0);

		// note: 3/2 scale is to account for NV12 data format (Y+UV section)
		DecoderOutput->InitializeWithBuffer(
			Dx9DecoderTexLockedRect.pBits,									// Buffer ptr
			Dx9DecoderTexLockedRect.Pitch * Dx9SurfaceDesc.Height * 3 / 2,	// Buffer size
			Dx9DecoderTexLockedRect.Pitch,									// Buffer stride
			FIntPoint(Dx9DecoderTexLockedRect.Pitch, Dx9SurfaceDesc.Height * 3 / 2), // Buffer dimensions
			ParamDict);

		CHECK_HR_DX9(Dx9DecoderSurface->UnlockRect());
	}
#endif
	else
	{
		LogMessage(IInfoLog::ELevel::Error, "FVideoDecoderH264::CopyTexture(): Unhandled D3D version");
		return false;
	}
	return true;
#else
	return false;
#endif
}

} // namespace Electra

