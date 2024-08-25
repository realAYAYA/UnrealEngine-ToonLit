// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/MovieGraphBlueprintLibrary.h"

#include "Graph/MovieGraphPipeline.h"
#include "Graph/MovieGraphProjectSettings.h"
#include "Graph/Nodes/MovieGraphCameraNode.h"
#include "Graph/Nodes/MovieGraphCommandLineEncoderNode.h"
#include "Graph/Nodes/MovieGraphFileOutputNode.h"
#include "Graph/Nodes/MovieGraphGlobalOutputSettingNode.h"
#include "Graph/Nodes/MovieGraphRenderLayerNode.h"
#include "MoviePipelineBlueprintLibrary.h"
#include "MoviePipelineUtils.h"

#include "HAL/FileManager.h"
#include "Internationalization/Regex.h"

FFrameRate UMovieGraphBlueprintLibrary::GetEffectiveFrameRate(UMovieGraphGlobalOutputSettingNode* InNode, const FFrameRate& InDefaultRate)
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

		// Add on Render Data Identifier, overwriting the dummy camera name above.
		OutMergedFormatArgs.FilenameArguments.Add(TEXT("camera_name"), InParams.RenderDataIdentifier.CameraName);
		OutMergedFormatArgs.FilenameArguments.Add(TEXT("renderer_name"), InParams.RenderDataIdentifier.RendererName);
		OutMergedFormatArgs.FilenameArguments.Add(TEXT("renderer_sub_name"), InParams.RenderDataIdentifier.SubResourceName);
		OutMergedFormatArgs.FilenameArguments.Add(TEXT("layer_name"), InParams.RenderDataIdentifier.LayerName);
		OutMergedFormatArgs.FilenameArguments.Add(TEXT("branch_name"), InParams.RenderDataIdentifier.RootBranchName.ToString());

		// TODO: Some of these are per render layer and need to be stored that way. EXRs will have the metadata for all the layers/cameras/etc. in one file.
		OutMergedFormatArgs.FileMetadata.Add(TEXT("unreal/cameraName"), InParams.RenderDataIdentifier.CameraName);
		OutMergedFormatArgs.FileMetadata.Add(TEXT("unreal/rendererName"), InParams.RenderDataIdentifier.RendererName);
		OutMergedFormatArgs.FileMetadata.Add(TEXT("unreal/subResourceName"), InParams.RenderDataIdentifier.SubResourceName);
		OutMergedFormatArgs.FileMetadata.Add(TEXT("unreal/layerName"), InParams.RenderDataIdentifier.LayerName);
		OutMergedFormatArgs.FileMetadata.Add(TEXT("unreal/branchName"), InParams.RenderDataIdentifier.RootBranchName.ToString());
	}


	// Add data from the job. ToDo: This probably needs to come from the job itself, so you can plug in different data sources eventually
	{
		// Frame Rate we handle separately, because it needs to be resolved.
		double FrameRate = InParams.DefaultFrameRate.AsDecimal();
		if (InParams.EvaluatedConfig)
		{
			const bool bIncludeCDOs = false;
			UMovieGraphGlobalOutputSettingNode* OutputSettingNode = InParams.EvaluatedConfig->GetSettingForBranch<UMovieGraphGlobalOutputSettingNode>(UMovieGraphSettingNode::GlobalsPinName, bIncludeCDOs);

			if (OutputSettingNode)
			{
				FrameRate = GetEffectiveFrameRate(OutputSettingNode, InParams.DefaultFrameRate).AsDecimal();
				bOverwriteExisting = OutputSettingNode->bOverwriteExistingOutput;
			}
		}

		OutMergedFormatArgs.FilenameArguments.Add(TEXT("frame_rate"), FString::SanitizeFloat(FrameRate));
		OutMergedFormatArgs.FileMetadata.Add(TEXT("unreal/frameRate"), FString::SanitizeFloat(FrameRate));

		FString LevelName;
		FString SequenceName;
		if (InParams.Job)
		{
			LevelName = InParams.Job->Map.GetAssetName();
			SequenceName = InParams.Job->Sequence.GetAssetName();
		}
		
		OutMergedFormatArgs.FilenameArguments.Add(TEXT("level_name"), LevelName);
		OutMergedFormatArgs.FilenameArguments.Add(TEXT("sequence_name"), SequenceName);
		
		OutMergedFormatArgs.FileMetadata.Add(TEXT("unreal/levelName"), LevelName);
		OutMergedFormatArgs.FileMetadata.Add(TEXT("unreal/sequenceName"), SequenceName);
		
		// Add KVP data from the job (date, time, job name, job author, job comment)
		UE::MoviePipeline::GetSharedFormatArguments(OutMergedFormatArgs.FilenameArguments, OutMergedFormatArgs.FileMetadata, InParams.InitializationTime, InParams.Version, InParams.Job, InParams.InitializationTimeOffset);
	}
	

	//  Now get the settings from our config. We need to gather KVP data from all possible nodes, even if not expressed in your configuration. This is because you might want to
	// always use the {ts_count} token even if you don't have a Temporal Sample Count node to add it. So we loop through all the possible class types, and call a function on the 
	// CDO, but then we pass that class type from the evaluated config (if it exists), and we pass the CDO as an argument if it doesn't.
	TArray<UClass*> AllSettingsNodeClasses = UE::MovieRenderPipeline::FindMoviePipelineSettingClasses(UMovieGraphSettingNode::StaticClass(), false);

	// ToDo: This loops through class iterators every frame, we should probably initialize a copy of everything into the flattened config, since we could cache the classes
	// once per run there. We don't cache the returned results here because you could potentially add/remove classes (via Blueprints) which would invalidate our cache.
	for (UClass* InClass : AllSettingsNodeClasses)
	{
		const UMovieGraphSettingNode* SettingInstance = nullptr;
		if (InParams.EvaluatedConfig)
		{
			const bool bIncludeCDOs = true;
			const bool bExactMatch = true;
			SettingInstance = InParams.EvaluatedConfig->GetSettingForBranch(InClass, InParams.RenderDataIdentifier.RootBranchName, bIncludeCDOs, bExactMatch);
		}
		else
		{
			SettingInstance = GetDefault<UMovieGraphSettingNode>(InClass);
		}

		if (SettingInstance)
		{
			SettingInstance->GetFormatResolveArgs(OutMergedFormatArgs, InParams.RenderDataIdentifier);
		}
	}

	// Copy the metadata in the parameters into the merged output version
	for (const TPair<FString, FString>& FileMetadata : InParams.FileMetadata)
	{
		OutMergedFormatArgs.FileMetadata.Add(FileMetadata.Key, FileMetadata.Value);
	}

	// We expect the incoming string to have a {file_dup} token where they want file duplication numbers to be handled
	// but we don't actually want a value there (unless there's a collision). So we override it with an empty string by default.
	OutMergedFormatArgs.FilenameArguments.Add(TEXT("file_dup"), FString());

	// Format the {version} token so it's of the form "v###"
	OutMergedFormatArgs.FilenameArguments.Add(TEXT("version"), FString::Printf(TEXT("v%0*d"), 3, InParams.Version));

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

	// If no format string is provided, there's nothing left to do.
	if (InFormatString.IsEmpty())
	{
		return FString();
	}

	// Apply all of our named args to the file and generate a path.
	FString BaseFilename = FString::Format(*InFormatString, NamedArgs);
	if (InParams.bEnsureAbsolutePath)
	{
		BaseFilename = FPaths::ConvertRelativePathToFull(BaseFilename);
	}

	// Fix-up slashes
	FPaths::NormalizeFilename(BaseFilename);
	FPaths::RemoveDuplicateSlashes(BaseFilename);

	// In the event of multiple dots in the filename, we need to replace them with a single dot to ensure a valid file name
	while (BaseFilename.Contains(".."))
	{
		BaseFilename.ReplaceInline(TEXT(".."), TEXT("."));
	}

	// If we end with a "." character, remove it. The extension will put it back on. We can end up with this sometimes
	// after resolving file format strings, ie: {sequence_name}.{frame_number} becomes {sequence_name}. for
	// videos (which can't use frame_numbers).
	BaseFilename.RemoveFromEnd(TEXT("."));
	
	// If the extension is not resolved, ThisTry will be a path
	const FString ExtToken = TEXT(".{ext}");
	FString Extension = FString::Format(*ExtToken, NamedArgs);
	FString ThisTry = Extension == ExtToken ? BaseFilename : BaseFilename + Extension;
	
	// Check that it's a valid path that we can write to.
	if (UE::MoviePipeline::CanWriteToFile(*ThisTry, bOverwriteExisting))
	{
		return ThisTry;
	}

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

