// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"
#include "AssetRegistry/AssetData.h"
#include "Templates/SharedPointer.h"

class URigVMGraph;
class FUICommandList;
class UAnimNextRigVMAssetEntry;
class UAnimNextRigVMAssetEditorData;

namespace UE::AnimNext::Editor
{

struct FRigVMAssetViewEntry; 

class SRigVMAssetView : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_OneParam(FOnSelectionChanged, const TArray<UObject*>& /*InEntries*/);

	DECLARE_DELEGATE_OneParam(FOnOpenGraph, URigVMGraph* /*InGraph*/);

	DECLARE_DELEGATE_OneParam(FOnDeleteEntries, const TArray<UAnimNextRigVMAssetEntry*>& /*InEntries*/);
	
	SLATE_BEGIN_ARGS(SRigVMAssetView) {}

	SLATE_EVENT(SRigVMAssetView::FOnSelectionChanged, OnSelectionChanged)
	
	SLATE_EVENT(SRigVMAssetView::FOnOpenGraph, OnOpenGraph)

	SLATE_EVENT(SRigVMAssetView::FOnDeleteEntries, OnDeleteEntries)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UAnimNextRigVMAssetEditorData* InEditorData);

	using FCategoryWidgetFactoryFunction = TUniqueFunction<TSharedRef<SWidget>(UAnimNextRigVMAssetEditorData*)>;

	// Register a factory function used to generate widgets for a category
	static void RegisterCategoryFactory(FName InCategory, FCategoryWidgetFactoryFunction&& InFunction);
	static void UnregisterCategoryFactory(FName InCategory);

private:
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	void RequestRefresh();

	void RefreshEntries();

	void RefreshFilter();

	// Bind input commands
	void BindCommands();

	// Handle modifications to the block
	void HandleBlockModified(UAnimNextRigVMAssetEditorData* InEditorData);

	// Get the content for the context menu
	TSharedRef<SWidget> HandleGetContextContent();

	void HandleDelete();

	void HandleRename();

	bool HasValidSelection() const;

	bool HasValidSingleSelection() const;

	// Generate a row for the list view
	TSharedRef<ITableRow> HandleGenerateRow(TSharedRef<FRigVMAssetViewEntry> InEntry, const TSharedRef<STableViewBase>& InOwnerTable);

	void HandleGetChildren(TSharedRef<FRigVMAssetViewEntry> InEntry, TArray<TSharedRef<FRigVMAssetViewEntry>>& OutChildren);

	// Handle rename after scrolling into view
	void HandleItemScrolledIntoView(TSharedRef<FRigVMAssetViewEntry> Entry, const TSharedPtr<ITableRow>& Widget);

	// Handle selection
	void HandleSelectionChanged(TSharedPtr<FRigVMAssetViewEntry> InEntry, ESelectInfo::Type InSelectionType);

	TSharedRef<FRigVMAssetViewEntry> GetCategoryEntry(FName InCategoryName);
	
private:
	friend class SRigVMAssetViewRow;
	
	TArray<TSharedRef<FRigVMAssetViewEntry>> Categories;

	TSharedPtr<STreeView<TSharedRef<FRigVMAssetViewEntry>>> EntriesList;

	TArray<TSharedRef<FRigVMAssetViewEntry>> Entries;

	FText FilterText;

	TArray<TSharedRef<FRigVMAssetViewEntry>> FilteredEntries;

	UAnimNextRigVMAssetEditorData* EditorData = nullptr;

	TSharedPtr<FUICommandList> UICommandList = nullptr;

	FOnSelectionChanged OnSelectionChangedDelegate;

	FOnOpenGraph OnOpenGraphDelegate;

	FOnDeleteEntries OnDeleteEntriesDelegate;

	FAssetData AssetData;

	TArray<UObject*> PendingSelection;

	bool bRefreshRequested = false;

	static TMap<FName, FCategoryWidgetFactoryFunction> CategoryFactories;
};

}