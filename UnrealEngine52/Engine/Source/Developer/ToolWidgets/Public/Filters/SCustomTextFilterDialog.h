// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Filters/CustomTextFilters.h"

class SColorBlock;
class SEditableTextBox;

class TOOLWIDGETS_API SCustomTextFilterDialog : public SCompoundWidget
{
public:

	DECLARE_DELEGATE_TwoParams(FOnCreateFilter, const FCustomTextFilterData& /* InFilterData */, bool /* bApplyFilter */);
	DECLARE_DELEGATE(FOnDeleteFilter);
	DECLARE_DELEGATE_OneParam(FOnModifyFilter, const FCustomTextFilterData& /* InFilterData */);
	DECLARE_DELEGATE(FOnCancelClicked);
	DECLARE_DELEGATE_OneParam(FOnGetFilterLabels, TArray<FText> & /* FilterNames */);

	SLATE_BEGIN_ARGS(SCustomTextFilterDialog) {}
	
    	/** The filter that this dialog is creating/editing */
    	SLATE_ARGUMENT(FCustomTextFilterData, FilterData)

		/** True if we are editing an existing filter, false if we are creating a new one */
		SLATE_ARGUMENT(bool, InEditMode)
		
		/** Delegate for when the Create button is clicked */
		SLATE_EVENT(FOnCreateFilter, OnCreateFilter)
		
		/** Delegate for when the Delete button is clicked */
		SLATE_EVENT(FOnDeleteFilter, OnDeleteFilter)
		
		/** Delegate for when the Cancel button is clicked */
		SLATE_EVENT(FOnCancelClicked, OnCancelClicked)

		/** Delegate for when the Modify Filter button is clicked */
        SLATE_EVENT(FOnModifyFilter, OnModifyFilter)

		/** Delegate to get all existing filter labels to check for duplicates */
		SLATE_EVENT(FOnGetFilterLabels, OnGetFilterLabels)
    
    SLATE_END_ARGS()
    	
    /** Constructs this widget with InArgs */
    void Construct( const FArguments& InArgs );

protected:

	/* Handler for when the color block is clicked to open the color picker */
	FReply ColorBlock_OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);

	void HandleColorValueChanged(FLinearColor NewValue);
	
	FReply OnDeleteButtonClicked() const;
	
	FReply OnCreateFilterButtonClicked(bool bApplyFilter) const;
	
	FReply OnCancelButtonClicked() const;

	FReply OnModifyButtonClicked() const;

	bool CheckFilterValidity() const;
	
protected:

	/* True if we are editing a filter, false if we are creating a new filter */
	bool bInEditMode;

	/* The current filter data we are editing */
	FCustomTextFilterData FilterData;
	
	/* The initial, unedited filter data we were provided */
	FCustomTextFilterData InitialFilterData;
	
	FOnCreateFilter OnCreateFilter;
	
	FOnDeleteFilter OnDeleteFilter;
	
	FOnCancelClicked OnCancelClicked;

	FOnModifyFilter OnModifyFilter;

	FOnGetFilterLabels OnGetFilterLabels;

	/* The color block widget that edits the filter color */
	TSharedPtr<SColorBlock> ColorBlock;

	/* The widget that edits the filter label */
	TSharedPtr<SEditableTextBox> FilterLabelTextBox;
};