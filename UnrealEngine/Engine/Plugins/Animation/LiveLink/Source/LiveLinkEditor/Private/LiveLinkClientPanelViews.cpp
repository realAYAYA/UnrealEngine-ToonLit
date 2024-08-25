// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkClientPanelViews.h"

#include "EditorFontGlyphs.h"
#include "Features/IModularFeatures.h"
#include "Framework/Commands/UICommandList.h"
#include "IDetailsView.h"
#include "ILiveLinkClient.h"
#include "Internationalization/Text.h"
#include "LiveLinkClient.h"
#include "LiveLinkClientCommands.h"
#include "LiveLinkSettings.h"
#include "LiveLinkTypes.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "SLiveLinkDataView.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"


#define LOCTEXT_NAMESPACE "LiveLinkClientPanel.PanelViews"

// Static Source UI FNames
namespace SourceListUI
{
	static const FName TypeColumnName(TEXT("Type"));
	static const FName MachineColumnName(TEXT("Machine"));
	static const FName StatusColumnName(TEXT("Status"));
	static const FName ActionsColumnName(TEXT("Action"));
};

// Static Subject UI FNames
namespace SubjectTreeUI
{
	static const FName EnabledColumnName(TEXT("Enabled"));
	static const FName NameColumnName(TEXT("Name"));
	static const FName RoleColumnName(TEXT("Role"));
	static const FName ActionsColumnName(TEXT("Action"));
};

namespace UE::LiveLink
{
TSharedPtr<IDetailsView> CreateSourcesDetailsView(const TSharedPtr<FLiveLinkSourcesView>& InSourcesView, const TAttribute<bool>& bInReadOnly)
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bUpdatesFromSelection = false;
	DetailsViewArgs.bLockable = false;
	DetailsViewArgs.bShowPropertyMatrixButton = false;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.ViewIdentifier = NAME_None;
	TSharedPtr<IDetailsView> SettingsDetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	// todo: use controller here instead of view widget
	SettingsDetailsView->OnFinishedChangingProperties().AddRaw(InSourcesView.Get(), &FLiveLinkSourcesView::OnPropertyChanged);
	SettingsDetailsView->SetIsPropertyEditingEnabledDelegate(FIsPropertyEditingEnabled::CreateLambda(
		[bInReadOnly](){
		return !bInReadOnly.Get();
	}));

	return SettingsDetailsView;
}

TSharedPtr<SLiveLinkDataView> CreateSubjectsDetailsView(FLiveLinkClient* InLiveLinkClient, const TAttribute<bool>& bInReadOnly)
{
	return SNew(SLiveLinkDataView, InLiveLinkClient)
		.ReadOnly(bInReadOnly);
}
} // namespace UE::LiveLink

FGuid FLiveLinkSourceUIEntry::GetGuid() const
{
	return EntryGuid;
}
FText FLiveLinkSourceUIEntry::GetSourceType() const
{
	return Client->GetSourceType(EntryGuid);
}
FText FLiveLinkSourceUIEntry::GetMachineName() const
{
	return Client->GetSourceMachineName(EntryGuid);
}
FText FLiveLinkSourceUIEntry::GetStatus() const
{
	return Client->GetSourceStatus(EntryGuid);
}
ULiveLinkSourceSettings* FLiveLinkSourceUIEntry::GetSourceSettings() const
{
	return Client->GetSourceSettings(EntryGuid);
}
void FLiveLinkSourceUIEntry::RemoveFromClient() const
{
	Client->RemoveSource(EntryGuid);
}
FText FLiveLinkSourceUIEntry::GetDisplayName() const
{
	return GetSourceType();
}

FLiveLinkSubjectUIEntry::FLiveLinkSubjectUIEntry(const FLiveLinkSubjectKey& InSubjectKey, FLiveLinkClient* InClient)
	: SubjectKey(InSubjectKey)
	, Client(InClient)
{
	if (InClient)
	{
		bIsVirtualSubject = InClient->IsVirtualSubject(InSubjectKey);
	}
}

bool FLiveLinkSubjectUIEntry::IsSubject() const
{
	return !SubjectKey.SubjectName.IsNone();
}

bool FLiveLinkSubjectUIEntry::IsSource() const
{
	return SubjectKey.SubjectName.IsNone();
}

