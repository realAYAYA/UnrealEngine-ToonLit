// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkClientPanel.h"

#include "Templates/SharedPointer.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/STextEntryPopup.h"
#include "Widgets/Views/SListView.h"
#include "SWarningOrErrorBox.h"

#include "ILiveLinkSource.h"
#include "LiveLinkClientCommands.h"
#include "LiveLinkClient.h"
#include "LiveLinkClientPanelToolbar.h"
#include "LiveLinkLog.h"
#include "LiveLinkSettings.h"
#include "LiveLinkSourceFactory.h"
#include "LiveLinkSourceSettings.h"
#include "LiveLinkSubjectSettings.h"
#include "SLiveLinkDataView.h"

#include "Editor.h"
#include "Editor/EditorPerformanceSettings.h"
#include "EditorFontGlyphs.h"
#include "Styling/AppStyle.h"
#include "MessageLogModule.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "UObject/UObjectHash.h"


#define LOCTEXT_NAMESPACE "LiveLinkClientPanel"

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

namespace
{
	/** Base class for live link list/tree views, handles removing list element by pressing delete. */
	template <typename ListType, typename ListElementType>
	class SLiveLinkListView : public ListType
	{
	public:
		virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override
		{
			if (InKeyEvent.GetKey() == EKeys::Delete || InKeyEvent.GetKey() == EKeys::BackSpace)
			{
				TArray<ListElementType> SelectedItem = ListType::GetSelectedItems();
				for (ListElementType Item : SelectedItem)
				{
					Item->RemoveFromClient();
				}
				return FReply::Handled();
			}

			return FReply::Unhandled();
		}
	};
}

// Structure that defines a single entry in the source UI
struct FLiveLinkSourceUIEntry
{
public:
	FLiveLinkSourceUIEntry(FGuid InEntryGuid, FLiveLinkClient* InClient)
		: EntryGuid(InEntryGuid)
		, Client(InClient)
	{}

	FGuid GetGuid() const { return EntryGuid; }
	FText GetSourceType() const { return Client->GetSourceType(EntryGuid); }
	FText GetMachineName() const { return Client->GetSourceMachineName(EntryGuid); }
	FText GetStatus() const { return Client->GetSourceStatus(EntryGuid); }
	ULiveLinkSourceSettings* GetSourceSettings() const { return Client->GetSourceSettings(EntryGuid); }
	void RemoveFromClient() const { Client->RemoveSource(EntryGuid); }
	FText GetDisplayName() const { return GetSourceType(); }

private:
	FGuid EntryGuid;
	FLiveLinkClient* Client;
};

// Structure that defines a single entry in the subject UI
struct FLiveLinkSubjectUIEntry
{
	FLiveLinkSubjectUIEntry(const FLiveLinkSubjectKey& InSubjectKey, FLiveLinkClient* InClient)
		: SubjectKey(InSubjectKey)
		, Client(InClient)
		, bIsVirtualSubject(false)
	{
		if (InClient)
		{
			bIsVirtualSubject = InClient->IsVirtualSubject(InSubjectKey);
		}
	}
	FLiveLinkSubjectKey SubjectKey;
	FLiveLinkClient* Client;

	// Children (if this entry represents a source instead of a specific subject
	TArray<FLiveLinkSubjectUIEntryPtr> Children;


	bool IsSubject() const { return !SubjectKey.SubjectName.IsNone(); }
	bool IsSource() const { return SubjectKey.SubjectName.IsNone(); }
	bool IsVirtualSubject() const { return (IsSubject() && bIsVirtualSubject); }

	UObject* GetSettings() const
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

	bool IsSubjectEnabled() const
	{
		return IsSubject() ? Client->IsSubjectEnabled(SubjectKey, false) : false;
	}

	bool IsSubjectValid() const
	{
		return IsSubject() ? Client->IsSubjectValid(SubjectKey) : false;
	}

	void SetSubjectEnabled(bool bIsEnabled)
	{
		if (IsSubject())
		{
			Client->SetSubjectEnabled(SubjectKey, bIsEnabled);
		}
	}

	FText GetItemText() const
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

	TSubclassOf<ULiveLinkRole> GetItemRole() const { return IsSubject() ? Client->GetSubjectRole(SubjectKey) : TSubclassOf<ULiveLinkRole>(); }

	void RemoveFromClient() const { Client->RemoveSubject_AnyThread(SubjectKey); }
	
private:
	bool bIsVirtualSubject;
};