int32 UMovieGraphBlueprintLibrary::ResolveVersionNumber(FMovieGraphFilenameResolveParams InParams, const bool bGetNextVersion)
{
	// Note: InParams is passed by copy rather than const& because it is modified within this method.
	
	if (!InParams.EvaluatedConfig)
	{
		FFrame::KismetExecutionMessage(TEXT("Cannot resolve version number without a valid evaluated graph to pull settings from."), ELogVerbosity::Error);
		return -1;
	}

	bool bIncludeCDOs = true;
	bool bExactMatch = true;
	const UMovieGraphGlobalOutputSettingNode* OutputSettingNode =
		InParams.EvaluatedConfig->GetSettingForBranch<UMovieGraphGlobalOutputSettingNode>(UMovieGraphNode::GlobalsPinName, bIncludeCDOs, bExactMatch);
	if (!OutputSettingNode->VersioningSettings.bAutoVersioning)
	{
		return OutputSettingNode->VersioningSettings.VersionNumber;
	}

	// Force the Version string to stay as {version} so we can substring based on it later.
	InParams.FileNameFormatOverrides.Add(TEXT("version"), TEXT("{version}"));

	// Get output nodes from the evaluated graph
	bIncludeCDOs = false;
	bExactMatch = false;
	TArray<UMovieGraphSettingNode*> ResultNodes = InParams.EvaluatedConfig->GetSettingsForBranch(
		UMovieGraphCommandLineEncoderNode::StaticClass(), InParams.RenderDataIdentifier.RootBranchName, bIncludeCDOs, bExactMatch);
	ResultNodes.Append(InParams.EvaluatedConfig->GetSettingsForBranch(
		UMovieGraphFileOutputNode::StaticClass(), InParams.RenderDataIdentifier.RootBranchName, bIncludeCDOs, bExactMatch));
	
	int32 HighestVersion = -1;

	auto ExtrapolateHighestVersionFromResultNode = [&InParams, OutputSettingNode, &HighestVersion](const FString& FileNameFormat)
	{
		// Calculate a version number by looking at the output path and then scanning for a version token.
		const FString FileNameFormatString = InParams.FileNameOverride.Len() > 0
			? InParams.FileNameOverride
			: OutputSettingNode->OutputDirectory.Path / FileNameFormat;

		FMovieGraphResolveArgs FinalFormatArgs;
		FString FinalPath = ResolveFilenameFormatArguments(FileNameFormatString, InParams, FinalFormatArgs);
		FinalPath = FPaths::ConvertRelativePathToFull(FinalPath);
		FPaths::NormalizeFilename(FinalPath);

		// Can't resolve a version if it's not clear from the path where the version number will be used.
		if (!FinalPath.Contains(TEXT("{version}")))
		{
			return;
		}

		// FinalPath can have {version} either in a folder name or in a file name. We need to find the 'parent' of either the
		// file or folder that contains it. We can do this by looking for {version} and then finding the last "/" character,
		// which will be the containing folder.
		const int32 VersionStringIndex = FinalPath.Find(TEXT("{version}"), ESearchCase::Type::IgnoreCase, ESearchDir::Type::FromStart);
		if (VersionStringIndex >= 0)
		{
			const int32 LastParentFolder = FinalPath.Find(TEXT("/"), ESearchCase::Type::IgnoreCase, ESearchDir::Type::FromEnd, VersionStringIndex);
			FinalPath.LeftInline(LastParentFolder + 1);

			// Now that we have the parent folder of either the folder with the version token, or the file with the version
			// token, we will look through all immediate children and scan for version tokens so we can find the highest one.
			const FRegexPattern VersionSearchPattern(TEXT("v([0-9]{3})"));
			constexpr bool bFindFiles = true;
			constexpr bool bFindDirectories = true;
			const FString SearchString = FinalPath / TEXT("*.*");
			TArray<FString> FoundFilesAndFoldersInDirectory;
			IFileManager& FileManager = IFileManager::Get();
			FileManager.FindFiles(FoundFilesAndFoldersInDirectory, *SearchString, bFindFiles, bFindDirectories);

			for (const FString& Path : FoundFilesAndFoldersInDirectory)
			{
				FRegexMatcher Regex(VersionSearchPattern, *Path);
				if (Regex.FindNext())
				{
					FString Result = Regex.GetCaptureGroup(0);
					if (Result.Len() > 0)
					{
						// Strip the "v" token off, expected pattern is vXXX
						Result.RightChopInline(1);
					}

					int32 VersionNumber = 0;
					LexFromString(VersionNumber, *Result);
					if (VersionNumber > HighestVersion)
					{
						HighestVersion = VersionNumber;
					}
				}
			}
		}
	};
	
	for (const UMovieGraphSettingNode* ResultNode : ResultNodes)
	{
		FString FileNameFormat = FString();

		if (const UMovieGraphFileOutputNode* FileOutputNode = Cast<UMovieGraphFileOutputNode>(ResultNode))
		{
			FileNameFormat = FileOutputNode->FileNameFormat;
		}
		else if (const UMovieGraphCommandLineEncoderNode* CommandLineEncoderNode = Cast<UMovieGraphCommandLineEncoderNode>(ResultNode))
		{
			FileNameFormat = CommandLineEncoderNode->FileNameFormat;
		}

		ExtrapolateHighestVersionFromResultNode(FileNameFormat);
	}
	
	return HighestVersion + (bGetNextVersion ? 1 : 0);
}

