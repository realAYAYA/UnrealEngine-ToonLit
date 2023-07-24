// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

enum class EPoseSearchMirrorOption;
enum class ESearchIndexAssetType;
class FUICommandList;
class ITableRow;
class STableViewBase;

namespace UE::PoseSearch
{
	class FDatabaseViewModel;
	class SDatabaseAssetTree;

	class FDatabaseAssetTreeNode : public TSharedFromThis<FDatabaseAssetTreeNode>
	{

	public:
		FDatabaseAssetTreeNode(
			int32 InSourceAssetIdx,
			ESearchIndexAssetType InSourceAssetType,
			const TSharedRef<FDatabaseViewModel>& InEditorViewModel);

		TSharedRef<ITableRow> MakeTreeRowWidget(
			const TSharedRef<STableViewBase>& InOwnerTable,
			TSharedRef<FDatabaseAssetTreeNode> InDatabaseAssetNode,
			TSharedRef<FUICommandList> InCommandList,
			TSharedPtr<SDatabaseAssetTree> InHierarchy);

		bool IsRootMotionEnabled() const;
		bool IsLooping() const;
		EPoseSearchMirrorOption GetMirrorOption() const;
		
		int32 SourceAssetIdx;
		ESearchIndexAssetType SourceAssetType;
		TSharedPtr<FDatabaseAssetTreeNode> Parent;
		TArray<TSharedPtr<FDatabaseAssetTreeNode>> Children;
		TWeakPtr<FDatabaseViewModel> EditorViewModel;
	};
}

