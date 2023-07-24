// Copyright Epic Games, Inc. All Rights Reserved.

#include "BoolColumnEditor.h"
#include "BoolColumn.h"
#include "ContextPropertyWidget.h"
#include "ChooserTableEditor.h"
#include "ObjectChooserWidgetFactories.h"
#include "Widgets/Input/SCheckBox.h"
#include "GraphEditorSettings.h"

#define LOCTEXT_NAMESPACE "BoolColumnEditor"

namespace UE::ChooserEditor
{

TSharedRef<SWidget> CreateBoolColumnWidget(UChooserTable* Chooser, FChooserColumnBase* Column, int Row)
{
	FBoolColumn* BoolColumn = static_cast<FBoolColumn*>(Column);

	return SNew (SCheckBox)
	.OnCheckStateChanged_Lambda([Chooser, BoolColumn,Row](ECheckBoxState State)
	{
		if (Row < BoolColumn->RowValues.Num())
		{
			const FScopedTransaction Transaction(LOCTEXT("Change Bool Value", "Change Bool Value"));
			Chooser->Modify(true);
			BoolColumn->RowValues[Row] = (State == ECheckBoxState::Checked);
		}
	})
	.IsChecked_Lambda([BoolColumn, Row]()
	{
		const bool value = (Row < BoolColumn->RowValues.Num()) ? BoolColumn->RowValues[Row] : false;
		return value ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	});
}
	
TSharedRef<SWidget> CreateBoolPropertyWidget(UObject* TransactionObject, void* Value, UClass* ContextClass)
{
	return CreatePropertyWidget<FBoolContextProperty>(TransactionObject, Value, ContextClass, GetDefault<UGraphEditorSettings>()->BooleanPinTypeColor);
}
	
void RegisterBoolWidgets()
{
	FObjectChooserWidgetFactories::ChooserWidgetCreators.Add(FBoolContextProperty::StaticStruct(), CreateBoolPropertyWidget);
	FChooserTableEditor::ColumnWidgetCreators.Add(FBoolColumn::StaticStruct(), CreateBoolColumnWidget);
}
	
}

#undef LOCTEXT_NAMESPACE
