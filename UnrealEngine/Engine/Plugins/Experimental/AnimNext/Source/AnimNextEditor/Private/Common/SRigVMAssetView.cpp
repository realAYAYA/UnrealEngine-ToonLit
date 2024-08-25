// Copyright Epic Games, Inc. All Rights Reserved.

#include "SRigVMAssetView.h"

#include "AnimNextRigVMAssetEntry.h"
#include "Param/AnimNextParameterBlock.h"
#include "Param/AnimNextParameterBlock_EditorData.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "DetailLayoutBuilder.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "EditorUtils.h"
#include "InstancedPropertyBagStructureDataProvider.h"
#include "UncookedOnlyUtils.h"
#include "RigVMAssetViewMenuContext.h"
#include "PropertyBagDetails.h"
#include "SourceControlOperations.h"
#include "SPinTypeSelector.h"
#include "Framework/Commands/GenericCommands.h"
#include "RevisionControlStyle/RevisionControlStyle.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "ToolMenus.h"
#include "ScopedTransaction.h"
#include "Param/AnimNextParameterBlockParameter.h"
#include "Param/AnimNextParameterBlockGraph.h"
#include "Framework/Application/SlateApplication.h"
#include "PropertyEditorModule.h"
#include "ISinglePropertyView.h"
#include "ObjectEditorUtils.h"
#include "Misc/NotifyHook.h"

#define LOCTEXT_NAMESPACE "SRigVMAssetView"

namespace UE::AnimNext::Editor
{

namespace ParameterBlockView
{

static FName ContextMenuName(TEXT("AnimNext.RigVMAssetView.ContextMenu"));
static FName Column_RevisionControl(TEXT("RevisionControl"));
static FName Column_ModifiedStatus(TEXT("ModifiedStatus"));
static FName Column_Name(TEXT("Name"));
static FName Column_Type(TEXT("Type"));
static FName Column_Value(TEXT("Value"));

static const FName NAME_Category("Category");

}

TMap<FName, SRigVMAssetView::FCategoryWidgetFactoryFunction> SRigVMAssetView::CategoryFactories;

// An entry category displayed in the parameters table
template<typename ItemType>
class SCategoryHeaderTableRow : public STableRow<ItemType>
{
public:
	SLATE_BEGIN_ARGS(SCategoryHeaderTableRow)
		{}
		SLATE_DEFAULT_SLOT(typename SCategoryHeaderTableRow::FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
	{
		STableRow<ItemType>::ChildSlot
			.Padding(0.0f, 2.0f, .0f, 0.0f)
			[
				SAssignNew(ContentBorder, SBorder)
					.BorderImage(this, &SCategoryHeaderTableRow::GetBackgroundImage)
					.Padding(FMargin(3.0f, 5.0f))
					[
						SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.VAlign(VAlign_Center)
							.Padding(5.0f)
							.AutoWidth()
							[
								SNew(SExpanderArrow, STableRow< ItemType >::SharedThis(this))
							]
							+ SHorizontalBox::Slot()
							.VAlign(VAlign_Center)
							.AutoWidth()
							[
								InArgs._Content.Widget
							]
					]
			];

		STableRow < ItemType >::ConstructInternal(
			typename STableRow< ItemType >::FArguments()
			.Style(FAppStyle::Get(), "DetailsView.TreeView.TableRow")
			.ShowSelection(false),
			InOwnerTableView
		);
	}

	const FSlateBrush* GetBackgroundImage() const
	{
		if (STableRow<ItemType>::IsHovered())
		{
			return FAppStyle::Get().GetBrush("Brushes.Secondary");
		}
		else
		{
			return FAppStyle::Get().GetBrush("Brushes.Header");
		}
	}

	virtual void SetContent(TSharedRef< SWidget > InContent) override
	{
		ContentBorder->SetContent(InContent);
	}

	virtual void SetRowContent(TSharedRef< SWidget > InContent) override
	{
		ContentBorder->SetContent(InContent);
	}

	virtual const FSlateBrush* GetBorder() const
	{
		return nullptr;
	}

	FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
	{
		if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			STableRow<ItemType>::ToggleExpansion();
			return FReply::Handled();
		}
		else
		{
			return FReply::Unhandled();
		}
	}
private:
	TSharedPtr<SBorder> ContentBorder;
};

// An entry displayed in the parameters view
struct FRigVMAssetViewEntry
{

