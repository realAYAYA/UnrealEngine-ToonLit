// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/Nodes/MovieGraphOutputSettingNode.h"
#include "Styling/AppStyle.h"

UMovieGraphOutputSettingNode::UMovieGraphOutputSettingNode()
	: OutputResolution(FIntPoint(1920, 1080))
	, OutputFrameRate(FFrameRate(24, 1))
	, bOverwriteExistingOutput(true)
	, ZeroPadFrameNumbers(4)
	, FrameNumberOffset(0)
{

	FileNameFormat = TEXT("{sequence_name}.{frame_number}");
	OutputDirectory.Path = TEXT("{project_dir}/Saved/MovieRenders/");
}

void UMovieGraphOutputSettingNode::GetFormatResolveArgs(FMovieGraphResolveArgs& OutMergedFormatArgs) const
{
	const FString ResolvedProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	OutMergedFormatArgs.FilenameArguments.Add(TEXT("project_dir"), ResolvedProjectDir);
	OutMergedFormatArgs.FileMetadata.Add(TEXT("unreal/project_dir"), ResolvedProjectDir);

	// Resolution Arguments
	{
		FString Resolution = FString::Printf(TEXT("%d_%d"), OutputResolution.X, OutputResolution.Y);
		OutMergedFormatArgs.FilenameArguments.Add(TEXT("output_resolution"), Resolution);
		OutMergedFormatArgs.FilenameArguments.Add(TEXT("output_width"), FString::FromInt(OutputResolution.X));
		OutMergedFormatArgs.FilenameArguments.Add(TEXT("output_height"), FString::FromInt(OutputResolution.Y));
	}

	// We don't resolve the version here because that's handled on a per-file/shot basis
}

#if WITH_EDITOR
FText UMovieGraphOutputSettingNode::GetNodeTitle(const bool bGetDescriptive) const
{
	static const FText OutputSettingsNodeName = NSLOCTEXT("MoviePipelineGraph", "NodeName_OutputSettings", "Output Settings");
	return OutputSettingsNodeName;
}

FText UMovieGraphOutputSettingNode::GetMenuCategory() const 
{
	return NSLOCTEXT("MoviePipelineGraph", "Settings_Category", "Settings");
}

FLinearColor UMovieGraphOutputSettingNode::GetNodeTitleColor() const 
{
	static const FLinearColor OutputSettingsColor = FLinearColor(0.854f, 0.509f, 0.039f);
	return OutputSettingsColor;
}

FSlateIcon UMovieGraphOutputSettingNode::GetIconAndTint(FLinearColor& OutColor) const 
{
	static const FSlateIcon SettingsIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Settings");

	OutColor = FLinearColor::White;
	return SettingsIcon;
}
#endif // WITH_EDITOR