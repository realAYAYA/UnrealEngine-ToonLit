// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/TVariant.h"
#include "MVVMPropertyPath.h"
#include "Styling/CoreStyle.h"
#include "Templates/ValueOrError.h"
#include "Types/MVVMBindingMode.h"
#include "Types/MVVMBindingSource.h"
#include "Types/MVVMFieldVariant.h"
#include "UObject/UnrealType.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SMVVMFieldEntry.h"
#include "Widgets/SMVVMViewModelBindingListWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STreeView.h"

class SComboButton;
class SSearchBox;
class SReadOnlyHierarchyView;
class STableViewBase;

namespace UE::MVVM
{

class SSourceEntry;
class SSourceBindingList;

class SFieldSelector : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_OneParam(FOnFieldSelectionChanged, FMVVMBlueprintPropertyPath);
	DECLARE_DELEGATE_OneParam(FOnConversionFunctionSelectionChanged, const UFunction*);

	SLATE_BEGIN_ARGS(SFieldSelector) :
		_TextStyle(&FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText")),
		_ShowConversionFunctions(false)
		{
		}
		SLATE_STYLE_ARGUMENT(FTextBlockStyle, TextStyle)
		SLATE_ATTRIBUTE(FMVVMBlueprintPropertyPath, SelectedField)
		
		/** Handler for when a field is selected. Always required. */
		SLATE_EVENT(FOnFieldSelectionChanged, OnFieldSelectionChanged)

		SLATE_ATTRIBUTE(const UFunction*, SelectedConversionFunction)
		
		/** Handler for when a conversion function is selected. Required if ShowConversionFunctions is set. */
		SLATE_EVENT(FOnConversionFunctionSelectionChanged, OnConversionFunctionSelectionChanged)
		
		/**
		 * Get the binding mode for this field. 
		 * Required for validating the availability of functions, eg. SetFoo() can only be used as a setter.
		 */
		SLATE_ATTRIBUTE(EMVVMBindingMode, BindingMode)

		/** 
		 * Should we show the source as well as the field?
		 * If this is set to false, then the Source attribute must be set.
		 */
		SLATE_ARGUMENT_DEFAULT(bool, ShowSource) = true;
		
		/** The source to use. Only used if ShowSource is false. */
		SLATE_ARGUMENT(FBindingSource, Source)

		/**
		 * Only show properties assignable to the given property.
		 */
		SLATE_ATTRIBUTE(const FProperty*, AssignableTo);

		/** Should we show all conversion functions with results compatible with the AssignableTo property? */
		SLATE_ATTRIBUTE(bool, ShowConversionFunctions);
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const UWidgetBlueprint* InWidgetBlueprint, bool bInViewModelProperty);
	void Refresh();

private:
	TSharedRef<SWidget> OnGenerateFieldWidget(FMVVMBlueprintPropertyPath Path) const;

	TSharedRef<SWidget> CreateSourcePanel();
	TSharedRef<SWidget> OnGetMenuContent();

	bool IsClearEnabled() const;
	FReply OnClearClicked();

	bool IsSelectEnabled() const;
	FReply OnSelectClicked();

	FReply OnCancelClicked();

	void SetPropertySelection(const FMVVMBlueprintPropertyPath& SelectedPath);
	EFieldVisibility GetFieldVisibilityFlags() const;

	void OnViewModelSelected(FBindingSource ViewModel, ESelectInfo::Type);
	TSharedRef<ITableRow> GenerateViewModelRow(MVVM::FBindingSource ViewModel, const TSharedRef<STableViewBase>& OwnerTable) const;
	void FilterViewModels();

	void OnWidgetSelected(FName WidgetName, ESelectInfo::Type);

	void OnSearchBoxTextChanged(const FText& NewText);

	struct FConversionFunctionItem
	{
		FString GetCategoryName() { return CategoryPath.Num() > 0 ? CategoryPath.Last() : FString(); }

		TArray<FString> CategoryPath;
		const UFunction* Function;
		TArray<TSharedPtr<FConversionFunctionItem>> Children;
		int32 NumFunctions = 0;
	};

