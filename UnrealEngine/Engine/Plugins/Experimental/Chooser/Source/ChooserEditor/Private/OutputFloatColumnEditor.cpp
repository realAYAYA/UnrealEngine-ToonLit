// Copyright Epic Games, Inc. All Rights Reserved.

#include "OutputFloatColumnEditor.h"
#include "OutputFloatColumn.h"
#include "ObjectChooserWidgetFactories.h"
#include "ChooserTableEditor.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Images/SImage.h"
#include "GraphEditorSettings.h"
#include "SPropertyAccessChainWidget.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "OutputBoolColumnEditor"

namespace UE::ChooserEditor
{
	
TSharedRef<SWidget> CreateOutputFloatColumnWidget(UChooserTable* Chooser, FChooserColumnBase* Column, int Row)
{
	FOutputFloatColumn* OutputFloatColumn = static_cast<FOutputFloatColumn*>(Column);

    if (Row == ColumnWidget_SpecialIndex_Header)
	{
		// create column header widget
		TSharedPtr<SWidget> InputValueWidget = nullptr;
		if (FChooserParameterBase* InputValue = Column->GetInputValue())
		{
			InputValueWidget = FObjectChooserWidgetFactories::CreateWidget(false, Chooser, InputValue, Column->GetInputType(), Chooser->OutputObjectType);
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
	
		if (Chooser->bEnableDebugTesting)
		{
			ColumnHeaderWidget = SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			[
				ColumnHeaderWidget
			]
			+ SVerticalBox::Slot()
			[
				SNew(SNumericEntryBox<float>).IsEnabled(false)
				.Value_Lambda([OutputFloatColumn]() { return OutputFloatColumn->TestValue; })
			];
		}

		return ColumnHeaderWidget;
	}
	if (Row == ColumnWidget_SpecialIndex_Fallback)
    {
		return SNew(SNumericEntryBox<double>)
        		.Value_Lambda([OutputFloatColumn]()
        		{
        			return OutputFloatColumn->FallbackValue;
        		})
        		.OnValueCommitted_Lambda([Chooser, OutputFloatColumn](double NewValue, ETextCommit::Type CommitType)
        		{
					const FScopedTransaction Transaction(LOCTEXT("Edit Min Value", "Edit Min Value"));
					Chooser->Modify(true);
					OutputFloatColumn->FallbackValue = NewValue;
        		});	
    }

	// create cell widget
	return SNew(SNumericEntryBox<double>)
    		.Value_Lambda([OutputFloatColumn, Row]()
    		{
    			return (Row < OutputFloatColumn->RowValues.Num()) ? OutputFloatColumn->RowValues[Row] : 0;
    		})
    		.OnValueCommitted_Lambda([Chooser, OutputFloatColumn, Row](double NewValue, ETextCommit::Type CommitType)
    		{
    			if (Row < OutputFloatColumn->RowValues.Num())
    			{
    				const FScopedTransaction Transaction(LOCTEXT("Edit Min Value", "Edit Min Value"));
    				Chooser->Modify(true);
    				OutputFloatColumn->RowValues[Row] = NewValue;
    			}
    		});
}

	
void RegisterOutputFloatWidgets()
{
	FObjectChooserWidgetFactories::RegisterColumnWidgetCreator(FOutputFloatColumn::StaticStruct(), CreateOutputFloatColumnWidget);
}
	
}

#undef LOCTEXT_NAMESPACE
