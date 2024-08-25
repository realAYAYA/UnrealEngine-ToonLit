// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/MovieGraphFilenameResolveParams.h"

#include "Graph/MovieGraphPipeline.h"
#include "Graph/Nodes/MovieGraphGlobalOutputSettingNode.h"
#include "MoviePipelineQueue.h"

FMovieGraphFilenameResolveParams FMovieGraphFilenameResolveParams::MakeResolveParams(
	const FMovieGraphRenderDataIdentifier& InRenderId,
	const UMovieGraphPipeline* InPipeline,
	const TObjectPtr<UMovieGraphEvaluatedConfig>& InEvaluatedConfig,
	const FMovieGraphTraversalContext& InTraversalContext,
	const TMap<FString, FString>& InAdditionalFormatArgs)
{
    FMovieGraphFilenameResolveParams Params = FMovieGraphFilenameResolveParams();
	
	if (ensureAlwaysMsgf(InPipeline, TEXT("InPipeline is not valid - ResolveParams will be created, but will be missing critical information.")))
	{
		Params.InitializationTime = InPipeline->GetInitializationTime();
		Params.InitializationTimeOffset = InPipeline->GetInitializationTimeOffset();
		Params.Job = InPipeline->GetCurrentJob();
		
		if (InPipeline->GetActiveShotList().IsValidIndex(InTraversalContext.ShotIndex))
		{
			const TObjectPtr<UMoviePipelineExecutorShot>& Shot = InPipeline->GetActiveShotList()[InTraversalContext.ShotIndex];
			Params.Version = Shot->ShotInfo.VersionNumber;
			Params.Shot = Shot;
		}
	}
        
	Params.RenderDataIdentifier = InRenderId;
	
	Params.RootFrameNumber = InTraversalContext.Time.RootFrameNumber.Value;
	Params.ShotFrameNumber = InTraversalContext.Time.ShotFrameNumber.Value;
	Params.RootFrameNumberRel = InTraversalContext.Time.OutputFrameNumber;
	Params.ShotFrameNumberRel = InTraversalContext.Time.ShotOutputFrameNumber;
	//Params.FileMetadata = ToDo: Track File Metadata

	if (InEvaluatedConfig)
	{
		const UMovieGraphGlobalOutputSettingNode* OutputSettingNode = InEvaluatedConfig->GetSettingForBranch<UMovieGraphGlobalOutputSettingNode>(UMovieGraphNode::GlobalsPinName);
		if (IsValid(OutputSettingNode))
		{
			Params.ZeroPadFrameNumberCount = OutputSettingNode->ZeroPadFrameNumbers;
			Params.FrameNumberOffset = OutputSettingNode->FrameNumberOffset;
		}
		Params.EvaluatedConfig = InEvaluatedConfig;
	}

    // If time dilation is in effect, RootFrameNumber and ShotFrameNumber will contain duplicates and the files will overwrite each other, 
    // so we force them into relative mode and then warn users we did that (as their numbers will jump from say 1001 -> 0000).
    bool bForceRelativeFrameNumbers = false;
    // if (FileNameFormatString.Contains(TEXT("{frame")) && InTraversalContext.Time.IsTimeDilated() && !FileNameFormatString.Contains(TEXT("_rel}")))
    // {
    // 	UE_LOG(LogMovieRenderPipeline, Warning, TEXT("Time Dilation was used but output format does not use relative time, forcing relative numbers. Change {frame_number} to {frame_number_rel} (or shot version) to remove this message."));
    // 	bForceRelativeFrameNumbers = true;
    // }
    Params.bForceRelativeFrameNumbers = bForceRelativeFrameNumbers;
	Params.bEnsureAbsolutePath = true;
	Params.FileNameFormatOverrides = InAdditionalFormatArgs;

	return Params;
}
