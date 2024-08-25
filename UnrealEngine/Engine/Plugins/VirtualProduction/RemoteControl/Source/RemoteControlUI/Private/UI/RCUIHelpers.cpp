// Copyright Epic Games, Inc. All Rights Reserved.

#include "RCUIHelpers.h"

#include "Controller/RCController.h"
#include "Editor.h"
#include "EdGraphSchema_K2.h"
#include "EdGraph/EdGraphPin.h"
#include "Framework/Application/SlateApplication.h"
#include "GraphEditorSettings.h"
#include "IDetailTreeNode.h"
#include "IPropertyRowGenerator.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "PropertyHandle.h"
#include "RCVirtualProperty.h"
#include "TimerManager.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"

FLinearColor UE::RCUIHelpers::GetFieldClassTypeColor(const FProperty* InProperty)
{
	if (ensure(InProperty))
	{
		const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
		FEdGraphPinType PinType;
		if (Schema->ConvertPropertyToPinType(InProperty, PinType))
		{
			return Schema->GetPinTypeColor(PinType);
		}
	}

	return FLinearColor::White;
}

FName UE::RCUIHelpers::GetFieldClassDisplayName(const FProperty* InProperty)
{
	FName FieldClassDisplayName = NAME_None;

	if (ensure(InProperty))
	{
		const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
		FEdGraphPinType PinType;

		if (Schema->ConvertPropertyToPinType(InProperty, PinType))
		{
			if(UObject* SubCategoryObject = PinType.PinSubCategoryObject.Get())
			{
				FieldClassDisplayName = *SubCategoryObject->GetName();
			}
			else
			{
				FieldClassDisplayName = PinType.PinCategory;
			}
		}
	}

	if (!FieldClassDisplayName.IsNone())
	{
		FString ResultString = FieldClassDisplayName.ToString();

		if (ResultString.StartsWith("Bool"))
		{
			ResultString = "Boolean";
		}

		if (ResultString.StartsWith("Int"))
		{
			ResultString = *ResultString.Replace(TEXT("Int"), TEXT("Integer"));
		}

		if (ResultString == "real")
		{
			ResultString = "Float";
		}

		ResultString[0] = FChar::ToUpper(ResultString[0]);

		return *ResultString;
	}

	return NAME_None;
}

TSharedPtr<IDetailTreeNode> UE::RCUIHelpers::GetDetailTreeNodeForVirtualProperty(TObjectPtr<URCVirtualPropertySelfContainer> InVirtualPropertySelfContainer, 
	TSharedPtr<IPropertyRowGenerator>& OutPropertyRowGenerator)
{
	TSharedPtr<IDetailTreeNode> DetailTreeNode;

	FPropertyRowGeneratorArgs Args;
	Args.bShouldShowHiddenProperties = true;
	OutPropertyRowGenerator = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor").CreatePropertyRowGenerator(Args);

	OutPropertyRowGenerator->SetStructure(InVirtualPropertySelfContainer->CreateStructOnScope());

	for (const TSharedRef<IDetailTreeNode>& CategoryNode : OutPropertyRowGenerator->GetRootTreeNodes())
	{
		TArray<TSharedRef<IDetailTreeNode>> Children;
		CategoryNode->GetChildren(Children);

		// Use the first child as the detail tree node
		if (Children.Num())
		{
			DetailTreeNode = Children[0];
			break;
		}
	}

	return DetailTreeNode;
}

static void SetFocusToWidgetNextTick(const TSharedRef< SWidget> InWidget)
{
	GEditor->GetTimerManager()->SetTimerForNextTick(
		FTimerDelegate::CreateLambda([InWidget]()
			{
				UE::RCUIHelpers::FindFocusableWidgetAndSetKeyboardFocus(InWidget);
			})
	);
}

TSharedRef<SWidget> UE::RCUIHelpers::GetGenericFieldWidget(const TSharedPtr<IDetailTreeNode> DetailTreeNode, 
	TSharedPtr<IPropertyHandle>* OutPropertyHandle /*=nullptr*/, bool bFocusInputWidget/*= false*/)
{
	if (!DetailTreeNode.IsValid())
	{
		return SNullWidget::NullWidget;
	}

	if (OutPropertyHandle)
	{
		*OutPropertyHandle = DetailTreeNode->CreatePropertyHandle();
	}

	const FNodeWidgets NodeWidgets = DetailTreeNode->CreateNodeWidgets();

	const TSharedRef<SHorizontalBox> FieldWidget = SNew(SHorizontalBox);

	if (NodeWidgets.ValueWidget)
	{
		FieldWidget->AddSlot()
			.Padding(FMargin(3.0f, 2.0f))
			.VAlign(VAlign_Center)
			[
				NodeWidgets.ValueWidget.ToSharedRef()
			];

		if (bFocusInputWidget)
			SetFocusToWidgetNextTick(NodeWidgets.ValueWidget.ToSharedRef());

	}
	else if (NodeWidgets.WholeRowWidget)
	{
		FieldWidget->AddSlot()
			.Padding(FMargin(3.0f, 2.0f))
			.VAlign(VAlign_Center)
			[
				NodeWidgets.WholeRowWidget.ToSharedRef()
			];

		if (bFocusInputWidget)
			SetFocusToWidgetNextTick(NodeWidgets.WholeRowWidget.ToSharedRef());
	}

	return FieldWidget;
}

bool UE::RCUIHelpers::FindFocusableWidgetAndSetKeyboardFocus(const TSharedRef< SWidget> InWidget)
{
	if (InWidget->SupportsKeyboardFocus())
	{
		FSlateApplication::Get().SetKeyboardFocus(InWidget, EFocusCause::Navigation);

		return true; // Success!
	}

	// Check child widgets...
	if (FChildren* Children = InWidget->GetChildren())
	{
		if(Children->Num() > 0)
		{
			// Note: We're only interested in the first child for all current usecases.
			return FindFocusableWidgetAndSetKeyboardFocus(Children->GetChildAt(0)); // Recurse!
		}
	}

	return false; // No focusable widget found in the entire widget hierarchy
}

TSharedRef<SWidget> UE::RCUIHelpers::GetTypeColorWidget(const FProperty* InProperty)
{
	if (!ensure(InProperty))
	{
		return SNullWidget::NullWidget;
	}

	const FLinearColor TypeColor = UE::RCUIHelpers::GetFieldClassTypeColor(InProperty);
	const FText TooltipText = FText::FromName(UE::RCUIHelpers::GetFieldClassDisplayName(InProperty));

	TSharedRef<SWidget> TypeColorWidget =
			SNew(SBox)
			.HeightOverride(5.f)
			.HAlign(HAlign_Left)
			.Padding(FMargin(5.f, 0.f))
			.ToolTipText(TooltipText)
			[
				SNew(SBorder)
				.Visibility(EVisibility::Visible)
				.BorderImage(FAppStyle::Get().GetBrush("NumericEntrySpinBox.Decorator"))
				.BorderBackgroundColor(TypeColor)
				.Padding(FMargin(5.0f, 0.0f, 0.0f, 0.f))
			];

	return TypeColorWidget;
}
