// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Blueprint/UserWidget.h"
#include "MovieRenderDebugWidget.generated.h"

class UMoviePipeline;

/**
* C++ Base Class for the debug widget that is drawn onto the game viewport
* (but not burned into the output files) that allow us to easily visualize
* the current state of the pipeline.
*/
UCLASS(Blueprintable, Abstract)
class UMovieRenderDebugWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintImplementableEvent)
	void OnInitializedForPipeline(UMoviePipeline* ForPipeline);
};