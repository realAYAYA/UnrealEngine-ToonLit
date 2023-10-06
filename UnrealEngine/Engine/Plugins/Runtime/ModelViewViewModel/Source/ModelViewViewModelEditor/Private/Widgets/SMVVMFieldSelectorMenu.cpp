// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SMVVMFieldSelectorMenu.h"

#include "Bindings/MVVMBindingHelper.h"
#include "EdGraphSchema_K2.h"
#include "Editor.h"
#include "Framework/Views/TableViewMetadata.h"
#include "Hierarchy/SReadOnlyHierarchyView.h"
#include "MVVMBlueprintView.h"
#include "MVVMEditorSubsystem.h"
#include "MVVMDeveloperProjectSettings.h"
#include "MVVMWidgetBlueprintExtension_View.h"
#include "SPrimaryButton.h"
#include "Styling/MVVMEditorStyle.h"
#include "Styling/SlateIconFinder.h"
#include "Types/MVVMBindingMode.h"
#include "Widgets/SMVVMFieldEntry.h"
#include "Widgets/SMVVMPropertyPath.h"
#include "Widgets/SMVVMSourceEntry.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SMVVMCachedViewBindingPropertyPath.h"
#include "Widgets/SMVVMCachedViewBindingConversionFunction.h"
#include "Widgets/SMVVMViewModelBindingListWidget.h"
#include "Widgets/Views/STreeView.h"

#define LOCTEXT_NAMESPACE "MVVMFieldSelectorMenu"

namespace UE::MVVM
{

namespace Private
{
EFieldVisibility GetFieldVisibilityFlags(EMVVMBindingMode Mode, bool bReadable, bool bWritable)
{
	EFieldVisibility Flags = EFieldVisibility::None;

	if (bReadable)
	{
		Flags |= EFieldVisibility::Readable;
	}
	if (bWritable)
	{
		Flags |= EFieldVisibility::Writable;
	}
	if (!IsOneTimeBinding(Mode))
	{
		Flags |= EFieldVisibility::Notify;
	}

	return Flags;
}
}

void SFieldSelectorMenu::Construct(const FArguments& InArgs, const UWidgetBlueprint* InWidgetBlueprint)
{
	WidgetBlueprint = InWidgetBlueprint;
	check(InWidgetBlueprint);

	OnFieldSelectionChanged = InArgs._OnFieldSelectionChanged;
	OnMenuCloseRequested = InArgs._OnMenuCloseRequested;
	SelectionContext = InArgs._SelectionContext;

	bIsMenuInitialized = false;
	bIsClearEnabled = (InArgs._CurrentPropertyPathSelected.IsSet() && !InArgs._CurrentPropertyPathSelected.GetValue().IsEmpty()) || InArgs._CurrentFunctionSelected != nullptr;

	// If we're showing conversion functions, we don't want to set the AssignableTo property of SSourceBindingList, because then it will only show exact matches, 
	// and since we're also showing conversion functions we know that's not what the user wants.
	// However, in the case we're not showing conversion functions, we want only exact matches.
	const FProperty* AssignableToProperty = nullptr;
	const bool bShowConversionFunctions = SelectionContext.bAllowConversionFunctions;
	if (!bShowConversionFunctions)
	{
		AssignableToProperty = SelectionContext.AssignableTo;
	}

	if (SelectionContext.bAllowConversionFunctions)
	{
		GenerateConversionFunctionItems();
	}

	TSharedRef<SVerticalBox> VBox = SNew(SVerticalBox)
	+ SVerticalBox::Slot()
	.HAlign(HAlign_Fill)
	.VAlign(VAlign_Top)
	.Padding(0, 4, 0, 4)
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.Padding(3.f, 0.f, 3.f, 0.f)	
		[
			SAssignNew(SearchBox, SSearchBox)
			.OnTextChanged(this, &SFieldSelectorMenu::HandleSearchBoxTextChanged)
		]
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SCheckBox)
			.OnCheckStateChanged(this, &SFieldSelectorMenu::HandleEnabledContextToggleChanged)
			.IsChecked(this, &SFieldSelectorMenu::ToggleEnabledContext)
			.ToolTipText(LOCTEXT("ContextFlagToolTip", "Should the list be filtered to only properties and functions that make sense in the current context?"))
			[
				SNew(STextBlock)
				.Text(LOCTEXT("MVVMContextSensitiveToggle", "Context Sensitive"))
			]
		]
	];

	VBox->AddSlot()
		.FillHeight(1.0f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("Brushes.Recessed"))
			[
				SNew(SSplitter)
				.PhysicalSplitterHandleSize(4.0f)
				+ SSplitter::Slot()
				.Value(0.5f)
				.MinSize(100.0f)
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[
						CreateBindingContextPanel(InArgs)
					]
				]
				+ SSplitter::Slot()
				.Value(0.5f)
				.MinSize(100.0f)
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[
						CreateBindingListPanel(InArgs, AssignableToProperty)
					]
				]
			]
		];

	VBox->AddSlot()
		.Padding(4.0f, 4.0f, 4.0f, 0.0f)
		.HAlign(HAlign_Right)
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SPrimaryButton)
				.OnClicked(this, &SFieldSelectorMenu::HandleSelectClicked)
				.IsEnabled(this, &SFieldSelectorMenu::IsSelectEnabled)
				.Text(LOCTEXT("Select", "Select"))
			]
			+ SHorizontalBox::Slot()
			.Padding(FAppStyle::Get().GetMargin("StandardDialog.SlotPadding"))
			.AutoWidth()
			[
				SNew(SButton)
				.OnClicked(this, &SFieldSelectorMenu::HandleClearClicked)
				.IsEnabled(this, &SFieldSelectorMenu::IsClearEnabled)
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Clear", "Clear"))
				]
			]
			+ SHorizontalBox::Slot()
			.Padding(FAppStyle::Get().GetMargin("StandardDialog.SlotPadding"))
			.AutoWidth()
			[
				SNew(SButton)
				.OnClicked(this, &SFieldSelectorMenu::HandleCancelClicked)
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Cancel", "Cancel"))
				]
			]
		];

	TSharedRef<SWidget> MenuWidget = SNew(SBox)
		.MinDesiredWidth(400.0f)
		.MinDesiredHeight(200.0f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
			.Padding(FMargin(8.0f, 2.0f, 8.0f, 3.0f))
			[
				VBox
			]
		];

	ChildSlot
	[
		MenuWidget
	];

	bIsMenuInitialized = true;
}

