// Copyright Epic Games, Inc. All Rights Reserved.

#include "SParameterLibraryView.h"

#include "Param/AnimNextParameterLibrary.h"
#include "Param/AnimNextParameter.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "DetailLayoutBuilder.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "EditorUtils.h"
#include "UncookedOnlyUtils.h"
#include "ParameterLibraryViewMenuContext.h"
#include "PropertyBagDetails.h"
#include "SAddParametersDialog.h"
#include "SLinkParametersDialog.h"
#include "SourceControlOperations.h"
#include "SPinTypeSelector.h"
#include "SSimpleButton.h"
#include "Framework/Commands/GenericCommands.h"
#include "RevisionControlStyle/RevisionControlStyle.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "ScopedTransaction.h"
#include "Framework/Commands/UICommandList.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "AnimNextParameterLibraryView"

namespace UE::AnimNext::Editor
{

namespace ParameterLibraryView
{

static FName ContextMenuName(TEXT("AnimNext.ParameterLibraryView.ContextMenu"));
static FName Column_RevisionControl(TEXT("RevisionControl"));
static FName Column_ModifiedStatus(TEXT("ModifiedStatus"));
static FName Column_Name(TEXT("Name"));
static FName Column_Type(TEXT("Type"));

}

// An entry displayed in the parameters view
struct FParameterLibraryViewEntry
{
	FParameterLibraryViewEntry() = default;

	explicit FParameterLibraryViewEntry(UAnimNextParameter* InEntry)
		: WeakEntry(InEntry)
	{}

	bool PassesFilter(const FString& InFilterText) const
	{
		if(UAnimNextParameter* Entry = WeakEntry.Get())
		{
			return Entry->GetName().Contains(InFilterText);
		}

		return false;
	}

	// Ptr to the underlying entry
	TWeakObjectPtr<UAnimNextParameter> WeakEntry;

	// Widget used to rename items
	TWeakPtr<SInlineEditableTextBlock> NameWidget;

	// Flag to indicate a rename was requested
	bool bRenameWhenScrolledIntoView = false;
};

void SParameterLibraryView::Construct(const FArguments& InArgs, UAnimNextParameterLibrary* InLibrary)
{
	using namespace ParameterLibraryView;
	
	check(InLibrary);

	Library = InLibrary;

	OnSelectionChangedDelegate = InArgs._OnSelectionChanged;

	Library->ModifiedDelegate.AddSP(this, &SParameterLibraryView::HandleLibraryModified);

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
					TSharedRef<SAddParametersDialog> AddParametersDialog =
						SNew(SAddParametersDialog)
						.Library(Library);
					TArray<FParameterToAdd> ParametersToAdd;
					if(AddParametersDialog->ShowModal(ParametersToAdd))
					{
						FScopedTransaction Transaction(FText::FormatOrdered(LOCTEXT("AddParameters", "Add {0}|plural(one=parameter,other=parameters)"), ParametersToAdd.Num()));

						PendingSelection.Empty();

						for(const FParameterToAdd& ParameterToAdd : ParametersToAdd)
						{
							// Create a new parameter in the library
							Library->AddParameter(ParameterToAdd.Name, ParameterToAdd.Type);
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
			SAssignNew(EntriesList, SListView<TSharedRef<FParameterLibraryViewEntry>>)
			.ListItemsSource(&FilteredEntries)
			.OnGenerateRow(this, &SParameterLibraryView::HandleGenerateRow)
			.OnItemScrolledIntoView(this, &SParameterLibraryView::HandleItemScrolledIntoView)
			.OnSelectionChanged(this, &SParameterLibraryView::HandleSelectionChanged)
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
			)
		]
	];

