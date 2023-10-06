// Copyright Epic Games, Inc. All Rights Reserved.


#include "SSkeletonAnimNotifies.h"

#include "AnimAssetFindReplaceNotifies.h"
#include "AnimAssetFindReplaceSyncMarkers.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "AssetRegistry/AssetData.h"
#include "Animation/AnimSequenceBase.h"
#include "Styling/AppStyle.h"
#include "Animation/EditorSkeletonNotifyObj.h"

#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "ScopedTransaction.h"
#include "IEditableSkeleton.h"
#include "TabSpawners.h"
#include "Editor.h"
#include "IAnimationEditor.h"
#include "IAnimationBlueprintEditor.h"
#include "IAnimationSequenceBrowser.h"
#include "ISkeletonEditor.h"
#include "SAnimAssetFindReplace.h"
#include "Filters/GenericFilter.h"
#include "Filters/SBasicFilterBar.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "SkeletonAnimNotifies"

typedef TSharedPtr< FDisplayedAnimNotifyInfo > FDisplayedAnimNotifyInfoPtr;

class FSkeletonAnimNotifiesFilter : public FGenericFilter<EAnimNotifyFilterFlags>
{
public:
	FSkeletonAnimNotifiesFilter(EAnimNotifyFilterFlags InFlags, const FString& InName, const FText& InDisplayName, const FText& InToolTipText, FLinearColor InColor, TSharedPtr<FFilterCategory> InCategory)
		: FGenericFilter<EAnimNotifyFilterFlags>(InCategory, InName, InDisplayName, FGenericFilter<EAnimNotifyFilterFlags>::FOnItemFiltered())
		, Flags(InFlags)
	{
		ToolTip = InToolTipText;
		Color = InColor;
	}

	bool IsActive() const
	{
		return bIsActive;
	}

	EAnimNotifyFilterFlags GetFlags() const
	{
		return Flags;
	}

private:
	// FFilterBase interface
	virtual void ActiveStateChanged(bool bActive) override
	{
		bIsActive = bActive;
	}

	virtual bool PassesFilter(EAnimNotifyFilterFlags InItem) const override
	{
		return EnumHasAnyFlags(InItem, Flags);
	}
	
private:
	EAnimNotifyFilterFlags Flags;
	bool bIsActive = false;
};

/////////////////////////////////////////////////////
// FSkeletonAnimNotifiesSummoner

FSkeletonAnimNotifiesSummoner::FSkeletonAnimNotifiesSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp, const TSharedRef<class IEditableSkeleton>& InEditableSkeleton, FOnObjectsSelected InOnObjectsSelected)
	: FWorkflowTabFactory(FPersonaTabs::SkeletonAnimNotifiesID, InHostingApp)
	, EditableSkeleton(InEditableSkeleton)
	, OnObjectsSelected(InOnObjectsSelected)
{
	TabLabel = LOCTEXT("SkeletonAnimNotifiesTabTitle", "Animation Notifies");
	TabIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "Persona.Tabs.AnimationNotifies");

	EnableTabPadding();
	bIsSingleton = true;

	ViewMenuDescription = LOCTEXT("SkeletonAnimNotifiesMenu", "Animation Notifies");
	ViewMenuTooltip = LOCTEXT("SkeletonAnimNotifies_ToolTip", "Shows the skeletons notifies list");
}

TSharedRef<SWidget> FSkeletonAnimNotifiesSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	return SNew(SSkeletonAnimNotifies, EditableSkeleton.Pin().ToSharedRef(), HostingApp.Pin())
		.ShowNotifies(true)
		.ShowSyncMarkers(true)
		.OnObjectsSelected(OnObjectsSelected);
}

/////////////////////////////////////////////////////
// SSkeletonAnimNotifies

