// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/MovieGraphBlueprintLibrary.h"

#include "Graph/Nodes/MovieGraphOutputSettingNode.h"
#include "Graph/Nodes/MovieGraphRenderLayerNode.h"
#include "MoviePipelineBlueprintLibrary.h"
#include "MoviePipelineUtils.h"

FFrameRate UMovieGraphBlueprintLibrary::GetEffectiveFrameRate(UMovieGraphOutputSettingNode* InNode, const FFrameRate& InDefaultRate)
{
	if (InNode && InNode->bOverride_OutputFrameRate)
	{
		return InNode->OutputFrameRate;
	}

	return InDefaultRate;
}

FString UMovieGraphBlueprintLibrary::ResolveFilenameFormatArguments(const FString& InFormatString, const FMovieGraphFilenameResolveParams& InParams, FMovieGraphResolveArgs& OutMergedFormatArgs)
{
	// There are a number of different sources for file/metadata KVPs. 
	// UMovieGraphPipeline - these are handled as resolve params to allow resolving during edit time
	// UMoviePipelineExecutorJob - author, sequence, level, etc.
	// UMovieGraphSettingNode - every node can contribute it's own file/metadata KVP based on their own settings
	
	bool bOverwriteExisting = true;

	// Data from the UMovieGraphPipeline comes from the resolve params themselves to allow resolving without a runtime instance.
	{
		// Zero-pad our frame numbers when we format the strings. Some programs struggle when ingesting frames that 
		// go 1,2,3,...,10,11. To work around this issue we allow the user to specify how many zeros they want to
		// pad the numbers with, 0001, 0002, etc. We also allow offsetting the output frame numbers, this is useful
		// when your sequence starts at zero and you use handle frames (which would cause negative output frame 
		// numbers), so we allow the user to add a fixed amount to all output to ensure they are positive.
		int32 FrameNumberOffset = InParams.FrameNumberOffset;
		FString FrameNumber = UE::MoviePipeline::GetPaddingFormatString(InParams.ZeroPadFrameNumberCount, InParams.RootFrameNumber + FrameNumberOffset); // Sequence Frame #
		FString FrameNumberShot = UE::MoviePipeline::GetPaddingFormatString(InParams.ZeroPadFrameNumberCount, InParams.ShotFrameNumber + FrameNumberOffset); // Shot Frame #
		FString FrameNumberRel = UE::MoviePipeline::GetPaddingFormatString(InParams.ZeroPadFrameNumberCount, InParams.RootFrameNumberRel + FrameNumberOffset); // Relative to 0
		FString FrameNumberShotRel = UE::MoviePipeline::GetPaddingFormatString(InParams.ZeroPadFrameNumberCount, InParams.ShotFrameNumberRel + FrameNumberOffset); // Relative to 0 within the shot.

		// Ensure they used relative frame numbers in the output so they get the right number of output frames.
		if (InParams.bForceRelativeFrameNumbers)
		{
			FrameNumber = FrameNumberRel;
			FrameNumberShot = FrameNumberShotRel;
		}

		FString ShotName = InParams.Shot ? InParams.Shot->OuterName : FString();
		ShotName = ShotName.Len() > 0 ? ShotName : TEXT("NoShot");

		// Use the legacy system to add filename and metadata kvp
		MoviePipeline::GetOutputStateFormatArgs(OutMergedFormatArgs.FilenameArguments, OutMergedFormatArgs.FileMetadata, FrameNumber, FrameNumberShot, FrameNumberRel, FrameNumberShotRel, TEXT("DummyCameraToken"), ShotName);

		// Look up the Render Layer display name from the evaluated config if possible.
		FString RenderLayerName = InParams.RenderDataIdentifier.RootBranchName.ToString();
		if (InParams.EvaluatedConfig)
		{
			const bool bIncludeCDOs = false;
			TObjectPtr<UMovieGraphRenderLayerNode> RenderLayerNode = InParams.EvaluatedConfig->GetSettingForBranch<UMovieGraphRenderLayerNode>(InParams.RenderDataIdentifier.RootBranchName, bIncludeCDOs);
			if (RenderLayerNode)
			{
				RenderLayerName = RenderLayerNode->GetRenderLayerName();
			}

			TObjectPtr<UMovieGraphOutputSettingNode> OutputSettingNode = InParams.EvaluatedConfig->GetSettingForBranch<UMovieGraphOutputSettingNode>(InParams.RenderDataIdentifier.RootBranchName, bIncludeCDOs);
			if(OutputSettingNode)
			{
				bOverwriteExisting = OutputSettingNode->bOverwriteExistingOutput;
			}
		}

		// Add on Render Data Identifier, overwriting the dummy camera name above.
		OutMergedFormatArgs.FilenameArguments.Add(TEXT("camera_name"), InParams.RenderDataIdentifier.CameraName);
		OutMergedFormatArgs.FilenameArguments.Add(TEXT("renderer_name"), InParams.RenderDataIdentifier.RendererName);
		OutMergedFormatArgs.FilenameArguments.Add(TEXT("renderer_sub_name"), InParams.RenderDataIdentifier.SubResourceName);
		OutMergedFormatArgs.FilenameArguments.Add(TEXT("render_layer"), RenderLayerName);

		OutMergedFormatArgs.FileMetadata.Add(TEXT("unreal/cameraName"), InParams.RenderDataIdentifier.CameraName);
		OutMergedFormatArgs.FileMetadata.Add(TEXT("unreal/rendererName"), InParams.RenderDataIdentifier.RendererName);
		OutMergedFormatArgs.FileMetadata.Add(TEXT("unreal/rendererSubName"), InParams.RenderDataIdentifier.SubResourceName);
		OutMergedFormatArgs.FileMetadata.Add(TEXT("unreal/renderLayer"), RenderLayerName);
	}


	// Add data from the job. ToDo: This probably needs to come from the job itself, so you can plug in different data sources eventually
	{
		// Frame Rate we handle separately, because it needs to be resolved.
		double FrameRate = InParams.DefaultFrameRate.AsDecimal();
		if (InParams.EvaluatedConfig)
		{
			const bool bIncludeCDOs = false;
			UMovieGraphOutputSettingNode* OutputSettingNode = InParams.EvaluatedConfig->GetSettingForBranch<UMovieGraphOutputSettingNode>(InParams.RenderDataIdentifier.RootBranchName, bIncludeCDOs);
			FrameRate = GetEffectiveFrameRate(OutputSettingNode, InParams.DefaultFrameRate).AsDecimal();
		}

		OutMergedFormatArgs.FilenameArguments.Add(TEXT("frame_rate"), FString::SanitizeFloat(FrameRate));
		OutMergedFormatArgs.FileMetadata.Add(TEXT("unreal/frameRate"), FString::SanitizeFloat(FrameRate));

		if (InParams.Job)
		{
			FString SequenceName = InParams.Job->Sequence.GetAssetName();
			OutMergedFormatArgs.FilenameArguments.Add(TEXT("sequence_name"), SequenceName);
			OutMergedFormatArgs.FileMetadata.Add(TEXT("unreal/sequenceName"), SequenceName);
		}


		// Add KVP data from the job (date, time, job name, job author, job comment)
		UE::MoviePipeline::GetSharedFormatArguments(OutMergedFormatArgs.FilenameArguments, OutMergedFormatArgs.FileMetadata, InParams.InitializationTime, InParams.Version, InParams.Job);
	}
	

	//  Now get the settings from our config. We need to gather KVP data from all possible nodes, even if not expressed in your configuration. This is because you might want to
	// always use the {ts_count} token even if you don't have a Temporal Sample Count node to add it. So we loop through all the possible class types, and call a function on the 
	// CDO, but then we pass that class type from the evaluated config (if it exists), and we pass the CDO as an argument if it doesn't.
	TArray<UClass*> AllSettingsNodeClasses = UE::MovieRenderPipeline::FindMoviePipelineSettingClasses(UMovieGraphSettingNode::StaticClass());

	// ToDo: This loops through class iterators every frame, we should probably initialize a copy of everything into the flattened config, since we could cache the classes
	// once per run there. We don't cache the returned results here because you could potentially add/remove classes (via Blueprints) which would invalidate our cache.
	for (UClass* InClass : AllSettingsNodeClasses)
	{
		UMovieGraphSettingNode* SettingInstance = nullptr;
		if (InParams.EvaluatedConfig)
		{
			const bool bIncludeCDOs = true;
			const bool bExactMatch = true;
			SettingInstance = InParams.EvaluatedConfig->GetSettingForBranch(InClass, InParams.RenderDataIdentifier.RootBranchName, bIncludeCDOs, bExactMatch);
		}

		if (SettingInstance)
		{
			SettingInstance->GetFormatResolveArgs(OutMergedFormatArgs);
		}
	}

	// Copy the metadata in the parameters into the merged output version
	OutMergedFormatArgs.FileMetadata = InParams.FileMetadata;

	// We expect the incoming string to have a {file_dup} token where they want file duplication numbers to be handled
	// but we don't actually want a value there (unless there's a collision). So we override it with an empty string by default.
	OutMergedFormatArgs.FilenameArguments.Add(TEXT("file_dup"), FString());

	// Overwrite any {tokens} with the user-supplied overrides if needed. This allows different requesters to share the same variables (ie: filename extension)
	for (const TPair<FString, FString>& KVP : InParams.FileNameFormatOverrides)
	{
		OutMergedFormatArgs.FilenameArguments.Add(KVP.Key, KVP.Value);
	}

	// Convert from our Python/BP exposed <FString, FString> to a named argument array for the formatter.
	FStringFormatNamedArguments NamedArgs;
	for (const TPair<FString, FString>& Argument : OutMergedFormatArgs.FilenameArguments)
	{
		NamedArgs.Add(Argument.Key, Argument.Value);
	}

	// Apply all of our named args to the file and generate a path.
	FString BaseFilename = FString::Format(*InFormatString, NamedArgs);
	if (InParams.bEnsureAbsolutePath)
	{
		BaseFilename = FPaths::ConvertRelativePathToFull(BaseFilename);
	}

	// Fix-up slashes
	FPaths::NormalizeFilename(BaseFilename);

	
	// Check that it's a valid path that we can write to.
	if (UE::MoviePipeline::CanWriteToFile(*BaseFilename, bOverwriteExisting))
	{
		return BaseFilename;
	}

	// The base filename must contain {file_dup} at this point, otherwise we're going to
	// be stuck in an infinite loop of never resolving names as we'd just keep checking the
	// base name!
	if (!BaseFilename.Contains(TEXT("{file_dup}")))
	{
		BaseFilename.Append(TEXT("{file_dup}"));
	}
	FString ThisTry = BaseFilename;

	// If we got here it means that there was already a file there with that name and bOverwriteExisting was false.
	// So we start by swapping in _(2) where the file_dup token is, and try again. If that fails, _(3), etc. We start
	// at _(2) because the file they're trying to write (foo.png) is technically "_(1)", so we start the rename process
	// at _(2) so it accurately reflects that this is the second verison of that file.
	int32 DuplicateIndex = 2;
	while (true)
	{
		NamedArgs.Add(TEXT("file_dup"), FString::Printf(TEXT("_(%d)"), DuplicateIndex));

		// Re-resolve the format string now that we've reassigned frame_dup to a number.
		ThisTry = FString::Format(*InFormatString, NamedArgs);

		// If the file doesn't exist, we can use that, else, increment the index and try again
		if (UE::MoviePipeline::CanWriteToFile(*ThisTry, bOverwriteExisting))
		{
			return ThisTry;
		}

		++DuplicateIndex;
	}
}

FIntPoint UMovieGraphBlueprintLibrary::GetEffectiveOutputResolution(UMovieGraphEvaluatedConfig* InEvaluatedGraph, const FName& InBranchName)
{
	if (!InEvaluatedGraph)
	{
		FFrame::KismetExecutionMessage(TEXT("InEvaluatedGraph cannot be null."), ELogVerbosity::Error);
		return FIntPoint();
	}
	
	constexpr bool bIncludeCDOs = true;
	const UMovieGraphOutputSettingNode* OutputSetting = InEvaluatedGraph->GetSettingForBranch<UMovieGraphOutputSettingNode>(InBranchName, bIncludeCDOs);
	if (!ensure(OutputSetting))
	{
		return FIntPoint();
	}
	
	return UMoviePipelineBlueprintLibrary::Utility_GetEffectiveOutputResolution(0 /* TODO: Overscan percentage needs to be provided */, OutputSetting->OutputResolution);
}