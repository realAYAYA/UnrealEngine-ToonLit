// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/Views/STableRow.h"

#include "CameraCalibrationEditorCommon.h"
#include "LensFile.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

DECLARE_DELEGATE_TwoParams(FOnDataRemoved, float /** Focus */, TOptional<float> /** Possible Zoom */);

/**
* Data entry item
*/
class FLensDataListItem : public TSharedFromThis<FLensDataListItem>
{
public:
	FLensDataListItem(ULensFile* InLensFile, ELensDataCategory InCategory, int32 InSubCategoryIndex, FOnDataRemoved InOnDataRemovedCallback);

	virtual ~FLensDataListItem() = default;
	
	virtual void OnRemoveRequested() const = 0;
	virtual TSharedRef<ITableRow> MakeTreeRowWidget(const TSharedRef<STableViewBase>& InOwnerTable) = 0;
	virtual TOptional<float> GetFocus() const { return TOptional<float>(); }
	virtual int32 GetIndex() const { return INDEX_NONE; }
	virtual void EditItem() {};

	/** Lens data category of that entry */
	ELensDataCategory Category;

	/** Used to know if it's a root category or not */
	int32 SubCategoryIndex;

	/** LensFile we're editing */
	TWeakObjectPtr<ULensFile> WeakLensFile;

	/** Children of this item */
	TArray<TSharedPtr<FLensDataListItem>> Children;

	/** Delegate to call when data is removed */
	FOnDataRemoved OnDataRemovedCallback;
};

/**
 * Encoder item
 */
class FEncoderDataListItem : public FLensDataListItem
{
public:
	FEncoderDataListItem(ULensFile* InLensFile, ELensDataCategory InCategory, float InInput, int32 InIndex, FOnDataRemoved InOnDataRemovedCallback);

	virtual void OnRemoveRequested() const override;
	virtual TSharedRef<ITableRow> MakeTreeRowWidget(const TSharedRef<STableViewBase>& InOwnerTable) override;

	/** Encoder input */
	float InputValue;

	/** Identifier for this focus point */
	int32 EntryIndex;
};

/**
 * Data entry item
 */
class FFocusDataListItem : public FLensDataListItem
{
public:
	FFocusDataListItem(ULensFile* InLensFile, ELensDataCategory InCategory, int32 InSubCategoryIndex, float InFocus, FOnDataRemoved InOnDataRemovedCallback);

	virtual TSharedRef<ITableRow> MakeTreeRowWidget(const TSharedRef<STableViewBase>& InOwnerTable) override;
	virtual void OnRemoveRequested() const override;
	virtual TOptional<float> GetFocus() const override { return Focus; }

	/** Focus value of this item */
	float Focus;
};

/**
 * Zoom data entry item
 */
class FZoomDataListItem : public FLensDataListItem
{
public:
	FZoomDataListItem(ULensFile* InLensFile, ELensDataCategory InCategory, int32 InSubCategoryIndex, const TSharedRef<FFocusDataListItem> InParent, float InZoom, FOnDataRemoved InOnDataRemovedCallback);

	//~ Begin FLensDataListItem interface
	virtual TSharedRef<ITableRow> MakeTreeRowWidget(const TSharedRef<STableViewBase>& InOwnerTable) override;
	virtual void OnRemoveRequested() const override;
	virtual TOptional<float> GetFocus() const override;
	virtual void EditItem() override;
	//~ End FLensDataListItem interface

	/** Zoom value of this item */
	float Zoom = 0.0f;

	/** Focus this zoom point is associated with */
	TWeakPtr<FFocusDataListItem> WeakParent;
};

/**
 * Widget a focus point entry
 */
class SLensDataItem : public STableRow<TSharedPtr<FLensDataListItem>>
{
	SLATE_BEGIN_ARGS(SLensDataItem)
		:  _EntryLabel(FText::GetEmpty())
		,  _EntryValue(0.f)
		, _AllowRemoval(false)
		, _EditPointVisibility(EVisibility::Collapsed)
		, _AllowEditPoint(false)
		{}

		SLATE_ARGUMENT(FText, EntryLabel)

		SLATE_ARGUMENT(float, EntryValue)

		SLATE_ARGUMENT(bool, AllowRemoval)

		/** Whether Item point Visible */
		SLATE_ARGUMENT(EVisibility, EditPointVisibility)

		/** Whether Item point editable */
		SLATE_ATTRIBUTE(bool, AllowEditPoint)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable, const TSharedRef<FLensDataListItem> InItemData);

private:
	/** Edit Button Handler */
	FReply OnEditPointClicked() const;

	/** Remove Button Handler */
	FReply OnRemovePointClicked() const;

private:

	/** WeakPtr to source data item */
	TWeakPtr<FLensDataListItem> WeakItem;
};
