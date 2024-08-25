// Copyright Epic Games, Inc. All Rights Reserved.

#include "SUSDReferencesList.h"

#include "SUSDStageEditorStyle.h"

#include "Styling/AppStyle.h"

#if USE_USD_SDK

#define LOCTEXT_NAMESPACE "USDReferencesList"

namespace UsdReferencesListConstants
{
	const FMargin RowPadding(6.0f, 2.5f, 2.0f, 2.5f);

	const TCHAR* NormalFont = TEXT("PropertyWindow.NormalFont");
}

void SUsdReferenceRow::Construct(const FArguments& InArgs, TSharedPtr<FUsdReference> InReference, const TSharedRef<STableViewBase>& OwnerTable)
{
	Reference = InReference;

	SMultiColumnTableRow<TSharedPtr<FUsdReference>>::Construct(SMultiColumnTableRow<TSharedPtr<FUsdReference>>::FArguments(), OwnerTable);
}

TSharedRef<SWidget> SUsdReferenceRow::GenerateWidgetForColumn(const FName& ColumnName)
{
	TSharedRef<SWidget> ColumnWidget = SNullWidget::NullWidget;

	// clang-format off
	if (ColumnName == TEXT("AssetPath"))
	{
		SAssignNew(ColumnWidget, STextBlock)
		.Text(FText::FromString(Reference->AssetPath.IsEmpty() ? TEXT("(internal reference)") : Reference->AssetPath))
		.Font(FAppStyle::GetFontStyle(UsdReferencesListConstants::NormalFont));
	}

	if (ColumnName == TEXT("PrimPath"))
	{
		SAssignNew(ColumnWidget, STextBlock)
		.Text(FText::FromString(Reference->PrimPath.IsEmpty() ? TEXT("(default prim)") : Reference->PrimPath))
		.Font(FAppStyle::GetFontStyle(UsdReferencesListConstants::NormalFont));
	}

	return SNew(SBox)
		.HeightOverride(FUsdStageEditorStyle::Get()->GetFloat("UsdStageEditor.ListItemHeight"))
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(UsdReferencesListConstants::RowPadding)
			.AutoWidth()
			[
				ColumnWidget
			]
		];
	// clang-format on
}

void SUsdReferencesList::Construct(const FArguments& InArgs)
{
	// clang-format off
	SAssignNew(HeaderRowWidget, SHeaderRow)

	+SHeaderRow::Column(FName(TEXT("AssetPath")))
	.DefaultLabel(LOCTEXT("ReferencedPath", "Referenced layers"))
	.FillWidth(100.f)

	+SHeaderRow::Column(FName(TEXT("PrimPath")))
	.DefaultLabel(LOCTEXT("ReferencedPrim", "Referenced prims"))
	.FillWidth(100.f);

	SListView::Construct
	(
		SListView::FArguments()
		.ListItemsSource(&ViewModel.References)
		.OnGenerateRow(this, &SUsdReferencesList::OnGenerateRow)
		.HeaderRow(HeaderRowWidget)
	);

	SetVisibility(EVisibility::Collapsed); // Start hidden until SetPrimPath displays us
	// clang-format on
}

TSharedRef<ITableRow> SUsdReferencesList::OnGenerateRow(TSharedPtr<FUsdReference> InDisplayNode, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SUsdReferenceRow, InDisplayNode, OwnerTable);
}

void SUsdReferencesList::SetPrimPath(const UE::FUsdStageWeak& UsdStage, const TCHAR* PrimPath)
{
	ViewModel.UpdateReferences(UsdStage, PrimPath);

	SetVisibility(ViewModel.References.Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed);

	RequestListRefresh();
}

#undef LOCTEXT_NAMESPACE

#endif	  // #if USE_USD_SDK