TSharedRef<SWidget> SFieldSelectorMenu::GetWidgetToFocus() const
{
	return SearchBox.ToSharedRef();
}

bool SFieldSelectorMenu::IsSelectEnabled() const
{
	if (BindingList.IsValid())
	{
		FMVVMBlueprintPropertyPath Path = BindingList->GetSelectedProperty();
		if (Path.IsFromViewModel() || Path.IsFromWidget())
		{
			return true;
		}
	}

	if (ConversionFunctionList.IsValid())
	{
		if (ConversionFunctionList->GetSelectedItems().Num() > 0)
		{
			return true;
		}
	}

	return false;
}

FReply SFieldSelectorMenu::HandleSelectClicked()
{
	FMVVMBlueprintPropertyPath PropertyPath;
	if (BindingList.IsValid())
	{
		PropertyPath = BindingList->GetSelectedProperty();
	}

	TArray<const UFunction*> Selection;
	if (ConversionFunctionList.IsValid())
	{
		Selection = ConversionFunctionList->GetSelectedItems();
	}

	OnFieldSelectionChanged.ExecuteIfBound(PropertyPath, Selection.Num() > 0 ? Selection[0] : nullptr);

	return FReply::Handled();
}

bool SFieldSelectorMenu::IsClearEnabled() const
{
	return bIsClearEnabled;
}

FReply SFieldSelectorMenu::HandleClearClicked()
{
	FMVVMBlueprintPropertyPath NewProperty;
	
	if (SelectionContext.FixedBindingSource.IsSet())
	{
		FBindingSource Source = SelectionContext.FixedBindingSource.GetValue();
		if (Source.ViewModelId.IsValid())
		{
			NewProperty.SetViewModelId(Source.ViewModelId);
		}
		else 
		{
			NewProperty.SetWidgetName(Source.Name);
		}
	}

	OnFieldSelectionChanged.ExecuteIfBound(NewProperty, nullptr);

	return FReply::Handled();
}

FReply SFieldSelectorMenu::HandleCancelClicked()
{
	OnMenuCloseRequested.ExecuteIfBound();

	return FReply::Handled();
}

void SFieldSelectorMenu::SetPropertyPathSelection(const FMVVMBlueprintPropertyPath& SelectedPath)
{
	OnFieldSelectionChanged.ExecuteIfBound(SelectedPath, nullptr);
}

void SFieldSelectorMenu::SetConversionFunctionSelection(const UFunction* SelectedFunction)
{
	OnFieldSelectionChanged.ExecuteIfBound(FMVVMBlueprintPropertyPath(), SelectedFunction);
}

TSharedPtr<SFieldSelectorMenu::FConversionFunctionItem> SFieldSelectorMenu::FindConversionFunctionCategory(const TArray<TSharedPtr<FConversionFunctionItem>>& Items, TArrayView<FString> CategoryNameParts) const
{
	if (CategoryNameParts.Num() > 0)
	{
		for (const TSharedPtr<FConversionFunctionItem>& Item : Items)
		{
			if (Item->GetCategoryName() == CategoryNameParts[0])
			{
				TArrayView<FString> RemainingParts = CategoryNameParts.RightChop(1);

				// last category part, this is what we're looking for
				if (RemainingParts.Num() == 0)
				{
					return Item;
				}

				// recurse into children
				return FindConversionFunctionCategory(Item->Children, RemainingParts);
			}
		}
	}

	return TSharedPtr<FConversionFunctionItem>();
}