	FRigVMAssetViewEntry() = default;

	FRigVMAssetViewEntry(UAnimNextRigVMAssetEntry* InEntry)
		: WeakEntry(InEntry)
	{}

	FRigVMAssetViewEntry(FName InCategoryType, const FText& InCategoryName)
		: CategoryTypeName(InCategoryType)
		, CategoryText(InCategoryName)
	{}

	bool PassesFilter(const FString& InFilterText) const
	{
		if(UAnimNextRigVMAssetEntry* Entry = WeakEntry.Get())
		{
			return Entry->GetDisplayName().ToString().Contains(InFilterText);
		}

		return false;
	}

	void ResetChildren(int32 NewSize = 0)
	{
		Children.Reset(NewSize);
	}

	void GetChildrenRecursive(TArray< TSharedRef<FRigVMAssetViewEntry> >& OutChildren)
	{
		for (TSharedRef<FRigVMAssetViewEntry>& Entry : Children)
		{
			OutChildren.Add(Entry);
			Entry->GetChildrenRecursive(OutChildren);
		}
	}

	// Ptr to the underlying entry
	TWeakObjectPtr<UAnimNextRigVMAssetEntry> WeakEntry;

	// Widget used to rename items
	TWeakPtr<SInlineEditableTextBlock> NameWidget;

	FName CategoryTypeName;
	FText CategoryText;

	// Children when entry is a category
	TArray< TSharedRef<FRigVMAssetViewEntry> > Children;

