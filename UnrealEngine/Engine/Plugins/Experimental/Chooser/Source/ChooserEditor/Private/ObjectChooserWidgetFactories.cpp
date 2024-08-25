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
	
void FObjectChooserWidgetFactories::RegisterWidgetCreator(const UStruct* Type, FChooserWidgetCreator Creator)
{
	ChooserWidgetCreators.Add(Type, Creator);	
}
	
void  FObjectChooserWidgetFactories::RegisterColumnWidgetCreator(const UStruct* ColumnType, FColumnWidgetCreator Creator)
{
	ColumnWidgetCreators.Add(ColumnType, Creator);	
}
	
TSharedPtr<SWidget> FObjectChooserWidgetFactories::CreateColumnWidget(FChooserColumnBase* Column, const UStruct* ColumnType, UChooserTable* Chooser, int RowIndex)
{
	if (Column)
	{
		while (ColumnType)
		{
			if (FColumnWidgetCreator* Creator = FObjectChooserWidgetFactories::ColumnWidgetCreators.Find(ColumnType))
			{
				return (*Creator)(Chooser, Column, RowIndex);
			}
			ColumnType = ColumnType->GetSuperStruct();
		}
	}

	return nullptr;
}
	
	
TSharedPtr<SWidget> FObjectChooserWidgetFactories::CreateWidget(bool bReadOnly, UObject* TransactionObject, void* Value, const UStruct* ValueType, UClass* ResultBaseClass, FChooserWidgetValueChanged ValueChanged)
{
	if (Value)
	{
		while (ValueType)
		{
			if (FChooserWidgetCreator* Creator = ChooserWidgetCreators.Find(ValueType))
			{
				return (*Creator)(bReadOnly, TransactionObject, Value, ResultBaseClass, ValueChanged);
			}
			ValueType = ValueType->GetSuperStruct();
		}
	}

	return nullptr;
}

TSharedPtr<SWidget> FObjectChooserWidgetFactories::CreateWidget(bool bReadOnly, UObject* TransactionObject, const UScriptStruct* BaseType, void* Value, const UStruct* ValueType, UClass* ResultBaseClass, const FOnStructPicked& CreateClassCallback, TSharedPtr<SBorder>* InnerWidget, FChooserWidgetValueChanged ValueChanged, FText NullValueDisplayText)
{
	TSharedPtr<SWidget> LeftWidget = CreateWidget(bReadOnly, TransactionObject, Value, ValueType, ResultBaseClass, ValueChanged);
	if (bReadOnly)
	{
		// don't need the type selector dropdown when read only
		return LeftWidget;
	}
	
	if (!LeftWidget.IsValid())
	{
		LeftWidget = SNew(STextBlock)
			.Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
			.Margin(2)
			.Text(NullValueDisplayText.IsEmpty() ? LOCTEXT("SelectDataType", "Select Data Type..." ) : NullValueDisplayText);
	}

	// button for replacing data with a different Data Interface class
	TSharedPtr<SComboButton> Button = SNew(SComboButton)
			.ComboButtonStyle(FAppStyle::Get(), "SimpleComboButton");
	
	Button->SetOnGetMenuContent(FOnGetContent::CreateLambda([BaseType, Button, CreateClassCallback]()
	{
		FStructViewerInitializationOptions Options;
		Options.StructFilter = MakeShared<FStructFilter>(BaseType);
		Options.NameTypeToDisplay = EStructViewerNameTypeToDisplay::DisplayName;
		Options.bShowNoneOption = true;
		
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

TMap<const UStruct*, TFunction<TSharedRef<SWidget>(UChooserTable* Chooser, FChooserColumnBase* Column, int Row)>> FObjectChooserWidgetFactories::ColumnWidgetCreators;
TMap<const UStruct*, FChooserWidgetCreator> FObjectChooserWidgetFactories::ChooserWidgetCreators;

void FObjectChooserWidgetFactories::RegisterWidgets()
{
}
	
}

#undef LOCTEXT_NAMESPACE
