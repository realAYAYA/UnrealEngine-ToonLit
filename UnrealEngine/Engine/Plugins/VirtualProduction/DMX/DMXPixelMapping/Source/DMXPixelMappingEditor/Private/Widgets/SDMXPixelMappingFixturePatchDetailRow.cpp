// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SDMXPixelMappingFixturePatchDetailRow.h"

#include "Library/DMXEntityFixturePatch.h"

#include "Styling/AppStyle.h"
#include "PropertyHandle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"


#define LOCTEXT_NAMESPACE "SDMXPixelMappingPreviewView"

namespace UE::DMX::Private::SDMXPixelMappingFixturePatchDetailRow
{
	constexpr FLinearColor NormalBGColor(0.f, 0.0f, 0.f, 0.f); // Fully transparent so it just shows the background
	constexpr FLinearColor HighlightBGColor(0.87f, 0.64f, 0.f, 0.5f); // Yellow transparent
}

void SDMXPixelMappingFixturePatchDetailRow::Construct(const FArguments& InArgs)
{
	using namespace UE::DMX::Private::SDMXPixelMappingFixturePatchDetailRow;

	OnLMBDown = InArgs._OnLMBDown;
	OnLMBUp   = InArgs._OnLMBUp;
	OnDragged = InArgs._OnDragged;

	// Requires OnDragged to be bound
	check(OnDragged.IsBound());
	
	TWeakObjectPtr<UDMXEntityFixturePatch> FixturePatch = InArgs._FixturePatch;
	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		.Padding(-3.f) // Need to overdraw to avoid having gaps between the detail rows
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
			.BorderBackgroundColor_Lambda([this]()
				{
					return bHighlight ? HighlightBGColor : NormalBGColor;
				})
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("NoBorder"))
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				[
					SAssignNew(FixturePatchNameTextBlock, STextBlock)
					.Text_Lambda([FixturePatch]()
						{
							const UDMXEntityFixtureType* FixtureType = FixturePatch.IsValid() ? FixturePatch->GetFixtureType() : nullptr;
							const FDMXFixtureMode* FixtureModePtr = FixturePatch.IsValid() ? FixturePatch->GetActiveMode() : nullptr;
							if (FixtureType && FixtureModePtr)
							{
								if (FixtureModePtr->bFixtureMatrixEnabled)
								{
									return FText::Format(LOCTEXT("MatrixFixturePatchName", "Matrix: {0}"), FText::FromString(FixturePatch->Name));
								}
								else
								{
									return FText::FromString(FixturePatch->Name);
								}
							}
							// The user object should take care of not showing invalid patches.
							return FText::GetEmpty();
						})
					.Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
					.ColorAndOpacity_Lambda([this]()
						{
							return bHighlight ? FLinearColor::Black : FLinearColor::White;
						})
				]
			]
		]
	];
}

FReply SDMXPixelMappingFixturePatchDetailRow::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		OnLMBDown.ExecuteIfBound(MyGeometry, MouseEvent);

		return FReply::Handled()
			.PreventThrottling()
			.DetectDrag(AsShared(), EKeys::LeftMouseButton);
	}

	return FReply::Unhandled();
}

FReply SDMXPixelMappingFixturePatchDetailRow::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		OnLMBUp.ExecuteIfBound(MyGeometry, MouseEvent);

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FReply SDMXPixelMappingFixturePatchDetailRow::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return OnDragged.Execute(MyGeometry, MouseEvent);
}

#undef LOCTEXT_NAMESPACE
