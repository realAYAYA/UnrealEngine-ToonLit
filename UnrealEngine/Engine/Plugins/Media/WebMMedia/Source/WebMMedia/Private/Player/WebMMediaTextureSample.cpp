// Copyright Epic Games, Inc. All Rights Reserved.

#include "WebMMediaTextureSample.h"
#include "RHI.h"

FWebMMediaTextureSample::FWebMMediaTextureSample()
	: Time(FTimespan::Zero())
{
}

void FWebMMediaTextureSample::Initialize(FIntPoint InDisplaySize, FIntPoint InTotalSize, FTimespan InTime, FTimespan InDuration)
{
	Time = InTime;
	DisplaySize = InDisplaySize;
	TotalSize = InTotalSize;
	Duration = InDuration;
}

void FWebMMediaTextureSample::CreateTexture()
{
	check(IsInRenderingThread());

	const FRHITextureCreateDesc Desc =
		FRHITextureCreateDesc::Create2D(TEXT("FWebMMediaTextureSample_DummyTexture2D"))
		.SetExtent(TotalSize)
		.SetFormat(PF_B8G8R8A8)
		.SetFlags(ETextureCreateFlags::Dynamic | ETextureCreateFlags::SRGB | ETextureCreateFlags::RenderTargetable | ETextureCreateFlags::ShaderResource)
		.SetInitialState(ERHIAccess::SRVMask);

	Texture = RHICreateTexture(Desc);
}

const void* FWebMMediaTextureSample::GetBuffer()
{
	return nullptr;
}

FIntPoint FWebMMediaTextureSample::GetDim() const
{
	return TotalSize;
}

FTimespan FWebMMediaTextureSample::GetDuration() const
{
	return Duration;
}

EMediaTextureSampleFormat FWebMMediaTextureSample::GetFormat() const
{
	return EMediaTextureSampleFormat::CharBGRA;
}

FIntPoint FWebMMediaTextureSample::GetOutputDim() const
{
	return DisplaySize;
}

uint32 FWebMMediaTextureSample::GetStride() const
{
	if (!Texture.IsValid())
	{
		return 0;
	}

	return Texture->GetSizeX() * 4;
}

FRHITexture* FWebMMediaTextureSample::GetTexture() const
{
	return Texture.GetReference();
}

FMediaTimeStamp FWebMMediaTextureSample::GetTime() const
{
	return FMediaTimeStamp(Time);
}

bool FWebMMediaTextureSample::IsCacheable() const
{
	return true;
}

bool FWebMMediaTextureSample::IsOutputSrgb() const
{
	return true;
}

void FWebMMediaTextureSample::ShutdownPoolable()
{
	// Drop reference to the texture. It should be released by the outside system.
	Texture = nullptr;
	
	Time = FTimespan::Zero();
}

TRefCountPtr<FRHITexture2D> FWebMMediaTextureSample::GetTextureRef() const
{
	return Texture;
}
