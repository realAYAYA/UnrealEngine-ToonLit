// Copyright Epic Games, Inc. All Rights Reserved.

#include "ObjectChooserWidgetFactories.h"

#include "Modules/ModuleManager.h"
#include "Widgets/Input/SHyperlink.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SComboButton.h"
#include "SClassViewer.h"
#include "DetailCategoryBuilder.h"
#include "IObjectChooser.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "ObjectChooserClassFilter.h"
#include "StructViewerModule.h"

#define LOCTEXT_NAMESPACE "DataInterfaceEditor"

namespace UE::ChooserEditor
{

TSharedPtr<SWidget> FObjectChooserWidgetFactories::CreateWidget(UObject* TransactionObject, void* Value, const UStruct* ValueType, UClass* ContextClass)
{
	if (Value)
	{
		while (ValueType)
		{
			if (FChooserWidgetCreator* Creator = ChooserWidgetCreators.Find(ValueType))
			{
				return (*Creator)(TransactionObject, Value, ContextClass);
			}
			ValueType = ValueType->GetSuperStruct();
		}
	}

	return nullptr;
}

TSharedPtr<SWidget> FObjectChooserWidgetFactories::CreateWidget(UObject* TransactionObject, const UScriptStruct* BaseType, void* Value, const UStruct* ValueType, UClass* ContextClass, const FOnStructPicked& CreateClassCallback, TSharedPtr<SBorder>* InnerWidget)
{
	TSharedPtr<SWidget> LeftWidget = CreateWidget(TransactionObject, Value, ValueType, ContextClass);
	
	if (!LeftWidget.IsValid())
	{
		LeftWidget = SNew(STextBlock).Text(LOCTEXT("SelectDataType", "Select Data Type..."));
	}

	// button for replacing data with a different Data Interface class
	TSharedPtr<SComboButton> Button = SNew(SComboButton)
			.ComboButtonStyle(FAppStyle::Get(), "SimpleComboButton");
	
	Button->SetOnGetMenuContent(FOnGetContent::CreateLambda([BaseType, Button, CreateClassCallback]()
	{
		FStructViewerInitializationOptions Options;
		Options.StructFilter = MakeShared<FStructFilter>(BaseType);
		Options.NameTypeToDisplay = EStructViewerNameTypeToDisplay::DisplayName;
		
		TSharedRef<SWidget> Widget = FModuleManager::LoadModuleChecked<FStructViewerModule>("StructViewer").CreateStructViewer(Options, FOnStructPicked::CreateLambda(
			[Button, CreateClassCallback](const UScriptStruct* ChosenStruct)
		{
			Button->SetIsOpen(false);
			CreateClassCallback.Execute(ChosenStruct);
		}));
		return Widget;
	}));

	TSharedPtr <SBorder> Border;
	if (InnerWidget && InnerWidget->IsValid())
	{
		Border = *InnerWidget;
	}
	else
	{
		Border = SNew(SBorder);
	}
	
	if (InnerWidget)
	{
		*InnerWidget = Border;
	}
	
	Border->SetContent(LeftWidget.ToSharedRef());

	TSharedPtr<SWidget> Widget = SNew(SHorizontalBox)
		+SHorizontalBox::Slot().FillWidth(100)
		[
			Border.ToSharedRef()
		]
		+SHorizontalBox::Slot().AutoWidth()
		[
			Button.ToSharedRef()
		]
	;


	return Widget;
}

TMap<const UStruct*, FChooserWidgetCreator> FObjectChooserWidgetFactories::ChooserWidgetCreators;

void FObjectChooserWidgetFactories::RegisterWidgets()
{
}
	
}

#undef LOCTEXT_NAMESPACE
