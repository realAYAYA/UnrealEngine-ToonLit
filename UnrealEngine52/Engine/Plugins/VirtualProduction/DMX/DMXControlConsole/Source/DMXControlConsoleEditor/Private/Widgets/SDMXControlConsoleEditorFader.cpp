// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleEditorFader.h"

#include "DMXControlConsoleEditorManager.h"
#include "DMXControlConsoleEditorSelection.h"
#include "DMXControlConsoleFaderGroup.h"
#include "DMXControlConsoleFixturePatchFunctionFader.h"
#include "DMXControlConsoleFixturePatchCellAttributeFader.h"
#include "DMXControlConsoleRawFader.h"
#include "DMXEditorUtils.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Style/DMXControlConsoleEditorStyle.h"
#include "Widgets/SDMXControlConsoleEditorSpinBoxVertical.h"

#include "ScopedTransaction.h"
#include "Misc/Optional.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleEditorFader"

void SDMXControlConsoleEditorFader::Construct(const FArguments& InArgs, const TObjectPtr<UDMXControlConsoleFaderBase>& InFader)
{
	Fader = InFader;

	ChildSlot
	[
		SNew(SBox)
		.WidthOverride(80.f)
		.HeightOverride(300.f)
		.Padding(InArgs._Padding)
		[
			SNew(SBorder)
			.BorderBackgroundColor(FLinearColor::White)
			 [
				SNew(SBorder)
				.BorderImage(this, &SDMXControlConsoleEditorFader::GetBorderImage)
				[
					SNew(SVerticalBox)

					// Top section
					+ SVerticalBox::Slot()
					.HAlign(HAlign_Center)
					.Padding(1.f, 4.f, 1.f, 0.f)
					.AutoHeight()
					[			
						SNew(SHorizontalBox)
		
						// Fader Name
						+ SHorizontalBox::Slot()
						.MaxWidth(40.f)
						.AutoWidth()
						[
							SNew(SBorder)
							.BorderBackgroundColor(FLinearColor::White)
							[
								SNew(STextBlock)
								.Text(this, &SDMXControlConsoleEditorFader::GetFaderNameText)
								.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
								.ColorAndOpacity(FLinearColor::White)
							]
						]

						// Delete Button
						+ SHorizontalBox::Slot()			
						.Padding(0.8f, 0.f)
						.AutoWidth()
						[
							SNew(SButton)
							.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
							.OnClicked(this, &SDMXControlConsoleEditorFader::OnDeleteClicked)
							.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorFader::GetDeleteButtonVisibility))
							[
								SNew(STextBlock)
								.Text(FText::FromString(TEXT("x")))
								.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
								.ColorAndOpacity(FLinearColor::White)
							]
						]
					]

					// Middle section
					+ SVerticalBox::Slot()
					.HAlign(HAlign_Center)
					.Padding(0.f, 8.f, 0.f, 0.f)
					.AutoHeight()
					[
						SNew(SBox)
						.WidthOverride(30.f)
						[
							SNew(SVerticalBox)
					
							// Max Value
							+ SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(SBorder)
								.BorderBackgroundColor(FLinearColor::White)
								.Padding(1.0f)
								[
									SNew(STextBlock)
									.Text(this, &SDMXControlConsoleEditorFader::GetMaxValueAsText)
									.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
									.Justification(ETextJustify::Center)
								]
							]
								
							// Fader Control
							+ SVerticalBox::Slot()
							.Padding(4.f, 1.0f)
							.AutoHeight()
							[
								SNew(SBorder)
								.BorderBackgroundColor(FLinearColor::White)
								[
									SAssignNew(FaderSpinBox, SDMXControlConsoleEditorSpinBoxVertical<uint32>)
									.Value(this, &SDMXControlConsoleEditorFader::GetValue)
									.MinValue(this, &SDMXControlConsoleEditorFader::GetMinValue)
									.MaxValue(this, &SDMXControlConsoleEditorFader::GetMaxValue)
									.MinSliderValue(this, &SDMXControlConsoleEditorFader::GetMinValue)
									.MaxSliderValue(this, &SDMXControlConsoleEditorFader::GetMaxValue)
									.OnValueChanged(this, &SDMXControlConsoleEditorFader::HandleValueChanged)
									.IsEnabled(this, &SDMXControlConsoleEditorFader::GetFaderSpinBoxEnabled)
									.Style(FDMXControlConsoleEditorStyle::Get(), "DMXControlConsole.Fader")
									.MinDesiredWidth(45.0f)
								]
							]

							// Fader Min Value
							+ SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(SBorder)
								.BorderBackgroundColor(FLinearColor::White)
								.Padding(1.0f)
								[
									SNew(STextBlock)
									.Text(this, &SDMXControlConsoleEditorFader::GetMinValueAsText)
									.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
									.Justification(ETextJustify::Center)
								]
							]
						]
					]

					// Bottom section
					+ SVerticalBox::Slot()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						GenerateMuteButtonWidget()
					]
				]
			]
		]
	];

	const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = FDMXControlConsoleEditorManager::Get().GetSelectionHandler();
	SelectionHandler->GetOnSelectionChanged().AddSP(this, &SDMXControlConsoleEditorFader::OnSelectionChanged);
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

