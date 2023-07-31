// Copyright Epic Games, Inc. All Rights Reserved.

#include "VideoDecoderH264_Dummy.h"

#ifdef AVENCODER_VIDEO_DECODER_AVAILABLE_H264_DUMMY


namespace AVEncoder
{

class FVideoDecoderOutputDummy : public FVideoDecoderOutput
{
public:
	FVideoDecoderOutputDummy(int32 w, int32 h, int64 pts)
	: Width(w), Height(h), PTS(pts)
	{}

	virtual ~FVideoDecoderOutputDummy()
	{
	}

	virtual int32 AddRef() override
	{
		return FPlatformAtomics::InterlockedIncrement(&RefCount);
	}

	virtual int32 Release() override
	{
		int32 c = FPlatformAtomics::InterlockedDecrement(&RefCount);
		// We do not release the allocated buffer from the application here.
		// This is meant to release only what the decoder uses internally, but not the
		// external buffers the application is working with!
		if (c == 0)
		{
			delete this;
		}
		return c;
	}

	virtual int32 GetWidth() const override
	{
		return Width;
	}
	virtual int32 GetHeight() const override
	{
		return Height;
	}
	virtual int64 GetPTS() const override
	{
		return PTS;
	}

	virtual const FVideoDecoderAllocFrameBufferResult* GetAllocatedBuffer() const
	{
		return &Buffer;
	}

	virtual int32 GetCropLeft() const override
	{
		return 0;
	}
	virtual int32 GetCropRight() const override
	{
		return 0;
	}
	virtual int32 GetCropTop() const override
	{
		return 0;
	}
	virtual int32 GetCropBottom() const override
	{
		return 0;
	}
	virtual int32 GetAspectX() const override
	{
		return 1;
	}
	virtual int32 GetAspectY() const override
	{
		return 1;
	}
	virtual int32 GetPitchX() const override
	{
		return Width;
	}
	virtual int32 GetPitchY() const override
	{
		return Height;
	}
	virtual uint32 GetColorFormat() const override
	{
		return 0;
	}

	// Internal for allocation.
	FVideoDecoderAllocFrameBufferResult* GetBuffer()
	{
		return &Buffer;
	}

private:
	FVideoDecoderAllocFrameBufferResult	Buffer = {};
	int32	RefCount = 1;
	int32	Width = 0;
	int32	Height = 0;
	int64	PTS = 0;
};

	// query whether or not encoder is supported and available
//	static bool GetIsAvailable(FVideoEncoderInputImpl& InInput, FVideoEncoderInfo& OutEncoderInfo);

void FVideoDecoderH264_Dummy::Register(FVideoDecoderFactory& InFactory)
{
	FVideoDecoderInfo	DecoderInfo;
	DecoderInfo.CodecType = ECodecType::H264;
	DecoderInfo.MaxWidth = 1920;
	DecoderInfo.MaxHeight = 1088;

	InFactory.Register(DecoderInfo, []() {
			return new FVideoDecoderH264_Dummy();
		});
}

FVideoDecoderH264_Dummy::FVideoDecoderH264_Dummy()
{
}

FVideoDecoderH264_Dummy::~FVideoDecoderH264_Dummy()
{
}

bool FVideoDecoderH264_Dummy::Setup(const FVideoDecoder::FInit& InInit)
{
	CreateDecoderAllocationInterfaceFN = InInit.CreateDecoderAllocationInterface;
	ReleaseDecoderAllocationInterfaceFN = InInit.ReleaseDecoderAllocationInterface;
	return true;
}

void FVideoDecoderH264_Dummy::Shutdown()
{
	if (bIsInitialized)
	{
		ReleaseDecoderAllocationInterface();
		bIsInitialized = false;
	}
	delete this;
}


FVideoDecoder::EDecodeResult FVideoDecoderH264_Dummy::Decode(const FVideoDecoderInput* InInput)
{
	// Lazily initialize the decoder.
	if (!bIsInitialized)
	{
		bIsInitialized = CreateDecoderAllocationInterface();
	}

	//
	if (OnDecodedFrame)
	{
		if (bIsInitialized)
		{
			int32_t	Width = InInput->GetWidth();
			int32_t Height = InInput->GetHeight();

			TUniquePtr<FVideoDecoderOutputDummy> pNew(new FVideoDecoderOutputDummy(Width, Height, InInput->GetPTS()));

			// Get memory from the application
			FVideoDecoderAllocFrameBufferParams ap {};
			EFrameBufferAllocReturn ar;
			ap.FrameBufferType = EFrameBufferType::CODEC_RawBuffer;
			ap.AllocSize = Width * Height * 3 / 2;
			ap.AllocAlignment = 16;
			ap.AllocFlags = 0;
			ap.Width = Width;
			ap.Height = Height;
			ap.BytesPerPixel = 1;
			ar = AllocateOutputFrameBuffer(pNew->GetBuffer(), &ap);
			if (ar == EFrameBufferAllocReturn::CODEC_Success)
			{
				// Got an output buffer.
				check(pNew->GetBuffer()->AllocatedBuffer);
			}
			else if (ar == EFrameBufferAllocReturn::CODEC_TryAgainLater)
			{
				check(!"TODO");
				return FVideoDecoder::EDecodeResult::Failure;
//				return FVideoDecoder::EDecodeResult::TryAgainLater;
			}
			else
			{
				// Error!
				return FVideoDecoder::EDecodeResult::Failure;
			}

			// Buffer needs to contain an NV12 texture.
			int32 ySize = Width * Height;
			uint8* pY = (uint8*)pNew->GetBuffer()->AllocatedBuffer;
			uint8* pUV = pY + ySize;
			FMemory::Memset(pUV, 128, ySize / 2);
			int C = ++YOffset;
			for(int32 y=0,yMax=Height; y<yMax; ++y)
			{
				FMemory::Memset(pY, (uint8)C++, Width);
				pY += Width;
			}
			OnDecodedFrame(pNew.Release());
			return FVideoDecoder::EDecodeResult::Success;
		}
		else
		{
// TODO: could not decode, but may need to still notify the caller with an empty frame??
			return FVideoDecoder::EDecodeResult::Failure;
		}
	}
	else
	{
		// With no registered callback that's interested in the result we can presume we would have been successful.
		return FVideoDecoder::EDecodeResult::Success;
	}
}


} // namespace AVEncoder


#endif // AVENCODER_VIDEO_DECODER_AVAILABLE_H264_DUMMY
