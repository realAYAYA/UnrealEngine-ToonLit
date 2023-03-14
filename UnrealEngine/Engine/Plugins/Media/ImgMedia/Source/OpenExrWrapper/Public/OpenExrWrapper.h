// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/FrameRate.h"
#include "Math/IntPoint.h"

class FString;


class OPENEXRWRAPPER_API FOpenExr
{
public:

	static void SetGlobalThreadCount(uint16 ThreadCount);
};


class OPENEXRWRAPPER_API FRgbaInputFile
{
public:

	FRgbaInputFile(const FString& FilePath);
	FRgbaInputFile(const FString& FilePath, uint16 ThreadCount);
	~FRgbaInputFile();

public:

	const TCHAR* GetCompressionName() const;
	FIntPoint GetDataWindow() const;
	FFrameRate GetFrameRate(const FFrameRate& DefaultValue) const;
	int32 GetUncompressedSize() const;
	int32 GetNumChannels() const;

	// Gets tile dimensions. Returns false if image has no tiles.
	bool GetTileSize(FIntPoint& OutTileSize) const;

	bool IsComplete() const;
	bool HasInputFile() const;
	void ReadPixels(int32 StartY, int32 EndY);
	void SetFrameBuffer(void* Buffer, const FIntPoint& Stride);

	/**
	 * Get an attribute from the image.
	 *
	 * @param Name		Name of attribute.
	 * @param Value		Will be set to the value of the attribute if the attribute is found.
	 *					Will NOT be set if the attribute is not found.
	 * @return			True if the attribute was found, false otherwise.
	 */
	bool GetIntAttribute(const FString& Name, int32& Value);

private:

	void* InputFile;
};

class OPENEXRWRAPPER_API FOpenExrHeaderReader
{
public:

	FOpenExrHeaderReader(const FString& FilePath);
	~FOpenExrHeaderReader();

public:

	const TCHAR* GetCompressionName() const;
	FIntPoint GetDataWindow() const;
	FFrameRate GetFrameRate(const FFrameRate& DefaultValue) const;
	int32 GetUncompressedSize() const;
	int32 GetNumChannels() const;
	bool ContainsMips() const;
	int32 CalculateNumMipLevels(const FIntPoint& NumTiles) const;

	/** Gets tile dimensions. Returns false if image has no tiles. */
	bool GetTileSize(FIntPoint& OutTileSize) const;

	bool HasInputFile() const;

	/**
	 * Get an attribute from the image.
	 *
	 * @param Name		Name of attribute.
	 * @param Value		Will be set to the value of the attribute if the attribute is found.
	 *					Will NOT be set if the attribute is not found.
	 * @return			True if the attribute was found, false otherwise.
	 */
	bool GetIntAttribute(const FString& Name, int32& Value);

private:
	TSharedPtr<void> FileContext;
};

/**
 * Base class for our classes that output EXR files.
 */
class OPENEXRWRAPPER_API FBaseOutputFile
{
public:
	/**
	 * Constructor.
	 *
	 * @param DisplayWindowMin		Normally (0, 0).
	 * @param DisplayWindowMax		Normally (width - 1, height - 1).
	 * @param DataWindowMin			Normally (0, 0).
	 * @param DataWindowMax			Normally (width - 1, height - 1).
	 */
	FBaseOutputFile(
		const FIntPoint& DisplayWindowMin,
		const FIntPoint& DisplayWindowMax,
		const FIntPoint& DataWindowMin,
		const FIntPoint& DataWindowMax);

	/**
	 * Destructor.
	 */
	virtual ~FBaseOutputFile();

	/**
	 * Call this to add an attribute to the EXR file.
	 * This MUST be called before CreateOutputFile.
	 *
	 * @param Name		Name for this attribute.
	 * @param Value		Value for this attribute.
	 */
	void AddIntAttribute(const FString& Name, int32 Value);

	/**
	 * Call this to get the number of mipmap levels.
	 */
	virtual int32 GetNumberOfMipLevels() { return 0; }

protected:
	/** Stores the EXR header object. */
	void* Header;
	/** Stores the EXR object. */
	void* OutputFile;
};

/**
 * Use this to write out tiled EXR images.
 * 
 * Add any attributes after construction.
 * Then you can call CreateOutputFile.
 * After that, you can write out data to the file.
 */
class OPENEXRWRAPPER_API FTiledRgbaOutputFile : public FBaseOutputFile
{
public:
	/**
	 * Constructor.
	 * 
	 * @param DisplayWindowMin		Normally (0, 0).
	 * @param DisplayWindowMax		Normally (width - 1, height - 1).
	 * @param DataWindowMin			Normally (0, 0).
	 * @param DataWindowMax			Normally (width - 1, height - 1).
	 */
	FTiledRgbaOutputFile(
		const FIntPoint& DisplayWindowMin,
		const FIntPoint& DisplayWindowMax,
		const FIntPoint& DataWindowMin,
		const FIntPoint& DataWindowMax);

	/**
	 * Call this after adding any attributes BUT before doing anything else.
	 *
	 * @param FilePath			Filename to save to.
	 * @param TileWidth			Width of a tile.
	 * @param TileHeight		Height of a tile.
	 * @param NumChannels		Number of channels out write out.
	 * @param bIsMipsEnabled	True to enable mip mapping.
	 */
	void CreateOutputFile(const FString& FilePath,
		int32 TileWidth, int32 TileHeight, int32 NumChannels, bool bIsMipsEnabled);

