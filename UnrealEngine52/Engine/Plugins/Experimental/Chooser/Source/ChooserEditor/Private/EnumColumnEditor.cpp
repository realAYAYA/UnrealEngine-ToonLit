// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnumColumnEditor.h"
#include "EnumColumn.h"
#include "ContextPropertyWidget.h"
#include "ObjectChooserWidgetFactories.h"
#include "ChooserTableEditor.h"
#include "GraphEditorSettings.h"
#include "SEnumCombo.h"

#define LOCTEXT_NAMESPACE "EnumColumnEditor"

namespace UE::ChooserEditor
{
	// Wrapper widget for EnumComboBox which will reconstruct the combo box when the Enum has changed
	class SEnumCell : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SEnumCell)
			: _RowIndex(-1)
		{}

		SLATE_ARGUMENT(FEnumColumn*, EnumColumn)
		SLATE_ATTRIBUTE(int, RowIndex);
                
		SLATE_END_ARGS()

		TSharedRef<SWidget> CreateEnumComboBox()
		{
			if (const FEnumColumn* EnumColumnPointer = EnumColumn)
			{
				if (EnumColumnPointer->InputValue.IsValid())
				{
					if (const UEnum* Enum = EnumColumnPointer->InputValue.Get<FChooserParameterEnumBase>().GetEnum())
					{
						return SNew(SEnumComboBox, Enum)
							.CurrentValue_Lambda([this]()
							{
								int Row = RowIndex.Get();
								return EnumColumn->RowValues.IsValidIndex(Row) ? static_cast<int32>(EnumColumn->RowValues[Row].Value) : 0;
							})
							.OnEnumSelectionChanged_Lambda([this](int32 EnumValue, ESelectInfo::Type)
							{
								int Row = RowIndex.Get();
								if (EnumColumn->RowValues.IsValidIndex(Row))
								{
									const FScopedTransaction Transaction(LOCTEXT("Edit RHS", "Edit Enum Value"));
									// todo: need a reference to the UChooserTable to modify
									// EnumColumn->Modify(true);
									EnumColumn->RowValues[Row].Value = static_cast<uint8>(EnumValue);
								}
							});
					}
				}
			}
			
			return SNullWidget::NullWidget;
		}

		void UpdateEnumComboBox()
		{
			ChildSlot[ CreateEnumComboBox()	];
		}

		void Construct( const FArguments& InArgs)
		{
			RowIndex = InArgs._RowIndex;
			EnumColumn = InArgs._EnumColumn;

			if (FEnumColumn* EnumColumnPointer = EnumColumn)
			{
				EnumChangedHandle = EnumColumnPointer->OnEnumChanged.AddSP(this, &SEnumCell::UpdateEnumComboBox);
			}
			
			UpdateEnumComboBox();
		}

		~SEnumCell()
		{
			if (FEnumColumn* EnumColumnPointer = EnumColumn)
			{
				EnumColumnPointer->OnEnumChanged.Remove(EnumChangedHandle);
			}
		}

	private:
		FEnumColumn* EnumColumn = nullptr;
		TAttribute<int> RowIndex;
		FDelegateHandle EnumChangedHandle;
	};

	TSharedRef<SWidget> CreateEnumColumnWidget(UChooserTable* Chooser, FChooserColumnBase* Column, int Row)
	{
		FEnumColumn* EnumColumn = static_cast<FEnumColumn*>(Column);
		
		return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().MaxWidth(95.0f)
		[
			SNew(SEnumComboBox, StaticEnum<EChooserEnumComparison>())
			.CurrentValue_Lambda([EnumColumn, Row]()
			{
				return EnumColumn->RowValues.IsValidIndex(Row) ? static_cast<int32>(EnumColumn->RowValues[Row].Comparison) : 0;
			})
			.OnEnumSelectionChanged_Lambda([Chooser, EnumColumn, Row](int32 EnumValue, ESelectInfo::Type)
			{
				if (EnumColumn->RowValues.IsValidIndex(Row))
				{
					const FScopedTransaction Transaction(LOCTEXT("Edit Comparison", "Edit Comparison Operation"));
					Chooser->Modify(true);
					EnumColumn->RowValues[Row].Comparison = static_cast<EChooserEnumComparison>(EnumValue);
				}
			})
		]
		+ SHorizontalBox::Slot()
		[
			SNew(SEnumCell).EnumColumn(EnumColumn).RowIndex(Row)
		];
	}

	

TSharedRef<SWidget> CreateEnumPropertyWidget(UObject* TransactionObject, void* Value, UClass* ContextClass)
{
	return CreatePropertyWidget<FEnumContextProperty>(TransactionObject, Value, ContextClass, GetDefault<UGraphEditorSettings>()->BytePinTypeColor);
}
	
void RegisterEnumWidgets()
{
	FObjectChooserWidgetFactories::ChooserWidgetCreators.Add(FEnumContextProperty::StaticStruct(), CreateEnumPropertyWidget);
	FChooserTableEditor::ColumnWidgetCreators.Add(FEnumColumn::StaticStruct(), CreateEnumColumnWidget);
}
	
}

#undef LOCTEXT_NAMESPACE
