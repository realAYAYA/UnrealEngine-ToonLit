// Copyright Epic Games, Inc. All Rights Reserved.

#include "SGameplayTagQueryGraphPin.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SScaleBox.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "GameplayTagQueryGraphPin"

void SGameplayTagQueryGraphPin::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	TagQuery = MakeShareable(new FGameplayTagQuery());
	SGraphPin::Construct( SGraphPin::FArguments(), InGraphPinObj );
}

TSharedRef<SWidget>	SGameplayTagQueryGraphPin::GetDefaultValueWidget()
{
	ParseDefaultValueData();

	//Create widget
	return SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SAssignNew( ComboButton, SComboButton )
			.OnGetMenuContent(this, &SGameplayTagQueryGraphPin::GetListContent)
			.ContentPadding( FMargin( 2.0f, 2.0f ) )
			.Visibility( this, &SGraphPin::GetDefaultValueVisibility )
			.ButtonContent()
			[
				SNew( STextBlock )
				.Text( LOCTEXT("GameplayTagQueryWidget_Edit", "Edit") )
			]
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			QueryDesc()
		];
}

void SGameplayTagQueryGraphPin::ParseDefaultValueData()
{
	FString const TagQueryString = GraphPinObj->GetDefaultAsString();

	FProperty* const TQProperty = FindFProperty<FProperty>(UEditableGameplayTagQuery::StaticClass(), TEXT("TagQueryExportText_Helper"));
	if (TQProperty)
	{
		FGameplayTagQuery* const TQ = TagQuery.Get();
		TQProperty->ImportText_Direct(*TagQueryString, TQ, nullptr, 0, GLog);
	}
}

TSharedRef<SWidget> SGameplayTagQueryGraphPin::GetListContent()
{
	EditableQueries.Empty();
	EditableQueries.Add(SGameplayTagQueryWidget::FEditableGameplayTagQueryDatum(GraphPinObj->GetOwningNode(), TagQuery.Get(), &TagQueryExportText));

	return SNew( SVerticalBox )
		+SVerticalBox::Slot()
		.AutoHeight()
		.MaxHeight( 400 )
		[
			SNew(SScaleBox)
			.HAlign(EHorizontalAlignment::HAlign_Left)
			.VAlign(EVerticalAlignment::VAlign_Top)
			.StretchDirection(EStretchDirection::DownOnly)
			.Stretch(EStretch::ScaleToFit)
			.Content()
			[
				SNew(SGameplayTagQueryWidget, EditableQueries, nullptr)
				.OnQueryChanged(this, &SGameplayTagQueryGraphPin::OnQueryChanged)
				.Visibility( this, &SGraphPin::GetDefaultValueVisibility )
				.AutoSave(true)
			]
		];
}

void SGameplayTagQueryGraphPin::OnQueryChanged()
{
	if (TagQueryExportText != GraphPinObj->GetDefaultAsString())
	{
		// Set Pin Data
		const FScopedTransaction Transaction(NSLOCTEXT("GraphEditor", "ChangePinValue", "Change Pin Value"));
		GraphPinObj->Modify();
		GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, TagQueryExportText);
	}
	QueryDescription = TagQuery->GetDescription();
}

TSharedRef<SWidget> SGameplayTagQueryGraphPin::QueryDesc()
{
	QueryDescription = TagQuery->GetDescription();

	return SNew(STextBlock)
		.Text(this, &SGameplayTagQueryGraphPin::GetQueryDescText)
		.AutoWrapText(true);
}

FText SGameplayTagQueryGraphPin::GetQueryDescText() const
{
	return FText::FromString(QueryDescription);
}

#undef LOCTEXT_NAMESPACE
