// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SlateFwd.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STableRow.h"

class SComboButton;
class SSearchBox;
class USkeleton;

DECLARE_DELEGATE_OneParam(FOnCurveSelectionChanged, const FString&);
DECLARE_DELEGATE_OneParam(FOnCurveNamePicked, const FString&)

DECLARE_DELEGATE_RetVal(FString, FOnGetSelectedCurve);
DECLARE_DELEGATE_RetVal(USkeleton*, FOnGetSkeleton);


namespace UE::MLDeformer
{
	/**
	 * The curve picker widget for the ML Deformer editor.
	 */
	class MLDEFORMERFRAMEWORKEDITOR_API SCurvePickerWidget
		: public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SCurvePickerWidget) {}
		SLATE_EVENT(FOnCurveNamePicked, OnCurveNamePicked)
		SLATE_EVENT(FOnGetSkeleton, OnGetSkeleton)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs);

		TSharedPtr<SSearchBox> GetFilterTextWidget() const { return SearchBox; }

	private:
		void RefreshListItems();
		void FilterAvailableCurves();
		void HandleSelectionChanged(TSharedPtr<FString> InItem, ESelectInfo::Type InSelectionType);
		void HandleFilterTextChanged(const FText& InFilterText);
		TSharedRef<ITableRow> HandleGenerateRow(TSharedPtr<FString> InItem, const TSharedRef<STableViewBase>& InOwnerTable);

	private:
		/** Delegate fired when a curve name is picked. */
		FOnCurveNamePicked OnCurveNamePicked;

		/** Provide us with a skeleton. */
		FOnGetSkeleton OnGetSkeleton;

		/** The search filter box. */
		TSharedPtr<SSearchBox> SearchBox;

		/** The skeleton to get the curves from. */
		TObjectPtr<USkeleton> Skeleton;

		/** The names of the curves we are displaying. */
		TArray<TSharedPtr<FString>> CurveNames;

		/** All the unique curve names we can find. */
		TSet<FString> UniqueCurveNames;

		/** The string we use to filter curve names. */
		FString FilterText;

		/** The list view used to display names. */
		TSharedPtr<SListView<TSharedPtr<FString>>> NameListView;
	};

	/**
	 * The curve selection widget for the ML Deformer.
	 */
	class SCurveSelectionWidget
		: public SCompoundWidget
	{
	public: 
		SLATE_BEGIN_ARGS(SCurveSelectionWidget) {}
			SLATE_EVENT(FOnCurveSelectionChanged, OnCurveSelectionChanged);
			SLATE_EVENT(FOnGetSelectedCurve, OnGetSelectedCurve);
			SLATE_EVENT(FOnGetSkeleton, OnGetSkeleton)
		SLATE_END_ARGS();

		void Construct(const FArguments& InArgs);

	private: 
		TSharedRef<SWidget> CreateSkeletonWidgetMenu();
		void OnSelectionChanged(const FString& CurveName);
		USkeleton* GetSkeleton() const;
		FText GetCurrentCurveName() const;
		FText GetFinalToolTip() const;

	private:
		TSharedPtr<SComboButton> CurvePickerButton;
		FOnCurveSelectionChanged OnCurveSelectionChanged;
		FOnGetSelectedCurve OnGetSelectedCurve;
		FOnGetSkeleton OnGetSkeleton;
		FText SuppliedToolTip;
		FText SelectedCurve;
	};
}	// namespace UE::MLDeformer
