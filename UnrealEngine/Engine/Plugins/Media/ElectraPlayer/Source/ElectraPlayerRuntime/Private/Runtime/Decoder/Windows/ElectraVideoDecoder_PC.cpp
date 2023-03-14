// Copyright Epic Games, Inc. All Rights Reserved.

#include "ElectraVideoDecoder_PC.h"
#include "Decoder/DX/DecoderErrors_DX.h"
#include "Renderer/RendererVideo.h"

FElectraPlayerVideoDecoderOutputPC::FElectraPlayerVideoDecoderOutputPC()
	: OutputType(EOutputType::Unknown)
	, Stride(0)
	, SampleDim(0,0)
{
}

void FElectraPlayerVideoDecoderOutputPC::SetSWDecodeTargetBufferSize(uint32 InTargetBufferSize)
{
	TargetBufferAllocSize = InTargetBufferSize;
}

void FElectraPlayerVideoDecoderOutputPC::SetOwner(const TSharedPtr<IDecoderOutputOwner, ESPMode::ThreadSafe>& InOwningRenderer)
{
	OwningRenderer = InOwningRenderer;
}

FElectraPlayerVideoDecoderOutputPC::EOutputType FElectraPlayerVideoDecoderOutputPC::GetOutputType() const
{
	return OutputType;
}

TRefCountPtr<IMFSample> FElectraPlayerVideoDecoderOutputPC::GetMFSample() const
{
	check(OutputType == EOutputType::SoftwareWin8Plus || OutputType == EOutputType::SoftwareWin7);
	return MFSample;
}

const TArray<uint8>& FElectraPlayerVideoDecoderOutputPC::GetBuffer() const
{
	check(OutputType == EOutputType::SoftwareWin8Plus || OutputType == EOutputType::SoftwareWin7 || OutputType == EOutputType::HardwareDX9_DX12);
	return Buffer;
}

uint32 FElectraPlayerVideoDecoderOutputPC::GetStride() const
{
	return Stride;
}

TRefCountPtr<ID3D11Texture2D> FElectraPlayerVideoDecoderOutputPC::GetTexture() const
{
	return Texture;
}

TRefCountPtr<ID3D11Device> FElectraPlayerVideoDecoderOutputPC::GetDevice() const
{
	check(OutputType == EOutputType::SoftwareWin8Plus || OutputType == EOutputType::HardwareWin8Plus);
	return D3D11Device;
}

FIntPoint FElectraPlayerVideoDecoderOutputPC::GetDim() const
{
	return SampleDim;
}

