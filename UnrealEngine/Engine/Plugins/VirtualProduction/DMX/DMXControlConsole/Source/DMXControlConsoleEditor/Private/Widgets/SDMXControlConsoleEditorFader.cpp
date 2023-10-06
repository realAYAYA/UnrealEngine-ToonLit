// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleEditorFader.h"

#include "DMXControlConsoleEditorSelection.h"
#include "DMXControlConsoleFaderGroup.h"
#include "DMXControlConsoleFixturePatchFunctionFader.h"
#include "DMXControlConsoleFixturePatchCellAttributeFader.h"
#include "DMXControlConsoleRawFader.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Misc/Optional.h"
#include "Models/DMXControlConsoleEditorModel.h"
#include "ScopedTransaction.h"
#include "Style/DMXControlConsoleEditorStyle.h"
#include "Styling/StyleColors.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/SDMXControlConsoleEditorSpinBoxVertical.h"
#include "Widgets/Text/STextBlock.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleEditorFader"

namespace UE::DMXControlConsoleEditor::DMXControlConsoleEditorFader::Private
{
	static float CollapsedViewModeHeight = 230.f;
	static float ExpandedViewModeHeight = 310.f;
};

void SDMXControlConsoleEditorFader::Construct(const FArguments& InArgs, const TObjectPtr<UDMXControlConsoleFaderBase>& InFader)
{
	Fader = InFader;

	ChildSlot
	[
		SNew(SBox)
		.WidthOverride(80.f)
		.HeightOverride(TAttribute<FOptionalSize>::CreateSP(this, &SDMXControlConsoleEditorFader::GetFaderHeightByViewMode))
		.Padding(InArgs._Padding)
		[
			SNew(SBorder)
			.BorderImage(this, &SDMXControlConsoleEditorFader::GetBorderImage)
			.Padding(0.f, 4.f)
			[
				SNew(SVerticalBox)
					
				// Top section
				+ SVerticalBox::Slot()
				.HAlign(HAlign_Center)
				.Padding(0.f, 8.f)
				.AutoHeight()
				[
					SNew(SHorizontalBox)

					// Fader Name
					+ SHorizontalBox::Slot()
					.MaxWidth(50.f)
					.AutoWidth()
					[
						SNew(STextBlock)
						.Text(this, &SDMXControlConsoleEditorFader::GetFaderNameText)
						.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
					]
				]

				// Middle section
				+ SVerticalBox::Slot()
				.HAlign(HAlign_Center)
				.AutoHeight()
				[
					SNew(SVerticalBox)

					// Max Value
					+ SVerticalBox::Slot()
					.HAlign(HAlign_Center)
					.Padding(6.f, 2.f, 6.f, 4.f)
					.AutoHeight()
					[
						SNew(SEditableTextBox)
						.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
						.IsReadOnly(this, &SDMXControlConsoleEditorFader::IsReadOnly)
						.Justification(ETextJustify::Center)
						.MinDesiredWidth(20.f)
						.OnTextCommitted(this, &SDMXControlConsoleEditorFader::OnMaxValueTextCommitted)
						.Text(this, &SDMXControlConsoleEditorFader::GetMaxValueAsText)
						.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorFader::GetExpandedViewModeVisibility))
					]

					// Fader Control
					+ SVerticalBox::Slot()
					.HAlign(HAlign_Center)
					.AutoHeight()
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.MaxWidth(40.f)
						[
							SNew(SOverlay)
							// Spin Box layer
							+ SOverlay::Slot()
							[
								SNew(SBorder)
								.BorderImage(this, &SDMXControlConsoleEditorFader::GetSpinBoxBorderImage)
								[
									SAssignNew(FaderSpinBox, SDMXControlConsoleEditorSpinBoxVertical<uint32>)
									.Value(this, &SDMXControlConsoleEditorFader::GetValue)
									.MinValue(this, &SDMXControlConsoleEditorFader::GetMinValue)
									.MaxValue(this, &SDMXControlConsoleEditorFader::GetMaxValue)
									.MinSliderValue(this, &SDMXControlConsoleEditorFader::GetMinValue)
									.MaxSliderValue(this, &SDMXControlConsoleEditorFader::GetMaxValue)
									.OnBeginSliderMovement(this, &SDMXControlConsoleEditorFader::OnBeginValueChange)
									.OnValueChanged(this, &SDMXControlConsoleEditorFader::HandleValueChanged)
									.OnValueCommitted(this, &SDMXControlConsoleEditorFader::OnValueCommitted)
									.IsActive(this, &SDMXControlConsoleEditorFader::IsFaderSpinBoxActive)
									.Style(FDMXControlConsoleEditorStyle::Get(), "DMXControlConsole.Fader")
									.ToolTipText(this, &SDMXControlConsoleEditorFader::GetToolTipText)
									.MinDesiredWidth(40.0f)
								]
							]

							// Lock Button layer
							+ SOverlay::Slot()
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot()
								[
									SNew(SBox)
								]

								+ SVerticalBox::Slot()
								.HAlign(HAlign_Center)
								.VAlign(VAlign_Center)
								.Padding(0.f, 4.f)
								.AutoHeight()
								[
									GenerateLockButtonWidget()
								]
							]
						]
					]

					// Fader Value
					+ SVerticalBox::Slot()
					.HAlign(HAlign_Center)
					.Padding(6.f, 4.f)
					.AutoHeight()
					[
						SNew(SEditableTextBox)
						.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
						.FocusedForegroundColor(FLinearColor::White)
						.ForegroundColor(FLinearColor::FromSRGBColor(FColor::FromHex("0088f7")))
						.IsReadOnly(this, &SDMXControlConsoleEditorFader::IsReadOnly)
						.Justification(ETextJustify::Center)
						.OnTextCommitted(this, &SDMXControlConsoleEditorFader::OnValueTextCommitted)
						.MinDesiredWidth(20.f)
						.Text(this, &SDMXControlConsoleEditorFader::GetValueAsText)
						.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorFader::GetExpandedViewModeVisibility))
					]

					// Fader Min Value
					+ SVerticalBox::Slot()
					.HAlign(HAlign_Center)
					.Padding(6.f, 4.f)
					.AutoHeight()
					[
						SNew(SEditableTextBox)
						.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
						.IsReadOnly(this, &SDMXControlConsoleEditorFader::IsReadOnly)
						.Justification(ETextJustify::Center)
						.MinDesiredWidth(20.f)
						.OnTextCommitted(this, &SDMXControlConsoleEditorFader::OnMinValueTextCommitted)
						.Text(this, &SDMXControlConsoleEditorFader::GetMinValueAsText)
						.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorFader::GetExpandedViewModeVisibility))
					]

					// Mute CheckBox section
					+ SVerticalBox::Slot()
					.HAlign(HAlign_Center)
					.Padding(6.f, 10.f)
					.AutoHeight()
					[
						SNew(SCheckBox)
						.IsChecked(this, &SDMXControlConsoleEditorFader::IsMuteChecked)
						.OnCheckStateChanged(this, &SDMXControlConsoleEditorFader::OnMuteToggleChanged)
					]
				]
			]
		]
	];
}