	// Flag to indicate a rename was requested
	bool bRenameWhenScrolledIntoView = false;
};

void SRigVMAssetView::RegisterCategoryFactory(FName InCategory, FCategoryWidgetFactoryFunction&& InFunction)
{
	CategoryFactories.Add(InCategory, MoveTemp(InFunction));
}

void SRigVMAssetView::UnregisterCategoryFactory(FName InCategory)
{
	CategoryFactories.Remove(InCategory);
}

void SRigVMAssetView::Construct(const FArguments& InArgs, UAnimNextRigVMAssetEditorData* InEditorData)
{
	using namespace ParameterBlockView;
	
	check(InEditorData);

	EditorData = InEditorData;
	EditorData->SetFlags(RF_Transactional);

	TMap<FName, FText> AllCategories;
	for (TSubclassOf<UAnimNextRigVMAssetEntry> EntryClass : EditorData->GetEntryClasses())
	{
		if(const FString* CategoryMetaData = EntryClass->FindMetaData(NAME_Category))
		{
			AllCategories.Add(**CategoryMetaData, FObjectEditorUtils::GetCategoryText(EntryClass));
		}
	}

	for(const TPair<FName, FText>& CategoryPair : AllCategories)
	{
		Categories.Add(MakeShared<FRigVMAssetViewEntry>(CategoryPair.Key, CategoryPair.Value));
	}

	// Cache asset data for comparisons/filtering
	AssetData = FAssetData(UncookedOnly::FUtils::GetAsset(EditorData));

	OnSelectionChangedDelegate = InArgs._OnSelectionChanged;
	OnOpenGraphDelegate = InArgs._OnOpenGraph;
	OnDeleteEntriesDelegate = InArgs._OnDeleteEntries;

	EditorData->ModifiedDelegate.AddSP(this, &SRigVMAssetView::HandleBlockModified);

	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(2.0f)
			[
				SNew(SSearchBox)
				.OnTextChanged_Lambda([this](FText InText)
				{
					FilterText = InText;
					RefreshFilter();
				})
			]
		]
		+SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SAssignNew(EntriesList, STreeView<TSharedRef<FRigVMAssetViewEntry>>)
			.TreeItemsSource(&FilteredEntries)
			.OnGenerateRow(this, &SRigVMAssetView::HandleGenerateRow)
			.OnGetChildren(this, &SRigVMAssetView::HandleGetChildren)
			.OnItemScrolledIntoView(this, &SRigVMAssetView::HandleItemScrolledIntoView)
			.OnSelectionChanged(this, &SRigVMAssetView::HandleSelectionChanged)
			.ItemHeight(20.0f)
			.HeaderRow(
				SNew(SHeaderRow)
				+SHeaderRow::Column(Column_RevisionControl)
				.DefaultLabel(FText::GetEmpty())
				.FixedWidth(24.0f)
				.HeaderContent()
				[
					SNew(SBox)
					.WidthOverride(16.0f)
					.HeightOverride(16.0f)
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					[
						SNew(SImage)
						.ColorAndOpacity(FSlateColor::UseForeground())
						.Image(FRevisionControlStyleManager::Get().GetBrush("RevisionControl.Icon"))
						.ToolTipText(LOCTEXT("RevisionControlStatusTooltip", "Revision control status of this parameter"))
					]
				]

				+SHeaderRow::Column(Column_ModifiedStatus)
				.DefaultLabel(FText::GetEmpty())
				.FixedWidth(24.0f)
				.HeaderContent()
				[
					SNew(SBox)
					.WidthOverride(16.0f)
					.HeightOverride(16.0f)
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					[
						SNew(SImage)
						.ColorAndOpacity(FSlateColor::UseForeground())
						.Image(FAppStyle::GetBrush("ContentBrowser.ContentDirty"))
						.ToolTipText(LOCTEXT("ModifiedStatusTooltip", "Modified status of this parameter"))
					]
				]

				+SHeaderRow::Column(Column_Name)
				.DefaultLabel(LOCTEXT("NameColumnHeader", "Name"))
				.ToolTipText(LOCTEXT("NameColumnHeaderTooltip", "The name of the parameter"))
				.FillWidth(20.0f)

				+SHeaderRow::Column(Column_Type)
				.DefaultLabel(LOCTEXT("TypeColumnHeader", "Type"))
				.ToolTipText(LOCTEXT("TypeColumnHeaderTooltip", "The type of the parameter"))
				.ManualWidth(145.0f)

				+SHeaderRow::Column(Column_Value)
				.DefaultLabel(LOCTEXT("ValueColumnHeader", "Value"))
				.ToolTipText(LOCTEXT("ValueColumnHeaderTooltip", "The value or binding to the parameter"))
				.FillWidth(10.0f)
			)
		]
	];

	// Update revision control status of all our files
	if (ISourceControlModule::Get().IsEnabled())
	{
		TArray<UPackage*> Packages = EditorData->GetPackage()->GetExternalPackages();

		ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
		SourceControlProvider.Execute(ISourceControlOperation::Create<FUpdateStatus>(), Packages);
	}

	BindCommands();

	RegisterActiveTimer(1.0f / 60.0f, FWidgetActiveTimerDelegate::CreateLambda(
		[this](double InCurrentTime, float InDeltaTime)
		{
			if(bRefreshRequested)
			{
				RefreshEntries();
				bRefreshRequested = false;
			}

			return EActiveTimerReturnType::Continue;
		}));

	RefreshEntries();

	for (TSharedRef<FRigVMAssetViewEntry>& Category : Categories)
	{
		EntriesList->SetItemExpansion(Category, true);
	}
}

