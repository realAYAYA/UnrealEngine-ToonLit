// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAvaTextAlignmentWidget.h"

#include "AvaTextDefs.h"
#include "AvaTextEditorStyle.h"
#include "PropertyHandle.h"
#include "SlateOptMacros.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/StyleColors.h"
#include "Text3DComponent.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SAvaTextAlignmentWidget"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SAvaTextAlignmentWidget::Construct(const FArguments& InArgs)
{
	TextAlignmentPropertyHandle = InArgs._TextAlignmentPropertyHandle;
	OnHorizontalAlignmentChanged = InArgs._OnHorizontalAlignmentChanged;
	OnVerticalAlignmentChanged = InArgs._OnVerticalAlignmentChanged;

	const SGridPanel::Layer CellBordersLayer(0);
	const SGridPanel::Layer BackgroundLayer(1);
	const SGridPanel::Layer ForegroundLayer(3);
	
	const FLinearColor CellBorderColor = FStyleColors::Foldout.GetSpecifiedColor();
	const FLinearColor BackgroundColor = FStyleColors::Panel.GetSpecifiedColor();

	HorizontalAlignmentPropertyHandle = TextAlignmentPropertyHandle.Get()->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAvaTextAlignment, HorizontalAlignment));
	VerticalAlignmentPropertyHandle = TextAlignmentPropertyHandle.Get()->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAvaTextAlignment, VerticalAlignment));
	
	ChildSlot
	[
		SNew(SBox)
		.Padding(FMargin(3.f, 10.f, 3.f, 10.f))
		[
			SNew(SGridPanel)
			+ SGridPanel::Slot(0, 0, CellBordersLayer)
			.RowSpan(2)
			.ColumnSpan(5)
			[
				SNew(SColorBlock)
				.Color(CellBorderColor)
			]
			+ SGridPanel::Slot(1, 0, ForegroundLayer)
			[
				GetHorizontalAlignmentButton(HorizontalLeftButton, EText3DHorizontalTextAlignment::Left, "Icons.Alignment.Left", LOCTEXT("AlignSelectedTextLeft", "Align Text to the Left"))
			]
			+ SGridPanel::Slot(2, 0, ForegroundLayer)
			[
				GetHorizontalAlignmentButton(HorizontalCenterButton, EText3DHorizontalTextAlignment::Center, "Icons.Alignment.Center_Y", LOCTEXT("AlignSelectedTextCenter", "Align Text to Center"))
			]
			+ SGridPanel::Slot(3, 0, ForegroundLayer)
			[
				GetHorizontalAlignmentButton(HorizontalRightButton, EText3DHorizontalTextAlignment::Right, "Icons.Alignment.Right", LOCTEXT("AlignSelectedTextRight", "Align Text to the Right"))
			]
			+ SGridPanel::Slot(0, 0, BackgroundLayer) // color 1st empty slot
			[
				SNew(SColorBlock)
				.Color(BackgroundColor)
			]
			+ SGridPanel::Slot(0, 1, ForegroundLayer)
			[
				GetVerticalAlignmentButton(VerticalFirstLineButton, EText3DVerticalTextAlignment::FirstLine, "Icons.Alignment.Top", LOCTEXT("AlignSelectedTextFirstLine", "Align Text to First Line"))
			]
			+ SGridPanel::Slot(1, 1, ForegroundLayer)
			[
				GetVerticalAlignmentButton(VerticalTopButton, EText3DVerticalTextAlignment::Top, "Icons.Alignment.Top", LOCTEXT("AlignSelectedTextTop", "Align Text to Top"))
			]
			+ SGridPanel::Slot(2, 1, ForegroundLayer)
			[
				GetVerticalAlignmentButton(VerticalCenterButton, EText3DVerticalTextAlignment::Center, "Icons.Alignment.Center_Z", LOCTEXT("AlignSelectedTextCenter", "Align Text to Center"))
			]
			+ SGridPanel::Slot(3, 1, ForegroundLayer)
			[
				GetVerticalAlignmentButton(VerticalBottomButton, EText3DVerticalTextAlignment::Bottom, "Icons.Alignment.Bottom", LOCTEXT("AlignSelectedTextBottom", "Align Text to Bottom"))
			]
		]
	];

	TextAlignmentPropertyHandle.Get()->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &SAvaTextAlignmentWidget::OnAlignmentPropertyChanged));
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