void SSkeletonAnimNotifies::Construct(const FArguments& InArgs, const TSharedRef<IEditableSkeleton>& InEditableSkeleton, const TSharedPtr<class FAssetEditorToolkit>& InHostingApp)
{
	OnObjectsSelected = InArgs._OnObjectsSelected;
	OnItemSelected = InArgs._OnItemSelected;
	bIsPicker = InArgs._IsPicker;
	bShowSyncMarkers = InArgs._ShowSyncMarkers;
	bShowNotifies = InArgs._ShowNotifies;

	EditableSkeleton = InEditableSkeleton;
	WeakHostingApp = InHostingApp;

	EditableSkeleton->RegisterOnNotifiesChanged(FSimpleDelegate::CreateSP(this, &SSkeletonAnimNotifies::OnNotifiesChanged));

	if(GEditor)
	{
		GEditor->RegisterForUndo(this);
	}

	FOnContextMenuOpening OnContextMenuOpening = !bIsPicker ? FOnContextMenuOpening::CreateSP(this, &SSkeletonAnimNotifies::OnGetContextMenuContent) : FOnContextMenuOpening();

	NameFilterBox = SNew( SSearchBox )
		.SelectAllTextWhenFocused( true )
		.OnTextChanged( this, &SSkeletonAnimNotifies::OnFilterTextChanged )
		.OnTextCommitted( this, &SSkeletonAnimNotifies::OnFilterTextCommitted );

	NotifiesListView = SNew( SAnimNotifyListType )
		.ListItemsSource( &NotifyList )
		.OnGenerateRow( this, &SSkeletonAnimNotifies::GenerateNotifyRow )
		.OnContextMenuOpening( OnContextMenuOpening )
		.OnSelectionChanged( this, &SSkeletonAnimNotifies::OnNotifySelectionChanged )
		.ItemHeight( 18.0f )
		.OnItemScrolledIntoView( this, &SSkeletonAnimNotifies::OnItemScrolledIntoView );
	
	TSharedPtr<FFilterCategory> FilterCategory = MakeShared<FFilterCategory>(LOCTEXT("AnimNotifyFiltersLabel", "Anim Notify Filters"), LOCTEXT("AnimNotifyFiltersLabelToolTip", "Filter what kind fo notifies and sync markers can be displayed."));

	// Hide filter UI if we are only displaying one type of thing
	const EVisibility FilterVisibility = (bShowNotifies ^ bShowSyncMarkers) ? EVisibility::Collapsed : EVisibility::Visible;

	TSharedRef<SBasicFilterBar<EAnimNotifyFilterFlags>> FilterBar = SNew(SBasicFilterBar<EAnimNotifyFilterFlags>)
	.Visibility(FilterVisibility)
	.CustomFilters(Filters)
	.UseSectionsForCategories(true)
	.OnFilterChanged_Lambda([this]()
	{
		CurrentFilterFlags = EAnimNotifyFilterFlags::None;

		for(const TSharedRef<FFilterBase<EAnimNotifyFilterFlags>>& Filter : Filters)
		{
			TSharedRef<FSkeletonAnimNotifiesFilter> AnimNotifiesFilter = StaticCastSharedRef<FSkeletonAnimNotifiesFilter>(Filter);
			if(AnimNotifiesFilter->IsActive())
			{
				CurrentFilterFlags |= AnimNotifiesFilter->GetFlags();
			}
		}

		RefreshNotifiesListWithFilter();
	});
	
	if(bShowNotifies)
	{
		TSharedRef<FFilterBase<EAnimNotifyFilterFlags>> Filter = Filters.Add_GetRef(MakeShared<FSkeletonAnimNotifiesFilter>(
			EAnimNotifyFilterFlags::Notifies,
			"Notifies",
			LOCTEXT("ShowNotifiesLabel", "Notifies"),
			LOCTEXT("ShowNotifiesTooltip", "Show notifies"),
			FLinearColor::Red.Desaturate(0.5f),
			FilterCategory
			));

		FilterBar->SetFilterCheckState(Filter, ECheckBoxState::Checked);
	}

	if(bShowSyncMarkers)
	{
		TSharedRef<FFilterBase<EAnimNotifyFilterFlags>> Filter = Filters.Add_GetRef(MakeShared<FSkeletonAnimNotifiesFilter>(
			EAnimNotifyFilterFlags::SyncMarkers,
			"SyncMarkers",
			LOCTEXT("ShowSyncMarkersLabel", "Sync Markers"),
			LOCTEXT("ShowSyncMarkersTooltip", "Show sync markers"),
			FLinearColor::Green.Desaturate(0.5f),
			FilterCategory
			));

		FilterBar->SetFilterCheckState(Filter, ECheckBoxState::Checked);
	}

	TSharedRef<SWidget> AddFilterButton = SBasicFilterBar<EAnimNotifyFilterFlags>::MakeAddFilterButton(FilterBar);
	AddFilterButton->SetVisibility(FilterVisibility);
	
	ChildSlot
	[
		SNew( SVerticalBox )

		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding( FMargin( 0.0f, 0.0f, 0.0f, 4.0f ) )
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f,0.0f)
			[
				AddFilterButton
			]

			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				NameFilterBox.ToSharedRef()
			]
		]

		+SVerticalBox::Slot()
		.AutoHeight()
		[
			FilterBar
		]

		+SVerticalBox::Slot()
		.FillHeight( 1.0f )		// This is required to make the scrollbar work, as content overflows Slate containers by default
		[
			NotifiesListView.ToSharedRef()
		]
	];

	CreateNotifiesList();
}