void SDMXControlConsoleEditorFader::SetValueByPercentage(float InNewPercentage)
{
	if (!ensureMsgf(Fader.IsValid(), TEXT("Invalid fader, cannot set fader value correctly.")))
	{
		return;
	}

	const float Range = Fader->GetMaxValue() - Fader->GetMinValue();
	FaderSpinBox->SetValue(static_cast<uint32>(Range * InNewPercentage / 100.f) + Fader->GetMinValue());
}

FReply SDMXControlConsoleEditorFader::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!ensureMsgf(FaderSpinBox.IsValid(), TEXT("Invalid fader widget, cannot handle selection correctly.")))
	{
		return FReply::Unhandled();
	}

	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		if (!Fader.IsValid())
		{
			return FReply::Unhandled();
		}

		UDMXControlConsoleEditorModel* EditorConsoleModel = GetMutableDefault<UDMXControlConsoleEditorModel>();
		const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorConsoleModel->GetSelectionHandler();

		if (MouseEvent.IsLeftShiftDown())
		{
			SelectionHandler->Multiselect(Fader.Get());
		}
		else if (MouseEvent.IsControlDown())
		{
			if (IsSelected())
			{
				SelectionHandler->RemoveFromSelection(Fader.Get());
			}
			else
			{
				SelectionHandler->AddToSelection(Fader.Get());
			}
		}
		else
		{
			if (!IsSelected() || !FaderSpinBox->IsHovered())
			{
				constexpr bool bNotifySelectionChange = false;
				SelectionHandler->ClearSelection(bNotifySelectionChange);
				SelectionHandler->AddToSelection(Fader.Get());
			}
		}

		// Let the spin box capture the mouse to change values interactively, prevent throttling
		return FReply::Handled()
			.CaptureMouse(FaderSpinBox.ToSharedRef())
			.UseHighPrecisionMouseMovement(FaderSpinBox.ToSharedRef())
			.SetUserFocus(FaderSpinBox.ToSharedRef(), EFocusCause::Mouse)
			.PreventThrottling();
	}

	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton && Fader.IsValid())
	{
		const FWidgetPath WidgetPath = MouseEvent.GetEventPath() != nullptr ? *MouseEvent.GetEventPath() : FWidgetPath();
		FSlateApplication::Get().PushMenu(AsShared(), WidgetPath, GenerateFaderOptionsMenuWidget(),
			FSlateApplication::Get().GetCursorPos(),
			FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

TSharedRef<SWidget> SDMXControlConsoleEditorFader::GenerateLockButtonWidget()
{
	TSharedRef<SWidget> MuteButtonWidget =
		SNew(SBox)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		.MinDesiredWidth(12.f)
		.MinDesiredHeight(12.f)
		.Padding(2.f)
		[
			SAssignNew(LockButton, SButton)
			.ButtonStyle(FAppStyle::Get(), "NoBorder")
			.ClickMethod(EButtonClickMethod::MouseDown)
			.OnClicked(this, &SDMXControlConsoleEditorFader::OnLockClicked)
			.ToolTipText(LOCTEXT("FaderLockButtonToolTipText", "Locked"))
			.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorFader::GetLockButtonVisibility))
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("Icons.Lock"))
				.ColorAndOpacity(this, &SDMXControlConsoleEditorFader::GetLockButtonColor)
			]
		];

	return MuteButtonWidget;
}

