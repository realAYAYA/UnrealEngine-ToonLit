// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXFader.h"

#include "DMXEditorLog.h"
#include "DMXEditorStyle.h"
#include "DMXProtocolCommon.h"
#include "DMXProtocolSettings.h"
#include "Interfaces/IDMXProtocol.h"
#include "IO/DMXOutputPort.h"
#include "Widgets/OutputConsole/SDMXOutputFaderList.h"

#include "Styling/SlateTypes.h"
#include "Widgets/Common/SSpinBoxVertical.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"


#define LOCTEXT_NAMESPACE "SDMXFader"

namespace DMXFader
{
	const FSlateFontInfo NameFont = FCoreStyle::GetDefaultFontStyle("Regular", 7);
	const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Regular", 8);
};

void SDMXFader::Construct(const FArguments& InArgs)
{
	OnRequestDelete = InArgs._OnRequestDelete;
	OnRequestSelect = InArgs._OnRequestSelect;
	FaderName = InArgs._FaderName.ToString();
	UniverseID = InArgs._UniverseID;
	MaxValue = InArgs._MaxValue;
	MinValue = InArgs._MinValue;
	StartingAddress = InArgs._StartingAddress;
	EndingAddress = InArgs._EndingAddress;

	SanetizeDMXProperties();

	ChildSlot
	.Padding(5.0f, 0.0f)
	[
		SNew(SBox)
		.WidthOverride(80.0f)
		[
			SAssignNew(BackgroundBorder, SBorder)
			.BorderImage(this, &SDMXFader::GetBorderImage)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.VAlign(VAlign_Top)
				.HAlign(HAlign_Fill)
				.Padding(1.0f, 5.0f, 1.0f, 1.0f)
				.AutoHeight()
				[					
					SNew(SHorizontalBox)
		
					// Fader Name
					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Fill)					
					.FillWidth(1.0f)
					[
						SNew(SBorder)
						.BorderBackgroundColor(FLinearColor::Black)
						.OnMouseButtonDown(this, &SDMXFader::OnFaderNameBorderClicked)
						[
							SAssignNew(FaderNameTextBox, SInlineEditableTextBlock)
							.MultiLine(false)
							.Text(InArgs._FaderName)		
							.Font(DMXFader::TitleFont)
							.Justification(ETextJustify::Center)
							.ColorAndOpacity(FLinearColor::White)
							.Style(FCoreStyle::Get(), "InlineEditableTextBlockSmallStyle")
							.OnTextCommitted(this, &SDMXFader::OnFaderNameCommitted)
						]
					]

					// Delete Button
					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Top)
					.HAlign(HAlign_Right)					
					.Padding(FMargin(1.0f, 0.0f, 0.0f, 0.0f))
					.AutoWidth()
					[
						SNew(SButton)
						.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
						.OnClicked(this, &SDMXFader::OnDeleteClicked)
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("x")))
							.Font(DMXFader::NameFont)
							.ColorAndOpacity(FLinearColor::White)
						]
					]
				]

				+ SVerticalBox::Slot()
				.VAlign(VAlign_Top)
				.HAlign(HAlign_Center)
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.MaxWidth(35.0f)
					[
						// Max Value
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.VAlign(VAlign_Bottom)
						.HAlign(HAlign_Fill)
						.AutoHeight()
						[
							SNew(SBorder)
							.OnMouseButtonDown(this, &SDMXFader::OnMaxValueBorderClicked)
							.BorderBackgroundColor(FLinearColor::Black)
							.Padding(5.0f)
							[
								SAssignNew(MaxValueEditableTextBlock, SInlineEditableTextBlock)
								.MultiLine(false)
								.Text(this, &SDMXFader::GetMaxValueAsText)
								.Justification(ETextJustify::Center)
								.OnTextCommitted(this, &SDMXFader::OnMaxValueCommitted)
								.Style(FCoreStyle::Get(), "InlineEditableTextBlockSmallStyle")
							]
						]
								
						// Fader Control
						+ SVerticalBox::Slot()
						.VAlign(VAlign_Top)
						.HAlign(HAlign_Center)
						.Padding(0.0f, 1.0f, 0.0f, 1.0f)
						.AutoHeight()
						[
							SNew(SBorder)
							.BorderBackgroundColor(FLinearColor::Black)
							[
								SAssignNew(FaderSpinBox, SSpinBoxVertical<uint8>)
								.Value(FMath::Clamp(Value, MinValue, MaxValue))
								.MinValue(MinValue)
								.MaxValue(MaxValue)
								.MinSliderValue(0)
								.MaxSliderValue(DMX_MAX_VALUE)
								.OnValueChanged(this, &SDMXFader::HandleValueChanged)
								.Style(FDMXEditorStyle::Get(), "DMXEditor.OutputConsole.Fader")
								.MinDesiredWidth(30.0f)
							]
						]

						// Fader Min Value
						+ SVerticalBox::Slot()
						.VAlign(VAlign_Bottom)
						.HAlign(HAlign_Fill)
						.AutoHeight()
						[
							SNew(SBorder)
							.OnMouseButtonDown(this, &SDMXFader::OnMinValueBorderClicked)
							.BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 1.0f))
							.Padding(5.0f)
							[
								SAssignNew(MinValueEditableTextBlock, SInlineEditableTextBlock)
								.MultiLine(false)
								.Text(this, &SDMXFader::GetMinValueAsText)
								.Justification(ETextJustify::Center)
								.OnTextCommitted(this, &SDMXFader::OnMinValueCommitted)
								.Style(FCoreStyle::Get(), "InlineEditableTextBlockSmallStyle")
							]
						]
					]
				]

				+ SVerticalBox::Slot()
				.VAlign(VAlign_Top)
				.HAlign(HAlign_Fill)
				.AutoHeight()
				.Padding(FMargin(0.0f, 4.0f, 0.0f, 0.0f))
				[
					GenerateAdressEditWidget()
				]
			]
		]
	];
}