FIntPoint UMovieGraphBlueprintLibrary::GetEffectiveOutputResolution(UMovieGraphEvaluatedConfig* InEvaluatedGraph)
{
	if (!InEvaluatedGraph)
	{
		FFrame::KismetExecutionMessage(TEXT("InEvaluatedGraph cannot be null."), ELogVerbosity::Error);
		return FIntPoint();
	}
	
	constexpr bool bIncludeCDOs = true;
	const UMovieGraphGlobalOutputSettingNode* OutputSetting = InEvaluatedGraph->GetSettingForBranch<UMovieGraphGlobalOutputSettingNode>(UMovieGraphNode::GlobalsPinName, bIncludeCDOs);
	if (!ensure(OutputSetting))
	{
		return FIntPoint();
	}

	float RescaledOverscan = 0.f;
	if (UMovieGraphCameraSettingNode* CameraSetting = InEvaluatedGraph->GetSettingForBranch<UMovieGraphCameraSettingNode>(UMovieGraphNode::GlobalsPinName, bIncludeCDOs))
	{
		// The old system used [0-1] range for floats, the new system will use [0-100], so we rescale down before calling through.
		RescaledOverscan = FMath::Clamp(CameraSetting->OverscanPercentage / 100.f, 0.f, 1.f);
	}

	// We need to look at the Project Settings for the latest value for a given profile
	FMovieGraphNamedResolution NamedResolution;
	if (UMovieGraphBlueprintLibrary::IsNamedResolutionValid(OutputSetting->OutputResolution.ProfileName))
	{
		NamedResolution = UMovieGraphBlueprintLibrary::NamedResolutionFromProfile(OutputSetting->OutputResolution.ProfileName);
	}
	else
	{
		// Otherwise if it's not in the output settings as a valid profile, we use our internally stored one.
		NamedResolution = OutputSetting->OutputResolution;
	}

	return UMoviePipelineBlueprintLibrary::Utility_GetEffectiveOutputResolution(RescaledOverscan, NamedResolution.Resolution);
}

