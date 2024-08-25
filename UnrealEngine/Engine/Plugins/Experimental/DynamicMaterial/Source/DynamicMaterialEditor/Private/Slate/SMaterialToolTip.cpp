// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMaterialToolTip.h"
#include "SlateOptMacros.h"
#include "Styling/SlateBrush.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SMaterialToolTip"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

SMaterialToolTip::SMaterialToolTip()
	: MaterialBrush(FSlateMaterialBrush(FVector2D(1.0f, 1.0f)))
{
}

void SMaterialToolTip::Construct(const FArguments& InArgs)
{
	Material = InArgs._Material;
	Text = InArgs._Text;
	MaterialSize = InArgs._MaterialSize;
	ShowMaterial = InArgs._ShowMaterial;
	ShowDefault = InArgs._ShowDefault;

	ChildSlot
	[
		CreateToolTipWidget()
	];
}

TSharedRef<SWidget> SMaterialToolTip::CreateToolTipWidget()
{
	if (ShowDefault)
	{
		return SNullWidget::NullWidget;
	}

	MaterialBrush.SetImageSize(MaterialSize.Get());
	MaterialBrush.SetMaterial(Material.Get());

	return 
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		[
			SNew(STextBlock)
			.Text(Text)
			.Font(FCoreStyle::Get().GetFontStyle("ToolTip.Font"))
			.ColorAndOpacity(FSlateColor::UseForeground())
			.WrapTextAt_Static(&SToolTip::GetToolTipWrapWidth)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Top)
		[
			SNew(SBox)
			.Padding(0.0f, 10.0f, 4.0f, 4.0f)
			.Visibility(this, &SMaterialToolTip::GetShowMaterialVisibility)
			[
				SNew(SBorder)
				.Padding(FMargin(0.0f, 0.0f, 5.0f, 5.0f))
				.BorderImage(FAppStyle::Get().GetBrush("ContentBrowser.AssetTileItem.DropShadow"))
				[
					SNew(SBox)
					.WidthOverride(this, &SMaterialToolTip::GetMaterialSizeX)
					.HeightOverride(this, &SMaterialToolTip::GetMaterialSizeY)
					[
						SNew(SScaleBox)
						.Stretch(EStretch::ScaleToFill)
						.StretchDirection(EStretchDirection::Both)
						[
							SNew(SBorder)
							.Padding(0.0f)
							.BorderImage(FAppStyle::Get().GetBrush("ContentBrowser.AssetTileItem.ThumbnailAreaBackground"))
							[
								SNew(SImage)
								.Image(&MaterialBrush)
							]
						]
					]
				]
			]
		];
}

EVisibility SMaterialToolTip::GetShowMaterialVisibility() const
{
	return ShowMaterial.Get() ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed;
}

FOptionalSize SMaterialToolTip::GetMaterialSizeX() const
{
	return MaterialSize.Get().X;
}

FOptionalSize SMaterialToolTip::GetMaterialSizeY() const
{
	return MaterialSize.Get().Y;
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

#undef LOCTEXT_NAMESPACE