void SFieldSelectorMenu::HandleSearchBoxTextChanged(const FText& NewText)
{
	if (BindingList.IsValid())
	{
		BindingList->SetRawFilterText(NewText);
	}

	if (ConversionFunctionCategoryTree.IsValid())
	{
		TArray<TSharedPtr<FConversionFunctionItem>> OldSelectedCategories;
		ConversionFunctionCategoryTree->GetSelectedItems(OldSelectedCategories);

		FilterConversionFunctionCategories();
		ConversionFunctionCategoryTree->RequestTreeRefresh();

		TArray<TSharedPtr<FConversionFunctionItem>> NewSelectedCategories;

		// reselect old selection
		for (const TSharedPtr<FConversionFunctionItem>& OldItem : OldSelectedCategories)
		{
			if (TSharedPtr<FConversionFunctionItem> NewItem = FindConversionFunctionCategory(FilteredConversionFunctionRoot, OldItem->CategoryPath))
			{
				NewSelectedCategories.Add(NewItem);
			}
		}
		
		if (NewSelectedCategories.Num() > 0)
		{
			ConversionFunctionCategoryTree->SetItemSelection(NewSelectedCategories, true);
		}
	}

	if (ConversionFunctionList.IsValid())
	{
		TArray<const UFunction*> OldSelectedFunctions;
		ConversionFunctionList->GetSelectedItems(OldSelectedFunctions);

		FilterConversionFunctions();
		ConversionFunctionList->RequestListRefresh();

		ConversionFunctionList->SetItemSelection(OldSelectedFunctions, true);
	}
}

void SFieldSelectorMenu::HandleViewModelSelected(FBindingSource Source, ESelectInfo::Type)
{
	if (!bIsMenuInitialized)
	{
		return;
	}

	if (BindingList.IsValid())
	{
		BindingList->ClearSources();
	}

	TArray<FBindingSource> Selection = ViewModelList->GetSelectedItems();
	if (Selection.Num() == 0)
	{
		return;
	}

	if (WidgetList.IsValid())
	{
		WidgetList->ClearSelection();
	}

	if (ConversionFunctionCategoryTree.IsValid())
	{
		ConversionFunctionCategoryTree->ClearSelection();
	}

	if (BindingList.IsValid())
	{
		BindingList->AddSources(Selection);
	}
}

void SFieldSelectorMenu::HandleWidgetSelected(FName WidgetName, ESelectInfo::Type)
{
	if (!bIsMenuInitialized)
	{
		return;
	}

	if (BindingList.IsValid())
	{
		BindingList->ClearSources();
	}

	TArray<FName> Selection = WidgetList->GetSelectedWidgets();
	if (Selection.Num() == 0)
	{
		return;
	}

	if (ViewModelList.IsValid())
	{
		ViewModelList->ClearSelection();
	}

	if (ConversionFunctionCategoryTree.IsValid())
	{
		ConversionFunctionCategoryTree->ClearSelection();
	}

	if (BindingList.IsValid())
	{
		const UWidgetBlueprint* WidgetBP = WidgetBlueprint.Get();

		TArray<FBindingSource> Sources;
		Algo::Transform(Selection, Sources, [WidgetBP](const FName& WidgetName)
			{
				return FBindingSource::CreateForWidget(WidgetBP, WidgetName);
			});

		BindingList->AddSources(Sources);
	}
}

TSharedRef<ITableRow> SFieldSelectorMenu::HandleGenerateViewModelRow(FBindingSource ViewModel, const TSharedRef<STableViewBase>& OwnerTable) const
{
	return SNew(STableRow<FBindingSource>, OwnerTable)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(4)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SImage)
				.Image(FSlateIconFinder::FindIconBrushForClass(ViewModel.Class.Get()))
				.ColorAndOpacity(FSlateColor::UseForeground())
			]
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(ViewModel.DisplayName)
				.ColorAndOpacity(FSlateColor::UseForeground())
				.HighlightText_Lambda([this]() { return SearchBox.IsValid() ? SearchBox->GetText() : FText::GetEmpty(); })
			]
		];
}

int32 SFieldSelectorMenu::FilterConversionFunctionCategoryChildren(const TArray<FString>& FilterStrings, const TArray<TSharedPtr<FConversionFunctionItem>>& SourceArray, TArray<TSharedPtr<FConversionFunctionItem>>& OutDestArray)
{
	int32 NumFunctions = 0;

	for (const TSharedPtr<FConversionFunctionItem>& SourceItem : SourceArray)
	{
		// check if our name matches the filters
		bool bMatchesFilters = false;
		if (SourceItem->Function != nullptr)
		{
			bMatchesFilters = true;
			for (const FString& Filter : FilterStrings)
			{
				bool bFoundMatches = false;
				for (const FString& Keyword : SourceItem->SearchKeywords)
				{
					if (Keyword.Contains(Filter))
					{
						bFoundMatches = true;
						break;
					}
				}

				if (!bFoundMatches)
				{
					bMatchesFilters = false;
					break;
				}
			}
		}

		int32 NumChildren = 0;
		TArray<TSharedPtr<FConversionFunctionItem>> FilteredChildren;
		if (bMatchesFilters)
		{
			ensureAlways(SourceItem->Function != nullptr);
			NumChildren = 1;
		}
		else
		{
			// if we don't match, then we still want to check all our children
			NumChildren = FilterConversionFunctionCategoryChildren(FilterStrings, SourceItem->Children, FilteredChildren);
		}

		// then add this item to the destination array
		if (NumChildren > 0)
		{
			TSharedPtr<FConversionFunctionItem>& NewItem = OutDestArray.Add_GetRef(MakeShared<FConversionFunctionItem>());
			NewItem->CategoryPath = SourceItem->CategoryPath;
			NewItem->Function = SourceItem->Function; 
			NewItem->Children = FilteredChildren;
			NewItem->NumFunctions = NumChildren;

			NumFunctions += NewItem->NumFunctions;
		}
	}

	return NumFunctions;
}