bool FLiveLinkSubjectUIEntry::IsVirtualSubject() const
{
	return IsSubject() && bIsVirtualSubject;
}

UObject* FLiveLinkSubjectUIEntry::GetSettings() const
{
	if (IsSource())
	{
		return Client->GetSourceSettings(SubjectKey.Source);
	}
	else
	{
		return Client->GetSubjectSettings(SubjectKey);
	}
}

bool FLiveLinkSubjectUIEntry::IsSubjectEnabled() const
{
	return IsSubject() ? Client->IsSubjectEnabled(SubjectKey, false) : false;
}

bool FLiveLinkSubjectUIEntry::IsSubjectValid() const
{
	return IsSubject() ? Client->IsSubjectValid(SubjectKey) : false;
}

void FLiveLinkSubjectUIEntry::SetSubjectEnabled(bool bIsEnabled)
{
	if (IsSubject())
	{
		Client->SetSubjectEnabled(SubjectKey, bIsEnabled);
	}
}

FText FLiveLinkSubjectUIEntry::GetItemText() const
{
	if (IsSource())
	{
		return Client->GetSourceType(SubjectKey.Source);
	}
	else
	{
		return FText::FromName(SubjectKey.SubjectName);
	}
}

TSubclassOf<ULiveLinkRole> FLiveLinkSubjectUIEntry::GetItemRole() const
{
	return IsSubject() ? Client->GetSubjectRole(SubjectKey) : TSubclassOf<ULiveLinkRole>();
}

void FLiveLinkSubjectUIEntry::RemoveFromClient() const
{
	Client->RemoveSubject_AnyThread(SubjectKey);
}


class SLiveLinkClientPanelSubjectRow : public SMultiColumnTableRow<FLiveLinkSubjectUIEntryPtr>
{
public:
	SLATE_BEGIN_ARGS(SLiveLinkClientPanelSubjectRow) {}
	/** The list item for this row */
	SLATE_ARGUMENT(FLiveLinkSubjectUIEntryPtr, Entry)
	SLATE_ATTRIBUTE(bool, ReadOnly)
	SLATE_END_ARGS()


	void Construct(const FArguments& Args, const TSharedRef<STableViewBase>& OwnerTableView)
	{
		EntryPtr = Args._Entry;
		bReadOnly = Args._ReadOnly;

		SMultiColumnTableRow<FLiveLinkSubjectUIEntryPtr>::Construct(
			FSuperRowType::FArguments()
			.Padding(1.0f),
			OwnerTableView
		);
	}

	/** Overridden from SMultiColumnTableRow.  Generates a widget for this column of the list view. */
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
	{
		if (ColumnName == SubjectTreeUI::EnabledColumnName)
		{
			if (EntryPtr->IsSubject())
			{
				return SNew(SCheckBox)
					.Visibility(this, &SLiveLinkClientPanelSubjectRow::GetVisibilityFromReadOnly)
					.IsChecked(MakeAttributeSP(this, &SLiveLinkClientPanelSubjectRow::GetSubjectEnabled))
					.OnCheckStateChanged(this, &SLiveLinkClientPanelSubjectRow::OnEnabledChanged);
			}
		}
		else if (ColumnName == SubjectTreeUI::NameColumnName)
		{
			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(6, 0, 0, 0)
				[
					SNew(SExpanderArrow, SharedThis(this)).IndentAmount(12)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(STextBlock)
					.Text(MakeAttributeSP(this, &SLiveLinkClientPanelSubjectRow::GetItemText))
				];
		}
		else if (ColumnName == SubjectTreeUI::RoleColumnName)
		{
			auto RoleAttribute = MakeAttributeSP(this, &SLiveLinkClientPanelSubjectRow::GetItemRole);
			return SNew(STextBlock)
				.Text(EntryPtr->IsSubject() ? RoleAttribute : FText::GetEmpty());
		}
		else if (ColumnName == SubjectTreeUI::ActionsColumnName)
		{
			if (EntryPtr->IsVirtualSubject())
			{
				return SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.OnClicked(this, &SLiveLinkClientPanelSubjectRow::OnRemoveClicked)
					.ToolTipText(LOCTEXT("RemoveVirtualSubject", "Remove selected live link virtual subject"))
					.ContentPadding(0.f)
					.ForegroundColor(FSlateColor::UseForeground())
					.IsFocusable(false)
					.Visibility(this, &SLiveLinkClientPanelSubjectRow::GetVisibilityFromReadOnly)
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush("Icons.Delete"))
						.ColorAndOpacity(FSlateColor::UseForeground())
					];
			}
			else
			{
				return SNew(SBox)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Font(FAppStyle::Get().GetFontStyle("FontAwesome.8"))
						.ColorAndOpacity(this, &SLiveLinkClientPanelSubjectRow::OnGetActivityColor)
						.Text(FEditorFontGlyphs::Circle)
					];
			}
		}

		return SNullWidget::NullWidget;
	}

