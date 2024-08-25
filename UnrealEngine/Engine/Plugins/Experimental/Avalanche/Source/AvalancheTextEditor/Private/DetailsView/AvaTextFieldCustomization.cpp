// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaTextFieldCustomization.h"

#include "AvaTextDefs.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "SResetToDefaultPropertyEditor.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "FAvaTextFieldCustomization"

TSharedRef<IPropertyTypeCustomization> FAvaTextFieldCustomization::MakeInstance()
{
	return MakeShared<FAvaTextFieldCustomization>();
}

void FAvaTextFieldCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle,
    FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	HeaderRow.ForceAutoExpansion = true;
}

void FAvaTextFieldCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle,
	IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	Text_Handle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAvaTextField, Text));
	MaximumLengthEnum_Handle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAvaTextField, MaximumLength));
	MaxTextCount_Handle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAvaTextField, Count));
	TextCase_Handle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAvaTextField, TextCase));
	
	Text_Handle->GetValue(DefaultText);

	bTextFieldIsValid = true;
	
	auto GetResetButton = [](TSharedPtr<IPropertyHandle> PropertyHandle)
	{
		return
			SNew(SBox)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.Padding(5.0f)
			[
				SNew(SResetToDefaultPropertyEditor, PropertyHandle)
			];			
	};

	// text row
	StructBuilder.AddProperty(Text_Handle.ToSharedRef())
		.CustomWidget()
		.NameContent()
		[
			SNew(SBox)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			.Padding(0, 0, 5.0, 0)
			[
				SNew(STextBlock)
				.Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
				.Text(LOCTEXT("Text", "Text"))
			]
		]
		.ValueContent()
		.HAlign(HAlign_Fill)
		[
			SNew(SBox)
			.HAlign(HAlign_Fill)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				[
					SAssignNew(MultilineTextBox, SMultiLineEditableTextBox)
					.Text(DefaultText)
					.OnKeyDownHandler(this, &FAvaTextFieldCustomization::OnKeyDownInTextField)
					.OnVerifyTextChanged(this, &FAvaTextFieldCustomization::OnVerifyTextChanged)
					.OnTextCommitted(this, &FAvaTextFieldCustomization::OnTextCommitted)			
				]
			]
		];

	// text length row
	StructBuilder.AddProperty(MaximumLengthEnum_Handle.ToSharedRef()).CustomWidget()
		.ValueContent()
		.HAlign(HAlign_Fill)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.FillWidth(0.3f)
			[
				SNew(SBox)
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Center)
				.Padding(0, 0, 5.0, 0)
				[
					SNew(STextBlock)
					.Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
					.Text(LOCTEXT("TextLength", "Length"))
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(0.7f)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Fill)
				[
					MaximumLengthEnum_Handle->CreatePropertyValueWidget()
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SHorizontalBox)
					.Visibility(MakeAttributeLambda([this]{return IsCharacterLimitEnabled() ? EVisibility::Visible : EVisibility::Collapsed;}))
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Fill)
					[
						MaxTextCount_Handle->CreatePropertyValueWidget()
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						GetResetButton(MaxTextCount_Handle)
					]
				]
			]
		];

	// text case row
	StructBuilder.AddProperty(TextCase_Handle.ToSharedRef()).CustomWidget()
		.ValueContent()
		.HAlign(HAlign_Fill)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(0.3f)
			[
				SNew(SBox)
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Center)
				.Padding(0, 0, 5.0, 0)
				[
					SNew(STextBlock)
					.Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
					.Text(LOCTEXT("TextCase", "Text Case"))
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(0.7f)
			[
				TextCase_Handle->CreatePropertyValueWidget()
			]
		];

	const FSimpleDelegate OnTextLengthPropertiesChangedDelegate = FSimpleDelegate::CreateSP(this, &FAvaTextFieldCustomization::OnMaximumLengthEnumChanged);
	MaximumLengthEnum_Handle->SetOnPropertyValueChanged(OnTextLengthPropertiesChangedDelegate);
	MaxTextCount_Handle->SetOnPropertyValueChanged(OnTextLengthPropertiesChangedDelegate);

	const FSimpleDelegate OnTextFieldResetDelegate = FSimpleDelegate::CreateSP(this, &FAvaTextFieldCustomization::ResetText);	
	StructPropertyHandle->SetOnPropertyResetToDefault(OnTextFieldResetDelegate);
	Text_Handle->SetOnPropertyResetToDefault(OnTextFieldResetDelegate);
}

