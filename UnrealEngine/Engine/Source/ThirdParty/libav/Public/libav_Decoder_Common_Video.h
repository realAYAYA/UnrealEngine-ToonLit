// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "libav_Decoder_Common.h"

class ILibavDecoderDecodedImage;
class ILibavDecoderVideoResourceAllocator;

class ILibavDecoderVideoCommon : public ILibavDecoder
{
public:
	struct FPlaneInfo
	{
		enum class EContent
		{
			Unknown,
			Luma,				// Y
			ChromaU,			// U
			ChromaV,			// V
			ChromaUV,			// U and V
			NV12				// YUV, single buffer
		};
		EContent Content = EContent::Unknown;
		int32 Width = 0;
		int32 Height = 0;
		int32 BytesPerPixel = 0;
		int32 ByteOffsetToFirstPixel = 0;
		int32 ByteOffsetBetweenPixels = 0;
		int32 ByteOffsetBetweenRows = 0;
		const void* Address = nullptr;
	};

	struct FOutputInfo
	{
		int64 PTS = 0;
		int64 UserValue = 0;
		int32 Width = 0;
		int32 Height = 0;
		int32 CropLeft = 0;
		int32 CropTop = 0;
		int32 CropRight = 0;
		int32 CropBottom = 0;
		int32 AspectNum = 1;
		int32 AspectDenom = 1;
		int32 ISO23001_8_ColorPrimaries = -1;				// Same value as ISO/IEC 23001-8:2013 section 7.1, set to -1 if none is spefified/known
		int32 ISO23001_8_TransferCharacteristics = -1;		// Same value as ISO/IEC 23001-8:2013 section 7.2, set to -1 if none is spefified/known
		int32 ISO23001_8_MatrixCoefficients = -1;			// Same value as ISO/IEC 23001-8:2013 section 7.3, set to -1 if none is spefified/known
		FPlaneInfo Planes[4];
		int32 NumPlanes = 0;
	};

	struct FInputAU : public ILibavDecoder::FInputAccessUnit
	{
		enum EFlags
		{
			EVidAUFlag_None = 0,
			EVidAUFlag_DoNotOutput = 1 << 0,
		};
		int32 Flags = EFlags::EVidAUFlag_None;
	};


	static bool IsAvailable(int64 CodecID);

	/*
		Possible options are:
			"hw_priority" : A string giving names of preferred hardware accelerators, in order of preference, separated by semicolon. Eg. "vdpau;cuda;vaapi"
						    If an environment variable "UE_LIBAV_HWACCEL_PREFS" is set it will override this option!
			"force_sw" : A boolean set to true to force software decoding. If not set or set to false, hardware decoding will be used if possible.
						 If an environment variable "UE_LIBAV_FORCE_SW" exists and is set to a value of "1" or "true" it overrides this option.
	*/

	static LIBAV_API TSharedPtr<ILibavDecoderVideoCommon, ESPMode::ThreadSafe> Create(int64 CodecID, ILibavDecoderVideoResourceAllocator* InVideoResourceAllocator, const TMap<FString, FVariant>& InOptions);

	virtual ~ILibavDecoderVideoCommon() = default;

	virtual EDecoderError DecodeAccessUnit(const FInputAU& InInputAccessUnit) = 0;
	virtual EDecoderError SendEndOfData() = 0;
	virtual void Reset() = 0;
	virtual EOutputStatus HaveOutput(FOutputInfo& OutInfo) = 0;
	virtual TSharedPtr<ILibavDecoderDecodedImage, ESPMode::ThreadSafe> GetOutput() = 0;
};

class ILibavDecoderDecodedImage
{
public:
	virtual ~ILibavDecoderDecodedImage() = default;

	virtual const ILibavDecoderVideoCommon::FOutputInfo& GetOutputInfo() const = 0;
};


/**
 * Interface provided to the video through which it calls back to our code to perform
 * allocations of buffers and other resources.
 */
class ILibavDecoderVideoResourceAllocator
{
public:
	virtual ~ILibavDecoderVideoResourceAllocator() = default;
};