void SDMXFader::Select()
{
	bSelected = true;

	BackgroundBorder->SetBorderImage(FDMXEditorStyle::Get().GetBrush("DMXEditor.OutputConsole.Fader_Highlighted"));

	FSlateApplication::Get().SetKeyboardFocus(SharedThis(this));
}

void SDMXFader::Unselect()
{
	bSelected = false;

	BackgroundBorder->SetBorderImage(FDMXEditorStyle::Get().GetBrush("DMXEditor.OutputConsole.Fader"));
}

void SDMXFader::SanetizeDMXProperties()
{
	if (UniverseID < 0)
	{
		UniverseID = 0;
	}
	else if (UniverseID > DMX_MAX_UNIVERSE)
	{
		UniverseID = DMX_MAX_UNIVERSE;
	}

	if (MaxValue > DMX_MAX_VALUE)
	{
		MaxValue = DMX_MAX_VALUE;
	}

	if (MinValue > MaxValue)
	{
		MinValue = MaxValue;
	}

	if (EndingAddress < 1)
	{
		EndingAddress = 1;
	}
	else if (EndingAddress > DMX_MAX_ADDRESS)
	{
		EndingAddress = DMX_MAX_ADDRESS;
	}

	if (StartingAddress < 1)
	{
		StartingAddress = 1;
	}
	else if (StartingAddress > EndingAddress)
	{
		StartingAddress = EndingAddress;
	}
}

