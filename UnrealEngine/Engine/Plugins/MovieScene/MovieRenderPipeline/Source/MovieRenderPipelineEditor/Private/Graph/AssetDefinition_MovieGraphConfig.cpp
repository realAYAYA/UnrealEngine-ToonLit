// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/AssetDefinition_MovieGraphConfig.h"
#include "Graph/MovieGraphAssetToolkit.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

TSoftClassPtr<UObject> UAssetDefinition_MovieGraphConfig::GetAssetClass() const
{
	return UMovieGraphConfig::StaticClass();
}

EAssetCommandResult UAssetDefinition_MovieGraphConfig::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	for (UMovieGraphConfig* GraphToEdit : OpenArgs.LoadObjects<UMovieGraphConfig>())
	{
		TSharedRef<FMovieGraphAssetToolkit> MovieGraphEditor = MakeShared<FMovieGraphAssetToolkit>();
		MovieGraphEditor->InitMovieGraphAssetToolkit(EToolkitMode::Standalone, OpenArgs.ToolkitHost, GraphToEdit);
	}	

	return EAssetCommandResult::Handled;
}

#undef LOCTEXT_NAMESPACE
