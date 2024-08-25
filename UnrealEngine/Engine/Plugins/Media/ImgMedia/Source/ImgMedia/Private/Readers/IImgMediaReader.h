// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/FrameRate.h"
#include "IMediaTextureSample.h"
#include "Assets/ImgMediaMipMapInfo.h"
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

	/** Uncompressed size (in bytes). All mip levels included. */
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
 * A thread-safe container for tile selections per mip level.
 */
struct FImgMediaMipTiles final
{
	/** Default constructor. */
	FImgMediaMipTiles() = default;
	/** Default destructor. */
	~FImgMediaMipTiles() = default;
	
	/** Move constructor. */
	FImgMediaMipTiles(FImgMediaMipTiles&& Other)
	{
		InternalMove(MoveTemp(Other));
	}
	
	/** Copy constructor. */
	FImgMediaMipTiles(const FImgMediaMipTiles& Other)
	{
		InternalCopy(Other);
	}

	/** Move assignment operator. */
	FImgMediaMipTiles& operator=(FImgMediaMipTiles&& Other)
	{
		InternalMove(MoveTemp(Other));
		return *this;
	}

	/** Copy assignment operator. */
	FImgMediaMipTiles& operator=(const FImgMediaMipTiles& Other)
	{
		InternalCopy(Other);
		return *this;
	}

	/**
	 * Contains the specified mip level.
	 * 
	 * @param InMipLevel mip level
	 * @return true when mip level is present.
	*/
	bool Contains(const int32 InMipLevel) const
	{
		FScopeLock Lock(&CriticalSection);
		return MipTiles.Contains(InMipLevel);
	}

	/**
	 * Check if the container is empty.
	 *
	 * @return true when no mips/tiles are present.
	*/
	bool IsEmpty() const
	{
		FScopeLock Lock(&CriticalSection);
		return MipTiles.IsEmpty();
	}

	/** Reset the container. */
	void Reset()
	{
		FScopeLock Lock(&CriticalSection);
		return MipTiles.Reset();
	}

	/**
	 * Check if the existing tiles contain all of the requested ones. (Is the container a superset of requested?)
	 *
	 * @param InRequestedMipTiles Requested tile selection per mip level.
	 * @return true if the selection is entirely contained, or the request was empty.
	 */
	bool ContainsMipTiles(const TMap<int32, FImgMediaTileSelection>& InRequestedMipTiles)
	{
		for (auto Iter = InRequestedMipTiles.CreateConstIterator(); Iter; ++Iter)
		{
			if (!ContainsTiles(Iter.Key(), Iter.Value()))
			{
				return false;
			}
		}

		return true;
	}

	/**
	 * Check if the existing tiles contain all of the requested ones, at the specified mip level.
	 * 
	 * @param InMipLevel Specified mip level.
	 * @param InSelection Requested tile selection.
	 * @return true if the selection is contained
	 */
	bool ContainsTiles(const int32 InMipLevel, const FImgMediaTileSelection& InSelection)
	{
		FScopeLock Lock(&CriticalSection);

		if (const FImgMediaTileSelection* CachedSelection = MipTiles.Find(InMipLevel))
		{
			// Check if requested tile selection is not present.
			return CachedSelection->Contains(InSelection);
		}

		// Requested mip level is not present.
		return false;
	}

	/**
	 * Include the selection at the specified mip level.
	 *
	 * @param InMipLevel Specified mip level.
	 * @param InSelection New tile selection.
	 */
	void Include(const int32 InMipLevel, const FImgMediaTileSelection& InSelection)
	{
		FScopeLock Lock(&CriticalSection);

		if (FImgMediaTileSelection* Tiles = MipTiles.Find(InMipLevel))
		{
			Tiles->Include(InSelection);
		}
		else
		{
			MipTiles.Emplace(InMipLevel, InSelection);
		}
	}

	/**
	 * Emplace the selection at the specified mip level.
	 *
	 * @param InMipLevel Specified mip level.
	 * @param InSelection New tile selection.
	 */
	void Emplace(const int32 InMipLevel, const FImgMediaTileSelection& InSelection)
	{
		FScopeLock Lock(&CriticalSection);

		MipTiles.Emplace(InMipLevel, InSelection);
	}

	/**
	 * Get a copy of the selection at the specified mip level.
	 * 
	 * @param InMipLevel Specified mip level.
	 * @param OutSelection Returned tile selection.
	 * @return true if mip level was found
	 */
	bool GetSelection(const int32 InMipLevel, FImgMediaTileSelection& OutSelection) const
	{
		FScopeLock Lock(&CriticalSection);

		if (const FImgMediaTileSelection* Selection = MipTiles.Find(InMipLevel))
		{
			OutSelection = *Selection;
			return true;
		}

		return false;
	}

	/**
	 * Returns a calculated list of contiguous visible tile regions. Only provides regions for the missing tiles if
	 * CurrentTileSelection is specified.
	 *
	 * @param InMipLevel Specified mip level.
	 * @param InSelection Existing tile selection.
	 * @param OutRegions Visible tile regions array.
	 * @return true if mip level was found
	 */
	bool GetVisibleRegions(const int32 InMipLevel, const FImgMediaTileSelection& InSelection, TArray<FIntRect>& OutRegions) const
	{
		FScopeLock Lock(&CriticalSection);

		if (const FImgMediaTileSelection* Selection = MipTiles.Find(InMipLevel))
		{
			OutRegions = Selection->GetVisibleRegions(&InSelection);
			return true;
		}

		return false;
	}