FReply SDMXFader::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Delete)
	{
		OnRequestDelete.ExecuteIfBound(SharedThis(this));
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FReply SDMXFader::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		OnRequestSelect.ExecuteIfBound(SharedThis(this));
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

TSharedRef<SWidget> SDMXFader::GenerateAdressEditWidget()
{
	FMargin LabelPadding = FMargin(4.0f, 0.0f, 8.0f, 1.0f);
	FMargin ValuePadding = FMargin(0.0f, 0.0f, 4.0f, 1.0f);

	return 
		SNew(SBorder)
		.Padding(FMargin(2.0f, 4.0f, 2.0f, 4.0f))
		[
			SNew(SGridPanel)
			.FillColumn(1, 1.0f)

			+ SGridPanel::Slot(0, 0)
			.VAlign(VAlign_Top)
			.HAlign(HAlign_Left)
			.Padding(LabelPadding)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Universe", "Uni:"))
				.Font(DMXFader::NameFont)
			]
	
			+ SGridPanel::Slot(1, 0)
			.VAlign(VAlign_Top)
			.HAlign(HAlign_Fill)
			.Padding(ValuePadding)
			[
				SNew(SBorder)
				.ToolTipText(LOCTEXT("UniverseIDTooltip", "The Universe to which DMX is sent to"))
				.BorderImage(FAppStyle::GetBrush("EditableTextBox.Background.Focused"))
				.OnMouseButtonDown(this, &SDMXFader::OnUniverseIDBorderClicked)
				[
					SAssignNew(UniverseIDEditableTextBlock, SInlineEditableTextBlock)						
					.MultiLine(false)
					.Text(this, &SDMXFader::GetUniverseIDAsText)
					.Justification(ETextJustify::Center)
					.Font(DMXFader::NameFont)
					.ColorAndOpacity(FLinearColor::Black)
					.OnTextCommitted(this, &SDMXFader::OnUniverseIDCommitted)
				]
			]
	
			+ SGridPanel::Slot(0, 1)
			.VAlign(VAlign_Top)		
			.HAlign(HAlign_Fill)
			.Padding(LabelPadding)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("StaringAddressLabel", "Adr:"))
				.Font(DMXFader::NameFont)
			]

			+ SGridPanel::Slot(1, 1)
			.VAlign(VAlign_Top)
			.HAlign(HAlign_Fill)
			.Padding(ValuePadding)
			[
				SNew(SBorder)
				.ToolTipText(LOCTEXT("StartingAdressTooltip", "The Starting Adress of the Channel to which DMX is sent to"))
				.BorderImage(FAppStyle::GetBrush("EditableTextBox.Background.Focused"))
				.OnMouseButtonDown(this, &SDMXFader::OnStartingAddressBorderClicked)
				[
					SAssignNew(StartingAddressEditableTextBlock, SInlineEditableTextBlock)
					.MultiLine(false)
					.Text(this, &SDMXFader::GetStartingAddressAsText)
					.Justification(ETextJustify::Center)
					.Font(DMXFader::NameFont)
					.ColorAndOpacity(FLinearColor::Black)
					.OnTextCommitted(this, &SDMXFader::OnStartingAddressCommitted)
				]
			]

			+ SGridPanel::Slot(1, 2)			
			.VAlign(VAlign_Top)	
			.HAlign(HAlign_Fill)
			.Padding(ValuePadding)
			[
				SNew(SBorder)
				.ToolTipText(LOCTEXT("EndingAdressTooltip", "The Ending Adress of the Channel to which DMX is sent to"))
				.BorderImage(FAppStyle::GetBrush("EditableTextBox.Background.Focused"))
				.OnMouseButtonDown(this, &SDMXFader::OnEndingAddressBorderClicked)
				[
					SAssignNew(EndingAddressEditableTextBlock, SInlineEditableTextBlock)					
					.MultiLine(false)
					.Text(this, &SDMXFader::GetEndingAddressAsText)
					.Justification(ETextJustify::Center)
					.Font(DMXFader::NameFont)
					.ColorAndOpacity(FLinearColor::Black)
					.OnTextCommitted(this, &SDMXFader::OnEndingAddressCommitted)
				]
			]
		];
}

uint8 SDMXFader::GetValue() const
{
	check(FaderSpinBox.IsValid());
	return FaderSpinBox->GetValue();
}

void SDMXFader::SetValueByPercentage(float InNewPercentage)
{
	check(FaderSpinBox.IsValid());
	float Range = MaxValue - MinValue;
	FaderSpinBox->SetValue(static_cast<uint8>(Range * InNewPercentage / 100.0f) + MinValue);
}

FReply SDMXFader::OnDeleteClicked()
{
	OnRequestDelete.ExecuteIfBound(SharedThis(this));

	return FReply::Handled();
}

void SDMXFader::HandleValueChanged(uint8 NewValue)
{
	Value = NewValue;
}

