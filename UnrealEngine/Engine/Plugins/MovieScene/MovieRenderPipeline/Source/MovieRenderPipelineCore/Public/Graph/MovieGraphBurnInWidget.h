// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"

#include "MovieGraphBurnInWidget.generated.h"

/**
 * Base class for graph-based level sequence burn-ins.
 */
UCLASS(Blueprintable, Abstract)
class MOVIERENDERPIPELINECORE_API UMovieGraphBurnInWidget : public UUserWidget
{
public:
	GENERATED_BODY()

	/** 
	* Called on the first temporal and first spatial sample of each output frame.
	* @param	InGraphPipeline		The graph pipeline the burn-in is for. This will be consistent throughout a burn-in widget's life.
	* @param	InEvaluatedConfig	The evaluated graph that was used to generate this output frame.
	*/
	UFUNCTION(BlueprintImplementableEvent)
	void UpdateForGraph(UMovieGraphPipeline* InGraphPipeline, UMovieGraphEvaluatedConfig* InEvaluatedConfig);
};
