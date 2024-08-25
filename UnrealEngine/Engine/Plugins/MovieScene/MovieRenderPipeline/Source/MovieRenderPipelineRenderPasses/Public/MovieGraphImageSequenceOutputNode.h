// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Graph/Nodes/MovieGraphFileOutputNode.h"

#include "IImageWrapper.h"
#include "MoviePipelineEXROutput.h"
#include "OpenColorIOColorSpace.h"
#include "Styling/AppStyle.h"
#include "Async/Future.h"

#include "MovieGraphImageSequenceOutputNode.generated.h"

// Forward Declare
class IImageWriteQueue;

/**
* The UMovieGraphImageSequenceOutputNode node is the base class for all image sequence outputs, such as 
* a series of jpeg, png, bmp, or .exr images. Create an instance of the appropriate class (such as 
* UMovieGraphImageSequenceOutputNode_JPG) instead of this abstract base class.
*/
UCLASS(Abstract)
class UMovieGraphImageSequenceOutputNode : public UMovieGraphFileOutputNode
{
	GENERATED_BODY()
public:
	UMovieGraphImageSequenceOutputNode();
	
	// UMovieGraphFileOutputNode Interface
	virtual void OnReceiveImageDataImpl(UMovieGraphPipeline* InPipeline, UE::MovieGraph::FMovieGraphOutputMergerFrame* InRawFrameData, const TSet<FMovieGraphRenderDataIdentifier>& InMask) override;
	virtual void OnAllFramesSubmittedImpl(UMovieGraphPipeline* InPipeline, TObjectPtr<UMovieGraphEvaluatedConfig>& InPrimaryJobEvaluatedGraph) override;
	virtual bool IsFinishedWritingToDiskImpl() const override;
	// ~UMovieGraphFileOutputNode Interface

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_OCIOConfiguration : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_OCIOContext : 1;

	/**
	* OCIO configuration/transform settings.
	*
	* Note: There are differences from the previous implementation in MRQ given that we are now doing CPU-side processing.
	* 1) This feature only works on desktop platforms when the OpenColorIO library is available.
	* 2) Users are now responsible for setting the renderer output space to Final Color (HDR) in Linear Working Color Space (SCS_FinalColorHDR).
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OCIO", DisplayName="OCIO Configuration", meta = (EditCondition = "bOverride_OCIOConfiguration"))
	FOpenColorIODisplayConfiguration OCIOConfiguration;

	/**
	* OCIO context of key-value string pairs, typically used to apply shot-specific looks (such as a CDL color correction, or a 1D grade LUT).
	* 
	* Notes:
	* 1) If a configuration asset base context was set, it remains active but can be overridden here with new key-values.
	* 2) Format tokens such as {shot_name} are supported and will get resolved before submission.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OCIO", DisplayName = "OCIO Context", meta = (EditCondition = "bOverride_OCIOContext"))
	TMap<FString, FString> OCIOContext;

protected:
	/** Convenience function to get the list of active composite passes from render data. */
	TArray<TPair<FMovieGraphRenderDataIdentifier, TUniquePtr<FImagePixelData>>> GetCompositedPasses(
		UE::MovieGraph::FMovieGraphOutputMergerFrame* InRawFrameData) const;

	/** Convenience function to create the output file name. */
	FString CreateFileName(
		UE::MovieGraph::FMovieGraphOutputMergerFrame* InRawFrameData,
		const UMovieGraphImageSequenceOutputNode* InParentNode,
		const UMovieGraphPipeline* InPipeline,
		const TPair<FMovieGraphRenderDataIdentifier, TUniquePtr<FImagePixelData>>& InRenderData,
		const EImageFormat InImageFormat,
		FMovieGraphResolveArgs& OutMergedFormatArgs) const;

protected:
	/** The output format (as known used by the ImageWriteQueue) to output into. */
	EImageFormat OutputFormat;

	/** Whether we enforce 8-bit depth on the output. */
	bool bQuantizeTo8Bit;

	/** A pointer to the image write queue used for asynchronously writing images */
	IImageWriteQueue* ImageWriteQueue;

	/** A fence to keep track of when the Image Write queue has fully flushed. */
	TFuture<void> FinalizeFence;
};

/**
 * Image sequence output node that can write single-layer EXR files.
 */
UCLASS()
class UMovieGraphImageSequenceOutputNode_EXR : public UMovieGraphImageSequenceOutputNode
{
	GENERATED_BODY()

public:
	UMovieGraphImageSequenceOutputNode_EXR()
	{
		OutputFormat = EImageFormat::EXR;
		bQuantizeTo8Bit = false;
		Compression = EEXRCompressionFormat::PIZ;
	}

	virtual void OnReceiveImageDataImpl(UMovieGraphPipeline* InPipeline, UE::MovieGraph::FMovieGraphOutputMergerFrame* InRawFrameData, const TSet<FMovieGraphRenderDataIdentifier>& InMask) override;

#if WITH_EDITOR
	virtual FText GetNodeTitle(const bool bGetDescriptive = false) const override 
	{ 
		static const FText EXRSequenceNodeName = NSLOCTEXT("MovieGraph", "NodeName_EXRSequence", ".exr Sequence");
		return EXRSequenceNodeName;
	}