	/** Thread-unsafe data getter: the caller is responsible for locking. */
	TMap<int32, FImgMediaTileSelection>& GetDataUnsafe()
	{
		return MipTiles;
	}

	/** Thread-unsafe data getter: the caller is responsible for locking. */
	const TMap<int32, FImgMediaTileSelection>& GetDataUnsafe() const
	{
		return MipTiles;
	}

public:

	/** Critical section to lock when using unsafe methods. */
	mutable FCriticalSection CriticalSection;

private:
	void InternalMove(FImgMediaMipTiles&& Other)
	{
		FScopeLock LockOther(&Other.CriticalSection);
		FScopeLock Lock(&CriticalSection);
		MipTiles = MoveTemp(Other.MipTiles);
	}

	void InternalCopy(const FImgMediaMipTiles& Other)
	{
		FScopeLock LockOther(&Other.CriticalSection);
		FScopeLock Lock(&CriticalSection);
		MipTiles = Other.MipTiles;
	}

	/** Tiles present per mip level. */
	TMap<int32, FImgMediaTileSelection> MipTiles;
};

/**
 * A single frame of an image sequence.
 */
struct FImgMediaFrame final
{
	/** Default constructor. */
	FImgMediaFrame() = default;

	/** Default destructor. */
	~FImgMediaFrame() = default;

	/** Copy constructor. */
	FImgMediaFrame(const FImgMediaFrame& Other)
		: Data(Other.Data)
		, Format(Other.Format.load())
		, MipTilesPresent(Other.MipTilesPresent)
		, NumTilesRead(Other.NumTilesRead.load())
		, Stride(Other.Stride.load())
		, Info() // copied below
		, SampleConverter() // copied below
	{
		{
			FScopeLock LockOther(&Other.SampleConverterCriticalSection);
			SampleConverter = Other.SampleConverter;
		}

		{
			FScopeLock LockOther(&Other.InfoCriticalSection);
			Info = Other.Info;
		}
	}

	/** Copy assignment operator. */
	FImgMediaFrame& operator=(const FImgMediaFrame& Other)
	{
		if (this != &Other)
		{
			this->~FImgMediaFrame();
			new(this) FImgMediaFrame(Other);
		}
		return *this;
	}

	FImgMediaFrame(FImgMediaFrame&& Other) = delete;
	FImgMediaFrame& operator=(FImgMediaFrame&& Other) = delete;

	/** The frame's data. */
	TSharedPtr<void, ESPMode::ThreadSafe> Data;

	/** The frame's sample format. */
	std::atomic<EMediaTextureSampleFormat> Format;

	/** Tiles present per mip level. */
	FImgMediaMipTiles MipTilesPresent;

	/** Total number of tiles read. */
	std::atomic<int32> NumTilesRead = 0;

	/** The frame's horizontal stride (in bytes). */
	std::atomic<uint32> Stride = 0;

	// This should only be used if you do not need to make changes to sample converter.
	IMediaTextureSampleConverter* GetSampleConverter()
	{
		return SampleConverter.Get();
	};

	template <class T>
	TSharedPtr<T, ESPMode::ThreadSafe> GetOrCreateSampleConverter()
	{
		FScopeLock ScopedLock(&SampleConverterCriticalSection);
		if (!SampleConverter.IsValid())
		{
			SampleConverter = MakeShared<T>();
		}
		return StaticCastSharedPtr<T>(SampleConverter);
	}

	/** Thread-safe info getter. */
	FImgMediaFrameInfo GetInfo() const
	{
		FScopeLock ScopedLock(&InfoCriticalSection);
		return Info;
	}

	/** Thread-safe info setter. */
	void SetInfo(FImgMediaFrameInfo InInfo)
	{
		FScopeLock ScopedLock(&InfoCriticalSection);
		Info = InInfo;
	}

	/** Thread-safe framerate getter. */
	FFrameRate GetFrameRate() const
	{
		FScopeLock ScopedLock(&InfoCriticalSection);
		return Info.FrameRate;
	}

	/** Thread-safe dimensions getter. */
	FIntPoint GetDim() const
	{
		FScopeLock ScopedLock(&InfoCriticalSection);
		return Info.Dim;
	}

	/** Thread-safe sRGB getter. */
	bool IsOutputSrgb() const
	{
		FScopeLock ScopedLock(&InfoCriticalSection);
		return Info.Srgb;
	}

	/** Thread-safe uncompressed size. */
	SIZE_T GetUncompressedSize() const
	{
		FScopeLock ScopedLock(&InfoCriticalSection);
		return Info.UncompressedSize;
	}

private:

	/** Information critical section. */
	mutable FCriticalSection InfoCriticalSection;

	/** Additional information about the frame. */
	FImgMediaFrameInfo Info;

	/** Lock to be used exclusively on reader threads. Note: this protects only the creation of the sample converter. */
	mutable FCriticalSection SampleConverterCriticalSection;

	/** Sample converter is used by Media Texture Resource to convert the texture or data. */
	TSharedPtr<IMediaTextureSampleConverter, ESPMode::ThreadSafe> SampleConverter;
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
	 * Makes sure a frame that was passed to CancelFrame is no longer marked to be cancelled.
	 * @param FrameNumber Frame that was passed to CancelFrame.
	 */
	virtual void UncancelFrame(int32 FrameNumber) = 0;

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
