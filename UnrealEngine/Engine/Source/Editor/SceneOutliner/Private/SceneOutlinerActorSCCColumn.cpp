// Copyright Epic Games, Inc. All Rights Reserved.

#include "SceneOutlinerActorSCCColumn.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Views/STreeView.h"
#include "ActorTreeItem.h"
#include "ActorDescTreeItem.h"
#include "ActorFolderTreeItem.h"
#include "WorldPartition/WorldPartitionActorDesc.h"
#include "ISourceControlModule.h"
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "SceneOutlinerSourceControlColumn"

FName FSceneOutlinerActorSCCColumn::GetColumnID()
{
	return GetID();
}

SHeaderRow::FColumn::FArguments FSceneOutlinerActorSCCColumn::ConstructHeaderRowColumn()
{
	return SHeaderRow::Column(GetColumnID())
		.FixedWidth(24.f)
		.HAlignHeader(HAlign_Center)
		.VAlignHeader(VAlign_Center)
		.HAlignCell(HAlign_Center)
		.VAlignCell(VAlign_Center)
		.DefaultTooltip(FText::FromName(GetColumnID()))
		[
			SNew(SImage)
			.ColorAndOpacity(FSlateColor::UseForeground())
			.Image(this, &FSceneOutlinerActorSCCColumn::GetHeaderIcon)
		];
}

const TSharedRef<SWidget> FSceneOutlinerActorSCCColumn::ConstructRowWidget(FSceneOutlinerTreeItemRef TreeItem, const STableRow<FSceneOutlinerTreeItemPtr>& Row)
{
	if (TreeItem->IsA<FActorTreeItem>() || 
		TreeItem->IsA<FActorDescTreeItem>() || 
		(TreeItem->IsA<FActorFolderTreeItem>() && TreeItem->CastTo<FActorFolderTreeItem>()->GetActorFolder()))
	{
		TSharedPtr<FSceneOutlinerTreeItemSCC> SourceControl = WeakSceneOutliner.Pin()->GetItemSourceControl(TreeItem);
		if (SourceControl.IsValid())
		{
			TSharedRef<SSourceControlWidget> Widget = SNew(SSourceControlWidget, SourceControl);
			
			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					Widget
				];
		}
	}
	return SNullWidget::NullWidget;
}

const FSlateBrush* FSceneOutlinerActorSCCColumn::GetHeaderIcon() const
{
	if (ISourceControlModule::Get().IsEnabled())
	{
		return FAppStyle::GetBrush("SourceControl.StatusIcon.On");
	}
	else
	{
		return FAppStyle::GetBrush("SourceControl.StatusIcon.Off");
	}
}

#undef LOCTEXT_NAMESPACE