void SFieldSelectorMenu::FilterConversionFunctionCategories()
{
	FilteredConversionFunctionRoot.Reset();

	TArray<FString> FilterStrings;
	if (SearchBox.IsValid())
	{
		SearchBox->GetText().ToString().ParseIntoArrayWS(FilterStrings);
	}

	if (FilterStrings.IsEmpty())
	{
		// don't bother filtering if we don't have any search terms
		FilteredConversionFunctionRoot = ConversionFunctionRoot;
		return;
	}

	TSharedPtr<FConversionFunctionItem> RootItem = FilteredConversionFunctionRoot.Add_GetRef(MakeShared<FConversionFunctionItem>());
	RootItem->CategoryPath = { TEXT("Conversion Functions") };
	
	RootItem->NumFunctions = FilterConversionFunctionCategoryChildren(FilterStrings, ConversionFunctionRoot[0]->Children, FilteredConversionFunctionRoot[0]->Children);
	
	ExpandFunctionCategoryTree(FilteredConversionFunctionRoot, true);
}

int32 SFieldSelectorMenu::SortConversionFunctionItemsRecursive(TArray<TSharedPtr<FConversionFunctionItem>>& Items)
{
	int32 NumFound = 0;

	Items.Sort([](const TSharedPtr<FConversionFunctionItem>& A, const TSharedPtr<FConversionFunctionItem>& B)
		{
			if (!A->GetCategoryName().IsEmpty() && !B->GetCategoryName().IsEmpty())
			{
				return A->GetCategoryName() < B->GetCategoryName();
			}
			if (!A->GetCategoryName().IsEmpty() && B->GetCategoryName().IsEmpty())
			{
				return true;
			}
			if (A->GetCategoryName().IsEmpty() && !B->GetCategoryName().IsEmpty())
			{
				return false;
			}
			if (A->Function != nullptr && B->Function != nullptr)
			{
				return A->Function->GetDisplayNameText().CompareTo(B->Function->GetDisplayNameText()) <= 0;
			}
			return true;
		});

	for (const TSharedPtr<FConversionFunctionItem>& Item : Items)
	{
		NumFound += SortConversionFunctionItemsRecursive(Item->Children);

		if (Item->Function != nullptr)
		{
			NumFound += 1;
		}
	}

	return NumFound;
}

void SFieldSelectorMenu::GenerateConversionFunctionItems()
{
	UMVVMEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
	TArray<UFunction*> AllConversionFunctions = Subsystem->GetAvailableConversionFunctions(WidgetBlueprint.Get(), FMVVMBlueprintPropertyPath(), FMVVMBlueprintPropertyPath());

	// remove all incompatible conversion functions
	for (int32 Index = AllConversionFunctions.Num() - 1; Index >= 0; --Index)
	{
		const UFunction* Function = AllConversionFunctions[Index];
		const FProperty* ReturnProperty = BindingHelper::GetReturnProperty(Function);
		const FProperty* AssignToProperty = SelectionContext.AssignableTo;

		if (AssignToProperty != nullptr && !BindingHelper::ArePropertiesCompatible(ReturnProperty, AssignToProperty))
		{
			AllConversionFunctions.RemoveAtSwap(Index);
		}
	}

	auto AddFunctionToItem = [](const UFunction* Function, const TSharedPtr<FConversionFunctionItem>& Parent)
	{
		TSharedPtr<FConversionFunctionItem>& Item = Parent->Children.Add_GetRef(MakeShared<FConversionFunctionItem>());
		Item->Function = Function;
		Item->NumFunctions = 1;
		Parent->NumFunctions += 1;

		Item->SearchKeywords.Add(Function->GetName());
		const FString& DisplayName = Function->GetMetaData(FBlueprintMetadata::MD_DisplayName);
		if (DisplayName.Len() > 0)
		{
			Item->SearchKeywords.Add(DisplayName);
		}
		FString MetadataKeywords = Function->GetMetaDataText(FBlueprintMetadata::MD_FunctionKeywords, TEXT("UObjectKeywords"), Function->GetFullGroupName(false)).ToString();
		if (MetadataKeywords.Len() > 0)
		{
			Item->SearchKeywords.Add(MoveTemp(MetadataKeywords));
		}
	};

	TSharedPtr<FConversionFunctionItem> CurrentSelectedItem;

	TArray<FString> CategoryPath;

	ConversionFunctionRoot.Reset();
	TSharedPtr<FConversionFunctionItem>& RootItem = ConversionFunctionRoot.Add_GetRef(MakeShared<FConversionFunctionItem>());
	RootItem->CategoryPath = { TEXT("Conversion Functions") };

	FName NAME_Category = "Category";
	for (const UFunction* Function : AllConversionFunctions)
	{
		const FText& CategoryName = Function->GetMetaDataText(NAME_Category);
		if (CategoryName.IsEmpty())
		{
			AddFunctionToItem(Function, RootItem);
			continue;
		}

		// split into subcategories and trim
		CategoryPath.Reset();
		CategoryName.ToString().ParseIntoArray(CategoryPath, TEXT("|"));
		for (FString& SubCategory : CategoryPath)
		{
			SubCategory.TrimStartAndEndInline();
		}

		TSharedPtr<FConversionFunctionItem> ParentItem = RootItem;

		// create items for the entire category path
		// eg. "Math|Boolean|AND" 
		// Math 
		//   > Boolean
		//     > AND
		for (int32 PathIndex = 0; PathIndex < CategoryPath.Num(); ++PathIndex)
		{
			ParentItem->NumFunctions += 1;

			ParentItem = FindOrCreateItemForCategory(ParentItem->Children, MakeArrayView(CategoryPath.GetData(), PathIndex + 1));
		}

		AddFunctionToItem(Function, ParentItem);
	}

	int32 NumItems = SortConversionFunctionItemsRecursive(ConversionFunctionRoot);
	ensure(NumItems == RootItem->NumFunctions);
}