SSkeletonAnimNotifies::~SSkeletonAnimNotifies()
{
	if(GEditor)
	{
		GEditor->UnregisterForUndo(this);
	}
}

void SSkeletonAnimNotifies::OnNotifiesChanged()
{
	RefreshNotifiesListWithFilter();
}

void SSkeletonAnimNotifies::OnFilterTextChanged( const FText& SearchText )
{
	FilterText = SearchText;

	RefreshNotifiesListWithFilter();
}

void SSkeletonAnimNotifies::OnFilterTextCommitted( const FText& SearchText, ETextCommit::Type CommitInfo )
{
	// Just do the same as if the user typed in the box
	OnFilterTextChanged( SearchText );
}

TSharedRef<ITableRow> SSkeletonAnimNotifies::GenerateNotifyRow(TSharedPtr<FDisplayedAnimNotifyInfo> InInfo, const TSharedRef<STableViewBase>& OwnerTable)
{
	check( InInfo.IsValid() );

	return
		SNew( STableRow<TSharedPtr<FDisplayedAnimNotifyInfo>>, OwnerTable )
		[
			SNew( SVerticalBox )
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding( 0.0f, 4.0f )
			.VAlign( VAlign_Center )
			[
				SAssignNew(InInfo->InlineEditableText, SInlineEditableTextBlock)
				.Text(FText::FromName(InInfo->Name))
				.Font(FAppStyle::Get().GetFontStyle("SmallFont"))
				.OnVerifyTextChanged(this, &SSkeletonAnimNotifies::OnVerifyNotifyNameCommit, InInfo)
				.OnTextCommitted(this, &SSkeletonAnimNotifies::OnNotifyNameCommitted, InInfo)
				.IsSelected(this, &SSkeletonAnimNotifies::IsSelected)
				.HighlightText_Lambda([this](){ return FilterText; })
				.IsReadOnly(bIsPicker)
			]
		];
}

