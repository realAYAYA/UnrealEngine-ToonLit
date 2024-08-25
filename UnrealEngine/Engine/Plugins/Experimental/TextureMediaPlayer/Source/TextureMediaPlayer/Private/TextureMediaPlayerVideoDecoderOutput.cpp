// Copyright Epic Games, Inc. All Rights Reserved.

#include "TextureMediaPlayerVideoDecoderOutput.h"

void TextureMediaPlayerVideoDecoderOutput::Initialize(TSharedPtr<Electra::FParamDict, ESPMode::ThreadSafe> InParamDict, const TArray<uint8>& InBuffer, const FIntPoint& InSampleDim)
{
	FVideoDecoderOutputPC::Initialize(MoveTemp(InParamDict));
	ByteBuffer = MakeShared<TArray<uint8>, ESPMode::ThreadSafe>(InBuffer);
	SampleDim = InSampleDim;
}

#if PLATFORM_WINDOWS
void TextureMediaPlayerVideoDecoderOutput::Initialize(TSharedPtr<Electra::FParamDict, ESPMode::ThreadSafe> InParamDict, FTexture2DRHIRef InTexture, const FIntPoint& InSampleDim, TRefCountPtr<ID3D12Fence> InFence, uint64 InFenceValue)
#else
void TextureMediaPlayerVideoDecoderOutput::Initialize(TSharedPtr<Electra::FParamDict, ESPMode::ThreadSafe> InParamDict, FTexture2DRHIRef InTexture, const FIntPoint& InSampleDim, FGPUFenceRHIRef InFence, uint64 InFenceValue)
#endif
{
	FVideoDecoderOutputPC::Initialize(MoveTemp(InParamDict));
	Texture = InTexture;
	SampleDim = InSampleDim;
	Fence = InFence;
	FenceValue = InFenceValue;
}

void TextureMediaPlayerVideoDecoderOutput::SetOwner(const TSharedPtr<IDecoderOutputOwner, ESPMode::ThreadSafe>& Renderer)
{
	check(!"Should not be called!");
}

TextureMediaPlayerVideoDecoderOutput::EOutputType TextureMediaPlayerVideoDecoderOutput::GetOutputType() const
{
	if(Texture.IsValid())
	{
		return TextureMediaPlayerVideoDecoderOutput::EOutputType::Hardware_DX;
	}

	return TextureMediaPlayerVideoDecoderOutput::EOutputType::HardwareDX9_DX12;
}

const TArray<uint8>& TextureMediaPlayerVideoDecoderOutput::GetBuffer() const
{
	return *ByteBuffer;
}

uint32 TextureMediaPlayerVideoDecoderOutput::GetStride() const
{
	return 0;
}

FIntPoint TextureMediaPlayerVideoDecoderOutput::GetDim() const
{
	return SampleDim;
}

#if PLATFORM_WINDOWS

TRefCountPtr<IUnknown> TextureMediaPlayerVideoDecoderOutput::GetTexture() const
{
	IUnknown* Result = nullptr;
	if (Texture.IsValid())
	{
		Result = static_cast<IUnknown*>(Texture->GetNativeResource());
	}
	return Result;
}

TRefCountPtr<IUnknown> TextureMediaPlayerVideoDecoderOutput::GetSync(uint64& SyncValue) const
{
	if (Fence.IsValid())
	{
		SyncValue = FenceValue;
		return static_cast<IUnknown*>(Fence.GetReference());
	}
	SyncValue = 0;
	return nullptr;
}

TRefCountPtr<ID3D11Device> TextureMediaPlayerVideoDecoderOutput::GetDevice() const
{
	check(!"Should not be called!");
	return nullptr;
}

#endif // PLATFORM_WINDOWS