TSharedRef<SWidget> SDMXControlConsoleEditorFader::GenerateFaderOptionsMenuWidget()
{
	constexpr bool bShouldCloseWindowAfterClosing = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterClosing, nullptr);

	MenuBuilder.BeginSection("Options", LOCTEXT("FaderOptionsCategory", "Options"));
	{
		MenuBuilder.AddMenuEntry
		(
			LOCTEXT("MuteLabel", "Mute"), 
			FText::GetEmpty(),
			FSlateIcon(FDMXControlConsoleEditorStyle::Get().GetStyleSetName(), "DMXControlConsole.Fader.Mute"), 
			FUIAction
			(
				FExecuteAction::CreateSP(this, &SDMXControlConsoleEditorFader::OnMuteFader, true),
				FCanExecuteAction::CreateLambda([this]() { return Fader.IsValid() ? !Fader->IsMuted() : false; }),
				FIsActionChecked(),
				FIsActionButtonVisible::CreateLambda([this]() { return Fader.IsValid() ? !Fader->IsMuted() : false; })
			),
			NAME_None, 
			EUserInterfaceActionType::Button
		);

		MenuBuilder.AddMenuEntry
		(
			LOCTEXT("UnmuteLabel", "Unmute"),
			FText::GetEmpty(),
			FSlateIcon(FDMXControlConsoleEditorStyle::Get().GetStyleSetName(), "DMXControlConsole.Fader.Unmute"),
			FUIAction
			(
				FExecuteAction::CreateSP(this, &SDMXControlConsoleEditorFader::OnMuteFader, false),
				FCanExecuteAction::CreateLambda([this]() { return Fader.IsValid() ? Fader->IsMuted() : false; }),
				FIsActionChecked(),
				FIsActionButtonVisible::CreateLambda([this]() { return Fader.IsValid() ? Fader->IsMuted() : false; })
			),
			NAME_None,
			EUserInterfaceActionType::Button
		);

		MenuBuilder.AddMenuEntry
		(
			LOCTEXT("RemoveLabel", "Remove"),
			FText::GetEmpty(),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Delete"),
			FUIAction
			(
				FExecuteAction::CreateSP(this, &SDMXControlConsoleEditorFader::OnRemoveFader),
				FCanExecuteAction::CreateSP(this, &SDMXControlConsoleEditorFader::IsRawFader)
			), 
			NAME_None,
			EUserInterfaceActionType::Button
		);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("Controls", LOCTEXT("FaderControlsCategory", "Controls"));
	{
		MenuBuilder.AddMenuEntry
		(
			LOCTEXT("ResetToDefaultLabel", "Reset to Default"),
			FText::GetEmpty(),
			FSlateIcon(FDMXControlConsoleEditorStyle::Get().GetStyleSetName(), "DMXControlConsole.ResetToDefault"),
			FUIAction
			(
				FExecuteAction::CreateSP(this, &SDMXControlConsoleEditorFader::OnResetFader)
			),
			NAME_None,
			EUserInterfaceActionType::Button
		);

		MenuBuilder.AddMenuEntry
		(
			LOCTEXT("LockLabel", "Lock"),
			FText::GetEmpty(),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Lock"),
			FUIAction
			(
				FExecuteAction::CreateSP(this, &SDMXControlConsoleEditorFader::OnLockFader, true),
				FCanExecuteAction::CreateLambda([this]() { return Fader.IsValid() ? !Fader->IsLocked() : false; }),
				FIsActionChecked(),
				FIsActionButtonVisible::CreateLambda([this]() { return Fader.IsValid() ? !Fader->IsLocked() : false; })
			),
			NAME_None,
			EUserInterfaceActionType::Button
		);

		MenuBuilder.AddMenuEntry
		(
			LOCTEXT("UnlockLabel", "Unlock"),
			FText::GetEmpty(),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Unlock"),
			FUIAction
			(
				FExecuteAction::CreateSP(this, &SDMXControlConsoleEditorFader::OnLockFader, false),
				FCanExecuteAction::CreateLambda([this]() { return Fader.IsValid() ? Fader->IsLocked() : false; }),
				FIsActionChecked(),
				FIsActionButtonVisible::CreateLambda([this]() { return Fader.IsValid() ? Fader->IsLocked() : false; })
			),
			NAME_None,
			EUserInterfaceActionType::Button
		);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

bool SDMXControlConsoleEditorFader::IsSelected() const
{
	if (Fader.IsValid())
	{
		UDMXControlConsoleEditorModel* EditorConsoleModel = GetMutableDefault<UDMXControlConsoleEditorModel>();
		const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorConsoleModel->GetSelectionHandler();
		return SelectionHandler->IsSelected(Fader.Get());
	}

	return false;
}

bool SDMXControlConsoleEditorFader::IsReadOnly() const
{ 
	return Fader.IsValid() ? Fader->IsLocked() : true; 
}

bool SDMXControlConsoleEditorFader::IsRawFader() const
{
	return IsValid(Cast<UDMXControlConsoleRawFader>(Fader));
}

FString SDMXControlConsoleEditorFader::GetFaderName() const
{
	return Fader.IsValid() ? Fader->GetFaderName() : FString();
}

FText SDMXControlConsoleEditorFader::GetFaderNameText() const
{
	return Fader.IsValid() ? FText::FromString(Fader->GetFaderName()) : FText::GetEmpty();
}

uint32 SDMXControlConsoleEditorFader::GetValue() const
{
	return Fader.IsValid() ? Fader->GetValue() : 0;
}

FText SDMXControlConsoleEditorFader::GetValueAsText() const
{
	if (!Fader.IsValid())
	{
		return FText::GetEmpty();
	}

	const uint32 Value = Fader->GetValue();
	return FText::FromString(FString::FromInt(Value));
}

void SDMXControlConsoleEditorFader::OnValueTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo)
{
	if (Fader.IsValid() && !NewText.IsEmpty())
	{
		const int32 NewValue = FCString::Atoi(*NewText.ToString());
		if (NewValue >= 0)
		{
			PreCommittedValue = Fader->GetValue();
			OnValueCommitted(NewValue, ETextCommit::Default);
		}
	}
}

TOptional<uint32> SDMXControlConsoleEditorFader::GetMinValue() const
{
	return Fader.IsValid() ? Fader->GetMinValue() : 0;
}

FText SDMXControlConsoleEditorFader::GetMinValueAsText() const
{
	if (!Fader.IsValid())
	{
		return FText::GetEmpty();
	}

	const uint32 MinValue = Fader->GetMinValue();
	return FText::FromString(FString::FromInt(MinValue));
}

void SDMXControlConsoleEditorFader::OnMinValueTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo)
{
	if (Fader.IsValid() && !NewText.IsEmpty())
	{
		const int32 NewValue = FCString::Atoi(*NewText.ToString());

		const FScopedTransaction FaderMinValueEditedTransaction(LOCTEXT("FaderMinValueEditedTransaction", "Edit Min Value"));
		Fader->PreEditChange(UDMXControlConsoleRawFader::StaticClass()->FindPropertyByName(UDMXControlConsoleRawFader::GetMinValuePropertyName()));
		Fader->SetMinValue(NewValue);
		Fader->PostEditChange();
	}
}