private:
	ECheckBoxState GetSubjectEnabled() const { return EntryPtr->IsSubjectEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }
	void OnEnabledChanged(ECheckBoxState NewState) { EntryPtr->SetSubjectEnabled(NewState == ECheckBoxState::Checked); }
	FText GetItemText() const { return EntryPtr->GetItemText(); }
	FText GetItemRole() const
	{
		TSubclassOf<ULiveLinkRole> Role = EntryPtr->GetItemRole();
		if (Role.Get())
		{
			return Role->GetDefaultObject<ULiveLinkRole>()->GetDisplayName();
		}
		return FText::GetEmpty();
	}

	FReply OnRemoveClicked()
	{
		EntryPtr->RemoveFromClient();
		return FReply::Handled();
	}

	FSlateColor OnGetActivityColor() const
	{
		if (EntryPtr->IsSubjectEnabled())
		{
			return EntryPtr->IsSubjectValid() ? GetDefault<ULiveLinkSettings>()->GetValidColor() : GetDefault<ULiveLinkSettings>()->GetInvalidColor();
		}

		if (!EntryPtr->IsSubjectEnabled() && EntryPtr->IsSubject())
		{
			UE_LOG(LogTemp, Warning, TEXT("Entry %s disabled"), *EntryPtr->SubjectKey.SubjectName.ToString());
		}
		return FLinearColor(0.f, 0.f, 0.f, 0.f);
	}

	/** Get widget visibility according to whether or not the panel is in read-only mode. */
	EVisibility GetVisibilityFromReadOnly() const
	{
		return bReadOnly.Get() ? EVisibility::Collapsed : EVisibility::Visible;
	}

	FLiveLinkSubjectUIEntryPtr EntryPtr;

	/** Returns whether the panel is in read-only mode. */
	TAttribute<bool> bReadOnly;
};

class SLiveLinkClientPanelSourcesRow : public SMultiColumnTableRow<FLiveLinkSourceUIEntryPtr>
{
public:
	SLATE_BEGIN_ARGS(SLiveLinkClientPanelSourcesRow) {}
	/** The list item for this row */
		SLATE_ARGUMENT(FLiveLinkSourceUIEntryPtr, Entry)
		SLATE_ATTRIBUTE(bool, ReadOnly)
	SLATE_END_ARGS()

	void Construct(const FArguments& Args, const TSharedRef<STableViewBase>& OwnerTableView)
	{
		EntryPtr = Args._Entry;
		bReadOnly = Args._ReadOnly;

		SMultiColumnTableRow<FLiveLinkSourceUIEntryPtr>::Construct(
			FSuperRowType::FArguments()
			.Padding(1.0f),
			OwnerTableView
		);
	}

	/** Overridden from SMultiColumnTableRow.  Generates a widget for this column of the list view. */
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
	{
		if (ColumnName == SourceListUI::TypeColumnName)
		{
			return SNew(STextBlock)
				.Text(EntryPtr->GetSourceType());
		}
		else if (ColumnName == SourceListUI::MachineColumnName)
		{
			return SNew(STextBlock)
				.Text(MakeAttributeSP(this, &SLiveLinkClientPanelSourcesRow::GetMachineName));
		}
		else if (ColumnName == SourceListUI::StatusColumnName)
		{
			return SNew(STextBlock)
				.Text(MakeAttributeSP(this, &SLiveLinkClientPanelSourcesRow::GetSourceStatus));
		}
		else if (ColumnName == SourceListUI::ActionsColumnName)
		{
			return SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Visibility(this, &SLiveLinkClientPanelSourcesRow::GetVisibilityFromReadOnly)
				.OnClicked(this, &SLiveLinkClientPanelSourcesRow::OnRemoveClicked)
				.ToolTipText(LOCTEXT("RemoveSource", "Remove selected live link source"))
				.ContentPadding(0.f)
				.ForegroundColor(FSlateColor::UseForeground())
				.IsFocusable(false)
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("Icons.Delete"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				];
		}

		return SNullWidget::NullWidget;
	}

private:
	FText GetMachineName() const
	{
		return EntryPtr->GetMachineName();
	}

