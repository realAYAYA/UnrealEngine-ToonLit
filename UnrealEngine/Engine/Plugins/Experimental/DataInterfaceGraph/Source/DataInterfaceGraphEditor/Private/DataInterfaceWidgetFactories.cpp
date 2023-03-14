// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataInterfaceWidgetFactories.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Input/SHyperlink.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SComboButton.h"
#include "SClassViewer.h"
#include "DetailCategoryBuilder.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "DataInterfaceClassFilter.h"

#define LOCTEXT_NAMESPACE "DataInterfaceEditor"

namespace UE::DataInterfaceGraphEditor
{

void ConvertToText(UObject* Object, FText& OutText)
{
	UClass* Class = Object->GetClass();
	while (Class)
	{
		if (auto TextConverter = FDataInterfaceWidgetFactories::DataInterfaceTextConverter.Find(Class))
		{
			(*TextConverter)(Object, OutText);
			break;
		}
		Class = Class->GetSuperClass();
	}
}

TSharedPtr<SWidget> FDataInterfaceWidgetFactories::CreateDataInterfaceWidget(FName DataInterfaceTypeName, UObject* Value, const FOnClassPicked& CreateClassCallback, TSharedPtr<SBorder>* InnerWidget)
{
	TSharedPtr<SWidget> LeftWidget;

	if (Value)
	{
		UClass* Class = Value->GetClass();
		while (Class && !LeftWidget.IsValid())
		{
			if (auto Creator = FDataInterfaceWidgetFactories::DataInterfaceWidgetCreators.Find(Class))
			{
				LeftWidget = (*Creator)(Value);
				break;
			}
			Class = Class->GetSuperClass();
		}
	}
	
	if (!LeftWidget.IsValid())
	{
		LeftWidget = SNew(STextBlock).Text(LOCTEXT("SelectDataType", "Select Data Type..."));
	}


	// button for replacing data with a different Data Interface class
	TSharedPtr<SComboButton> Button = SNew(SComboButton)
			.ComboButtonStyle(FAppStyle::Get(), "SimpleComboButton");
	
	Button->SetOnGetMenuContent(FOnGetContent::CreateLambda([DataInterfaceTypeName, Button, CreateClassCallback]()
	{
		FClassViewerInitializationOptions Options;
		Options.ClassFilters.Add(MakeShared<FDataInterfaceClassFilter>(DataInterfaceTypeName, nullptr));
		
		// Add class filter for columns here
		TSharedRef<SWidget>  ClassMenu = FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer").
		CreateClassViewer(Options, FOnClassPicked::CreateLambda([Button, CreateClassCallback](UClass* Class)
		{
			Button->SetIsOpen(false);
			CreateClassCallback.Execute(Class);
		}));
		return ClassMenu;
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
		+SHorizontalBox::Slot().FillWidth(75)
		[
			Border.ToSharedRef()
		]
		+SHorizontalBox::Slot().FillWidth(25)
		[
			Button.ToSharedRef()
		]
	;


	return Widget;
}

TMap<const UClass*, TFunction<void (const UObject* Object, FText& OutText)>> FDataInterfaceWidgetFactories::DataInterfaceTextConverter;
TMap<const UClass*, TFunction<TSharedRef<SWidget> (UObject* Object)>> FDataInterfaceWidgetFactories::DataInterfaceWidgetCreators;

void ConvertToText_Base(const UObject* Object, FText& OutText)
{
	OutText = FText::FromString(Object->GetName());
}

void FDataInterfaceWidgetFactories::RegisterWidgets()
{
	DataInterfaceTextConverter.Add(UObject::StaticClass(), ConvertToText_Base);
}
	
}

#undef LOCTEXT_NAMESPACE
