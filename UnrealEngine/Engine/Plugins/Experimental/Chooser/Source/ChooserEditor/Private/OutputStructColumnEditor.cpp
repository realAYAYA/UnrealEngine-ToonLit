// Copyright Epic Games, Inc. All Rights Reserved.

#include "OutputStructColumnEditor.h"
#include "OutputStructColumn.h"
#include "SPropertyAccessChainWidget.h"
#include "ObjectChooserWidgetFactories.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SNullWidget.h"
#include "GraphEditorSettings.h"

#define LOCTEXT_NAMESPACE "StructOutputColumnEditor"

namespace UE::ChooserEditor
{
	
TSharedRef<SWidget> CreateOutputStructColumnWidget(UChooserTable* Chooser, FChooserColumnBase* Column, int Row)
{
	if (Row < 0)
	{
		// create column header widget
		TSharedPtr<SWidget> InputValueWidget = nullptr;
		if (FChooserParameterBase* InputValue = Column->GetInputValue())
		{
			InputValueWidget = FObjectChooserWidgetFactories::CreateWidget(false, Chooser, InputValue, Column->GetInputType(), Chooser->OutputObjectType,
				FChooserWidgetValueChanged::CreateLambda([Column]()
				{
					FOutputStructColumn* StructColumn = static_cast<FOutputStructColumn*>(Column);
					StructColumn->StructTypeChanged();
				})
				);
		}
		
		const FSlateBrush* ColumnIcon = FCoreStyle::Get().GetBrush("Icons.ArrowRight");
		
		TSharedRef<SWidget> ColumnHeaderWidget = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBorder)
				.BorderBackgroundColor(FLinearColor(0,0,0,0))
				.Content()
				[
					SNew(SImage).Image(ColumnIcon)
				]
			]
			+ SHorizontalBox::Slot()
			[
				InputValueWidget ? InputValueWidget.ToSharedRef() : SNullWidget::NullWidget
			];
	
		return ColumnHeaderWidget;
	}

	return SNullWidget::NullWidget;
}
	
TSharedRef<SWidget> CreateStructPropertyWidget(bool bReadOnly, UObject* TransactionObject, void* Value, UClass* ResultBaseClass, FChooserWidgetValueChanged ValueChanged)
{
	IHasContextClass* HasContextClass = Cast<IHasContextClass>(TransactionObject);

	FStructContextProperty* ContextProperty = reinterpret_cast<FStructContextProperty*>(Value);

	return SNew(SPropertyAccessChainWidget).ContextClassOwner(HasContextClass).BindingColor("StructPinTypeColor").TypeFilter("struct")
		.PropertyBindingValue(&ContextProperty->Binding)
		.OnAddBinding_Lambda(
    		[ContextProperty, TransactionObject, ValueChanged](FName InPropertyName, const TArray<FBindingChainElement>& InBindingChain)
    		{
    			const FScopedTransaction Transaction(NSLOCTEXT("ContextPropertyWidget", "Change Property Binding", "Change Property Binding"));
    			TransactionObject->Modify(true);
    			ContextProperty->SetBinding(TransactionObject, InBindingChain);
    			ValueChanged.ExecuteIfBound();
    		});
}
	
void RegisterStructWidgets()
{
	FObjectChooserWidgetFactories::RegisterWidgetCreator(FStructContextProperty::StaticStruct(), CreateStructPropertyWidget);
	FObjectChooserWidgetFactories::RegisterColumnWidgetCreator(FOutputStructColumn::StaticStruct(), CreateOutputStructColumnWidget);
}
	
}

#undef LOCTEXT_NAMESPACE