TOptional<uint32> SDMXControlConsoleEditorFader::GetMaxValue() const
{
	return Fader.IsValid() ? Fader->GetMaxValue() : 0;
}

FText SDMXControlConsoleEditorFader::GetMaxValueAsText() const
{
	if (!Fader.IsValid())
	{
		return FText::GetEmpty();
	}

	const uint32 MaxValue = Fader->GetMaxValue();
	return FText::FromString(FString::FromInt(MaxValue));
}

void SDMXControlConsoleEditorFader::OnMaxValueTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo)
{
	if (Fader.IsValid() && !NewText.IsEmpty())
	{
		const int32 NewValue = FCString::Atoi(*NewText.ToString());

		const FScopedTransaction FaderMaxValueEditedTransaction(LOCTEXT("FaderMaxValueEditedTransaction", "Edit Max Value"));
		Fader->PreEditChange(UDMXControlConsoleRawFader::StaticClass()->FindPropertyByName(UDMXControlConsoleRawFader::GetMaxValuePropertyName()));
		Fader->SetMaxValue(NewValue);
		Fader->PostEditChange();
	}
}

void SDMXControlConsoleEditorFader::HandleValueChanged(uint32 NewValue)
{
	if (!ensureMsgf(Fader.IsValid(), TEXT("Invalid fader, cannot set fader value correctly.")))
	{
		return;
	}

	if (!ensureMsgf(FaderSpinBox.IsValid(), TEXT("Invalid fader widget, cannot set fader value correctly.")))
	{
		return;
	}

	UDMXControlConsoleEditorModel* EditorConsoleModel = GetMutableDefault<UDMXControlConsoleEditorModel>();
	const EDMXControlConsoleEditorControlMode InputMode = EditorConsoleModel->GetControlMode();
	const float Range = Fader->GetMaxValue() - Fader->GetMinValue();
	const float FaderSpinBoxValue = FaderSpinBox->GetValue();

	const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorConsoleModel->GetSelectionHandler();
	const TArray<TWeakObjectPtr<UObject>> SelectedFadersObjects = SelectionHandler->GetSelectedFaders();
	for (const TWeakObjectPtr<UObject> SelectFaderObject : SelectedFadersObjects)
	{
		UDMXControlConsoleFaderBase* SelectedFader = Cast<UDMXControlConsoleFaderBase>(SelectFaderObject);
		if (!SelectedFader || 
			!SelectedFader->IsMatchingFilter() ||
			SelectedFader->IsLocked())
		{
			continue;
		}

		const float SelectedFaderRange = SelectedFader->GetMaxValue() - SelectedFader->GetMinValue();
		float SelectedFaderPercentage = 0.f;
		uint32 SelectedFaderValue = 0;

		switch (InputMode)
		{
		case EDMXControlConsoleEditorControlMode::Relative:
		{
			// Relative percentage
			SelectedFaderPercentage = (static_cast<float>(NewValue) - FaderSpinBoxValue) / Range;
			const float SelectedFaderClampedValue = FMath::Clamp(static_cast<float>(SelectedFader->GetValue()) + SelectedFaderRange * SelectedFaderPercentage, 0.f, MAX_uint32);
			SelectedFaderValue = static_cast<uint32>(SelectedFaderClampedValue);
			break;
		} 
		case EDMXControlConsoleEditorControlMode::Absolute:
		{
			// Absolute percentage
			SelectedFaderPercentage = (NewValue - Fader->GetMinValue()) / Range;
			SelectedFaderValue = SelectedFader->GetMinValue() + static_cast<uint32>(SelectedFaderRange * SelectedFaderPercentage);
			break;
		}
		}
			
		SelectedFader->SetValue(SelectedFaderValue);
	}
}

