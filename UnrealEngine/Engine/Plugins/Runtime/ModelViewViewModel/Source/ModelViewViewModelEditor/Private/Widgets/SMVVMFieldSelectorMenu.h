// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Types/MVVMBindingMode.h"
#include "Types/MVVMBindingSource.h"
#include "Types/MVVMLinkedPinValue.h"

#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SCompoundWidget.h"

namespace ESelectInfo { enum Type : int; }
class ITableRow;
class SSearchBox;
class SReadOnlyHierarchyView;
class STableViewBase;
template <typename ItemType> class SListView;
template <typename ItemType> class STreeView;
namespace UE::MVVM {class SSourceBindingList; }

class UWidgetBlueprint;

namespace UE::MVVM
{

struct FFieldSelectionContext
{
	EMVVMBindingMode BindingMode = EMVVMBindingMode::OneWayToDestination;
	const FProperty* AssignableTo = nullptr;
	TOptional<FBindingSource> FixedBindingSource;
	bool bAllowWidgets = true;
	bool bAllowViewModels = true;
	bool bAllowConversionFunctions = true;
	bool bReadable = true;
	bool bWritable = true;
};

DECLARE_DELEGATE_OneParam(FOnLinkedValueSelectionChanged, FMVVMLinkedPinValue);

class SFieldSelectorMenu : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_RetVal(FFieldSelectionContext, FOnGetFieldSelectionContext);

	SLATE_BEGIN_ARGS(SFieldSelectorMenu){}
		SLATE_ARGUMENT(TOptional<FMVVMLinkedPinValue>, CurrentSelected)
		SLATE_EVENT(FOnLinkedValueSelectionChanged, OnSelectionChanged)
		SLATE_EVENT(FSimpleDelegate, OnMenuCloseRequested)
		SLATE_ARGUMENT(FFieldSelectionContext, SelectionContext)
		SLATE_ARGUMENT_DEFAULT(bool, IsBindingToEvent) { false };
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const UWidgetBlueprint* InWidgetBlueprint);

	TSharedRef<SWidget> GetWidgetToFocus() const;

private:
	struct FConversionFunctionItem
	{
		FString GetCategoryName() { return CategoryPath.Num() > 0 ? CategoryPath.Last() : FString(); }

		TArray<FString> CategoryPath;
		TArray<FString> SearchKeywords;
		const UFunction* Function = nullptr;
		TArray<TSharedPtr<FConversionFunctionItem>> Children;
		int32 NumFunctions = 0;
	};

private:
	void SetPropertyPathSelection(const FMVVMBlueprintPropertyPath& SelectedPath);
	void SetConversionFunctionSelection(const UFunction* SelectedFunction);

	TSharedRef<SWidget> CreateBindingContextPanel(const FArguments& InArgs);
	TSharedRef<SWidget> CreateBindingListPanel(const FArguments& InArgs, const FProperty* AssignableToProperty);

	void HandleWidgetSelected(FName WidgetName, ESelectInfo::Type);

	void HandleViewModelSelected(FBindingSource ViewModel, ESelectInfo::Type);
	TSharedRef<ITableRow> HandleGenerateViewModelRow(MVVM::FBindingSource ViewModel, const TSharedRef<STableViewBase>& OwnerTable) const;

	int32 SortConversionFunctionItemsRecursive(TArray<TSharedPtr<FConversionFunctionItem>>& Items);
	void GenerateConversionFunctionItems();
	void FilterConversionFunctionCategories();
	void FilterConversionFunctions();
	/** Recursively filter the items in SourceArray and place them into DestArray. Returns true if any items were added. */
	int32 FilterConversionFunctionCategoryChildren(const TArray<FString>& FilterStrings, const TArray<TSharedPtr<FConversionFunctionItem>>& SourceArray, TArray<TSharedPtr<FConversionFunctionItem>>& OutDestArray);
	void AddConversionFunctionChildrenRecursive(const TSharedPtr<FConversionFunctionItem>& Parent, TArray<const UFunction*>& OutFunctions);
	TSharedPtr<FConversionFunctionItem> FindOrCreateItemForCategory(TArray<TSharedPtr<FConversionFunctionItem>>& Items, TArrayView<FString> CategoryPath);
	TSharedPtr<FConversionFunctionItem> FindConversionFunctionCategory(const TArray<TSharedPtr<FConversionFunctionItem>>& Items, TArrayView<FString> CategoryNameParts) const;
	void HandleGetConversionFunctionCategoryChildren(TSharedPtr<FConversionFunctionItem> Item, TArray<TSharedPtr<FConversionFunctionItem>>& OutItems) const;
	void HandleConversionFunctionCategorySelected(TSharedPtr<FConversionFunctionItem> Item, ESelectInfo::Type);
	TSharedRef<ITableRow> HandleGenerateConversionFunctionCategoryRow(TSharedPtr<FConversionFunctionItem> Item, const TSharedRef<STableViewBase>& OwnerTable);
	TSharedRef<ITableRow> HandleGenerateConversionFunctionRow(const UFunction* Function, const TSharedRef<STableViewBase>& OwnerTable);

	TSharedPtr<FConversionFunctionItem> ExpandFunctionCategoryTreeToItem(const UFunction* Function);
	void ExpandFunctionCategoryTree(const TArray<TSharedPtr<FConversionFunctionItem>>& Items, bool bRecursive);

	void FilterViewModels(const FText& NewText);
	void HandleSearchBoxTextChanged(const FText& NewText);
	void HandleEnabledContextToggleChanged(ECheckBoxState CheckState);
	ECheckBoxState ToggleEnabledContext() const;

	bool IsClearEnabled() const;
	bool IsSelectEnabled() const;
	FReply HandleClearClicked();
	FReply HandleSelectClicked();
	FReply HandleCancelClicked();

private:
	TWeakObjectPtr<const UWidgetBlueprint> WidgetBlueprint;
	FOnLinkedValueSelectionChanged OnSelectionChanged;
	FSimpleDelegate OnMenuCloseRequested;
	FFieldSelectionContext SelectionContext;

	TSharedPtr<SSearchBox> SearchBox;

	//~ viewmodels (binding context panel)
	TSharedPtr<SListView<FBindingSource>> ViewModelList;
	TArray<FBindingSource> ViewModelSources;
	TArray<FBindingSource> FilteredViewModelSources;

	//~ widgets (binding context panel)
	TSharedPtr<SReadOnlyHierarchyView> WidgetList;

	//~ viewmodel and widgets (selection panel)
	TSharedPtr<SSourceBindingList> BindingList;

	//~ functions (binding context panel)
	TSharedPtr<STreeView<TSharedPtr<FConversionFunctionItem>>> ConversionFunctionCategoryTree;
	TArray<TSharedPtr<FConversionFunctionItem>> FilteredConversionFunctionRoot;
	TArray<TSharedPtr<FConversionFunctionItem>> ConversionFunctionRoot;

	//~ functions (selection panel)
	TSharedPtr<SListView<const UFunction*>> ConversionFunctionList;
	TArray<const UFunction*> ConversionFunctions;
	TArray<const UFunction*> FilteredConversionFunctions;

	bool bIsMenuInitialized = false;
	bool bIsClearEnabled = false;
}; 

} // namespace UE::MVVM
