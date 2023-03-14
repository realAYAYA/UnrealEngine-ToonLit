// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SMVVMFieldSelector.h"

#include "Algo/Transform.h"
#include "Bindings/MVVMBindingHelper.h"
#include "ClassViewerModule.h"
#include "Editor.h"
#include "Hierarchy/SReadOnlyHierarchyView.h"
#include "Modules/ModuleManager.h"
#include "MVVMEditorSubsystem.h"
#include "SNegativeActionButton.h"
#include "SPositiveActionButton.h"
#include "SPrimaryButton.h"
#include "SSimpleButton.h"
#include "Styling/MVVMEditorStyle.h"
#include "Styling/SlateIconFinder.h"
#include "WidgetBlueprint.h"
#include "Widgets/SMVVMFieldEntry.h"
#include "Widgets/SMVVMSourceEntry.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SNullWidget.h"

#define LOCTEXT_NAMESPACE "MVVMFieldSelector"

namespace UE::MVVM
{

namespace Private
{

FBindingSource GetSourceFromPath(const UWidgetBlueprint* WidgetBlueprint, const FMVVMBlueprintPropertyPath& Path)
{
	if (Path.IsFromViewModel())
	{
		return FBindingSource::CreateForViewModel(WidgetBlueprint, Path.GetViewModelId());
	}
	else if (Path.IsFromWidget())
	{
		return FBindingSource::CreateForWidget(WidgetBlueprint, Path.GetWidgetName());
	}
	return FBindingSource();
}

} // namespace Private

void SFieldSelector::Construct(const FArguments& InArgs, const UWidgetBlueprint* InWidgetBlueprint, bool bInViewModelProperty)
{
	WidgetBlueprint = InWidgetBlueprint;
	check(InWidgetBlueprint);
	
	BindingMode = InArgs._BindingMode;
	check(BindingMode.IsSet());

	bViewModelProperty = bInViewModelProperty;
	AssignableTo = InArgs._AssignableTo;
	TextStyle = InArgs._TextStyle;

	OnFieldSelectionChangedDelegate = InArgs._OnFieldSelectionChanged;
	check(OnFieldSelectionChangedDelegate.IsBound());

	SelectedField = InArgs._SelectedField;
	check(SelectedField.IsSet());

	CachedSelectedField = SelectedField.Get();

	ShowConversionFunctions = InArgs._ShowConversionFunctions;
	SelectedConversionFunction = InArgs._SelectedConversionFunction;
	CachedSelectedConversionFunction = SelectedConversionFunction.Get(nullptr);
	OnConversionFunctionSelectionChangedDelegate = InArgs._OnConversionFunctionSelectionChanged;

	TSharedPtr<SHorizontalBox> SourceEntryBox;

	ChildSlot
	[
		SAssignNew(ComboButton, SComboButton)
		.ComboButtonStyle(FMVVMEditorStyle::Get(), "FieldSelector.ComboButton")
		.OnGetMenuContent(this, &SFieldSelector::OnGetMenuContent)
		.ContentPadding(FMargin(4, 2))
		.ButtonContent()
		[
			SNew(SOverlay)

			// is a property set?
			+ SOverlay::Slot()
			[
				SAssignNew(SourceEntryBox, SHorizontalBox)
				.Visibility_Lambda([this]() { return CachedSelectedConversionFunction == nullptr && CachedSelectedField.GetFields().Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed; })
			]

			// is a conversion function set?
			+ SOverlay::Slot()
			[
				SNew(SHorizontalBox)
				.Visibility_Lambda([this]() { return CachedSelectedConversionFunction != nullptr ? EVisibility::Visible : EVisibility::Collapsed; })
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
					.Text_Lambda([this]() { return CachedSelectedConversionFunction != nullptr ? CachedSelectedConversionFunction->GetDisplayNameText() : FText::GetEmpty(); })
				]
			]

			// nothing selected
			+ SOverlay::Slot()
			[
				SNew(SBox)
				.Padding(FMargin(8, 0, 8, 0))
				[
					SNew(STextBlock)
					.Visibility_Lambda([this]() { return CachedSelectedField.IsEmpty() && CachedSelectedConversionFunction == nullptr ? EVisibility::Visible : EVisibility::Collapsed; })
					.TextStyle(FAppStyle::Get(), "HintText")
					.Text(LOCTEXT("None", "No field selected"))
				]
			]
		]
	];
	
