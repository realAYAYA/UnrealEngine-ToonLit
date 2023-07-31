// Copyright Epic Games, Inc. All Rights Reserved.

#include "SLensDataListItem.h"

#include "LensFile.h"
#include "ScopedTransaction.h"
#include "SLensDataEditPointDialog.h"
#include "UI/CameraCalibrationEditorStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "SCameraCalibrationRemovePointDialog.h"

#define LOCTEXT_NAMESPACE "LensDataListItem"

FLensDataListItem::FLensDataListItem(ULensFile* InLensFile, ELensDataCategory InCategory, int32 InSubCategoryIndex, FOnDataRemoved InOnDataRemovedCallback)
	: Category(InCategory)
	, SubCategoryIndex(InSubCategoryIndex)
	, WeakLensFile(InLensFile)
	, OnDataRemovedCallback(InOnDataRemovedCallback)
{
}

FEncoderDataListItem::FEncoderDataListItem(ULensFile* InLensFile, ELensDataCategory InCategory, float InInputValue, int32 InIndex, FOnDataRemoved InOnDataRemovedCallback)
	: FLensDataListItem(InLensFile, InCategory, INDEX_NONE, InOnDataRemovedCallback)
	, InputValue(InInputValue)
	, EntryIndex()
{
}

void FEncoderDataListItem::OnRemoveRequested() const
{
	if (ULensFile* LensFilePtr = WeakLensFile.Get())
	{
		FScopedTransaction Transaction(LOCTEXT("RemoveEncoderPointTransaction", "Remove encoder point"));
		LensFilePtr->Modify();

		//Pass encoder mapping raw input value as focus to remove it
		LensFilePtr->RemoveFocusPoint(Category, InputValue);
		OnDataRemovedCallback.ExecuteIfBound(InputValue, TOptional<float>());
	}
}

TSharedRef<ITableRow> FEncoderDataListItem::MakeTreeRowWidget(const TSharedRef<STableViewBase>& InOwnerTable)
{
	return SNew(SLensDataItem, InOwnerTable, AsShared())
		.EntryLabel(LOCTEXT("EncoderLabel", "Input:"))
		.EntryValue(InputValue)
		.AllowRemoval(true);
}

FFocusDataListItem::FFocusDataListItem(ULensFile* InLensFile, ELensDataCategory InCategory, int32 InSubCategoryIndex, float InFocus, FOnDataRemoved InOnDataRemovedCallback)
	: FLensDataListItem(InLensFile, InCategory, InSubCategoryIndex, InOnDataRemovedCallback)
	, Focus(InFocus)
{
}

TSharedRef<ITableRow> FFocusDataListItem::MakeTreeRowWidget(const TSharedRef<STableViewBase>& InOwnerTable)
{
	return SNew(SLensDataItem, InOwnerTable, AsShared())
		.EntryLabel(LOCTEXT("FocusLabel", "Focus: "))
		.EntryValue(Focus)
		.AllowRemoval(SubCategoryIndex == INDEX_NONE);
}

void FFocusDataListItem::OnRemoveRequested() const
{	
	if (ULensFile* LensFilePtr = WeakLensFile.Get())
	{
		const FBaseLensTable* const LinkDataTable = LensFilePtr->GetDataTable(Category);
		if (!ensure(LinkDataTable))
		{
			return;	
		}
		
		if (LinkDataTable->HasLinkedFocusValues(Focus))
		{
			SCameraCalibrationRemovePointDialog::OpenWindow(
				LensFilePtr,
				Category,
				FSimpleDelegate::CreateLambda([this]()
				{
					OnDataRemovedCallback.ExecuteIfBound(Focus, TOptional<float>());
				}),
				Focus);
		}
		else
		{
			FScopedTransaction Transaction(LOCTEXT("RemoveFocusPointsTransaction", "Remove Focus Points"));
			LensFilePtr->Modify();

			LensFilePtr->RemoveFocusPoint(Category, Focus);
			OnDataRemovedCallback.ExecuteIfBound(Focus, TOptional<float>());
		}
	}
}

FZoomDataListItem::FZoomDataListItem(ULensFile* InLensFile, ELensDataCategory InCategory, int32 InSubCategoryIndex, const TSharedRef<FFocusDataListItem> InParent, float InZoom, FOnDataRemoved InOnDataRemovedCallback)
	: FLensDataListItem(InLensFile, InCategory, InSubCategoryIndex, InOnDataRemovedCallback)
	, Zoom(InZoom)
	, WeakParent(InParent)
{
}

TSharedRef<ITableRow> FZoomDataListItem::MakeTreeRowWidget(const TSharedRef<STableViewBase>& InOwnerTable)
{
	return SNew(SLensDataItem, InOwnerTable, SharedThis(this))
		.EntryLabel(LOCTEXT("ZoomLabel", "Zoom: "))
		.EntryValue(Zoom)
		.AllowRemoval(SubCategoryIndex == INDEX_NONE )
		.EditPointVisibility(EVisibility::Visible)
		.AllowEditPoint(MakeAttributeLambda([this]() { return SubCategoryIndex == INDEX_NONE; }));
}

