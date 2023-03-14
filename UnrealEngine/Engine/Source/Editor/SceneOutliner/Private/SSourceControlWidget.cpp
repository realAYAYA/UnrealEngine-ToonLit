// Copyright Epic Games, Inc. All Rights Reserved.

#include "SSourceControlWidget.h"

void SSourceControlWidget::Construct(const FArguments& InArgs, TSharedPtr<FSceneOutlinerTreeItemSCC> InItemSourceControl)
{
	check(InItemSourceControl.IsValid());

	ItemSourceControl = InItemSourceControl;

	ItemSourceControl->OnSourceControlStateChanged.BindSP(this, &SSourceControlWidget::UpdateSourceControlState);

	SImage::Construct(
		SImage::FArguments()
		.ColorAndOpacity(this, &SSourceControlWidget::GetForegroundColor)
		.Image(FStyleDefaults::GetNoBrush()));

	UpdateSourceControlState(ItemSourceControl->GetSourceControlState());
}

FReply SSourceControlWidget::OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	check(ItemSourceControl);

	FSourceControlStatePtr SourceControlState = ItemSourceControl->RefreshSourceControlState();
	UpdateSourceControlState(SourceControlState);
	return FReply::Handled();
}

void SSourceControlWidget::UpdateSourceControlState(FSourceControlStatePtr SourceControlState)
{
	if(SourceControlState.IsValid())
	{
		SetFromSlateIcon(SourceControlState->GetIcon());
		SetToolTipText(SourceControlState->GetDisplayTooltip());
	}
	else
	{
		SetImage(nullptr);
		SetToolTipText(TAttribute<FText>());
		RemoveAllLayers();
	}
}
