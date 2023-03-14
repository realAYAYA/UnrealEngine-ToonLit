// Copyright Epic Games, Inc. All Rights Reserved.

#include "SConsoleVariablesEditorListValueInput.h"

#include "ConsoleVariablesEditorListRow.h"
#include "ConsoleVariablesEditorModule.h"
#include "ConsoleVariablesEditorProjectSettings.h"

#include "Widgets/Input/SNumericEntryBox.h"

#define LOCTEXT_NAMESPACE "ConsoleVariablesEditor"

SConsoleVariablesEditorListValueInput::~SConsoleVariablesEditorListValueInput()
{
	Item.Reset();
}

TSharedRef<SConsoleVariablesEditorListValueInput> SConsoleVariablesEditorListValueInput::GetInputWidget(
	const TWeakPtr<FConsoleVariablesEditorListRow> InRow)
{
	const FConsoleVariablesEditorListRowPtr PinnedItem = InRow.Pin();

	check(PinnedItem);
	
	if (PinnedItem->GetCommandInfo().IsValid())
	{
		const TSharedPtr<FConsoleVariablesEditorCommandInfo> PinnedInfo = PinnedItem->GetCommandInfo().Pin();

		if (const TObjectPtr<IConsoleVariable> Variable = PinnedInfo->GetConsoleVariablePtr())
		{
			if (Variable->IsVariableFloat())
			{
				return SNew(SConsoleVariablesEditorListValueInput_Float, InRow);
			}

			if (Variable->IsVariableBool())
			{
				return SNew(SConsoleVariablesEditorListValueInput_Bool, InRow);
			}

			if (Variable->IsVariableInt())
			{
				return SNew(SConsoleVariablesEditorListValueInput_Int, InRow);
			}

			if (Variable->IsVariableString())
			{
				return SNew(SConsoleVariablesEditorListValueInput_String, InRow);
			}

			// Showflags are not considered to be any of these types, but they should be ints with a min/max of 0/2
			if (PinnedInfo->Command.Contains("showflag", ESearchCase::IgnoreCase))
			{
				return SNew(SConsoleVariablesEditorListValueInput_Int, InRow, true);
			}
		}

		// For Commands
		const FString CachedValue = PinnedItem->GetCachedValue();
		return SNew(SConsoleVariablesEditorListValueInput_Command, InRow,
			CachedValue.IsEmpty() ? PinnedItem->GetPresetValue() : CachedValue);
	}

	// fallback
	return SNew(SConsoleVariablesEditorListValueInput_String, InRow);
}

bool SConsoleVariablesEditorListValueInput::IsRowChecked() const
{
	return Item.Pin()->IsRowChecked();
}

void SConsoleVariablesEditorListValueInput_Float::Construct(const FArguments& InArgs,
                                                            const TWeakPtr<FConsoleVariablesEditorListRow> InRow)
{
	check (InRow.IsValid());
	
	Item = InRow;

	ProjectSettingsPtr = GetMutableDefault<UConsoleVariablesEditorProjectSettings>();
	
	ChildSlot
	[
		SAssignNew(InputWidget, SSpinBox<float>)
		.MaxFractionalDigits(3)
		.Value_Lambda([this]
		{
			check (Item.IsValid());
			
			if (Item.Pin()->GetWidgetCheckedState() == ECheckBoxState::Checked ||
					(ProjectSettingsPtr &&
						ProjectSettingsPtr->UncheckedRowDisplayType == EConsoleVariablesEditorRowDisplayType::ShowCurrentValue))
			{
				if (const IConsoleVariable* AsVariable = Item.Pin()->GetCommandInfo().Pin()->GetConsoleVariablePtr())
				{
					return FCString::Atof(*FString::SanitizeFloat(AsVariable->GetFloat()));
				}
			}

			return FCString::Atof(*Item.Pin()->GetCachedValue());
		})
		.OnValueChanged_Lambda([this] (const float InValue)
		{
			check (Item.IsValid());
					
			const FString ValueAsString = FString::SanitizeFloat(InValue);
					
			if (const TSharedPtr<FConsoleVariablesEditorListRow> PinnedItem = Item.Pin();
				!PinnedItem->GetCachedValue().Equals(ValueAsString))
			{				
				PinnedItem->GetCommandInfo().Pin()->ExecuteCommand(ValueAsString);
					
				PinnedItem->SetCachedValue(ValueAsString);
			}
		})
		.IsEnabled(this, &SConsoleVariablesEditorListValueInput::IsRowChecked)
	];

	Item.Pin()->SetCachedValue(GetInputValueAsString());
}