void FZoomDataListItem::OnRemoveRequested() const
{
	if (ULensFile* LensFilePtr = WeakLensFile.Get())
	{
		if(TSharedPtr<FFocusDataListItem> ParentItem = WeakParent.Pin())
		{
			const FBaseLensTable* const LinkDataTable = LensFilePtr->GetDataTable(Category);
			if (!ensure(LinkDataTable))
			{
				return;	
			}
			if (LinkDataTable->HasLinkedZoomValues(ParentItem->Focus, Zoom))
			{
				SCameraCalibrationRemovePointDialog::OpenWindow(
					LensFilePtr,
					Category,
					FSimpleDelegate::CreateLambda([this, ParentItem]()
					{
						OnDataRemovedCallback.ExecuteIfBound(ParentItem->Focus, Zoom);
					}),
					ParentItem->Focus,
					Zoom);
			}
			else
			{
				FScopedTransaction Transaction(LOCTEXT("RemoveZoomPointTransaction", "Remove Zoom Point"));
				LensFilePtr->Modify();
				LensFilePtr->RemoveZoomPoint(Category, ParentItem->Focus, Zoom);
				OnDataRemovedCallback.ExecuteIfBound(ParentItem->Focus, Zoom);
			}
		}
	}
}

TOptional<float> FZoomDataListItem::GetFocus() const
{
	if (TSharedPtr<FFocusDataListItem> ParentItem = WeakParent.Pin())
	{
		return ParentItem->GetFocus();
	}

	return TOptional<float>();
}

void FZoomDataListItem::EditItem()
{
	ULensFile* LensFilePtr = WeakLensFile.Get();
	if (!ensure(LensFilePtr))
	{
		return;
	}

	if (!ensure(GetFocus().IsSet()))
	{
		return;
	}
	
	switch (Category)
	{
		case ELensDataCategory::Zoom:
		{
			LensDataEditPointDialog::OpenDialog<FFocalLengthInfo>(LensFilePtr, GetFocus().GetValue(), Zoom, LensFilePtr->FocalLengthTable);
			break;
		}
		case ELensDataCategory::ImageCenter:
		{
			LensDataEditPointDialog::OpenDialog<FImageCenterInfo>(LensFilePtr, GetFocus().GetValue(), Zoom, LensFilePtr->ImageCenterTable);
			break;
		}
		case ELensDataCategory::Distortion:
		{
			LensDataEditPointDialog::OpenDialog<FDistortionInfo>(LensFilePtr, GetFocus().GetValue(), Zoom, LensFilePtr->DistortionTable);
			break;
		}
		case ELensDataCategory::NodalOffset:
		{
			LensDataEditPointDialog::OpenDialog<FNodalPointOffset>(LensFilePtr, GetFocus().GetValue(), Zoom, LensFilePtr->NodalOffsetTable);
			break;
		}
		case ELensDataCategory::STMap:
		{
			LensDataEditPointDialog::OpenDialog<FSTMapInfo>(LensFilePtr, GetFocus().GetValue(), Zoom, LensFilePtr->STMapTable);
			break;
		}
	}
}

void SLensDataItem::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable, const TSharedRef<FLensDataListItem> InItemData)
{
	WeakItem = InItemData;

	const float EntryValue = InArgs._EntryValue;

	STableRow<TSharedPtr<FLensDataListItem>>::Construct(
		STableRow<TSharedPtr<FLensDataListItem>>::FArguments()
		.Content()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(5.0f, 5.0f)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(InArgs._EntryLabel)
			]
			+ SHorizontalBox::Slot()
			.Padding(5.0f, 5.0f)
			.FillWidth(1.0f)
			.HAlign(HAlign_Left)
			[
				SNew(STextBlock)
				.Text_Lambda([EntryValue](){ return FText::AsNumber(EntryValue); })
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.OnClicked(this, &SLensDataItem::OnEditPointClicked)
				.IsEnabled(InArgs._AllowEditPoint)
				.Visibility(InArgs._EditPointVisibility)
				.ButtonStyle(FAppStyle::Get(), "FlatButton")
				.ToolTipText(LOCTEXT("EditLensDataPoint", "Edit the value at this point"))
				[
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush("Icons.Edit"))
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.IsEnabled(InArgs._AllowRemoval)
				.OnClicked(this, &SLensDataItem::OnRemovePointClicked)
				.ButtonStyle(FAppStyle::Get(), "FlatButton")
				.ToolTipText(LOCTEXT("RemoveLensDataPoint", "Remove this point"))
				[
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush("Icons.Delete"))
				]
			]
		], OwnerTable);
}

FReply SLensDataItem::OnRemovePointClicked() const
{
	if (TSharedPtr<FLensDataListItem> Item = WeakItem.Pin())
	{
		Item->OnRemoveRequested();
	}

	return FReply::Handled();
}

FReply SLensDataItem::OnEditPointClicked() const
{
	if (TSharedPtr<FLensDataListItem> Item = WeakItem.Pin())
	{
		Item->EditItem();
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE /* LensDataListItem */
