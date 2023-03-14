// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "UObject/ObjectMacros.h"
#include "UObject/TextProperty.h"
#include "Internationalization/Text.h"
#include "Containers/UnrealString.h"
#include "MoviePipelineMasterConfig.h"

#include "MoviePipelineEditorBlueprintLibrary.generated.h"

// Forward Declare
class UMoviePipelineMasterConfig;
class UMoviePipelineExecutorJob;
class ULevelSequence;

UCLASS(meta=(ScriptName="MoviePipelineEditorLibrary"))
class MOVIERENDERPIPELINEEDITOR_API UMoviePipelineEditorBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable, Category = "Movie Render Pipeline")
	static bool ExportConfigToAsset(const UMoviePipelineMasterConfig* InConfig, const FString& InPackagePath, const FString& InFileName, const bool bInSaveAsset, UMoviePipelineMasterConfig*& OutAsset, FText& OutErrorReason);

	/** Checks to see if any of the Jobs try to point to maps that wouldn't be valid on a remote render (ie: unsaved maps) */
	UFUNCTION(BlueprintPure, Category = "Movie Render Pipeline")
	static bool IsMapValidForRemoteRender(const TArray<UMoviePipelineExecutorJob*>& InJobs);

	/** Pop a dialog box that specifies that they cannot render due to never saved map. Only shows OK button. */
	UFUNCTION(BlueprintCallable, Category = "Movie Render Pipeline")
	static void WarnUserOfUnsavedMap();

	/** Take the specified Queue, duplicate it and write it to disk in the ../Saved/MovieRenderPipeline/ folder. Returns the duplicated queue. */
	UFUNCTION(BlueprintCallable, Category = "Movie Render Pipeline")
	static UMoviePipelineQueue* SaveQueueToManifestFile(UMoviePipelineQueue* InPipelineQueue, FString& OutManifestFilePath);

	/** Loads the specified manifest file and converts it into an FString to be embedded with HTTP REST requests. Use in combination with SaveQueueToManifestFile. */
	UFUNCTION(BlueprintCallable, Category = "Movie Render Pipeline")
	static FString ConvertManifestFileToString(const FString& InManifestFilePath);

	/** Create a job from a level sequence. Sets the map as the currently editor world, the author, the sequence and the job name as the sequence name on the new job. Returns the newly created job. */
	UFUNCTION(BlueprintCallable, Category = "Movie Render Pipeline")
	static UMoviePipelineExecutorJob* CreateJobFromSequence(UMoviePipelineQueue* InPipelineQueue, const ULevelSequence* InSequence);

	/** Ensure the job has the settings specified by the project settings added. If they're already added we don't modify the object so that we don't make it confused about whether or not you've modified the preset. */
	UFUNCTION(BlueprintCallable, Category = "Movie Render Pipeline")
	static void EnsureJobHasDefaultSettings(UMoviePipelineExecutorJob* InJob);

	/** Resolves as much of the output directory for this job into a usable directory path as possible. Cannot resolve anything that relies on shot name, frame numbers, etc. */
	UFUNCTION(BlueprintCallable, Category = "Movie Render Pipeline")
	static FString ResolveOutputDirectoryFromJob(UMoviePipelineExecutorJob* InJob);
};