class SLiveLinkClientPanelSubjectRow : public SMultiColumnTableRow<FLiveLinkSubjectUIEntryPtr>
{
public:
	SLATE_BEGIN_ARGS(SLiveLinkClientPanelSubjectRow) {}
		/** The list item for this row */
		SLATE_ARGUMENT(FLiveLinkSubjectUIEntryPtr, Entry)
	SLATE_END_ARGS()


	void Construct(const FArguments& Args, const TSharedRef<STableViewBase>& OwnerTableView)
	{
		EntryPtr = Args._Entry;

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
		return FLinearColor(0.f, 0.f, 0.f, 0.f);
	}

	FLiveLinkSubjectUIEntryPtr EntryPtr;
};

class SLiveLinkClientPanelSourcesRow : public SMultiColumnTableRow<FLiveLinkSourceUIEntryPtr>
{
public:
	SLATE_BEGIN_ARGS(SLiveLinkClientPanelSourcesRow) {}
		/** The list item for this row */
		SLATE_ARGUMENT(FLiveLinkSourceUIEntryPtr, Entry)
	SLATE_END_ARGS()

	void Construct(const FArguments& Args, const TSharedRef<STableViewBase>& OwnerTableView)
	{
		EntryPtr = Args._Entry;

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
			return	SNew(STextBlock)
					.Text(EntryPtr->GetSourceType());
		}
		else if (ColumnName == SourceListUI::MachineColumnName)
		{
			return	SNew(STextBlock)
					.Text(MakeAttributeSP(this, &SLiveLinkClientPanelSourcesRow::GetMachineName));
		}
		else if (ColumnName == SourceListUI::StatusColumnName)
		{
			return	SNew(STextBlock)
					.Text(MakeAttributeSP(this, &SLiveLinkClientPanelSourcesRow::GetSourceStatus));
		}
		else if (ColumnName == SourceListUI::ActionsColumnName)
		{
			return	SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
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

	FLiveLinkSourceUIEntryPtr EntryPtr;
};

class SLiveLinkSourceListView : public SLiveLinkListView<SListView<FLiveLinkSourceUIEntryPtr>, FLiveLinkSourceUIEntryPtr>
{
};

class SLiveLinkSubjectsTreeView : public SLiveLinkListView<STreeView<FLiveLinkSubjectUIEntryPtr>, FLiveLinkSubjectUIEntryPtr>
{
};

SLiveLinkClientPanel::~SLiveLinkClientPanel()
{
	if (Client)
	{
		Client->OnLiveLinkSourcesChanged().Remove(OnSourcesChangedHandle);
		OnSourcesChangedHandle.Reset();

		Client->OnLiveLinkSubjectsChanged().Remove(OnSubjectsChangedHandle);
		OnSubjectsChangedHandle.Reset();
	}
	GEditor->UnregisterForUndo(this);
}

void SLiveLinkClientPanel::Construct(const FArguments& Args, FLiveLinkClient* InClient)
{
	GEditor->RegisterForUndo(this);

	check(InClient);
	Client = InClient;

	bSelectionChangedGuard = false;
	DetailWidgetIndex = 0;

	OnSourcesChangedHandle = Client->OnLiveLinkSourcesChanged().AddSP(this, &SLiveLinkClientPanel::OnSourcesChangedHandler);
	OnSubjectsChangedHandle = Client->OnLiveLinkSubjectsChanged().AddSP(this, &SLiveLinkClientPanel::OnSubjectsChangedHandler);

	RefreshSourceData(false);

	CommandList = MakeShareable(new FUICommandList);

	BindCommands();

	// Connection Settings
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bUpdatesFromSelection = false;
	DetailsViewArgs.bLockable = false;
	DetailsViewArgs.bShowPropertyMatrixButton = false;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.ViewIdentifier = NAME_None;
	SettingsDetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	SettingsDetailsView->OnFinishedChangingProperties().AddSP(this, &SLiveLinkClientPanel::OnPropertyChanged);

	SAssignNew(SubjectsTreeView, SLiveLinkSubjectsTreeView)
		.TreeItemsSource(&SubjectData)
		.OnGenerateRow(this, &SLiveLinkClientPanel::MakeTreeRowWidget)
		.OnGetChildren(this, &SLiveLinkClientPanel::GetChildrenForInfo)
		.OnSelectionChanged(this, &SLiveLinkClientPanel::OnSubjectTreeSelectionChanged)
		.OnContextMenuOpening(this, &SLiveLinkClientPanel::OnOpenVirtualSubjectContextMenu)
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


	const FName LogName = "Live Link";
	TSharedPtr<class IMessageLogListing> MessageLogListing;

	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
	if (MessageLogModule.IsRegisteredLogListing(LogName))
	{
		MessageLogListing = MessageLogModule.GetLogListing(LogName);
	}

	TSharedRef<class SWidget> MessageLogListingWidget = MessageLogListing.IsValid() ? MessageLogModule.CreateLogListingWidget(MessageLogListing.ToSharedRef()) : SNullWidget::NullWidget;


	const int WarningPadding = 8;
	FProperty* PerformanceThrottlingProperty = FindFieldChecked<FProperty>(UEditorPerformanceSettings::StaticClass(), GET_MEMBER_NAME_CHECKED(UEditorPerformanceSettings, bThrottleCPUWhenNotForeground));
	PerformanceThrottlingProperty->GetDisplayNameText();
	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("PropertyName"), PerformanceThrottlingProperty->GetDisplayNameText());
	FText PerformanceWarningText = FText::Format(LOCTEXT("LiveLinkPerformanceWarningMessage", "Warning: The editor setting '{PropertyName}' is currently enabled.  This will stop editor windows from updating in realtime while the editor is not in focus."), Arguments);
	
	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("MessageLog.ListBorder")) // set panel background color to same color as message log at the bottom
		.Padding(FMargin(4.f, 4.f, 4.f, 4.f))
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SLiveLinkClientPanelToolbar, Client)
			]
			+SVerticalBox::Slot()
			.FillHeight(1.f)
			.Padding(FMargin(0.0f, 4.0f, 0.0f, 0.0f))
			[
				SNew(SSplitter)
				.Orientation(EOrientation::Orient_Vertical)
				+SSplitter::Slot()
				.Value(0.8f)
				[
					SNew(SSplitter)
					.Orientation(EOrientation::Orient_Horizontal)
					+SSplitter::Slot()
					.Value(0.5f)
					[
						SNew(SSplitter)
						.Orientation(EOrientation::Orient_Vertical)
						+SSplitter::Slot()
						.Value(0.25f)
						[
							SNew(SBorder)
							.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
							.Padding(FMargin(4.0f, 4.0f))
							[
								SAssignNew(SourceListView, SLiveLinkSourceListView)
								.ListItemsSource(&SourceData)
								.SelectionMode(ESelectionMode::Single)
								.OnGenerateRow(this, &SLiveLinkClientPanel::MakeSourceListViewWidget)
								.OnContextMenuOpening(this, &SLiveLinkClientPanel::OnSourceConstructContextMenu)
								.OnSelectionChanged(this, &SLiveLinkClientPanel::OnSourceListSelectionChanged)
								.HeaderRow
								(
									SNew(SHeaderRow)
									+SHeaderRow::Column(SourceListUI::TypeColumnName)
									.FillWidth(25.f)
									.DefaultLabel(LOCTEXT("TypeColumnHeaderName", "Source Type"))
									+SHeaderRow::Column(SourceListUI::MachineColumnName)
									.FillWidth(25.f)
									.DefaultLabel(LOCTEXT("MachineColumnHeaderName", "Source Machine"))
									+SHeaderRow::Column(SourceListUI::StatusColumnName)
									.FillWidth(50.f)
									.DefaultLabel(LOCTEXT("StatusColumnHeaderName", "Status"))
									+SHeaderRow::Column(SourceListUI::ActionsColumnName)
									.ManualWidth(20.f)
									.DefaultLabel(LOCTEXT("ActionsColumnHeaderName", ""))
								)
							]
						]
						+SSplitter::Slot()
						.Value(0.75f)
						[
							SNew(SBorder)
							.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
							.Padding(FMargin(4.0f, 4.0f))
							[
								SubjectsTreeView->AsShared()
							]
						]
					]
					+SSplitter::Slot()
					.Value(0.5f)
					[
						SNew(SWidgetSwitcher)
						.WidgetIndex(this, &SLiveLinkClientPanel::GetDetailWidgetIndex)
						+SWidgetSwitcher::Slot()
						[
							//[0] Detail view for Source
							SettingsDetailsView.ToSharedRef()
						]
						+SWidgetSwitcher::Slot()
						[
							// [1] Detail view for Subject, Frame data & Static data
							SAssignNew(DataDetailsView, SLiveLinkDataView, Client)
						]
					]
				]
				+SSplitter::Slot()
				.Value(0.2f)
				[
					SNew(SOverlay)
					+SOverlay::Slot()
					[
						MessageLogListingWidget
					]
					+SOverlay::Slot()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Bottom)
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(10, 4, 4, 10)
						[
							SNew(STextBlock)
							.Text(this, &SLiveLinkClientPanel::GetMessageCountText)
						]
						+SHorizontalBox::Slot()
						.Padding(20, 4, 50, 10)
						.AutoWidth()
						[
							SNew(STextBlock)
							.Text(this, &SLiveLinkClientPanel::GetSelectedMessageOccurrenceText)
						]
					]
				]
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(0.0f, 8.0f, 0.0f, 4.0f))
			[
				SNew(SWarningOrErrorBox)
				.Visibility(this, &SLiveLinkClientPanel::ShowEditorPerformanceThrottlingWarning)
				.MessageStyle(EMessageStyle::Warning)
				.Message(PerformanceWarningText)
				[
					SNew(SButton)
					.OnClicked(this, &SLiveLinkClientPanel::DisableEditorPerformanceThrottling)
					.TextStyle(FAppStyle::Get(), "DialogButtonText")
					.Text(LOCTEXT("LiveLinkPerformanceWarningDisable", "Disable"))
				]
			]
		]
	];


	RebuildSubjectList();
}

