// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "ImageWriteTask.h"
#include "ImagePixelData.h"
#include "MoviePipelineImageSequenceOutput.h"
#include "Misc/StringFormatArg.h"

#if WITH_UNREALEXR
THIRD_PARTY_INCLUDES_START
#include "Imath/ImathBox.h"
#include "OpenEXR/ImfArray.h"
#include "OpenEXR/ImfChannelList.h"
#include "OpenEXR/ImfHeader.h"
#include "OpenEXR/ImfIO.h"
#include "OpenEXR/ImfInputFile.h"
#include "OpenEXR/ImfOutputFile.h"
#include "OpenEXR/ImfRgbaFile.h"
#include "OpenEXR/ImfStdIO.h"
THIRD_PARTY_INCLUDES_END
#endif // WITH_UNREALEXR

#include "MoviePipelineEXROutput.generated.h"

class UMoviePipelineColorSetting;
class FEXRImageWriteTask;

namespace UE
{
	namespace MoviePipeline
	{
		/** Collection of color space metadata for EXR. */
		struct FEXRColorSpaceMetadata
		{
			FString SourceName;
			FString DestinationName;
			TArray<FVector2d> Chromaticities;
		};

		/** Update the image write task with color space metadata from the OpenColorIO transform. */
		void UpdateColorSpaceMetadata(const FOpenColorIOColorConversionSettings& InConversionSettings, FEXRImageWriteTask& InOutImageTask);

		/** Update the image write task with color space metadata from the render capture source mode. */
		void UpdateColorSpaceMetadata(ESceneCaptureSource InSceneCaptureSource, FEXRImageWriteTask& InOutImageTask);
	}
}

// Exr compression format options. Exactly matches the exr library Imf::Compression enum.
UENUM(BlueprintType)
enum class EEXRCompressionFormat : uint8
{
	/** No compression is applied. */
	None = 0,
	/** This compression method is fast, and works well for images with large flat areas but yields worse results for grainy images. Lossless. */
	RLE = 1,
	/** This compression method is similar to ZIP but compresses only one image row at a time. Lossless. */
	ZIPS = 2 UMETA(DisplayName = "ZIP (1 scanline)"),
	/** Good compression quality for images with low amounts of noise. This compression method operates in in blocks of 16 scan lines. Lossless. */
	ZIP = 3  UMETA(DisplayName = "ZIP (16 scanlines)"),
	/** Good compression quality for grainy images. Lossless.*/
	PIZ = 4,
	/** This format only stores 24 bits of the 32 bit data and has subsequently a significant loss of precision. This method is only applied when saving in FLOAT color depth. HALF and UINT remain unchanged. Lossy. */
	PXR24 = 5,
	/** This compression method only applies to images stored in HALF color depth. Blocks of 4×4 pixels are stored with using only 14 byte each (instead of the 32 byte they would normally need). Each block is compressed to the exact same size. Different images with the same dimensions require the same storage space regardless of image content. Lossy. */
	B44 = 6,
	/** A modified version of B44. If all pixels in a 4*4 block have the same color it will use only 3 instead of 14 byte. */
	B44A = 7,
	/** Lossy DCT-based compression for RGB channels. Alpha and other channels are uncompressed. More efficient than DWAB for partial buffer access on read in 3rd party tools. */
	DWAA = 8,
	/** Similar to DWAA but goes in blocks of 256 scanlines instead of 32. More efficient disk space and faster to decode than DWAA. */
	DWAB = 9,

	Max UMETA(Hidden)
};

#if WITH_UNREALEXR
class FEXRImageWriteTask : public IImageWriteTaskBase
{
public:

	/** The filename to write to */
	FString Filename;

	/** True if this task is allowed to overwrite an existing file, false otherwise. */
	bool bOverwriteFile;
	
	/** Compression method used for the resulting EXR files. */
	EEXRCompressionFormat Compression;

	/** When using a lossy compression format, what is the base-error (CompressionLevel/100000) */
	int32 CompressionLevel;

	/** A function to invoke on the game thread when the task has completed */
	TFunction<void(bool)> OnCompleted;
	
	/** Width/Height of the image data. All samples should match this. */
	int32 Width;

	int32 Height;

	/** A set of key/value pairs to write into the exr file as metadata. */
	TMap<FString, FStringFormatArg> FileMetadata;

	/** The image data to write. Supports multiple layers of different bitdepths. */
	TArray<TUniquePtr<FImagePixelData>> Layers;

	/** Per-layer array of preprocessors to apply serially to the pixel data when this task is executed. */
	TSortedMap<int32, TArray<FPixelPreProcessor>> PixelPreprocessors;

	/** Optional. A mapping between the FImagePixelData and a name. The standard is that the default layer is nameless (at which point it would be omitted) and other layers are prefixed. */
	TMap<FImagePixelData*, FString> LayerNames;

	/** Overscan info used to create apropriate dataWindow for EXR output. Goes from 0.0 to 1.0. */
	float OverscanPercentage;

	/** Color space chromaticity metadata. */
	TArray<FVector2d> ColorSpaceChromaticities;

	FEXRImageWriteTask()
		: bOverwriteFile(true)
		, Compression(EEXRCompressionFormat::PIZ)
		, CompressionLevel(45)
		, OverscanPercentage(0.0f)
	{}

public:

	virtual bool RunTask() override final;
	virtual void OnAbandoned() override final;

private:

	/**
	 * Run the task, attempting to write out the raw data using the currently specified parameters
	 *
	 * @return true on success, false on any failure
	 */
	bool WriteToDisk();

	/**
	 * Ensures that the desired output filename is writable, deleting an existing file if bOverwriteFile is true
	 *
	 * @return True if the file is writable and the task can proceed, false otherwise
	 */
	bool EnsureWritableFile();

	/**
	* Adds arbitrary key/value pair metadata to the header of the file.
	*/
	void AddFileMetadata(Imf::Header& InHeader);

	/**
	 * Run over all the processors for the pixel data
	 */
	void PreProcess();

	template <Imf::PixelType OutputFormat>
	int64 CompressRaw(Imf::Header& InHeader, Imf::FrameBuffer& InFrameBuffer, FImagePixelData* InLayer);
};
#endif // WITH_UNREALEXR

UCLASS()
class MOVIERENDERPIPELINERENDERPASSES_API UMoviePipelineImageSequenceOutput_EXR : public UMoviePipelineImageSequenceOutputBase
{
	GENERATED_BODY()
public:
#if WITH_EDITOR
	virtual FText GetDisplayText() const override { return NSLOCTEXT("MovieRenderPipeline", "ImgSequenceEXRSettingDisplayName", ".exr Sequence [16bit]"); }
#endif
public:
	UMoviePipelineImageSequenceOutput_EXR()
	{
		OutputFormat = EImageFormat::EXR;
		Compression = EEXRCompressionFormat::PIZ;
		bMultilayer = true;
	}

	virtual void OnReceiveImageDataImpl(FMoviePipelineMergerOutputFrame* InMergedOutputFrame) override;
	virtual bool IsAlphaAllowed() const override { return true; }

public:
	/**
	* Which compression method should the resulting EXR file be compressed with
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EXR")
	EEXRCompressionFormat Compression;

	/**
	* Should we write all render passes to the same exr file? Not all software supports multi-layer exr files.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EXR")
	bool bMultilayer;

};
