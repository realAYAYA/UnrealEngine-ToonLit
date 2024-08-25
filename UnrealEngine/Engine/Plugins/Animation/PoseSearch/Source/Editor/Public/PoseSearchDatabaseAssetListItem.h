// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PoseSearchDatabaseAssetTreeNode.h"
#include "Widgets/Views/STableRow.h"

class FUICommandList;

namespace UE::PoseSearch
{
	class FDatabaseViewModel;
	class SDatabaseAssetTree;

	class SDatabaseAssetListItem : public STableRow<TSharedPtr<FDatabaseAssetTreeNode>>
	{
	public:
		void Construct(
			const FArguments& InArgs,
			const TSharedRef<FDatabaseViewModel>& InEditorViewModel,
			const TSharedRef<STableViewBase>& OwnerTable,
			TSharedRef<FDatabaseAssetTreeNode> InAssetTreeNode,
			TSharedRef<FUICommandList> InCommandList,
			TSharedPtr<SDatabaseAssetTree> InHierarchy);

	protected:
		FText GetName() const;
		TSharedRef<SWidget> GenerateItemWidget();

		const FSlateBrush* GetGroupBackgroundImage() const;
		void ConstructGroupItem(const TSharedRef<STableViewBase>& OwnerTable);
		void ConstructAssetItem(const TSharedRef<STableViewBase>& OwnerTable);

		void OnAddSequence();
		void OnAddBlendSpace();
		void OnAddAnimComposite();
		void OnAddAnimMontage();
		virtual FReply OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override;

		
		EVisibility GetSelectedActorIconVisbility() const;

		FSlateColor GetNameTextColorAndOpacity() const;
		
		FSlateColor GetLoopingColorAndOpacity() const;
		FText GetLoopingToolTip() const;
		
		FSlateColor GetRootMotionColorAndOpacity() const;
		FText GetRootMotionOptionToolTip() const;
		
		const FSlateBrush* GetMirrorOptionSlateBrush() const;
		FText GetMirrorOptionToolTip() const;
		FReply MirrorOptionOnMouseButtonDown(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent);
		
		FText GetDisableReselectionToolTip() const;
		ECheckBoxState GetDisableReselectionChecked() const;
		void OnDisableReselectionChanged(ECheckBoxState NewCheckboxState);

		FText GetAssetEnabledToolTip() const;
		ECheckBoxState GetAssetEnabledChecked() const;
		void OnAssetIsEnabledChanged(ECheckBoxState NewCheckboxState);

		TWeakPtr<FDatabaseAssetTreeNode> WeakAssetTreeNode;
		TWeakPtr<FDatabaseViewModel> EditorViewModel;
		TWeakPtr<SDatabaseAssetTree> SkeletonView;
	};
}

