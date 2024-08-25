// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnumColumnEditor.h"
#include "EnumColumn.h"
#include "OutputEnumColumn.h"
#include "SPropertyAccessChainWidget.h"
#include "ObjectChooserWidgetFactories.h"
#include "ChooserTableEditor.h"
#include "GraphEditorSettings.h"
#include "SEnumCombo.h"
#include "TransactionCommon.h"
#include "Widgets/Input/SButton.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "EnumColumnEditor"

namespace UE::ChooserEditor
{

	
// Wrapper widget for EnumComboBox which will reconstruct the combo box when the Enum has changed
template <typename ColumnType>
class SEnumCell : public SCompoundWidget
{
public:

	DECLARE_DELEGATE_OneParam(FOnValueSet, int);
	
	SLATE_BEGIN_ARGS(SEnumCell)
	{}

	SLATE_ARGUMENT(UObject*, TransactionObject)
	SLATE_ARGUMENT(ColumnType*, EnumColumn)
	SLATE_ATTRIBUTE(int32, EnumValue);
	SLATE_EVENT(FOnValueSet, OnValueSet)
            
	SLATE_END_ARGS()

	TSharedRef<SWidget> CreateEnumComboBox()
	{
		if (const ColumnType* EnumColumnPointer = EnumColumn)
		{
			if (EnumColumnPointer->InputValue.IsValid())
			{
				if (const UEnum* Enum = EnumColumnPointer->InputValue.template Get<FChooserParameterEnumBase>().GetEnum())
				{
					return SNew(SEnumComboBox, Enum)
						.IsEnabled_Lambda([this](){ return IsEnabled(); } )
						.CurrentValue(EnumValue)
						.OnEnumSelectionChanged_Lambda([this](int32 InEnumValue, ESelectInfo::Type)
						{
							const FScopedTransaction Transaction(LOCTEXT("Edit RHS", "Edit Enum Value"));
							TransactionObject->Modify(true);
							OnValueSet.ExecuteIfBound(InEnumValue);
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

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
	{
		const UEnum* CurrentEnumSource = nullptr;
		if (EnumColumn->InputValue.IsValid())
		{
			CurrentEnumSource = EnumColumn->InputValue.template Get<FChooserParameterEnumBase>().GetEnum(); 
		}
		if (EnumSource != CurrentEnumSource)
		{
			EnumComboBorder->SetContent(CreateEnumComboBox());
			EnumSource = CurrentEnumSource;
		}
	}
    					

	void Construct( const FArguments& InArgs)
	{
		SetEnabled(InArgs._IsEnabled);
		
		SetCanTick(true);
		EnumColumn = InArgs._EnumColumn;
		TransactionObject = InArgs._TransactionObject;
		EnumValue = InArgs._EnumValue;
		OnValueSet = InArgs._OnValueSet;

		if (EnumColumn)
		{
			if (EnumColumn->InputValue.IsValid())
			{
				EnumSource = EnumColumn->InputValue.template Get<FChooserParameterEnumBase>().GetEnum();
			}
		}

		UpdateEnumComboBox();

		int Row = RowIndex.Get();

		ChildSlot
		[
			SAssignNew(EnumComboBorder, SBorder).Padding(0).BorderBackgroundColor(FLinearColor(0,0,0,0))
			[
				CreateEnumComboBox()
			]
		];
		
	}

	~SEnumCell()
	{
	}

private:
	UObject* TransactionObject = nullptr;
	ColumnType* EnumColumn = nullptr;
	const UEnum* EnumSource = nullptr;
	TSharedPtr<SBorder> EnumComboBorder;
	TAttribute<int> RowIndex;
	FDelegateHandle EnumChangedHandle;
	
	FOnValueSet OnValueSet;
	TAttribute<int32> EnumValue;
};

TSharedRef<SWidget> CreateEnumColumnWidget(UChooserTable* Chooser, FChooserColumnBase* Column, int Row)
{
	FEnumColumn* EnumColumn = static_cast<FEnumColumn*>(Column);
	
	if (Row == ColumnWidget_SpecialIndex_Fallback)
	{
		return SNullWidget::NullWidget;
	}
	if (Row == ColumnWidget_SpecialIndex_Header)
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
				SNew(SEnumCell<FEnumColumn>).TransactionObject(Chooser).EnumColumn(EnumColumn)
					.OnValueSet_Lambda([EnumColumn](int Value) { EnumColumn->TestValue = Value; })
					.EnumValue_Lambda([EnumColumn]() { return EnumColumn->TestValue; })
					.IsEnabled_Lambda([Chooser] { return !Chooser->HasDebugTarget(); })
			];
		}

		return ColumnHeaderWidget;
	}

	// create cell widget
	
	return SNew(SHorizontalBox)
    		+ SHorizontalBox::Slot().AutoWidth()
    		[
    			SNew(SBox).WidthOverride(Row < 0 ? 0 : 55)
    			[
    				SNew(SButton).ButtonStyle(FAppStyle::Get(),"FlatButton").TextStyle(FAppStyle::Get(),"RichTextBlock.Bold").HAlign(HAlign_Center)
    				.Visibility(Row < 0 ? EVisibility::Hidden : EVisibility::Visible)
					.Text_Lambda([EnumColumn, Row]()
					{
						switch (EnumColumn->RowValues[Row].Comparison)
						{
						case EEnumColumnCellValueComparison::MatchEqual:
							return LOCTEXT("CompEqual", "=");

						case EEnumColumnCellValueComparison::MatchNotEqual:
							return LOCTEXT("CompNotEqual", "!=");

						case EEnumColumnCellValueComparison::MatchAny:
							return LOCTEXT("CompAny", "Any");
						}
						return FText::GetEmpty();
					})
					.OnClicked_Lambda([EnumColumn, Chooser, Row]()
					{
						if (EnumColumn->RowValues.IsValidIndex(Row))
						{
							const FScopedTransaction Transaction(LOCTEXT("Edit Comparison", "Edit Comparison Operation"));
							Chooser->Modify(true);
							// cycle through comparison options
							EEnumColumnCellValueComparison& Comparison = EnumColumn->RowValues[Row].Comparison;
							const int32 NextComparison = (static_cast<int32>(Comparison) + 1) % static_cast<int32>(EEnumColumnCellValueComparison::Modulus);
							Comparison = static_cast<EEnumColumnCellValueComparison>(NextComparison);
						}
						return FReply::Handled();
					})
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1)
			[
				SNew(SEnumCell<FEnumColumn>).TransactionObject(Chooser).EnumColumn(EnumColumn)
					.OnValueSet_Lambda([EnumColumn, Row](int Value)
					{
						if (EnumColumn->RowValues.IsValidIndex(Row))
						{
							EnumColumn->RowValues[Row].Value = static_cast<uint8>(Value);
						}
					})
					.EnumValue_Lambda([EnumColumn, Row]()
					{
						return EnumColumn->RowValues.IsValidIndex(Row) ? static_cast<int32>(EnumColumn->RowValues[Row].Value) : 0;
					})
				.Visibility_Lambda([EnumColumn,Column, Row]()
					{
						return (EnumColumn->RowValues.IsValidIndex(Row) &&
								EnumColumn->RowValues[Row].Comparison == EEnumColumnCellValueComparison::MatchAny)
								   ? EVisibility::Collapsed
								   : EVisibility::Visible;
					})
			];
}

TSharedRef<SWidget> CreateOutputEnumColumnWidget(UChooserTable* Chooser, FChooserColumnBase* Column, int Row)
{
	FOutputEnumColumn* EnumColumn = static_cast<FOutputEnumColumn*>(Column);
	
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
				SNew(SEnumCell<FOutputEnumColumn>).TransactionObject(Chooser).EnumColumn(EnumColumn).IsEnabled(false)
				.EnumValue_Lambda([EnumColumn]() { return static_cast<int32>(EnumColumn->TestValue); })
			];
		}

		return ColumnHeaderWidget;
	}
	else if (Row == ColumnWidget_SpecialIndex_Fallback)
	{
		return 	SNew(SEnumCell<FOutputEnumColumn>).TransactionObject(Chooser).EnumColumn(EnumColumn)
        			.OnValueSet_Lambda([EnumColumn](int Value) { EnumColumn->FallbackValue.Value = Value; })
        			.EnumValue_Lambda([EnumColumn]() { return EnumColumn->FallbackValue.Value; });
	}

	// create cell widget
	
	return SNew(SEnumCell<FOutputEnumColumn>).TransactionObject(Chooser).EnumColumn(EnumColumn)
		.OnValueSet_Lambda([EnumColumn, Row](int Value)
		{
			if (EnumColumn->RowValues.IsValidIndex(Row))
			{
				EnumColumn->RowValues[Row].Value = static_cast<uint8>(Value);
			}
		})
		.EnumValue_Lambda([EnumColumn, Row]()
		{
			return EnumColumn->RowValues.IsValidIndex(Row) ? static_cast<int32>(EnumColumn->RowValues[Row].Value) : 0;
		});
}

TSharedRef<SWidget> CreateEnumPropertyWidget(bool bReadOnly, UObject* TransactionObject, void* Value, UClass* ResultBaseClass, FChooserWidgetValueChanged ValueChanged)
{
	IHasContextClass* HasContextClass = Cast<IHasContextClass>(TransactionObject);

	FEnumContextProperty* ContextProperty = reinterpret_cast<FEnumContextProperty*>(Value);

	return SNew(SPropertyAccessChainWidget).ContextClassOwner(HasContextClass).AllowFunctions(false).BindingColor("BytePinTypeColor").TypeFilter("enum")
	.PropertyBindingValue(&ContextProperty->Binding)
	.OnValueChanged(ValueChanged);
}
	
void RegisterEnumWidgets()
{
	FObjectChooserWidgetFactories::RegisterWidgetCreator(FEnumContextProperty::StaticStruct(), CreateEnumPropertyWidget);
	FObjectChooserWidgetFactories::RegisterColumnWidgetCreator(FEnumColumn::StaticStruct(), CreateEnumColumnWidget);
	FObjectChooserWidgetFactories::RegisterColumnWidgetCreator(FOutputEnumColumn::StaticStruct(), CreateOutputEnumColumnWidget);
}
	
}

#undef LOCTEXT_NAMESPACE