FReply SDMXFader::OnFaderNameBorderClicked(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		FaderNameTextBox->EnterEditingMode();

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void SDMXFader::OnFaderNameCommitted(const FText& NewFaderName, ETextCommit::Type InCommit)
{
	check(FaderNameTextBox.IsValid());

	FaderName = NewFaderName.ToString();

	FaderNameTextBox->SetText(FaderName);
}

FReply SDMXFader::OnMaxValueBorderClicked(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		MaxValueEditableTextBlock->EnterEditingMode();

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FReply SDMXFader::OnMinValueBorderClicked(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		MinValueEditableTextBlock->EnterEditingMode();

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FReply SDMXFader::OnUniverseIDBorderClicked(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		UniverseIDEditableTextBlock->EnterEditingMode();
		
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FReply SDMXFader::OnStartingAddressBorderClicked(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		StartingAddressEditableTextBlock->EnterEditingMode();

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FReply SDMXFader::OnEndingAddressBorderClicked(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		EndingAddressEditableTextBlock->EnterEditingMode();

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

const FSlateBrush* SDMXFader::GetBorderImage() const
{
	if (IsHovered())
	{
		return FAppStyle::GetBrush("DetailsView.CategoryMiddle_Hovered");
	}
	else
	{
		return FAppStyle::GetBrush("DetailsView.CategoryMiddle");
	}
}

void SDMXFader::OnUniverseIDCommitted(const FText& UniverseIDText, ETextCommit::Type InCommit)
{
	FString Str = UniverseIDText.ToString();

	int32 NewUniverseID;
	if (LexTryParseString<int32>(NewUniverseID, *Str))
	{
		UniverseID = FMath::Clamp(NewUniverseID, 1, DMX_MAX_UNIVERSE);
	}
}

void SDMXFader::OnStartingAddressCommitted(const FText& StartingAddressText, ETextCommit::Type InCommit)
{
	FString Str = StartingAddressText.ToString();
	int32 StrValue;
	if (LexTryParseString<int32>(StrValue, *Str))
	{
		if (StartingAddress != StrValue)
		{
			const int32 EndingAddressOffset = EndingAddress - StartingAddress;

			StartingAddress = FMath::Clamp(StrValue, 1, DMX_MAX_ADDRESS);
			EndingAddress = FMath::Clamp(StartingAddress + EndingAddressOffset, 1, DMX_MAX_ADDRESS);
		}
	}
}

void SDMXFader::OnEndingAddressCommitted(const FText& EndingAddressText, ETextCommit::Type InCommit)
{
	FString Str = EndingAddressText.ToString();
	int32 StrValue;
	if (LexTryParseString<int32>(StrValue, *Str))
	{
		if (EndingAddress != StrValue)
		{
			EndingAddress = FMath::Clamp(StrValue, 1, DMX_MAX_ADDRESS);
		}
	}
}

void SDMXFader::OnMaxValueCommitted(const FText& MaxValueText, ETextCommit::Type InCommit)
{
	FString Str = MaxValueText.ToString();
	int32 StrValue;
	if (LexTryParseString<int32>(StrValue, *Str))
	{
		MaxValue = FMath::Clamp(StrValue, 0, DMX_MAX_VALUE);
		MinValue = FMath::Clamp(MinValue, static_cast<uint8>(0), MaxValue);

		FaderSpinBox->SetMinValue(MinValue);
		FaderSpinBox->SetMaxValue(MaxValue);

		const float NewValue = FMath::Clamp(FaderSpinBox->GetValue(), static_cast<float>(MinValue), static_cast<float>(MaxValue));
		FaderSpinBox->SetValue(NewValue);
	}
}

void SDMXFader::OnMinValueCommitted(const FText& MinValueText, ETextCommit::Type InCommit)
{
	check(FaderSpinBox.IsValid());

	FString Str = MinValueText.ToString();
	int32 StrValue;
	if (LexTryParseString<int32>(StrValue, *Str))
	{
		MinValue = FMath::Clamp(StrValue, 0, DMX_MAX_VALUE);
		MaxValue = FMath::Clamp(MaxValue, MinValue, static_cast<uint8>(DMX_MAX_VALUE));

		FaderSpinBox->SetMinValue(MinValue);
		FaderSpinBox->SetMaxValue(MaxValue);

		const float NewValue = FMath::Clamp(FaderSpinBox->GetValue(), static_cast<float>(MinValue), static_cast<float>(MaxValue));
		FaderSpinBox->SetValue(NewValue);
	}
}

#undef LOCTEXT_NAMESPACE
