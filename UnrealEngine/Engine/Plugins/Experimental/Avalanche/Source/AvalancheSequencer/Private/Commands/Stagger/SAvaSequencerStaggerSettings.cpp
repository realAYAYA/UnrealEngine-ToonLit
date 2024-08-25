// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAvaSequencerStaggerSettings.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/Reply.h"
#include "Misc/Optional.h"
#include "SPrimaryButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SAvaSequencerStaggerSettings"

FAvaSequencerStaggerSettings SAvaSequencerStaggerSettings::Settings;

void SAvaSequencerStaggerSettings::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(5.f)
		[
			ConstructStartPositionRadioGroup()
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(5.f, 0.f, 5.f, 5.f)
		[
			ConstructOperationPointRadioGroup()
		]
		+ SVerticalBox::Slot()
		.Padding(10.f, 5.f, 10.f, 10.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ContentPadding(2.f)
				.ButtonStyle(FAppStyle::Get(), TEXT("SimpleButton"))
				.ToolTipText(LOCTEXT("ResetToDefaultsToolTip", "Reset All to Defaults"))
				.OnClicked(this, &SAvaSequencerStaggerSettings::OnResetToDefaults)
				[
					SNew(SBox)
					.WidthOverride(16.f)
					.HeightOverride(16.f)
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush(TEXT("PropertyWindow.DiffersFromDefault")))
					]
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(10.f, 0.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ShiftFramesLabel", "Shift Frames:"))
				.Justification(ETextJustify::Right)
			]
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.Padding(10.f, 0.f, 0.f, 0.f)
			[
				SNew(SNumericEntryBox<int32>)
				.AllowSpin(true)
				.MinSliderValue(-60)
				.MaxSliderValue(60)
				.Value(this, &SAvaSequencerStaggerSettings::GetShiftFrame)
				.OnValueChanged(this, &SAvaSequencerStaggerSettings::OnShiftFrameChanged)
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(10.f, 5.f, 10.f, 10.f)
		.HAlign(HAlign_Right)
		[
			SNew(SUniformGridPanel)
			.SlotPadding(FMargin(5.f, 0.f))
			+ SUniformGridPanel::Slot(0, 0)
			[
				SNew(SPrimaryButton)
				.Text(LOCTEXT("OkButtonText", "OK"))
				.OnClicked(this, &SAvaSequencerStaggerSettings::OnReturnButtonClicked, EAppReturnType::Ok)
			]
			+ SUniformGridPanel::Slot(1, 0)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.Text(LOCTEXT("CancelButtonText", "Cancel"))
				.OnClicked(this, &SAvaSequencerStaggerSettings::OnReturnButtonClicked, EAppReturnType::Cancel)
			]
		]
	];
}

TSharedRef<SCheckBox> SAvaSequencerStaggerSettings::ConstructRadioButton(const TAttribute<ECheckBoxState>& InCheckBoxState
	, const FOnCheckStateChanged& InOnCheckStateChanged
	, const TAttribute<FText>& InText
	, const TAttribute<FText>& InToolTipText)
{
	return SNew(SCheckBox)
		.Style(FAppStyle::Get(), TEXT("RadioButton"))
		.ToolTipText(InToolTipText)
		.IsChecked(InCheckBoxState)
		.OnCheckStateChanged(InOnCheckStateChanged)
		[
			SNew(STextBlock)
			.Text(InText)
		];
}