FText UMovieGraphBlueprintLibrary::GetJobName(const UMovieGraphPipeline* InMovieGraphPipeline)
{
	return InMovieGraphPipeline ? FText::FromString(InMovieGraphPipeline->GetCurrentJob()->JobName) : FText();
}

FText UMovieGraphBlueprintLibrary::GetJobAuthor(const UMovieGraphPipeline* InMovieGraphPipeline)
{
	return InMovieGraphPipeline ? FText::FromString(UE::MoviePipeline::GetJobAuthor(InMovieGraphPipeline->GetCurrentJob())) : FText();
}

float UMovieGraphBlueprintLibrary::GetCompletionPercentage(const UMovieGraphPipeline* InPipeline)
{
	if (!InPipeline)
	{
		return 0.f;
	}

	int32 OutputFrames;
	int32 TotalOutputFrames;
	GetOverallOutputFrames(InPipeline, OutputFrames, TotalOutputFrames);

	const float CompletionPercentage = FMath::Clamp(OutputFrames / (float)TotalOutputFrames, 0.f, 1.f);
	return CompletionPercentage;
}

void UMovieGraphBlueprintLibrary::GetOverallOutputFrames(const UMovieGraphPipeline* InMovieGraphPipeline, int32& OutCurrentIndex, int32& OutTotalCount)
{
	OutCurrentIndex = 0;
	OutTotalCount = 0;
	
	if (InMovieGraphPipeline && InMovieGraphPipeline->GetTimeStepInstance())
	{
		OutCurrentIndex = InMovieGraphPipeline->GetTimeStepInstance()->GetCalculatedTimeData().OutputFrameNumber;

		for (const TObjectPtr<UMoviePipelineExecutorShot>& Shot : InMovieGraphPipeline->GetActiveShotList())
		{
			OutTotalCount += Shot->ShotInfo.WorkMetrics.TotalOutputFrameCount;
		}
	}
}