	virtual FText GetKeywords() const override
	{
		static const FText Keywords = NSLOCTEXT("MovieGraph", "ImageSequenceOutputNode_EXR_Keywords", "exr image");
		return Keywords;
	}
	
	virtual FLinearColor GetNodeTitleColor() const override
	{
		return FLinearColor(0.047f, 0.654f, 0.537f);
	}
	
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override
	{
		static const FSlateIcon ImageSequenceIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.Texture2D");

		OutColor = FLinearColor::White;
		return ImageSequenceIcon;
	}
#endif
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_Compression : 1;
	
	/**
	 * Which compression method should the resulting EXR file be compressed with.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(EditCondition="bOverride_Compression"), Category = "EXR")
	EEXRCompressionFormat Compression;

protected:

	/** Convenience function to create a new EXR image write task, given a file name and compression format. */
	TUniquePtr<FEXRImageWriteTask> CreateImageWriteTask(
		FString InFileName,
		EEXRCompressionFormat InCompression
	) const;
	
	/** Convenience function to prepare the image write task's global file metadata. */
	void PrepareTaskGlobalMetadata(
		FEXRImageWriteTask& InOutImageTask,
		UE::MovieGraph::FMovieGraphOutputMergerFrame* InRawFrameData,
		TMap<FString, FString>& InMetadata
	) const;

	/** Convenience function to update the image write task for layer data. */
	void UpdateTaskPerLayer(
		FEXRImageWriteTask& InOutImageTask,
		const UMovieGraphImageSequenceOutputNode* InParentNode,
		TUniquePtr<FImagePixelData> InImageData,
		int32 InLayerIndex,
		const FString& InLayerName = {},
		const TMap<FString, FString>& InResolvedOCIOContext = {}
	) const;
};


/**
 * Image sequence output node that can write multi-layer EXR files.
 */
UCLASS()
class UMovieGraphImageSequenceOutputNode_MultiLayerEXR : public UMovieGraphImageSequenceOutputNode_EXR
{
	GENERATED_BODY()

public:
	UMovieGraphImageSequenceOutputNode_MultiLayerEXR()
		: UMovieGraphImageSequenceOutputNode_EXR()
	{
		// Multi-layer default excludes {layer_name}.
		FileNameFormat = TEXT("{sequence_name}.{frame_number}");
	}

	virtual EMovieGraphBranchRestriction GetBranchRestriction() const override
	{
		return EMovieGraphBranchRestriction::Globals;
	}

	virtual void OnReceiveImageDataImpl(UMovieGraphPipeline* InPipeline, UE::MovieGraph::FMovieGraphOutputMergerFrame* InRawFrameData, const TSet<FMovieGraphRenderDataIdentifier>& InMask) override;

#if WITH_EDITOR
	virtual FText GetNodeTitle(const bool bGetDescriptive = false) const override
	{
		static const FText EXRSequenceNodeName = NSLOCTEXT("MovieGraph", "NodeName_EXRSequenceMultilayer", ".exr Sequence (Multilayer)");
		return EXRSequenceNodeName;
	}

	virtual FText GetKeywords() const override
	{
		static const FText Keywords = NSLOCTEXT("MovieGraph", "ImageSequenceOutputNode_EXRMultilayer_Keywords", ".exr image (Multilayer)");
		return Keywords;
	}
		
	virtual FLinearColor GetNodeTitleColor() const override
	{
		return FLinearColor(0.047f, 0.654f, 0.537f);
	}

	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override
	{
		static const FSlateIcon ImageSequenceIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.Texture2D");

		OutColor = FLinearColor::White;
		return ImageSequenceIcon;
	}

	virtual FText GetMenuCategory() const override
	{
		return NSLOCTEXT("MovieGraphNodes", "FileOutputGraphNode_Category", "Output Type");
	}
#endif

private:
	/**
	 * Generates filenames for each render pass (renderID), placed in OutFilenameToRenderIDs. Also provides the resolve
	 * args that were created when resolving the filename in OutFilenameToResolveArgs. The generated mapping ensures
	 * that each filename only points to renderIDs with a common resolution, since currently EXRs can only contain layers
	 * with the same resolution.
	 */
	void GetFilenameToRenderIDMappings(
		const UMovieGraphImageSequenceOutputNode_MultiLayerEXR* InParentNode,
		UMovieGraphPipeline* InPipeline, UE::MovieGraph::FMovieGraphOutputMergerFrame* InRawFrameData,
		TMap<FString, TArray<FMovieGraphRenderDataIdentifier>>& OutFilenameToRenderIDs,
		TMap<FString, FMovieGraphResolveArgs>& OutFilenameToResolveArgs) const;
	
	/**
	 * Generates the filename that the EXR will be written to, as well as the resulting resolve args via OutResolveArgs.
	 * Use GetFilenameToRenderIDMappings() to guarantee that the filename respects EXR limitations.
	 */
	FString ResolveOutputFilename(
		const UMovieGraphImageSequenceOutputNode_MultiLayerEXR* InParentNode,
		const UMovieGraphPipeline* InPipeline, const int32 ResolutionIndex, const UE::MovieGraph::FMovieGraphOutputMergerFrame* InRawFrameData,
		const FName& InBranchName, FMovieGraphResolveArgs& OutResolveArgs) const;
};