FReply SRigVMAssetView::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (UICommandList.IsValid() && UICommandList->ProcessCommandBindings(InKeyEvent))
	{
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void SRigVMAssetView::RequestRefresh()
{
	bRefreshRequested = true;
}

template<typename ItemType, typename ComparisonType>
void RestoreExpansionState(TSharedPtr< STreeView<ItemType> > InTree, const TArray<ItemType>& ItemSource, const TSet<ItemType>& OldExpansionState, ComparisonType ComparisonFunction)
{
	check(InTree.IsValid());

	// Iterate over new tree items
	for (int32 ItemIdx = 0; ItemIdx < ItemSource.Num(); ItemIdx++)
	{
		ItemType NewItem = ItemSource[ItemIdx];

		// Look through old expansion state
		for (typename TSet<ItemType>::TConstIterator OldExpansionIter(OldExpansionState); OldExpansionIter; ++OldExpansionIter)
		{
			const ItemType OldItem = *OldExpansionIter;
			// See if this matches this new item
			if (ComparisonFunction(OldItem, NewItem))
			{
				// It does, so expand it
				InTree->SetItemExpansion(NewItem, true);
			}
		}
	}
}

static bool CompareGraphActionNode(TSharedRef<FRigVMAssetViewEntry> A, TSharedRef<FRigVMAssetViewEntry> B)
{
	if (A->CategoryTypeName != B->CategoryTypeName)
	{
		return false;
	}

	return true;
}

void SRigVMAssetView::RefreshEntries()
{
	// First, save off current expansion state
	TSet< TSharedRef<FRigVMAssetViewEntry> > OldExpansionState;
	EntriesList->GetExpandedItems(OldExpansionState);

	Entries.Reset();

	TArray<TSharedRef<FRigVMAssetViewEntry>> EntriesToSelect;

	for (UAnimNextRigVMAssetEntry* SubEntry : EditorData->GetAllEntries())
	{
		TSharedRef<FRigVMAssetViewEntry> NewEntry = MakeShared<FRigVMAssetViewEntry>(SubEntry);

		Entries.Add(NewEntry);

		if (PendingSelection.Contains(SubEntry))
		{
			EntriesToSelect.Add(NewEntry);
		}
	}

	// Restore the expanded items
	TArray< TSharedRef<FRigVMAssetViewEntry> > AllEntries;
	for (const TSharedRef<FRigVMAssetViewEntry> & Entry : Entries)
	{
		AllEntries.Add(Entry);
		Entry->GetChildrenRecursive(AllEntries);
	}
	RestoreExpansionState< TSharedRef<FRigVMAssetViewEntry> >(EntriesList, AllEntries, OldExpansionState, CompareGraphActionNode);

	PendingSelection.Empty();

	RefreshFilter();

	if(EntriesToSelect.Num() > 0)
	{
		EntriesList->SetItemSelection(EntriesToSelect, true);
	}
}

void SRigVMAssetView::RefreshFilter()
{
	using namespace ParameterBlockView;

	FilteredEntries = Categories;

	for (TSharedRef<FRigVMAssetViewEntry>& Category : Categories)
	{
		Category->ResetChildren();
	}

	// add the entries as categories children
	const FString FilterTextAsString = FilterText.ToString();
	for (const TSharedRef<FRigVMAssetViewEntry>& Entry : Entries)
	{
		if (Entry->PassesFilter(FilterTextAsString))
		{
			const UObject* BlockEntry = Entry->WeakEntry.Get();
			const UClass* EntryClass = BlockEntry->GetClass();
			const FString& CategoryMetaData = EntryClass->GetMetaData(NAME_Category);
			TSharedRef<FRigVMAssetViewEntry> CategoryEntry = GetCategoryEntry(*CategoryMetaData);
			CategoryEntry->Children.Add(Entry);
		}
	}

	EntriesList->RequestListRefresh();
}

void SRigVMAssetView::BindCommands()
{
	UICommandList = MakeShared<FUICommandList>();

	UICommandList->MapAction(FGenericCommands::Get().Delete,
		FExecuteAction::CreateSP(this, &SRigVMAssetView::HandleDelete),
		FCanExecuteAction::CreateSP(this, &SRigVMAssetView::HasValidSelection));

	UICommandList->MapAction(FGenericCommands::Get().Rename,
		FExecuteAction::CreateSP(this, &SRigVMAssetView::HandleRename),
		FCanExecuteAction::CreateSP(this, &SRigVMAssetView::HasValidSingleSelection));
}

void SRigVMAssetView::HandleBlockModified(UAnimNextRigVMAssetEditorData* InEditorData)
{
	check(InEditorData == EditorData);

	RequestRefresh();
}

TSharedRef<SWidget> SRigVMAssetView::HandleGetContextContent()
{
	using namespace ParameterBlockView;
	
	UToolMenus* ToolMenus = UToolMenus::Get();

	if(!ToolMenus->IsMenuRegistered(ContextMenuName))
	{
		UToolMenu* Menu = ToolMenus->RegisterMenu(ContextMenuName);

		FToolMenuSection& Section = Menu->AddSection("EntryOperations", LOCTEXT("EntryOperationsMenuSection", "Entry"));
		Section.AddMenuEntry(FGenericCommands::Get().Delete);
		Section.AddMenuEntry(FGenericCommands::Get().Rename);
	}

	URigVMAssetViewMenuContext* MenuContext = NewObject<URigVMAssetViewMenuContext>();
	MenuContext->RigVMAssetView = SharedThis(this);
	return ToolMenus->GenerateWidget(ContextMenuName, FToolMenuContext(MenuContext));
}

void SRigVMAssetView::HandleDelete()
{
	auto DeleteSelected = [this](TArray<TSharedRef<FRigVMAssetViewEntry>> SelectedItems)
	{
		TArray<UAnimNextRigVMAssetEntry*> EntriesToRemove;
		Algo::Transform(SelectedItems, EntriesToRemove, [](const TSharedRef<FRigVMAssetViewEntry>& InEntry) { return InEntry->WeakEntry.Get(); });

		bool bAnyEntriesRemoved = false;
		{
			FScopedTransaction Transaction(FText::FormatOrdered(LOCTEXT("DeleteEntry", "Delete asset {0}|plural(one=entry,other=entries)"), EntriesList->GetNumItemsSelected()));
			bAnyEntriesRemoved = EditorData->RemoveEntries(EntriesToRemove);
		}
		
		if(bAnyEntriesRemoved)
		{
			// Remove all the entries that werent actually removed
			EntriesToRemove.RemoveAll([this](UAnimNextRigVMAssetEntry* InEntry) { return EditorData->FindEntry(InEntry->GetEntryName()) != nullptr; });
			OnDeleteEntriesDelegate.ExecuteIfBound(EntriesToRemove);
		}
	};

	if (EntriesList->GetNumItemsSelected() > 0)
	{
		TArray<TSharedRef<FRigVMAssetViewEntry>> SelectedItems = EntriesList->GetSelectedItems();
		DeleteSelected(SelectedItems);
	}
}

void SRigVMAssetView::HandleRename()
{
	auto RenameSelected = [this](TSharedPtr<STreeView<TSharedRef<FRigVMAssetViewEntry>>>& InEntriesList, const TSharedRef<FRigVMAssetViewEntry>& SelectedItem)
	{
		SelectedItem->bRenameWhenScrolledIntoView = true;
		InEntriesList->RequestScrollIntoView(SelectedItem);
	};

	if(EntriesList->GetNumItemsSelected() == 1)
	{
		TArray<TSharedRef<FRigVMAssetViewEntry>> SelectedItems = EntriesList->GetSelectedItems();
		RenameSelected(EntriesList, SelectedItems[0]);
	}
}

bool SRigVMAssetView::HasValidSelection() const
{
	return EntriesList->GetNumItemsSelected() > 0;
}

bool SRigVMAssetView::HasValidSingleSelection() const
{
	return EntriesList->GetNumItemsSelected() == 1;
}

void SRigVMAssetView::HandleItemScrolledIntoView(TSharedRef<FRigVMAssetViewEntry> InEntry, const TSharedPtr<ITableRow>& InWidget)
{
	if(InEntry->bRenameWhenScrolledIntoView)
	{
		InEntry->bRenameWhenScrolledIntoView = false;

		if(TSharedPtr<SInlineEditableTextBlock> NameWidget = InEntry->NameWidget.Pin())
		{
			NameWidget->EnterEditingMode();
		}
	}
}

class SRigVMAssetViewRow : public SMultiColumnTableRow<TSharedRef<FRigVMAssetViewEntry>>, private FNotifyHook
{
	SLATE_BEGIN_ARGS(SRigVMAssetViewRow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, TSharedRef<SRigVMAssetView> InView, TSharedRef<FRigVMAssetViewEntry> InEntry)
	{
		WeakView = InView;
		Entry = InEntry;

		SMultiColumnTableRow<TSharedRef<FRigVMAssetViewEntry>>::Construct( SMultiColumnTableRow<TSharedRef<FRigVMAssetViewEntry>>::FArguments(), InOwnerTableView);
	}

	virtual FReply OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override
	{
		if(IAnimNextRigVMGraphInterface* GraphInterface = Cast<IAnimNextRigVMGraphInterface>(Entry->WeakEntry.Get()))
		{
			if(TSharedPtr<SRigVMAssetView> View = WeakView.Pin())
			{
				View->OnOpenGraphDelegate.ExecuteIfBound(GraphInterface->GetRigVMGraph());
			}
			return FReply::Handled();
		}

		return SMultiColumnTableRow<TSharedRef<FRigVMAssetViewEntry>>::OnMouseButtonDoubleClick(InMyGeometry, InMouseEvent);
	}
	
	//~ Begin FNotifyHook Interface
	virtual void NotifyPreChange(FProperty* PropertyAboutToChange) override
	{
		if (TSharedPtr<SRigVMAssetView> View = WeakView.Pin())
		{
			if (UAnimNextRigVMAssetEditorData* EditorData = View->EditorData)
			{
				if (UAnimNextRigVMAssetEntry* AssetEntry = EditorData->FindEntry(PropertyAboutToChange->GetFName()))
				{
					UAnimNextRigVMAsset* Asset = UE::AnimNext::UncookedOnly::FUtils::GetAsset(EditorData);

					// needed to enable the transaction when we modify the PropertyBag
					// TODO: remove this by moving defaults into entries
					Asset->Modify();
				}
			}
		}
	}

	virtual void NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged) override
	{
		if (TSharedPtr<SRigVMAssetView> View = WeakView.Pin())
		{
			if (UAnimNextRigVMAssetEditorData* EditorData = View->EditorData)
			{
				if (UAnimNextRigVMAssetEntry* AssetEntry = EditorData->FindEntry(PropertyChangedEvent.GetMemberPropertyName()))
				{
					// Needed to show the changed sign a the table when we modify the PropertyBag
					// TODO: remove this by moving defaults into entries
					AssetEntry->MarkPackageDirty(); 
				}
			}
		}
	}
	// End of FNotifyHook

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& InColumnName) override
	{
		using namespace ParameterBlockView;
		
		if(InColumnName == Column_RevisionControl)
		{
			return
				SNew(SBox)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.HeightOverride(20.0f)
				[
					SNew(SImage)
					.ToolTipText_Lambda([this]()
					{
						if(UAnimNextRigVMAssetEntry* AssetEntry = Entry->WeakEntry.Get())
						{
							ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
							UPackage* Package = AssetEntry->GetExternalPackage();
							if(FSourceControlStatePtr State = SourceControlProvider.GetState(Package, EStateCacheUsage::Use))
							{
								return FText::Format(LOCTEXT("RevisionControlStatusFormat", "File: {0}\nStatus: {1}"), FText::FromName(Package->GetFName()),  State->GetDisplayTooltip());
							}
						}

						return LOCTEXT("RevisionControlStatus", "Revision control status of this parameter");
					})
					.Image_Lambda([this]() -> const FSlateBrush*
					{
						if(UAnimNextRigVMAssetEntry* AssetEntry = Entry->WeakEntry.Get())
						{
							ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
							if(FSourceControlStatePtr State = SourceControlProvider.GetState(AssetEntry->GetExternalPackage(), EStateCacheUsage::Use))
							{
								return State->GetIcon().GetSmallIcon();
							}
						}
						return nullptr;
					})
				];
		}
		else if(InColumnName == Column_ModifiedStatus)
		{
			return
				SNew(SBox)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.HeightOverride(20.0f)
				[
					SNew(SImage)
					.ToolTipText_Lambda([this]()
					{
						FTextBuilder TextBuilder;
						TextBuilder.AppendLine(LOCTEXT("ModifiedTooltip", "Modified Status"));

						if(UAnimNextRigVMAssetEntry* AssetEntry = Entry->WeakEntry.Get())
						{
							const UPackage* ExternalPackage = AssetEntry->GetExternalPackage();
							check(ExternalPackage);
							if(ExternalPackage->IsDirty())
							{
								TextBuilder.AppendLine(FText::FromName(ExternalPackage->GetFName()));
							}
						}

						return TextBuilder.ToText();
					})
					.Image_Lambda([this]() -> const FSlateBrush*
					{
						if(UAnimNextRigVMAssetEntry* AssetEntry = Entry->WeakEntry.Get())
						{
							bool bIsDirty = false;
							const UPackage* ExternalPackage = AssetEntry->GetExternalPackage();
							check(ExternalPackage);
							if(ExternalPackage->IsDirty())
							{
								bIsDirty = true;
							}


							return bIsDirty ? FAppStyle::GetBrush("ContentBrowser.ContentDirty") : nullptr;
						}
						return nullptr;
					})
				];
		}
		else if(InColumnName == Column_Type)
		{
			if(Entry->WeakEntry.Get()->Implements<UAnimNextRigVMParameterInterface>())
			{
				return
					SNew(SBox)
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					[
						SNew(SPinTypeSelector, FGetPinTypeTree::CreateStatic(&Editor::FUtils::GetFilteredVariableTypeTree))
							.TargetPinType_Lambda([this]()
							{
								if(const IAnimNextRigVMParameterInterface* Binding = Cast<IAnimNextRigVMParameterInterface>(Entry->WeakEntry.Get()))
								{
									return UncookedOnly::FUtils::GetPinTypeFromParamType(Binding->GetParamType());
								}

								return FEdGraphPinType();
							})
							.OnPinTypeChanged_Lambda([this](const FEdGraphPinType& PinType)
							{
								if(IAnimNextRigVMParameterInterface* Binding = Cast<IAnimNextRigVMParameterInterface>(Entry->WeakEntry.Get()))
								{
									const FAnimNextParamType ParamType = UncookedOnly::FUtils::GetParamTypeFromPinType(PinType);
									if(ParamType.IsValid())
									{
										Binding->SetParamType(ParamType);
									}
								}
							})
							.Schema(GetDefault<UPropertyBagSchema>())
							.bAllowArrays(true)
							.TypeTreeFilter(ETypeTreeFilter::None)
							.Font(IDetailLayoutBuilder::GetDetailFont())
					];
			}
		}
		else if(InColumnName == Column_Name)
		{
			return
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(SExpanderArrow, SharedThis(this))
					.Visibility_Lambda([this]() { return DoesItemHaveChildren() ? EVisibility::Visible : EVisibility::Collapsed; })
				]
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SAssignNew(Entry->NameWidget, SInlineEditableTextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.IsSelected(this, &SRigVMAssetViewRow::IsSelectedExclusively)
					.IsReadOnly_Lambda([this]()
					{
						if (Cast<IAnimNextRigVMParameterInterface>(Entry->WeakEntry.Get()) 
							|| Cast<IAnimNextRigVMGraphInterface>(Entry->WeakEntry.Get()))
						{
							return false;
						}
						return true;
					})
					.OnTextCommitted_Lambda([this](const FText& InNewText, ETextCommit::Type InCommitType)
					{
						if(InCommitType == ETextCommit::OnEnter)
						{
							if(UAnimNextRigVMAssetEntry* AssetEntry = Cast<UAnimNextRigVMAssetEntry>(Entry->WeakEntry.Get()))
							{
								FScopedTransaction Transaction(LOCTEXT("SetParameterName", "Set Entry name"));
								AssetEntry->SetEntryName(*InNewText.ToString());
							}
						}
					})
					.OnVerifyTextChanged_Lambda([this](const FText& InNewText, FText& OutErrorText)
					{
						const FString NewString = InNewText.ToString();
						if (!FUtils::IsValidEntryNameString(NewString, OutErrorText))
						{
							return false;
						}

						FName Name(*NewString);
						if (TSharedPtr<SRigVMAssetView> View = WeakView.Pin())
						{
							if(UAnimNextRigVMAssetEntry* ExistingEntry = View->EditorData->FindEntry(Name))
							{
								OutErrorText = LOCTEXT("Error_NameExists", "This name already exists in this asset");
								return false;
							}
						}
						return true;
					})
					.Text_Lambda([this]()
					{
						if(UAnimNextRigVMAssetEntry* AssetEntry = Cast<UAnimNextRigVMAssetEntry>(Entry->WeakEntry.Get()))
						{
							if(IAnimNextRigVMParameterInterface* ParameterInterface = Cast<IAnimNextRigVMParameterInterface>(AssetEntry))
							{
								return UncookedOnly::FUtils::GetParameterDisplayNameText(AssetEntry->GetEntryName());
							}
							else
							{
								return FText::FromName(AssetEntry->GetEntryName());
							}
						}
						return FText::GetEmpty();
					})
					.ToolTipText_Lambda([this]()
					{
						if(UAnimNextRigVMAssetEntry* AssetEntry = Entry->WeakEntry.Get())
						{
							return AssetEntry->GetDisplayNameTooltip();
						}
						return FText::GetEmpty();
					})
				];
		}
		else if(InColumnName == Column_Value)
		{
			TSharedRef< SWidget > ColumnWidget = SNullWidget::NullWidget;

			if (TSharedPtr<SRigVMAssetView> View = WeakView.Pin())
			{
				if(UAnimNextRigVMAssetEntry* AssetEntry = Cast<UAnimNextRigVMAssetEntry>(Entry->WeakEntry.Get()))
				{
					if (const IAnimNextRigVMParameterInterface* ParameterInterface = Cast<IAnimNextRigVMParameterInterface>(AssetEntry))
					{
						FInstancedPropertyBag& PropertyBag = ParameterInterface->GetPropertyBag();
						const FName ParameterName = AssetEntry->GetEntryName();

						if (PropertyBag.FindPropertyDescByName(ParameterName))
						{
							FSinglePropertyParams SinglePropertyArgs;
							SinglePropertyArgs.NamePlacement = EPropertyNamePlacement::Hidden;
							SinglePropertyArgs.NotifyHook = this;

							FPropertyEditorModule& PropertyEditorModule = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
							const TSharedPtr<ISinglePropertyView> SingleStructPropertyView = PropertyEditorModule.CreateSingleProperty(MakeShared<FInstancePropertyBagStructureDataProvider>(PropertyBag), ParameterName, SinglePropertyArgs);
							if (SingleStructPropertyView.IsValid())
							{
								ColumnWidget = SingleStructPropertyView.ToSharedRef();
							}
						}
					}
				}
			}

			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Center)
				.AutoWidth()
				[
					ColumnWidget
				];
		}

		return SNullWidget::NullWidget;
	}

	TWeakPtr<SRigVMAssetView> WeakView;
	TSharedPtr<FRigVMAssetViewEntry> Entry;
};