FDateTime UMovieGraphBlueprintLibrary::GetJobInitializationTime(const UMovieGraphPipeline* InMovieGraphPipeline)
{
	return InMovieGraphPipeline ? InMovieGraphPipeline->GetInitializationTime() : FDateTime();
}

bool UMovieGraphBlueprintLibrary::GetEstimatedTimeRemaining(const UMovieGraphPipeline* InMovieGraphPipeline, FTimespan& OutEstimate)
{
	if (!InMovieGraphPipeline)
	{
		OutEstimate = FTimespan();
		return false;
	}

	// If they haven't produced a single frame yet, we can't give an estimate.
	int32 OutputFrames;
	int32 TotalOutputFrames;
	GetOverallOutputFrames(InMovieGraphPipeline, OutputFrames, TotalOutputFrames);

	if (OutputFrames <= 0 || TotalOutputFrames <= 0)
	{
		OutEstimate = FTimespan();
		return false;
	}

	const float CompletionPercentage = OutputFrames / static_cast<float>(TotalOutputFrames);
	const FTimespan CurrentDuration = FDateTime::UtcNow() - InMovieGraphPipeline->GetInitializationTime();

	// If it has taken us CurrentDuration to process CompletionPercentage samples, then we can get a total duration
	// estimate by taking (CurrentDuration/CompletionPercentage) and then take that total estimate minus elapsed
	// to get remaining.
	const FTimespan EstimatedTotalDuration = CurrentDuration / CompletionPercentage;
	OutEstimate = EstimatedTotalDuration - CurrentDuration;

	return true;
}