SConsoleVariablesEditorListValueInput_Float::~SConsoleVariablesEditorListValueInput_Float()
{
	InputWidget.Reset();
}

void SConsoleVariablesEditorListValueInput_Float::SetInputValue(const FString& InValueAsString)
{
	InputWidget->SetValue(FCString::Atof(*InValueAsString));
}

FString SConsoleVariablesEditorListValueInput_Float::GetInputValueAsString()
{
	return FString::SanitizeFloat(GetInputValue());
}

float SConsoleVariablesEditorListValueInput_Float::GetInputValue() const
{
	return InputWidget->GetValue();
}

void SConsoleVariablesEditorListValueInput_Int::Construct(const FArguments& InArgs,
                                                          const TWeakPtr<FConsoleVariablesEditorListRow> InRow,
                                                          const bool bIsShowFlag)
{
	check (InRow.IsValid());
	
	Item = InRow;

	ProjectSettingsPtr = GetMutableDefault<UConsoleVariablesEditorProjectSettings>();
	
	ChildSlot
	[
		SAssignNew(InputWidget, SSpinBox<int32>)
		.Value_Lambda([this]
		{
			check (Item.IsValid());
			
			if (Item.Pin()->GetWidgetCheckedState() == ECheckBoxState::Checked ||
					(ProjectSettingsPtr &&
						ProjectSettingsPtr->UncheckedRowDisplayType == EConsoleVariablesEditorRowDisplayType::ShowCurrentValue))
			{
				if (const IConsoleVariable* AsVariable = Item.Pin()->GetCommandInfo().Pin()->GetConsoleVariablePtr())
				{
					return FCString::Atoi(*AsVariable->GetString());
				}
			}

			return FCString::Atoi(*Item.Pin()->GetCachedValue());
		})
		.OnValueChanged_Lambda([this] (const int32 InValue)
		{
			check (Item.IsValid());
			
			const FString ValueAsString = FString::FromInt(InValue);
			
			if (const TSharedPtr<FConsoleVariablesEditorListRow> PinnedItem = Item.Pin();
				!PinnedItem->GetCachedValue().Equals(ValueAsString))
			{				
				PinnedItem->GetCommandInfo().Pin()->ExecuteCommand(ValueAsString);

				PinnedItem->SetCachedValue(ValueAsString);
			}
		})
		.IsEnabled(this, &SConsoleVariablesEditorListValueInput::IsRowChecked)
	];

	if (bIsShowFlag)
	{
		InputWidget->SetMinSliderValue(0);
		InputWidget->SetMaxSliderValue(2);

		const int32 PresetValue = FCString::Atoi(*Item.Pin()->GetPresetValue());

		Item.Pin()->SetPresetValue(FString::FromInt(FMath::Clamp(PresetValue, 0, 2)));
	}
	
	Item.Pin()->SetCachedValue(GetInputValueAsString());
}

SConsoleVariablesEditorListValueInput_Int::~SConsoleVariablesEditorListValueInput_Int()
{
	InputWidget.Reset();
}

void SConsoleVariablesEditorListValueInput_Int::SetInputValue(const FString& InValueAsString)
{
	if (InValueAsString.IsNumeric())
	{
		InputWidget->SetValue(FCString::Atoi(*InValueAsString));
	}
	else
	{
		InputWidget->SetValue(2);
		
		if (InValueAsString.TrimStartAndEnd().ToLower() == "true")
		{
			InputWidget->SetValue(1);
		}
		else if (InValueAsString.TrimStartAndEnd().ToLower() == "false")
		{
			InputWidget->SetValue(0);
		}
	}
}

FString SConsoleVariablesEditorListValueInput_Int::GetInputValueAsString()
{
	return FString::FromInt(GetInputValue());
}

