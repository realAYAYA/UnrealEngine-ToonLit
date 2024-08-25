// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMMaterialParameters.h"
#include "Components/DMMaterialParameter.h"
#include "Components/DMMaterialValue.h"
#include "DetailLayoutBuilder.h"
#include "DMValueDefinition.h"
#include "DynamicMaterialEditorModule.h"
#include "DynamicMaterialEditorStyle.h"
#include "DynamicMaterialModule.h"
#include "Model/DynamicMaterialModel.h"
#include "ScopedTransaction.h"
#include "Slate/Properties/SDMDetailsGrid.h"
#include "SlateOptMacros.h"
#include "Styling/StyleColors.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SDMMaterialParameters"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SDMMaterialParameters::Construct(const FArguments& InArgs, TWeakObjectPtr<UDynamicMaterialModel> InBuilderWeak)
{
	ensure(InBuilderWeak.IsValid());
	ModelWeak = InBuilderWeak;

	static const FVector2D AddButtonSize = FVector2D(12.f, 12.f);
	static const FMargin AddButtonPadding = FMargin(0.f, 0.f, 0.f, 0.f);
	static const FMargin AddButtonContentMargin = FMargin(1.f, 1.f, 1.f, 1.f);

	auto CreateAddValueButton = [this](EDMValueType ValueType)
	{
		FText Tooltip = FText::Format(
			LOCTEXT("AddValueTemplate", "Add {0} Value."), 
			UDMValueDefinitionLibrary::GetValueDefinition(ValueType).GetDisplayName()
		);

		const FName Icon = FDynamicMaterialEditorStyle::GetBrushNameForType(ValueType);

		SHorizontalBox::FSlot::FSlotArguments Slot(SHorizontalBox::Slot());
		Slot
			.Padding(AddButtonPadding)
			.AutoWidth()
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.ContentPadding(AddButtonContentMargin)
				.OnClicked(this, &SDMMaterialParameters::OnAddValueButtonClicked, ValueType)
				.ToolTipText(Tooltip)
				[
					SNew(SImage)
					.Image(FDynamicMaterialEditorStyle::GetBrush(Icon))
					.DesiredSizeOverride(AddButtonSize)
				]
			];
		return Slot;
	};

	ChildSlot
	[
		SAssignNew(ValuesArea, SExpandableArea)
		.HeaderContent()
		[
			SNew(SHorizontalBox)
			.ToolTipText(LOCTEXT("GlobalValuesTooltip", "Values which can be re-used within your material. These values just be constants or exposed as modifiable paramters if given a name."))
			+ SHorizontalBox::Slot()
			.Padding(0.f, 1.f, 3.f, 0.f)
			.AutoWidth()
			.VAlign(EVerticalAlignment::VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("GlobalValuesHeader", "Global Values"))
				.Font(IDetailLayoutBuilder::GetDetailFontBold())
			]
			+ CreateAddValueButton(EDMValueType::VT_Float1)
			+ CreateAddValueButton(EDMValueType::VT_Float2)
			+ CreateAddValueButton(EDMValueType::VT_Float3_XYZ)
			+ CreateAddValueButton(EDMValueType::VT_Float3_RPY)
			+ CreateAddValueButton(EDMValueType::VT_Float3_RGB)
			+ CreateAddValueButton(EDMValueType::VT_Float4_RGBA)
			+ CreateAddValueButton(EDMValueType::VT_Texture)
			+ CreateAddValueButton(EDMValueType::VT_Bool)
		]
		.InitiallyCollapsed(false)
		.HeaderPadding(FMargin(3.f, 5.f, 3.f, 5.f))
		.BodyContent()
		[
			SAssignNew(AreaBody, SBox)
			.HAlign(EHorizontalAlignment::HAlign_Left)
			[
				CreateValuesWidget()
			]
		]
	];
}