TSharedRef<SButton> SAvaTextAlignmentWidget::GetHorizontalAlignmentButton(TSharedPtr<SButton>& OutButton, EText3DHorizontalTextAlignment InHorizontalAlignment, FName Image, FText Tooltip)
{
	const ISlateStyle* MotionDesignEditorStyle = FSlateStyleRegistry::FindSlateStyle("AvaEditor");

	OutButton = SNew(SButton)
		.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
		.ContentPadding(FMargin(2.f))
		.OnClicked(this, &SAvaTextAlignmentWidget::ApplyHorizontalAlignment, InHorizontalAlignment)
		.ToolTipText(Tooltip)
		[
			SNew(SImage)
			.Image(MotionDesignEditorStyle ? MotionDesignEditorStyle->GetBrush(Image) : nullptr)
			.DesiredSizeOverride(FVector2D(16.f, 16.f))
			.ColorAndOpacity(this, &SAvaTextAlignmentWidget::ApplyHorizontalAlignmentButtonColorAndOpacity, InHorizontalAlignment)
		];

	return OutButton.ToSharedRef();
}

TSharedRef<SButton> SAvaTextAlignmentWidget::GetVerticalAlignmentButton(TSharedPtr<SButton>& OutButton, EText3DVerticalTextAlignment InVerticalAlignment, FName Image, FText Tooltip)
{
	const ISlateStyle* MotionDesignEditorStyle = FSlateStyleRegistry::FindSlateStyle("AvaEditor");

	OutButton = SNew(SButton)
		.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
		.ContentPadding(FMargin(2.f))
		.OnClicked(this, &SAvaTextAlignmentWidget::ApplyVerticalAlignment, InVerticalAlignment)
		.ToolTipText(Tooltip)
		[
			SNew(SImage)
			.Image(MotionDesignEditorStyle ? MotionDesignEditorStyle->GetBrush(Image) : nullptr)
			.DesiredSizeOverride(FVector2D(16.f, 16.f))
			.ColorAndOpacity(this, &SAvaTextAlignmentWidget::ApplyVerticalAlignmentButtonColorAndOpacity, InVerticalAlignment)
		];

	return OutButton.ToSharedRef();
}

FReply SAvaTextAlignmentWidget::ApplyHorizontalAlignment(EText3DHorizontalTextAlignment InHorizontalAlignment) const
{
	OnHorizontalAlignmentChanged.ExecuteIfBound(InHorizontalAlignment);

	return FReply::Handled();
}

FReply SAvaTextAlignmentWidget::ApplyVerticalAlignment(EText3DVerticalTextAlignment InVerticalAlignment) const
{
	OnVerticalAlignmentChanged.ExecuteIfBound(InVerticalAlignment);

	return FReply::Handled();
}

FSlateColor SAvaTextAlignmentWidget::ApplyHorizontalAlignmentButtonColorAndOpacity(EText3DHorizontalTextAlignment InHorizontalAlignment) const
{
	return InHorizontalAlignment == GetCurrentHorizontalAlignment() ?
		FSlateColor(EStyleColor::AccentBlue).GetSpecifiedColor() :
		FSlateColor(EStyleColor::Foreground).GetSpecifiedColor();
}

FSlateColor SAvaTextAlignmentWidget::ApplyVerticalAlignmentButtonColorAndOpacity(EText3DVerticalTextAlignment InVerticalAlignment) const
{
	return InVerticalAlignment == GetCurrentVerticalAlignment() ?
		FSlateColor(EStyleColor::AccentBlue).GetSpecifiedColor() :
		FSlateColor(EStyleColor::Foreground).GetSpecifiedColor();
}

void SAvaTextAlignmentWidget::OnAlignmentPropertyChanged() const
{
	OnHorizontalAlignmentChanged.ExecuteIfBound(GetCurrentHorizontalAlignment());
	OnVerticalAlignmentChanged.ExecuteIfBound(GetCurrentVerticalAlignment());
}

EText3DHorizontalTextAlignment SAvaTextAlignmentWidget::GetCurrentHorizontalAlignment() const
{
	if (HorizontalAlignmentPropertyHandle)
	{
		uint8 HorizontalAlignAsUint8;
		HorizontalAlignmentPropertyHandle->GetValue(HorizontalAlignAsUint8);
		return static_cast<EText3DHorizontalTextAlignment>(HorizontalAlignAsUint8);
	}

	return EText3DHorizontalTextAlignment::Left;
}

EText3DVerticalTextAlignment SAvaTextAlignmentWidget::GetCurrentVerticalAlignment() const
{
	if (VerticalAlignmentPropertyHandle)
	{
		uint8 VerticalAlignAsUint8;
		VerticalAlignmentPropertyHandle->GetValue(VerticalAlignAsUint8);
		return static_cast<EText3DVerticalTextAlignment>(VerticalAlignAsUint8);
	}

	return EText3DVerticalTextAlignment::FirstLine;
}

#undef LOCTEXT_NAMESPACE
