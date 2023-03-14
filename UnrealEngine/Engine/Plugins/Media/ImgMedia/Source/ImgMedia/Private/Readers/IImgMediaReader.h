// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/FrameRate.h"
#include "IMediaTextureSample.h"
#include "ImgMediaMipMapInfo.h"
#include "Math/IntPoint.h"
#include "Templates/SharedPointer.h"
#include "RHI.h"
#include "RHIResources.h"

/**
 * Information about an image sequence frame.
 */
struct FImgMediaFrameInfo
{
	/** Name of the image compression algorithm (i.e. "ZIP"). */
	FString CompressionName;

	/** Width and height of the frame (in pixels). */
	FIntPoint Dim;

	/** Name of the image format (i.e. "EXR"). */
	FString FormatName;

	/** Frame rate. */
	FFrameRate FrameRate;

	/** Whether the frame is in sRGB color space. */
	bool Srgb;

	/** Uncompressed size (in bytes). */
	SIZE_T UncompressedSize;

	/** Number of channels (RGB - 3 or RGBA - 4). */
	SIZE_T NumChannels;

	/** Does this frame have tiles?. */
	bool bHasTiles;

	/** Number of tiles in X and Y direction. */
	FIntPoint NumTiles;

	/** Tile dimensions. */
	FIntPoint TileDimensions;

	/** Tile border size in texels. This is required for more elaborate tile texel sampling. */
	int32 TileBorder;

	/** Number of Mip Levels. */
	int32 NumMipLevels;
};


/**
 * A single frame of an image sequence.
 */
struct FImgMediaFrame
{
	/** The frame's data. */
	TSharedPtr<void, ESPMode::ThreadSafe> Data;

	/** The frame's sample format. */
	EMediaTextureSampleFormat Format;

	/** Additional information about the frame. */
	FImgMediaFrameInfo Info;

	/** Tiles present per mip level. */
	TMap<int32, FImgMediaTileSelection> MipTilesPresent;

	/** Total number of tiles read. */
	int32 NumTilesRead = 0;

	/** The frame's horizontal stride (in bytes). */
	uint32 Stride = 0;

	/** Sample converter is used by Media Texture Resource to convert the texture or data. */
	TSharedPtr<IMediaTextureSampleConverter, ESPMode::ThreadSafe> SampleConverter;

	virtual IMediaTextureSampleConverter* GetSampleConverter()
	{
		if (!SampleConverter.IsValid())
		{
			return nullptr;
		}
		return SampleConverter.Get();
	};

	/** Virtual non trivial destructor. */
	virtual ~FImgMediaFrame() {};
};


/**
 * Interface for image sequence readers.
 */
class IImgMediaReader
{
public:

	/**
	 * Get information about an image sequence frame.
	 *
	 * @param ImagePath Path to the image file containing the frame.
	 * @param OutInfo Will contain the frame info.
	 * @return true on success, false otherwise.
	 * @see ReadFrame
	 */
	virtual bool GetFrameInfo(const FString& ImagePath, FImgMediaFrameInfo& OutInfo) = 0;

	/**
	 * Read a single image frame.
	 *
	 * @param FrameId Frame number to read.
	 * @param MipLevel Will read in this level and all higher levels.
	 * @param InTileSelection Which tiles to read.
	 * @param OutFrame Will contain the frame.
	 * @return true on success, false otherwise.
	 * @see GetFrameInfo
	 */
	virtual bool ReadFrame(int32 FrameId, const TMap<int32, FImgMediaTileSelection>& InMipTiles, TSharedPtr<FImgMediaFrame, ESPMode::ThreadSafe> OutFrame) = 0;

	/**
	 * Mark Frame to be canceled based on Frame number. Typically this will be 
	 * @param FrameNumber used for frame lookup.
	 *
	 */
	virtual void CancelFrame(int32 FrameNumber) = 0;

	/**
	 * For some readers this function allows to pre-allocate enough memory to support the
	 * maximum number of frames with as much efficiency as possible.
	 *
	 */
	virtual void PreAllocateMemoryPool(int32 NumFrames, const FImgMediaFrameInfo& FrameInfo, const bool bCustomFormat) {};

	/**
	 * Used in case reader needs to do some processing once per frame.
	 * Example: ExrImgMediaReaderGpu which returns unused memory to memory pool.
	 * 
	 */
	virtual void OnTick() {};


public:

	/** Virtual destructor. */
	virtual ~IImgMediaReader() { }
};
