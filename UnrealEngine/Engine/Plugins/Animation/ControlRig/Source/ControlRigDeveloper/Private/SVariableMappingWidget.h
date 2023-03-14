// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SSearchableComboBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Views/STreeView.h"
#include "Widgets/Input/SSearchBox.h"

//=====================================================================================
// UI for displaying property info
//=====================================================================================

struct FVariableMappingInfo
{
public:
	// This is the property that is the most shallow type
	// It will be Transform.Translation.X
	FName PropertyName; 
	// Display Name
	FString DisplayName;
	// List of Children
	// in theory, this actually shouldn't be active if you have children
	// but it represent each raw nonetheless
	// this is to map curve which is always float
	TArray<TSharedPtr<FVariableMappingInfo>> Children;

	/** Static function for creating a new item, but ensures that you can only have a TSharedRef to one */
	static TSharedRef<FVariableMappingInfo> Make(const FName InPathName)
	{
		return MakeShareable(new FVariableMappingInfo(InPathName));
	}

	FName GetPathName() const
	{
		return PropertyName;
	}

	FString GetDisplayName() const
	{
		return DisplayName;
	}

protected:
	/** Hidden constructor, always use Make above */
	FVariableMappingInfo(const FName InPathName)
		: PropertyName(InPathName)
	{
		FString PathString = InPathName.ToString();
		int32 Found = PathString.Find(TEXT("."), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		DisplayName = (Found != INDEX_NONE) ? PathString.RightChop(Found + 1) : PathString;
	}

	/** Hidden constructor, always use Make above */
	FVariableMappingInfo() {}
};

//////////////////////////////////////////////////////////////////////////
// SVariableMappingListRow

typedef TSharedPtr< FVariableMappingInfo > FVariableMappingInfoPtr;

DECLARE_DELEGATE_TwoParams(FOnVariableMappingChanged, const FName& /** PathName */, const FName&/** Curve Name**/)
DECLARE_DELEGATE_RetVal_OneParam(FName, FOnGetVariableMapping, const FName& /** PathName **/)
// type verified, and make sure it's not double booked
DECLARE_DELEGATE_TwoParams(FOnGetAvailableMapping, const FName& /** PathName **/, TArray<FName>& /** List of available mappings**/)
// to highlight item
DECLARE_DELEGATE_RetVal(FText&, FOnGetFilteredText)
// caller to create list
DECLARE_DELEGATE_TwoParams(FOnCreateVariableMapping, const FString&, TArray< TSharedPtr<FVariableMappingInfo> >&)
DECLARE_DELEGATE_RetVal_OneParam(bool, FOnVarOptionAvailable, const FName& /** Property Name */);

// pin exposure option
DECLARE_DELEGATE_TwoParams(FOnPinCheckStateChanged, ECheckBoxState, FName /** Property Name */);
DECLARE_DELEGATE_RetVal_OneParam(ECheckBoxState, FOnPinGetCheckState, FName /** Property Name */);
DECLARE_DELEGATE_RetVal_OneParam(bool, FOnPinIsCheckEnabled, FName /** Property Name */);

class SVariableMappingTreeRow
	: public STableRow< TSharedPtr<FVariableMappingInfoPtr> >
{
public:
	
	SLATE_BEGIN_ARGS(SVariableMappingTreeRow) {}

	/** The item for this row **/
	SLATE_ARGUMENT(FVariableMappingInfoPtr, Item)
	SLATE_EVENT(FOnVariableMappingChanged, OnVariableMappingChanged)
	SLATE_EVENT(FOnGetVariableMapping, OnGetVariableMapping)
	SLATE_EVENT(FOnGetAvailableMapping, OnGetAvailableMapping)
	SLATE_EVENT(FOnGetFilteredText, OnGetFilteredText)
	SLATE_EVENT(FOnVarOptionAvailable, OnVariableOptionAvailable);
	SLATE_EVENT(FOnPinCheckStateChanged, OnPinCheckStateChanged)
	SLATE_EVENT(FOnPinGetCheckState, OnPinGetCheckState)
	SLATE_EVENT(FOnPinIsCheckEnabled, OnPinIsEnabledCheckState)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTableView);

private:

	/** Widget used to display the list of variable option list*/
	TSharedPtr<SSearchableComboBox>	VarOptionComboBox;
	TArray< TSharedPtr< FString > >	VariableOptionList;

	/** The name and weight of the variable option*/
	FVariableMappingInfoPtr	Item;

	// Curve combo box options
	FReply OnClearButtonClicked();
	FText GetFilterText() const;

	TSharedRef<SWidget> MakeVarOptionComboWidget(TSharedPtr<FString> InItem);
	void OnVarOptionSourceChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	FText GetVarOptionComboBoxContent() const;
	FText GetVarOptionComboBoxToolTip() const;
	void OnVarOptionComboOpening();
	bool IsVarOptionEnabled() const;
	TSharedPtr<FString> GetVarOptionString(FName VarOptionName) const;

