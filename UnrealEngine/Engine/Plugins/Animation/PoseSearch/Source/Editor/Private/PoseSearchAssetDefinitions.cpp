// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearchAssetDefinitions.h"
#include "PoseSearchDatabaseEditor.h"
#include "ThumbnailRendering/SceneThumbnailInfo.h"

namespace UE::PoseSearch
{

FLinearColor GetAssetColor()
{
	static const FLinearColor AssetColor(FColor(29, 96, 125));
	return AssetColor;
}

TConstArrayView<FAssetCategoryPath> GetAssetCategories()
{
	static const auto Categories = { FAssetCategoryPath(EAssetCategoryPaths::Animation, NSLOCTEXT("PoseSearchAssetDefinition", "PoseSearchAssetDefinitionMenu", "Motion Matching")) };
	return Categories;
}

UThumbnailInfo* LoadThumbnailInfo(const FAssetData & InAssetData)
{
	return UE::Editor::FindOrCreateThumbnailInfo(InAssetData.GetAsset(), USceneThumbnailInfo::StaticClass());
}

} // namespace UE::PoseSearch

EAssetCommandResult UAssetDefinition_PoseSearchDatabase::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	using namespace UE::PoseSearch;

	TArray<UPoseSearchDatabase*> Objects = OpenArgs.LoadObjects<UPoseSearchDatabase>();
	const EToolkitMode::Type Mode = OpenArgs.ToolkitHost.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;
	for (UPoseSearchDatabase* Database : Objects)
	{
		if (Database)
		{
			TSharedRef<FDatabaseEditor> NewEditor(new FDatabaseEditor());
			NewEditor->InitAssetEditor(Mode, OpenArgs.ToolkitHost, Database);
		}
	}
	
	return EAssetCommandResult::Handled;
}
