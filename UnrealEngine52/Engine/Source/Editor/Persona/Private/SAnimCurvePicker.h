// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STableRow.h"
#include "Animation/SmartName.h"
#include "Widgets/Input/SSearchBox.h"

class IEditableSkeleton;

DECLARE_DELEGATE_OneParam(FOnCurveNamePicked, const FSmartName& /*PickedName*/)
DECLARE_DELEGATE_RetVal_OneParam(bool, FIsCurveMarkedForExclusion, const FSmartName& /*CurveName*/)

class SAnimCurvePicker : public SCompoundWidget
{
public:

	/** Virtual destructor. */
	virtual ~SAnimCurvePicker() override;
	
	SLATE_BEGIN_ARGS(SAnimCurvePicker) {}

	SLATE_EVENT(FOnCurveNamePicked, OnCurveNamePicked)
	SLATE_EVENT(FIsCurveMarkedForExclusion, IsCurveMarkedForExclusion)
	
	SLATE_END_ARGS()

	/**
	 * Construct this widget.
	 * @param InArgs - The declaration data for this widget
	 * @param InEditableSkeleton - The skeleton from which the widget extracts curve information 
	 */
	void Construct(const FArguments& InArgs, const TSharedRef<IEditableSkeleton>& InEditableSkeleton);

private:
	/** Refresh the list of available curves */
	void RefreshListItems();

	/** Filter available curves */
	void FilterAvailableCurves();

	/** UI handlers */
	void HandleSelectionChanged(TSharedPtr<FSmartName> InItem, ESelectInfo::Type InSelectionType);
	TSharedRef<ITableRow> HandleGenerateRow(TSharedPtr<FSmartName> InItem, const TSharedRef<STableViewBase>& InOwnerTable);
	void HandleFilterTextChanged(const FText& InFilterText);

private:
	/** Delegate fired when a curve name is picked */
	FOnCurveNamePicked OnCurveNamePicked;

	/* Filter predicate to determine if curve should be excluded from the picker's curve list */
	FIsCurveMarkedForExclusion IsCurveMarkedForExclusion;
	
	/** The editable skeleton we use to grab curves from */
	TWeakPtr<IEditableSkeleton> EditableSkeleton;

	/** The names of the curves we are displaying */
	TArray<TSharedPtr<FSmartName>> CurveNames;

	struct FSmartNameKeyFuncs : public DefaultKeyFuncs<FSmartName>
	{
		static FORCEINLINE FSmartName GetSetKey(FSmartName const& Element) { return Element; }
		static FORCEINLINE uint32 GetKeyHash(FSmartName const& Key) { return GetTypeHash(Key.DisplayName); }
		static FORCEINLINE bool Matches(FSmartName const& A, FSmartName const& B) { return A == B; }
	};
	
	/** All the unique curve names we can find */
	TSet<FSmartName, FSmartNameKeyFuncs> UniqueCurveNames;

	/** The string we use to filter curve names */
	FString FilterText;

	/** The list view used to display names */
	TSharedPtr<SListView<TSharedPtr<FSmartName>>> NameListView;

	/** The search box used to filter curves */
	TSharedPtr<SSearchBox> SearchBox;

	/** The Uids of all the skeleton's curves in its mapping container */
	TArray<SmartName::UID_Type> CurveSmartNameUids;

	/** The container used to store curve names obtained from asset registry */
	TArray<FString> CurveNamesQueriedFromAssetRegistry;
};