	/**
	 * Call this prior to WriteTile to set where the data is coming from.
	 * 
	 * @param Buffer		Source of data.
	 * @param Stride		A pixels location is calculated by Buffer + x * StrideX + y * StrideY.
	 */
	void SetFrameBuffer(void* Buffer, const FIntPoint& Stride);

	/**
	 * Call this to write data to a specific tile.
	 *
	 * @param TileX			X coordinate of tile.
	 * @param TileY			Y coordinate of tile.
	 * @param MipLevel		Mipmap level of tile.
	 */
	void WriteTile(int32 TileX, int32 TileY, int32 MipLevel);

	//~ FBaseOutputFile interface.
	virtual int32 GetNumberOfMipLevels() override;
};

/**
 * Use this to write out tiled EXR images using the general interface.
 *
 * Add any attributes and AddChannel after construction.
 * Then you can call CreateOutputFile.
 * Then you can call AddFrameBufferChannel for each channel, and then SetFrameBuffer.
 * After that, you can write out data to the file.
 */
class OPENEXRWRAPPER_API FTiledOutputFile : public FBaseOutputFile
{
public:
	/**
	 * Constructor.
	 *
	 * @param DisplayWindowMin		Normally (0, 0).
	 * @param DisplayWindowMax		Normally (width - 1, height - 1).
	 * @param DataWindowMin			Normally (0, 0).
	 * @param DataWindowMax			Normally (width - 1, height - 1).
	 * @param bInIsTiled			True if you want tiles.
	 */
	FTiledOutputFile(
		const FIntPoint& DisplayWindowMin,
		const FIntPoint& DisplayWindowMax,
		const FIntPoint& DataWindowMin,
		const FIntPoint& DataWindowMax,
		bool bInIsTiled = true);

	/**
	 * Destructor.
	 */
	virtual ~FTiledOutputFile();

	/**
	 * Call this before CreateOutputFile for each channel in the image.
	 *
	 * @param Name		Name of channel (e.g. R, G, B, or A). 
	 */
	void AddChannel(const FString& Name);

	/**
	 * Call this after adding any attributes or channels BUT before doing anything else.
	 *
	 * @param FilePath			Filename to save to.
	 * @param TileWidth			Width of a tile.
	 * @param TileHeight		Height of a tile.
	 * @param bIsMipsEnabled	True to enable mip mapping.
	 * @param NumThreads		Number of threads for EXR to use.
	 */
	void CreateOutputFile(const FString& FilePath,
		int32 TileWidth, int32 TileHeight, bool bIsMipsEnabled, int32 NumThreads);

	/**
	 * Get the width of a mip level.
	 */
	int32 GetMipWidth(int32 MipLevel);

	/**
	 * Get the height of a mip level.
	 */
	int32 GetMipHeight(int32 MipLevel);

	/**
	 * Get number of horizontal tiles for a mip level.
	 */
	int32 GetNumXTiles(int32 MipLevel);

	/**
	 * Get number of vertical tiles for a mip level.
	 */
	int32 GetNumYTiles(int32 MipLevel);

	/**
	 * Call this before SetFrameBuffer for each channel in the frame buffer.
	 *
	 * @param Name			Name of channel (e.g. R, G, B, or A).
	 * @param Base			Address of the start of the data.
	 * @param Stride		A pixels location is calculated by Base + x * StrideX + y * StrideY.
	 */
	void AddFrameBufferChannel(const FString& Name, void* Base,
		const FIntPoint& Stride);

	/**
	 * Call this to change a channel in the frame buffer.
	 *
	 * @param Name			Name of channel (e.g. R, G, B, or A).
	 * @param Base			Address of the start of the data.
	 * @param Stride		A pixels location is calculated by Base + x * StrideX + y * StrideY.
	 */
	void UpdateFrameBufferChannel(const FString& Name, void* Base,
		const FIntPoint& Stride);

	/**
	 * Call this prior to WriteTile to set where the data is coming from.
	 *
	 * @param Buffer		Source of data.
	 * @param Stride		A pixels location is calculated by Buffer + x * StrideX + y * StrideY.
	 */
	void SetFrameBuffer();

	/**
	 * Call this to write data to a specific tile.
	 * If tiles are not enabled then this just writes out the whole image.
	 *
	 * @param TileX			X coordinate of tile.
	 * @param TileY			Y coordinate of tile.
	 * @param MipLevel		Mipmap level of tile.
	 */
	void WriteTile(int32 TileX, int32 TileY, int32 MipLevel);

	/**
	 * Call this to write data to some tiles.
	 * If tiles are not enabled then this just writes out the whole image.
	 *
	 * @param TileX1		X coordinate of first tile.
	 * @param TileX2		X coordinate of last tile.
	 * @param TileY1		Y coordinate of first tile.
	 * @param TileY2		Y coordinate of last tile.
	 * @param MipLevel		Mipmap level of tile.
	 */
	void WriteTiles(int32 TileX1, int32 TileX2, int32 TileY1, int32 TileY2, int32 MipLevel);

	//~ FBaseOutputFile interface.
	virtual int32 GetNumberOfMipLevels() override;

private:

	/** Stores the EXR frame buffer object. */
	void* FrameBuffer;
	/** True if tiles are enabled. */
	bool bIsTiled;
};