	FText GetSourceStatus() const
	{
		return EntryPtr->GetStatus();
	}

	FReply OnRemoveClicked()
	{
		EntryPtr->RemoveFromClient();
		return FReply::Handled();
	}

	/** Get widget visibility according to whether or not the panel is in read-only mode. */
	EVisibility GetVisibilityFromReadOnly() const
	{
		return bReadOnly.Get() ? EVisibility::Collapsed : EVisibility::Visible;
	}

private:
	FLiveLinkSourceUIEntryPtr EntryPtr;

	/** Attribute used to query whether the panel is in read only mode or not. */
	TAttribute<bool> bReadOnly;
};

FLiveLinkSourcesView::FLiveLinkSourcesView(FLiveLinkClient* InLiveLinkClient, TSharedPtr<FUICommandList> InCommandList, TAttribute<bool> bInReadOnly, FOnSourceSelectionChanged InOnSourceSelectionChanged)
	: Client(InLiveLinkClient)
	, OnSourceSelectionChangedDelegate(MoveTemp(InOnSourceSelectionChanged))
	, bReadOnly(MoveTemp(bInReadOnly))
{
	CreateSourcesListView(InCommandList);
}

TSharedRef<ITableRow> FLiveLinkSourcesView::MakeSourceListViewWidget(FLiveLinkSourceUIEntryPtr Entry, const TSharedRef<STableViewBase>& OwnerTable) const
{
	return SNew(SLiveLinkClientPanelSourcesRow, OwnerTable)
		.Entry(Entry)
		.ReadOnly(bReadOnly);
}

void FLiveLinkSourcesView::OnSourceListSelectionChanged(FLiveLinkSourceUIEntryPtr Entry, ESelectInfo::Type SelectionType) const
{
	OnSourceSelectionChangedDelegate.Execute(Entry, SelectionType);
}

void FLiveLinkSourcesView::CreateSourcesListView(const TSharedPtr<FUICommandList>& InCommandList)
{
	SAssignNew(SourcesListView, SLiveLinkSourceListView, bReadOnly)
		.ListItemsSource(&SourceData)
		.SelectionMode(ESelectionMode::Single)
		.OnGenerateRow_Raw(this, &FLiveLinkSourcesView::MakeSourceListViewWidget)
		.OnContextMenuOpening_Raw(this, &FLiveLinkSourcesView::OnSourceConstructContextMenu, InCommandList)
		.OnSelectionChanged_Raw(this, &FLiveLinkSourcesView::OnSourceListSelectionChanged)
		.HeaderRow
		(
			SNew(SHeaderRow)
			+ SHeaderRow::Column(SourceListUI::TypeColumnName)
			.FillWidth(25.f)
			.DefaultLabel(LOCTEXT("TypeColumnHeaderName", "Source Type"))
			+ SHeaderRow::Column(SourceListUI::MachineColumnName)
			.FillWidth(25.f)
			.DefaultLabel(LOCTEXT("MachineColumnHeaderName", "Source Machine"))
			+ SHeaderRow::Column(SourceListUI::StatusColumnName)
			.FillWidth(50.f)
			.DefaultLabel(LOCTEXT("StatusColumnHeaderName", "Status"))
			+ SHeaderRow::Column(SourceListUI::ActionsColumnName)
			.ManualWidth(20.f)
			.DefaultLabel(LOCTEXT("ActionsColumnHeaderName", ""))
		);
}

