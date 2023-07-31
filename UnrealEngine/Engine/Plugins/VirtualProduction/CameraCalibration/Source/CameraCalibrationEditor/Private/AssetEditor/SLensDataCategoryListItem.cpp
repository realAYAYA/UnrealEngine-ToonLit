// Copyright Epic Games, Inc. All Rights Reserved.

#include "SLensDataCategoryListItem.h"
#include "LensFile.h"
#include "SLensDataListItem.h"
#include "Widgets/Input/SButton.h"
#include "IStructureDetailsView.h"


#define LOCTEXT_NAMESPACE "LensDataCategoryListItem"



FLensDataCategoryItem::FLensDataCategoryItem(ULensFile* InLensFile, TWeakPtr<FLensDataCategoryItem> InParent, ELensDataCategory InCategory, FName InLabel)
	: Category(InCategory)
	, Label(InLabel)
	, Parent(InParent)
	, LensFile(InLensFile)
{
}

TSharedRef<ITableRow> FLensDataCategoryItem::MakeTreeRowWidget(const TSharedRef<STableViewBase>& InOwnerTable)
{
	return SNew(SLensDataCategoryItem, InOwnerTable, AsShared());
}

void SLensDataCategoryItem::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable, const TSharedRef<FLensDataCategoryItem> InItemData)
{
	WeakItem = InItemData;

	STableRow<TSharedPtr<FLensDataCategoryItem>>::Construct(
		STableRow<TSharedPtr<FLensDataCategoryItem>>::FArguments()
		.Content()
		[
			SNew(STextBlock)
			.Text(this, &SLensDataCategoryItem::GetLabelText)
		], OwnerTable);
}

FText SLensDataCategoryItem::GetLabelText() const
{
	return (FText::FromName(WeakItem.Pin()->Label));
}




#undef LOCTEXT_NAMESPACE /* LensDataViewer */



