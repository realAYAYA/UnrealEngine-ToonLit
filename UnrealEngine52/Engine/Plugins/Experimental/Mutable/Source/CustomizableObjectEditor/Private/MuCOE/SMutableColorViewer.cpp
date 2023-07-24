// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/SMutableColorViewer.h"

#include "MuCOE/SMutableColorPreviewBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


void SMutableColorViewer::Construct(const FArguments& InArgs)
{
	const FText ColorValueTitle = LOCTEXT("ColorValuesTitle", "Color Values : ");

	const FText RedColorValueTitle = LOCTEXT("RedColorValueTitle", "Red : ");
	const FText GreenColorValueTitle = LOCTEXT("GreenColorValueTitle", "Green : ");
	const FText BlueColorValueTitle = LOCTEXT("BlueColorValueTitle", "Blue : ");
	const FText AlphaColorValueTitle = LOCTEXT("AlphaColorValueTitle", "Alpha : ");

	const int32 IndentationSpace = 16;	
	const int32 AfterTitleSpacing = 4;
	const int32 ColorPreviewHorizontalPadding = 18;

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[	
			SNew(STextBlock).
			Text(ColorValueTitle)
		]
		
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(IndentationSpace, AfterTitleSpacing)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SVerticalBox)

				// Red channel -----------------------------------------
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(STextBlock).
						Text(RedColorValueTitle)
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(STextBlock).
						Text(this, &SMutableColorViewer::GetRedValue)
					]
				]
				// -----------------------------------------------------

				// Green channel ---------------------------------------
				+SVerticalBox::Slot()
				.Padding(0, 1)
				.AutoHeight()
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(STextBlock).
						Text(GreenColorValueTitle)
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(STextBlock).
						Text(this, &SMutableColorViewer::GetGreenValue)
					]
				]
				// -----------------------------------------------------

				// Blue channel ----------------------------------------
				+SVerticalBox::Slot()
					.Padding(0, 1)
					.AutoHeight()
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(STextBlock).
						Text(BlueColorValueTitle)
					]

				+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(STextBlock).
						Text(this, &SMutableColorViewer::GetBlueValue)
					]
					]
				// -----------------------------------------------------

				// Alpha channel ----------------------------------------
				+SVerticalBox::Slot()
					.Padding(0, 1)
					.AutoHeight()
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(STextBlock).
						Text(AlphaColorValueTitle)
					]

				+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(STextBlock).
						Text(this, &SMutableColorViewer::GetAlphaValue)
					]
					]
				// -----------------------------------------------------
			]
		
			// Color preview widget --------------------------------
			+ SHorizontalBox::Slot()
			.Padding(ColorPreviewHorizontalPadding,0)
			.MaxWidth(120)
			[
				SAssignNew(this->ColorPreview, SMutableColorPreviewBox)
				// .BoxColor(this, &SMutableColorViewer::GetPreviewColor)
			]
			// -----------------------------------------------------
		]
	];
}

void SMutableColorViewer::SetColor(float InRed, float InGreen, float InBlue, float InAlpha)
{
	this->RedValue = InRed;
	this->GreenValue = InGreen;
	this->BlueValue = InBlue;
	this->AlphaValue = InAlpha;

	this->ColorPreview->SetColor( this->GetPreviewColor() );
}


FSlateColor SMutableColorViewer::GetPreviewColor() const
{
	return FSlateColor { FLinearColor{this->RedValue, this->GreenValue, this->BlueValue, this->AlphaValue} };
}

FText SMutableColorViewer::GetRedValue() const
{
	return FText::AsNumber(this->RedValue);
}

FText SMutableColorViewer::GetGreenValue() const
{
	return FText::AsNumber(this->GreenValue);
}

FText SMutableColorViewer::GetBlueValue() const
{
	return FText::AsNumber(this->BlueValue);
}

FText SMutableColorViewer::GetAlphaValue() const
{
	return FText::AsNumber(this->AlphaValue);
}

#undef LOCTEXT_NAMESPACE