void FAvaTextFieldCustomization::UpdateTextFieldValidity(const FText& InText)
{
	bTextFieldIsValid = true;
	
	if (GetCurrentTextLengthType() == EAvaTextLength::CharacterCount)
	{
		int32 MaxTextCount;

		const FPropertyAccess::Result Result = MaxTextCount_Handle->GetValue(MaxTextCount);

		if (Result == FPropertyAccess::Success)
		{
			if (InText.ToString().Len() > MaxTextCount)
			{				
				bTextFieldIsValid = false;
			}
		}
	}
}

bool FAvaTextFieldCustomization::OnVerifyTextChanged(const FText& InText, FText& OutErrorText)
{
	bTextFieldIsValid = true;	
	
	if (GetCurrentTextLengthType() == EAvaTextLength::CharacterCount)
	{
		int32 MaxTextCount;

		const FPropertyAccess::Result Result = MaxTextCount_Handle->GetValue(MaxTextCount);

		if (Result == FPropertyAccess::Success)
		{
			if (InText.ToString().Len() > MaxTextCount)
			{
				OutErrorText = LOCTEXT("TextLengthError", "Text character limit exceeded.");
				bTextFieldIsValid = false;
				return false;
			}
		}
	}
	
	return true;
}

void FAvaTextFieldCustomization::OnTextCommitted(const FText& InText, ETextCommit::Type InCommitType) const
{
	if (bTextFieldIsValid)
	{
		Text_Handle->SetValue(InText);
	}
	else
	{
		int32 MaxTextCount;
		const FPropertyAccess::Result Result = MaxTextCount_Handle->GetValue(MaxTextCount);
		
		FString TrimmedText = InText.ToString();

		const int32 TextLength = TrimmedText.Len();

		// just to be safe
		if (TextLength > MaxTextCount)
		{
			TrimmedText.RemoveAt(MaxTextCount, TextLength - MaxTextCount);
			Text_Handle->SetValue(TrimmedText);
		}
	}
}

FReply FAvaTextFieldCustomization::OnKeyDownInTextField(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (!InKeyEvent.GetModifierKeys().IsShiftDown())
	{		
		if (InKeyEvent.GetKey() == EKeys::Enter && MultilineTextBox->HasKeyboardFocus())
		{
			// Clear focus
			return FReply::Handled().SetUserFocus(MultilineTextBox->GetParentWidget().ToSharedRef(), EFocusCause::Cleared);
		}
	}
	
	return FReply::Unhandled();
}

void FAvaTextFieldCustomization::Refresh(const FText& InText)
{
	// let's simulate a text field update
	FText OutError;
	OnVerifyTextChanged(InText, OutError);

	if (!bTextFieldIsValid)
	{
		MultilineTextBox->SetError(OutError);
	}
	else
	{
		MultilineTextBox->SetError(FText::GetEmpty());
	}
	
	OnTextCommitted(InText, ETextCommit::Default);
}

void FAvaTextFieldCustomization::OnMaximumLengthEnumChanged()
{
	Refresh(MultilineTextBox->GetText());
}

void FAvaTextFieldCustomization::ResetText()
{
	FText Text;
	Text_Handle->GetValue(Text);
	MultilineTextBox->SetText(Text);
	
	Refresh(Text);
}

EAvaTextLength FAvaTextFieldCustomization::GetCurrentTextLengthType() const
{
	if (MaximumLengthEnum_Handle.IsValid())
	{
		uint8 EnumAsUint;
		const FPropertyAccess::Result Result = MaximumLengthEnum_Handle->GetValue(EnumAsUint);

		if (Result == FPropertyAccess::Success)
		{
			return static_cast<EAvaTextLength>(EnumAsUint);
		}
	}

	return EAvaTextLength::Unlimited;	
}

bool FAvaTextFieldCustomization::IsCharacterLimitEnabled() const
{
	return GetCurrentTextLengthType() == EAvaTextLength::CharacterCount;
}

#undef LOCTEXT_NAMESPACE