	ECheckBoxState IsPinChecked() const;
	void OnPinCheckStatusChanged(ECheckBoxState NewState);
	bool IsPinEnabled() const;

	FOnVariableMappingChanged	OnVariableMappingChanged;
	FOnGetVariableMapping		OnGetVariableMapping;
	FOnGetAvailableMapping		OnGetAvailableMapping;
	FOnGetFilteredText			OnGetFilteredText;
	FOnVarOptionAvailable		OnVariableOptionAvailable;
	FOnPinCheckStateChanged		OnPinCheckStateChanged;
	FOnPinGetCheckState			OnPinGetCheckState;
	FOnPinIsCheckEnabled		OnPinIsEnabledCheckState;
};

typedef STreeView< TSharedPtr<FVariableMappingInfo> > SVariableMappingTreeView;

class SVariableMappingWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SVariableMappingWidget)
	{}

	SLATE_EVENT(FOnVariableMappingChanged, OnVariableMappingChanged)
	SLATE_EVENT(FOnGetVariableMapping, OnGetVariableMapping)
	SLATE_EVENT(FOnGetAvailableMapping, OnGetAvailableMapping)
	SLATE_EVENT(FOnCreateVariableMapping, OnCreateVariableMapping)
	SLATE_EVENT(FOnGetFilteredText, OnGetFilteredText)
	SLATE_EVENT(FOnVarOptionAvailable, OnVariableOptionAvailable);
	SLATE_EVENT(FOnPinCheckStateChanged, OnPinCheckStateChanged)
	SLATE_EVENT(FOnPinGetCheckState, OnPinGetCheckState)
	SLATE_EVENT(FOnPinIsCheckEnabled, OnPinIsEnabledCheckState)
	SLATE_END_ARGS()

	/**
	* Slate construction function
	*
	* @param InArgs - Arguments passed from Slate
	*
	*/
	void Construct(const FArguments& InArgs/*, FSimpleMulticastDelegate& InOnPostUndo*/);

	/**
	* Filters the SListView when the user changes the search text box (NameFilterBox)
	*
	* @param SearchText - The text the user has typed
	*
	*/
	void OnFilterTextChanged(const FText& SearchText);

	/**
	* Filters the SListView when the user hits enter or clears the search box
	* Simply calls OnFilterTextChanged
	*
	* @param SearchText - The text the user has typed
	* @param CommitInfo - Not used
	*
	*/
	void OnFilterTextCommitted(const FText& SearchText, ETextCommit::Type CommitInfo);

	/**
	* Create a widget for an entry in the tree from an info
	*
	* @param InInfo - Shared pointer to the morph target we're generating a row for
	* @param OwnerTable - The table that owns this row
	*
	* @return A new Slate widget, containing the UI for this row
	*/
	TSharedRef<ITableRow> GenerateVariableMappingRow(TSharedPtr<FVariableMappingInfo> InInfo, const TSharedRef<STableViewBase>& OwnerTable);

	/**
	* Handler for the delete of retarget source
	*/
	void RefreshVariableMappingList();

private:

	/**
	* Accessor so our rows can grab the filtertext for highlighting
	*
	*/
	FText& GetFilterText() { return FilterText; }

	/** Box to filter to a specific morph target name */
	TSharedPtr<SSearchBox>	NameFilterBox;

	/** Widget used to display the list of retarget sources */
	TSharedPtr<SVariableMappingTreeView> VariableMappingTreeView;

	/** A list of variable mapping list. Used by the VariableMappingListView. */
	TArray< TSharedPtr<FVariableMappingInfo> > VariableMappingList;

	/** Current text typed into NameFilterBox */
	FText FilterText;

	/** Delegate for undo/redo transaction **/
	void PostUndo();

	// Make a single tree row widget
	TSharedRef<ITableRow> MakeTreeRowWidget(TSharedPtr<FVariableMappingInfo> InInfo, const TSharedRef<STableViewBase>& OwnerTable);

	// Get the children for the provided bone info
	void GetChildrenForInfo(TSharedPtr<FVariableMappingInfo> InInfo, TArray< TSharedPtr<FVariableMappingInfo> >& OutChildren);

	FOnGetAvailableMapping		OnGetAvailableMappingDelegate;
	FOnVariableMappingChanged	OnVariableMappingChangedDelegate;
	FOnGetVariableMapping		OnGetVariableMappingDelegate;
	FOnCreateVariableMapping	OnCreateVariableMappingDelegate;
	FOnVarOptionAvailable		OnVariableOptionAvailableDelegate;
	FOnPinCheckStateChanged		OnPinCheckStateChangedDelegate;
	FOnPinGetCheckState			OnPinGetCheckStateDelegate;
	FOnPinIsCheckEnabled		OnPinIsEnabledCheckStateDelegate;

};