TSharedPtr<SFieldSelectorMenu::FConversionFunctionItem> SFieldSelectorMenu::ExpandFunctionCategoryTreeToItem(const UFunction* Function)
{
	TArray<TSharedPtr<FConversionFunctionItem>> Path;

	FText FullCategoryName = Function->GetMetaDataText("Category");
	if (FullCategoryName.IsEmpty())
	{
		Path.Add(FilteredConversionFunctionRoot[0]);
	}
	else
	{
		TArray<FString> CategoryPath;
		FullCategoryName.ToString().ParseIntoArray(CategoryPath, TEXT("|"));

		TSharedPtr<FConversionFunctionItem> CurrentParent = FilteredConversionFunctionRoot[0];

		for (const FString& SubCategory : CategoryPath)
		{
			const FString Trimmed = SubCategory.TrimStartAndEnd();

			TSharedPtr<FConversionFunctionItem>* FoundItem =
				CurrentParent->Children.FindByPredicate([Trimmed, Function](const TSharedPtr<FConversionFunctionItem>& Item)
					{
						return Item->GetCategoryName() == Trimmed || Item->Function == Function;
					});

			if (FoundItem != nullptr)
			{
				Path.Add(*FoundItem);
				CurrentParent = *FoundItem;
			}
		}
	}

	if (Path.Num() > 0)
	{
		ConversionFunctionCategoryTree->SetItemExpansion(FilteredConversionFunctionRoot[0], true);
		ExpandFunctionCategoryTree(Path, false);
		return Path.Last();
	}
	return TSharedPtr<FConversionFunctionItem>();
}

void SFieldSelectorMenu::ExpandFunctionCategoryTree(const TArray<TSharedPtr<FConversionFunctionItem>>& Items, bool bRecursive)
{
	for (const TSharedPtr<FConversionFunctionItem>& Item : Items)
	{
		ConversionFunctionCategoryTree->SetItemExpansion(Item, true);

		if (bRecursive)
		{
			ExpandFunctionCategoryTree(Item->Children, bRecursive);
		}
	}
}

void SFieldSelectorMenu::FilterViewModels(const FText& NewText)
{
	FilteredViewModelSources.Reset();

	TArray<FString> FilterStrings;
	NewText.ToString().ParseIntoArrayWS(FilterStrings);

	if (FilterStrings.IsEmpty())
	{
		FilteredViewModelSources = ViewModelSources;
		return;
	}

	for (const FBindingSource& ViewModel : ViewModelSources)
	{
		const FString DisplayName = ViewModel.DisplayName.ToString();
		const FString ClassName = ViewModel.Class != nullptr ? ViewModel.Class->GetName() : FString();
		const FString Name = ViewModel.Name.ToString();

		bool bMatchesFilters = true;
		
		for (const FString& Filter : FilterStrings)
		{
			if (!DisplayName.Contains(Filter) && 
				!ClassName.Contains(Filter) &&
				!Name.Contains(Filter))
			{
				bMatchesFilters = false;
				break;
			}
		}

		if (bMatchesFilters)
		{
			FBindingSource& NewSource = FilteredViewModelSources.Add_GetRef(FBindingSource());
			NewSource.Class = ViewModel.Class;
			NewSource.Name = ViewModel.Name;
			NewSource.DisplayName = ViewModel.DisplayName;
			NewSource.ViewModelId = ViewModel.ViewModelId;
		}
	}
}