	TSharedPtr<FConversionFunctionItem> FindOrCreateItemForCategory(TArray<TSharedPtr<FConversionFunctionItem>>& Items, TArrayView<FString> CategoryPath);
	TSharedRef<ITableRow> GenerateConversionFunctionCategoryRow(TSharedPtr<FConversionFunctionItem> Item, const TSharedRef<STableViewBase>& OwnerTable);
	void GenerateConversionFunctionItems();
	void FilterConversionFunctionCategories();
	/** Recursively filter the items in SourceArray and place them into DestArray. Returns true if any items were added. */
	int32 FilterConversionFunctionCategoryChildren(const TArray<FString>& FilterStrings, const TArray<TSharedPtr<FConversionFunctionItem>>& SourceArray, TArray<TSharedPtr<FConversionFunctionItem>>& OutDestArray);
	void FilterConversionFunctions();

	void OnConversionFunctionCategorySelected(TSharedPtr<FConversionFunctionItem> Item, ESelectInfo::Type);
	void GetConversionFunctionCategoryChildren(TSharedPtr<FConversionFunctionItem> Item, TArray<TSharedPtr<FConversionFunctionItem>>& OutItems) const;
	TSharedPtr<FConversionFunctionItem> FindConversionFunctionCategory(const TArray<TSharedPtr<FConversionFunctionItem>>& Items, TArrayView<FString> CategoryNameParts) const;
	
	void AddConversionFunctionChildrenRecursive(const TSharedPtr<FConversionFunctionItem>& Parent, TArray<const UFunction*>& OutFunctions);

	int32 SortConversionFunctionItemsRecursive(TArray<TSharedPtr<FConversionFunctionItem>>& Items);

	void SetConversionFunctionSelection(const UFunction* SelectedFunction);
	void ExpandAllToItem(const UFunction* Function);
	void ExpandAll(const TArray<TSharedPtr<FConversionFunctionItem>>& Items, bool bRecursive);

	TSharedRef<ITableRow> GenerateConversionFunctionRow(const UFunction* Function, const TSharedRef<STableViewBase>& OwnerTable);

private:
	TAttribute<EMVVMBindingMode> BindingMode;

	TAttribute<FMVVMBlueprintPropertyPath> SelectedField;
	FMVVMBlueprintPropertyPath CachedSelectedField;

	TSharedPtr<SFieldEntry> SelectedEntryWidget;
	TSharedPtr<SSourceEntry> SelectedSourceWidget;

	const FTextBlockStyle* TextStyle = nullptr;

	FOnFieldSelectionChanged OnFieldSelectionChangedDelegate;

	TSharedPtr<SSourceBindingList> BindingList;
	TSharedPtr<SSearchBox> SearchBox;
	TSharedPtr<SComboButton> ComboButton;

	TOptional<FBindingSource> FixedSource;

	TWeakObjectPtr<const UWidgetBlueprint> WidgetBlueprint;
	bool bViewModelProperty = false;

	TArray<FBindingSource> ViewModelSources;
	TArray<FBindingSource> FilteredViewModelSources;

	TSharedPtr<SListView<FBindingSource>> ViewModelList;
	TSharedPtr<SReadOnlyHierarchyView> WidgetList;

	TAttribute<const FProperty*> AssignableTo;
	TAttribute<bool> ShowConversionFunctions;

	TAttribute<const UFunction*> SelectedConversionFunction;
	const UFunction* CachedSelectedConversionFunction = nullptr;

	FOnConversionFunctionSelectionChanged OnConversionFunctionSelectionChangedDelegate;

	TSharedPtr<STreeView<TSharedPtr<FConversionFunctionItem>>> ConversionFunctionCategoryTree;
	TArray<TSharedPtr<FConversionFunctionItem>> FilteredConversionFunctionRoot;
	TArray<TSharedPtr<FConversionFunctionItem>> ConversionFunctionRoot;

	TSharedPtr<SListView<const UFunction*>> ConversionFunctionList;

	TArray<const UFunction*> ConversionFunctions;
	TArray<const UFunction*> FilteredConversionFunctions;

	bool bIsMenuInitialized = false;
}; 

} // namespace UE::MVVM