void SDMXControlConsoleEditorFader::ApplyGlobalFilter(const FString& InSearchString)
{	
	if (!Fader.IsValid())
	{
		return;
	}

	if (InSearchString.IsEmpty())
	{
		SetVisibility(EVisibility::Visible);
		return;
	}

	// Filter and return in order of precendence

	// Attribute Name
	const TArray<FString> AttributeNames = FDMXEditorUtils::ParseAttributeNames(InSearchString);
	if (!AttributeNames.IsEmpty())
	{
		FString AttributeNameOfFader;
		if (UDMXControlConsoleFixturePatchFunctionFader* FixturePatchFunctionFader = Cast<UDMXControlConsoleFixturePatchFunctionFader>(Fader.Get()))
		{
			AttributeNameOfFader = FixturePatchFunctionFader->GetAttributeName().Name.ToString();
		}
		else if (UDMXControlConsoleFixturePatchCellAttributeFader* FixturePatchCellAttribute = Cast<UDMXControlConsoleFixturePatchCellAttributeFader>(Fader.Get()))
		{
			AttributeNameOfFader = FixturePatchCellAttribute->GetAttributeName().Name.ToString();
		}
		else if (UDMXControlConsoleRawFader* RawFader = Cast<UDMXControlConsoleRawFader>(Fader.Get()))
		{
			AttributeNameOfFader = RawFader->GetFaderName();
		}

		if (!AttributeNameOfFader.IsEmpty())
		{
			for (const FString& AttributeName : AttributeNames)
			{				
				if (AttributeNameOfFader.Contains(AttributeName))
				{
					SetVisibility(EVisibility::Visible);
					return;
				}
			}
		}
	}

	// Universe
	const TArray<int32> Universes = FDMXEditorUtils::ParseUniverses(InSearchString);
	if (!Universes.IsEmpty() && Universes.Contains(Fader->GetUniverseID()))
	{
		SetVisibility(EVisibility::Visible);
		return;
	}

	// Address
	int32 Address;
	if (FDMXEditorUtils::ParseAddress(InSearchString, Address))
	{
		if (!Universes.IsEmpty() && Universes.Contains(Fader->GetUniverseID()) && Address == Fader->GetStartingAddress())
		{
			SetVisibility(EVisibility::Visible);
			return;
		}
	}

	// Fixture ID
	bool bFoundFixtureIDs = false;
	const TArray<int32> FixtureIDs = FDMXEditorUtils::ParseFixtureIDs(InSearchString);
	for (int32 FixtureID : FixtureIDs)
	{
		const UDMXEntityFixturePatch* FixturePatch = Fader->GetOwnerFaderGroupChecked().GetFixturePatch();
		int32 FixtureIDOfPatch;
		if (FixturePatch &&
			FixturePatch->FindFixtureID(FixtureIDOfPatch) &&
			FixtureIDOfPatch == FixtureID)
		{
			SetVisibility(EVisibility::Visible);
			bFoundFixtureIDs = true;
		}
	}
	if (bFoundFixtureIDs)
	{
		return;
	}

	SetVisibility(EVisibility::Collapsed);

	// If not visible, remove from selection
	if (IsSelected())
	{
		const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = FDMXControlConsoleEditorManager::Get().GetSelectionHandler();
		SelectionHandler->RemoveFromSelection(Fader.Get());
	}
}

