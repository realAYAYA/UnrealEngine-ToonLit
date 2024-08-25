// Copyright Epic Games, Inc. All Rights Reserved.

#include "RandomizeColumnEditor.h"
#include "RandomizeColumn.h"
#include "SPropertyAccessChainWidget.h"
#include "ObjectChooserWidgetFactories.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "GraphEditorSettings.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "RandomizeColumnEditor"

namespace UE::ChooserEditor
{

TSharedRef<SWidget> CreateRandomizeColumnWidget(UChooserTable* Chooser, FChooserColumnBase* Column, int Row)
{
	FRandomizeColumn* RandomizeColumn = static_cast<FRandomizeColumn*>(Column);
	
	if (Row == ColumnWidget_SpecialIndex_Fallback)
	{
		return SNullWidget::NullWidget;
	}
	if (Row == ColumnWidget_SpecialIndex_Header)
	{
		const FSlateBrush* ColumnIcon = FAppStyle::Get().GetBrush("Icons.Help");
		
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
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SSpacer).Size(FVector2D(10,10))
			]
			+ SHorizontalBox::Slot().VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(LOCTEXT("Randomize","Randomize"))
			];

		return ColumnHeaderWidget;
	}

	// create cell widget
	return
	SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1)
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SBox).WidthOverride(75).Content()
			[
				SNew(SNumericEntryBox<float>)
    				.Value_Lambda([RandomizeColumn, Row]()
    				{
    					if (!RandomizeColumn->RowValues.IsValidIndex(Row))
    					{
    						return 0.0f;
    					}
    					return RandomizeColumn->RowValues[Row];
    				})
    				.OnValueCommitted_Lambda([Chooser, Row, RandomizeColumn](float Value, ETextCommit::Type CommitType)
    				{
    					if (RandomizeColumn->RowValues.IsValidIndex(Row))
    					{
    						const FScopedTransaction Transaction(LOCTEXT("Edit Randomize Cell Data", "Edit Randomize Cell Data"));
    						Chooser->Modify(true);
    						RandomizeColumn->RowValues[Row] = FMath::Max(Value, 0.0);
    					}
    				})
    		]
    	]
		+ SHorizontalBox::Slot().FillWidth(1);
}
	
void RegisterRandomizeWidgets()
{
	FObjectChooserWidgetFactories::RegisterColumnWidgetCreator(FRandomizeColumn::StaticStruct(), CreateRandomizeColumnWidget);
}
	
}

#undef LOCTEXT_NAMESPACE
