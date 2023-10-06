// Copyright Epic Games, Inc. All Rights Reserved.

#include "SParameterBlockView.h"

#include "Param/AnimNextParameterBlock.h"
#include "Param/AnimNextParameter.h"
#include "Param/AnimNextParameterBlock_EditorData.h"
#include "Param/AnimNextParameterBlockBinding.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Param/AnimNextParameterBlockEntry.h"
#include "Param/AnimNextParameterLibrary.h"
#include "DetailLayoutBuilder.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "EditorUtils.h"
#include "UncookedOnlyUtils.h"
#include "ParameterBlockViewMenuContext.h"
#include "PropertyBagDetails.h"
#include "SAddParametersDialog.h"
#include "SLinkParametersDialog.h"
#include "SourceControlOperations.h"
#include "SPinTypeSelector.h"
#include "SSimpleButton.h"
#include "Framework/Commands/GenericCommands.h"
#include "Param/IAnimNextParameterBlockReferenceInterface.h"
#include "RevisionControlStyle/RevisionControlStyle.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Param/AnimNextParameterBlockBindingReference.h"
#include "ToolMenus.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "AnimNextParameterBlockView"

namespace UE::AnimNext::Editor
{

namespace ParameterBlockView
{

static FName ContextMenuName(TEXT("AnimNext.ParameterBlockView.ContextMenu"));
static FName Column_RevisionControl(TEXT("RevisionControl"));
static FName Column_ModifiedStatus(TEXT("ModifiedStatus"));
static FName Column_Name(TEXT("Name"));
static FName Column_Type(TEXT("Type"));
static FName Column_Value(TEXT("Value"));

}

// An entry displayed in the parameters view
struct FParameterBlockViewEntry
{
	FParameterBlockViewEntry() = default;

	FParameterBlockViewEntry(UAnimNextParameterBlockEntry* InEntry)
		: WeakEntry(InEntry)
	{}

	bool PassesFilter(const FString& InFilterText) const
	{
		if(UAnimNextParameterBlockEntry* Entry = WeakEntry.Get())
		{
			return Entry->GetDisplayName().ToString().Contains(InFilterText);
		}

		return false;
	}

	// Ptr to the underlying entry
	TWeakObjectPtr<UAnimNextParameterBlockEntry> WeakEntry;

	// Widget used to rename items
	TWeakPtr<SInlineEditableTextBlock> NameWidget;

	// Flag to indicate a rename was requested
	bool bRenameWhenScrolledIntoView = false;
};

void SParameterBlockView::Construct(const FArguments& InArgs, UAnimNextParameterBlock_EditorData* InEditorData)
{
	using namespace ParameterBlockView;
	
	check(InEditorData);

	EditorData = InEditorData;

	// Cache asset data for block for comparisons/filtering
	BlockAssetData = FAssetData(UncookedOnly::FUtils::GetBlock(EditorData));

	OnSelectionChangedDelegate = InArgs._OnSelectionChanged;
	OnOpenGraphDelegate = InArgs._OnOpenGraph;

	EditorData->ModifiedDelegate.AddSP(this, &SParameterBlockView::HandleBlockModified);

	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f)
			[
				SNew(SSimpleButton)
				.Text(LOCTEXT("AddNewParameterButton", "New"))
				.Icon(FAppStyle::Get().GetBrush("Icons.Plus"))
				.OnClicked_Lambda([this]()
				{
					TSharedRef<SAddParametersDialog> AddParametersDialog = SNew(SAddParametersDialog);
					TArray<FParameterToAdd> ParametersToAdd;
					if(AddParametersDialog->ShowModal(ParametersToAdd))
					{
						FScopedTransaction Transaction(FText::FormatOrdered(LOCTEXT("AddParameters", "Add {0}|plural(one=parameter,other=parameters)"), ParametersToAdd.Num()));

						PendingSelection.Empty();

						for(const FParameterToAdd& ParameterToAdd : ParametersToAdd)
						{
							// Create a new parameter in the supplied library
							UAnimNextParameterLibrary* Library = Cast<UAnimNextParameterLibrary>(ParameterToAdd.Library.GetAsset());
							UAnimNextParameter* NewParameter = Library->AddParameter(ParameterToAdd.Name, ParameterToAdd.Type);

							// Create a new entry for the parameter
							UAnimNextParameterBlockBinding* Binding = EditorData->AddBinding(NewParameter->GetFName(), Library);
							PendingSelection.Add(Binding);
						}
					}

					return FReply::Handled();
				})
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f)
			[
				SNew(SSimpleButton)
				.Text(LOCTEXT("LinkParameterButton", "Link"))
				.Icon(FAppStyle::Get().GetBrush("Icons.Link"))
				.OnClicked_Lambda([this]()
				{
					TSharedRef<SLinkParametersDialog> AddParametersDialog = SNew(SLinkParametersDialog)
						.OnFilterParameter(this, &SParameterBlockView::HandleFilterLinkedParameter);
					TArray<FParameterBindingReference> ParametersToLink;
					if(AddParametersDialog->ShowModal(ParametersToLink))
					{
						FScopedTransaction Transaction(FText::FormatOrdered(LOCTEXT("LinkParameters", "Link {0}|plural(one=parameter,other=parameters)"), ParametersToLink.Num()));

						PendingSelection.Empty();

						for(const FParameterBindingReference& ParameterToLink : ParametersToLink)
						{
							check(ParameterToLink.Block.IsValid());
							check(ParameterToLink.Parameter != NAME_None);

							// Load our assets
							UAnimNextParameterBlock* ExistingBlock = CastChecked<UAnimNextParameterBlock>(ParameterToLink.Block.GetAsset());
							UAnimNextParameterLibrary* ExistingLibrary = CastChecked<UAnimNextParameterLibrary>(ParameterToLink.Library.GetAsset());

							// Create a new entry for the supplied parameter
							UAnimNextParameterBlockBindingReference* BindingReference = EditorData->AddBindingReference(ParameterToLink.Parameter, ExistingLibrary, ExistingBlock);
							PendingSelection.Add(BindingReference);
						}
					}

					return FReply::Handled();
				})
			]
			+SHorizontalBox::Slot()
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
			SAssignNew(EntriesList, SListView<TSharedRef<FParameterBlockViewEntry>>)
			.ListItemsSource(&FilteredEntries)
			.OnGenerateRow(this, &SParameterBlockView::HandleGenerateRow)
			.OnItemScrolledIntoView(this, &SParameterBlockView::HandleItemScrolledIntoView)
			.OnSelectionChanged(this, &SParameterBlockView::HandleSelectionChanged)
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