void SLiveLinkClientPanel::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObjects(DetailsPanelEditorObjects);
}

void SLiveLinkClientPanel::BindCommands()
{
	CommandList->MapAction(FLiveLinkClientCommands::Get().RemoveSource,
		FExecuteAction::CreateSP(this, &SLiveLinkClientPanel::HandleRemoveSource),
		FCanExecuteAction::CreateSP(this, &SLiveLinkClientPanel::CanRemoveSource)
	);

	CommandList->MapAction(FLiveLinkClientCommands::Get().RemoveAllSources,
		FExecuteAction::CreateSP(this, &SLiveLinkClientPanel::HandleRemoveAllSources),
		FCanExecuteAction::CreateSP(this, &SLiveLinkClientPanel::HasSource)
	);

	CommandList->MapAction(FLiveLinkClientCommands::Get().RemoveSubject,
	FExecuteAction::CreateSP(this, &SLiveLinkClientPanel::HandleRemoveSubject),
		FCanExecuteAction::CreateSP(this, &SLiveLinkClientPanel::CanRemoveSubject)
	);
}

void SLiveLinkClientPanel::RefreshSourceData(bool bRefreshUI)
{
	SourceData.Reset();

	for (FGuid SourceGuid : Client->GetDisplayableSources())
	{
		SourceData.Add(MakeShared<FLiveLinkSourceUIEntry>(SourceGuid, Client));
	}
	SourceData.Sort([](const FLiveLinkSourceUIEntryPtr& LHS, const FLiveLinkSourceUIEntryPtr& RHS) { return LHS->GetMachineName().CompareTo(RHS->GetMachineName()) < 0; });

	if (bRefreshUI)
	{
		SourceListView->RequestListRefresh();
	}
}