	if (InArgs._Source.IsValid())
	{
		FixedSource = InArgs._Source;
	}

	if (InArgs._ShowSource)
	{
		SourceEntryBox->AddSlot()
			.Padding(8, 0, 0, 0)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SAssignNew(SelectedSourceWidget, SSourceEntry)
			];

		SourceEntryBox->AddSlot()
			.Padding(6, 0)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SImage)
				.Image(FAppStyle::Get().GetBrush("Icons.ChevronRight"))
			];					
	}

	SourceEntryBox->AddSlot()
		.Padding(0, 0, 8, 0)
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SAssignNew(SelectedEntryWidget, SFieldEntry)
			.TextStyle(TextStyle)
		];

	Refresh();
}

void SFieldSelector::Refresh()
{
	CachedSelectedField = SelectedField.Get();
	CachedSelectedConversionFunction = SelectedConversionFunction.Get(nullptr);

	if (SelectedEntryWidget.IsValid())
	{
		SelectedEntryWidget->SetField(CachedSelectedField);
	}

	if (SelectedSourceWidget.IsValid() && WidgetBlueprint.IsValid())
	{
		SelectedSourceWidget->RefreshSource(Private::GetSourceFromPath(WidgetBlueprint.Get(), CachedSelectedField));
	}
}

TSharedRef<SWidget> SFieldSelector::OnGenerateFieldWidget(FMVVMBlueprintPropertyPath Path) const
{
	return SNew(SFieldEntry)
		.TextStyle(TextStyle)
		.Field(Path);
}