	RequestRefresh();
}

FReply SParameterBlockView::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (UICommandList.IsValid() && UICommandList->ProcessCommandBindings(InKeyEvent))
	{
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void SParameterBlockView::RequestRefresh()
{
	bRefreshRequested = true;
}

void SParameterBlockView::RefreshEntries()
{
	Entries.Empty();

	TArray<TSharedRef<FParameterBlockViewEntry>> EntriesToSelect;

	for(UAnimNextParameterBlockEntry* Entry : EditorData->Entries)
	{
		TSharedRef<FParameterBlockViewEntry> NewEntry = MakeShared<FParameterBlockViewEntry>(Entry);
		Entries.Add(NewEntry);

		if(PendingSelection.Contains(Entry))
		{
			EntriesToSelect.Add(NewEntry);
		}
	}

	PendingSelection.Empty();

	RefreshFilter();

	if(EntriesToSelect.Num() > 0)
	{
		EntriesList->SetItemSelection(EntriesToSelect, true);
	}
}

void SParameterBlockView::RefreshFilter()
{
	FilteredEntries.Empty();

	const FString FilterTextAsString = FilterText.ToString();
	for(const TSharedRef<FParameterBlockViewEntry>& Entry : Entries)
	{
		if(Entry->PassesFilter(FilterTextAsString))
		{
			FilteredEntries.Add(Entry);
		}
	}

	EntriesList->RequestListRefresh();
}

void SParameterBlockView::BindCommands()
{
	UICommandList = MakeShared<FUICommandList>();

	UICommandList->MapAction(FGenericCommands::Get().Delete,
		FExecuteAction::CreateSP(this, &SParameterBlockView::HandleDelete),
		FCanExecuteAction::CreateSP(this, &SParameterBlockView::HasValidSelection));

	UICommandList->MapAction(FGenericCommands::Get().Rename,
		FExecuteAction::CreateSP(this, &SParameterBlockView::HandleRename),
		FCanExecuteAction::CreateSP(this, &SParameterBlockView::HasValidSingleSelection));
}

void SParameterBlockView::HandleBlockModified(UAnimNextParameterBlock_EditorData* InEditorData)
{
	check(InEditorData == EditorData);

	RequestRefresh();
}

TSharedRef<SWidget> SParameterBlockView::HandleGetContextContent()
{
	using namespace ParameterBlockView;
	
	UToolMenus* ToolMenus = UToolMenus::Get();

	if(!ToolMenus->IsMenuRegistered(ContextMenuName))
	{
		UToolMenu* Menu = ToolMenus->RegisterMenu(ContextMenuName);

		FToolMenuSection& Section = Menu->AddSection("EntryOperations", LOCTEXT("EntryOperationsMenuSection", "Block Entry"));
		Section.AddMenuEntry(FGenericCommands::Get().Delete);
		Section.AddMenuEntry(FGenericCommands::Get().Rename);
	}

	UParameterBlockViewMenuContext* MenuContext = NewObject<UParameterBlockViewMenuContext>();
	MenuContext->ParameterBlockView = SharedThis(this);
	return ToolMenus->GenerateWidget(ContextMenuName, FToolMenuContext(MenuContext));
}

void SParameterBlockView::HandleDelete()
{
	if(EntriesList->GetNumItemsSelected() > 0)
	{
		TArray<TSharedRef<FParameterBlockViewEntry>> SelectedItems = EntriesList->GetSelectedItems();

		{
			FScopedTransaction Transaction(FText::FormatOrdered(LOCTEXT("DeleteParameterBlockEntry", "Delete parameter block {0}|plural(one=entry,other=entries)"), EntriesList->GetNumItemsSelected()));

			TArray<UAnimNextParameterBlockEntry*> EntriesToRemove;
			Algo::Transform(SelectedItems, EntriesToRemove, [](const TSharedRef<FParameterBlockViewEntry>& InEntry){ return InEntry->WeakEntry.Get(); });
			EditorData->RemoveEntries(EntriesToRemove);
		}
	}
}

void SParameterBlockView::HandleRename()
{
	if(EntriesList->GetNumItemsSelected() == 1)
	{
		TArray<TSharedRef<FParameterBlockViewEntry>> SelectedItems = EntriesList->GetSelectedItems();
		SelectedItems[0]->bRenameWhenScrolledIntoView = true;
		EntriesList->RequestScrollIntoView(SelectedItems[0]);
	}
}

bool SParameterBlockView::HasValidSelection() const
{
	return EntriesList->GetNumItemsSelected() > 0;
}

bool SParameterBlockView::HasValidSingleSelection() const
{
	return EntriesList->GetNumItemsSelected() == 1;
}

void SParameterBlockView::HandleItemScrolledIntoView(TSharedRef<FParameterBlockViewEntry> InEntry, const TSharedPtr<ITableRow>& InWidget)
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

class SParameterBlockViewRow : public SMultiColumnTableRow<TSharedRef<FParameterBlockViewEntry>>
{
	SLATE_BEGIN_ARGS(SParameterBlockViewRow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, TSharedRef<SParameterBlockView> InView, TSharedRef<FParameterBlockViewEntry> InEntry)
	{
		WeakView = InView;
		Entry = InEntry;

		SMultiColumnTableRow<TSharedRef<FParameterBlockViewEntry>>::Construct( SMultiColumnTableRow<TSharedRef<FParameterBlockViewEntry>>::FArguments(), InOwnerTableView);
	}

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
						if(UAnimNextParameterBlockEntry* BlockEntry = Entry->WeakEntry.Get())
						{
							ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
							UPackage* Package = BlockEntry->GetExternalPackage();
							if(FSourceControlStatePtr State = SourceControlProvider.GetState(Package, EStateCacheUsage::Use))
							{
								return FText::Format(LOCTEXT("RevisionControlStatusFormat", "File: {0}\nStatus: {1}"), FText::FromName(Package->GetFName()),  State->GetDisplayTooltip());
							}
						}

						return LOCTEXT("RevisionControlStatus", "Revision control status of this parameter");
					})
					.Image_Lambda([this]() -> const FSlateBrush*
					{
						if(UAnimNextParameterBlockEntry* BlockEntry = Entry->WeakEntry.Get())
						{
							ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
							if(FSourceControlStatePtr State = SourceControlProvider.GetState(BlockEntry->GetExternalPackage(), EStateCacheUsage::Use))
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

						if(UAnimNextParameterBlockEntry* BlockEntry = Entry->WeakEntry.Get())
						{
							const UPackage* ExternalPackage = BlockEntry->GetExternalPackage();
							check(ExternalPackage);
							if(ExternalPackage->IsDirty())
							{
								TextBuilder.AppendLine(FText::FromName(ExternalPackage->GetFName()));
							}

							if(const IAnimNextParameterBlockBindingInterface* Binding = Cast<IAnimNextParameterBlockBindingInterface>(BlockEntry))
							{
								if(const UAnimNextParameter* Parameter = Binding->GetParameter())
								{
									const UPackage* ParameterPackage = Parameter->GetPackage();
									check(ParameterPackage);
									if(ParameterPackage->IsDirty())
									{
										TextBuilder.AppendLine(FText::FromName(ParameterPackage->GetFName()));
									}
								}
							}
						}

						return TextBuilder.ToText();
					})
					.Image_Lambda([this]() -> const FSlateBrush*
					{
						if(UAnimNextParameterBlockEntry* BlockEntry = Entry->WeakEntry.Get())
						{
							bool bIsDirty = false;
							const UPackage* ExternalPackage = BlockEntry->GetExternalPackage();
							check(ExternalPackage);
							if(ExternalPackage->IsDirty())
							{
								bIsDirty = true;
							}

							if(const IAnimNextParameterBlockBindingInterface* Binding = Cast<IAnimNextParameterBlockBindingInterface>(BlockEntry))
							{
								if(const UAnimNextParameter* Parameter = Binding->GetParameter())
								{
									const UPackage* ParameterPackage = Parameter->GetPackage();
									check(ParameterPackage);
									if(ParameterPackage->IsDirty())
									{
										bIsDirty = true;
									}
								}
							}

							return bIsDirty ? FAppStyle::GetBrush("ContentBrowser.ContentDirty") : nullptr;
						}
						return nullptr;
					})
				];
		}
		else if(InColumnName == Column_Type)
		{
			if(Entry->WeakEntry.Get()->Implements<UAnimNextParameterBlockBindingInterface>())
			{
				return
					SNew(SBox)
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					.IsEnabled(false)
					[
						SNew(SPinTypeSelector, FGetPinTypeTree::CreateStatic(&Editor::FUtils::GetFilteredVariableTypeTree))
							.TargetPinType_Lambda([this]()
							{
								if(IAnimNextParameterBlockBindingInterface* Binding = Cast<IAnimNextParameterBlockBindingInterface>(Entry->WeakEntry.Get()))
								{
									return UncookedOnly::FUtils::GetPinTypeFromParamType(Binding->GetParamType());
								}

								return FEdGraphPinType();
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
				SNew(SBox)
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				[
					// TODO: make this into a picker appropriately
					SAssignNew(Entry->NameWidget, SInlineEditableTextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.IsSelected(this, &SParameterBlockViewRow::IsSelectedExclusively)
					.IsReadOnly_Lambda([this]()
					{
						return Cast<UAnimNextParameterBlockBinding>(Entry->WeakEntry.Get()) == nullptr;
					})
					.OnTextCommitted_Lambda([this](const FText& InNewText, ETextCommit::Type InCommitType)
					{
						if(InCommitType == ETextCommit::OnEnter)
						{
							if(IAnimNextParameterBlockBindingInterface* Binding = Cast<IAnimNextParameterBlockBindingInterface>(Entry->WeakEntry.Get()))
							{
								FScopedTransaction Transaction(LOCTEXT("SetParameteName", "Set parameter name"));

								Binding->SetParameterName(*InNewText.ToString());
							}
						}
					})
					.OnVerifyTextChanged_Lambda([this](const FText& InNewText, FText& OutErrorText)
					{
						const FString NewString = InNewText.ToString();

						if(IAnimNextParameterBlockBindingInterface* Binding = Cast<IAnimNextParameterBlockBindingInterface>(Entry->WeakEntry.Get()))
						{
							// Make sure the new name only contains valid characters
							if (!FName::IsValidXName(NewString, INVALID_OBJECTNAME_CHARACTERS INVALID_LONGPACKAGE_CHARACTERS, &OutErrorText))
							{
								return false;
							}

							const FName Name(*NewString);
							if(!FUtils::DoesParameterExistInLibrary(Binding->GetLibrary(), Name))
							{
								OutErrorText = LOCTEXT("Error_NameDoesNotExistInLibrary", "This name does not exist in the specified library");
								return false;
							}
						}

						if(IAnimNextParameterBlockReferenceInterface* Reference = Cast<IAnimNextParameterBlockReferenceInterface>(Entry->WeakEntry.Get()))
						{
							const FName Name(*NewString);
							if(!FUtils::DoesParameterExistInLibrary(Reference->GetBlock(), Name))
							{
								OutErrorText = LOCTEXT("Error_NameDoesNotExist", "This name does not exist in the specified block");
								return false;
							}
						}

						return true;
					})
					.Text_Lambda([this]()
					{
						if(UAnimNextParameterBlockEntry* BlockEntry = Entry->WeakEntry.Get())
						{
							return BlockEntry->GetDisplayName();
						}
						return FText::GetEmpty();
					})
					.ToolTipText_Lambda([this]()
					{
						if(UAnimNextParameterBlockEntry* BlockEntry = Entry->WeakEntry.Get())
						{
							return BlockEntry->GetDisplayNameTooltip();
						}
						return FText::GetEmpty();
					})
				];
		}
		else if(InColumnName == Column_Value)
		{
			TSharedPtr<SHorizontalBox> HorizontalBox;

			TSharedRef<SWidget> Widget =
				SNew(SBox)
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				[
					SAssignNew(HorizontalBox, SHorizontalBox)
				];

			if(UAnimNextParameterBlockEntry* BlockEntry = Entry->WeakEntry.Get())
			{
				if(IAnimNextParameterBlockReferenceInterface* ReferenceInterface = Cast<IAnimNextParameterBlockReferenceInterface>(BlockEntry))
				{
					const UAnimNextParameterBlock* ReferencedBlock = ReferenceInterface->GetBlock();
					HorizontalBox->AddSlot()
					[
						SNew(STextBlock)
						.Font(IDetailLayoutBuilder::GetDetailFont())
						.IsEnabled(false)
						.Text_Lambda([WeakBlock = TWeakObjectPtr<const UAnimNextParameterBlock>(ReferencedBlock)]()
						{
							if(const UAnimNextParameterBlock* Block = WeakBlock.Get())
							{
								return FText::FromString(Block->GetName());
							}

							return LOCTEXT("MissingParameterBlock", "Missing Parameter Block");
						})
						.ToolTipText_Lambda([WeakBlock = TWeakObjectPtr<const UAnimNextParameterBlock>(ReferencedBlock)]()
						{
							if(const UAnimNextParameterBlock* Block = WeakBlock.Get())
							{
								return FText::FromString(Block->GetPathName());
							}

							return LOCTEXT("MissingParameterBlock", "Missing Parameter Block");
						})
					];
				}
				
				if(IAnimNextParameterBlockGraphInterface* GraphInterface = Cast<IAnimNextParameterBlockGraphInterface>(BlockEntry))
				{
					URigVMGraph* ReferencedGraph = GraphInterface->GetGraph();
					HorizontalBox->AddSlot()
					[
						SNew(SSimpleButton)
						.Icon(FAppStyle::GetBrush("Icons.Blueprints"))
						.OnClicked_Lambda([this, WeakGraph = TWeakObjectPtr<URigVMGraph>(ReferencedGraph)]()
						{
							if(URigVMGraph* Graph = WeakGraph.Get())
							{
								if(TSharedPtr<SParameterBlockView> View = WeakView.Pin())
								{
									View->OnOpenGraphDelegate.ExecuteIfBound(Graph);
								}
							}
							return FReply::Handled();
						})
					];
				}
			}
			
			return Widget;
		}

		return SNullWidget::NullWidget;
	}

	TWeakPtr<SParameterBlockView> WeakView;
	TSharedPtr<FParameterBlockViewEntry> Entry;
};

TSharedRef<ITableRow> SParameterBlockView::HandleGenerateRow(TSharedRef<FParameterBlockViewEntry> InEntry, const TSharedRef<STableViewBase>& InOwnerTable)
{
	return SNew(SParameterBlockViewRow, InOwnerTable, SharedThis(this), InEntry);
}

void SParameterBlockView::HandleSelectionChanged(TSharedPtr<FParameterBlockViewEntry> InEntry, ESelectInfo::Type InSelectionType)
{
	if(OnSelectionChangedDelegate.IsBound())
	{
		TArray<UObject*> SelectedItems;
		SelectedItems.Reserve(EntriesList->GetNumItemsSelected());
		for(const TSharedRef<FParameterBlockViewEntry>& SelectedItem : EntriesList->GetSelectedItems())
		{
			if(UAnimNextParameterBlockEntry* Entry = SelectedItem->WeakEntry.Get())
			{
				SelectedItems.Add(Entry);
			}
		}

		OnSelectionChangedDelegate.Execute(SelectedItems);
	}
}

EFilterParameterResult SParameterBlockView::HandleFilterLinkedParameter(const FParameterBindingReference& InParameterBinding)
{
	// Don't display parameters that are already bound in this block
	if(InParameterBinding.Block.IsValid())
	{
		if(BlockAssetData == InParameterBinding.Block)
		{
			return EFilterParameterResult::Exclude;
		}
	}

	return EFilterParameterResult::Include;
}

}

#undef LOCTEXT_NAMESPACE