TSharedRef<ITableRow> SRigVMAssetView::HandleGenerateRow(TSharedRef<FRigVMAssetViewEntry> InEntry, const TSharedRef<STableViewBase>& InOwnerTable)
{
	if (InEntry->CategoryTypeName != NAME_None)
	{
		TSharedPtr< STableRow< TSharedPtr<FRigVMAssetViewEntry> > > TableRow;
		TableRow = SNew(SCategoryHeaderTableRow< TSharedPtr<FRigVMAssetViewEntry> >, InOwnerTable);

		TSharedPtr<SHorizontalBox> RowContainer;
		TableRow->SetRowContent
		(
			SAssignNew(RowContainer, SHorizontalBox)
		);

		const FMargin RowPadding = FMargin(0, 2);
		TSharedPtr<SHorizontalBox> RowContent = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
				.Text(InEntry->CategoryText)
			];

		RowContainer->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Right)
			[
				SNew(SExpanderArrow, TableRow)
					.BaseIndentLevel(1)
			];

		RowContainer->AddSlot()
			.FillWidth(1.0)
			.Padding(RowPadding)
			[
				RowContent.ToSharedRef()
			];

		if(FCategoryWidgetFactoryFunction* FactoryFunction = CategoryFactories.Find(InEntry->CategoryTypeName))
		{
			RowContainer->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Right)
			[
				(*FactoryFunction)(EditorData)
			];
		}

		return TableRow.ToSharedRef();
	}
	else
	{
		return SNew(SRigVMAssetViewRow, InOwnerTable, SharedThis(this), InEntry);
	}
}