void FElectraPlayerVideoDecoderOutputPC::PreInitForDecode(FIntPoint OutputDim, const TFunction<void(int32 /*ApiReturnValue*/, const FString& /*Message*/, uint16 /*Code*/, UEMediaError /*Error*/ )>& PostError)
{
	FIntPoint Dim;
	Dim.X = (OutputDim.X + 15) & ~15;
	Dim.Y = ((OutputDim.Y + 15) & ~15) * 3/2;	// adjust height to encompass Y and UV planes

	bool bNeedNew = !MFSample.IsValid() || SampleDim != Dim;

	if (bNeedNew)
	{
		HRESULT Result;
		MFSample = nullptr;
		Texture = nullptr;
		TRefCountPtr<IMFMediaBuffer> MediaBuffer;

		SampleDim = Dim;

		// Windows 8+ and a valid DX11 rendering device?
		// (Rendering device will be null if the rendering device is not DX11 (DX12, Vulkan...))
		if (Electra::IsWindows8Plus() && Electra::FDXDeviceInfo::s_DXDeviceInfo->RenderingDx11Device)
		{
			// Software decode into DX11 texture

			// We SW decode right into a DX11 texture (via the media buffer created from it below)
			/*
				As we cannot use RHI at this point, we cannot create a texture RHI can readily use,
				but can just generate a plain D3D one. We cannot easily use a delegate to ask UE to do
				it for use as we want to keep the "how" of the decoder hidden behind the abstract interfaces
				so we can also encapsulate non-UE-aware systems easily.

				So: we will later need to copy this once. (TODO: see if the render team can provide a way RHI could use
				a native texture like this directly)
			*/
			D3D11_TEXTURE2D_DESC TextureDesc;
			TextureDesc.Width = Dim.X;
			TextureDesc.Height = Dim.Y;
			TextureDesc.MipLevels = 1;
			TextureDesc.ArraySize = 1;
			TextureDesc.Format = DXGI_FORMAT_R8_UINT;
			TextureDesc.SampleDesc.Count = 1;
			TextureDesc.SampleDesc.Quality = 0;
			TextureDesc.Usage = D3D11_USAGE_DEFAULT;
			TextureDesc.BindFlags = 0;
			TextureDesc.CPUAccessFlags = 0;
			TextureDesc.MiscFlags = 0;

			if (FAILED(Result = Electra::FDXDeviceInfo::s_DXDeviceInfo->RenderingDx11Device->CreateTexture2D(&TextureDesc, nullptr, Texture.GetInitReference())))
			{
				UE_LOG(LogElectraPlayer, Error, TEXT("CreateTexture2D() failed with 0x%X %s"), Result, *GetComErrorDescription(Result));
				PostError(Result, "Failed to create software decode output texture", ERRCODE_INTERNAL_COULD_NOT_CREATE_OUTPUTBUFFER, UEMEDIA_ERROR_OK);
				return;
			}
			if (FAILED(Result = MFCreateDXGISurfaceBuffer(__uuidof(ID3D11Texture2D), Texture, 0, false, MediaBuffer.GetInitReference())))
			{
				UE_LOG(LogElectraPlayer, Error, TEXT("MFCreateDXGISurfaceBuffer() failed with 0x%X %s"), Result, *GetComErrorDescription(Result));
				PostError(Result, "Failed to create software decode output texture", ERRCODE_INTERNAL_COULD_NOT_CREATE_OUTPUTBUFFER, UEMEDIA_ERROR_OK);
				return;
			}

			OutputType = EOutputType::SoftwareWin8Plus;
		}
		else // Software decode into CPU buffer
		{
			DWORD AllocSize;

			if (TargetBufferAllocSize.IsSet())
			{
				AllocSize = TargetBufferAllocSize.GetValue();
			}
			else
			{
				AllocSize = Dim.X * Dim.Y * sizeof(uint8);
			}

			// SW decode results are just delivered in a simple CPU-side buffer. Create the decoder side version of this...
			if (FAILED(Result = MFCreateMemoryBuffer(AllocSize, MediaBuffer.GetInitReference())))
			{
				UE_LOG(LogElectraPlayer, Error, TEXT("MFCreateMemoryBuffer() failed with 0x%X %s"), Result, *GetComErrorDescription(Result));
				PostError(Result, "Failed to create software decode output buffer", ERRCODE_INTERNAL_COULD_NOT_CREATE_OUTPUTBUFFER, UEMEDIA_ERROR_OK);
				return;
			}
			OutputType = EOutputType::SoftwareWin7;
		}

		if (SUCCEEDED(Result = MFCreateSample(MFSample.GetInitReference())))
		{
			if (FAILED(Result = MFSample->AddBuffer(MediaBuffer)))
			{
				UE_LOG(LogElectraPlayer, Error, TEXT("IMFSample::AddBuffer() failed with 0x%X %s"), Result, *GetComErrorDescription(Result));
				PostError(Result, "Failed to add output buffer to software decoder sample", ERRCODE_INTERNAL_COULD_NOT_ADD_OUTPUT_BUFFER_TO_SAMPLE, UEMEDIA_ERROR_OK);
				return;
			}
		}
		else
		{
			UE_LOG(LogElectraPlayer, Error, TEXT("MFCreateSample() failed with 0x%X %s"), Result, *GetComErrorDescription(Result));
			PostError(Result, "Failed to create software decode output sample", ERRCODE_INTERNAL_COULD_NOT_CREATE_OUTPUT_SAMPLE, UEMEDIA_ERROR_OK);
			return;
		}
	}
}


