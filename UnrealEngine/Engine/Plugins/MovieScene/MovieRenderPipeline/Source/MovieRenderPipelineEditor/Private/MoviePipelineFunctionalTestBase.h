// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "FunctionalTest.h"
#include "Misc/AutomationTest.h"
#include "MovieRenderPipelineDataTypes.h"

#include "MoviePipelineFunctionalTestBase.generated.h"

class UMoviePipelineQueue;
class UMoviePipeline;

/**
* Base class for Movie Pipeline functional tests which render pre-made queues
* and compare their output against pre-existing render outputs.
*/
UCLASS(Blueprintable)
class MOVIERENDERPIPELINEEDITOR_API AMoviePipelineFunctionalTestBase : public AFunctionalTest
{
	GENERATED_BODY()

public:
	AMoviePipelineFunctionalTestBase();

protected:
	// AFunctionalTest
	virtual void PrepareTest() override;
	virtual bool IsReady_Implementation() override;
	virtual void StartTest() override;
	// ~AFunctionalTest

	void OnJobShotFinished(FMoviePipelineOutputData InOutputData);
	void OnMoviePipelineFinished(FMoviePipelineOutputData InOutputData);
	void CompareRenderOutputToGroundTruth(FMoviePipelineOutputData InOutputData);

protected:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movie Pipeline")
	TObjectPtr<UMoviePipelineQueue> QueuePreset;

protected:
	UPROPERTY(Transient)
	TObjectPtr<UMoviePipeline> ActiveMoviePipeline;

	UPROPERTY(Transient)
	TObjectPtr<UMoviePipelineQueue> ActiveQueue;
};