	// Update revision control status of all our files
	if (ISourceControlModule::Get().IsEnabled())
	{
		TArray<UPackage*> Packages = Library->GetPackage()->GetExternalPackages();

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

FReply SParameterLibraryView::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (UICommandList.IsValid() && UICommandList->ProcessCommandBindings(InKeyEvent))
	{
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void SParameterLibraryView::RequestRefresh()
{
	bRefreshRequested = true;
}

void SParameterLibraryView::RefreshEntries()
{
	Entries.Empty();

	TArray<TSharedRef<FParameterLibraryViewEntry>> EntriesToSelect;

	for(UAnimNextParameter* Entry : Library->Parameters)
	{
		TSharedRef<FParameterLibraryViewEntry> NewEntry = MakeShared<FParameterLibraryViewEntry>(Entry);
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

void SParameterLibraryView::RefreshFilter()
{
	FilteredEntries.Empty();

	const FString FilterTextAsString = FilterText.ToString();
	for(const TSharedRef<FParameterLibraryViewEntry>& Entry : Entries)
	{
		if(Entry->PassesFilter(FilterTextAsString))
		{
			FilteredEntries.Add(Entry);
		}
	}

	EntriesList->RequestListRefresh();
}

void SParameterLibraryView::BindCommands()
{
	UICommandList = MakeShared<FUICommandList>();

	UICommandList->MapAction(FGenericCommands::Get().Delete,
		FExecuteAction::CreateSP(this, &SParameterLibraryView::HandleDelete),
		FCanExecuteAction::CreateSP(this, &SParameterLibraryView::HasValidSelection));

	UICommandList->MapAction(FGenericCommands::Get().Rename,
		FExecuteAction::CreateSP(this, &SParameterLibraryView::HandleRename),
		FCanExecuteAction::CreateSP(this, &SParameterLibraryView::HasValidSingleSelection));
}

void SParameterLibraryView::HandleLibraryModified(UAnimNextParameterLibrary* InLibrary)
{
	check(InLibrary == Library);

	RequestRefresh();
}

TSharedRef<SWidget> SParameterLibraryView::HandleGetContextContent()
{
	using namespace ParameterLibraryView;
	
	UToolMenus* ToolMenus = UToolMenus::Get();

	if(!ToolMenus->IsMenuRegistered(ContextMenuName))
	{
		UToolMenu* Menu = ToolMenus->RegisterMenu(ContextMenuName);

		FToolMenuSection& Section = Menu->AddSection("EntryOperations", LOCTEXT("EntryOperationsMenuSection", "Parameter"));
		Section.AddMenuEntry(FGenericCommands::Get().Delete);
		Section.AddMenuEntry(FGenericCommands::Get().Rename);
	}

	UParameterLibraryViewMenuContext* MenuContext = NewObject<UParameterLibraryViewMenuContext>();
	MenuContext->ParameterLibraryView = SharedThis(this);
	return ToolMenus->GenerateWidget(ContextMenuName, FToolMenuContext(MenuContext));
}

void SParameterLibraryView::HandleDelete()
{
	if(EntriesList->GetNumItemsSelected() > 0)
	{
		TArray<TSharedRef<FParameterLibraryViewEntry>> SelectedItems = EntriesList->GetSelectedItems();

		{
			FScopedTransaction Transaction(FText::FormatOrdered(LOCTEXT("DeleteParameter", "Delete {0}|plural(one=parameter,other=parameters)"), EntriesList->GetNumItemsSelected()));

			TArray<FName> EntriesToRemove;
			Algo::Transform(SelectedItems, EntriesToRemove, [](const TSharedRef<FParameterLibraryViewEntry>& InEntry){ return InEntry->WeakEntry.Get()->GetFName(); });
			Library->RemoveParameters(EntriesToRemove);
		}
	}
}

void SParameterLibraryView::HandleRename()
{
	if(EntriesList->GetNumItemsSelected() == 1)
	{
		TArray<TSharedRef<FParameterLibraryViewEntry>> SelectedItems = EntriesList->GetSelectedItems();
		SelectedItems[0]->bRenameWhenScrolledIntoView = true;
		EntriesList->RequestScrollIntoView(SelectedItems[0]);
	}
}

bool SParameterLibraryView::HasValidSelection() const
{
	return EntriesList->GetNumItemsSelected() > 0;
}

bool SParameterLibraryView::HasValidSingleSelection() const
{
	return EntriesList->GetNumItemsSelected() == 1;
}

void SParameterLibraryView::HandleItemScrolledIntoView(TSharedRef<FParameterLibraryViewEntry> InEntry, const TSharedPtr<ITableRow>& InWidget)
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

class SParameterLibraryViewRow : public SMultiColumnTableRow<TSharedRef<FParameterLibraryViewEntry>>
{
	SLATE_BEGIN_ARGS(SParameterLibraryViewRow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, TSharedRef<SParameterLibraryView> InView, TSharedRef<FParameterLibraryViewEntry> InEntry)
	{
		WeakView = InView;
		Entry = InEntry;

		SMultiColumnTableRow<TSharedRef<FParameterLibraryViewEntry>>::Construct( SMultiColumnTableRow<TSharedRef<FParameterLibraryViewEntry>>::FArguments(), InOwnerTableView);
	}

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& InColumnName) override
	{
		using namespace ParameterLibraryView;
		
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
						if(UAnimNextParameter* Parameter = Entry->WeakEntry.Get())
						{
							ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
							UPackage* Package = Parameter->GetExternalPackage();
							if(FSourceControlStatePtr State = SourceControlProvider.GetState(Package, EStateCacheUsage::Use))
							{
								return FText::Format(LOCTEXT("RevisionControlStatusFormat", "File: {0}\nStatus: {1}"), FText::FromName(Package->GetFName()),  State->GetDisplayTooltip());
							}
						}

						return LOCTEXT("RevisionControlStatus", "Revision control status of this parameter");
					})
					.Image_Lambda([this]() -> const FSlateBrush*
					{
						if(UAnimNextParameter* Parameter = Entry->WeakEntry.Get())
						{
							ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
							if(FSourceControlStatePtr State = SourceControlProvider.GetState(Parameter->GetExternalPackage(), EStateCacheUsage::Use))
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

						if(UAnimNextParameter* Parameter = Entry->WeakEntry.Get())
						{
							const UPackage* ExternalPackage = Parameter->GetExternalPackage();
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
						if(UAnimNextParameter* Parameter = Entry->WeakEntry.Get())
						{
							bool bIsDirty = false;
							const UPackage* ExternalPackage = Parameter->GetExternalPackage();
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
			return
				SNew(SBox)
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				[
					SNew(SPinTypeSelector, FGetPinTypeTree::CreateStatic(&Editor::FUtils::GetFilteredVariableTypeTree))
						.TargetPinType_Lambda([this]()
						{
							if(UAnimNextParameter* Parameter = Entry->WeakEntry.Get())
							{
								return UncookedOnly::FUtils::GetPinTypeFromParamType(Parameter->GetType());
							}

							return FEdGraphPinType();
						})
						.OnPinTypeChanged_Lambda([this](const FEdGraphPinType& InPinType)
						{
							if(UAnimNextParameter* Parameter = Entry->WeakEntry.Get())
							{
								FScopedTransaction Transaction(LOCTEXT("SetParameterType", "Set parameter type"));

								Parameter->SetType(UncookedOnly::FUtils::GetParamTypeFromPinType(InPinType));
							}
						})
						.Schema(GetDefault<UPropertyBagSchema>())
						.bAllowArrays(true)
						.TypeTreeFilter(ETypeTreeFilter::None)
						.Font(IDetailLayoutBuilder::GetDetailFont())
				];
		}
		else if(InColumnName == Column_Name)
		{
			return
				SNew(SBox)
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				[
					SAssignNew(Entry->NameWidget, SInlineEditableTextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.IsSelected(this, &SParameterLibraryViewRow::IsSelectedExclusively)
					.OnTextCommitted_Lambda([this](const FText& InNewText, ETextCommit::Type InCommitType)
					{
						if(InCommitType == ETextCommit::OnEnter)
						{
							if(UAnimNextParameter* Parameter = Entry->WeakEntry.Get())
							{
								FScopedTransaction Transaction(LOCTEXT("SetParameterName", "Set parameter name"));

								Parameter->Rename(*InNewText.ToString());
							}
						}
					})
					.OnVerifyTextChanged_Lambda([this](const FText& InNewText, FText& OutErrorText)
					{
						const FString NewString = InNewText.ToString();

						if(UAnimNextParameter* Parameter = Entry->WeakEntry.Get())
						{
							// Make sure the new name only contains valid characters
							if (!FName::IsValidXName(NewString, INVALID_OBJECTNAME_CHARACTERS INVALID_LONGPACKAGE_CHARACTERS, &OutErrorText))
							{
								return false;
							}

							const FName Name(*NewString);
							if(FUtils::DoesParameterExistInLibrary(WeakView.Pin()->Library, Name))
							{
								OutErrorText = LOCTEXT("Error_NameExistsInLibrary", "This name already exists in this library");
								return false;
							}
						}

						return true;
					})
					.Text_Lambda([this]()
					{
						if(UAnimNextParameter* Parameter = Entry->WeakEntry.Get())
						{
							return FText::FromName(Parameter->GetFName());
						}
						return FText::GetEmpty();
					})
					.ToolTipText_Lambda([this]()
					{
						if(UAnimNextParameter* Parameter = Entry->WeakEntry.Get())
						{
							return FText::FromStringView(Parameter->GetComment());
						}
						return FText::GetEmpty();
					})
				];
		}

		return SNullWidget::NullWidget;
	}

	TWeakPtr<SParameterLibraryView> WeakView;
	TSharedPtr<FParameterLibraryViewEntry> Entry;
};

TSharedRef<ITableRow> SParameterLibraryView::HandleGenerateRow(TSharedRef<FParameterLibraryViewEntry> InEntry, const TSharedRef<STableViewBase>& InOwnerTable)
{
	return SNew(SParameterLibraryViewRow, InOwnerTable, SharedThis(this), InEntry);
}

void SParameterLibraryView::HandleSelectionChanged(TSharedPtr<FParameterLibraryViewEntry> InEntry, ESelectInfo::Type InSelectionType)
{
	if(OnSelectionChangedDelegate.IsBound())
	{
		TArray<UObject*> SelectedItems;
		SelectedItems.Reserve(EntriesList->GetNumItemsSelected());
		for(const TSharedRef<FParameterLibraryViewEntry>& SelectedItem : EntriesList->GetSelectedItems())
		{
			if(UAnimNextParameter* Parameter = SelectedItem->WeakEntry.Get())
			{
				SelectedItems.Add(Parameter);
			}
		}

		OnSelectionChangedDelegate.Execute(SelectedItems);
	}
}

}

#undef LOCTEXT_NAMESPACE