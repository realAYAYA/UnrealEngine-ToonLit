// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Graph/MovieGraphNode.h"
#include "MovieGraphDebugNode.generated.h"

/** A node which configures various debug settings that may be useful when debugging an issue. */
UCLASS()
class MOVIERENDERPIPELINECORE_API UMovieGraphDebugSettingNode : public UMovieGraphSettingNode
{
	GENERATED_BODY()

public:
	UMovieGraphDebugSettingNode() = default;

	virtual EMovieGraphBranchRestriction GetBranchRestriction() const override { return EMovieGraphBranchRestriction::Globals; }

#if WITH_EDITOR
	virtual FText GetNodeTitle(const bool bGetDescriptive = false) const override;
	virtual FText GetMenuCategory() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
#endif

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_bCaptureFramesWithRenderDoc : 1;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_bCaptureUnrealInsightsTrace : 1;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_UnrealInsightsTraceFileNameFormat : 1;

	/** 
	* If true, automatically trigger RenderDoc to capture rendering information. RenderDoc plugin must be enabled, 
	* and the editor must have been launched with -AttachRenderDoc. Resulting capture will be in /Saved/RenderDocCaptures.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (EditCondition = "bOverride_bCaptureFramesWithRenderDoc"))
	bool bCaptureFramesWithRenderDoc;
	
	/** 
	* If true, automatically capture an Unreal Insights trace file for the duration of the render.
	* Resulting capture will be in the global Output Directory for the job.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (EditCondition = "bOverride_bCaptureUnrealInsightsTrace"))
	bool bCaptureUnrealInsightsTrace;

	/** 
	* If true, automatically capture an Unreal Insights trace file for the duration of the render.
	* Resulting capture will be in the global Output Directory for the job.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (EditCondition = "bOverride_UnrealInsightsTraceFileNameFormat"))
	FString UnrealInsightsTraceFileNameFormat = TEXT("{sequence_name}_UnrealInsights");
};