void FElectraPlayerVideoDecoderOutputPC::ProcessDecodeOutput(FIntPoint OutputDim, Electra::FParamDict* InParamDict)
{
	check(OutputType == EOutputType::SoftwareWin7 || OutputType == EOutputType::SoftwareWin8Plus);

	FVideoDecoderOutput::Initialize(InParamDict);

	// This needs to be a multiple of 16
	SampleDim.X = (OutputDim.X + 15) & ~15;
	SampleDim.Y = ((OutputDim.Y + 15) & ~15) * 3 / 2;

	Stride = SampleDim.X * sizeof(uint8);

	if (OutputType == EOutputType::SoftwareWin7) // Win7 & DX12 (SW)
	{
		// Retrieve frame data and store it in Buffer for rendering later
		TRefCountPtr<IMFMediaBuffer> MediaBuffer;
		if (MFSample->GetBufferByIndex(0, MediaBuffer.GetInitReference()) != S_OK)
		{
			return;
		}
		DWORD BufferSize = 0;
		if (MediaBuffer->GetCurrentLength(&BufferSize) != S_OK)
		{
			return;
		}
		check(SampleDim.X * SampleDim.Y <= (int32)BufferSize);

		// MediaBuffer reports incorrect buffer sizes (too large) for some resolutions: we use our computed values!
		BufferSize = SampleDim.X * SampleDim.Y;

		uint8* Data = nullptr;
		Buffer.Reset(BufferSize);
		if (MediaBuffer->Lock(&Data, NULL, NULL) == S_OK)
		{
			Buffer.Append(Data, BufferSize);
			MediaBuffer->Unlock();
		}

		// The output IMFSample needs to be released (and recreated for the next frame) for unknown reason.
		// If not destroyed decoder throws an unknown error later on.
		MFSample = nullptr;
	}
	else
	{
		// SW decode into texture need Win8+ (but no additional processing)
		check(Electra::IsWindows8Plus());
	}
}


void FElectraPlayerVideoDecoderOutputPC::InitializeWithBuffer(const void* InBuffer, uint32 InSize, uint32 InStride, FIntPoint Dim, Electra::FParamDict* InParamDict)
{
	FVideoDecoderOutput::Initialize(InParamDict);

	OutputType = EOutputType::HardwareDX9_DX12;

	Buffer.Reset(InSize);
	Buffer.Append((uint8*)InBuffer, InSize);

	SampleDim = Dim;
	Stride = InStride;
}


