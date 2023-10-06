// Copyright Epic Games, Inc. All Rights Reserved.

#include "ElectraVideoDecoder_PC.h"
#include "Renderer/RendererVideo.h"

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
	if (Buffer.IsValid())
	{
		return *Buffer;
	}
	else
	{
		static TArray<uint8> Empty;
		return Empty;
	}
}

uint32 FElectraPlayerVideoDecoderOutputPC::GetStride() const
{
	return Stride;
}

TRefCountPtr<IUnknown> FElectraPlayerVideoDecoderOutputPC::GetTexture() const
{
	return static_cast<IUnknown*>(Texture.GetReference());
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

TRefCountPtr<IUnknown> FElectraPlayerVideoDecoderOutputPC::GetSync(uint64& SyncValue) const
{
	TRefCountPtr<IUnknown> KeyedMutex;
	HRESULT res = Texture->QueryInterface(_uuidof(IDXGIKeyedMutex), (void**)&KeyedMutex);
	return KeyedMutex;
}

void FElectraPlayerVideoDecoderOutputPC::InitializeWithBuffer(const void* InBuffer, uint32 InSize, uint32 InStride, FIntPoint Dim, TSharedPtr<Electra::FParamDict, ESPMode::ThreadSafe> InParamDict)
{
	FVideoDecoderOutput::Initialize(MoveTemp(InParamDict));

	OutputType = EOutputType::HardwareDX9_DX12;

	Buffer = MakeShared<TArray<uint8>, ESPMode::ThreadSafe>();
	Buffer->Append((uint8*)InBuffer, InSize);

	SampleDim = Dim;
	Stride = InStride;
}

void FElectraPlayerVideoDecoderOutputPC::InitializeWithBuffer(TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> InBuffer, uint32 InStride, FIntPoint Dim, TSharedPtr<Electra::FParamDict, ESPMode::ThreadSafe> InParamDict)
{
	FVideoDecoderOutput::Initialize(MoveTemp(InParamDict));

	OutputType = EOutputType::HardwareDX9_DX12;

	Buffer = MoveTemp(InBuffer);

	SampleDim = Dim;
	Stride = InStride;
}


void FElectraPlayerVideoDecoderOutputPC::InitializeWithSharedTexture(const TRefCountPtr<ID3D11Device>& InD3D11Device, const TRefCountPtr<ID3D11DeviceContext> InDeviceContext, const TRefCountPtr<IMFSample> InMFSample, const FIntPoint& OutputDim, TSharedPtr<Electra::FParamDict, ESPMode::ThreadSafe> InParamDict)
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

	InitializeWithSharedTexture(InD3D11Device, InDeviceContext, DecoderTexture, ViewIndex, OutputDim, InParamDict);
}

void FElectraPlayerVideoDecoderOutputPC::InitializeWithSharedTexture(const TRefCountPtr<ID3D11Device>& InD3D11Device, const TRefCountPtr<ID3D11DeviceContext> InDeviceContext, const TRefCountPtr<ID3D11Texture2D> DecoderTexture, uint32 ViewIndex, const FIntPoint& OutputDim, TSharedPtr<Electra::FParamDict, ESPMode::ThreadSafe> InParamDict)
{
	FVideoDecoderOutput::Initialize(InParamDict);

	OutputType = EOutputType::HardwareWin8Plus;

	bool bNeedsNew = !Texture.IsValid() || (SampleDim.X != OutputDim.X || SampleDim.Y != OutputDim.Y);

	if (bNeedsNew)
	{
		SampleDim = OutputDim;

		D3D11_TEXTURE2D_DESC DecoderTextureDesc;
		DecoderTexture->GetDesc(&DecoderTextureDesc);

		// Make a texture we pass on as output
		D3D11_TEXTURE2D_DESC TextureDesc;
		TextureDesc.Width = SampleDim.X;
		TextureDesc.Height = SampleDim.Y;
		TextureDesc.MipLevels = 1;
		TextureDesc.ArraySize = 1;
		TextureDesc.Format = DecoderTextureDesc.Format;
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