bool SFieldSelector::IsSelectEnabled() const
{
	if (BindingList.IsValid())
	{
		FMVVMBlueprintPropertyPath Path = BindingList->GetSelectedProperty();
		if ((Path.IsFromViewModel() || Path.IsFromWidget()) && !Path.GetBasePropertyPath().IsEmpty())
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

FReply SFieldSelector::OnSelectClicked()
{
	if (BindingList.IsValid())
	{
		SetPropertySelection(BindingList->GetSelectedProperty());
	}

	if (ConversionFunctionList.IsValid())
	{
		TArray<const UFunction*> Selection = ConversionFunctionList->GetSelectedItems();
		SetConversionFunctionSelection(Selection.Num() > 0 ? Selection[0] : nullptr);
	}

	return FReply::Handled();
}

bool SFieldSelector::IsClearEnabled() const
{
	return !CachedSelectedField.IsEmpty() || CachedSelectedConversionFunction != nullptr;
}

FReply SFieldSelector::OnClearClicked()
{
	SetPropertySelection(FMVVMBlueprintPropertyPath());
	SetConversionFunctionSelection(nullptr);
	return FReply::Handled();
}

FReply SFieldSelector::OnCancelClicked()
{
	if (ComboButton.IsValid())
	{
		ComboButton->SetIsOpen(false);
	}

	return FReply::Handled();
}

void SFieldSelector::SetPropertySelection(const FMVVMBlueprintPropertyPath& SelectedPath)
{
	if (ComboButton.IsValid())
	{
		ComboButton->SetIsOpen(false);
	}

	OnFieldSelectionChangedDelegate.Execute(SelectedPath);
	Refresh();
}

TSharedPtr<SFieldSelector::FConversionFunctionItem> SFieldSelector::FindConversionFunctionCategory(const TArray<TSharedPtr<FConversionFunctionItem>>& Items, TArrayView<FString> CategoryNameParts) const
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

void SFieldSelector::OnSearchBoxTextChanged(const FText& NewText)
{
	if (BindingList.IsValid())
	{
		BindingList->SetRawFilterText(NewText);
	}

	if (WidgetList.IsValid())
	{
		WidgetList->SetRawFilterText(NewText);
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

void SFieldSelector::OnViewModelSelected(FBindingSource Source, ESelectInfo::Type)
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

	if (ConversionFunctionCategoryTree.IsValid())
	{
		ConversionFunctionCategoryTree->ClearSelection();
	}

	if (BindingList.IsValid())
	{
		BindingList->AddSources(Selection);
	}
}

void SFieldSelector::OnWidgetSelected(FName WidgetName, ESelectInfo::Type)
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

TSharedRef<ITableRow> SFieldSelector::GenerateViewModelRow(FBindingSource ViewModel, const TSharedRef<STableViewBase>& OwnerTable) const
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

int32 SFieldSelector::FilterConversionFunctionCategoryChildren(const TArray<FString>& FilterStrings, const TArray<TSharedPtr<FConversionFunctionItem>>& SourceArray, TArray<TSharedPtr<FConversionFunctionItem>>& OutDestArray)
{
	int32 NumFunctions = 0;

	for (const TSharedPtr<FConversionFunctionItem>& SourceItem : SourceArray)
	{
		TArray<TSharedPtr<FConversionFunctionItem>> FilteredChildren;

		// check if our name matches the filters
		bool bMatchesFilters = true;

		const FString ItemName = SourceItem->Function != nullptr ? SourceItem->Function->GetName() : FString();
		
		for (const FString& Filter : FilterStrings)
		{
			if (!ItemName.Contains(Filter))
			{
				bMatchesFilters = false;
				break;
			}
		}

		int32 NumChildren = 0;
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

void SFieldSelector::FilterConversionFunctionCategories()
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
	
	ExpandAll(FilteredConversionFunctionRoot, true);
}

int32 SFieldSelector::SortConversionFunctionItemsRecursive(TArray<TSharedPtr<FConversionFunctionItem>>& Items)
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

void SFieldSelector::GenerateConversionFunctionItems()
{
	UMVVMEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
	TArray<UFunction*> AllConversionFunctions = Subsystem->GetAvailableConversionFunctions(WidgetBlueprint.Get(), FMVVMBlueprintPropertyPath(), FMVVMBlueprintPropertyPath());

	// remove all incompatible conversion functions
	for (int32 Idx = 0; Idx < AllConversionFunctions.Num(); ++Idx)
	{
		const UFunction* Function = AllConversionFunctions[Idx];
		const FProperty* ReturnProperty = BindingHelper::GetReturnProperty(Function);
		const FProperty* AssignToProperty = AssignableTo.Get(nullptr);

		if (AssignToProperty != nullptr && !BindingHelper::ArePropertiesCompatible(ReturnProperty, AssignToProperty))
		{
			AllConversionFunctions.RemoveAt(Idx);
			--Idx;
		}
	}

	auto AddFunctionToItem = [](const UFunction* Function, const TSharedPtr<FConversionFunctionItem>& Parent)
	{
		TSharedPtr<FConversionFunctionItem>& Item = Parent->Children.Add_GetRef(MakeShared<FConversionFunctionItem>());
		Item->Function = Function;
		Item->NumFunctions = 1;
		Parent->NumFunctions += 1;
	};

	TSharedPtr<FConversionFunctionItem> CurrentSelectedItem;

	TArray<FString> CategoryPath;

	ConversionFunctionRoot.Reset();
	TSharedPtr<FConversionFunctionItem>& RootItem = ConversionFunctionRoot.Add_GetRef(MakeShared<FConversionFunctionItem>());
	RootItem->CategoryPath = { TEXT("Conversion Functions") };

	for (const UFunction* Function : AllConversionFunctions)
	{
		FText CategoryName = Function->GetMetaDataText("Category");
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

void SFieldSelector::ExpandAllToItem(const UFunction* Function)
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

		ExpandAll(Path, false);
	}
}

void SFieldSelector::ExpandAll(const TArray<TSharedPtr<FConversionFunctionItem>>& Items, bool bRecursive)
{
	for (const TSharedPtr<FConversionFunctionItem>& Item : Items)
	{
		ConversionFunctionCategoryTree->SetItemExpansion(Item, true);

		if (bRecursive)
		{
			ExpandAll(Item->Children, bRecursive);
		}
	}
}

void SFieldSelector::FilterViewModels()
{
	FilteredViewModelSources.Reset();

	TArray<FString> FilterStrings;
	if (SearchBox.IsValid())
	{
		SearchBox->GetText().ToString().ParseIntoArrayWS(FilterStrings);
	}

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

TSharedRef<SWidget> SFieldSelector::CreateSourcePanel()
{
	if (!WidgetBlueprint.IsValid())
	{
		return SNullWidget::NullWidget;
	}

	// show source picker
	TSharedPtr<SWidget> SourcePicker;
	if (bViewModelProperty)
	{
		if (FixedSource.IsSet())
		{
			ViewModelSources.Add(FixedSource.GetValue());
		}
		else
		{
			UMVVMEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
			ViewModelSources = EditorSubsystem->GetAllViewModels(WidgetBlueprint.Get());
		}

		FilterViewModels();

		ViewModelList = SNew(SListView<FBindingSource>)
			.ListItemsSource(&FilteredViewModelSources)
			.OnGenerateRow(this, &SFieldSelector::GenerateViewModelRow)
			.SelectionMode(ESelectionMode::Multi)
			.OnSelectionChanged(this, &SFieldSelector::OnViewModelSelected);

		FBindingSource SelectedSource;
		for (const FBindingSource& Source : FilteredViewModelSources)
		{
			if (Source.ViewModelId == CachedSelectedField.GetViewModelId())
			{
				SelectedSource = Source;
			}
		}

		if (SelectedSource.IsValid())
		{
			ViewModelList->SetItemSelection(SelectedSource, true);
		}

		SourcePicker = ViewModelList;
	}
	else
	{
		TArray<FName> ShowOnly;
		if (FixedSource.IsSet())
		{
			ShowOnly.Add(FixedSource.GetValue().Name);
		}

		WidgetList = SNew(SReadOnlyHierarchyView, WidgetBlueprint.Get())
			.OnSelectionChanged(this, &SFieldSelector::OnWidgetSelected)
			.SelectionMode(ESelectionMode::Multi)
			.ShowSearch(false)
			.ShowOnly(ShowOnly);

		if (CachedSelectedField.IsFromWidget())
		{
			WidgetList->SetSelectedWidget(CachedSelectedField.GetWidgetName());
		}

		SourcePicker = WidgetList;
	}

	if (ShowConversionFunctions.Get(false))
	{
		GenerateConversionFunctionItems();

		FilterConversionFunctionCategories();
		
		TSharedRef<SWidget> StackedSourcePicker =
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SourcePicker.ToSharedRef()
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SAssignNew(ConversionFunctionCategoryTree, STreeView<TSharedPtr<FConversionFunctionItem>>)
					.SelectionMode(ESelectionMode::Multi)
					.TreeItemsSource(&FilteredConversionFunctionRoot)
					.OnGenerateRow(this, &SFieldSelector::GenerateConversionFunctionCategoryRow)
					.OnSelectionChanged(this, &SFieldSelector::OnConversionFunctionCategorySelected)
					.OnGetChildren(this, &SFieldSelector::GetConversionFunctionCategoryChildren)
				];

		SourcePicker = StackedSourcePicker;
	}

	return SourcePicker.ToSharedRef();
}

TSharedRef<SWidget> SFieldSelector::OnGetMenuContent()
{
	if (!WidgetBlueprint.IsValid())
	{
		return SNullWidget::NullWidget;
	}

	bIsMenuInitialized = false;

	ConversionFunctionCategoryTree.Reset();
	FilteredConversionFunctionRoot.Reset();
	ConversionFunctions.Reset();
	FilteredConversionFunctions.Reset();
	ConversionFunctionRoot.Reset();
	ViewModelSources.Reset();
	ViewModelList.Reset();
	WidgetList.Reset();

	TSharedRef<SVerticalBox> VBox = SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Top)
		.Padding(0, 4, 0, 4)
		.AutoHeight()
		[
			SAssignNew(SearchBox, SSearchBox)
			.OnTextChanged(this, &SFieldSelector::OnSearchBoxTextChanged)
		];

	// If we're showing conversion functions, we don't want to set the AssignableTo property of SSourceBindingList, because then it will only show exact matches, 
	// and since we're also showing conversion functions we know that's not what the user wants.
	// However, in the case we're not showing conversion functions, we want only exact matches.
	const FProperty* AssignableToProperty = nullptr;
	const bool bShowConversionFunctions = ShowConversionFunctions.Get(false);
	if (!bShowConversionFunctions)
	{
		AssignableToProperty = AssignableTo.Get(nullptr);
	}

	BindingList = SNew(SSourceBindingList, WidgetBlueprint.Get())
		.ShowSearchBox(false)
		.OnDoubleClicked(this, &SFieldSelector::SetPropertySelection)
		.FieldVisibilityFlags(GetFieldVisibilityFlags())
		.AssignableTo(AssignableToProperty);

	if (FixedSource.IsSet())
	{
		// Single fixed source, don't show the separate source panel.
		BindingList->AddSource(FixedSource.GetValue());
	}
	else if (!CachedSelectedField.IsEmpty())
	{
		FBindingSource Source = CachedSelectedField.IsFromViewModel() ? 
			FBindingSource::CreateForViewModel(WidgetBlueprint.Get(), CachedSelectedField.GetViewModelId()) : 
			FBindingSource::CreateForWidget(WidgetBlueprint.Get(), CachedSelectedField.GetWidgetName());
		BindingList->AddSource(Source);
		BindingList->SetSelectedProperty(CachedSelectedField);
	}
	
	TSharedPtr<SVerticalBox> BindingListVBox;
		
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
				.MinSize(100)
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[
						CreateSourcePanel()
					]
				]
				+ SSplitter::Slot()
				.Value(0.5f)
				.MinSize(100)
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[
						SAssignNew(BindingListVBox, SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							BindingList.ToSharedRef()
						]
					]
				]
			]
		];

	if (bShowConversionFunctions)
	{
		BindingListVBox->AddSlot()
			.AutoHeight()
			[
				SAssignNew(ConversionFunctionList, SListView<const UFunction*>)
				.SelectionMode(ESelectionMode::Single)
				.ListItemsSource(&FilteredConversionFunctions)
				.OnMouseButtonDoubleClick(this, &SFieldSelector::SetConversionFunctionSelection)
				.OnGenerateRow(this, &SFieldSelector::GenerateConversionFunctionRow)
			];

		if (CachedSelectedConversionFunction != nullptr)
		{
			ExpandAllToItem(CachedSelectedConversionFunction);
			ConversionFunctionList->SetItemSelection(CachedSelectedConversionFunction, true);
		}
	}

	VBox->AddSlot()
		.Padding(4, 4, 4, 0)
		.HAlign(HAlign_Right)
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SPrimaryButton)
				.OnClicked(this, &SFieldSelector::OnSelectClicked)
				.IsEnabled(this, &SFieldSelector::IsSelectEnabled)
				.Text(LOCTEXT("Select", "Select"))
			]
			+ SHorizontalBox::Slot()
			.Padding(FAppStyle::Get().GetMargin("StandardDialog.SlotPadding"))
			.AutoWidth()
			[
				SNew(SButton)
				.OnClicked(this, &SFieldSelector::OnClearClicked)
				.IsEnabled(this, &SFieldSelector::IsClearEnabled)
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
				.OnClicked(this, &SFieldSelector::OnCancelClicked)
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Cancel", "Cancel"))
				]
			]
		];

	TSharedRef<SWidget> MenuWidget = 
		SNew(SBox)
		.MinDesiredWidth(400)
		.MinDesiredHeight(200)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
			.Padding(FMargin(8, 2, 8, 3))
			[
				VBox
			]
		];

	ComboButton->SetMenuContentWidgetToFocus(SearchBox);

	bIsMenuInitialized = true;
	return MenuWidget;
}