TSharedRef<SWidget> SFieldSelectorMenu::CreateBindingContextPanel(const FArguments& InArgs)
{
	// show source picker
	TSharedRef<SVerticalBox> StackedSourcePicker = SNew(SVerticalBox);

	if (SelectionContext.bAllowViewModels)
	{
		if (SelectionContext.FixedBindingSource.IsSet()
			&& SelectionContext.FixedBindingSource.GetValue().IsValid()
			&& SelectionContext.FixedBindingSource.GetValue().ViewModelId.IsValid())
		{
			ViewModelSources.Add(SelectionContext.FixedBindingSource.GetValue());
		}
		else
		{
			ViewModelSources = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>()->GetAllViewModels(WidgetBlueprint.Get());
		}

		FilteredViewModelSources = ViewModelSources;

		ViewModelList = SNew(SListView<FBindingSource>)
			.ListItemsSource(&FilteredViewModelSources)
			.OnGenerateRow(this, &SFieldSelectorMenu::HandleGenerateViewModelRow)
			.SelectionMode(ESelectionMode::Multi)
			.OnSelectionChanged(this, &SFieldSelectorMenu::HandleViewModelSelected);

		FBindingSource SelectedSource;
		if (InArgs._CurrentPropertyPathSelected.IsSet() && InArgs._CurrentPropertyPathSelected.GetValue().IsFromViewModel())
		{
			for (const FBindingSource& Source : FilteredViewModelSources)
			{
				if (Source.ViewModelId == InArgs._CurrentPropertyPathSelected.GetValue().GetViewModelId())
				{
					SelectedSource = Source;
				}
			}
		}

		if (SelectedSource.IsValid())
		{
			ViewModelList->SetItemSelection(SelectedSource, true);
		}

		StackedSourcePicker->AddSlot()
			.AutoHeight()
			[
				ViewModelList.ToSharedRef()
			];
	}
	
	if (SelectionContext.bAllowWidgets)
	{
		TArray<FName> ShowOnly;
		if (SelectionContext.FixedBindingSource.IsSet()
			&& SelectionContext.FixedBindingSource.GetValue().IsValid()
			&& !SelectionContext.FixedBindingSource.GetValue().Name.IsNone())
		{
			ShowOnly.Add(SelectionContext.FixedBindingSource.GetValue().Name);
		}

		WidgetList = SNew(SReadOnlyHierarchyView, WidgetBlueprint.Get())
			.OnSelectionChanged(this, &SFieldSelectorMenu::HandleWidgetSelected)
			.SelectionMode(ESelectionMode::Multi)
			.ShowSearch(false)
			.ShowOnly(ShowOnly)
			.ExpandAll(false);

		if (InArgs._CurrentPropertyPathSelected.IsSet() && InArgs._CurrentPropertyPathSelected.GetValue().IsFromWidget())
		{
			WidgetList->SetSelectedWidget(InArgs._CurrentPropertyPathSelected.GetValue().GetWidgetName());
		}

		StackedSourcePicker->AddSlot()
			.AutoHeight()
			[
				WidgetList.ToSharedRef()
			];
	}

	if (SelectionContext.bAllowConversionFunctions)
	{
		FilterConversionFunctionCategories();
		
		SAssignNew(ConversionFunctionCategoryTree, STreeView<TSharedPtr<FConversionFunctionItem>>)
			.SelectionMode(ESelectionMode::Multi)
			.TreeItemsSource(&FilteredConversionFunctionRoot)
			.OnGenerateRow(this, &SFieldSelectorMenu::HandleGenerateConversionFunctionCategoryRow)
			.OnSelectionChanged(this, &SFieldSelectorMenu::HandleConversionFunctionCategorySelected)
			.OnGetChildren(this, &SFieldSelectorMenu::HandleGetConversionFunctionCategoryChildren);

		if (InArgs._CurrentFunctionSelected != nullptr)
		{
			TSharedPtr<FConversionFunctionItem> FunctionItem = ExpandFunctionCategoryTreeToItem(InArgs._CurrentFunctionSelected);
			if (FunctionItem)
			{
				TGuardValue<bool> TmpGuard(bIsMenuInitialized, true);
				ConversionFunctionCategoryTree->SetItemSelection(FunctionItem, true);
			}
		}

		StackedSourcePicker->AddSlot()
			.AutoHeight()
			[
				ConversionFunctionCategoryTree.ToSharedRef()
			];
	}

	return StackedSourcePicker;
}

