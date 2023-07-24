// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMVVMFunctionParameter.h" 
#include "EdGraphSchema_K2.h"
#include "Editor.h"
#include "MVVMBlueprintViewBinding.h"
#include "MVVMEditorSubsystem.h"
#include "NodeFactory.h"
#include "SGraphPin.h"
#include "Styling/MVVMEditorStyle.h"
#include "WidgetBlueprint.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SMVVMFieldSelector.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "MVVMFieldBinding"

namespace UE::MVVM
{

void SFunctionParameter::Construct(const FArguments& InArgs)
{
	WidgetBlueprint = InArgs._WidgetBlueprint;
	check(InArgs._WidgetBlueprint != nullptr);

	Binding = InArgs._Binding;
	check(Binding != nullptr);

	ParameterName = InArgs._ParameterName;
	check(!ParameterName.IsNone());
	
	bSourceToDestination = InArgs._SourceToDestination;
	bAllowDefault = InArgs._AllowDefault;

	GetBindingModeDelegate = InArgs._OnGetBindingMode;
	check(GetBindingModeDelegate.IsBound());

	UMVVMEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
	const UFunction* ConversionFunction = EditorSubsystem->GetConversionFunction(InArgs._WidgetBlueprint, *Binding, bSourceToDestination);
	const FProperty* Property = ConversionFunction->FindPropertyByName(ParameterName);
	check(Property);

	TSharedRef<SWidget> ValueWidget = SNullWidget::NullWidget;

	bool bIsBooleanPin = false;
	UEdGraphPin* Pin = EditorSubsystem->GetConversionFunctionArgumentPin(InArgs._WidgetBlueprint, *Binding, ParameterName, bSourceToDestination);
	if (Pin != nullptr)
	{
		// create a new pin widget so that we can get the default value widget out of it
		if (TSharedPtr<SGraphPin> PinWidget = FNodeFactory::CreateK2PinWidget(Pin))
		{
			GraphPin = PinWidget;
			ValueWidget = PinWidget->GetDefaultValueWidget();
		}

		bIsBooleanPin = Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Boolean;
	}

	if (ValueWidget == SNullWidget::NullWidget)
	{
		ValueWidget = SNew(STextBlock)
			.Text(LOCTEXT("DefaultValue", "Default Value"))
			.TextStyle(FAppStyle::Get(), "HintText");
	}
	// booleans are represented by a checkbox which doesn't expand to the minsize we have, so don't put a border around them
	else if (!bIsBooleanPin)
	{
		ValueWidget = SNew(SBorder)
			.Padding(0)
			.BorderImage(FMVVMEditorStyle::Get().GetBrush("FunctionParameter.Border"))
			[
				ValueWidget
			];
	}

	FMVVMBlueprintPropertyPath Path = OnGetSelectedField();
	bDefaultValueVisible = Path.IsEmpty();

	ValueWidget->SetVisibility(TAttribute<EVisibility>::CreateSP(this, &SFunctionParameter::OnGetVisibility, true));

	bool bFromViewModel = UE::MVVM::IsForwardBinding(Binding->BindingType);
	TSharedPtr<SHorizontalBox> HBox;

	ChildSlot
	[
		SNew(SBox)
		.MinDesiredWidth(bIsBooleanPin ? FOptionalSize() : 100)
		[
			SAssignNew(HBox, SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(0, 2)
			.VAlign(VAlign_Center)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					ValueWidget
				]
				+ SOverlay::Slot()
				[
					SAssignNew(FieldSelector, UE::MVVM::SFieldSelector, WidgetBlueprint.Get(), bFromViewModel)
					.Visibility(this, &SFunctionParameter::OnGetVisibility, false)
					.SelectedField(this, &SFunctionParameter::OnGetSelectedField)
					.BindingMode_Lambda([this]() { return GetBindingModeDelegate.Execute(); })
					.OnFieldSelectionChanged(this, &SFunctionParameter::OnFieldSelectionChanged)
					.AssignableTo(Property)
				]
			]
		]
	];

	if (InArgs._AllowDefault)
	{
		HBox->AddSlot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			.Padding(8, 0, 0, 0)
			.AutoWidth()
			[
				SNew(SCheckBox)
				.ToolTipText(LOCTEXT("BindArgument", "Bind this argument to a property."))
				.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
				.IsChecked(this, &SFunctionParameter::OnGetIsBindArgumentChecked)
				.OnCheckStateChanged(this, &SFunctionParameter::OnBindArgumentChecked)
				.Padding(FMargin(4))
				[
					SNew(SImage)
					.DesiredSizeOverride(FVector2D(16, 16))
					.Image_Lambda([this]()
						{
							ECheckBoxState CheckState = OnGetIsBindArgumentChecked();
							return (CheckState == ECheckBoxState::Checked) ? FAppStyle::GetBrush("Icons.Link") : FAppStyle::GetBrush("Icons.Unlink");
						})
				]
			];		
	}
}

EVisibility SFunctionParameter::OnGetVisibility(bool bDefaultValue) const
{
	if (!bAllowDefault)
	{
		return bDefaultValue ? EVisibility::Collapsed : EVisibility::Visible;
	}

	// if we're not bound then show the default value widget, otherwise show the binding widget
	return bDefaultValue == bDefaultValueVisible ? EVisibility::Visible : EVisibility::Collapsed;
}

ECheckBoxState SFunctionParameter::OnGetIsBindArgumentChecked() const 
{
	return bDefaultValueVisible ? ECheckBoxState::Unchecked : ECheckBoxState::Checked;
}

void SFunctionParameter::OnBindArgumentChecked(ECheckBoxState Checked)
{
	UMVVMEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();

	bDefaultValueVisible = (Checked != ECheckBoxState::Checked);

	if (bDefaultValueVisible)
	{
		PreviousSelectedField = OnGetSelectedField();
		SetSelectedField(FMVVMBlueprintPropertyPath());
	}
	else
	{
		SetSelectedField(PreviousSelectedField);
	}
}

FMVVMBlueprintPropertyPath SFunctionParameter::OnGetSelectedField() const
{
	UMVVMEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
	return EditorSubsystem->GetPathForConversionFunctionArgument(WidgetBlueprint.Get(), *Binding, ParameterName, bSourceToDestination);
}

void SFunctionParameter::SetSelectedField(const FMVVMBlueprintPropertyPath& Path)
{
	UMVVMEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
	EditorSubsystem->SetPathForConversionFunctionArgument(WidgetBlueprint.Get(), *Binding, ParameterName, Path, bSourceToDestination);
}

void SFunctionParameter::OnFieldSelectionChanged(FMVVMBlueprintPropertyPath Selected)
{
	SetSelectedField(Selected);
}

}
#undef LOCTEXT_NAMESPACE