EFieldVisibility SFieldSelector::GetFieldVisibilityFlags() const
{
	EFieldVisibility Flags = EFieldVisibility::None;

	EMVVMBindingMode Mode = BindingMode.Get();
	if (bViewModelProperty)
	{
		if (IsForwardBinding(Mode))
		{
			Flags |= EFieldVisibility::Readable;

			if (!IsOneTimeBinding(Mode))
			{
				Flags |= EFieldVisibility::Notify;
			}
		}
		if (IsBackwardBinding(Mode))
		{
			Flags |= EFieldVisibility::Writable;
		}
	}
	else
	{
		if (IsForwardBinding(Mode))
		{
			Flags |= EFieldVisibility::Writable;
		}
		if (IsBackwardBinding(Mode))
		{
			Flags |= EFieldVisibility::Readable;

			if (!IsOneTimeBinding(Mode))
			{
				Flags |= EFieldVisibility::Notify;
			}
		}
	}

	return Flags;
}

TSharedPtr<SFieldSelector::FConversionFunctionItem> SFieldSelector::FindOrCreateItemForCategory(TArray<TSharedPtr<FConversionFunctionItem>>& Items, TArrayView<FString> CategoryPath)
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

TSharedRef<ITableRow> SFieldSelector::GenerateConversionFunctionCategoryRow(TSharedPtr<FConversionFunctionItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
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

void SFieldSelector::GetConversionFunctionCategoryChildren(TSharedPtr<FConversionFunctionItem> Item, TArray<TSharedPtr<FConversionFunctionItem>>& OutItems) const
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

TSharedRef<ITableRow> SFieldSelector::GenerateConversionFunctionRow(const UFunction* Function, const TSharedRef<STableViewBase>& OwnerTable)
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

void SFieldSelector::AddConversionFunctionChildrenRecursive(const TSharedPtr<FConversionFunctionItem>& Parent, TArray<const UFunction*>& OutFunctions)
{
	for (const TSharedPtr<FConversionFunctionItem>& Item : Parent->Children)
	{
		if (Item->Function != nullptr)
		{
			int32 Index = 0;
			for (; Index < OutFunctions.Num(); ++Index)
			{
				if (OutFunctions[Index]->GetName() > Item->Function->GetName())
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

void SFieldSelector::FilterConversionFunctions()
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
	
	for (const UFunction* Function : ConversionFunctions)
	{
		const FString FunctionName = Function->GetName();

		bool bMatches = true;
		for (const FString& Filter : FilterStrings)
		{
			if (!FunctionName.Contains(Filter))
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

void SFieldSelector::OnConversionFunctionCategorySelected(TSharedPtr<FConversionFunctionItem> SelectedItem, ESelectInfo::Type)
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

void SFieldSelector::SetConversionFunctionSelection(const UFunction* SelectedFunction)
{
	if (ComboButton.IsValid())
	{
		ComboButton->SetIsOpen(false);
	}

	OnConversionFunctionSelectionChangedDelegate.ExecuteIfBound(SelectedFunction);
	Refresh();
}

} // namespace UE::MVVM

#undef LOCTEXT_NAMESPACE