EMovieRenderPipelineState UMovieGraphBlueprintLibrary::GetPipelineState(const UMovieGraphPipeline* InMovieGraphPipeline)
{
	return InMovieGraphPipeline ? InMovieGraphPipeline->GetPipelineState() : EMovieRenderPipelineState::Uninitialized;
}

EMovieRenderShotState UMovieGraphBlueprintLibrary::GetCurrentSegmentState(const UMovieGraphPipeline* InMovieGraphPipeline)
{
	if (InMovieGraphPipeline)
	{
		const int32 ShotIndex = InMovieGraphPipeline->GetCurrentShotIndex();
		
		if (ShotIndex < InMovieGraphPipeline->GetActiveShotList().Num())
		{
			return InMovieGraphPipeline->GetActiveShotList()[ShotIndex]->ShotInfo.State;
		}
	}

	return EMovieRenderShotState::Uninitialized;
}

void UMovieGraphBlueprintLibrary::GetCurrentSegmentName(const UMovieGraphPipeline* InMovieGraphPipeline, FText& OutOuterName, FText& OutInnerName)
{
	if (InMovieGraphPipeline)
    {
    	const int32 ShotIndex = InMovieGraphPipeline->GetCurrentShotIndex();
		
    	if (ShotIndex < InMovieGraphPipeline->GetActiveShotList().Num())
    	{
    		OutOuterName = FText::FromString(InMovieGraphPipeline->GetActiveShotList()[ShotIndex]->OuterName);
    		OutInnerName = FText::FromString(InMovieGraphPipeline->GetActiveShotList()[ShotIndex]->InnerName);
    	}
    }
}

void UMovieGraphBlueprintLibrary::GetOverallSegmentCounts(const UMovieGraphPipeline* InMovieGraphPipeline, int32& OutCurrentIndex, int32& OutTotalCount)
{
	OutCurrentIndex = InMovieGraphPipeline->GetCurrentShotIndex();
	OutTotalCount = InMovieGraphPipeline->GetActiveShotList().Num();
}

FMoviePipelineSegmentWorkMetrics UMovieGraphBlueprintLibrary::GetCurrentSegmentWorkMetrics(const UMovieGraphPipeline* InMovieGraphPipeline)
{
	if (InMovieGraphPipeline)
	{
		const int32 ShotIndex = InMovieGraphPipeline->GetCurrentShotIndex();
		
		if (ShotIndex < InMovieGraphPipeline->GetActiveShotList().Num())
		{
			return InMovieGraphPipeline->GetActiveShotList()[ShotIndex]->ShotInfo.WorkMetrics;
		}
	}

	return FMoviePipelineSegmentWorkMetrics();
}

FTimecode UMovieGraphBlueprintLibrary::GetRootTimecode(const UMovieGraphPipeline* InMovieGraphPipeline)
{
	if (InMovieGraphPipeline)
	{
		return InMovieGraphPipeline->GetTimeStepInstance()->GetCalculatedTimeData().RootTimeCode;
	}

	return FTimecode();
}

FFrameNumber UMovieGraphBlueprintLibrary::GetRootFrameNumber(const UMovieGraphPipeline* InMovieGraphPipeline)
{
	if (InMovieGraphPipeline)
	{
		return InMovieGraphPipeline->GetTimeStepInstance()->GetCalculatedTimeData().RootFrameNumber;
	}

	return FFrameNumber(-1);
}

FTimecode UMovieGraphBlueprintLibrary::GetCurrentShotTimecode(const UMovieGraphPipeline* InMovieGraphPipeline)
{
	if (InMovieGraphPipeline)
	{
		return InMovieGraphPipeline->GetTimeStepInstance()->GetCalculatedTimeData().ShotTimeCode;
	}

	return FTimecode();
}