TSharedPtr<SWidget> FLiveLinkSourcesView::OnSourceConstructContextMenu(TSharedPtr<FUICommandList> InCommandList)
{
	if (bReadOnly.Get())
	{
		return nullptr;
	}

	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, InCommandList);

	MenuBuilder.BeginSection(TEXT("Remove"));
	{
		if (CanRemoveSource())
		{
			MenuBuilder.AddMenuEntry(FLiveLinkClientCommands::Get().RemoveSource);
		}
		MenuBuilder.AddMenuEntry(FLiveLinkClientCommands::Get().RemoveAllSources);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void FLiveLinkSourcesView::RefreshSourceData(bool bRefreshUI)
{
	SourceData.Reset();

	for (FGuid SourceGuid : Client->GetDisplayableSources())
	{
		SourceData.Add(MakeShared<FLiveLinkSourceUIEntry>(SourceGuid, Client));
	}
	SourceData.Sort([](const FLiveLinkSourceUIEntryPtr& LHS, const FLiveLinkSourceUIEntryPtr& RHS) { return LHS->GetMachineName().CompareTo(RHS->GetMachineName()) < 0; });

	if (bRefreshUI)
	{
		SourcesListView->RequestListRefresh();
	}
}

void FLiveLinkSourcesView::HandleRemoveSource()
{
	TArray<FLiveLinkSourceUIEntryPtr> Selected;
	SourcesListView->GetSelectedItems(Selected);
	if (Selected.Num() > 0)
	{
		Selected[0]->RemoveFromClient();
	}
}

bool FLiveLinkSourcesView::CanRemoveSource()
{
	return SourcesListView->GetNumItemsSelected() > 0;
}

FLiveLinkSubjectsView::FLiveLinkSubjectsView(FOnSubjectSelectionChanged InOnSubjectSelectionChanged, const TSharedPtr<FUICommandList>& InCommandList, TAttribute<bool> bInReadOnly)
	: SubjectSelectionChangedDelegate(InOnSubjectSelectionChanged)
	, bReadOnly(MoveTemp(bInReadOnly))
{
	CreateSubjectsTreeView(InCommandList);
}

void FLiveLinkSubjectsView::OnSubjectSelectionChanged(FLiveLinkSubjectUIEntryPtr SubjectEntry, ESelectInfo::Type SelectInfo)
{
	SubjectSelectionChangedDelegate.Execute(SubjectEntry, SelectInfo);
}

void FLiveLinkSourcesView::OnPropertyChanged(const FPropertyChangedEvent& InEvent)
{
	TArray<FLiveLinkSourceUIEntryPtr> Selected;
	SourcesListView->GetSelectedItems(Selected);
	for (FLiveLinkSourceUIEntryPtr Item : Selected)
	{
		Client->OnPropertyChanged(Item->GetGuid(), InEvent);
	}
}

TSharedRef<ITableRow> FLiveLinkSubjectsView::MakeTreeRowWidget(FLiveLinkSubjectUIEntryPtr InInfo, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SLiveLinkClientPanelSubjectRow, OwnerTable)
		.Entry(InInfo)
		.ReadOnly(bReadOnly);
}

void FLiveLinkSubjectsView::GetChildrenForInfo(FLiveLinkSubjectUIEntryPtr InInfo, TArray< FLiveLinkSubjectUIEntryPtr >& OutChildren)
{
	OutChildren = InInfo->Children;
}

