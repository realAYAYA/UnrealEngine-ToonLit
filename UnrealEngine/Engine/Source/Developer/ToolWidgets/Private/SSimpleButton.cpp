// Copyright Epic Games, Inc. All Rights Reserved.

#include "SSimpleButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"

void SSimpleButton::Construct(const FArguments& InArgs)
{
	SButton::Construct(SButton::FArguments()
		.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>(InArgs._Text.IsSet() ? "SimpleButtonLabelAndIcon" : "SimpleButton"))
		.OnClicked(InArgs._OnClicked)
		.ForegroundColor(FSlateColor::UseStyle())
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SImage)
				.ColorAndOpacity(FSlateColor::UseForeground())
				.Image(InArgs._Icon)
				.Visibility(InArgs._Icon.IsSet() ? EVisibility::HitTestInvisible : EVisibility::Collapsed)
			]
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.Padding(FMargin(3, 0, 0, 0))
			.AutoWidth()
			[
				SNew(STextBlock)
				.TextStyle(&FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("SmallButtonText"))
				.Text(InArgs._Text)
				.Visibility(InArgs._Text.IsSet() ? EVisibility::HitTestInvisible : EVisibility::Collapsed)
			]
		]
	);
}