int32 SLiveLinkClientPanel::GetDetailWidgetIndex() const
{
	return DataDetailsView->GetSubjectKey().Source.IsValid() && !DataDetailsView->GetSubjectKey().SubjectName.IsNone() ? 1 : 0;
}

TSharedRef<ITableRow> SLiveLinkClientPanel::MakeSourceListViewWidget(FLiveLinkSourceUIEntryPtr Entry, const TSharedRef<STableViewBase>& OwnerTable) const
{
	return SNew(SLiveLinkClientPanelSourcesRow, OwnerTable)
		.Entry(Entry);
}

void SLiveLinkClientPanel::OnSourceListSelectionChanged(FLiveLinkSourceUIEntryPtr Entry, ESelectInfo::Type SelectionType) const
{
	if (bSelectionChangedGuard)
	{
		return;
	}
	TGuardValue<bool> ReentrantGuard(bSelectionChangedGuard, true);

	DataDetailsView->SetSubjectKey(FLiveLinkSubjectKey());

	int32 FoundSubjectEntryIndex = INDEX_NONE;
	if(Entry.IsValid())
	{
		SettingsDetailsView->SetObject(Entry->GetSourceSettings());

		// Find the corresponding subject
		FoundSubjectEntryIndex = SubjectData.IndexOfByPredicate([Entry](const FLiveLinkSubjectUIEntryPtr& SubjectEntry) { return SubjectEntry->SubjectKey.Source == Entry->GetGuid() && SubjectEntry->IsSource(); });
	}
	else
	{
		SettingsDetailsView->SetObject(nullptr);
	}

	// Set the corresponding subject
	if (FoundSubjectEntryIndex != INDEX_NONE)
	{
		SubjectsTreeView->SetSelection(SubjectData[FoundSubjectEntryIndex]);
	}
	else
	{
		SubjectsTreeView->ClearSelection();
	}
}

