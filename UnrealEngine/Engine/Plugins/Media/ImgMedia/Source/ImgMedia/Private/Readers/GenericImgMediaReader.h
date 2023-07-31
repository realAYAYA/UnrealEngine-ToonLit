// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ContainersFwd.h"
#include "IImgMediaReader.h"

class FImgMediaLoader;
class FRgbaInputFile;
class IImageWrapper;
class IImageWrapperModule;


/**
 * Implements a reader for various image sequence formats.
 */
class FGenericImgMediaReader
	: public IImgMediaReader
{
public:

	/**
	 * Create and initialize a new instance.
	 *
	 * @param InImageWrapperModule The image wrapper module to use.
	 */
	FGenericImgMediaReader(IImageWrapperModule& InImageWrapperModule, const TSharedRef<FImgMediaLoader, ESPMode::ThreadSafe>& InLoader);

	/** Virtual destructor. */
	virtual ~FGenericImgMediaReader() { }

public:

	//~ IImgMediaReader interface

	virtual bool GetFrameInfo(const FString& ImagePath, FImgMediaFrameInfo& OutInfo) override;
	virtual bool ReadFrame(int32 FrameId, const TMap<int32, FImgMediaTileSelection>& InMipTiles, TSharedPtr<FImgMediaFrame, ESPMode::ThreadSafe> OutFrame) override;
	virtual void CancelFrame(int32 FrameNumber) override {};

private:

	TSharedPtr<IImageWrapper> LoadFrameImage(const FString& ImagePath, TArray64<uint8>& OutBuffer, FImgMediaFrameInfo& OutInfo, bool bUseLoaderFirstFrame);

	/** The image wrapper module. */
	IImageWrapperModule& ImageWrapperModule;
	/** Our parent loader. */
	TWeakPtr<FImgMediaLoader, ESPMode::ThreadSafe> LoaderPtr;
};