TSharedPtr<SWidget> FLiveLinkSubjectsView::OnOpenVirtualSubjectContextMenu(TSharedPtr<FUICommandList> InCommandList)
{
	if (bReadOnly.Get())
	{
		return nullptr;
	}

	constexpr bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, InCommandList);

	MenuBuilder.BeginSection(TEXT("Remove"));
	{
		if (CanRemoveSubject())
		{
			MenuBuilder.AddMenuEntry(FLiveLinkClientCommands::Get().RemoveSubject);
		}
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

bool FLiveLinkSubjectsView::CanRemoveSubject() const
{
	TArray<FLiveLinkSubjectUIEntryPtr> Selected;
	SubjectsTreeView->GetSelectedItems(Selected);
	return Selected.Num() > 0 && Selected[0] && Selected[0]->IsVirtualSubject();
}

void FLiveLinkSubjectsView::RefreshSubjects()
{
	TArray<FLiveLinkSubjectKey> SavedSelection;
	{
		TArray<FLiveLinkSubjectUIEntryPtr> SelectedItems = SubjectsTreeView->GetSelectedItems();
		for (const FLiveLinkSubjectUIEntryPtr& SelectedItem : SelectedItems)
		{
			SavedSelection.Add(SelectedItem->SubjectKey);
		}
	}

	if (IModularFeatures::Get().IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName))
	{
		if (ILiveLinkClient* Client = &IModularFeatures::Get().GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName))
		{
			TArray<FLiveLinkSubjectKey> SubjectKeys = Client->GetSubjects(true, true);
			SubjectData.Reset();

			TMap<FGuid, FLiveLinkSubjectUIEntryPtr> SourceHeaderItems;
			TArray<FLiveLinkSubjectUIEntryPtr> AllItems;
			AllItems.Reserve(SubjectKeys.Num());

			for (const FLiveLinkSubjectKey& SubjectKey : SubjectKeys)
			{
				FLiveLinkSubjectUIEntryPtr Source;
				if (FLiveLinkSubjectUIEntryPtr* SourcePtr = SourceHeaderItems.Find(SubjectKey.Source))
				{
					Source = *SourcePtr;
				}
				else
				{
					FLiveLinkSubjectKey SourceKey = SubjectKey;
					SourceKey.SubjectName = NAME_None;
					Source = MakeShared<FLiveLinkSubjectUIEntry>(SourceKey, static_cast<FLiveLinkClient*>(Client));
					SubjectData.Add(Source);
					SourceHeaderItems.Add(SubjectKey.Source) = Source;

					SubjectsTreeView->SetItemExpansion(Source, true);
					AllItems.Add(Source);
				}

				FLiveLinkSubjectUIEntryPtr SubjectEntry = MakeShared<FLiveLinkSubjectUIEntry>(SubjectKey, static_cast<FLiveLinkClient*>(Client));
				Source->Children.Add(SubjectEntry);
				AllItems.Add(SubjectEntry);
			}

			auto SortPredicate = [](const FLiveLinkSubjectUIEntryPtr& LHS, const FLiveLinkSubjectUIEntryPtr& RHS) {return LHS->GetItemText().CompareTo(RHS->GetItemText()) < 0; };
			SubjectData.Sort(SortPredicate);
			for (FLiveLinkSubjectUIEntryPtr& Subject : SubjectData)
			{
				Subject->Children.Sort(SortPredicate);
			}

			for (const FLiveLinkSubjectUIEntryPtr& Item : AllItems)
			{
				for (FLiveLinkSubjectKey& Selection : SavedSelection)
				{
					if (Item->SubjectKey == Selection)
					{
						SubjectsTreeView->SetItemSelection(Item, true);
						break;
					}
				}
			}

			SubjectsTreeView->RequestTreeRefresh();
		}
	}
}

void FLiveLinkSubjectsView::CreateSubjectsTreeView(const TSharedPtr<FUICommandList>& InCommandList)
{
	SAssignNew(SubjectsTreeView, SLiveLinkSubjectsTreeView, bReadOnly)
		.TreeItemsSource(&SubjectData)
		.OnGenerateRow_Raw(this, &FLiveLinkSubjectsView::MakeTreeRowWidget)
		.OnGetChildren_Raw(this, &FLiveLinkSubjectsView::GetChildrenForInfo)
		.OnSelectionChanged_Raw(this, &FLiveLinkSubjectsView::OnSubjectSelectionChanged)
		.OnContextMenuOpening_Raw(this, &FLiveLinkSubjectsView::OnOpenVirtualSubjectContextMenu, InCommandList)
		.SelectionMode(ESelectionMode::Single)
		.HeaderRow
		(
			SNew(SHeaderRow)
			+ SHeaderRow::Column(SubjectTreeUI::EnabledColumnName)
			.DefaultLabel(LOCTEXT("EnabledName", ""))
			.FixedWidth(22)
			+ SHeaderRow::Column(SubjectTreeUI::NameColumnName)
			.DefaultLabel(LOCTEXT("SubjectItemName", "Subject Name"))
			.FillWidth(0.60f)
			+ SHeaderRow::Column(SubjectTreeUI::RoleColumnName)
			.DefaultLabel(LOCTEXT("RoleName", "Role"))
			.FillWidth(0.40f)
			+ SHeaderRow::Column(SubjectTreeUI::ActionsColumnName)
			.ManualWidth(20.f)
			.DefaultLabel(LOCTEXT("ActionsColumnHeaderName", ""))
		);
}


#undef LOCTEXT_NAMESPACE /**LiveLinkClientPanel*/