int32 SConsoleVariablesEditorListValueInput_Int::GetInputValue() const
{
	return InputWidget->GetValue();
}

void SConsoleVariablesEditorListValueInput_String::Construct(const FArguments& InArgs,
                                                             const TWeakPtr<FConsoleVariablesEditorListRow> InRow)
{
	check (InRow.IsValid());
	
	Item = InRow;

	ProjectSettingsPtr = GetMutableDefault<UConsoleVariablesEditorProjectSettings>();
	
	ChildSlot
	[
		SAssignNew(InputWidget, SEditableText)
		.Text_Lambda([this]
		{
			check (Item.IsValid());
			
			if (Item.Pin()->GetWidgetCheckedState() == ECheckBoxState::Checked ||
					(ProjectSettingsPtr &&
						ProjectSettingsPtr->UncheckedRowDisplayType == EConsoleVariablesEditorRowDisplayType::ShowCurrentValue))
			{
				if (const IConsoleVariable* AsVariable = Item.Pin()->GetCommandInfo().Pin()->GetConsoleVariablePtr())
				{
					return FText::FromString(*AsVariable->GetString());
				}
			}

			return FText::FromString(Item.Pin()->GetCachedValue());
		})
		.OnTextCommitted_Lambda([this] (const FText& InValue, ETextCommit::Type InTextCommitType)
		{
			check (Item.IsValid());
			
			const FString ValueAsString = InValue.ToString();
			
			if (const TSharedPtr<FConsoleVariablesEditorListRow> PinnedItem = Item.Pin();
				!PinnedItem->GetCachedValue().Equals(ValueAsString))
			{				
				PinnedItem->GetCommandInfo().Pin()->ExecuteCommand(ValueAsString);

				PinnedItem->SetCachedValue(ValueAsString);
			}
		})
		.IsEnabled(this, &SConsoleVariablesEditorListValueInput::IsRowChecked)
	];

	Item.Pin()->SetCachedValue(GetInputValueAsString());
}

SConsoleVariablesEditorListValueInput_String::~SConsoleVariablesEditorListValueInput_String()
{
	InputWidget.Reset();
}

void SConsoleVariablesEditorListValueInput_String::SetInputValue(const FString& InValueAsString)
{
	InputWidget->SetText(FText::FromString(InValueAsString));
}

FString SConsoleVariablesEditorListValueInput_String::GetInputValueAsString()
{
	return GetInputValue();
}

FString SConsoleVariablesEditorListValueInput_String::GetInputValue() const
{
	return InputWidget->GetText().ToString();
}

void SConsoleVariablesEditorListValueInput_Bool::Construct(const FArguments& InArgs,
                                                           const TWeakPtr<FConsoleVariablesEditorListRow> InRow)
{
	check (InRow.IsValid());
	
	Item = InRow;

	ProjectSettingsPtr = GetMutableDefault<UConsoleVariablesEditorProjectSettings>();
	
	ChildSlot
	[
		SAssignNew(InputWidget, SButton)
		.OnClicked_Lambda([this] ()
		{
			const bool bValueAsBool = GetInputValue();

			SetInputValue(!bValueAsBool);

			return FReply::Handled();
		})
		.IsEnabled(this, &SConsoleVariablesEditorListValueInput::IsRowChecked)
		[
			SAssignNew(ButtonText, STextBlock)
			.Justification(ETextJustify::Center)
			.Text_Lambda([this]()
			{
				check (Item.IsValid());
			
				if (Item.Pin()->GetWidgetCheckedState() == ECheckBoxState::Checked ||
						(ProjectSettingsPtr &&
							ProjectSettingsPtr->UncheckedRowDisplayType == EConsoleVariablesEditorRowDisplayType::ShowCurrentValue))
				{
					if (const IConsoleVariable* AsVariable = Item.Pin()->GetCommandInfo().Pin()->GetConsoleVariablePtr())
					{
						return FText::FromString(AsVariable->GetString());
					}
				}

				return FText::FromString(Item.Pin()->GetCachedValue());
			})
		]
	];
	
	Item.Pin()->SetCachedValue(GetInputValueAsString());
}

SConsoleVariablesEditorListValueInput_Bool::~SConsoleVariablesEditorListValueInput_Bool()
{
	InputWidget.Reset();
	ButtonText.Reset();
}