void SDMXControlConsoleEditorFader::OnBeginValueChange()
{
	if (!ensureMsgf(Fader.IsValid(), TEXT("Invalid fader, cannot set fader value correctly.")))
	{
		return;
	}

	PreCommittedValue = Fader->GetValue();
}

void SDMXControlConsoleEditorFader::OnValueCommitted(uint32 NewValue, ETextCommit::Type CommitType)
{
	if (!ensureMsgf(Fader.IsValid(), TEXT("Invalid fader, cannot set fader value correctly.")))
	{
		return;
	}

	if (!ensureMsgf(FaderSpinBox.IsValid(), TEXT("Invalid fader widget, cannot set fader value correctly.")))
	{
		return;
	}

	const FScopedTransaction FaderValueCommittedTransaction(LOCTEXT("FaderValueCommittedTransaction", "Edit Fader Value"));

	const float Range = Fader->GetMaxValue() - Fader->GetMinValue();
	const float FaderSpinBoxValue = FaderSpinBox->GetValue();
	float PreCommittedPercentage = 0.f;
	float Percentage = 0.f;
	
	UDMXControlConsoleEditorModel* EditorConsoleModel = GetMutableDefault<UDMXControlConsoleEditorModel>();
	const EDMXControlConsoleEditorControlMode ControlMode = EditorConsoleModel->GetControlMode();
	switch (ControlMode)
	{
	case EDMXControlConsoleEditorControlMode::Relative:
	{
		// Relative percentages
		PreCommittedPercentage = (static_cast<float>(PreCommittedValue) - FaderSpinBoxValue) / Range;
		Percentage = (static_cast<float>(NewValue) - FaderSpinBoxValue) / Range;
		break;
	}
	case EDMXControlConsoleEditorControlMode::Absolute:
	{
		// Absolute percentages
		PreCommittedPercentage = (PreCommittedValue - Fader->GetMinValue()) / Range;
		Percentage = (NewValue - Fader->GetMinValue()) / Range;
		break;
	}
	}

	const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorConsoleModel->GetSelectionHandler();
	const TArray<TWeakObjectPtr<UObject>> SelectedFadersObjects = SelectionHandler->GetSelectedFaders();
	for (const TWeakObjectPtr<UObject> SelectFaderObject : SelectedFadersObjects)
	{
		UDMXControlConsoleFaderBase* SelectedFader = Cast<UDMXControlConsoleFaderBase>(SelectFaderObject);
		if (!SelectedFader ||
			!SelectedFader->IsMatchingFilter() ||
			SelectedFader->IsLocked())
		{
			continue;
		}

		const float SelectedFaderRange = SelectedFader->GetMaxValue() - SelectedFader->GetMinValue();
		uint32 SelectedFaderPreCommittedValue = 0;
		uint32 SelectedFaderValue = 0;

		switch (ControlMode)
		{
		case EDMXControlConsoleEditorControlMode::Relative:
		{
			const float SelectedFaderPreCommittedClampedValue = FMath::Clamp(static_cast<float>(SelectedFader->GetValue()) + SelectedFaderRange * PreCommittedPercentage, 0.f, MAX_uint32);
			SelectedFaderPreCommittedValue = static_cast<uint32>(SelectedFaderPreCommittedClampedValue);
			const float SelectedFaderClampedValue = FMath::Clamp(static_cast<float>(SelectedFader->GetValue()) + SelectedFaderRange * Percentage, 0.f, MAX_uint32);
			SelectedFaderValue = static_cast<uint32>(SelectedFaderClampedValue);
			break;
		}
		case EDMXControlConsoleEditorControlMode::Absolute:
		{
			SelectedFaderPreCommittedValue = SelectedFader->GetMinValue() + static_cast<uint32>(SelectedFaderRange * PreCommittedPercentage);
			SelectedFaderValue = SelectedFader->GetMinValue() + static_cast<uint32>(SelectedFaderRange * Percentage);
			break;
		}
		}

		// Reset to PreCommittedValue to handle transactions
		SelectedFader->SetValue(SelectedFaderPreCommittedValue);

		SelectedFader->PreEditChange(UDMXControlConsoleRawFader::StaticClass()->FindPropertyByName(UDMXControlConsoleRawFader::GetValuePropertyName()));
		SelectedFader->SetValue(SelectedFaderValue);
		SelectedFader->PostEditChange();
	}
}

