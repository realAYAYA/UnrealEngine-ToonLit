// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Graph/MovieGraphFilenameResolveParams.h"
#include "Misc/FrameRate.h"

#include "MovieGraphBlueprintLibrary.generated.h"

// Forward Declare
class UMovieGraphOutputSettingNode;

UCLASS(meta = (ScriptName = "MovieGraphLibrary"))
class MOVIERENDERPIPELINECORE_API UMovieGraphBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	* If InNode is valid, inspects the provided OutputsettingNode to determine if it wants to override the
	* Frame Rate, and if so, returns the overwritten frame rate. If nullptr, or it does not have the
	* bOverride_bUseCustomFrameRate flag set, then InDefaultrate is returned.
	* @param	InNode			- Optional, setting to inspect for a custom framerate.
	* @param	InDefaultRate	- The frame rate to use if the node is nullptr or doesn't want to override the rate.
	* @return					- The effective frame rate (taking into account the node's desire to override it). 
	*/
	UFUNCTION(BlueprintCallable, Category = "Movie Graph")
	static FFrameRate GetEffectiveFrameRate(UMovieGraphOutputSettingNode* InNode, const FFrameRate& InDefaultRate);

	/**
	* Takes a Movie Graph format string (in the form of {token}), a list of parameters (which normally come from the running UMovieGraphPipeline) and
	* resolves them into a string. Unknown tokens are ignored. Which tokens can be resolved depends on the contents of InParams, tokens from settings
	* rely on a evaluated config being provided, etc.
	* @param	InFormatString		- Format string to attempt to resolve.
	* @param	InParams			- A list of parameters to use as source data for the resolve step. Normally comes from the UMovieGraphPipeline instance,
	*								- but takes (mostly) POD here to allow using this function outside of the render runtime.
	* @param	OutMergedFormatArgs - The set of KVP for both filename formats and file metadata that is generated as a result of this. Provided in case you
	* 								- needed to do your own string resolving with the final dataset.
	* @return						- The resolved format string.
	*/
	UFUNCTION(BlueprintCallable, Category = "Movie Graph")
	static FString ResolveFilenameFormatArguments(const FString& InFormatString, const FMovieGraphFilenameResolveParams& InParams, FMovieGraphResolveArgs& OutMergedFormatArgs);

	/**
	* In case of overscan percentage being higher than 0, additional pixels are rendered. This function returns the resolution with overscan taken into account.
	* @param	InEvaluatedGraph	- The evaluated graph that will provide context for resolving the resolution
	* @param	InBranchName		- The graph branch that the output resolution should be resolved on
	* @return						- The output resolution, taking into account overscan
	*/
	UFUNCTION(BlueprintCallable, Category = "Movie Graph")
	static FIntPoint GetEffectiveOutputResolution(UMovieGraphEvaluatedConfig* InEvaluatedGraph, const FName& InBranchName);
};