TSharedRef<SWidget> SFieldSelectorMenu::CreateBindingListPanel(const FArguments& InArgs, const FProperty* AssignableToProperty)
{
	BindingList = SNew(SSourceBindingList, WidgetBlueprint.Get())
		.ShowSearchBox(false)
		.OnDoubleClicked(this, &SFieldSelectorMenu::SetPropertyPathSelection)
		.FieldVisibilityFlags(Private::GetFieldVisibilityFlags(SelectionContext.BindingMode, SelectionContext.bReadable, SelectionContext.bWritable))
		.AssignableTo(AssignableToProperty);

	if (SelectionContext.FixedBindingSource.IsSet())
	{
		// Single fixed source, don't show the separate source panel.
		BindingList->AddSource(SelectionContext.FixedBindingSource.GetValue());
	}
	else if (InArgs._CurrentPropertyPathSelected.IsSet())
	{
		FBindingSource Source = InArgs._CurrentPropertyPathSelected.GetValue().IsFromViewModel() ?
			FBindingSource::CreateForViewModel(WidgetBlueprint.Get(), InArgs._CurrentPropertyPathSelected.GetValue().GetViewModelId()) :
			FBindingSource::CreateForWidget(WidgetBlueprint.Get(), InArgs._CurrentPropertyPathSelected.GetValue().GetWidgetName());
		BindingList->AddSource(Source);
	}

	if (InArgs._CurrentPropertyPathSelected.IsSet())
	{
		BindingList->SetSelectedProperty(InArgs._CurrentPropertyPathSelected.GetValue());
	}


	TSharedRef<SVerticalBox> BindingListVBox = SNew(SVerticalBox);
	BindingListVBox->AddSlot()
		[
			BindingList.ToSharedRef()
		];

	if (SelectionContext.bAllowConversionFunctions)
	{
		BindingListVBox->AddSlot()
			.AutoHeight()
			[
				SAssignNew(ConversionFunctionList, SListView<const UFunction*>)
				.SelectionMode(ESelectionMode::Single)
				.ListItemsSource(&FilteredConversionFunctions)
				.OnMouseButtonDoubleClick(this, &SFieldSelectorMenu::SetConversionFunctionSelection)
				.OnGenerateRow(this, &SFieldSelectorMenu::HandleGenerateConversionFunctionRow)
			];

		if (InArgs._CurrentFunctionSelected != nullptr)
		{
			ConversionFunctionList->SetItemSelection(InArgs._CurrentFunctionSelected, true);
		}
	}

	return BindingListVBox;
}

void SFieldSelectorMenu::HandleEnabledContextToggleChanged(ECheckBoxState CheckState)
{
	UMVVMWidgetBlueprintExtension_View* ExtensionView = UMVVMWidgetBlueprintExtension_View::GetExtension<UMVVMWidgetBlueprintExtension_View>(WidgetBlueprint.Get());
	check(ExtensionView);

	FMVVMViewBindingFilterSettings FilterSettings = ExtensionView->GetFilterSettings();
	FilterSettings.FilterFlags = CheckState == ECheckBoxState::Checked ? EFilterFlag::All : EFilterFlag::None;
	ExtensionView->SetFilterSettings(FilterSettings);

	if (WidgetList.IsValid())
	{
		HandleWidgetSelected(WidgetBlueprint->GetFName(), ESelectInfo::Direct);
	}

	if (ViewModelList.IsValid())
	{
		HandleViewModelSelected(FBindingSource(), ESelectInfo::Direct);
	}
}

ECheckBoxState SFieldSelectorMenu::ToggleEnabledContext() const
{
	UMVVMWidgetBlueprintExtension_View* ExtensionView = UMVVMWidgetBlueprintExtension_View::GetExtension<UMVVMWidgetBlueprintExtension_View>(WidgetBlueprint.Get());
	check(ExtensionView);

	return ExtensionView->GetFilterSettings().FilterFlags == EFilterFlag::All ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

TSharedPtr<SFieldSelectorMenu::FConversionFunctionItem> SFieldSelectorMenu::FindOrCreateItemForCategory(TArray<TSharedPtr<FConversionFunctionItem>>& Items, TArrayView<FString> CategoryPath)
{
	check(CategoryPath.Num() > 0);

	const FString& CategoryName = CategoryPath.Last();

	int32 Idx = 0;
	for (; Idx < Items.Num(); ++Idx)
	{
		// found item
		if (Items[Idx]->GetCategoryName() == CategoryName)
		{
			return Items[Idx];
		}

		// passed the place where it should have been, break out
		if (Items[Idx]->GetCategoryName() > CategoryName)
		{
			break;
		}
	}

	TSharedPtr<FConversionFunctionItem> NewItem = Items.Insert_GetRef(MakeShared<FConversionFunctionItem>(), Idx);
	NewItem->CategoryPath = CategoryPath;
	return NewItem;
}

TSharedRef<ITableRow> SFieldSelectorMenu::HandleGenerateConversionFunctionCategoryRow(TSharedPtr<FConversionFunctionItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	FText DisplayName = FText::FormatOrdered(FText::FromString("{0} ({1})"), FText::FromString(Item->GetCategoryName()), FText::FromString(LexToString(Item->NumFunctions)));

	return SNew(STableRow<TSharedPtr<FConversionFunctionItem>>, OwnerTable)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(0, 2.0f, 4.0f, 2.0f)
			.AutoWidth()
			[
				SNew(SImage)
				.DesiredSizeOverride(FVector2D(16.0f, 16.0f))
				.Image(FAppStyle::Get().GetBrush("GraphEditor.Function_16x"))
			]
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Font(Item == FilteredConversionFunctionRoot[0] ? FAppStyle::Get().GetFontStyle("NormalText") : FAppStyle::Get().GetFontStyle("BoldFont"))
				.Text(DisplayName)
				.ToolTipText(FText::FromString(Item->GetCategoryName()))
				.HighlightText_Lambda([this]() { return SearchBox.IsValid() ? SearchBox->GetText() : FText::GetEmpty(); })
			]
		];
}