void FElectraPlayerVideoDecoderOutputPC::InitializeWithSharedTexture(const TRefCountPtr<ID3D11Device>& InD3D11Device, const TRefCountPtr<ID3D11DeviceContext> InDeviceContext, const TRefCountPtr<IMFSample>& InMFSample, const FIntPoint& OutputDim, Electra::FParamDict* InParamDict)
{
	FVideoDecoderOutput::Initialize(InParamDict);

	OutputType = EOutputType::HardwareWin8Plus;

	bool bNeedsNew = !Texture.IsValid() || (SampleDim.X != OutputDim.X || SampleDim.Y != OutputDim.Y);

	if (bNeedsNew)
	{
		SampleDim = OutputDim;

		// Make a texture we pass on as output
		D3D11_TEXTURE2D_DESC TextureDesc;
		TextureDesc.Width = SampleDim.X;
		TextureDesc.Height = SampleDim.Y;
		TextureDesc.MipLevels = 1;
		TextureDesc.ArraySize = 1;
		TextureDesc.Format = DXGI_FORMAT_NV12;
		TextureDesc.SampleDesc.Count = 1;
		TextureDesc.SampleDesc.Quality = 0;
		TextureDesc.Usage = D3D11_USAGE_DEFAULT;
		TextureDesc.BindFlags = 0;
		TextureDesc.CPUAccessFlags = 0;
		TextureDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

		if (InD3D11Device->CreateTexture2D(&TextureDesc, nullptr, Texture.GetInitReference()) != S_OK)
		{
			return;
		}

		D3D11Device = InD3D11Device;
	}

	// If we got a texture, copy the data from the decoder into it...
	if (Texture)
	{
		// Get output texture from decoder...
		TRefCountPtr<IMFMediaBuffer> MediaBuffer;
		if (InMFSample->GetBufferByIndex(0, MediaBuffer.GetInitReference()) != S_OK)
		{
			return;
		}
		TRefCountPtr<IMFDXGIBuffer> DXGIBuffer;
		if (MediaBuffer->QueryInterface(__uuidof(IMFDXGIBuffer), (void**)DXGIBuffer.GetInitReference()) != S_OK)
		{
			return;
		}
		TRefCountPtr<ID3D11Texture2D> DecoderTexture;
		if (DXGIBuffer->GetResource(IID_PPV_ARGS(DecoderTexture.GetInitReference())) != S_OK)
		{
			return;
		}
		uint32 ViewIndex = 0;
		if (DXGIBuffer->GetSubresourceIndex(&ViewIndex) != S_OK)
		{
			return;
		}

		// Source data may be larger than desired output, but crop area will be aligned such that we can always copy from 0,0
		D3D11_BOX SrcBox;
		SrcBox.left = 0;
		SrcBox.top = 0;
		SrcBox.front = 0;
		SrcBox.right = OutputDim.X;
		SrcBox.bottom = OutputDim.Y;
		SrcBox.back = 1;

		TRefCountPtr<IDXGIKeyedMutex> KeyedMutex;
		HRESULT res = Texture->QueryInterface(_uuidof(IDXGIKeyedMutex), (void**)&KeyedMutex);

		if (KeyedMutex)
		{
			// No wait on acquire since sample is new and key is 0.
			res = KeyedMutex->AcquireSync(0, 0);
			if (res == S_OK)
			{
				// Copy texture using the decoder DX11 device... (and apply any cropping - see above note)
				InDeviceContext->CopySubresourceRegion(Texture, 0, 0, 0, 0, DecoderTexture, ViewIndex, &SrcBox);

				// Mark texture as updated with key of 1
				// Sample will be read in Convert() method of texture sample
				KeyedMutex->ReleaseSync(1);
			}
		}

		// Make sure texture is updated before giving access to the sample in the rendering thread.
		InDeviceContext->Flush();
	}
}

void FElectraPlayerVideoDecoderOutputPC::ShutdownPoolable()
{
	TSharedPtr<IDecoderOutputOwner, ESPMode::ThreadSafe> lockedVideoRenderer = OwningRenderer.Pin();
	if (lockedVideoRenderer.IsValid())
	{
		lockedVideoRenderer->SampleReleasedToPool(this);
	}

	if (OutputType != EOutputType::HardwareWin8Plus)
	{
		return;
	}

	// Correctly release the keyed mutex when the sample is returned to the pool
	TRefCountPtr<IDXGIResource> OtherResource(nullptr);
	if (Texture)
	{
		Texture->QueryInterface(__uuidof(IDXGIResource), (void**)&OtherResource);
	}

	if (OtherResource)
	{
		HANDLE SharedHandle = nullptr;
		if (OtherResource->GetSharedHandle(&SharedHandle) == S_OK)
		{
			TRefCountPtr<ID3D11Resource> SharedResource;
			D3D11Device->OpenSharedResource(SharedHandle, __uuidof(ID3D11Texture2D), (void**)&SharedResource);
			if (SharedResource)
			{
				TRefCountPtr<IDXGIKeyedMutex> KeyedMutex;
				OtherResource->QueryInterface(_uuidof(IDXGIKeyedMutex), (void**)&KeyedMutex);

				if (KeyedMutex)
				{
					// Reset keyed mutex
					if (KeyedMutex->AcquireSync(1, 0) == S_OK)
					{
						// Texture was never read
						KeyedMutex->ReleaseSync(0);
					}
					else if (KeyedMutex->AcquireSync(2, 0) == S_OK)
					{
						// Texture was read at least once
						KeyedMutex->ReleaseSync(0);
					}
				}
			}
		}
	}

}

// -----------------------------------------------------------------------------------------------------------------------------

FVideoDecoderOutput* FElectraPlayerPlatformVideoDecoderOutputFactory::Create()
{
	return new FElectraPlayerVideoDecoderOutputPC();
}
