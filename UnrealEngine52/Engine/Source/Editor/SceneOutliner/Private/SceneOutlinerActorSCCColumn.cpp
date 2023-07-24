// Copyright Epic Games, Inc. All Rights Reserved.

#include "SceneOutlinerActorSCCColumn.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Images/SLayeredImage.h"
#include "Widgets/Views/STreeView.h"
#include "ActorTreeItem.h"
#include "ActorDescTreeItem.h"
#include "ActorFolderTreeItem.h"
#include "WorldPartition/WorldPartitionActorDesc.h"
#include "ISourceControlModule.h"
#include "Misc/MessageDialog.h"
#include "RevisionControlStyle/RevisionControlStyle.h"

#define LOCTEXT_NAMESPACE "SceneOutlinerSourceControlColumn"

FName FSceneOutlinerActorSCCColumn::GetColumnID()
{
	return GetID();
}

SHeaderRow::FColumn::FArguments FSceneOutlinerActorSCCColumn::ConstructHeaderRowColumn()
{
	TSharedRef<SLayeredImage> HeaderRowIcon = SNew(SLayeredImage)
			.ColorAndOpacity(FSlateColor::UseForeground())
			.Image(FRevisionControlStyleManager::Get().GetBrush("RevisionControl.Icon"));
	
	HeaderRowIcon->AddLayer(TAttribute<const FSlateBrush*>::CreateSP(this, &FSceneOutlinerActorSCCColumn::GetHeaderIconBadge));
	
	return SHeaderRow::Column(GetColumnID())
		.FixedWidth(24.f)
		.HAlignHeader(HAlign_Center)
		.VAlignHeader(VAlign_Center)
		.HAlignCell(HAlign_Center)
		.VAlignCell(VAlign_Center)
		.DefaultTooltip(FText::FromName(GetColumnID()))
		[
			HeaderRowIcon
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

const FSlateBrush* FSceneOutlinerActorSCCColumn::GetHeaderIconBadge() const
{
	if (ISourceControlModule::Get().IsEnabled())
	{
		return FRevisionControlStyleManager::Get().GetBrush("RevisionControl.Icon.ConnectedBadge");
	}
	else
	{
		return nullptr;
	}
}

#undef LOCTEXT_NAMESPACE