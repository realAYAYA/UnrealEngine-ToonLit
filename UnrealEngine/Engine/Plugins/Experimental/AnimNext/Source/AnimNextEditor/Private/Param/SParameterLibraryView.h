// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class UAnimNextParameterLibrary;
class UAnimNextParameter;
struct FAssetData;
class FUICommandList;

namespace UE::AnimNext::Editor
{

enum class EFilterParameterResult : int32;
struct FParameterBindingReference;
struct FParameterLibraryViewEntry; 

class SParameterLibraryView : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_OneParam(FOnSelectionChanged, const TArray<UObject*>& /*InEntries*/);

	SLATE_BEGIN_ARGS(SParameterLibraryView) {}

	SLATE_EVENT(SParameterLibraryView::FOnSelectionChanged, OnSelectionChanged)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UAnimNextParameterLibrary* InLibrary);

private:
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	void RequestRefresh();

	void RefreshEntries();

	void RefreshFilter();

	// Bind input commands
	void BindCommands();

	// Handle modifications to the library
	void HandleLibraryModified(UAnimNextParameterLibrary* InEditorData);

	// Get the content for the context menu
	TSharedRef<SWidget> HandleGetContextContent();

	void HandleDelete();

	void HandleRename();

	bool HasValidSelection() const;

	bool HasValidSingleSelection() const;

	// Generate a row for the list view
	TSharedRef<ITableRow> HandleGenerateRow(TSharedRef<FParameterLibraryViewEntry> InEntry, const TSharedRef<STableViewBase>& InOwnerTable);

	// Handle rename after scrolling into view
	void HandleItemScrolledIntoView(TSharedRef<FParameterLibraryViewEntry> Entry, const TSharedPtr<ITableRow>& Widget);

	// Handle selection
	void HandleSelectionChanged(TSharedPtr<FParameterLibraryViewEntry> InEntry, ESelectInfo::Type InSelectionType);

private:
	friend class SParameterLibraryViewRow;
	
	TSharedPtr<SListView<TSharedRef<FParameterLibraryViewEntry>>> EntriesList;

	TArray<TSharedRef<FParameterLibraryViewEntry>> Entries;

	FText FilterText;

	TArray<TSharedRef<FParameterLibraryViewEntry>> FilteredEntries;

	UAnimNextParameterLibrary* Library = nullptr;

	TSharedPtr<FUICommandList> UICommandList = nullptr;

	FOnSelectionChanged OnSelectionChangedDelegate;

	TArray<UAnimNextParameter*> PendingSelection;

	bool bRefreshRequested = false;
};

}