/**
* Save the images generated by the Movie Graph Pipeline as an lossless 8 bit bmp format. This can
* be useful in rare occasions (bmp files are uncompressed but larger). sRGB is applied.
* No metadata is supported.
*/
UCLASS()
class UMovieGraphImageSequenceOutputNode_BMP : public UMovieGraphImageSequenceOutputNode
{
	GENERATED_BODY()

public:
	UMovieGraphImageSequenceOutputNode_BMP()
	{
		OutputFormat = EImageFormat::BMP;
		bQuantizeTo8Bit = true;
	}

#if WITH_EDITOR
	virtual FText GetNodeTitle(const bool bGetDescriptive = false) const override 
	{ 
		if(bGetDescriptive)
		{
			return NSLOCTEXT("MovieGraph", "ImgSequenceBMPSetting_NodeTitleFull", ".bmp Sequence\n[8bit]"); 
		}
		return NSLOCTEXT("MovieGraph", "ImgSequenceBMPSetting_NodeTitleShort", ".bmp Sequence");
	}

	virtual FText GetKeywords() const override
	{
		static const FText Keywords = NSLOCTEXT("MovieGraph", "ImageSequenceOutputNode_BMP_Keywords", "bmp image");
		return Keywords;
	}
	
	virtual FLinearColor GetNodeTitleColor() const override
	{
		return FLinearColor(0.047f, 0.654f, 0.537f);
	}
	
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override
	{
		static const FSlateIcon ImageSequenceIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.Texture2D");

		OutColor = FLinearColor::White;
		return ImageSequenceIcon;
	}
#endif
};


/**
* Save the images generated by the Movie Graph Pipeline as an 8 bit jpg format. JPEG image files
* are lossy, but a good balance between compression speed and final filesize. sRGB is applied.
* No metadata is supported.
*/
UCLASS()
class UMovieGraphImageSequenceOutputNode_JPG : public UMovieGraphImageSequenceOutputNode
{
	GENERATED_BODY()

public:
	UMovieGraphImageSequenceOutputNode_JPG()
	{
		OutputFormat = EImageFormat::JPEG;
		bQuantizeTo8Bit = true;
	}

#if WITH_EDITOR
	virtual FText GetNodeTitle(const bool bGetDescriptive = false) const override 
	{ 
		if(bGetDescriptive)
		{
			return NSLOCTEXT("MovieGraph", "ImgSequenceJPGSetting_NodeTitleFull", ".jpg Sequence\n[8bit]"); 
		}
		return NSLOCTEXT("MovieGraph", "ImgSequenceJPGSetting_NodeTitleShort", ".jpg Sequence");
	}

	virtual FText GetKeywords() const override
	{
		static const FText Keywords = NSLOCTEXT("MovieGraph", "ImageSequenceOutputNode_JPG_Keywords", "jpg jpeg image");
		return Keywords;
	}
	
	virtual FLinearColor GetNodeTitleColor() const override
	{
		return FLinearColor(0.047f, 0.654f, 0.537f);
	}
	
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override
	{
		static const FSlateIcon ImageSequenceIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.Texture2D");

		OutColor = FLinearColor::White;
		return ImageSequenceIcon;
	}
#endif
};

/**
* Save the images generated by the Movie Graph Pipeline as an 8 bit png format. PNG image files
* are lossless but slow to compress and have a larger final filesize than JPEG. sRGB is applied.
* No metadata is supported.
*/
UCLASS()
class UMovieGraphImageSequenceOutputNode_PNG : public UMovieGraphImageSequenceOutputNode
{
	GENERATED_BODY()

public:
	UMovieGraphImageSequenceOutputNode_PNG()
	{
		OutputFormat = EImageFormat::PNG;

		// Note: we could offer linear 16-bit pngs simply by letting users turn this to false.
		bQuantizeTo8Bit = true;
	}

#if WITH_EDITOR
	virtual FText GetNodeTitle(const bool bGetDescriptive = false) const override 
	{ 
		if(bGetDescriptive)
		{
			return NSLOCTEXT("MovieGraph", "ImgSequencePNGSetting_NodeTitleFull", ".png Sequence\n[8bit]"); 
		}
		return NSLOCTEXT("MovieGraph", "ImgSequencePNGSetting_NodeTitleShort", ".png Sequence");
	}

	virtual FText GetKeywords() const override
	{
		static const FText Keywords = NSLOCTEXT("MovieGraph", "ImageSequenceOutputNode_PNG_Keywords", "png image");
		return Keywords;
	}
	
	virtual FLinearColor GetNodeTitleColor() const override
	{
		return FLinearColor(0.047f, 0.654f, 0.537f);
	}
	
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override
	{
		static const FSlateIcon ImageSequenceIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.Texture2D");

		OutColor = FLinearColor::White;
		return ImageSequenceIcon;
	}
#endif
};