TSharedRef<SWidget> SDMMaterialParameters::CreateValuesWidget()
{
	if (!ModelWeak.IsValid())
	{
		// @formatter:off
		return
			SNew(SBox)
			.HAlign(EHorizontalAlignment::HAlign_Center)
			.VAlign(EVerticalAlignment::VAlign_Center)
			.Padding(10.f, 5.f, 10.f, 5.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("GlobalValuesContent", "Global Values Content"))
				.Font(IDetailLayoutBuilder::GetDetailFont())
			];
		// @formatter:on
	}

	const TArray<UDMMaterialValue*>& Values = ModelWeak->GetValues();

	if (Values.IsEmpty())
	{
		// @formatter:off
		return
			SNew(SBox)
			.HAlign(EHorizontalAlignment::HAlign_Center)
			.VAlign(EVerticalAlignment::VAlign_Center)
			.Padding(10.f, 5.f, 10.f, 5.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("GlobalValuesNoContent", "Click a button to add a value!"))
				.Font(IDetailLayoutBuilder::GetDetailFont())
			];
		// @formatter:on
	}

	TSharedRef<SDMDetailsGrid> ValuesList = SNew(SDMDetailsGrid);

	static const FMargin CellPadding = FMargin(8.f, 8.f, 8.f, 6.f);

	for (int32 Index = 0; Index < Values.Num(); ++Index)
	{
		TSharedPtr<SWidget> ValueEditWidget = FDynamicMaterialEditorModule::GetValueEditWidgetDelegate(Values[Index]->GetClass())
			.Execute(nullptr, Values[Index]);

		if (ValueEditWidget.IsValid() == false)
		{
			continue;
		}

		const FName Icon = FDynamicMaterialEditorStyle::GetBrushNameForType(Values[Index]->GetType());

		TSharedRef<SHorizontalBox> HBox = 
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Padding(CellPadding)
			[
				SNew(SBox)
				.WidthOverride(16.0f)
				[
					SNew(SButton)
					.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
					.ContentPadding(FMargin(6.f, 6.f, 6.f, 6.f))
					.OnClicked(this, &SDMMaterialParameters::OnRemoveValueButtonClicked, Index)
					.ToolTipText(LOCTEXT("RemoveValue", "Remove Value"))
					[
						SNew(SImage)
						.Image(FDynamicMaterialEditorStyle::GetBrush(Icon))
						.DesiredSizeOverride(FVector2D(12.f, 12.f))
					]
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(CellPadding)
			[
				SNew(SBox)
				.WidthOverride(75.0f)
				[
					SNew(SEditableTextBox)
					.MinDesiredWidth(69.f)
					.BackgroundColor(FStyleColors::Background.GetSpecifiedColor())
					.ForegroundColor(FStyleColors::AccentWhite.GetSpecifiedColor())
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.HintText(LOCTEXT("Name", "Name"))
					.IsEnabled(true)
					.Text(Values[Index]->GetParameter() ? FText::FromName(Values[Index]->GetParameter()->GetParameterName()) : FText::GetEmpty())
					.OnVerifyTextChanged(this, &SDMMaterialParameters::OnVerifyValueNameChanged, Index)
					.OnTextChanged(this, &SDMMaterialParameters::OnAcceptValueNameChanged, Index)
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(CellPadding)
			[
				SNew(SBox)
				.MinDesiredWidth(100.0f)
				[
					ValueEditWidget.ToSharedRef()
				]
			];


		ValuesList->AddRow_TextLabel(FText::AsNumber(Index + 1), HBox);
	}

	return ValuesList;
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SDMMaterialParameters::RefreshWidgets()
{
	if (AreaBody.IsValid())
	{
		AreaBody->SetContent(CreateValuesWidget());
	}
}

FReply SDMMaterialParameters::OnAddValueButtonClicked(EDMValueType Type)
{
	UDynamicMaterialModel* MaterialModel = ModelWeak.Get();

	if (!MaterialModel)
	{
		return FReply::Unhandled();
	}

	if (Type <= EDMValueType::VT_None || Type == EDMValueType::VT_Float_Any || Type > EDMValueType::VT_Texture)
	{
		return FReply::Unhandled();
	}

	FScopedTransaction Transaction(LOCTEXT("AddGlobalValue", "Add Global Value"));
	MaterialModel->Modify();
	MaterialModel->AddValue(Type);

	if (ValuesArea.IsValid())
	{
		if (!ValuesArea->IsExpanded())
		{
			ValuesArea->SetExpanded(true);
		}
	}

	return FReply::Handled();
}

FReply SDMMaterialParameters::OnRemoveValueButtonClicked(int32 Index)
{
	UDynamicMaterialModel* MaterialModel = ModelWeak.Get();

	if (!MaterialModel)
	{
		return FReply::Unhandled();
	}

	if (MaterialModel->GetValueByIndex(Index) == nullptr)
	{
		return FReply::Unhandled();
	}

	FScopedTransaction Transaction(LOCTEXT("RemoveGlobalValue", "Remove Global Value"));
	MaterialModel->Modify();
	MaterialModel->RemoveValueByIndex(Index);

	return FReply::Handled();
}

bool SDMMaterialParameters::OnVerifyValueNameChanged(const FText& InNewName, FText& OutErrorText, int32 ValueIndex)
{
	UDynamicMaterialModel* MaterialModel = ModelWeak.Get();

	if (!MaterialModel)
	{
		OutErrorText = LOCTEXT("InternalError", "Internal Error");
		return false;
	}

	const FString NewNameString = InNewName.ToString();

	if (NewNameString.Len() > 20)
	{
		OutErrorText = LOCTEXT("NameTooLong", "That name is too long.");
		return false;
	};

	if (NewNameString.Len() == 0)
	{
		return true;
	}

	const TArray<UDMMaterialValue*>& Values = MaterialModel->GetValues();

	if (!Values.IsValidIndex(ValueIndex))
	{
		OutErrorText = LOCTEXT("InternalError", "Internal Error");
		return false;
	}

	if (MaterialModel->HasParameterName(*NewNameString))
	{
		OutErrorText = LOCTEXT("NameTaken", "That name has already been used.");
		return false;
	}

	return true;
}

void SDMMaterialParameters::OnAcceptValueNameChanged(const FText& InNewName, int32 Index)
{
	FText ErrorText;

	if (!OnVerifyValueNameChanged(InNewName, ErrorText, Index))
	{
		return;
	}

	const TArray<UDMMaterialValue*>& Values = ModelWeak->GetValues();

	FScopedTransaction Transaction(LOCTEXT("SetGlobalValueName", "Set Global Value Name"));
	Values[Index]->Modify();

	if (UDMMaterialParameter* Parameter = Values[Index]->GetParameter())
	{
		Parameter->Modify();
	}

	Values[Index]->SetParameterName(*InNewName.ToString());
}

#undef LOCTEXT_NAMESPACE
