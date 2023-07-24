// Copyright Epic Games, Inc. All Rights Reserved.

#include "FloatRangeColumnEditor.h"
#include "FloatRangeColumn.h"
#include "ContextPropertyWidget.h"
#include "ObjectChooserWidgetFactories.h"
#include "ChooserTableEditor.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "GraphEditorSettings.h"

#define LOCTEXT_NAMESPACE "FloatRangeColumnEditor"

namespace UE::ChooserEditor
{
TSharedRef<SWidget> CreateFloatPropertyWidget(UObject* TransactionObject, void* Value, UClass* ContextClass)
{
	return CreatePropertyWidget<FFloatContextProperty>(TransactionObject, Value, ContextClass, GetDefault<UGraphEditorSettings>()->FloatPinTypeColor);
}
	
TSharedRef<SWidget> CreateFloatRangeColumnWidget(UChooserTable* Chooser, FChooserColumnBase* Column, int Row)
{
	FFloatRangeColumn* FloatRangeColumn = static_cast<FFloatRangeColumn*>(Column);

	return SNew(SHorizontalBox)
	+ SHorizontalBox::Slot().MaxWidth(10)
	[
		SNew(STextBlock).Text(LOCTEXT("FloatRangeLeft", "("))
	]
	+ SHorizontalBox::Slot().FillWidth(1.0)
	[
		SNew(SNumericEntryBox<float>)
		.MaxValue_Lambda([FloatRangeColumn, Row]()
		{
			return (Row < FloatRangeColumn->RowValues.Num()) ? FloatRangeColumn->RowValues[Row].Max : 0;
		})
		.Value_Lambda([FloatRangeColumn, Row]()
		{
			return (Row < FloatRangeColumn->RowValues.Num()) ? FloatRangeColumn->RowValues[Row].Min : 0;
		})
		.OnValueCommitted_Lambda([Chooser, FloatRangeColumn, Row](float NewValue, ETextCommit::Type CommitType)
		{
			if (Row < FloatRangeColumn->RowValues.Num())
			{
				const FScopedTransaction Transaction(LOCTEXT("Edit Min Value", "Edit Min Value"));
				Chooser->Modify(true);
				FloatRangeColumn->RowValues[Row].Min = NewValue;
			}
		})
	]
	+ SHorizontalBox::Slot().MaxWidth(10)
	[
		SNew(STextBlock).Text(LOCTEXT("FloatRangeComma", " ,"))
	]
	+ SHorizontalBox::Slot().FillWidth(1.0)
	[
		SNew(SNumericEntryBox<float>)
		.MinValue_Lambda([FloatRangeColumn, Row]()
		{
			return (Row < FloatRangeColumn->RowValues.Num()) ? FloatRangeColumn->RowValues[Row].Min : 0;
		})
		.Value_Lambda([FloatRangeColumn, Row]()
		{
			return (Row < FloatRangeColumn->RowValues.Num()) ? FloatRangeColumn->RowValues[Row].Max : 0;
		})
		.OnValueCommitted_Lambda([Chooser, FloatRangeColumn, Row](float NewValue, ETextCommit::Type CommitType)
		{
			if (Row < FloatRangeColumn->RowValues.Num())
			{
				const FScopedTransaction Transaction(LOCTEXT("Edit Max", "Edit Max Value"));
				Chooser->Modify(true);
				FloatRangeColumn->RowValues[Row].Max = NewValue;
			}
		})
	]
	+ SHorizontalBox::Slot().MaxWidth(10)
	[
		SNew(STextBlock).Text(LOCTEXT("FloatRangeRight", " )"))
	];
}

	
void RegisterFloatRangeWidgets()
{
	FObjectChooserWidgetFactories::ChooserWidgetCreators.Add(FFloatContextProperty::StaticStruct(), CreateFloatPropertyWidget);
	FChooserTableEditor::ColumnWidgetCreators.Add(FFloatRangeColumn::StaticStruct(), CreateFloatRangeColumnWidget);
}
	
}

#undef LOCTEXT_NAMESPACE