FFrameNumber UMovieGraphBlueprintLibrary::GetCurrentShotFrameNumber(const UMovieGraphPipeline* InMovieGraphPipeline)
{
	if (InMovieGraphPipeline)
	{
		return InMovieGraphPipeline->GetTimeStepInstance()->GetCalculatedTimeData().ShotFrameNumber;
	}

	return FFrameNumber(-1);
}

float UMovieGraphBlueprintLibrary::GetCurrentFocusDistance(const UMovieGraphPipeline* InMovieGraphPipeline)
{
	if (const UCineCameraComponent* CineCameraComponent = UMoviePipelineBlueprintLibrary::Utility_GetCurrentCineCamera(InMovieGraphPipeline->GetWorld()))
	{
		return CineCameraComponent->CurrentFocusDistance;
	}

	return -1.f;
}

float UMovieGraphBlueprintLibrary::GetCurrentFocalLength(const UMovieGraphPipeline* InMovieGraphPipeline)
{
	if (const UCineCameraComponent* CineCameraComponent = UMoviePipelineBlueprintLibrary::Utility_GetCurrentCineCamera(InMovieGraphPipeline->GetWorld()))
	{
		return CineCameraComponent->CurrentFocalLength;
	}

	return -1.f;
}

float UMovieGraphBlueprintLibrary::GetCurrentAperture(const UMovieGraphPipeline* InMovieGraphPipeline)
{
	if (const UCineCameraComponent* CineCameraComponent = UMoviePipelineBlueprintLibrary::Utility_GetCurrentCineCamera(InMovieGraphPipeline->GetWorld()))
	{
		return CineCameraComponent->CurrentAperture;
	}

	return 0.f;
}

UCineCameraComponent* UMovieGraphBlueprintLibrary::GetCurrentCineCamera(const UMovieGraphPipeline* InMovieGraphPipeline)
{
	if (UCineCameraComponent* CineCameraComponent = UMoviePipelineBlueprintLibrary::Utility_GetCurrentCineCamera(InMovieGraphPipeline->GetWorld()))
	{
		return CineCameraComponent;
	}

	return nullptr;
}

FMovieGraphNamedResolution UMovieGraphBlueprintLibrary::NamedResolutionFromProfile(const FName& InResolutionProfileName)
{
	// Find a matching custom entry from Project Settings
	if (const UMovieGraphProjectSettings* MovieGraphProjectSettings =
		GetDefault<UMovieGraphProjectSettings>())
	{
		if (const FMovieGraphNamedResolution* Match = MovieGraphProjectSettings->FindNamedResolutionForOption(InResolutionProfileName))
		{
			return *Match;
		}
	}

	// If we couldn't find it, throw an exception
	FFrame::KismetExecutionMessage(
		*FString::Printf(
			TEXT("%hs: Could not find named resolution with profile name %s in Project Settings."), __FUNCTION__, *InResolutionProfileName.ToString()),
		ELogVerbosity::Error);

	return FMovieGraphNamedResolution();
}

bool UMovieGraphBlueprintLibrary::IsNamedResolutionValid(const FName& InResolutionProfileName)
{
	if (const UMovieGraphProjectSettings* MovieGraphProjectSettings =
		GetDefault<UMovieGraphProjectSettings>())
	{
		if (const FMovieGraphNamedResolution* Match = MovieGraphProjectSettings->FindNamedResolutionForOption(InResolutionProfileName))
		{
			return true;
		}
	}

	return false;
}

FMovieGraphNamedResolution UMovieGraphBlueprintLibrary::NamedResolutionFromSize(const int32 InResX, const int32 InResY)
{
	return FMovieGraphNamedResolution(FMovieGraphNamedResolution::CustomEntryName, FIntPoint(InResX, InResY), FString());
}