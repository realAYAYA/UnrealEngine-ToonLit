// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayTagColumnEditor.h"
#include "SPropertyAccessChainWidget.h"
#include "GameplayTagColumn.h"
#include "ObjectChooserWidgetFactories.h"
#include "SGameplayTagWidget.h"
#include "SSimpleComboButton.h"
#include "GraphEditorSettings.h"
#include "Widgets/Images/SImage.h"

#define LOCTEXT_NAMESPACE "FGameplayTagColumnEditor"

namespace UE::ChooserEditor
{

TSharedRef<SWidget> CreateGameplayTagColumnWidget(UChooserTable* Chooser, FChooserColumnBase* Column, int Row)
{
	FGameplayTagColumn* GameplayTagColumn = static_cast<struct FGameplayTagColumn*>(Column);
	
	if (Row == ColumnWidget_SpecialIndex_Fallback)
	{
		return SNullWidget::NullWidget;
	}
	else if (Row == ColumnWidget_SpecialIndex_Header)
	{
		// create column header widget
		TSharedPtr<SWidget> InputValueWidget = nullptr;
		if (FChooserParameterBase* InputValue = Column->GetInputValue())
		{
			InputValueWidget = FObjectChooserWidgetFactories::CreateWidget(false, Chooser, InputValue, Column->GetInputType(), Chooser->OutputObjectType);
		}
		
		const FSlateBrush* ColumnIcon = FCoreStyle::Get().GetBrush("Icons.Filter");
		
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
				SNew(SSimpleComboButton)
					.IsEnabled_Lambda([Chooser]()
					{
						 return !Chooser->HasDebugTarget();
					})
					.Text_Lambda([GameplayTagColumn]()
					{
						FText Text = FText::FromString(GameplayTagColumn->TestValue.ToStringSimple(false));
						if (Text.IsEmpty())
						{
							Text = LOCTEXT("None", "None");
						}
						return Text;
					})	
					.OnGetMenuContent_Lambda([Chooser, GameplayTagColumn, Row]()
					{
						TArray<SGameplayTagWidget::FEditableGameplayTagContainerDatum> EditableContainers;
						EditableContainers.Emplace(Chooser, &(GameplayTagColumn->TestValue));
						return TSharedRef<SWidget>(SNew(SGameplayTagWidget, EditableContainers));
					})
			];
		}

		return ColumnHeaderWidget;
	}

	// create cell widget
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

TSharedRef<SWidget> CreateGameplayTagPropertyWidget(bool bReadOnly, UObject* TransactionObject, void* Value, UClass* ResultBaseClass, FChooserWidgetValueChanged ValueChanged)
{
	IHasContextClass* HasContextClass = Cast<IHasContextClass>(TransactionObject);

	FGameplayTagContextProperty* ContextProperty = reinterpret_cast<FGameplayTagContextProperty*>(Value);

	return SNew(SPropertyAccessChainWidget).ContextClassOwner(HasContextClass).AllowFunctions(false).BindingColor("StructPinTypeColor").TypeFilter("FGameplayTagContainer")
	.PropertyBindingValue(&ContextProperty->Binding)
	.OnValueChanged(ValueChanged);
}
	
void RegisterGameplayTagWidgets()
{
	FObjectChooserWidgetFactories::RegisterWidgetCreator(FGameplayTagContextProperty::StaticStruct(), CreateGameplayTagPropertyWidget);
	FObjectChooserWidgetFactories::RegisterColumnWidgetCreator(FGameplayTagColumn::StaticStruct(), CreateGameplayTagColumnWidget);
}
	
}

#undef LOCTEXT_NAMESPACE
