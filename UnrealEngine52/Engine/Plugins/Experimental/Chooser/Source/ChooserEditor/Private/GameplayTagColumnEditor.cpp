// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayTagColumnEditor.h"
#include "ContextPropertyWidget.h"
#include "ChooserTableEditor.h"
#include "GameplayTagColumn.h"
#include "ObjectChooserWidgetFactories.h"
#include "SGameplayTagWidget.h"
#include "SSimpleComboButton.h"
#include "GraphEditorSettings.h"

#define LOCTEXT_NAMESPACE "FGameplayTagColumnEditor"

namespace UE::ChooserEditor
{

TSharedRef<SWidget> CreateGameplayTagColumnWidget(UChooserTable* Chooser, FChooserColumnBase* Column, int Row)
{
	FGameplayTagColumn* GameplayTagColumn = static_cast<struct FGameplayTagColumn*>(Column);

	return SNew(SSimpleComboButton)
		.Text_Lambda([GameplayTagColumn, Row]()
		{
			if (Row < GameplayTagColumn->RowValues.Num())
			{
				FText Result = FText::FromString(GameplayTagColumn->RowValues[Row].ToStringSimple(false));
				if (Result.IsEmpty())
				{
					Result = LOCTEXT("Any Tag", "[Any]");
				}
				return Result;
			}
			else
			{
				return FText();
			}
		})	
		.OnGetMenuContent_Lambda([Chooser, GameplayTagColumn, Row]()
		{
			if (Row < GameplayTagColumn->RowValues.Num())
			{
				TArray<SGameplayTagWidget::FEditableGameplayTagContainerDatum> EditableContainers;
				EditableContainers.Emplace(Chooser, &(GameplayTagColumn->RowValues[Row]));
				return TSharedRef<SWidget>(SNew(SGameplayTagWidget, EditableContainers));
			}

			return SNullWidget::NullWidget;
		}
	);
}

TSharedRef<SWidget> CreateGameplayTagPropertyWidget(UObject* TransactionObject, void* Value, UClass* ContextClass)
{
	return CreatePropertyWidget<FGameplayTagContextProperty>(TransactionObject, Value, ContextClass, GetDefault<UGraphEditorSettings>()->StructPinTypeColor);
}
	
void RegisterGameplayTagWidgets()
{
	FObjectChooserWidgetFactories::ChooserWidgetCreators.Add(FGameplayTagContextProperty::StaticStruct(), CreateGameplayTagPropertyWidget);
	FChooserTableEditor::ColumnWidgetCreators.Add(FGameplayTagColumn::StaticStruct(), CreateGameplayTagColumnWidget);
}
	
}

#undef LOCTEXT_NAMESPACE
