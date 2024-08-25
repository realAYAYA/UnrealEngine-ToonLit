// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Video/VideoDecoder.h"

#include "Containers/Queue.h"

#include "Video/Resources/VideoResourceCUDA.h"

#include "NVDEC.h"
#include "Video/Decoders/Configs/VideoDecoderConfigNVDEC.h"

class NVDEC_API FVideoDecoderNVDEC : public TVideoDecoder<FVideoResourceCUDA, FVideoDecoderConfigNVDEC>
{
private:
	uint8 bIsOpen : 1;
	CUvideodecoder Decoder = nullptr;
	CUvideoparser Parser = nullptr;
	CUvideoctxlock CtxLock;

	struct FFrame
	{
		int32 SurfaceIndex;
		uint32 Width;
		uint32 Height;
		cudaVideoSurfaceFormat SurfaceFormat;
		
		CUVIDPROCPARAMS MapParams;
	};

	int32 FramesCount = 0;
	TQueue<FFrame> Frames;
	
public:
	FVideoDecoderNVDEC() = default;
	virtual ~FVideoDecoderNVDEC() override;

	virtual bool IsOpen() const override;
	virtual FAVResult Open(TSharedRef<FAVDevice> const& NewDevice, TSharedRef<FAVInstance> const& NewInstance) override;
	virtual void Close() override;

	bool IsInitialized() const;

	virtual FAVResult ApplyConfig() override;

	bool GetCapability(CUVIDDECODECAPS& CapsToQuery) const;

	virtual FAVResult SendPacket(FVideoPacket const& Packet) override;
	
	virtual FAVResult ReceiveFrame(TResolvableVideoResource<FVideoResourceCUDA>& InOutResource) override;

public:
	int HandleVideoSequence(CUVIDEOFORMAT *VideoFormat);
    int HandlePictureDecode(CUVIDPICPARAMS *PicParams);
    int HandlePictureDisplay(CUVIDPARSERDISPINFO *DispInfo);
};

namespace Internal 
{
    int HandleVideoSequenceCallback(void *UserData, CUVIDEOFORMAT *VideoFormat);
	int HandlePictureDecodeCallback(void *UserData, CUVIDPICPARAMS *PicParams);
	int HandlePictureDisplayCallback(void *UserData, CUVIDPARSERDISPINFO *DispInfo);
}