void SFieldSelectorMenu::HandleGetConversionFunctionCategoryChildren(TSharedPtr<FConversionFunctionItem> Item, TArray<TSharedPtr<FConversionFunctionItem>>& OutItems) const
{
	Algo::TransformIf(Item->Children, OutItems,
		[](const TSharedPtr<FConversionFunctionItem>& Item)
		{
			return !Item->GetCategoryName().IsEmpty();
		},
		[](const TSharedPtr<FConversionFunctionItem>& Item)
		{
			return Item;
		});
}

TSharedRef<ITableRow> SFieldSelectorMenu::HandleGenerateConversionFunctionRow(const UFunction* Function, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<const UFunction*>, OwnerTable)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(0, 0, 4.0f, 0)
			.AutoWidth()
			[
				SNew(SImage)
				.DesiredSizeOverride(FVector2D(16.0f, 16.0f))
				.Image(FAppStyle::Get().GetBrush("GraphEditor.Function_16x"))
			]
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(Function->GetDisplayNameText())
				.ToolTipText(Function->GetToolTipText())
				.HighlightText_Lambda([this]() { return SearchBox.IsValid() ? SearchBox->GetText() : FText::GetEmpty(); })
			]
		];
}

void SFieldSelectorMenu::AddConversionFunctionChildrenRecursive(const TSharedPtr<FConversionFunctionItem>& Parent, TArray<const UFunction*>& OutFunctions)
{
	for (const TSharedPtr<FConversionFunctionItem>& Item : Parent->Children)
	{
		if (Item->Function != nullptr)
		{
			int32 Index = 0;
			for (; Index < OutFunctions.Num(); ++Index)
			{
				if (OutFunctions[Index]->GetFName().Compare(Item->Function->GetFName()) > 0)
				{
					break;
				}
			}

			OutFunctions.Insert(Item->Function, Index);
		}
		else
		{
			AddConversionFunctionChildrenRecursive(Item, OutFunctions);
		}
	}
}

void SFieldSelectorMenu::FilterConversionFunctions()
{
	TArray<FString> FilterStrings;
	if (SearchBox.IsValid())
	{
		SearchBox->GetText().ToString().ParseIntoArrayWS(FilterStrings);
	}

	if (FilterStrings.IsEmpty())
	{
		FilteredConversionFunctions = ConversionFunctions;
		return;
	}

	FilteredConversionFunctions.Reset();
	for (const UFunction* Function : ConversionFunctions)
	{
		FString FunctionName = Function->GetName();
		const FString& DisplayName = Function->GetMetaData(FBlueprintMetadata::MD_DisplayName);
		FString MetadataKeywords = Function->GetMetaDataText(FBlueprintMetadata::MD_FunctionKeywords, TEXT("UObjectKeywords"), Function->GetFullGroupName(false)).ToString();

		bool bMatches = true;
		for (const FString& Filter : FilterStrings)
		{
			if (!FunctionName.Contains(Filter) && !DisplayName.Contains(Filter) && !MetadataKeywords.Contains(Filter))
			{
				bMatches = false;
				break;
			}
		}

		if (bMatches)
		{
			FilteredConversionFunctions.Add(Function);
		}
	}
}

void SFieldSelectorMenu::HandleConversionFunctionCategorySelected(TSharedPtr<FConversionFunctionItem> SelectedItem, ESelectInfo::Type)
{
	if (!bIsMenuInitialized)
	{
		return;
	}

	ConversionFunctions.Reset();
	FilteredConversionFunctions.Reset();

	TArray<TSharedPtr<FConversionFunctionItem>> SelectedItems = ConversionFunctionCategoryTree->GetSelectedItems();
	if (SelectedItems.Num() > 0)
	{
		if (BindingList.IsValid())
		{
			BindingList->ClearSources();
		}

		if (ViewModelList.IsValid())
		{
			ViewModelList->ClearSelection();
		}

		if (WidgetList.IsValid())
		{
			WidgetList->ClearSelection();
		}

		for (const TSharedPtr<FConversionFunctionItem>& Item : SelectedItems)
		{
			AddConversionFunctionChildrenRecursive(Item, ConversionFunctions);
		}

		FilterConversionFunctions();
	}

	if (ConversionFunctionList.IsValid())
	{
		ConversionFunctionList->RequestListRefresh();
	}
}

} // namespace UE::MVVM

#undef LOCTEXT_NAMESPACE