void SConsoleVariablesEditorListValueInput_Bool::SetInputValue(const FString& InValueAsString)
{
	check (Item.IsValid());
			
	if (const TSharedPtr<FConsoleVariablesEditorListRow> PinnedItem = Item.Pin();
		!PinnedItem->GetCachedValue().Equals(InValueAsString))
	{				
		PinnedItem->GetCommandInfo().Pin()->ExecuteCommand(InValueAsString);

		PinnedItem->SetCachedValue(InValueAsString);
	}
}

void SConsoleVariablesEditorListValueInput_Bool::SetInputValue(const bool bNewValue)
{	
	SetInputValue(BoolToString(bNewValue));
}

FString SConsoleVariablesEditorListValueInput_Bool::GetInputValueAsString()
{
	return ButtonText->GetText().ToString();
}

bool SConsoleVariablesEditorListValueInput_Bool::GetInputValue()
{
	return StringToBool(GetInputValueAsString());
}

void SConsoleVariablesEditorListValueInput_Command::Construct(const FArguments& InArgs,
	const TWeakPtr<FConsoleVariablesEditorListRow> InRow, const FString& InSavedText)
{
	check (InRow.IsValid());
	
	Item = InRow;

	ProjectSettingsPtr = GetMutableDefault<UConsoleVariablesEditorProjectSettings>();
	
	ChildSlot
	[
		SNew(SHorizontalBox)

		+SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		[
			SAssignNew(InputText, SEditableText)
			.Text(FText::FromString(InSavedText))
			.HintText(LOCTEXT("CommandValueTypeRowInputHintText", "Value..."))
			.IsEnabled(this, &SConsoleVariablesEditorListValueInput::IsRowChecked)
			.Visibility(Item.Pin()->GetCommandInfo().Pin()->ObjectType ==
				FConsoleVariablesEditorCommandInfo::EConsoleObjectType::Command ?
				EVisibility::Visible : EVisibility::Collapsed)
		]

		+SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		[
			SAssignNew(InputWidget, SButton)
			.OnClicked_Lambda([this] ()
			{
				check (Item.IsValid());
				
				const FString ValueAsString = InputText->GetText().ToString();
				
				if (const TSharedPtr<FConsoleVariablesEditorListRow> PinnedItem = Item.Pin())
				{
					// Only execute if this row is a non-value command OR
					// it's a value-based command whose cached value differs from the input value
					if (PinnedItem->GetCommandInfo().Pin()->ObjectType ==
						FConsoleVariablesEditorCommandInfo::EConsoleObjectType::NullObject ||
						(PinnedItem->GetCommandInfo().Pin()->ObjectType ==
						FConsoleVariablesEditorCommandInfo::EConsoleObjectType::Command &&
						!PinnedItem->GetCachedValue().Equals(ValueAsString)))
					{
						PinnedItem->GetCommandInfo().Pin()->ExecuteCommand(ValueAsString);

						PinnedItem->SetCachedValue(ValueAsString);
					}

					return FReply::Handled();
				}

				return FReply::Unhandled();
			})
			.IsEnabled(this, &SConsoleVariablesEditorListValueInput::IsRowChecked)
			.ContentPadding(FMargin(0.f))
			[
				SNew(STextBlock)
				.Justification(ETextJustify::Center)
				.Text(LOCTEXT("ConsoleCommandExecutionButtonText","Execute"))
			]
		]
	];

	Item.Pin()->SetCachedValue(GetInputValueAsString());
}

SConsoleVariablesEditorListValueInput_Command::~SConsoleVariablesEditorListValueInput_Command()
{
	InputWidget.Reset();
	InputText.Reset();
}

void SConsoleVariablesEditorListValueInput_Command::SetInputValue(const FString& InValueAsString)
{
	InputText->SetText(FText::FromString(InValueAsString));
}

FString SConsoleVariablesEditorListValueInput_Command::GetInputValueAsString()
{
	return GetInputValue();
}

FString SConsoleVariablesEditorListValueInput_Command::GetInputValue() const
{
	return InputText->GetText().ToString();
}

#undef LOCTEXT_NAMESPACE
