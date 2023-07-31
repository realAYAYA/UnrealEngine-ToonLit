// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVMConversionPathCustomization.h"

#include "Algo/Transform.h"
#include "Bindings/MVVMBindingHelper.h"
#include "Blueprint/WidgetTree.h"
#include "Containers/Deque.h"
#include "DetailWidgetRow.h"
#include "EdGraphSchema_K2.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "Editor.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailGroup.h"
#include "IPropertyUtilities.h"
#include "K2Node_CallFunction.h"
#include "MVVMBlueprintViewBinding.h"
#include "MVVMEditorSubsystem.h"
#include "MVVMSubsystem.h"
#include "PropertyHandle.h"
#include "UObject/StructOnScope.h"
#include "WidgetBlueprint.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SMVVMConversionPath.h"
#include "Widgets/SMVVMFunctionParameter.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "MVVMConversionPath"

namespace UE::MVVM
{

	FConversionPathCustomization::FConversionPathCustomization(UWidgetBlueprint* InWidgetBlueprint)
	{
		check(InWidgetBlueprint);
		WidgetBlueprint = InWidgetBlueprint;
	}

	void FConversionPathCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
	{
		WeakPropertyUtilities = CustomizationUtils.GetPropertyUtilities();

		InPropertyHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FConversionPathCustomization::RefreshDetailsView));
		InPropertyHandle->SetOnChildPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FConversionPathCustomization::RefreshDetailsView));

		ParentHandle = InPropertyHandle->GetParentHandle();
		BindingModeHandle = ParentHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMVVMBlueprintViewBinding, BindingType));

		HeaderRow
			.ShouldAutoExpand()
			.NameContent()
			[
				InPropertyHandle->CreatePropertyNameWidget()
			]
			.ValueContent()
			[
				InPropertyHandle->CreatePropertyValueWidget()
			];
	}

	void FConversionPathCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> InPropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
	{
		AddRowForProperty(ChildBuilder, InPropertyHandle, true);
		AddRowForProperty(ChildBuilder, InPropertyHandle, false);
	}

	void FConversionPathCustomization::RefreshDetailsView() const
	{
		if (TSharedPtr<IPropertyUtilities> PropertyUtilities = WeakPropertyUtilities.Pin())
		{
			PropertyUtilities->RequestRefresh();
		}
	}

	FText FConversionPathCustomization::GetFunctionPathText(TSharedRef<IPropertyHandle> Property) const
	{
		void* Value;
		FPropertyAccess::Result Result = Property->GetValueData(Value);

		if (Result == FPropertyAccess::Success)
		{
			FMemberReference* MemberReference = reinterpret_cast<FMemberReference*>(Value);
			UFunction* Function = MemberReference->ResolveMember<UFunction>(WidgetBlueprint);

			return FText::FromString(Function->GetPathName());
		}

		if (Result == FPropertyAccess::MultipleValues)
		{
			return LOCTEXT("MultipleValues", "Multiple Values");
		}

		return FText::GetEmpty();
	}

	void FConversionPathCustomization::OnTextCommitted(const FText& NewValue, ETextCommit::Type CommitType, TSharedRef<IPropertyHandle> Property, bool bSourceToDestination)
	{
		UFunction* FoundFunction = FindObject<UFunction>(nullptr, *NewValue.ToString(), true);
		OnFunctionPathChanged(FoundFunction, Property, bSourceToDestination);
	}

	void FConversionPathCustomization::OnFunctionPathChanged(const UFunction* NewFunction, TSharedRef<IPropertyHandle> InPropertyHandle, bool bSourceToDestination)
	{
		UMVVMEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
		
		TArray<void*> RawBindings;
		ParentHandle->AccessRawData(RawBindings);

		for (void* RawBinding : RawBindings)
		{
			FMVVMBlueprintViewBinding* ViewBinding = static_cast<FMVVMBlueprintViewBinding*>(RawBinding);
			if (bSourceToDestination)
			{
				EditorSubsystem->SetSourceToDestinationConversionFunction(WidgetBlueprint, *ViewBinding, NewFunction);
			}
			else
			{
				EditorSubsystem->SetDestinationToSourceConversionFunction(WidgetBlueprint, *ViewBinding, NewFunction);
			}
		}

		RefreshDetailsView();
	}

	void FConversionPathCustomization::AddRowForProperty(IDetailChildrenBuilder& ChildBuilder, const TSharedPtr<IPropertyHandle>& InPropertyHandle, bool bSourceToDestination)
	{
		TArray<FMVVMBlueprintViewBinding*> ViewBindings;
		{
			TArray<void*> RawData;
			ParentHandle->AccessRawData(RawData);

			for (void* Data : RawData)
			{
				ViewBindings.Add((FMVVMBlueprintViewBinding*) Data);
			}
		}

		const FName FunctionPropertyName = bSourceToDestination ? GET_MEMBER_NAME_CHECKED(FMVVMBlueprintViewConversionPath, SourceToDestinationFunction) : GET_MEMBER_NAME_CHECKED(FMVVMBlueprintViewConversionPath, DestinationToSourceFunction);
		TSharedRef<IPropertyHandle> FunctionProperty = InPropertyHandle->GetChildHandle(FunctionPropertyName).ToSharedRef();

		ChildBuilder.AddProperty(FunctionProperty)
			.IsEnabled(
				TAttribute<bool>::CreateLambda([this, bSourceToDestination]()
					{
						uint8 EnumValue;
						if (BindingModeHandle->GetValue(EnumValue) == FPropertyAccess::Success)
						{
							return bSourceToDestination ? IsForwardBinding((EMVVMBindingMode) EnumValue) : IsBackwardBinding((EMVVMBindingMode) EnumValue);
						}
						return true;
					}))
			.CustomWidget()
			.NameContent()
			[
				FunctionProperty->CreatePropertyNameWidget()
			]
			.ValueContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				[
					SNew(SEditableTextBox)
					.Text(this, &FConversionPathCustomization::GetFunctionPathText, FunctionProperty)
					.OnTextCommitted(this, &FConversionPathCustomization::OnTextCommitted, FunctionProperty, bSourceToDestination)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SMVVMConversionPath, WidgetBlueprint, bSourceToDestination)
					.Bindings(ViewBindings)
					.OnFunctionChanged(this, &FConversionPathCustomization::OnFunctionPathChanged, FunctionProperty, bSourceToDestination)
				]
			];

		if (ViewBindings.Num() != 1)
		{
			// don't show arguments for multi-selection
			return;
		}

		UMVVMEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();

		UEdGraph* Graph = EditorSubsystem->GetConversionFunctionGraph(WidgetBlueprint, *ViewBindings[0], bSourceToDestination);
		if (Graph == nullptr)
		{
			return;
		}

		TArray<UK2Node_CallFunction*> FunctionNodes;
		Graph->GetNodesOfClass<UK2Node_CallFunction>(FunctionNodes);

		if (FunctionNodes.Num() != 1)
		{
			// ambiguous result, no idea what our function node is
			return;
		}

		UK2Node_CallFunction* CallFunctionNode = FunctionNodes[0];
		for (UEdGraphPin* Pin : CallFunctionNode->Pins)
		{
			// skip output and exec pins, the rest should be arguments
			if (Pin->Direction != EGPD_Input || Pin->PinName == UEdGraphSchema_K2::PN_Execute || Pin->PinName == UEdGraphSchema_K2::PN_Self)
			{
				continue;
			}

			ChildBuilder.AddCustomRow(Pin->GetDisplayName())
			.NameContent()
			[
				SNew(SBox)
				.Padding(FMargin(16, 0, 0, 0))
				[
					SNew(STextBlock)
					.Text(Pin->GetDisplayName())
				]
			]
			.ValueContent()
			[
				SNew(UE::MVVM::SFunctionParameter)
				.WidgetBlueprint(WidgetBlueprint)
				.Binding(ViewBindings[0])
				.ParameterName(Pin->GetFName())
				.SourceToDestination(bSourceToDestination)
				.OnGetBindingMode(this, &FConversionPathCustomization::GetBindingMode)
			];
		}
	}

	EMVVMBindingMode FConversionPathCustomization::GetBindingMode() const
	{
		uint8 EnumValue = 0;
		if (BindingModeHandle->GetValue(EnumValue) == FPropertyAccess::Success)
		{
			return static_cast<EMVVMBindingMode>(EnumValue);
		}

		return EMVVMBindingMode::OneWayToDestination;
	}
}

#undef LOCTEXT_NAMESPACE 