void SRigVMAssetView::HandleGetChildren(TSharedRef<FRigVMAssetViewEntry> InEntry, TArray<TSharedRef<FRigVMAssetViewEntry>>& OutChildren)
{
	if (InEntry->CategoryTypeName != NAME_None)
	{
		OutChildren = InEntry->Children;
	}
}

void SRigVMAssetView::HandleSelectionChanged(TSharedPtr<FRigVMAssetViewEntry> InEntry, ESelectInfo::Type InSelectionType)
{
	if(OnSelectionChangedDelegate.IsBound())
	{
		TArray<UObject*> SelectedItems;
		SelectedItems.Reserve(EntriesList->GetNumItemsSelected());
		for(const TSharedRef<FRigVMAssetViewEntry>& SelectedItem : EntriesList->GetSelectedItems())
		{
			if(UAnimNextRigVMAssetEntry* Entry = SelectedItem->WeakEntry.Get())
			{
				SelectedItems.Add(Entry);
			}
		}
		OnSelectionChangedDelegate.Execute(SelectedItems);
	}
}

TSharedRef<FRigVMAssetViewEntry> SRigVMAssetView::GetCategoryEntry(FName InCategoryName)
{
	for (TSharedRef<FRigVMAssetViewEntry>& Category : Categories)
	{
		if (Category->CategoryTypeName == InCategoryName)
		{
			return Category;
		}
	}

	checkNoEntry();	// If we hit here, an entry's class has not been set up with a category

	return TSharedRef<FRigVMAssetViewEntry>();
}

}

#undef LOCTEXT_NAMESPACE