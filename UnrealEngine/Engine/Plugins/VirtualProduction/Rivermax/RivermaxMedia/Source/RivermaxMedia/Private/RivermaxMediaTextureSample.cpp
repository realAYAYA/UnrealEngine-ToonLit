// Copyright Epic Games, Inc. All Rights Reserved.

#include "RivermaxMediaTextureSample.h"

#include "RenderGraphUtils.h"
#include "RivermaxMediaPlayer.h"
#include "RivermaxMediaTextureSampleConverter.h"
#include "RivermaxMediaUtils.h"

namespace UE::RivermaxMedia
{

FRivermaxMediaTextureSample::FRivermaxMediaTextureSample()
	: Converter(MakePimpl<FRivermaxMediaTextureSampleConverter>())
{
	
}

const FMatrix& FRivermaxMediaTextureSample::GetYUVToRGBMatrix() const
{
	return MediaShaders::YuvToRgbRec709Scaled;
}

IMediaTextureSampleConverter* FRivermaxMediaTextureSample::GetMediaTextureSampleConverter()
{
	return Converter.Get();
}

bool FRivermaxMediaTextureSample::IsOutputSrgb() const
{
	// We always do the sRGB to Linear conversion in shader if specified by the source
	// If true is returned here, MediaTextureResource will create (try) a sRGB texture which only works for 8 bits
	return false;
}

bool FRivermaxMediaTextureSample::ConfigureSample(const FSampleConfigurationArgs& Args)
{
	switch (Args.SampleFormat)
	{
		case ERivermaxMediaSourcePixelFormat::RGB_12bit:
			// Falls through
		case ERivermaxMediaSourcePixelFormat::RGB_16bit_Float:
		{
			SampleFormat = EMediaTextureSampleFormat::FloatRGBA;
			break;
		}
		case ERivermaxMediaSourcePixelFormat::RGB_10bit:
			// Falls through
		case ERivermaxMediaSourcePixelFormat::YUV422_10bit:
		{
			SampleFormat = EMediaTextureSampleFormat::CharBGR10A2;
			break;
		}
		case ERivermaxMediaSourcePixelFormat::YUV422_8bit:
			// Falls through
		case ERivermaxMediaSourcePixelFormat::RGB_8bit:
			// Falls through
		default:
		{
			SampleFormat = EMediaTextureSampleFormat::CharBGRA;
			break;
		}
	}

	Converter->Setup(AsShared());

	WeakPlayer = Args.Player;
	InputFormat = Args.SampleFormat;
	Dimension = FIntPoint(Args.Width, Args.Height);
	Time = Args.Time;
	Duration = FTimespan(1); // We keep our frame to have a duration of 1 frame
	bIsInputSRGB = Args.bInIsSRGBInput;

	return true;
}

TRefCountPtr<FRDGPooledBuffer> FRivermaxMediaTextureSample::GetGPUBuffer() const
{
	return GPUBuffer;
}

void FRivermaxMediaTextureSample::InitializeGPUBuffer(const FIntPoint& InResolution, ERivermaxMediaSourcePixelFormat InSampleFormat)
{
	using namespace UE::RivermaxMediaUtils::Private;

	const FSourceBufferDesc BufferDescription = GetBufferDescription(InResolution, InSampleFormat);

	FRDGBufferDesc RDGBufferDesc = FRDGBufferDesc::CreateStructuredDesc(BufferDescription.BytesPerElement, BufferDescription.NumberOfElements);
	
	// Required to share resource across different graphics API (DX, Cuda)
	RDGBufferDesc.Usage |= EBufferUsageFlags::Shared;

	TWeakPtr<FRivermaxMediaTextureSample> WeakSample = AsShared();
	ENQUEUE_RENDER_COMMAND(RivermaxPlayerBufferCreation)(
	[WeakSample, RDGBufferDesc](FRHICommandListImmediate& CommandList)
	{
		if (TSharedPtr<FRivermaxMediaTextureSample> Sample = WeakSample.Pin())
		{
			Sample->GPUBuffer = AllocatePooledBuffer(RDGBufferDesc, TEXT("RmaxInput Buffer"));
		}
	});
}

const void* FRivermaxMediaTextureSample::GetBuffer()
{
	return Buffer.GetData();
}

FIntPoint FRivermaxMediaTextureSample::GetDim() const
{
	return Dimension;
}

FTimespan FRivermaxMediaTextureSample::GetDuration() const
{
	return Duration;
}

EMediaTextureSampleFormat FRivermaxMediaTextureSample::GetFormat() const
{
	return SampleFormat;
}

FIntPoint FRivermaxMediaTextureSample::GetOutputDim() const
{
	return GetDim();
}

uint32 FRivermaxMediaTextureSample::GetStride() const
{
	return Stride;
}

FMediaTimeStamp FRivermaxMediaTextureSample::GetTime() const
{
	return FMediaTimeStamp(Time);
}

bool FRivermaxMediaTextureSample::IsCacheable() const
{
	return false;
}

FRHITexture* FRivermaxMediaTextureSample::GetTexture() const
{
	return nullptr;
}

void* FRivermaxMediaTextureSample::RequestBuffer(uint32 InBufferSize)
{
	Buffer.Reset();
	Buffer.SetNumUninitialized(InBufferSize);
	return Buffer.GetData();
}

void FRivermaxMediaTextureSample::SetBuffer(TRefCountPtr<FRDGPooledBuffer> NewBuffer)
{
	GPUBuffer = NewBuffer;
}

TSharedPtr<FRivermaxMediaPlayer> FRivermaxMediaTextureSample::GetPlayer() const
{
	return WeakPlayer.Pin();
}

ERivermaxMediaSourcePixelFormat FRivermaxMediaTextureSample::GetInputFormat() const
{
	return InputFormat;
}

bool FRivermaxMediaTextureSample::NeedsSRGBToLinearConversion() const
{
	return bIsInputSRGB;
}

bool FRivermaxMediaTextureSamples::FetchVideo(TRange<FTimespan> TimeRange, TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe>& OutSample)
{
	if (!CurrentSample.IsValid())
	{
		return false;
	}

	OutSample = CurrentSample;

	CurrentSample.Reset();

	return true;
}

bool FRivermaxMediaTextureSamples::PeekVideoSampleTime(FMediaTimeStamp& TimeStamp)
{
	return false;
}

void FRivermaxMediaTextureSamples::FlushSamples()
{
	CurrentSample.Reset();
}

}

