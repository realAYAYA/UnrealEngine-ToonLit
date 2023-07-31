// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

#include "CameraCalibrationEditorCommon.h"
#include "LensFile.h"
#include "SLensFilePanel.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Views/STreeView.h"


class FLensDataListItem;


/**
 * Data category item
 */
class FLensDataCategoryItem : public TSharedFromThis<FLensDataCategoryItem>
{
public:
	FLensDataCategoryItem(ULensFile* InLensFile, TWeakPtr<FLensDataCategoryItem> Parent, ELensDataCategory InCategory, FName InLabel);
	virtual ~FLensDataCategoryItem() = default;

	/** Makes the widget for its associated row */
	TSharedRef<ITableRow> MakeTreeRowWidget(const TSharedRef<STableViewBase>& InOwnerTable);

	virtual int32 GetParameterIndex() const { return INDEX_NONE; }

public:

	/** Category this item is associated with */
	ELensDataCategory Category;

	/** Label of this category */
	FName Label;

	/** WeakPtr to parent of this item */
	TWeakPtr<FLensDataCategoryItem> Parent;

	/** Children of this category */
	TArray<TSharedPtr<FLensDataCategoryItem>> Children;

	/** LensFile being edited */
	TWeakObjectPtr<ULensFile> LensFile;
};

/**
 * Data category row widget
 */
class SLensDataCategoryItem : public STableRow<TSharedPtr<FLensDataCategoryItem>>
{
	SLATE_BEGIN_ARGS(SLensDataCategoryItem) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable, const TSharedRef<FLensDataCategoryItem> InItemData);

private:

	/** Returns the label of this row */
	FText GetLabelText() const;

private:

	/** WeakPtr to source data item */
	TWeakPtr<FLensDataCategoryItem> WeakItem;
};

/**
 * Distortion parameters category
 */
class FDistortionParametersCategoryItem : public FLensDataCategoryItem
{
public:
	FDistortionParametersCategoryItem(ULensFile* LensFile, TWeakPtr<FLensDataCategoryItem> Parent, ELensDataCategory InCategory, FName InLabel, int32 InParameterIndex)
		: FLensDataCategoryItem(LensFile, Parent, InCategory, InLabel)
		, ParameterIndex(InParameterIndex)
	{}

	//~ Begin FLensDataCategoryItem interface
	virtual int32 GetParameterIndex() const override { return ParameterIndex; }
	//~ End FLensDataCategoryItem interface

public:

	/** Used to identify which distortion parameter this represents */
	int32 ParameterIndex = 0;
};

/**
 * Focal length parameters
 */
class FFocalLengthCategoryItem : public FLensDataCategoryItem
{
public:
	FFocalLengthCategoryItem(ULensFile* LensFile, TWeakPtr<FLensDataCategoryItem> Parent, ELensDataCategory InCategory, FName InLabel, int32 InParameterIndex)
		: FLensDataCategoryItem(LensFile, Parent, InCategory, InLabel)
		, ParameterIndex(InParameterIndex)
	{
		check(InParameterIndex >= 0 && InParameterIndex < 2);
	}

	//~ Begin FLensDataCategoryItem interface
	virtual int32 GetParameterIndex() const { return ParameterIndex; }
	//~ End FLensDataCategoryItem interface

public:

	/**
	 * Used to identify which image center parameter this represents
	 * 0: Fx
	 * 1: Fy
	 */
	int32 ParameterIndex = 0;
};

/**
 * Image Center parameters
 */
class FImageCenterCategoryItem : public FLensDataCategoryItem
{
public:
	FImageCenterCategoryItem(ULensFile* LensFile, TWeakPtr<FLensDataCategoryItem> Parent, ELensDataCategory InCategory, FName InLabel, int32 InParameterIndex)
		: FLensDataCategoryItem(LensFile, Parent, InCategory, InLabel)
		, ParameterIndex(InParameterIndex)
	{
		check(InParameterIndex >= 0 && InParameterIndex < 2);
	}

	//~ Begin FLensDataCategoryItem interface
	virtual int32 GetParameterIndex() const { return ParameterIndex; }
	//~ End FLensDataCategoryItem interface

public:

	/** 
	 * Used to identify which image center parameter this represents 
	 * 0: Cx
	 * 1: Cy
	 */
	int32 ParameterIndex = 0;
};


/**
 * Nodal Offset parameters
 */
class FNodalOffsetCategoryItem : public FLensDataCategoryItem
{
public:
	FNodalOffsetCategoryItem(ULensFile* LensFile, TWeakPtr<FLensDataCategoryItem> Parent, ELensDataCategory InCategory, FName InLabel, int32 InParameterIndex, EAxis::Type InAxis)
		: FLensDataCategoryItem(LensFile, Parent, InCategory, InLabel)
		, ParameterIndex(InParameterIndex)
		, Axis(InAxis)
	{
		check(InParameterIndex >= 0 && InParameterIndex < 2);
	}

	//~ Begin FLensDataCategoryItem interface
	virtual int32 GetParameterIndex() const { return ParameterIndex; }
	//~ End FLensDataCategoryItem interface

public:

	/**
	 * Used to identify which image center parameter this represents
	 * 0: Location
	 * 1: Rotation
	 */
	int32 ParameterIndex = 0;

	/** Axis for the given parameter */
	EAxis::Type Axis;
};