void SDMXControlConsoleEditorFader::OnMuteFader(bool bMute) const
{
	if (Fader.IsValid())
	{
		const FScopedTransaction MuteFaderOptionTransaction(LOCTEXT("MuteFaderOptionTransaction", "Edit Fader mute state"));
		Fader->PreEditChange(UDMXControlConsoleFaderBase::StaticClass()->FindPropertyByName(UDMXControlConsoleFaderBase::GetIsMutedPropertyName()));
		Fader->SetMute(bMute);
		Fader->PostEditChange();
	}
}

void SDMXControlConsoleEditorFader::OnRemoveFader() const
{
	if (Fader.IsValid())
	{
		const FScopedTransaction RemoveFaderOptionTransaction(LOCTEXT("RemoveFaderOptionTransaction", "Fader removed"));
		Fader->PreEditChange(nullptr);
		Fader->Destroy();
		Fader->PostEditChange();
	}
}

void SDMXControlConsoleEditorFader::OnResetFader() const
{
	if (Fader.IsValid())
	{
		const FScopedTransaction ResetFaderOptionTransaction(LOCTEXT("ResetFaderOptionTransaction", "Fader reset to default"));
		Fader->PreEditChange(nullptr);
		Fader->ResetToDefault();
		Fader->PostEditChange();
	}
}

