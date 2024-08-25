// Copyright Epic Games, Inc. All Rights Reserved.

#include "DragDrop/DMStageDragDropOperation.h"
#include "Components/DMMaterialStage.h"
#include "Slate/SDMSlot.h"
#include "Slate/SDMStage.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/SOverlay.h"

FDMStageDragDropOperation::FDMStageDragDropOperation(const TSharedRef<SDMStage>& InStageWidget)
	: StageWidgetWeak(InStageWidget)
{
	Construct();
}

TSharedPtr<SWidget> FDMStageDragDropOperation::GetDefaultDecorator() const
{
	static const FLinearColor InvalidLocationColor = FLinearColor(1.0f, 0.0f, 0.0f, 0.5f);

	TSharedPtr<SDMStage> StageWidget = StageWidgetWeak.Pin();
	check(StageWidget.IsValid());

	UDMMaterialStage* const Stage = StageWidget->GetStage();
	check(Stage);

	return 
		SNew(SOverlay)
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SNew(SDMStage, Stage)
			.DesiredSize(StageWidget->GetPreviewSize())
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SNew(SColorBlock)
			.Color(InvalidLocationColor)
			.Visibility_Raw(this, &FDMStageDragDropOperation::GetInvalidDropVisibility)
		];
}

FCursorReply FDMStageDragDropOperation::OnCursorQuery()
{
	return FCursorReply::Cursor(EMouseCursor::GrabHandClosed);
}

EVisibility FDMStageDragDropOperation::GetInvalidDropVisibility() const
{
	return bValidDropLocation ? EVisibility::Hidden : EVisibility::SelfHitTestInvisible;
}

UDMMaterialStage* FDMStageDragDropOperation::GetStage() const
{
	if (TSharedPtr<SDMStage> StageWidget = StageWidgetWeak.Pin())
	{
		return StageWidget->GetStage();
	}

	return nullptr;
}
