// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/Nodes/MovieGraphGlobalOutputSettingNode.h"
#include "Graph/MovieGraphProjectSettings.h"
#include "Graph/MovieGraphBlueprintLibrary.h"
#include "Styling/AppStyle.h"
#include "Algo/Find.h"

UMovieGraphGlobalOutputSettingNode::UMovieGraphGlobalOutputSettingNode()
	: OutputFrameRate(FFrameRate(24, 1))
	, bOverwriteExistingOutput(true)
	, ZeroPadFrameNumbers(4)
	, FrameNumberOffset(0)
	, HandleFrameCount(0)
	, CustomPlaybackRangeStartFrame(0)
	, CustomPlaybackRangeEndFrame(0)
	, bFlushDiskWritesPerShot(false)
{
	OutputDirectory.Path = TEXT("{project_dir}/Saved/MovieRenders/");
}

void UMovieGraphGlobalOutputSettingNode::GetFormatResolveArgs(FMovieGraphResolveArgs& OutMergedFormatArgs, const FMovieGraphRenderDataIdentifier& InRenderDataIdentifier) const
{
	const FString ResolvedProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	OutMergedFormatArgs.FilenameArguments.Add(TEXT("project_dir"), ResolvedProjectDir);
	OutMergedFormatArgs.FileMetadata.Add(TEXT("unreal/project_dir"), ResolvedProjectDir);

	// We need to look at the Project Settings for the latest value for a given profile
	FMovieGraphNamedResolution NamedResolution;
	if (UMovieGraphBlueprintLibrary::IsNamedResolutionValid(OutputResolution.ProfileName))
	{
		NamedResolution = UMovieGraphBlueprintLibrary::NamedResolutionFromProfile(OutputResolution.ProfileName);
	}
	else
	{
		// Otherwise if it's not in the output settings as a valid profile, we use our internally stored one.
		NamedResolution = OutputResolution;
	}
	
	// Resolution Arguments
	{
		FString Resolution = FString::Printf(TEXT("%d_%d"), NamedResolution.Resolution.X, NamedResolution.Resolution.Y);
		OutMergedFormatArgs.FilenameArguments.Add(TEXT("output_resolution"), Resolution);
		OutMergedFormatArgs.FilenameArguments.Add(TEXT("output_width"), FString::FromInt(NamedResolution.Resolution.X));
		OutMergedFormatArgs.FilenameArguments.Add(TEXT("output_height"), FString::FromInt(NamedResolution.Resolution.Y));
	}

	// We don't resolve the version here because that's handled on a per-file/shot basis
}

#if WITH_EDITOR
FText UMovieGraphGlobalOutputSettingNode::GetNodeTitle(const bool bGetDescriptive) const
{
	static const FText OutputSettingsNodeName = NSLOCTEXT("MoviePipelineGraph", "NodeName_GlobalOutputSettings", "Global Output Settings");
	return OutputSettingsNodeName;
}

FText UMovieGraphGlobalOutputSettingNode::GetMenuCategory() const 
{
	return NSLOCTEXT("MoviePipelineGraph", "Settings_Category", "Settings");
}

FLinearColor UMovieGraphGlobalOutputSettingNode::GetNodeTitleColor() const 
{
	static const FLinearColor OutputSettingsColor = FLinearColor(0.854f, 0.509f, 0.039f);
	return OutputSettingsColor;
}

FSlateIcon UMovieGraphGlobalOutputSettingNode::GetIconAndTint(FLinearColor& OutColor) const 
{
	static const FSlateIcon SettingsIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Settings");

	OutColor = FLinearColor::White;
	return SettingsIcon;
}

EMovieGraphBranchRestriction UMovieGraphGlobalOutputSettingNode::GetBranchRestriction() const
{
	return EMovieGraphBranchRestriction::Globals;
}
#endif // WITH_EDITOR