void SDMXControlConsoleEditorFader::OnLockFader(bool bLock) const
{
	if (Fader.IsValid())
	{
		const FScopedTransaction LockFaderOptionTransaction(LOCTEXT("LockFaderOptionTransaction", "Edit Fader lock state"));
		Fader->PreEditChange(UDMXControlConsoleFaderBase::StaticClass()->FindPropertyByName(UDMXControlConsoleFaderBase::GetIsLockedPropertyName()));
		Fader->SetLock(bLock);
		Fader->PostEditChange();
	}
}

FReply SDMXControlConsoleEditorFader::OnDeleteClicked()
{
	if (Fader.IsValid())
	{
		UDMXControlConsoleEditorModel* EditorConsoleModel = GetMutableDefault<UDMXControlConsoleEditorModel>();
		const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorConsoleModel->GetSelectionHandler();
		const TArray<TWeakObjectPtr<UObject>> SelectedFadersObjects = SelectionHandler->GetSelectedFaders();

		if (SelectedFadersObjects.IsEmpty() || !SelectedFadersObjects.Contains(Fader))
		{
			Fader->Destroy();
		}
		else
		{
			for (const TWeakObjectPtr<UObject> SelectFaderObject : SelectedFadersObjects)
			{
				UDMXControlConsoleFaderBase* SelectedFader = Cast<UDMXControlConsoleFaderBase>(SelectFaderObject);
				if (!SelectedFader || !SelectedFader->IsMatchingFilter())
				{
					continue;
				}

				SelectedFader->Destroy();

				return FReply::Handled();
			}
		}
	}

	return FReply::Unhandled();
}