TSharedPtr<SWidget> SLiveLinkClientPanel::OnSourceConstructContextMenu()
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, CommandList);

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

TSharedRef<ITableRow> SLiveLinkClientPanel::MakeTreeRowWidget(FLiveLinkSubjectUIEntryPtr InInfo, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SLiveLinkClientPanelSubjectRow, OwnerTable)
		.Entry(InInfo);
}

void SLiveLinkClientPanel::GetChildrenForInfo(FLiveLinkSubjectUIEntryPtr InInfo, TArray< FLiveLinkSubjectUIEntryPtr >& OutChildren)
{
	OutChildren = InInfo->Children;
}

void SLiveLinkClientPanel::OnSubjectTreeSelectionChanged(FLiveLinkSubjectUIEntryPtr SubjectEntry, ESelectInfo::Type SelectInfo)
{
	if (bSelectionChangedGuard)
	{
		return;
	}
	TGuardValue<bool> ReentrantGuard(bSelectionChangedGuard, true);

	int32 FoundSourceIndex = INDEX_NONE;
	bool bDetailViewSet = false;
	if (SubjectEntry.IsValid())
	{
		// Find the corresponding Source entry
		FGuid SourceGuid = SubjectEntry->SubjectKey.Source;
		FoundSourceIndex = SourceData.IndexOfByPredicate([SourceGuid](FLiveLinkSourceUIEntryPtr SourceEntry) { return SourceEntry->GetGuid() == SourceGuid; });

		if (SubjectEntry->IsSource())
		{
			SettingsDetailsView->SetObject(SubjectEntry->GetSettings());
			DataDetailsView->SetSubjectKey(FLiveLinkSubjectKey());
		}
		else
		{
			SettingsDetailsView->SetObject(nullptr);
			DataDetailsView->SetSubjectKey(SubjectEntry->SubjectKey);
		}
		bDetailViewSet = true;
	}

	if (!bDetailViewSet)
	{
		SettingsDetailsView->SetObject(nullptr);
		DataDetailsView->SetSubjectKey(FLiveLinkSubjectKey());
	}

	// Select the corresponding Source entry
	if (FoundSourceIndex != INDEX_NONE)
	{
		SourceListView->SetSelection(SourceData[FoundSourceIndex]);
	}
	else
	{
		SourceListView->ClearSelection();
	}
}

TSharedPtr<SWidget> SLiveLinkClientPanel::OnOpenVirtualSubjectContextMenu()
{
	constexpr bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, CommandList);

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