TSharedRef<SWidget> SAvaSequencerStaggerSettings::ConstructStartPositionRadioGroup()
{
	auto CreateRadioButton = [this](const FAvaSequencerStaggerSettings::EStartPosition InStartPosition
		, const TAttribute<FText>& InText
		, const TAttribute<FText>& InToolTipText) -> TSharedRef<SCheckBox>
	{
		const TAttribute<ECheckBoxState> CheckBoxState = TAttribute<ECheckBoxState>::CreateLambda(
			[this, InStartPosition]() -> ECheckBoxState
			{
				return (Settings.StartPosition == InStartPosition)
					? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			});
		const FOnCheckStateChanged OnCheckStateChanged = FOnCheckStateChanged::CreateLambda(
			[this, InStartPosition](const ECheckBoxState InState)
			{
				if (InState == ECheckBoxState::Checked)
				{
					Settings.StartPosition = InStartPosition;
				}
			});
		return ConstructRadioButton(CheckBoxState, OnCheckStateChanged, InText, InToolTipText);
	};

	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
		.Padding(3.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(0.35f)
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ShiftFirstLabel", "First:"))
			]
			+ SHorizontalBox::Slot()
			.FillWidth(0.65f)
			.Padding(10.f, 0.f, 0.f, 0.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f)
				[
					CreateRadioButton(FAvaSequencerStaggerSettings::EStartPosition::FirstSelected
						, LOCTEXT("FirstSelectedLabel", "Selected")
						, LOCTEXT("FirstSelectedTooltip", "Uses the first selected sequence, regardless of time, to begin the operation"))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 1.f, 0.f, 0.f)
				[
					CreateRadioButton(FAvaSequencerStaggerSettings::EStartPosition::FirstInTimeline
						, LOCTEXT("FirstInTimelineLabel", "In Timeline")
						, LOCTEXT("FirstInTimelineTooltip", "Uses the start of the first sequence in time, from all selected sequences, to begin the operation"))
				]
			]
		];
}

TSharedRef<SWidget> SAvaSequencerStaggerSettings::ConstructOperationPointRadioGroup()
{
	auto CreateRadioButton = [this](const FAvaSequencerStaggerSettings::EOperationPoint InOperationPoint
		, const TAttribute<FText>& InText
		, const TAttribute<FText>& InToolTipText) -> TSharedRef<SCheckBox>
	{
		const TAttribute<ECheckBoxState> CheckBoxState = TAttribute<ECheckBoxState>::CreateLambda(
			[this, InOperationPoint]() -> ECheckBoxState
			{
				return (Settings.OperationPoint == InOperationPoint)
					? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			});
		const FOnCheckStateChanged OnCheckStateChanged = FOnCheckStateChanged::CreateLambda(
			[this, InOperationPoint](const ECheckBoxState InState)
			{
				if (InState == ECheckBoxState::Checked)
				{
					Settings.OperationPoint = InOperationPoint;
				}
			});
		return ConstructRadioButton(CheckBoxState, OnCheckStateChanged, InText, InToolTipText);
	};

	return SNew(SBorder)
		.Padding(3.f)
		.BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(0.35f)
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ShiftFromLabel", "From:"))
			]
			+ SHorizontalBox::Slot()
			.FillWidth(0.65f)
			.Padding(10.f, 0.f, 0.f, 0.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f)
				[
					CreateRadioButton(FAvaSequencerStaggerSettings::EOperationPoint::Start
						, LOCTEXT("ShiftFromStartLabel", "Layer Start")
						, LOCTEXT("ShiftFromStartToolTip", "Staggers each sequence from the start of the previous sequence in the selection order"))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 1.f, 0.f, 0.f)
				[
					CreateRadioButton(FAvaSequencerStaggerSettings::EOperationPoint::End
						, LOCTEXT("ShiftFromEndLabel", "Layer End")
						, LOCTEXT("ShiftFromEndLabelToolTip", "Staggers each sequence from the end of the previous sequence in the selection order"))
				]
			]
		];
}

TOptional<int32> SAvaSequencerStaggerSettings::GetShiftFrame() const
{
	return Settings.Shift.FrameNumber.Value;
}

void SAvaSequencerStaggerSettings::OnShiftFrameChanged(int32 InNewValue)
{
	Settings.Shift.FrameNumber.Value = InNewValue;
}

FReply SAvaSequencerStaggerSettings::OnReturnButtonClicked(EAppReturnType::Type InReturnType)
{
	ReturnType = InReturnType;
	FSlateApplication::Get().FindWidgetWindow(AsShared())->RequestDestroyWindow();
	return FReply::Handled();
}

FReply SAvaSequencerStaggerSettings::OnResetToDefaults()
{
	Settings = FAvaSequencerStaggerSettings();
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