FReply SDMXControlConsoleEditorFader::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Delete)
	{
		if (!Fader.IsValid())
		{
			return FReply::Handled();
		}

		UDMXControlConsoleFaderGroup& FaderGroup = Fader->GetOwnerFaderGroupChecked();
		if (FaderGroup.HasFixturePatch())
		{
			return FReply::Handled();
		}

		const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = FDMXControlConsoleEditorManager::Get().GetSelectionHandler();
		const TArray<TWeakObjectPtr<UObject>> SelectedFadersObjects = SelectionHandler->GetSelectedFaders();

		// If there's only one fader to delete, replace it in selection
		if (SelectedFadersObjects.Num() == 1)
		{
			UDMXControlConsoleFaderBase* SelectedFader = Cast<UDMXControlConsoleFaderBase>(SelectedFadersObjects[0]);
			SelectionHandler->ReplaceInSelection(SelectedFader);
		}

		const FScopedTransaction DeleteSelectedFaderTransaction(LOCTEXT("DeleteSelectedFaderTransaction", "Delete selected Faders"));

		// Delete all selected faders
		for (TWeakObjectPtr<UObject> SelectedFaderObject : SelectedFadersObjects)
		{
			UDMXControlConsoleFaderBase* SelectedFader = Cast<UDMXControlConsoleFaderBase>(SelectedFaderObject);
			if (!SelectedFader)
			{
				continue;
			}

			UDMXControlConsoleFaderGroup& SelectedFaderGroup = SelectedFader->GetOwnerFaderGroupChecked();
			SelectedFaderGroup.Modify();

			SelectionHandler->RemoveFromSelection(SelectedFader);
			SelectedFader->Destroy();
		}

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FReply SDMXControlConsoleEditorFader::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		if (!Fader.IsValid())
		{
			return FReply::Unhandled();
		}

		const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = FDMXControlConsoleEditorManager::Get().GetSelectionHandler();

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
			SelectionHandler->ClearSelection();
			SelectionHandler->AddToSelection(Fader.Get());
		}

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

TSharedRef<SWidget> SDMXControlConsoleEditorFader::GenerateMuteButtonWidget()
{
	TSharedRef<SWidget> MuteButtonWidget =
		SNew(SVerticalBox)

		+SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		[
			SNew(SBorder)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(16)
				.HeightOverride(16.f)
				[
					SNew(SButton)
					.ButtonColorAndOpacity(this, &SDMXControlConsoleEditorFader::GetMuteButtonColor)
					.OnClicked(this, &SDMXControlConsoleEditorFader::OnMuteClicked)
				]
			]
		]
	
		+SVerticalBox::Slot()
		.HAlign(HAlign_Center)
		.Padding(0.f, 4.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("MuteButton", "On/Off"))
			.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		];

	return MuteButtonWidget;
}

