// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMVVMFunctionParameter.h" 
#include "EdGraphSchema_K2.h"
#include "Editor.h"
#include "MVVMBlueprintView.h"
#include "MVVMEditorSubsystem.h"
#include "NodeFactory.h"
#include "SGraphPin.h"
#include "Styling/MVVMEditorStyle.h"
#include "Types/MVVMBindingMode.h"
#include "WidgetBlueprint.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SMVVMFieldSelector.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "MVVMFieldBinding"

namespace UE::MVVM
{

void SFunctionParameter::Construct(const FArguments& InArgs, UWidgetBlueprint* InWidgetBlueprint)
{
	WidgetBlueprint = InWidgetBlueprint;
	check(InWidgetBlueprint != nullptr);

	BindingId = InArgs._BindingId;
	check(BindingId.IsValid());

	ParameterName = InArgs._ParameterName;
	check(!ParameterName.IsNone());
	
	bSourceToDestination = InArgs._SourceToDestination;
	bAllowDefault = InArgs._AllowDefault;

	UMVVMEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
	UMVVMBlueprintView* View = EditorSubsystem->GetView(InWidgetBlueprint);
	check(View);

	const FMVVMBlueprintViewBinding* Binding = View->GetBinding(BindingId);
	check(Binding);

	const UFunction* ConversionFunction = EditorSubsystem->GetConversionFunction(InWidgetBlueprint, *Binding, bSourceToDestination);
	const FProperty* Property = ConversionFunction->FindPropertyByName(ParameterName);
	check(Property);

	TSharedRef<SWidget> ValueWidget = SNullWidget::NullWidget;
	bool bIsBooleanPin = CastField<const FBoolProperty>(Property) != nullptr;

	if (UEdGraphPin* Pin = EditorSubsystem->GetConversionFunctionArgumentPin(InWidgetBlueprint, *Binding, ParameterName, bSourceToDestination))
	{
		bIsBooleanPin = Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Boolean;
		// create a new pin widget so that we can get the default value widget out of it
		if (TSharedPtr<SGraphPin> PinWidget = FNodeFactory::CreatePinWidget(Pin))
		{
			GraphPin = PinWidget;
			ValueWidget = PinWidget->GetDefaultValueWidget();
		}
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
			.Padding(0.0f)
			.BorderImage(FMVVMEditorStyle::Get().GetBrush("FunctionParameter.Border"))
			[
				ValueWidget
			];
	}

	FMVVMBlueprintPropertyPath Path = OnGetSelectedField();
	bDefaultValueVisible = Path.IsEmpty();

	bool bFromViewModel = UE::MVVM::IsForwardBinding(Binding->BindingType);
	TSharedPtr<SHorizontalBox> HBox;

	ChildSlot
	[
		SNew(SBox)
		.MinDesiredWidth(bIsBooleanPin ? FOptionalSize() : 100)
		[
			SAssignNew(HBox, SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(0.0f, 2.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SWidgetSwitcher)
				.WidgetIndex(this, &SFunctionParameter::GetCurrentWidgetIndex)
				+ SWidgetSwitcher::Slot()
				[
					ValueWidget
				]
				+ SWidgetSwitcher::Slot()
				[
					SNew(SFieldSelector, WidgetBlueprint.Get())
					.OnGetPropertyPath(this, &SFunctionParameter::OnGetSelectedField)
					.OnFieldSelectionChanged(this, &SFunctionParameter::HandleFieldSelectionChanged)
					.OnGetSelectionContext(this, &SFunctionParameter::GetSelectedSelectionContext)
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

int32 SFunctionParameter::GetCurrentWidgetIndex() const
{
	return bDefaultValueVisible && bAllowDefault ? 0 : 1;
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
	if (const UWidgetBlueprint* WidgetBlueprintPtr = WidgetBlueprint.Get())
	{
		const UMVVMEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
		if (const UMVVMBlueprintView* View = Subsystem->GetView(WidgetBlueprintPtr))
		{
			if (const FMVVMBlueprintViewBinding* Binding = View->GetBinding(BindingId))
			{
				return Subsystem->GetPathForConversionFunctionArgument(WidgetBlueprint.Get(), *Binding, ParameterName, bSourceToDestination);
			}
		}
	}
	return FMVVMBlueprintPropertyPath();
}

void SFunctionParameter::SetSelectedField(const FMVVMBlueprintPropertyPath& Path)
{
	if (const UWidgetBlueprint* WidgetBlueprintPtr = WidgetBlueprint.Get())
	{
		const UMVVMEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
		if (UMVVMBlueprintView* View = Subsystem->GetView(WidgetBlueprintPtr))
		{
			if (FMVVMBlueprintViewBinding* Binding = View->GetBinding(BindingId))
			{
				Subsystem->SetPathForConversionFunctionArgument(WidgetBlueprint.Get(), *Binding, ParameterName, Path, bSourceToDestination);
			}
		}
	}
}

void SFunctionParameter::HandleFieldSelectionChanged(FMVVMBlueprintPropertyPath SelectedField, const UFunction* Function)
{
	SetSelectedField(SelectedField);
}

FFieldSelectionContext SFunctionParameter::GetSelectedSelectionContext() const
{
	FFieldSelectionContext Result;

	UWidgetBlueprint* WidgetBlueprintPtr = WidgetBlueprint.Get();
	if (WidgetBlueprintPtr == nullptr)
	{
		return Result;
	}

	UMVVMBlueprintView* View = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>()->GetView(WidgetBlueprintPtr);
	check(View);

	const FMVVMBlueprintViewBinding* Binding = View->GetBinding(BindingId);
	if (Binding == nullptr)
	{
		return Result;
	}

	const UFunction* ConversionFunction = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>()->GetConversionFunction(WidgetBlueprintPtr, *Binding, bSourceToDestination);
	if (ConversionFunction == nullptr)
	{
		return Result;
	}

	Result.BindingMode = EMVVMBindingMode::OneWayToDestination;
	Result.AssignableTo = ConversionFunction->FindPropertyByName(ParameterName);
	Result.bAllowWidgets = true;
	Result.bAllowViewModels = true;
	Result.bAllowConversionFunctions = false;
	Result.bReadable = true;
	Result.bWritable = false;

	return Result;
}

}//namespace
#undef LOCTEXT_NAMESPACE
