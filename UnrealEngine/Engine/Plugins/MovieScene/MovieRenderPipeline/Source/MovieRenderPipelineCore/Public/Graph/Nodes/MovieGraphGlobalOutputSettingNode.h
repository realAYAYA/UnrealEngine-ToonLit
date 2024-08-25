// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Graph/MovieGraphNamedResolution.h"
#include "Graph/MovieGraphNode.h"

#include "Misc/FrameRate.h"

#include "MovieGraphGlobalOutputSettingNode.generated.h"

USTRUCT(BlueprintType)
struct FMovieGraphVersioningSettings
{
	GENERATED_BODY()

	/**
	 * If true, {version} tokens specified in the Output Directory and File Name Format properties will automatically
	 * be incremented with each local render. If false, the version specified in Version Number will be used instead.
	 *
	 * Auto-versioning will search across all render branches and use the highest version found as the basis for the
	 * next version used.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Versioning")
	bool bAutoVersioning = true;
	
	/** The value to use for the version token if versions are not automatically incremented (Auto Version is off). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Versioning", meta = (UIMin = 1, UIMax = 50, ClampMin = 1))
	int32 VersionNumber = 1;
};

UCLASS()
class MOVIERENDERPIPELINECORE_API UMovieGraphGlobalOutputSettingNode : public UMovieGraphSettingNode
{
	GENERATED_BODY()
public:
	UMovieGraphGlobalOutputSettingNode();

	// UMovieGraphSettingNode Interface
	virtual void GetFormatResolveArgs(FMovieGraphResolveArgs& OutMergedFormatArgs, const FMovieGraphRenderDataIdentifier& InRenderDataIdentifier) const override;

#if WITH_EDITOR
	virtual FText GetNodeTitle(const bool bGetDescriptive) const override;
	virtual FText GetMenuCategory() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	virtual EMovieGraphBranchRestriction GetBranchRestriction() const override;
#endif
	// ~UMovieGraphSettingNode Interface

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_OutputDirectory : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_OutputResolution : 1;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_OutputFrameRate : 1;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_bOverwriteExistingOutput : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_ZeroPadFrameNumbers : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_FrameNumberOffset : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_HandleFrameCount : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_CustomPlaybackRangeStartFrame : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_CustomPlaybackRangeEndFrame : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_VersioningSettings : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_bFlushDiskWritesPerShot : 1;

	/** What directory should all of our output files be relative to. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "File Output", meta=(EditCondition="bOverride_OutputDirectory"))
	FDirectoryPath OutputDirectory;

	/** What resolution should our output files be exported at? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "File Output", meta = (EditCondition = "bOverride_OutputResolution"))
	FMovieGraphNamedResolution OutputResolution;
	
	/** What frame rate should the output files be exported at? This overrides the Display Rate of the target sequence. If not overwritten, uses the default Sequence Display Rate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "File Output", meta=(EditCondition="bOverride_OutputFrameRate"))
	FFrameRate OutputFrameRate;
	
	/** If true, output containers should attempt to override any existing files with the same name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "File Output", meta = (EditCondition = "bOverride_bOverwriteExistingOutput"))
	bool bOverwriteExistingOutput;

	/** How many digits should all output frame numbers be padded to? MySequence_1.png -> MySequence_0001.png. Useful for software that struggles to recognize frame ranges when non-padded. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "File Output", meta = (EditCondition = "bOverride_ZeroPadFrameNumbers", UIMin = "1", UIMax = "5", ClampMin = "1"))
	int32 ZeroPadFrameNumbers;

	/**
	* How many frames should we offset the output frame number by? This is useful when using handle frames on Sequences that start at frame 0,
	* as the output would start in negative numbers. This can be used to offset by a fixed amount to ensure there's no negative numbers.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite,  Category = "File Output", meta = (EditCondition = "bOverride_FrameNumberOffset"))
	int32 FrameNumberOffset;

	/**
	* Top level shot track sections will automatically be expanded by this many frames in both directions, and the resulting
	* additional time will be rendered as part of that shot. The inner sequence should have sections long enough to cover
	* this expanded range, otherwise these tracks will not evaluate during handle frames and may produce unexpected results.
	* This can be used to generate handle frames for traditional non linear editing tools.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frames", meta = (UIMin = 0, ClampMin = 0, EditCondition = "bOverride_HandleFrameCount"))
	int32 HandleFrameCount;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frames", meta = (EditCondition = "bOverride_CustomPlaybackRangeStartFrame"))
	int32 CustomPlaybackRangeStartFrame;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frames", meta = (EditCondition = "bOverride_CustomPlaybackRangeEndFrame"))
	int32 CustomPlaybackRangeEndFrame;

	/**
	 * Determines how versioning should be handled (Auto Version, Version Number, etc.).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Versioning", meta = (EditCondition = "bOverride_VersioningSettings"))
	FMovieGraphVersioningSettings VersioningSettings;

	/**
	* If true, the game thread will stall at the end of each shot to flush the rendering queue, and then flush any outstanding writes to disk, finalizing any
	* outstanding videos and generally completing the work. This is only relevant for scripting where scripts may do post-shot callback work.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scripting", meta = (EditCondition = "bOverride_bFlushDiskWritesPerShot"))
	bool bFlushDiskWritesPerShot;
};