void SLiveLinkClientPanel::RebuildSubjectList()
{
	TArray<FLiveLinkSubjectKey> SavedSelection;
	{
		TArray<FLiveLinkSubjectUIEntryPtr> SelectedItems = SubjectsTreeView->GetSelectedItems();
		for (const FLiveLinkSubjectUIEntryPtr& SelectedItem : SelectedItems)
		{
			SavedSelection.Add(SelectedItem->SubjectKey);
		}
	}

	TArray<FLiveLinkSubjectKey> SubjectKeys = Client->GetSubjects(true, true);
	SubjectData.Reset();

	TMap<FGuid, FLiveLinkSubjectUIEntryPtr> SourceHeaderItems;
	TArray<FLiveLinkSubjectUIEntryPtr> AllItems;
	AllItems.Reserve(SubjectKeys.Num());

	for (const FLiveLinkSubjectKey& SubjectKey : SubjectKeys)
	{
		FLiveLinkSubjectUIEntryPtr Source;
		if(FLiveLinkSubjectUIEntryPtr* SourcePtr = SourceHeaderItems.Find(SubjectKey.Source))
		{
			Source = *SourcePtr;
		}
		else
		{
			FLiveLinkSubjectKey SourceKey = SubjectKey;
			SourceKey.SubjectName = NAME_None;
			Source = MakeShared<FLiveLinkSubjectUIEntry>(SourceKey, Client);
			SubjectData.Add(Source);
			SourceHeaderItems.Add(SubjectKey.Source) = Source;

			SubjectsTreeView->SetItemExpansion(Source, true);
			AllItems.Add(Source);
		}

		FLiveLinkSubjectUIEntryPtr SubjectEntry = MakeShared<FLiveLinkSubjectUIEntry>(SubjectKey, Client);
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

void SLiveLinkClientPanel::OnPropertyChanged(const FPropertyChangedEvent& InEvent)
{
	TArray<FLiveLinkSourceUIEntryPtr> Selected;
	SourceListView->GetSelectedItems(Selected);
	for (FLiveLinkSourceUIEntryPtr Item : Selected)
	{
		Client->OnPropertyChanged(Item->GetGuid(), InEvent);
	}
}

void SLiveLinkClientPanel::HandleRemoveSource()
{
	TArray<FLiveLinkSourceUIEntryPtr> Selected;
	SourceListView->GetSelectedItems(Selected);
	if (Selected.Num() > 0)
	{
		Selected[0]->RemoveFromClient();
	}
}

bool SLiveLinkClientPanel::CanRemoveSource()
{
	return SourceListView->GetNumItemsSelected() > 0;
}

bool SLiveLinkClientPanel::HasSource() const
{
	constexpr bool bIncludeVirtualSources = true;
	return Client->GetDisplayableSources(bIncludeVirtualSources).Num() > 0;
}

void SLiveLinkClientPanel::HandleRemoveAllSources()
{
	Client->RemoveAllSources();
}

bool SLiveLinkClientPanel::CanRemoveSubject() const
{
	TArray<FLiveLinkSubjectUIEntryPtr> Selected;
	SubjectsTreeView->GetSelectedItems(Selected);
	return Selected.Num() > 0 && Selected[0] && Selected[0]->IsVirtualSubject();
}

void SLiveLinkClientPanel::HandleRemoveSubject()
{
	TArray<FLiveLinkSubjectUIEntryPtr> Selected;
	SubjectsTreeView->GetSelectedItems(Selected);
	if (Selected.Num() > 0 && Selected[0])
	{
		Selected[0]->RemoveFromClient();
	}
}

void SLiveLinkClientPanel::OnSourcesChangedHandler()
{
	RefreshSourceData(true);
	RebuildSubjectList();
}

void SLiveLinkClientPanel::OnSubjectsChangedHandler()
{
	RebuildSubjectList();
	SettingsDetailsView->ForceRefresh();
}

void SLiveLinkClientPanel::PostUndo(bool bSuccess)
{
	SettingsDetailsView->ForceRefresh();
}

void SLiveLinkClientPanel::PostRedo(bool bSuccess)
{
	SettingsDetailsView->ForceRefresh();
}

EVisibility SLiveLinkClientPanel::ShowEditorPerformanceThrottlingWarning() const
{
	const UEditorPerformanceSettings* Settings = GetDefault<UEditorPerformanceSettings>();
	return Settings->bThrottleCPUWhenNotForeground ? EVisibility::Visible : EVisibility::Collapsed;
}

FReply SLiveLinkClientPanel::DisableEditorPerformanceThrottling()
{
	UEditorPerformanceSettings* Settings = GetMutableDefault<UEditorPerformanceSettings>();
	Settings->bThrottleCPUWhenNotForeground = false;
	Settings->PostEditChange();
	Settings->SaveConfig();
	return FReply::Handled();
}

FText SLiveLinkClientPanel::GetMessageCountText() const
{
	int32 ErrorCount, WarningCount, InfoCount;
	FLiveLinkLog::GetInstance()->GetLogCount(ErrorCount, WarningCount, InfoCount);
	return FText::Format(LOCTEXT("MessageCountText", "{0} Error(s)  {1} Warning(s)"), FText::AsNumber(ErrorCount), FText::AsNumber(WarningCount));
}

FText SLiveLinkClientPanel::GetSelectedMessageOccurrenceText() const
{
	TPair<int32, FTimespan> Occurrence = FLiveLinkLog::GetInstance()->GetSelectedOccurrence();
	if (Occurrence.Get<0>() > 1)
	{
		return FText::Format(LOCTEXT("SelectedMessageOccurrenceText", "Last selected occurrence: {0}"), FText::FromString(Occurrence.Get<1>().ToString()));
	}
	return FText::GetEmpty();
}

#undef LOCTEXT_NAMESPACE