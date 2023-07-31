// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ObjectMixerEditorSerializedData.h"
#include "ObjectFilter/ObjectMixerEditorObjectFilter.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SCompoundWidget.h"

class FObjectMixerEditorListFilter_Collection;
class FObjectMixerEditorMainPanel;

class OBJECTMIXEREDITOR_API SObjectMixerEditorMainPanel final : public SCompoundWidget
{

public:

	SLATE_BEGIN_ARGS(SObjectMixerEditorMainPanel)
	{}

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FObjectMixerEditorMainPanel>& InMainPanel);

	TWeakPtr<FObjectMixerEditorMainPanel> GetMainPanelModel()
	{
		return MainPanelModel;
	}

	FText GetSearchTextFromSearchInputField() const;
	FString GetSearchStringFromSearchInputField() const;
	void SetSearchStringInSearchInputField(const FString InSearchString) const;
	void ExecuteListViewSearchOnAllRows(const FString& SearchString, const bool bShouldRefreshAfterward = true);

	/**
	 * Determines the style of the tree (flat list or hierarchy)
	 */
	EObjectMixerTreeViewMode GetTreeViewMode();
	/**
	 * Determine the style of the tree (flat list or hierarchy)
	 */
	void SetTreeViewMode(EObjectMixerTreeViewMode InViewMode);

	void ToggleFilterActive(const FString& FilterName);

	/** Get the filters that affect list item visibility. Distinct from Object Filters. */
	const TArray<TSharedRef<class IObjectMixerEditorListFilter>>& GetListFilters()
	{
		return ListFilters;	
	}

	TArray<TWeakPtr<IObjectMixerEditorListFilter>> GetWeakActiveListFiltersSortedByName();

	TSet<TSharedRef<FObjectMixerEditorListFilter_Collection>> GetCurrentCollectionSelection();
	
	void RebuildCollectionSelector();

	bool RequestRemoveCollection(const FName& CollectionName);
	bool RequestDuplicateCollection(const FName& CollectionToDuplicateName, FName& DesiredDuplicateName) const;
	bool RequestRenameCollection(const FName& CollectionNameToRename, const FName& NewCollectionName);
	bool DoesCollectionExist(const FName& CollectionName) const;
	
	void OnCollectionCheckedStateChanged(bool bShouldBeChecked, FName CollectionName);
	ECheckBoxState IsCollectionChecked(FName CollectionName) const;

	virtual ~SObjectMixerEditorMainPanel() override;

private:

	/** A reference to the struct that controls this widget */
	TWeakPtr<FObjectMixerEditorMainPanel> MainPanelModel;

	/** Filters that affect list item visibility. Distinct from Object Filters. */
	TArray<TSharedRef<IObjectMixerEditorListFilter>> ListFilters;

	TSharedPtr<class SSearchBox> SearchBoxPtr;
	TSharedPtr<class SComboButton> ViewOptionsComboButton;

	TSharedRef<SWidget> GenerateToolbar();
	TSharedRef<SWidget> OnGenerateAddObjectButtonMenu() const;

	TSharedRef<SWidget> OnGenerateFilterClassMenu();
	TSharedRef<SWidget> BuildShowOptionsMenu();

	void OnSearchTextChanged(const FText& Text);

	// User Collections
	
	TSharedPtr<class SWrapBox> CollectionSelectorBox;

	/** Remove all collection filters */
	void ResetCollectionFilters();

	/** Disable all collection filters except CollectionToEnableName */
	void SetSingleCollectionSelection(const FName& CollectionToEnableName = UObjectMixerEditorSerializedData::AllCollectionName);
};