bool SDMXControlConsoleEditorFader::IsSelected() const
{
	if (!Fader.IsValid())
	{
		return false;
	}

	const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = FDMXControlConsoleEditorManager::Get().GetSelectionHandler();
	return SelectionHandler->IsSelected(Fader.Get());
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

void SDMXControlConsoleEditorFader::HandleValueChanged(uint32 NewValue)
{
	if (!ensureMsgf(Fader.IsValid(), TEXT("Invalid fader, cannot set fader value correctly.")))
	{
		return;
	}

	const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = FDMXControlConsoleEditorManager::Get().GetSelectionHandler();
	const TArray<TWeakObjectPtr<UObject>> SelectedFadersObjects = SelectionHandler->GetSelectedFaders();

	if (SelectedFadersObjects.IsEmpty() || !SelectedFadersObjects.Contains(Fader))
	{
		Fader->SetValue(NewValue);
	}
	else
	{ 
		const float Range = Fader->GetMaxValue() - Fader->GetMinValue();
		const float Percentage = (NewValue - Fader->GetMinValue()) / Range;

		for (const TWeakObjectPtr<UObject> SelectFaderObject : SelectedFadersObjects)
		{
			UDMXControlConsoleFaderBase* SelectedFader = Cast<UDMXControlConsoleFaderBase>(SelectFaderObject);
			if (!SelectedFader || SelectedFader->IsMuted())
			{
				continue;
			}

			const float SelectedFaderRange = SelectedFader->GetMaxValue() - SelectedFader->GetMinValue();
			const uint32 Value = (uint32)(SelectedFaderRange * Percentage);
			SelectedFader->SetValue(Value);
		}
	}
}

void SDMXControlConsoleEditorFader::OnSelectionChanged()
{
	const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = FDMXControlConsoleEditorManager::Get().GetSelectionHandler();
	
	// Remove from selection if not visible. Avoids selecting this when filtering
	const bool bMultiSelecting = SelectionHandler->GetSelectedFaders().Num() + SelectionHandler->GetSelectedFaderGroups().Num() > 1;
	if (bMultiSelecting && GetVisibility() == EVisibility::Collapsed)
	{
		SelectionHandler->RemoveFromSelection(Fader.Get());
		return;
	}

	// Set keyboard focus on the Fader, if selected
	if (IsSelected())
	{
		FSlateApplication::Get().SetKeyboardFocus(AsShared());
	}
}

FReply SDMXControlConsoleEditorFader::OnDeleteClicked()
{
	if (Fader.IsValid())
	{
		const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = FDMXControlConsoleEditorManager::Get().GetSelectionHandler();
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
				if (!SelectedFader)
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

FReply SDMXControlConsoleEditorFader::OnMuteClicked()
{
	if (Fader.IsValid())
	{
		Fader->ToggleMute();

		const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = FDMXControlConsoleEditorManager::Get().GetSelectionHandler();
		const TArray<TWeakObjectPtr<UObject>> SelectedFadersObjects = SelectionHandler->GetSelectedFaders();
		if (!SelectedFadersObjects.IsEmpty() && SelectedFadersObjects.Contains(Fader))
		{
			for (const TWeakObjectPtr<UObject> SelectFaderObject : SelectedFadersObjects)
			{
				UDMXControlConsoleFaderBase* SelectedFader = Cast<UDMXControlConsoleFaderBase>(SelectFaderObject);
				if (!SelectedFader)
				{
					continue;
				}

				SelectedFader->SetMute(Fader->IsMuted());
			}
		}

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FSlateColor SDMXControlConsoleEditorFader::GetMuteButtonColor() const
{
	if (Fader.IsValid())
	{
		return Fader->IsMuted() ? FLinearColor(0.1f, 0.1f, 0.1f) : FLinearColor(0.8f, 0.f, 0.f);
	}

	return FLinearColor::Black;
}

bool SDMXControlConsoleEditorFader::GetFaderSpinBoxEnabled() const
{
	if (Fader.IsValid())
	{
		return !Fader->IsMuted();
	}

	return false;
}

EVisibility SDMXControlConsoleEditorFader::GetDeleteButtonVisibility() const
{
	if (!Fader.IsValid())
	{
		return EVisibility::Collapsed;
	}

	const UDMXControlConsoleRawFader* RawFader = Cast<UDMXControlConsoleRawFader>(Fader);
	return RawFader ? EVisibility::Visible : EVisibility::Collapsed;
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
			return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.FaderGroup_Highlighted");;
		}
		else
		{
			return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.FaderGroup_Hovered");;
		}
	}
	else
	{
		if (IsSelected())
		{
			return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.FaderGroup_Selected");;
		}
		else
		{
			return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.BlackBrush");
		}
	}
}

#undef LOCTEXT_NAMESPACE
