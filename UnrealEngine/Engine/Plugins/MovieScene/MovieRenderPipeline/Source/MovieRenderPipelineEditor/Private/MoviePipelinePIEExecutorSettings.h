// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Engine/DeveloperSettings.h"
#include "MoviePipelinePIEExecutorSettings.generated.h"

/**
* This is the implementation responsible for executing the rendering of
* multiple movie pipelines within the editor using PIE.
*/
UCLASS(BlueprintType, config = Editor, defaultconfig, meta = (DisplayName = "Movie Pipeline In Editor"))
class MOVIERENDERPIPELINEEDITOR_API UMoviePipelinePIEExecutorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Gets the settings container name for the settings, either Project or Editor */
	virtual FName GetContainerName() const override { return FName("Project"); }
	/** Gets the category for the settings, some high level grouping like, Editor, Engine, Game...etc. */
	virtual FName GetCategoryName() const override { return FName("Plugins"); }

	/**
	* How long should we wait after being initialized to start doing any work? This can be used
	* to work around situations where the game is not fully loaded by the time the pipeline
	* is automatically started and it is important that the game is fully loaded before we do
	* any work (such as evaluating frames for warm-up).
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, config, meta = (UIMin = "0", ClampMin = "0", UIMax = "150"), Category = "Startup")
	int32 InitialDelayFrameCount;
};