FReply SDMXControlConsoleEditorFader::OnLockClicked()
{
	if (Fader.IsValid())
	{
		const FScopedTransaction FaderLockStateEditedtTransaction(LOCTEXT("FaderLockStateEditedtTransaction", "Edit Lock state"));
		Fader->PreEditChange(UDMXControlConsoleFaderBase::StaticClass()->FindPropertyByName(UDMXControlConsoleFaderBase::GetIsLockedPropertyName()));
		Fader->ToggleLock();
		Fader->PostEditChange();

		UDMXControlConsoleEditorModel* EditorConsoleModel = GetMutableDefault<UDMXControlConsoleEditorModel>();
		const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorConsoleModel->GetSelectionHandler();
		const TArray<TWeakObjectPtr<UObject>> SelectedFadersObjects = SelectionHandler->GetSelectedFaders();
		if (!SelectedFadersObjects.IsEmpty() && SelectedFadersObjects.Contains(Fader))
		{
			for (const TWeakObjectPtr<UObject> SelectFaderObject : SelectedFadersObjects)
			{
				UDMXControlConsoleFaderBase* SelectedFader = Cast<UDMXControlConsoleFaderBase>(SelectFaderObject);
				if (!SelectedFader || !SelectedFader->IsMatchingFilter())
				{
					continue;
				}

				SelectedFader->PreEditChange(UDMXControlConsoleFaderBase::StaticClass()->FindPropertyByName(UDMXControlConsoleFaderBase::GetIsLockedPropertyName()));
				SelectedFader->SetLock(Fader->IsLocked());
				SelectedFader->PostEditChange();
			}
		}

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void SDMXControlConsoleEditorFader::OnMuteToggleChanged(ECheckBoxState CheckState)
{
	if (Fader.IsValid())
	{
		const FScopedTransaction FaderMuteStateEditedtTransaction(LOCTEXT("FaderMuteStateEditedtTransaction", "Edit Mute state"));
		Fader->PreEditChange(UDMXControlConsoleFaderBase::StaticClass()->FindPropertyByName(UDMXControlConsoleFaderBase::GetIsMutedPropertyName()));
		Fader->ToggleMute();
		Fader->PostEditChange();

		UDMXControlConsoleEditorModel* EditorConsoleModel = GetMutableDefault<UDMXControlConsoleEditorModel>();
		const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorConsoleModel->GetSelectionHandler();
		const TArray<TWeakObjectPtr<UObject>> SelectedFadersObjects = SelectionHandler->GetSelectedFaders();
		if (!SelectedFadersObjects.IsEmpty() && SelectedFadersObjects.Contains(Fader))
		{
			for (const TWeakObjectPtr<UObject> SelectFaderObject : SelectedFadersObjects)
			{
				UDMXControlConsoleFaderBase* SelectedFader = Cast<UDMXControlConsoleFaderBase>(SelectFaderObject);
				if (!SelectedFader || !SelectedFader->IsMatchingFilter())
				{
					continue;
				}

				SelectedFader->PreEditChange(UDMXControlConsoleFaderBase::StaticClass()->FindPropertyByName(UDMXControlConsoleFaderBase::GetIsMutedPropertyName()));
				SelectedFader->SetMute(Fader->IsMuted());
				SelectedFader->PostEditChange();
			}
		}
	}
}

ECheckBoxState SDMXControlConsoleEditorFader::IsMuteChecked() const
{
	if (Fader.IsValid())
	{
		if (Fader->IsMuted()) 
		{
			return ECheckBoxState::Unchecked;
		}

		const UDMXControlConsoleFaderGroup& OwnerFaderGroup = Fader->GetOwnerFaderGroupChecked();
		return OwnerFaderGroup.IsMuted() ? ECheckBoxState::Undetermined : ECheckBoxState::Checked;
	}

	return ECheckBoxState::Undetermined;
}

bool SDMXControlConsoleEditorFader::IsFaderSpinBoxActive() const
{
	return Fader.IsValid() && !Fader->IsMuted();
}

FOptionalSize SDMXControlConsoleEditorFader::GetFaderHeightByViewMode() const
{
	using namespace UE::DMXControlConsoleEditor::DMXControlConsoleEditorFader::Private;

	const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
	const EDMXControlConsoleEditorViewMode ViewMode = EditorConsoleModel->GetFadersViewMode();
	return ViewMode == EDMXControlConsoleEditorViewMode::Collapsed ? CollapsedViewModeHeight : ExpandedViewModeHeight;
}

FText SDMXControlConsoleEditorFader::GetToolTipText() const
{
	if (Fader.IsValid())
	{
		const FString& FaderName = Fader->GetFaderName();
		const FString& FaderValeAsString = FString::FromInt(Fader->GetValue());
		const FString& FaderMaxValueAsString = FString::FromInt(Fader->GetMaxValue());
		const FString& ToolTipString = FString::Format(TEXT("{0}\n{1}/{2}"), { FaderName, FaderValeAsString, FaderMaxValueAsString });
		return FText::FromString(ToolTipString);
	}

	return FText::GetEmpty();
}

FSlateColor SDMXControlConsoleEditorFader::GetLockButtonColor() const
{
	if (LockButton.IsValid())
	{
		return LockButton->IsHovered() ? FStyleColors::AccentWhite : FLinearColor(1.f, 1.f, 1.f, .4f);
	}

	return FLinearColor::White;
}

EVisibility SDMXControlConsoleEditorFader::GetExpandedViewModeVisibility() const
{
	const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
	const EDMXControlConsoleEditorViewMode ViewMode = EditorConsoleModel->GetFadersViewMode();

	const bool bIsVisible =
		Fader.IsValid() &&
		ViewMode == EDMXControlConsoleEditorViewMode::Expanded;

	return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SDMXControlConsoleEditorFader::GetLockButtonVisibility() const
{
	const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
	const EDMXControlConsoleEditorViewMode ViewMode = EditorConsoleModel->GetFadersViewMode();

	const bool bIsVisible = Fader.IsValid() && Fader->IsLocked();
	return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
}

const FSlateBrush* SDMXControlConsoleEditorFader::GetBorderImage() const
{
	if (!Fader.IsValid())
	{
		return nullptr;
	}

	if (IsHovered())
	{
		if (IsSelected())
		{
			return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.Rounded.Fader_Highlighted");;
		}
		else
		{
			return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.Rounded.Fader_Hovered");
		}
	}
	else
	{
		if (IsSelected())
		{
			return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.Rounded.Fader_Selected");;
		}
		else
		{
			return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.Rounded.Fader");
		}
	}
}

const FSlateBrush* SDMXControlConsoleEditorFader::GetSpinBoxBorderImage() const
{
	if (!Fader.IsValid())
	{
		return nullptr;
	}

	if (Fader->IsMuted() || Fader->IsLocked())
	{
		return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.Rounded.DefaultBrush");
	}

	if (IsHovered())
	{
		return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.Rounded.SpinBoxBorder_Hovered");
	}
	else
	{
		if (IsSelected())
		{
			return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.Rounded.SpinBoxBorder_Hovered");
		}
		else
		{
			return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.Rounded.SpinBoxBorder");
		}
	}
}

#undef LOCTEXT_NAMESPACE