TSharedPtr<SWidget> SSkeletonAnimNotifies::OnGetContextMenuContent() const
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder( bShouldCloseWindowAfterMenuSelection, NULL);

	MenuBuilder.BeginSection("AnimItemAction", LOCTEXT("ItemActions", "New Item"));
	{
		FUIAction Action = FUIAction(FExecuteAction::CreateSP(const_cast<SSkeletonAnimNotifies*>(this), &SSkeletonAnimNotifies::OnAddItem, false));
		const FText Label = LOCTEXT("NewAnimNotifyButtonLabel", "New Notify...");
		const FText ToolTipText = LOCTEXT("NewAnimNotifyButtonTooltip", "Creates a new anim notify.");
		MenuBuilder.AddMenuEntry(Label, ToolTipText, FSlateIcon(), Action);
	}

	{
		FUIAction Action = FUIAction(FExecuteAction::CreateSP(const_cast<SSkeletonAnimNotifies*>(this), &SSkeletonAnimNotifies::OnAddItem, true));
		const FText Label = LOCTEXT("NewSyncMarkerButtonLabel", "New Sync Marker...");
		const FText ToolTipText = LOCTEXT("NewSyncMarkerButtonTooltip", "Creates a new sync marker.");
		MenuBuilder.AddMenuEntry(Label, ToolTipText, FSlateIcon(), Action);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("AnimNotifyAction", LOCTEXT("SelectedItemActions", "Selected Item Actions"));
	{
		{
			FUIAction Action = FUIAction(FExecuteAction::CreateSP(const_cast<SSkeletonAnimNotifies*>(this), &SSkeletonAnimNotifies::OnRenameItem),
				FCanExecuteAction::CreateSP(this, &SSkeletonAnimNotifies::CanPerformRename));
			const FText Label = LOCTEXT("RenameAnimNotifyButtonLabel", "Rename");
			const FText ToolTipText = LOCTEXT("RenameAnimNotifyButtonTooltip", "Renames the selected item.");
			MenuBuilder.AddMenuEntry(Label, ToolTipText, FSlateIcon(), Action);
		}

		{
			FUIAction Action = FUIAction(FExecuteAction::CreateSP(const_cast<SSkeletonAnimNotifies*>(this), &SSkeletonAnimNotifies::OnDeleteItems),
				FCanExecuteAction::CreateSP(this, &SSkeletonAnimNotifies::CanPerformDelete));
			const FText Label = LOCTEXT("DeleteAnimNotifyButtonLabel", "Delete");
			const FText ToolTipText = LOCTEXT("DeleteAnimNotifyButtonTooltip", "Deletes the selected items.");
			MenuBuilder.AddMenuEntry(Label, ToolTipText, FSlateIcon(), Action);
		}

		if(WeakHostingApp.IsValid() && NotifiesListView->GetNumItemsSelected() == 1)
		{
			FUIAction Action = FUIAction(FExecuteAction::CreateSP(const_cast<SSkeletonAnimNotifies*>(this), &SSkeletonAnimNotifies::OnFindReferences),
				FCanExecuteAction::CreateSP(this, &SSkeletonAnimNotifies::CanPerformDelete));
			const FText Label = LOCTEXT("FindNotifyReferences", "Find References");
			const FText ToolTipText = LOCTEXT("FindNotifyReferencesTooltip", "Find all references to this item in the asset browser");
			MenuBuilder.AddMenuEntry(Label, ToolTipText, FSlateIcon(), Action);
		}
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void SSkeletonAnimNotifies::OnNotifySelectionChanged(TSharedPtr<FDisplayedAnimNotifyInfo> Selection, ESelectInfo::Type SelectInfo)
{
	if(Selection.IsValid())
	{
		if (!Selection->bIsSyncMarker)
		{
			ShowNotifyInDetailsView(Selection->Name);
		}

		OnItemSelected.ExecuteIfBound(Selection->Name);
	}
}

bool SSkeletonAnimNotifies::CanPerformDelete() const
{
	return NotifiesListView->GetNumItemsSelected() > 0;
}

bool SSkeletonAnimNotifies::CanPerformRename() const
{
	return NotifiesListView->GetNumItemsSelected() == 1;
}

void SSkeletonAnimNotifies::OnAddItem(bool bIsSyncMarker)
{
	// Find a unique name for this notify
	const TCHAR* BaseNotifyString = bIsSyncMarker ? TEXT("NewSyncMarker") : TEXT("NewNotify");
	FString NewNotifyString = BaseNotifyString;
	int32 NumericExtension = 0;

	while(EditableSkeleton->GetSkeleton().AnimationNotifies.ContainsByPredicate([&NewNotifyString](const FName& InNotifyName){ return InNotifyName.ToString() == NewNotifyString; }))
	{
		NewNotifyString = FString::Printf(TEXT("%s_%d"), BaseNotifyString, NumericExtension);
		NumericExtension++;
	}

	// Add an item. The subsequent rename will commit the item.
	TSharedPtr<FDisplayedAnimNotifyInfo> NewItem = FDisplayedAnimNotifyInfo::Make(*NewNotifyString, bIsSyncMarker);
	NewItem->bIsNew = true;
	NotifyList.Add(NewItem);

	NotifiesListView->ClearSelection();
	NotifiesListView->RequestListRefresh();
	NotifiesListView->RequestScrollIntoView(NewItem);
}

void SSkeletonAnimNotifies::OnItemScrolledIntoView(TSharedPtr<FDisplayedAnimNotifyInfo> InItem, const TSharedPtr<ITableRow>& InTableRow)
{
	if(InItem.IsValid() && InItem->InlineEditableText.IsValid() && InItem->bIsNew)
	{
		InItem->InlineEditableText->EnterEditingMode();
	}
}

void SSkeletonAnimNotifies::OnDeleteItems()
{
	TArray< TSharedPtr< FDisplayedAnimNotifyInfo > > SelectedRows = NotifiesListView->GetSelectedItems();

	TArray<FName> SelectedSyncMarkerNames;
	TArray<FName> SelectedNotifyNames;

	for (TSharedPtr< FDisplayedAnimNotifyInfo > Selection : SelectedRows)
	{
		if(Selection->bIsSyncMarker)
		{
			SelectedSyncMarkerNames.Add(Selection->Name);
		}
		else
		{
			SelectedNotifyNames.Add(Selection->Name);
		}
	}

	int32 NumAnimationsModified = 0;
	
	if(SelectedSyncMarkerNames.Num())
	{
		NumAnimationsModified += EditableSkeleton->DeleteSyncMarkers(SelectedSyncMarkerNames);
	}

	if(SelectedNotifyNames.Num())
	{
		NumAnimationsModified += EditableSkeleton->DeleteAnimNotifies(SelectedNotifyNames);
	}

	if(NumAnimationsModified > 0)
	{
		FFormatNamedArguments Args;
		Args.Add( TEXT("NumAnimationsModified"), NumAnimationsModified );
		FNotificationInfo Info( FText::Format( LOCTEXT( "ItemsDeleted", "{NumAnimationsModified} animation(s) modified to delete items" ), Args ) );

		Info.bUseLargeFont = false;
		Info.ExpireDuration = 5.0f;

		NotifyUser( Info );
	}
	
	CreateNotifiesList(NameFilterBox->GetText().ToString());
}

void SSkeletonAnimNotifies::OnRenameItem()
{
	TArray< TSharedPtr< FDisplayedAnimNotifyInfo > > SelectedRows = NotifiesListView->GetSelectedItems();

	check(SelectedRows.Num() == 1); // Should be guaranteed by CanPerformRename

	SelectedRows[0]->InlineEditableText->EnterEditingMode();
}

bool SSkeletonAnimNotifies::OnVerifyNotifyNameCommit( const FText& NewName, FText& OutErrorMessage, TSharedPtr<FDisplayedAnimNotifyInfo> Item )
{
	bool bValid(true);

	if(NewName.IsEmpty())
	{
		OutErrorMessage = LOCTEXT( "NameMissing_Error", "You must provide a name." );
		bValid = false;
	}

	FName NotifyName( *NewName.ToString() );
	if(NotifyName != Item->Name || Item->bIsNew)
	{
		if(Item->bIsSyncMarker)
		{
			if(EditableSkeleton->GetSkeleton().GetExistingMarkerNames().Contains(NotifyName))
			{
				OutErrorMessage = FText::Format( LOCTEXT("AlreadyInUseMessage", "'{0}' is already in use."), NewName );
				bValid = false;
			}
		}
		else
		{
			if(EditableSkeleton->GetSkeleton().AnimationNotifies.Contains(NotifyName))
			{
				OutErrorMessage = FText::Format( LOCTEXT("AlreadyInUseMessage", "'{0}' is already in use."), NewName );
				bValid = false;
			}
		}
	}

	return bValid;
}

void SSkeletonAnimNotifies::OnNotifyNameCommitted( const FText& NewName, ETextCommit::Type, TSharedPtr<FDisplayedAnimNotifyInfo> Item )
{
	FName NewFName = FName(*NewName.ToString());
	if(Item->bIsNew)
	{
		if(Item->bIsSyncMarker)
		{
			EditableSkeleton->AddSyncMarker(NewFName);
		}
		else
		{
			EditableSkeleton->AddNotify(NewFName);
		}
		Item->bIsNew = false;
	}
	else
	{
		if(NewFName != Item->Name)
		{
			if(Item->bIsSyncMarker)
			{
				int32 NumAnimationsModified = EditableSkeleton->RenameSyncMarker(FName(*NewName.ToString()), Item->Name);
				if(NumAnimationsModified > 0)
				{
					FFormatNamedArguments Args;
					Args.Add( TEXT("NumAnimationsModified"), NumAnimationsModified );
					FNotificationInfo Info( FText::Format( LOCTEXT( "SyncMarkersRenamed", "{NumAnimationsModified} animation(s) modified to rename sync marker" ), Args ) );

					Info.bUseLargeFont = false;
					Info.ExpireDuration = 5.0f;

					NotifyUser( Info );
				}
			}
			else
			{
				int32 NumAnimationsModified = EditableSkeleton->RenameNotify(FName(*NewName.ToString()), Item->Name);
				if(NumAnimationsModified > 0)
				{
					FFormatNamedArguments Args;
					Args.Add( TEXT("NumAnimationsModified"), NumAnimationsModified );
					FNotificationInfo Info( FText::Format( LOCTEXT( "AnimNotifiesRenamed", "{NumAnimationsModified} animation(s) modified to rename notification" ), Args ) );

					Info.bUseLargeFont = false;
					Info.ExpireDuration = 5.0f;

					NotifyUser( Info );
				}
			}
			
			RefreshNotifiesListWithFilter();
		}
	}
}

void SSkeletonAnimNotifies::RefreshNotifiesListWithFilter()
{
	CreateNotifiesList( NameFilterBox->GetText().ToString() );
}

void SSkeletonAnimNotifies::CreateNotifiesList( const FString& SearchText )
{
	NotifyList.Empty();

	const USkeleton& TargetSkeleton = EditableSkeleton->GetSkeleton();

	auto AddItem = [this, SearchText](FName InItemName, bool bInIsSyncMarker)
	{
		if ( !SearchText.IsEmpty() )
		{
			if (InItemName.ToString().Contains( SearchText ) )
			{
				NotifyList.Add( FDisplayedAnimNotifyInfo::Make(InItemName, bInIsSyncMarker) );
			}
		}
		else
		{
			NotifyList.Add( FDisplayedAnimNotifyInfo::Make(InItemName, bInIsSyncMarker) );
		}
	};

	if(EnumHasAnyFlags(CurrentFilterFlags, EAnimNotifyFilterFlags::Notifies))
	{
		for(const FName& ItemName : TargetSkeleton.AnimationNotifies)
		{
			AddItem(ItemName, false);
		}
	}

	if(EnumHasAnyFlags(CurrentFilterFlags, EAnimNotifyFilterFlags::SyncMarkers))
	{
		for(const FName& ItemName : TargetSkeleton.GetExistingMarkerNames())
		{
			AddItem(ItemName, true);
		}
	}

	NotifiesListView->RequestListRefresh();
}

void SSkeletonAnimNotifies::ShowNotifyInDetailsView(FName NotifyName)
{
	if(OnObjectsSelected.IsBound())
	{
		ClearDetailsView();

		UEditorSkeletonNotifyObj *Obj = Cast<UEditorSkeletonNotifyObj>(ShowInDetailsView(UEditorSkeletonNotifyObj::StaticClass()));
		if(Obj != NULL)
		{
			Obj->EditableSkeleton = EditableSkeleton;
			Obj->Name = NotifyName;
		}
	}
}

UObject* SSkeletonAnimNotifies::ShowInDetailsView( UClass* EdClass )
{
	UObject *Obj = EditorObjectTracker.GetEditorObjectForClass(EdClass);

	if(Obj != NULL)
	{
		TArray<UObject*> Objects;
		Objects.Add(Obj);
		OnObjectsSelected.ExecuteIfBound(Objects);
	}
	return Obj;
}

void SSkeletonAnimNotifies::ClearDetailsView()
{
	TArray<UObject*> Objects;
	OnObjectsSelected.ExecuteIfBound(Objects);
}

void SSkeletonAnimNotifies::PostUndo( bool bSuccess )
{
	RefreshNotifiesListWithFilter();
}

void SSkeletonAnimNotifies::PostRedo( bool bSuccess )
{
	RefreshNotifiesListWithFilter();
}

void SSkeletonAnimNotifies::AddReferencedObjects( FReferenceCollector& Collector )
{
	EditorObjectTracker.AddReferencedObjects(Collector);
}

void SSkeletonAnimNotifies::NotifyUser( FNotificationInfo& NotificationInfo )
{
	TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification( NotificationInfo );
	if ( Notification.IsValid() )
	{
		Notification->SetCompletionState( SNotificationItem::CS_Fail );
	}
}

void SSkeletonAnimNotifies::OnFindReferences()
{
	TSharedPtr<FAssetEditorToolkit> HostingApp = WeakHostingApp.Pin();
	if (HostingApp.IsValid())
	{
		check(NotifiesListView->GetNumItemsSelected() == 1);
		TArray<TSharedPtr<FDisplayedAnimNotifyInfo>> SelectedItems;
		NotifiesListView->GetSelectedItems(SelectedItems);
		FName Name = SelectedItems[0]->Name;

		if(TSharedPtr<SDockTab> Tab = HostingApp->GetTabManager()->TryInvokeTab(FPersonaTabs::FindReplaceID))
		{
			TSharedRef<IAnimAssetFindReplace> FindReplaceWidget = StaticCastSharedRef<IAnimAssetFindReplace>(Tab->GetContent());
			FindReplaceWidget->SetCurrentProcessor(SelectedItems[0]->bIsSyncMarker ? UAnimAssetFindReplaceSyncMarkers::StaticClass() : UAnimAssetFindReplaceNotifies::StaticClass());
			UAnimAssetFindReplaceProcessor_StringBase* Processor = Cast<UAnimAssetFindReplaceProcessor_StringBase>(FindReplaceWidget->GetCurrentProcessor());
			Processor->SetFindString(Name.ToString());
		}
	}
}

#undef LOCTEXT_NAMESPACE
