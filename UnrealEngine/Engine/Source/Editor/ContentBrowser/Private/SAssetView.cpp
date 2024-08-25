// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAssetView.h"

#include "Algo/Transform.h"
#include "AssetRegistry/AssetRegistryState.h"
#include "AssetSelection.h"
#include "AssetToolsModule.h"
#include "AssetView/AssetViewConfig.h"
#include "AssetViewTypes.h"
#include "AssetViewWidgets.h"
#include "ContentBrowserConfig.h"
#include "ContentBrowserDataDragDropOp.h"
#include "ContentBrowserDataLegacyBridge.h"
#include "ContentBrowserDataSource.h"
#include "ContentBrowserDataSubsystem.h"
#include "ContentBrowserLog.h"
#include "ContentBrowserMenuContexts.h"
#include "ContentBrowserModule.h"
#include "ContentBrowserSingleton.h"
#include "ContentBrowserUtils.h"
#include "DesktopPlatformModule.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "DragDropHandler.h"
#include "Editor.h"
#include "EditorWidgetsModule.h"
#include "Engine/Blueprint.h"
#include "Engine/Level.h"
#include "Factories/Factory.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Notifications/NotificationManager.h"
#include "FrontendFilterBase.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "IAssetTools.h"
#include "IContentBrowserDataModule.h"
#include "Materials/Material.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/NamePermissionList.h"
#include "Misc/TextFilterUtils.h"
#include "ObjectTools.h"
#include "SContentBrowser.h"
#include "SFilterList.h"
#include "SPrimaryButton.h"
#include "Settings/ContentBrowserSettings.h"
#include "SlateOptMacros.h"
#include "Styling/AppStyle.h"
#include "TelemetryRouter.h"
#include "Textures/SlateIcon.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "ToolMenus.h"
#include "UObject/UnrealType.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SScrollBorder.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#include "ISourceControlModule.h"
#include "RevisionControlStyle/RevisionControlStyle.h"

#define LOCTEXT_NAMESPACE "ContentBrowser"
#define MAX_THUMBNAIL_SIZE 4096

#define ASSET_VIEW_PARANOIA_LIST_CHECKS (0)
#if ASSET_VIEW_PARANOIA_LIST_CHECKS
	#define checkAssetList(cond) check(cond)
#else
	#define checkAssetList(cond)
#endif

namespace
{
	/** Time delay between recently added items being added to the filtered asset items list */
	const double TimeBetweenAddingNewAssets = 4.0;

	/** Time delay between performing the last jump, and the jump term being reset */
	const double JumpDelaySeconds = 2.0;
}


FText SAssetView::ThumbnailSizeToDisplayName(EThumbnailSize InSize)
{
	switch (InSize)
	{
	case EThumbnailSize::Tiny:
		return LOCTEXT("TinyThumbnailSize", "Tiny");
	case EThumbnailSize::Small:
		return LOCTEXT("SmallThumbnailSize", "Small");
	case EThumbnailSize::Medium:
		return LOCTEXT("MediumThumbnailSize", "Medium");
	case EThumbnailSize::Large:
		return LOCTEXT("LargeThumbnailSize", "Large");
	case EThumbnailSize::Huge:
		return LOCTEXT("HugeThumbnailSize", "Huge");
	default:
		return FText::GetEmpty();
	}
}

class FAssetViewFrontendFilterHelper
{
public:
	explicit FAssetViewFrontendFilterHelper(SAssetView* InAssetView)
		: AssetView(InAssetView)
		, ContentBrowserData(IContentBrowserDataModule::Get().GetSubsystem())
		, bDisplayEmptyFolders(AssetView->IsShowingEmptyFolders())
	{
	}

	bool DoesItemPassQueryFilter(const TSharedPtr<FAssetViewItem>& InItemToFilter)
	{
		// Folders aren't subject to additional filtering
		if (InItemToFilter->IsFolder())
		{
			return true;
		}

		if (AssetView->OnShouldFilterItem.IsBound() && AssetView->OnShouldFilterItem.Execute(InItemToFilter->GetItem()))
		{
			return false;
		}

		// If we have OnShouldFilterAsset then it is assumed that we really only want to see true assets and 
		// nothing else so only include things that have asset data and also pass the query filter
		if (AssetView->OnShouldFilterAsset.IsBound())
		{
			FAssetData ItemAssetData;
			if (!InItemToFilter->GetItem().Legacy_TryGetAssetData(ItemAssetData) || AssetView->OnShouldFilterAsset.Execute(ItemAssetData))
			{
				return false;
			}
		}

		return true;
	}

	bool DoesItemPassFrontendFilter(const TSharedPtr<FAssetViewItem>& InItemToFilter)
	{
		// Folders are only subject to "empty" filtering
		if (InItemToFilter->IsFolder())
		{
			return ContentBrowserData->IsFolderVisible(InItemToFilter->GetItem().GetVirtualPath(), ContentBrowserUtils::GetIsFolderVisibleFlags(bDisplayEmptyFolders));
		}

		// Run the item through the filters
		if (AssetView->IsFrontendFilterActive() && !AssetView->PassesCurrentFrontendFilter(InItemToFilter->GetItem()))
		{
			return false;
		}

		return true;
	}

private:
	SAssetView* AssetView = nullptr;
	UContentBrowserDataSubsystem* ContentBrowserData = nullptr;
	const bool bDisplayEmptyFolders = true;
};

SAssetView::~SAssetView()
{
	if (IContentBrowserDataModule* ContentBrowserDataModule = IContentBrowserDataModule::GetPtr())
	{
		if (UContentBrowserDataSubsystem* ContentBrowserData = ContentBrowserDataModule->GetSubsystem())
		{
			ContentBrowserData->OnItemDataUpdated().RemoveAll(this);
			ContentBrowserData->OnItemDataRefreshed().RemoveAll(this);
			ContentBrowserData->OnItemDataDiscoveryComplete().RemoveAll(this);
		}
	}

	// Remove the listener for when view settings are changed
	UContentBrowserSettings::OnSettingChanged().RemoveAll(this);

	if ( FrontendFilters.IsValid() )
	{
		// Clear the frontend filter changed delegate
		FrontendFilters->OnChanged().RemoveAll( this );
	}
}


void SAssetView::Construct( const FArguments& InArgs )
{
	ViewCorrelationGuid = FGuid::NewGuid();	

	InitialNumAmortizedTasks = 0;
	TotalAmortizeTime = 0;
	AmortizeStartTime = 0;
	MaxSecondsPerFrame = 0.015f;

	bFillEmptySpaceInTileView = InArgs._FillEmptySpaceInTileView;
	FillScale = 1.0f;

	ThumbnailHintFadeInSequence.JumpToStart();
	ThumbnailHintFadeInSequence.AddCurve(0, 0.5f, ECurveEaseFunction::Linear);

	UContentBrowserDataSubsystem* ContentBrowserData = IContentBrowserDataModule::Get().GetSubsystem();
	ContentBrowserData->OnItemDataUpdated().AddSP(this, &SAssetView::HandleItemDataUpdated);
	ContentBrowserData->OnItemDataRefreshed().AddSP(this, &SAssetView::RequestSlowFullListRefresh);
	ContentBrowserData->OnItemDataDiscoveryComplete().AddSP(this, &SAssetView::HandleItemDataDiscoveryComplete);
	FilterCacheID.Initialaze(ContentBrowserData);

	FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();
	CollectionManagerModule.Get().OnAssetsAddedToCollection().AddSP( this, &SAssetView::OnAssetsAddedToCollection );
	CollectionManagerModule.Get().OnAssetsRemovedFromCollection().AddSP( this, &SAssetView::OnAssetsRemovedFromCollection );
	CollectionManagerModule.Get().OnCollectionRenamed().AddSP( this, &SAssetView::OnCollectionRenamed );
	CollectionManagerModule.Get().OnCollectionUpdated().AddSP( this, &SAssetView::OnCollectionUpdated );

	// Listen for when view settings are changed
	UContentBrowserSettings::OnSettingChanged().AddSP(this, &SAssetView::HandleSettingChanged);

	ThumbnailSize = InArgs._InitialThumbnailSize;

	// Get desktop metrics
	FDisplayMetrics DisplayMetrics;
	FSlateApplication::Get().GetCachedDisplayMetrics( DisplayMetrics );

	const FIntPoint DisplaySize(
		DisplayMetrics.PrimaryDisplayWorkAreaRect.Right - DisplayMetrics.PrimaryDisplayWorkAreaRect.Left,
		DisplayMetrics.PrimaryDisplayWorkAreaRect.Bottom - DisplayMetrics.PrimaryDisplayWorkAreaRect.Top );

	ThumbnailScaleRangeScalar = (float)DisplaySize.Y / 2160.f;

	// Use the shared ThumbnailPool for the rendering of thumbnails
	AssetThumbnailPool = UThumbnailManager::Get().GetSharedThumbnailPool();
	NumOffscreenThumbnails = 64;
	ListViewThumbnailResolution = 128;
	ListViewThumbnailSize = 64;
	ListViewThumbnailPadding = 4;
	TileViewThumbnailResolution = 256;
	TileViewThumbnailSize = 150;
	TileViewThumbnailPadding = 9;

	TileViewNameHeight = 50;

	MinThumbnailScale = 0.2f * ThumbnailScaleRangeScalar;
	MaxThumbnailScale = 1.9f * ThumbnailScaleRangeScalar;

	bCanShowClasses = InArgs._CanShowClasses;

	bCanShowFolders = InArgs._CanShowFolders;
	
	bCanShowReadOnlyFolders = InArgs._CanShowReadOnlyFolders;

	bFilterRecursivelyWithBackendFilter = InArgs._FilterRecursivelyWithBackendFilter;
		
	bCanShowRealTimeThumbnails = InArgs._CanShowRealTimeThumbnails;

	bCanShowDevelopersFolder = InArgs._CanShowDevelopersFolder;

	bCanShowFavorites = InArgs._CanShowFavorites;
	bCanDockCollections = InArgs._CanDockCollections;

	SelectionMode = InArgs._SelectionMode;

	bShowPathInColumnView = InArgs._ShowPathInColumnView;
	bShowTypeInColumnView = InArgs._ShowTypeInColumnView;
	bSortByPathInColumnView = bShowPathInColumnView && InArgs._SortByPathInColumnView;
	bShowTypeInTileView = InArgs._ShowTypeInTileView;
	bForceShowEngineContent = InArgs._ForceShowEngineContent;
	bForceShowPluginContent = InArgs._ForceShowPluginContent;
	bForceHideScrollbar = InArgs._ForceHideScrollbar;
	bShowDisallowedAssetClassAsUnsupportedItems = InArgs._ShowDisallowedAssetClassAsUnsupportedItems;

	bPendingUpdateThumbnails = false;
	bShouldNotifyNextAssetSync = true;
	CurrentThumbnailSize = TileViewThumbnailSize;

	SourcesData = InArgs._InitialSourcesData;
	BackendFilter = InArgs._InitialBackendFilter;

	FrontendFilters = InArgs._FrontendFilters;
	if ( FrontendFilters.IsValid() )
	{
		FrontendFilters->OnChanged().AddSP( this, &SAssetView::OnFrontendFiltersChanged );
	}

	OnShouldFilterAsset = InArgs._OnShouldFilterAsset;
	OnShouldFilterItem = InArgs._OnShouldFilterItem;

	OnNewItemRequested = InArgs._OnNewItemRequested;
	OnItemSelectionChanged = InArgs._OnItemSelectionChanged;
	OnItemsActivated = InArgs._OnItemsActivated;
	OnGetItemContextMenu = InArgs._OnGetItemContextMenu;
	OnItemRenameCommitted = InArgs._OnItemRenameCommitted;
	OnAssetTagWantsToBeDisplayed = InArgs._OnAssetTagWantsToBeDisplayed;
	OnIsAssetValidForCustomToolTip = InArgs._OnIsAssetValidForCustomToolTip;
	OnGetCustomAssetToolTip = InArgs._OnGetCustomAssetToolTip;
	OnVisualizeAssetToolTip = InArgs._OnVisualizeAssetToolTip;
	OnAssetToolTipClosing = InArgs._OnAssetToolTipClosing;
	OnGetCustomSourceAssets = InArgs._OnGetCustomSourceAssets;
	HighlightedText = InArgs._HighlightedText;
	ThumbnailLabel = InArgs._ThumbnailLabel;
	AllowThumbnailHintLabel = InArgs._AllowThumbnailHintLabel;
	InitialCategoryFilter = InArgs._InitialCategoryFilter;
	AssetShowWarningText = InArgs._AssetShowWarningText;
	bAllowDragging = InArgs._AllowDragging;
	bAllowFocusOnSync = InArgs._AllowFocusOnSync;
	HiddenColumnNames = DefaultHiddenColumnNames = InArgs._HiddenColumnNames;
	CustomColumns = InArgs._CustomColumns;
	OnSearchOptionsChanged = InArgs._OnSearchOptionsChanged;
	bShowPathViewFilters = InArgs._bShowPathViewFilters;
	OnExtendAssetViewOptionsMenuContext = InArgs._OnExtendAssetViewOptionsMenuContext;

	if ( InArgs._InitialViewType >= 0 && InArgs._InitialViewType < EAssetViewType::MAX )
	{
		CurrentViewType = InArgs._InitialViewType;
	}
	else
	{
		CurrentViewType = EAssetViewType::Tile;
	}

	bPendingSortFilteredItems = false;
	bQuickFrontendListRefreshRequested = false;
	bSlowFullListRefreshRequested = false;
	LastSortTime = 0;
	SortDelaySeconds = 8;

	bBulkSelecting = false;
	bAllowThumbnailEditMode = InArgs._AllowThumbnailEditMode;
	bThumbnailEditMode = false;
	bUserSearching = false;
	bPendingFocusOnSync = false;
	bWereItemsRecursivelyFiltered = false;

	OwningContentBrowser = InArgs._OwningContentBrowser;

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	AssetClassPermissionList = AssetToolsModule.Get().GetAssetClassPathPermissionList(EAssetClassAction::ViewAsset);
	FolderPermissionList = AssetToolsModule.Get().GetFolderPermissionList();
	WritableFolderPermissionList = AssetToolsModule.Get().GetWritableFolderPermissionList();

	FEditorWidgetsModule& EditorWidgetsModule = FModuleManager::LoadModuleChecked<FEditorWidgetsModule>("EditorWidgets");
	TSharedRef<SWidget> AssetDiscoveryIndicator = EditorWidgetsModule.CreateAssetDiscoveryIndicator(EAssetDiscoveryIndicatorScaleMode::Scale_Vertical);

	TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox);

	ChildSlot
	.Padding(0.0f)
	[
		SNew(SBorder)
		.Padding(0.f)
		.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
		[
			VerticalBox
		]
	];

	// Assets area
	VerticalBox->AddSlot()
	.FillHeight(1.f)
	[
		SNew( SVerticalBox ) 

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew( SBox )
			.Visibility_Lambda([this] { return InitialNumAmortizedTasks > 0 ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; })
			.HeightOverride( 2.f )
			[
				SNew( SProgressBar )
				.Percent( this, &SAssetView::GetIsWorkingProgressBarState )
				.BorderPadding( FVector2D(0,0) )
			]
		]
		
		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			SNew(SOverlay)

			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				SAssignNew(ViewContainer, SBox)
				.Padding(6.0f)

			]

			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			.Padding(FMargin(0, 14, 0, 0))
			[
				// A warning to display when there are no assets to show
				SNew( STextBlock )
				.Justification( ETextJustify::Center )
				.Text( this, &SAssetView::GetAssetShowWarningText )
				.Visibility( this, &SAssetView::IsAssetShowWarningTextVisible )
				.AutoWrapText( true )
			]

			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Bottom)
			.Padding(FMargin(24, 0, 24, 0))
			[
				// Asset discovery indicator
				AssetDiscoveryIndicator
			]

			+ SOverlay::Slot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Bottom)
			.Padding(FMargin(8, 0))
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ErrorReporting.EmptyBox"))
				.BorderBackgroundColor(this, &SAssetView::GetQuickJumpColor)
				.Visibility(this, &SAssetView::IsQuickJumpVisible)
				[
					SNew(STextBlock)
					.Text(this, &SAssetView::GetQuickJumpTerm)
				]
			]
		]
	];

	// Thumbnail edit mode banner
	VerticalBox->AddSlot()
	.AutoHeight()
	.Padding(0.f, 4.f)
	[
		SNew(SBorder)
		.Visibility( this, &SAssetView::GetEditModeLabelVisibility )
		.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
		.Content()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.Padding(4.f, 0.f, 0.f, 0.f)
			.FillWidth(1.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ThumbnailEditModeLabel", "Editing Thumbnails. Drag a thumbnail to rotate it if there is a 3D environment."))
				.ColorAndOpacity(FAppStyle::Get().GetSlateColor("Colors.Primary"))
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SPrimaryButton)
				.Text(LOCTEXT("EndThumbnailEditModeButton", "Done Editing"))
				.OnClicked(this, &SAssetView::EndThumbnailEditModeClicked)
			]
		]
	];

	if (InArgs._ShowBottomToolbar)
	{
		// Bottom panel
		VerticalBox->AddSlot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)

			// Asset count
			+SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			.Padding(8, 5)
			[
				SNew(STextBlock)
				.Text(this, &SAssetView::GetAssetCountText)
			]

			// View mode combo button
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SComboButton)
				.Visibility(InArgs._ShowViewOptions ? EVisibility::Visible : EVisibility::Collapsed)
				.ContentPadding(0.f)
				.ButtonStyle( FAppStyle::Get(), "ToggleButton" ) // Use the tool bar item style for this button
				.OnGetMenuContent( this, &SAssetView::GetViewButtonContent )
				.ButtonContent()
				[
					SNew(SHorizontalBox)
 
					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SImage)
						.Image( FAppStyle::GetBrush("GenericViewButton") )
					]
 
					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2.f, 0.f, 0.f, 0.f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text( LOCTEXT("ViewButton", "View Options") )
					]
				]
			]
		];
	}

	CreateCurrentView();

	if( InArgs._InitialAssetSelection.IsValid() )
	{
		// sync to the initial item without notifying of selection
		bShouldNotifyNextAssetSync = false;
		SyncToLegacy( MakeArrayView(&InArgs._InitialAssetSelection, 1), TArrayView<const FString>() );
	}

	// If currently looking at column, and you could choose to sort by path in column first and then name
	// Generalizing this is a bit difficult because the column ID is not accessible or is not known
	// Currently I assume this won't work, if this view mode is not column. Otherwise, I don't think sorting by path
	// is a good idea. 
	if (CurrentViewType == EAssetViewType::Column && bSortByPathInColumnView)
	{
		SortManager.SetSortColumnId(EColumnSortPriority::Primary, SortManager.PathColumnId);
		SortManager.SetSortColumnId(EColumnSortPriority::Secondary, SortManager.NameColumnId);
		SortManager.SetSortMode(EColumnSortPriority::Primary, EColumnSortMode::Ascending);
		SortManager.SetSortMode(EColumnSortPriority::Secondary, EColumnSortMode::Ascending);
		SortList();
	}
}

TOptional< float > SAssetView::GetIsWorkingProgressBarState() const
{
	if (InitialNumAmortizedTasks > 0)
	{
		const int32 CompletedTasks = FMath::Max(0, InitialNumAmortizedTasks - ItemsPendingFrontendFilter.Num());
		return static_cast<float>(CompletedTasks) / static_cast<float>(InitialNumAmortizedTasks);
	}
	return 0.0f;
}

void SAssetView::SetSourcesData(const FSourcesData& InSourcesData)
{
	// Update the path and collection lists
	SourcesData = InSourcesData;
	RequestSlowFullListRefresh();
	ClearSelection();
}

const FSourcesData& SAssetView::GetSourcesData() const
{
	return SourcesData;
}

bool SAssetView::IsAssetPathSelected() const
{
	UContentBrowserDataSubsystem* ContentBrowserData = IContentBrowserDataModule::Get().GetSubsystem();

	TArray<FName> InternalPaths;
	InternalPaths.Reserve(SourcesData.VirtualPaths.Num());
	for (const FName& VirtualPath : SourcesData.VirtualPaths)
	{
		FName ConvertedPath;
		if (ContentBrowserData->TryConvertVirtualPath(VirtualPath, ConvertedPath) == EContentBrowserPathType::Internal)
		{
			InternalPaths.Add(ConvertedPath);
		}
	}

	int32 NumAssetPaths, NumClassPaths;
	ContentBrowserUtils::CountPathTypes(InternalPaths, NumAssetPaths, NumClassPaths);

	// Check that only asset paths are selected
	return NumAssetPaths > 0 && NumClassPaths == 0;
}

void SAssetView::SetBackendFilter(const FARFilter& InBackendFilter)
{
	// Update the path and collection lists
	BackendFilter = InBackendFilter;
	RequestSlowFullListRefresh();
}

void SAssetView::AppendBackendFilter(FARFilter& FilterToAppendTo) const
{
	FilterToAppendTo.Append(BackendFilter);
}

void SAssetView::NewFolderItemRequested(const FContentBrowserItemTemporaryContext& NewItemContext)
{
	// Don't allow asset creation while renaming
	if (IsRenamingAsset())
	{
		return;
	}

	// we should only be creating one deferred folder at a time
	if (!ensureAlwaysMsgf(!DeferredItemToCreate.IsValid(), TEXT("Deferred new asset folder creation while there is already a deferred item creation: %s"), *NewItemContext.GetItem().GetItemName().ToString()))
	{
		if (DeferredItemToCreate->bWasAddedToView)
		{
			FContentBrowserItemKey ItemToRemoveKey(DeferredItemToCreate->ItemContext.GetItem());
			FilteredAssetItems.RemoveAll([&ItemToRemoveKey](const TSharedPtr<FAssetViewItem>& InAssetViewItem) { return ItemToRemoveKey == FContentBrowserItemKey(InAssetViewItem->GetItem()); });
			RefreshList();
		}

		DeferredItemToCreate.Reset();
	}


	// Folder creation requires focus to give object a name, otherwise object will not be created
	TSharedPtr<SWindow> OwnerWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
	if (OwnerWindow.IsValid() && !OwnerWindow->HasAnyUserFocusOrFocusedDescendants())
	{
		FSlateApplication::Get().SetUserFocus(FSlateApplication::Get().GetUserIndexForKeyboard(), AsShared(), EFocusCause::SetDirectly);
	}

	// Notify that we're about to start creating this item, as we may need to do things like ensure the parent folder is visible
	OnNewItemRequested.ExecuteIfBound(NewItemContext.GetItem());

	// Defer folder creation until next tick, so we get a chance to refresh the view
	DeferredItemToCreate = MakeUnique<FCreateDeferredItemData>();
	DeferredItemToCreate->ItemContext = NewItemContext;

	UE_LOG(LogContentBrowser, Log, TEXT("Deferred new asset folder creation: %s"), *NewItemContext.GetItem().GetItemName().ToString());
}

void SAssetView::NewFileItemRequested(const FContentBrowserItemDataTemporaryContext& NewItemContext)
{
	// Don't allow asset creation while renaming
	if (IsRenamingAsset())
	{
		return;
	}

	// We should only be creating one deferred file at a time
	if (!ensureAlwaysMsgf(!DeferredItemToCreate.IsValid(), TEXT("Deferred new asset file creation while there is already a deferred item creation: %s"), *NewItemContext.GetItemData().GetItemName().ToString()))
	{
		if (DeferredItemToCreate->bWasAddedToView)
		{
			FContentBrowserItemKey ItemToRemoveKey(DeferredItemToCreate->ItemContext.GetItem());
			FilteredAssetItems.RemoveAll([&ItemToRemoveKey](const TSharedPtr<FAssetViewItem>& InAssetViewItem){ return ItemToRemoveKey == FContentBrowserItemKey(InAssetViewItem->GetItem()); });
			RefreshList();
		}

		DeferredItemToCreate.Reset();
	}

	// File creation requires focus to give item a name, otherwise the item will not be created
	TSharedPtr<SWindow> OwnerWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
	if (OwnerWindow.IsValid() && !OwnerWindow->HasAnyUserFocusOrFocusedDescendants())
	{
		FSlateApplication::Get().SetUserFocus(FSlateApplication::Get().GetUserIndexForKeyboard(), AsShared(), EFocusCause::SetDirectly);
	}

	// Notify that we're about to start creating this item, as we may need to do things like ensure the parent folder is visible
	if (OnNewItemRequested.IsBound())
	{
		OnNewItemRequested.Execute(FContentBrowserItem(NewItemContext.GetItemData()));
	}

	// Defer file creation until next tick, so we get a chance to refresh the view
	DeferredItemToCreate = MakeUnique<FCreateDeferredItemData>();
	DeferredItemToCreate->ItemContext.AppendContext(CopyTemp(NewItemContext));

	UE_LOG(LogContentBrowser, Log, TEXT("Deferred new asset file creation: %s"), *NewItemContext.GetItemData().GetItemName().ToString());
}

void SAssetView::BeginCreateDeferredItem()
{
	if (DeferredItemToCreate.IsValid() && !DeferredItemToCreate->bWasAddedToView)
	{
		TSharedPtr<FAssetViewItem> NewItem = MakeShared<FAssetViewItem>(DeferredItemToCreate->ItemContext.GetItem());
		NewItem->RenameWhenScrolledIntoView();
		DeferredItemToCreate->bWasAddedToView = true;

		FilteredAssetItems.Insert(NewItem, 0);
		SortManager.SortList(FilteredAssetItems, MajorityAssetType, CustomColumns);

		SetSelection(NewItem);
		RequestScrollIntoView(NewItem);

		UE_LOG(LogContentBrowser, Log, TEXT("Creating deferred item: %s"), *NewItem->GetItem().GetItemName().ToString());
	}
}

FContentBrowserItem SAssetView::EndCreateDeferredItem(const TSharedPtr<FAssetViewItem>& InItem, const FString& InName, const bool bFinalize, FText& OutErrorText)
{
	FContentBrowserItem FinalizedItem;

	if (DeferredItemToCreate.IsValid() && DeferredItemToCreate->bWasAddedToView)
	{
		checkf(FContentBrowserItemKey(InItem->GetItem()) == FContentBrowserItemKey(DeferredItemToCreate->ItemContext.GetItem()), TEXT("DeferredItemToCreate was still set when attempting to rename a different item!"));

		// Remove the temporary item before we do any work to ensure the new item creation is not prevented
		FilteredAssetItems.Remove(InItem);
		RefreshList();

		// If not finalizing then we just discard the temporary
		if (bFinalize)
		{
			UContentBrowserDataSubsystem* ContentBrowserData = IContentBrowserDataModule::Get().GetSubsystem();
			FScopedSuppressContentBrowserDataTick TickSuppression(ContentBrowserData);

			if (DeferredItemToCreate->ItemContext.ValidateItem(InName, &OutErrorText))
			{
				FinalizedItem = DeferredItemToCreate->ItemContext.FinalizeItem(InName, &OutErrorText);
			}
		}
	}

	// Always reset the deferred item to avoid having it dangle, which can lead to potential crashes.
	DeferredItemToCreate.Reset();

	UE_LOG(LogContentBrowser, Log, TEXT("End creating deferred item %s"), *InItem->GetItem().GetItemName().ToString());

	return FinalizedItem;
}

void SAssetView::CreateNewAsset(const FString& DefaultAssetName, const FString& PackagePath, UClass* AssetClass, UFactory* Factory)
{
	ContentBrowserDataLegacyBridge::OnCreateNewAsset().ExecuteIfBound(*DefaultAssetName, *PackagePath, AssetClass, Factory, UContentBrowserDataMenuContext_AddNewMenu::FOnBeginItemCreation::CreateSP(this, &SAssetView::NewFileItemRequested));
}

void SAssetView::RenameItem(const FContentBrowserItem& ItemToRename)
{
	if (const TSharedPtr<FAssetViewItem> Item = AvailableBackendItems.FindRef(FContentBrowserItemKey(ItemToRename)))
	{
		Item->RenameWhenScrolledIntoView();
		
		SetSelection(Item);
		RequestScrollIntoView(Item);
	}
}

void SAssetView::SyncToItems(TArrayView<const FContentBrowserItem> ItemsToSync, const bool bFocusOnSync)
{
	PendingSyncItems.Reset();

	for (const FContentBrowserItem& Item : ItemsToSync)
	{
		PendingSyncItems.SelectedVirtualPaths.Add(Item.GetVirtualPath());
	}

	bPendingFocusOnSync = bFocusOnSync;
}

void SAssetView::SyncToVirtualPaths(TArrayView<const FName> VirtualPathsToSync, const bool bFocusOnSync)
{
	PendingSyncItems.Reset();
	for (const FName& VirtualPathToSync : VirtualPathsToSync)
	{
		PendingSyncItems.SelectedVirtualPaths.Add(VirtualPathToSync);
	}

	bPendingFocusOnSync = bFocusOnSync;
}

void SAssetView::SyncToLegacy(TArrayView<const FAssetData> AssetDataList, TArrayView<const FString> FolderList, const bool bFocusOnSync)
{
	PendingSyncItems.Reset();
	ContentBrowserUtils::ConvertLegacySelectionToVirtualPaths(AssetDataList, FolderList, /*UseFolderPaths*/false, PendingSyncItems.SelectedVirtualPaths);

	bPendingFocusOnSync = bFocusOnSync;
}

void SAssetView::SyncToSelection( const bool bFocusOnSync )
{
	PendingSyncItems.Reset();

	TArray<TSharedPtr<FAssetViewItem>> SelectedItems = GetSelectedViewItems();
	for (const TSharedPtr<FAssetViewItem>& Item : SelectedItems)
	{
		if (Item.IsValid())
		{
			PendingSyncItems.SelectedVirtualPaths.Add(Item->GetItem().GetVirtualPath());
		}
	}

	bPendingFocusOnSync = bFocusOnSync;
}

void SAssetView::ApplyHistoryData( const FHistoryData& History )
{
	SetSourcesData(History.SourcesData);
	PendingSyncItems = History.SelectionData;
	bPendingFocusOnSync = true;
}

TArray<TSharedPtr<FAssetViewItem>> SAssetView::GetSelectedViewItems() const
{
	switch ( GetCurrentViewType() )
	{
		case EAssetViewType::List: return ListView->GetSelectedItems();
		case EAssetViewType::Tile: return TileView->GetSelectedItems();
		case EAssetViewType::Column: return ColumnView->GetSelectedItems();
		default:
		ensure(0); // Unknown list type
		return TArray<TSharedPtr<FAssetViewItem>>();
	}
}

TArray<FContentBrowserItem> SAssetView::GetSelectedItems() const
{
	TArray<TSharedPtr<FAssetViewItem>> SelectedViewItems = GetSelectedViewItems();

	TArray<FContentBrowserItem> SelectedItems;
	for (const TSharedPtr<FAssetViewItem>& SelectedViewItem : SelectedViewItems)
	{
		if (!SelectedViewItem->IsTemporary())
		{
			SelectedItems.Emplace(SelectedViewItem->GetItem());
		}
	}
	return SelectedItems;
}

TArray<FContentBrowserItem> SAssetView::GetSelectedFolderItems() const
{
	TArray<TSharedPtr<FAssetViewItem>> SelectedViewItems = GetSelectedViewItems();

	TArray<FContentBrowserItem> SelectedFolders;
	for (const TSharedPtr<FAssetViewItem>& SelectedViewItem : SelectedViewItems)
	{
		if (SelectedViewItem->IsFolder() && !SelectedViewItem->IsTemporary())
		{
			SelectedFolders.Emplace(SelectedViewItem->GetItem());
		}
	}
	return SelectedFolders;
}

TArray<FContentBrowserItem> SAssetView::GetSelectedFileItems() const
{
	TArray<TSharedPtr<FAssetViewItem>> SelectedViewItems = GetSelectedViewItems();

	TArray<FContentBrowserItem> SelectedFiles;
	for (const TSharedPtr<FAssetViewItem>& SelectedViewItem : SelectedViewItems)
	{
		if (SelectedViewItem->IsFile() && !SelectedViewItem->IsTemporary())
		{
			SelectedFiles.Emplace(SelectedViewItem->GetItem());
		}
	}
	return SelectedFiles;
}

TArray<FAssetData> SAssetView::GetSelectedAssets() const
{
	TArray<TSharedPtr<FAssetViewItem>> SelectedViewItems = GetSelectedViewItems();

	// TODO: Abstract away?
	TArray<FAssetData> SelectedAssets;
	for (const TSharedPtr<FAssetViewItem>& SelectedViewItem : SelectedViewItems)
	{
		// Only report non-temporary & non-folder items
		FAssetData ItemAssetData;
		if (!SelectedViewItem->IsTemporary() && SelectedViewItem->IsFile() && SelectedViewItem->GetItem().Legacy_TryGetAssetData(ItemAssetData))
		{
			SelectedAssets.Add(MoveTemp(ItemAssetData));
		}
	}
	return SelectedAssets;
}

TArray<FString> SAssetView::GetSelectedFolders() const
{
	TArray<TSharedPtr<FAssetViewItem>> SelectedViewItems = GetSelectedViewItems();

	// TODO: Abstract away?
	TArray<FString> SelectedFolders;
	for (const TSharedPtr<FAssetViewItem>& SelectedViewItem : SelectedViewItems)
	{
		if (SelectedViewItem->IsFolder())
		{
			SelectedFolders.Emplace(SelectedViewItem->GetItem().GetVirtualPath().ToString());
		}
	}
	return SelectedFolders;
}

void SAssetView::RequestSlowFullListRefresh()
{
	bSlowFullListRefreshRequested = true;
}

void SAssetView::RequestQuickFrontendListRefresh()
{
	bQuickFrontendListRefreshRequested = true;
}

FString SAssetView::GetThumbnailScaleSettingPath(const FString& SettingsString) const
{
	return SettingsString + TEXT(".ThumbnailSize");
}

FString SAssetView::GetCurrentViewTypeSettingPath(const FString& SettingsString) const
{
	return SettingsString + TEXT(".CurrentViewType");
}

void SAssetView::SaveSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString) const
{
	GConfig->SetInt(*IniSection, *GetThumbnailScaleSettingPath(SettingsString), (int32)ThumbnailSize, IniFilename);
	GConfig->SetInt(*IniSection, *GetCurrentViewTypeSettingPath(SettingsString), CurrentViewType, IniFilename);
	
	GConfig->SetArray(*IniSection, *(SettingsString + TEXT(".HiddenColumns")), HiddenColumnNames, IniFilename);
}

void SAssetView::LoadSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString)
{
	int32 ThumbnailSizeConfig = (int32)EThumbnailSize::Medium;
	if ( GConfig->GetInt(*IniSection, *GetThumbnailScaleSettingPath(SettingsString), ThumbnailSizeConfig, IniFilename) )
	{
		// Clamp value to normal range and update state
		ThumbnailSizeConfig = FMath::Clamp<int32>(ThumbnailSizeConfig, 0, (int32)EThumbnailSize::MAX-1);

		ThumbnailSize = (EThumbnailSize) ThumbnailSizeConfig;
	}

	int32 ViewType = EAssetViewType::Tile;
	if ( GConfig->GetInt(*IniSection, *GetCurrentViewTypeSettingPath(SettingsString), ViewType, IniFilename) )
	{
		// Clamp value to normal range and update state
		if ( ViewType < 0 || ViewType >= EAssetViewType::MAX)
		{
			ViewType = EAssetViewType::Tile;
		}
		SetCurrentViewType( (EAssetViewType::Type)ViewType );
	}
	
	TArray<FString> LoadedHiddenColumnNames;
	GConfig->GetArray(*IniSection, *(SettingsString + TEXT(".HiddenColumns")), LoadedHiddenColumnNames, IniFilename);
	if (LoadedHiddenColumnNames.Num() > 0)
	{
		HiddenColumnNames = LoadedHiddenColumnNames;

		// Also update the visibility of the columns we just loaded in (unless this is called before creation and ColumnView doesn't exist)
		if(ColumnView)
		{
			for (const SHeaderRow::FColumn& Column : ColumnView->GetHeaderRow()->GetColumns())
			{
				ColumnView->GetHeaderRow()->SetShowGeneratedColumn(Column.ColumnId, !HiddenColumnNames.Contains(Column.ColumnId.ToString()));
			}
		}
	}
}

// Adjusts the selected asset by the selection delta, which should be +1 or -1)
void SAssetView::AdjustActiveSelection(int32 SelectionDelta)
{
	// Find the index of the first selected item
	TArray<TSharedPtr<FAssetViewItem>> SelectionSet = GetSelectedViewItems();
	
	int32 SelectedSuggestion = INDEX_NONE;

	if (SelectionSet.Num() > 0)
	{
		if (!FilteredAssetItems.Find(SelectionSet[0], /*out*/ SelectedSuggestion))
		{
			// Should never happen
			ensureMsgf(false, TEXT("SAssetView has a selected item that wasn't in the filtered list"));
			return;
		}
	}
	else
	{
		SelectedSuggestion = 0;
		SelectionDelta = 0;
	}

	if (FilteredAssetItems.Num() > 0)
	{
		// Move up or down one, wrapping around
		SelectedSuggestion = (SelectedSuggestion + SelectionDelta + FilteredAssetItems.Num()) % FilteredAssetItems.Num();

		// Pick the new asset
		const TSharedPtr<FAssetViewItem>& NewSelection = FilteredAssetItems[SelectedSuggestion];

		RequestScrollIntoView(NewSelection);
		SetSelection(NewSelection);
	}
	else
	{
		ClearSelection();
	}
}

void SAssetView::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	// Adjust min and max thumbnail scale based on dpi
	MinThumbnailScale = (0.2f * ThumbnailScaleRangeScalar)/AllottedGeometry.Scale;
	MaxThumbnailScale = (1.9f * ThumbnailScaleRangeScalar)/AllottedGeometry.Scale;

	CalculateFillScale( AllottedGeometry );

	CurrentTime = InCurrentTime;

	if (FSlateApplication::Get().GetActiveModalWindow().IsValid())
	{
		// If we're in a model window then we need to tick the thumbnail pool in order for thumbnails to render correctly.
		AssetThumbnailPool->Tick(InDeltaTime);
	}

	CalculateThumbnailHintColorAndOpacity();

	if (bPendingUpdateThumbnails)
	{
		UpdateThumbnails();
		bPendingUpdateThumbnails = false;
	}

	if (bSlowFullListRefreshRequested)
	{
		RefreshSourceItems();
		bSlowFullListRefreshRequested = false;
		bQuickFrontendListRefreshRequested = true;
	}

	bool bForceViewUpdate = false;
	if (bQuickFrontendListRefreshRequested)
	{
		ResetQuickJump();

		RefreshFilteredItems();

		bQuickFrontendListRefreshRequested = false;
		bForceViewUpdate = true; // If HasItemsPendingFilter is empty we still need to update the view
	}

	if (HasItemsPendingFilter() || bForceViewUpdate)
	{
		bForceViewUpdate = false;

		const double TickStartTime = FPlatformTime::Seconds();
		const bool bWasWorking = InitialNumAmortizedTasks > 0;

		// Mark the first amortize time
		if (AmortizeStartTime == 0)
		{
			AmortizeStartTime = FPlatformTime::Seconds();
			InitialNumAmortizedTasks = ItemsPendingFrontendFilter.Num();
			
			CurrentFrontendFilterTelemetry = { ViewCorrelationGuid, FilterSessionCorrelationGuid };
			CurrentFrontendFilterTelemetry.FrontendFilters = FrontendFilters;
			CurrentFrontendFilterTelemetry.TotalItemsToFilter = ItemsPendingFrontendFilter.Num() + ItemsPendingPriorityFilter.Num();
			CurrentFrontendFilterTelemetry.PriorityItemsToFilter = ItemsPendingPriorityFilter.Num();
		}

		int32 PreviousFilteredAssetItems = FilteredAssetItems.Num();
		ProcessItemsPendingFilter(TickStartTime);
		if (PreviousFilteredAssetItems == 0 && FilteredAssetItems.Num() != 0)
		{
			CurrentFrontendFilterTelemetry.ResultLatency = FPlatformTime::Seconds() - AmortizeStartTime;
		}
		CurrentFrontendFilterTelemetry.TotalResults = FilteredAssetItems.Num(); // Provide number of results even if filtering is interrupted

		if (HasItemsPendingFilter())
		{
			if (bPendingSortFilteredItems && InCurrentTime > LastSortTime + SortDelaySeconds)
			{
				// Don't sync to selection if we are just going to do it below
				SortList(!PendingSyncItems.Num());
			}
			
			CurrentFrontendFilterTelemetry.WorkDuration += FPlatformTime::Seconds() - TickStartTime;

			// Need to finish processing queried items before rest of function is safe
			return;
		}
		else
		{
			// Update the columns in the column view now that we know the majority type
			if (CurrentViewType == EAssetViewType::Column)
			{
				int32 HighestCount = 0;
				FName HighestType;
				for (auto TypeIt = FilteredAssetItemTypeCounts.CreateConstIterator(); TypeIt; ++TypeIt)
				{
					if (TypeIt.Value() > HighestCount)
					{
						HighestType = TypeIt.Key();
						HighestCount = TypeIt.Value();
					}
				}

				SetMajorityAssetType(HighestType);
			}

			if (bPendingSortFilteredItems && (bWasWorking || (InCurrentTime > LastSortTime + SortDelaySeconds)))
			{
				// Don't sync to selection if we are just going to do it below
				SortList(!PendingSyncItems.Num());
			}

			CurrentFrontendFilterTelemetry.WorkDuration += FPlatformTime::Seconds() - TickStartTime;

			double AmortizeDuration = FPlatformTime::Seconds() - AmortizeStartTime;
			TotalAmortizeTime += AmortizeDuration;
			AmortizeStartTime = 0;
			InitialNumAmortizedTasks = 0;
			
			OnCompleteFiltering(AmortizeDuration);
		}
	}

	if ( PendingSyncItems.Num() > 0 )
	{
		if (bPendingSortFilteredItems)
		{
			// Don't sync to selection because we are just going to do it below
			SortList(/*bSyncToSelection=*/false);
		}
		
		bBulkSelecting = true;
		ClearSelection();
		bool bFoundScrollIntoViewTarget = false;

		for ( auto ItemIt = FilteredAssetItems.CreateConstIterator(); ItemIt; ++ItemIt )
		{
			const auto& Item = *ItemIt;
			if(Item.IsValid())
			{
				if (PendingSyncItems.SelectedVirtualPaths.Contains(Item->GetItem().GetVirtualPath()))
				{
					SetItemSelection(Item, true, ESelectInfo::OnNavigation);
					
					// Scroll the first item in the list that can be shown into view
					if ( !bFoundScrollIntoViewTarget )
					{
						RequestScrollIntoView(Item);
						bFoundScrollIntoViewTarget = true;
					}
				}
			}
		}
	
		bBulkSelecting = false;

		if (bShouldNotifyNextAssetSync && !bUserSearching)
		{
			AssetSelectionChanged(TSharedPtr<FAssetViewItem>(), ESelectInfo::Direct);
		}

		// Default to always notifying
		bShouldNotifyNextAssetSync = true;

		PendingSyncItems.Reset();

		if (bAllowFocusOnSync && bPendingFocusOnSync)
		{
			FocusList();
		}
	}

	if ( IsHovered() )
	{
		// This prevents us from sorting the view immediately after the cursor leaves it
		LastSortTime = CurrentTime;
	}
	else if ( bPendingSortFilteredItems && InCurrentTime > LastSortTime + SortDelaySeconds )
	{
		SortList();
	}

	// create any pending items now
	BeginCreateDeferredItem();

	// Do quick-jump last as the Tick function might have canceled it
	if(QuickJumpData.bHasChangedSinceLastTick)
	{
		QuickJumpData.bHasChangedSinceLastTick = false;

		const bool bWasJumping = QuickJumpData.bIsJumping;
		QuickJumpData.bIsJumping = true;

		QuickJumpData.LastJumpTime = InCurrentTime;
		QuickJumpData.bHasValidMatch = PerformQuickJump(bWasJumping);
	}
	else if(QuickJumpData.bIsJumping && InCurrentTime > QuickJumpData.LastJumpTime + JumpDelaySeconds)
	{
		ResetQuickJump();
	}

	TSharedPtr<FAssetViewItem> AssetAwaitingRename = AwaitingRename.Pin();
	if (AssetAwaitingRename.IsValid())
	{
		TSharedPtr<SWindow> OwnerWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
		if (!OwnerWindow.IsValid())
		{
			AssetAwaitingRename->ClearRenameWhenScrolledIntoView();
			AwaitingRename = nullptr;
		}
		else if (OwnerWindow->HasAnyUserFocusOrFocusedDescendants())
		{
			AssetAwaitingRename->OnRenameRequested().ExecuteIfBound();
			AssetAwaitingRename->ClearRenameWhenScrolledIntoView();
			AwaitingRename = nullptr;
		}
	}
}

void SAssetView::CalculateFillScale( const FGeometry& AllottedGeometry )
{
	if ( bFillEmptySpaceInTileView && CurrentViewType == EAssetViewType::Tile )
	{
		float ItemWidth = GetTileViewItemBaseWidth();

		// Scrollbars are 16, but we add 1 to deal with half pixels.
		const float ScrollbarWidth = 16 + 1;
		float TotalWidth = AllottedGeometry.GetLocalSize().X -(ScrollbarWidth);
		float Coverage = TotalWidth / ItemWidth;
		int32 Items = (int)( TotalWidth / ItemWidth );

		// If there isn't enough room to support even a single item, don't apply a fill scale.
		if ( Items > 0 )
		{
			float GapSpace = ItemWidth * ( Coverage - (float)Items );
			float ExpandAmount = GapSpace / (float)Items;
			FillScale = ( ItemWidth + ExpandAmount ) / ItemWidth;
			FillScale = FMath::Max( 1.0f, FillScale );
		}
		else
		{
			FillScale = 1.0f;
		}
	}
	else
	{
		FillScale = 1.0f;
	}
}

void SAssetView::CalculateThumbnailHintColorAndOpacity()
{
	if ( HighlightedText.Get().IsEmpty() )
	{
		if ( ThumbnailHintFadeInSequence.IsPlaying() )
		{
			if ( ThumbnailHintFadeInSequence.IsForward() )
			{
				ThumbnailHintFadeInSequence.Reverse();
			}
		}
		else if ( ThumbnailHintFadeInSequence.IsAtEnd() ) 
		{
			ThumbnailHintFadeInSequence.PlayReverse(this->AsShared());
		}
	}
	else 
	{
		if ( ThumbnailHintFadeInSequence.IsPlaying() )
		{
			if ( ThumbnailHintFadeInSequence.IsInReverse() )
			{
				ThumbnailHintFadeInSequence.Reverse();
			}
		}
		else if ( ThumbnailHintFadeInSequence.IsAtStart() ) 
		{
			ThumbnailHintFadeInSequence.Play(this->AsShared());
		}
	}

	const float Opacity = ThumbnailHintFadeInSequence.GetLerp();
	ThumbnailHintColorAndOpacity = FLinearColor( 1.0, 1.0, 1.0, Opacity );
}

bool SAssetView::HasItemsPendingFilter() const
{
	return (ItemsPendingPriorityFilter.Num() + ItemsPendingFrontendFilter.Num()) > 0;
}

void SAssetView::ProcessItemsPendingFilter(const double TickStartTime)
{
	const double ProcessItemsPendingFilterStartTime = FPlatformTime::Seconds();

	FAssetViewFrontendFilterHelper FrontendFilterHelper(this);

	auto UpdateFilteredAssetItemTypeCounts = [this](const TSharedPtr<FAssetViewItem>& InItem)
	{
		if (CurrentViewType == EAssetViewType::Column)
		{
			const FContentBrowserItemDataAttributeValue TypeNameValue = InItem->GetItem().GetItemAttribute(ContentBrowserItemAttributes::ItemTypeName);
			if (TypeNameValue.IsValid())
			{
				FilteredAssetItemTypeCounts.FindOrAdd(TypeNameValue.GetValue<FName>())++;
			}
		}
	};

	const bool bRunQueryFilter = OnShouldFilterAsset.IsBound() || OnShouldFilterItem.IsBound();
	const bool bFlushAllPendingItems = TickStartTime < 0;

	bool bRefreshList = false;
	bool bHasTimeRemaining = true;

	auto FilterItem = [this, bRunQueryFilter, &bRefreshList, &FrontendFilterHelper, &UpdateFilteredAssetItemTypeCounts](const TSharedPtr<FAssetViewItem>& ItemToFilter)
	{
		// Run the query filter if required
		if (bRunQueryFilter)
		{
			const bool bPassedBackendFilter = FrontendFilterHelper.DoesItemPassQueryFilter(ItemToFilter);
			if (!bPassedBackendFilter)
			{
				AvailableBackendItems.Remove(FContentBrowserItemKey(ItemToFilter->GetItem()));
				return;
			}
		}

		// Run the frontend filter
		{
			const bool bPassedFrontendFilter = FrontendFilterHelper.DoesItemPassFrontendFilter(ItemToFilter);
			if (bPassedFrontendFilter)
			{
				checkAssetList(!FilteredAssetItems.Contains(ItemToFilter));

				bRefreshList = true;
				FilteredAssetItems.Add(ItemToFilter);
				UpdateFilteredAssetItemTypeCounts(ItemToFilter);
			}
		}
	};

	// Run the prioritized set first
	// This data must be processed this frame, so skip the amortization time checks within the loop itself
	if (ItemsPendingPriorityFilter.Num() > 0)
	{
		for (const TSharedPtr<FAssetViewItem>& ItemToFilter : ItemsPendingPriorityFilter)
		{
			// Make sure this item isn't pending in another list
			{
				const uint32 ItemToFilterHash = GetTypeHash(ItemToFilter);
				ItemsPendingFrontendFilter.RemoveByHash(ItemToFilterHash, ItemToFilter);
			}

			// Apply any filters and update the view
			FilterItem(ItemToFilter);
		}
		ItemsPendingPriorityFilter.Reset();

		// Check to see if we have run out of time in this tick
		if (!bFlushAllPendingItems && (FPlatformTime::Seconds() - TickStartTime) > MaxSecondsPerFrame)
		{
			bHasTimeRemaining = false;
		}
	}

	// Filter as many items as possible until we run out of time
	if (bHasTimeRemaining && ItemsPendingFrontendFilter.Num() > 0)
	{
		for (auto ItemIter = ItemsPendingFrontendFilter.CreateIterator(); ItemIter; ++ItemIter)
		{
			const TSharedPtr<FAssetViewItem> ItemToFilter = *ItemIter;
			ItemIter.RemoveCurrent();

			// Apply any filters and update the view
			FilterItem(ItemToFilter);

			// Check to see if we have run out of time in this tick
			if (!bFlushAllPendingItems && (FPlatformTime::Seconds() - TickStartTime) > MaxSecondsPerFrame)
			{
				bHasTimeRemaining = false;
				break;
			}
		}
	}

	if (bRefreshList)
	{
		bPendingSortFilteredItems = true;
		RefreshList();
	}

	UE_LOG(LogContentBrowser, VeryVerbose, TEXT("AssetView - ProcessItemsPendingFilter completed in %0.4f seconds"), FPlatformTime::Seconds() - ProcessItemsPendingFilterStartTime);
}

void SAssetView::OnDragLeave( const FDragDropEvent& DragDropEvent )
{
	TSharedPtr< FAssetDragDropOp > AssetDragDropOp = DragDropEvent.GetOperationAs< FAssetDragDropOp >();
	if( AssetDragDropOp.IsValid() )
	{
		AssetDragDropOp->ResetToDefaultToolTip();
	}

	TSharedPtr<FDragDropOperation> DragDropOp = DragDropEvent.GetOperation();
	if (DragDropOp.IsValid())
	{
		// Do we have a custom handler for this drag event?
		FContentBrowserModule& ContentBrowserModule = FModuleManager::GetModuleChecked<FContentBrowserModule>("ContentBrowser");
		const TArray<FAssetViewDragAndDropExtender>& AssetViewDragAndDropExtenders = ContentBrowserModule.GetAssetViewDragAndDropExtenders();
		for (const auto& AssetViewDragAndDropExtender : AssetViewDragAndDropExtenders)
		{
			if (AssetViewDragAndDropExtender.OnDragLeaveDelegate.IsBound() && AssetViewDragAndDropExtender.OnDragLeaveDelegate.Execute(FAssetViewDragAndDropExtender::FPayload(DragDropOp, SourcesData.VirtualPaths, SourcesData.Collections)))
			{
				return;
			}
		}
	}
}

FReply SAssetView::OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	TSharedPtr<FDragDropOperation> DragDropOp = DragDropEvent.GetOperation();
	if (DragDropOp.IsValid())
	{
		// Do we have a custom handler for this drag event?
		FContentBrowserModule& ContentBrowserModule = FModuleManager::GetModuleChecked<FContentBrowserModule>("ContentBrowser");
		const TArray<FAssetViewDragAndDropExtender>& AssetViewDragAndDropExtenders = ContentBrowserModule.GetAssetViewDragAndDropExtenders();
		for (const auto& AssetViewDragAndDropExtender : AssetViewDragAndDropExtenders)
		{
			if (AssetViewDragAndDropExtender.OnDragOverDelegate.IsBound() && AssetViewDragAndDropExtender.OnDragOverDelegate.Execute(FAssetViewDragAndDropExtender::FPayload(DragDropOp, SourcesData.VirtualPaths, SourcesData.Collections)))
			{
				return FReply::Handled();
			}
		}
	}

	if (SourcesData.HasVirtualPaths())
	{
		UContentBrowserDataSubsystem* ContentBrowserData = IContentBrowserDataModule::Get().GetSubsystem();

		const FContentBrowserItem DropFolderItem = ContentBrowserData->GetItemAtPath(SourcesData.VirtualPaths[0], EContentBrowserItemTypeFilter::IncludeFolders);
		if (DropFolderItem.IsValid() && DragDropHandler::HandleDragOverItem(DropFolderItem, DragDropEvent))
		{
			return FReply::Handled();
		}
	}
	else if (HasSingleCollectionSource())
	{
		TArray<FSoftObjectPath> NewCollectionItems;

		if (TSharedPtr<FContentBrowserDataDragDropOp> ContentDragDropOp = DragDropEvent.GetOperationAs<FContentBrowserDataDragDropOp>())
		{
			for (const FContentBrowserItem& DraggedItem : ContentDragDropOp->GetDraggedFiles())
			{
				FSoftObjectPath CollectionItemId;
				if (DraggedItem.TryGetCollectionId(CollectionItemId))
				{
					NewCollectionItems.Add(CollectionItemId);
				}
			}
		}
		else
		{
			const TArray<FAssetData> AssetDatas = AssetUtil::ExtractAssetDataFromDrag(DragDropEvent);
			Algo::Transform(AssetDatas, NewCollectionItems, &FAssetData::GetSoftObjectPath);
		}

		if (NewCollectionItems.Num() > 0)
		{
			if (TSharedPtr<FAssetDragDropOp> AssetDragDropOp = DragDropEvent.GetOperationAs<FAssetDragDropOp>())
			{
				TArray<FSoftObjectPath> ObjectPaths;
				FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();
				const FCollectionNameType& Collection = SourcesData.Collections[0];
				CollectionManagerModule.Get().GetObjectsInCollection(Collection.Name, Collection.Type, ObjectPaths);

				bool IsValidDrop = false;
				for (const FSoftObjectPath& NewCollectionItem : NewCollectionItems)
				{
					if (!ObjectPaths.Contains(NewCollectionItem))
					{
						IsValidDrop = true;
						break;
					}
				}

				if (IsValidDrop)
				{
					AssetDragDropOp->SetToolTip(NSLOCTEXT("AssetView", "OnDragOverCollection", "Add to Collection"), FAppStyle::GetBrush(TEXT("Graph.ConnectorFeedback.OK")));
				}
			}

			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

FReply SAssetView::OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	TSharedPtr<FDragDropOperation> DragDropOp = DragDropEvent.GetOperation();
	if (DragDropOp.IsValid())
	{
		// Do we have a custom handler for this drag event?
		FContentBrowserModule& ContentBrowserModule = FModuleManager::GetModuleChecked<FContentBrowserModule>("ContentBrowser");
		const TArray<FAssetViewDragAndDropExtender>& AssetViewDragAndDropExtenders = ContentBrowserModule.GetAssetViewDragAndDropExtenders();
		for (const auto& AssetViewDragAndDropExtender : AssetViewDragAndDropExtenders)
		{
			if (AssetViewDragAndDropExtender.OnDropDelegate.IsBound() && AssetViewDragAndDropExtender.OnDropDelegate.Execute(FAssetViewDragAndDropExtender::FPayload(DragDropOp, SourcesData.VirtualPaths, SourcesData.Collections)))
			{
				return FReply::Handled();
			}
		}
	}

	if (SourcesData.HasVirtualPaths())
	{
		UContentBrowserDataSubsystem* ContentBrowserData = IContentBrowserDataModule::Get().GetSubsystem();

		const FContentBrowserItem DropFolderItem = ContentBrowserData->GetItemAtPath(SourcesData.VirtualPaths[0], EContentBrowserItemTypeFilter::IncludeFolders);
		if (DropFolderItem.IsValid() && DragDropHandler::HandleDragDropOnItem(DropFolderItem, DragDropEvent, AsShared()))
		{
			return FReply::Handled();
		}
	}
	else if (HasSingleCollectionSource())
	{
		TArray<FSoftObjectPath> NewCollectionItems;

		if (TSharedPtr<FContentBrowserDataDragDropOp> ContentDragDropOp = DragDropEvent.GetOperationAs<FContentBrowserDataDragDropOp>())
		{
			for (const FContentBrowserItem& DraggedItem : ContentDragDropOp->GetDraggedFiles())
			{
				FSoftObjectPath CollectionItemId;
				if (DraggedItem.TryGetCollectionId(CollectionItemId))
				{
					NewCollectionItems.Add(CollectionItemId);
				}
			}
		}
		else
		{
			const TArray<FAssetData> AssetDatas = AssetUtil::ExtractAssetDataFromDrag(DragDropEvent);
			Algo::Transform(AssetDatas, NewCollectionItems, &FAssetData::GetSoftObjectPath);
		}

		if (NewCollectionItems.Num() > 0)
		{
			FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();
			const FCollectionNameType& Collection = SourcesData.Collections[0];
			CollectionManagerModule.Get().AddToCollection(Collection.Name, Collection.Type, NewCollectionItems);

			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

FReply SAssetView::OnKeyChar( const FGeometry& MyGeometry,const FCharacterEvent& InCharacterEvent )
{
	const bool bIsControlOrCommandDown = InCharacterEvent.IsControlDown() || InCharacterEvent.IsCommandDown();
	
	const bool bTestOnly = false;
	if(HandleQuickJumpKeyDown(InCharacterEvent.GetCharacter(), bIsControlOrCommandDown, InCharacterEvent.IsAltDown(), bTestOnly).IsEventHandled())
	{
		return FReply::Handled();
	}

	// If the user pressed a key we couldn't handle, reset the quick-jump search
	ResetQuickJump();

	return FReply::Unhandled();
}

static bool IsValidObjectPath(const FString& Path, FString& OutObjectClassName, FString& OutObjectPath, FString& OutPackageName)
{
	if (FPackageName::ParseExportTextPath(Path, &OutObjectClassName, &OutObjectPath))
	{
		if (UClass* ObjectClass = UClass::TryFindTypeSlow<UClass>(OutObjectClassName, EFindFirstObjectOptions::ExactClass))
		{
			OutPackageName = FPackageName::ObjectPathToPackageName(OutObjectPath);
			if (FPackageName::IsValidLongPackageName(OutPackageName))
			{
				return true;
			}
		}
	}

	return false;
}

static bool ContainsT3D(const FString& ClipboardText)
{
	return (ClipboardText.StartsWith(TEXT("Begin Object")) && ClipboardText.EndsWith(TEXT("End Object")))
		|| (ClipboardText.StartsWith(TEXT("Begin Map")) && ClipboardText.EndsWith(TEXT("End Map")));
}

FReply SAssetView::OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
{
	const bool bIsControlOrCommandDown = InKeyEvent.IsControlDown() || InKeyEvent.IsCommandDown();
	
	if (bIsControlOrCommandDown && InKeyEvent.GetCharacter() == 'V' && IsAssetPathSelected())
	{
		FString AssetPaths;
		TArray<FString> AssetPathsSplit;

		// Get the copied asset paths
		FPlatformApplicationMisc::ClipboardPaste(AssetPaths);

		// Make sure the clipboard does not contain T3D
		AssetPaths.TrimEndInline();
		if (!ContainsT3D(AssetPaths))
		{
			AssetPaths.ParseIntoArrayLines(AssetPathsSplit);

			// Get assets and copy them
			TArray<UObject*> AssetsToCopy;
			FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
			for (const FString& AssetPath : AssetPathsSplit)
			{
				// Validate string
				FString ObjectClassName;
				FString ObjectPath;
				FString PackageName;
				if (IsValidObjectPath(AssetPath, ObjectClassName, ObjectPath, PackageName))
				{
					// Only duplicate the objects of the supported classes.
					if (AssetToolsModule.Get().GetAssetClassPathPermissionList(EAssetClassAction::ViewAsset)->PassesStartsWithFilter(ObjectClassName))
					{
						FLinkerInstancingContext InstancingContext({ ULevel::LoadAllExternalObjectsTag });
						UObject* ObjectToCopy = LoadObject<UObject>(nullptr, *ObjectPath, nullptr, LOAD_None, nullptr, &InstancingContext);
						if (ObjectToCopy && !ObjectToCopy->IsA(UClass::StaticClass()))
						{
							AssetsToCopy.Add(ObjectToCopy);
						}
					}
				}
			}
			
			if (AssetsToCopy.Num())
			{
				UContentBrowserDataSubsystem* ContentBrowserData = IContentBrowserDataModule::Get().GetSubsystem();
				if (ensure(ContentBrowserData))
				{
					for (const FName& SelectedVirtualPath : SourcesData.VirtualPaths)
					{
						const FContentBrowserItem SelectedItem = ContentBrowserData->GetItemAtPath(SelectedVirtualPath, EContentBrowserItemTypeFilter::IncludeFolders);
						if (SelectedItem.IsValid())
						{
							FName PackagePath;
							if (SelectedItem.Legacy_TryGetPackagePath(PackagePath))
							{
								ContentBrowserUtils::CopyAssets(AssetsToCopy, PackagePath.ToString());
								break;
							}
						}
					}
				}
			}
		}

		return FReply::Handled();
	}
	// Swallow the key-presses used by the quick-jump in OnKeyChar to avoid other things (such as the viewport commands) getting them instead
	// eg) Pressing "W" without this would set the viewport to "translate" mode
	else if(HandleQuickJumpKeyDown((TCHAR)InKeyEvent.GetCharacter(), bIsControlOrCommandDown, InKeyEvent.IsAltDown(), /*bTestOnly*/true).IsEventHandled())
	{
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FReply SAssetView::OnMouseWheel( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	// Make sure to not change the thumbnail scaling when we're in Columns view since thumbnail scaling isn't applicable there.
	if( MouseEvent.IsControlDown() && IsThumbnailScalingAllowed() )
	{
		// Step up/down a level depending on the scroll wheel direction.
		// Clamp value to enum min/max before updating.
		const int32 Delta = MouseEvent.GetWheelDelta() > 0 ? 1 : -1;
		const EThumbnailSize DesiredThumbnailSize = (EThumbnailSize)FMath::Clamp<int32>((int32)ThumbnailSize + Delta, 0, (int32)EThumbnailSize::MAX - 1);
		if ( DesiredThumbnailSize != ThumbnailSize )
		{
			OnThumbnailSizeChanged(DesiredThumbnailSize);
		}		
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void SAssetView::OnFocusChanging( const FWeakWidgetPath& PreviousFocusPath, const FWidgetPath& NewWidgetPath, const FFocusEvent& InFocusEvent)
{
	ResetQuickJump();
}

TSharedRef<SAssetTileView> SAssetView::CreateTileView()
{
	return SNew(SAssetTileView)
		.SelectionMode( SelectionMode )
		.ListItemsSource(&FilteredAssetItems)
		.OnGenerateTile(this, &SAssetView::MakeTileViewWidget)
		.OnItemScrolledIntoView(this, &SAssetView::ItemScrolledIntoView)
		.OnContextMenuOpening(this, &SAssetView::OnGetContextMenuContent)
		.OnMouseButtonDoubleClick(this, &SAssetView::OnListMouseButtonDoubleClick)
		.OnSelectionChanged(this, &SAssetView::AssetSelectionChanged)
		.ItemHeight(this, &SAssetView::GetTileViewItemHeight)
		.ItemWidth(this, &SAssetView::GetTileViewItemWidth)
		.ScrollbarVisibility(bForceHideScrollbar ? EVisibility::Collapsed : EVisibility::Visible);
}

TSharedRef<SAssetListView> SAssetView::CreateListView()
{
	TSharedRef<SLayeredImage> RevisionControlColumnIcon = SNew(SLayeredImage)
			.ColorAndOpacity(FSlateColor::UseForeground())
			.Image(FRevisionControlStyleManager::Get().GetBrush("RevisionControl.Icon"));

	RevisionControlColumnIcon->AddLayer(TAttribute<const FSlateBrush*>::CreateSP(this, &SAssetView::GetRevisionControlColumnIconBadge));

	return SNew(SAssetListView)
		.SelectionMode( SelectionMode )
		.ListItemsSource(&FilteredAssetItems)
		.OnGenerateRow(this, &SAssetView::MakeListViewWidget)
		.OnItemScrolledIntoView(this, &SAssetView::ItemScrolledIntoView)
		.OnContextMenuOpening(this, &SAssetView::OnGetContextMenuContent)
		.OnMouseButtonDoubleClick(this, &SAssetView::OnListMouseButtonDoubleClick)
		.OnSelectionChanged(this, &SAssetView::AssetSelectionChanged)
		.ItemHeight(this, &SAssetView::GetListViewItemHeight)
		.ScrollbarVisibility(bForceHideScrollbar ? EVisibility::Collapsed : EVisibility::Visible)
		.HeaderRow
		(
			SNew(SHeaderRow)
			.ResizeMode(ESplitterResizeMode::FixedSize)

			// Revision Control column, currently doesn't support sorting
			+ SHeaderRow::Column(SortManager.RevisionControlColumnId)
			.FixedWidth(30.f)
			.HAlignHeader(HAlign_Center)
			.VAlignHeader(VAlign_Center)
			.HAlignCell(HAlign_Center)
			.VAlignCell(VAlign_Center)
			.DefaultLabel( LOCTEXT("Column_RC", "Revision Control") )
			[
				RevisionControlColumnIcon
			]
			
			+ SHeaderRow::Column(SortManager.NameColumnId)
			.FillWidth(300)
			.SortMode( TAttribute< EColumnSortMode::Type >::Create( TAttribute< EColumnSortMode::Type >::FGetter::CreateSP( this, &SAssetView::GetColumnSortMode, SortManager.NameColumnId ) ) )
			.SortPriority(TAttribute< EColumnSortPriority::Type >::Create(TAttribute< EColumnSortPriority::Type >::FGetter::CreateSP(this, &SAssetView::GetColumnSortPriority, SortManager.NameColumnId)))
			.OnSort( FOnSortModeChanged::CreateSP( this, &SAssetView::OnSortColumnHeader ) )
			.DefaultLabel( LOCTEXT("Column_Name", "Name") )
			.ShouldGenerateWidget(true) // Can't hide name column, so at least one column is visible
		);
}

TSharedRef<SAssetColumnView> SAssetView::CreateColumnView()
{
	TSharedRef<SLayeredImage> RevisionControlColumnIcon = SNew(SLayeredImage)
				.ColorAndOpacity(FSlateColor::UseForeground())
				.Image(FRevisionControlStyleManager::Get().GetBrush("RevisionControl.Icon"));

	RevisionControlColumnIcon->AddLayer(TAttribute<const FSlateBrush*>::CreateSP(this, &SAssetView::GetRevisionControlColumnIconBadge));
	
	TSharedPtr<SAssetColumnView> NewColumnView = SNew(SAssetColumnView)
		.SelectionMode( SelectionMode )
		.ListItemsSource(&FilteredAssetItems)
		.OnGenerateRow(this, &SAssetView::MakeColumnViewWidget)
		.OnItemScrolledIntoView(this, &SAssetView::ItemScrolledIntoView)
		.OnContextMenuOpening(this, &SAssetView::OnGetContextMenuContent)
		.OnMouseButtonDoubleClick(this, &SAssetView::OnListMouseButtonDoubleClick)
		.OnSelectionChanged(this, &SAssetView::AssetSelectionChanged)
		.Visibility(this, &SAssetView::GetColumnViewVisibility)
		.ScrollbarVisibility(bForceHideScrollbar ? EVisibility::Collapsed : EVisibility::Visible)
		.HeaderRow
		(
			SNew(SHeaderRow)
			.ResizeMode(ESplitterResizeMode::Fill)
			.CanSelectGeneratedColumn(true)
			.OnHiddenColumnsListChanged(this, &SAssetView::OnHiddenColumnsChanged)

			// Revision Control column, currently doesn't support sorting
			+ SHeaderRow::Column(SortManager.RevisionControlColumnId)
			.FixedWidth(30.f)
			.HAlignHeader(HAlign_Center)
			.VAlignHeader(VAlign_Center)
			.HAlignCell(HAlign_Center)
			.VAlignCell(VAlign_Center)
			.DefaultLabel( LOCTEXT("Column_RC", "Revision Control") )
			[
				RevisionControlColumnIcon
			]
			
			+ SHeaderRow::Column(SortManager.NameColumnId)
			.FillWidth(300)
			.SortMode( TAttribute< EColumnSortMode::Type >::Create( TAttribute< EColumnSortMode::Type >::FGetter::CreateSP( this, &SAssetView::GetColumnSortMode, SortManager.NameColumnId ) ) )
			.SortPriority(TAttribute< EColumnSortPriority::Type >::Create(TAttribute< EColumnSortPriority::Type >::FGetter::CreateSP(this, &SAssetView::GetColumnSortPriority, SortManager.NameColumnId)))
			.OnSort( FOnSortModeChanged::CreateSP( this, &SAssetView::OnSortColumnHeader ) )
			.DefaultLabel( LOCTEXT("Column_Name", "Name") )
			.ShouldGenerateWidget(true) // Can't hide name column, so at least one column is visible
		);

	{
		const bool bIsColumnVisible = !HiddenColumnNames.Contains(SortManager.RevisionControlColumnId.ToString());
        NewColumnView->GetHeaderRow()->SetShowGeneratedColumn(SortManager.RevisionControlColumnId, bIsColumnVisible);
	}

	NewColumnView->GetHeaderRow()->SetOnGetMaxRowSizeForColumn(FOnGetMaxRowSizeForColumn::CreateRaw(NewColumnView.Get(), &SAssetColumnView::GetMaxRowSizeForColumn));

	if(bShowTypeInColumnView)
	{
		NewColumnView->GetHeaderRow()->AddColumn(
				SHeaderRow::Column(SortManager.ClassColumnId)
				.FillWidth(160)
				.SortMode(TAttribute< EColumnSortMode::Type >::Create(TAttribute< EColumnSortMode::Type >::FGetter::CreateSP(this, &SAssetView::GetColumnSortMode, SortManager.ClassColumnId)))
				.SortPriority(TAttribute< EColumnSortPriority::Type >::Create(TAttribute< EColumnSortPriority::Type >::FGetter::CreateSP(this, &SAssetView::GetColumnSortPriority, SortManager.ClassColumnId)))
				.OnSort(FOnSortModeChanged::CreateSP(this, &SAssetView::OnSortColumnHeader))
				.DefaultLabel(LOCTEXT("Column_Class", "Type"))
			);

		const bool bIsColumnVisible = !HiddenColumnNames.Contains(SortManager.ClassColumnId.ToString());
		
		NewColumnView->GetHeaderRow()->SetShowGeneratedColumn(SortManager.ClassColumnId, bIsColumnVisible);
	}

	if (bShowPathInColumnView)
	{
		NewColumnView->GetHeaderRow()->AddColumn(
				SHeaderRow::Column(SortManager.PathColumnId)
				.FillWidth(160)
				.SortMode(TAttribute< EColumnSortMode::Type >::Create(TAttribute< EColumnSortMode::Type >::FGetter::CreateSP(this, &SAssetView::GetColumnSortMode, SortManager.PathColumnId)))
				.SortPriority(TAttribute< EColumnSortPriority::Type >::Create(TAttribute< EColumnSortPriority::Type >::FGetter::CreateSP(this, &SAssetView::GetColumnSortPriority, SortManager.PathColumnId)))
				.OnSort(FOnSortModeChanged::CreateSP(this, &SAssetView::OnSortColumnHeader))
				.DefaultLabel(LOCTEXT("Column_Path", "Path"))
			);

		const bool bIsColumnVisible = !HiddenColumnNames.Contains(SortManager.PathColumnId.ToString());
		
		NewColumnView->GetHeaderRow()->SetShowGeneratedColumn(SortManager.PathColumnId, bIsColumnVisible);
	}

	return NewColumnView.ToSharedRef();
}

const FSlateBrush* SAssetView::GetRevisionControlColumnIconBadge() const
{
	if (ISourceControlModule::Get().IsEnabled())
	{
		return FRevisionControlStyleManager::Get().GetBrush("RevisionControl.Icon.ConnectedBadge");
	}
	else
	{
		return nullptr;
	}
}


bool SAssetView::IsValidSearchToken(const FString& Token) const
{
	if ( Token.Len() == 0 )
	{
		return false;
	}

	// A token may not be only apostrophe only, or it will match every asset because the text filter compares against the pattern Class'ObjectPath'
	if ( Token.Len() == 1 && Token[0] == '\'' )
	{
		return false;
	}

	return true;
}

FContentBrowserDataFilter SAssetView::CreateBackendDataFilter(bool bInvalidateCache) const
{
	// Assemble the filter using the current sources
	// Force recursion when the user is searching
	const bool bHasCollections = SourcesData.HasCollections();
	const bool bRecurse = ShouldFilterRecursively();
	const bool bUsingFolders = IsShowingFolders() && !bRecurse;

	// Check whether any legacy delegates are bound (the Content Browser doesn't use these, only pickers do)
	// These limit the view to things that might use FAssetData
	const bool bHasLegacyDelegateBindings 
		=  OnIsAssetValidForCustomToolTip.IsBound()
		|| OnGetCustomAssetToolTip.IsBound()
		|| OnVisualizeAssetToolTip.IsBound()
		|| OnAssetToolTipClosing.IsBound()
		|| OnShouldFilterAsset.IsBound();

	FContentBrowserDataFilter DataFilter;
	DataFilter.bRecursivePaths = bRecurse || !bUsingFolders || bHasCollections;

	DataFilter.ItemTypeFilter = EContentBrowserItemTypeFilter::IncludeFiles
		| ((bUsingFolders && !bHasCollections) ? EContentBrowserItemTypeFilter::IncludeFolders : EContentBrowserItemTypeFilter::IncludeNone);

	DataFilter.ItemCategoryFilter = bHasLegacyDelegateBindings ? EContentBrowserItemCategoryFilter::IncludeAssets : InitialCategoryFilter;
	if (IsShowingCppContent())
	{
		DataFilter.ItemCategoryFilter |= EContentBrowserItemCategoryFilter::IncludeClasses;
	}
	else
	{
		DataFilter.ItemCategoryFilter &= ~EContentBrowserItemCategoryFilter::IncludeClasses;
	}
	DataFilter.ItemCategoryFilter |= EContentBrowserItemCategoryFilter::IncludeCollections;

	DataFilter.ItemAttributeFilter = EContentBrowserItemAttributeFilter::IncludeProject
		| (IsShowingEngineContent() ? EContentBrowserItemAttributeFilter::IncludeEngine : EContentBrowserItemAttributeFilter::IncludeNone)
		| (IsShowingPluginContent() ? EContentBrowserItemAttributeFilter::IncludePlugins : EContentBrowserItemAttributeFilter::IncludeNone)
		| (IsShowingDevelopersContent() ? EContentBrowserItemAttributeFilter::IncludeDeveloper : EContentBrowserItemAttributeFilter::IncludeNone)
		| (IsShowingLocalizedContent() ? EContentBrowserItemAttributeFilter::IncludeLocalized : EContentBrowserItemAttributeFilter::IncludeNone);

	TSharedPtr<FPathPermissionList> CombinedFolderPermissionList = ContentBrowserUtils::GetCombinedFolderPermissionList(FolderPermissionList, IsShowingReadOnlyFolders() ? nullptr : WritableFolderPermissionList);

	if (bShowDisallowedAssetClassAsUnsupportedItems && AssetClassPermissionList && AssetClassPermissionList->HasFiltering())
	{
		// The unsupported item will created as an unsupported asset item instead of normal asset item for the writable folders
		FContentBrowserDataUnsupportedClassFilter& UnsupportedClassFilter = DataFilter.ExtraFilters.FindOrAddFilter<FContentBrowserDataUnsupportedClassFilter>();
		UnsupportedClassFilter.ClassPermissionList = AssetClassPermissionList;
		UnsupportedClassFilter.FolderPermissionList = WritableFolderPermissionList;
	}

	ContentBrowserUtils::AppendAssetFilterToContentBrowserFilter(BackendFilter, AssetClassPermissionList, CombinedFolderPermissionList, DataFilter);

	if (bHasCollections && !SourcesData.IsDynamicCollection())
	{
		FContentBrowserDataCollectionFilter& CollectionFilter = DataFilter.ExtraFilters.FindOrAddFilter<FContentBrowserDataCollectionFilter>();
		CollectionFilter.SelectedCollections = SourcesData.Collections;
		CollectionFilter.bIncludeChildCollections = !bUsingFolders;
	}

	if (OnGetCustomSourceAssets.IsBound())
	{
		FContentBrowserDataLegacyFilter& LegacyFilter = DataFilter.ExtraFilters.FindOrAddFilter<FContentBrowserDataLegacyFilter>();
		LegacyFilter.OnGetCustomSourceAssets = OnGetCustomSourceAssets;
	}

	DataFilter.CacheID = FilterCacheID;

	if (bInvalidateCache)
	{
		if (SourcesData.IsIncludingVirtualPaths())
		{
			static const FName RootPath = "/";
			const TArrayView<const FName> DataSourcePaths = SourcesData.HasVirtualPaths() ? MakeArrayView(SourcesData.VirtualPaths) : MakeArrayView(&RootPath, 1);
			FilterCacheID.RemoveUnusedCachedData(DataSourcePaths, DataFilter);
		}
		else
		{
			// Not sure what is the right thing to do here so clear the cache
			FilterCacheID.ClearCachedData();
		}
	}

	return DataFilter;
}

void SAssetView::RefreshSourceItems()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(SAssetView::RefreshSourceItems);
	const double RefreshSourceItemsStartTime = FPlatformTime::Seconds();
	
	OnInterruptFiltering();

	FilterSessionCorrelationGuid = FGuid::NewGuid();
	UE::Telemetry::ContentBrowser::FBackendFilterTelemetry Telemetry(ViewCorrelationGuid, FilterSessionCorrelationGuid);
	FilteredAssetItems.Reset();
	FilteredAssetItemTypeCounts.Reset();
	VisibleItems.Reset();
	RelevantThumbnails.Reset();

	TMap<FContentBrowserItemKey, TSharedPtr<FAssetViewItem>> PreviousAvailableBackendItems = MoveTemp(AvailableBackendItems);
	AvailableBackendItems.Reset();
	ItemsPendingPriorityFilter.Reset();
	ItemsPendingFrontendFilter.Reset();
	{
		UContentBrowserDataSubsystem* ContentBrowserData = IContentBrowserDataModule::Get().GetSubsystem();

		auto AddNewItem = [this, &PreviousAvailableBackendItems](FContentBrowserItemData&& InItemData)
		{
			const FContentBrowserItemKey ItemDataKey(InItemData);
			const uint32 ItemDataKeyHash = GetTypeHash(ItemDataKey);

			TSharedPtr<FAssetViewItem>& NewItem = AvailableBackendItems.FindOrAddByHash(ItemDataKeyHash, ItemDataKey);
			if (!NewItem && InItemData.IsFile())
			{
				// Re-use the old view item where possible to avoid list churn when our backend view already included the item
				if (TSharedPtr<FAssetViewItem>* PreviousItem = PreviousAvailableBackendItems.FindByHash(ItemDataKeyHash, ItemDataKey))
				{
					NewItem = *PreviousItem;
					NewItem->ClearCachedCustomColumns();
				}
			}
			if (NewItem)
			{
				NewItem->AppendItemData(InItemData);
				NewItem->CacheCustomColumns(CustomColumns, true, true, false /*bUpdateExisting*/);
			}
			else
			{
				NewItem = MakeShared<FAssetViewItem>(MoveTemp(InItemData));
			}

			return true;
		};

		if (SourcesData.OnEnumerateCustomSourceItemDatas.IsBound())
		{
			Telemetry.bHasCustomItemSources = true;
			SourcesData.OnEnumerateCustomSourceItemDatas.Execute(AddNewItem);
		}

		FContentBrowserDataFilter DataFilter; // Must live long enough to provide telemetry
		if (SourcesData.IsIncludingVirtualPaths() || SourcesData.HasCollections()) 
		{
			const bool bInvalidateFilterCache = true;
			DataFilter = CreateBackendDataFilter(bInvalidateFilterCache);
			Telemetry.DataFilter = &DataFilter;

			bWereItemsRecursivelyFiltered = DataFilter.bRecursivePaths;

			if (SourcesData.HasCollections() && EnumHasAnyFlags(DataFilter.ItemCategoryFilter, EContentBrowserItemCategoryFilter::IncludeCollections))
			{
				// If we are showing collections then we may need to add dummy folder items for the child collections
				// Note: We don't check the IncludeFolders flag here, as that is forced to false when collections are selected,
				// instead we check the state of bIncludeChildCollections which will be false when we want to show collection folders
				const FContentBrowserDataCollectionFilter* CollectionFilter = DataFilter.ExtraFilters.FindFilter<FContentBrowserDataCollectionFilter>();
				if (CollectionFilter && !CollectionFilter->bIncludeChildCollections)
				{
					FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();
				
					TArray<FCollectionNameType> ChildCollections;
					for(const FCollectionNameType& Collection : SourcesData.Collections)
					{
						ChildCollections.Reset();
						CollectionManagerModule.Get().GetChildCollections(Collection.Name, Collection.Type, ChildCollections);

						for (const FCollectionNameType& ChildCollection : ChildCollections)
						{
							// Use "Collections" as the root of the path to avoid this being confused with other view folders - see ContentBrowserUtils::IsCollectionPath
							FContentBrowserItemData FolderItemData(
								nullptr, 
								EContentBrowserItemFlags::Type_Folder | EContentBrowserItemFlags::Category_Collection, 
								*FString::Printf(TEXT("/Collections/%s/%s"), ECollectionShareType::ToString(ChildCollection.Type), *ChildCollection.Name.ToString()), 
								ChildCollection.Name, 
								FText::FromName(ChildCollection.Name), 
								nullptr
								);

							const FContentBrowserItemKey FolderItemDataKey(FolderItemData);
							AvailableBackendItems.Add(FolderItemDataKey, MakeShared<FAssetViewItem>(MoveTemp(FolderItemData)));
						}
					}
				}
			}

			if (SourcesData.IsIncludingVirtualPaths())
			{
				static const FName RootPath = "/";
				const TArrayView<const FName> DataSourcePaths = SourcesData.HasVirtualPaths() ? MakeArrayView(SourcesData.VirtualPaths) : MakeArrayView(&RootPath, 1);
				for (const FName& DataSourcePath : DataSourcePaths)
				{
					// Ensure paths do not contain trailing slash
					ensure(DataSourcePath == RootPath || !FStringView(FNameBuilder(DataSourcePath)).EndsWith(TEXT('/')));
					ContentBrowserData->EnumerateItemsUnderPath(DataSourcePath, DataFilter, AddNewItem);
				}
			}
		}

		Telemetry.NumBackendItems = AvailableBackendItems.Num();
		Telemetry.RefreshSourceItemsDurationSeconds = FPlatformTime::Seconds() - RefreshSourceItemsStartTime;
		FTelemetryRouter::Get().ProvideTelemetry(Telemetry);
	}

	UE_LOG(LogContentBrowser, VeryVerbose, TEXT("AssetView - RefreshSourceItems completed in %0.4f seconds"), FPlatformTime::Seconds() - RefreshSourceItemsStartTime);
}

bool SAssetView::IsFilteringRecursively() const
{
	if (!bFilterRecursivelyWithBackendFilter)
	{
		return false;
	}

	// In some cases we want to not filter recursively even if we have a backend filter (e.g. the open level window)
	// Most of the time, bFilterRecursivelyWithBackendFilter is true
	if (const FContentBrowserInstanceConfig* EditorConfig = GetContentBrowserConfig())
	{
		return EditorConfig->bFilterRecursively;
	}

	return GetDefault<UContentBrowserSettings>()->FilterRecursively;
}

bool SAssetView::IsToggleFilteringRecursivelyAllowed() const
{
	return bFilterRecursivelyWithBackendFilter;
}

void SAssetView::ToggleFilteringRecursively()
{
	check(IsToggleFilteringRecursivelyAllowed());

	bool bNewState = !GetDefault<UContentBrowserSettings>()->FilterRecursively;
	
	if (FContentBrowserInstanceConfig* EditorConfig = GetContentBrowserConfig())
	{
		bNewState = !EditorConfig->bFilterRecursively;

		EditorConfig->bFilterRecursively = bNewState;
		UContentBrowserConfig::Get()->SaveEditorConfig();
	}

	GetMutableDefault<UContentBrowserSettings>()->FilterRecursively = bNewState;
	GetMutableDefault<UContentBrowserSettings>()->PostEditChange();
}

bool SAssetView::ShouldFilterRecursively() const
{
	// Quick check for conditions which force recursive filtering
	if (bUserSearching)
	{
		return true;
	}

	if (IsFilteringRecursively() && !BackendFilter.IsEmpty() )
	{
		return true;
	}

	// Otherwise, check if there are any non-inverse frontend filters selected
	if (FrontendFilters.IsValid())
	{
		for (int32 FilterIndex = 0; FilterIndex < FrontendFilters->Num(); ++FilterIndex)
		{
			const auto* Filter = static_cast<FFrontendFilter*>(FrontendFilters->GetFilterAtIndex(FilterIndex).Get());
			if (Filter)
			{
				if (!Filter->IsInverseFilter())
				{
					return true;
				}
			}
		}
	}

	// No sources - view will show everything
	if (SourcesData.IsEmpty())
	{
		return true;
	}

	// No filters, do not override folder view with recursive filtering
	return false;
}

void SAssetView::RefreshFilteredItems()
{
	const double RefreshFilteredItemsStartTime = FPlatformTime::Seconds();

	OnInterruptFiltering();
	
	ItemsPendingFrontendFilter.Reset();
	FilteredAssetItems.Reset();
	FilteredAssetItemTypeCounts.Reset();
	RelevantThumbnails.Reset();

	AmortizeStartTime = 0;
	InitialNumAmortizedTasks = 0;

	LastSortTime = 0;
	bPendingSortFilteredItems = true;

	ItemsPendingFrontendFilter.Reserve(AvailableBackendItems.Num());
	for (const auto& AvailableBackendItemPair : AvailableBackendItems)
	{
		ItemsPendingFrontendFilter.Add(AvailableBackendItemPair.Value);
	}

	// Let the frontend filters know the currently used asset filter in case it is necessary to conditionally filter based on path or class filters
	if (IsFrontendFilterActive() && FrontendFilters.IsValid())
	{
		static const FName RootPath = "/";
		const TArrayView<const FName> DataSourcePaths = SourcesData.HasVirtualPaths() ? MakeArrayView(SourcesData.VirtualPaths) : MakeArrayView(&RootPath, 1);

		const bool bInvalidateFilterCache = false;
		const FContentBrowserDataFilter DataFilter = CreateBackendDataFilter(bInvalidateFilterCache);

		for (int32 FilterIdx = 0; FilterIdx < FrontendFilters->Num(); ++FilterIdx)
		{
			// There are only FFrontendFilters in this collection
			const TSharedPtr<FFrontendFilter>& Filter = StaticCastSharedPtr<FFrontendFilter>(FrontendFilters->GetFilterAtIndex(FilterIdx));
			if (Filter.IsValid())
			{
				Filter->SetCurrentFilter(DataSourcePaths, DataFilter);
			}
		}
	}

	UE_LOG(LogContentBrowser, VeryVerbose, TEXT("AssetView - RefreshFilteredItems completed in %0.4f seconds"), FPlatformTime::Seconds() - RefreshFilteredItemsStartTime);
}

FContentBrowserInstanceConfig* SAssetView::GetContentBrowserConfig() const
{
	if (TSharedPtr<SContentBrowser> ContentBrowser = OwningContentBrowser.Pin())
	{
		if (UContentBrowserConfig* EditorConfig = UContentBrowserConfig::Get())
		{
			return UContentBrowserConfig::Get()->Instances.Find(ContentBrowser->GetInstanceName());
		}
	}
	return nullptr;
}

FAssetViewInstanceConfig* SAssetView::GetAssetViewConfig() const
{
	if (TSharedPtr<SContentBrowser> ContentBrowser = OwningContentBrowser.Pin())
	{
		const FName InstanceName = ContentBrowser->GetInstanceName();
		if (!InstanceName.IsNone())
		{
			if (UAssetViewConfig* Config = UAssetViewConfig::Get())
			{
				return &Config->GetInstanceConfig(InstanceName);
			}
		}
	}

	return nullptr;
}

void SAssetView::ToggleShowAllFolder()
{
	const bool bNewValue = !IsShowingAllFolder();
	GetMutableDefault<UContentBrowserSettings>()->bShowAllFolder = bNewValue;
	GetMutableDefault<UContentBrowserSettings>()->PostEditChange();
}

bool SAssetView::IsShowingAllFolder() const
{
	return GetDefault<UContentBrowserSettings>()->bShowAllFolder;
}

void SAssetView::ToggleOrganizeFolders()
{
	const bool bNewValue = !IsOrganizingFolders();
	GetMutableDefault<UContentBrowserSettings>()->bOrganizeFolders = bNewValue;
	GetMutableDefault<UContentBrowserSettings>()->PostEditChange();
}

bool SAssetView::IsOrganizingFolders() const
{
	return GetDefault<UContentBrowserSettings>()->bOrganizeFolders;
}

void SAssetView::SetMajorityAssetType(FName NewMajorityAssetType)
{
	if (CurrentViewType != EAssetViewType::Column)
	{
		return;
	}

	auto IsFixedColumn = [this](FName InColumnId)
	{
		const bool bIsFixedNameColumn = InColumnId == SortManager.NameColumnId;
		const bool bIsFixedRevisionControlColumn = InColumnId == SortManager.RevisionControlColumnId;
		const bool bIsFixedClassColumn = bShowTypeInColumnView && InColumnId == SortManager.ClassColumnId;
		const bool bIsFixedPathColumn = bShowPathInColumnView && InColumnId == SortManager.PathColumnId;
		return bIsFixedNameColumn || bIsFixedRevisionControlColumn || bIsFixedClassColumn || bIsFixedPathColumn;
	};


	FContentBrowserModule& ContentBrowserModule = FModuleManager::GetModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	bool bHasDynamicColumns = ContentBrowserModule.IsDynamicTagAssetClass(NewMajorityAssetType);

	if ( NewMajorityAssetType != MajorityAssetType || bHasDynamicColumns)
	{
		UE_LOG(LogContentBrowser, Verbose, TEXT("The majority of assets in the view are of type: %s"), *NewMajorityAssetType.ToString());

		MajorityAssetType = NewMajorityAssetType;

		TArray<FName> AddedColumns;

		// Since the asset type has changed, remove all columns except name and class
		const TIndirectArray<SHeaderRow::FColumn>& Columns = ColumnView->GetHeaderRow()->GetColumns();

		for ( int32 ColumnIdx = Columns.Num() - 1; ColumnIdx >= 0; --ColumnIdx )
		{
			const FName ColumnId = Columns[ColumnIdx].ColumnId;

			if ( ColumnId != NAME_None && !IsFixedColumn(ColumnId) )
			{
				ColumnView->GetHeaderRow()->RemoveColumn(ColumnId);
			}
		}

		// Keep track of the current column name to see if we need to change it now that columns are being removed
		// Name, Class, and Path are always relevant
		struct FSortOrder
		{
			bool bSortRelevant;
			FName SortColumn;
			FSortOrder(bool bInSortRelevant, const FName& InSortColumn) : bSortRelevant(bInSortRelevant), SortColumn(InSortColumn) {}
		};
		TArray<FSortOrder> CurrentSortOrder;
		for (int32 PriorityIdx = 0; PriorityIdx < EColumnSortPriority::Max; PriorityIdx++)
		{
			const FName SortColumn = SortManager.GetSortColumnId(static_cast<EColumnSortPriority::Type>(PriorityIdx));
			if (SortColumn != NAME_None)
			{
				const bool bSortRelevant = SortColumn == FAssetViewSortManager::NameColumnId
					|| SortColumn == FAssetViewSortManager::ClassColumnId
					|| SortColumn == FAssetViewSortManager::PathColumnId;
				CurrentSortOrder.Add(FSortOrder(bSortRelevant, SortColumn));
			}
		}

		// Add custom columns
		for (const FAssetViewCustomColumn& Column : CustomColumns)
		{
			FName TagName = Column.ColumnName;

			if (AddedColumns.Contains(TagName))
			{
				continue;
			}
			AddedColumns.Add(TagName);

			ColumnView->GetHeaderRow()->AddColumn(
				SHeaderRow::Column(TagName)
				.SortMode(TAttribute< EColumnSortMode::Type >::Create(TAttribute< EColumnSortMode::Type >::FGetter::CreateSP(this, &SAssetView::GetColumnSortMode, TagName)))
				.SortPriority(TAttribute< EColumnSortPriority::Type >::Create(TAttribute< EColumnSortPriority::Type >::FGetter::CreateSP(this, &SAssetView::GetColumnSortPriority, TagName)))
				.OnSort(FOnSortModeChanged::CreateSP(this, &SAssetView::OnSortColumnHeader))
				.DefaultLabel(Column.DisplayName)
				.DefaultTooltip(Column.TooltipText)
				.FillWidth(180));

			const bool bIsColumnVisible = !HiddenColumnNames.Contains(TagName.ToString());
		
			ColumnView->GetHeaderRow()->SetShowGeneratedColumn(TagName, bIsColumnVisible);

			// If we found a tag the matches the column we are currently sorting on, there will be no need to change the column
			for (int32 SortIdx = 0; SortIdx < CurrentSortOrder.Num(); SortIdx++)
			{
				if (TagName == CurrentSortOrder[SortIdx].SortColumn)
				{
					CurrentSortOrder[SortIdx].bSortRelevant = true;
				}
			}
		}

		// If we have a new majority type, add the new type's columns
		if (NewMajorityAssetType != NAME_None)
		{
			FContentBrowserItemDataAttributeValues UnionedItemAttributes;

			// Find an item of this type so we can extract the relevant attribute data from it
			TSharedPtr<FAssetViewItem> MajorityAssetItem;
			for (const TSharedPtr<FAssetViewItem>& FilteredAssetItem : FilteredAssetItems)
			{
				const FContentBrowserItemDataAttributeValue ClassValue = FilteredAssetItem->GetItem().GetItemAttribute(ContentBrowserItemAttributes::ItemTypeName);
				if (ClassValue.IsValid() && ClassValue.GetValue<FName>() == NewMajorityAssetType)
				{
					if (bHasDynamicColumns)
					{
						const FContentBrowserItemDataAttributeValues ItemAttributes = FilteredAssetItem->GetItem().GetItemAttributes(/*bIncludeMetaData*/true);
						UnionedItemAttributes.Append(ItemAttributes); 
						MajorityAssetItem = FilteredAssetItem;
					}
					else
					{
						MajorityAssetItem = FilteredAssetItem;
						break;
					}
				}
			}

			// Determine the columns by querying the reference item
			if (MajorityAssetItem)
			{
				FContentBrowserItemDataAttributeValues ItemAttributes = bHasDynamicColumns ? UnionedItemAttributes : MajorityAssetItem->GetItem().GetItemAttributes(/*bIncludeMetaData*/true);

				// Add a column for every tag that isn't hidden or using a reserved name
				for (const auto& TagPair : ItemAttributes)
				{
					if (IsFixedColumn(TagPair.Key))
					{
						// Reserved name
						continue;
					}

					if (TagPair.Value.GetMetaData().AttributeType == UObject::FAssetRegistryTag::TT_Hidden)
					{
						// Hidden attribute
						continue;
					}

					if (!OnAssetTagWantsToBeDisplayed.IsBound() || OnAssetTagWantsToBeDisplayed.Execute(NewMajorityAssetType, TagPair.Key))
					{
						if (AddedColumns.Contains(TagPair.Key))
						{
							continue;
						}
						AddedColumns.Add(TagPair.Key);

						ColumnView->GetHeaderRow()->AddColumn(
							SHeaderRow::Column(TagPair.Key)
							.SortMode(TAttribute< EColumnSortMode::Type >::Create(TAttribute< EColumnSortMode::Type >::FGetter::CreateSP(this, &SAssetView::GetColumnSortMode, TagPair.Key)))
							.SortPriority(TAttribute< EColumnSortPriority::Type >::Create(TAttribute< EColumnSortPriority::Type >::FGetter::CreateSP(this, &SAssetView::GetColumnSortPriority, TagPair.Key)))
							.OnSort(FOnSortModeChanged::CreateSP(this, &SAssetView::OnSortColumnHeader))
							.DefaultLabel(TagPair.Value.GetMetaData().DisplayName)
							.DefaultTooltip(TagPair.Value.GetMetaData().TooltipText)
							.FillWidth(180));

						const bool bIsColumnVisible = !HiddenColumnNames.Contains(TagPair.Key.ToString());
		
						ColumnView->GetHeaderRow()->SetShowGeneratedColumn(TagPair.Key, bIsColumnVisible);
						
						// If we found a tag the matches the column we are currently sorting on, there will be no need to change the column
						for (int32 SortIdx = 0; SortIdx < CurrentSortOrder.Num(); SortIdx++)
						{
							if (TagPair.Key == CurrentSortOrder[SortIdx].SortColumn)
							{
								CurrentSortOrder[SortIdx].bSortRelevant = true;
							}
						}
					}
				}
			}
		}

		// Are any of the sort columns irrelevant now, if so remove them from the list
		bool CurrentSortChanged = false;
		for (int32 SortIdx = CurrentSortOrder.Num() - 1; SortIdx >= 0; SortIdx--)
		{
			if (!CurrentSortOrder[SortIdx].bSortRelevant)
			{
				CurrentSortOrder.RemoveAt(SortIdx);
				CurrentSortChanged = true;
			}
		}
		if (CurrentSortOrder.Num() > 0 && CurrentSortChanged)
		{
			// Sort order has changed, update the columns keeping those that are relevant
			int32 PriorityNum = EColumnSortPriority::Primary;
			for (int32 SortIdx = 0; SortIdx < CurrentSortOrder.Num(); SortIdx++)
			{
				check(CurrentSortOrder[SortIdx].bSortRelevant);
				if (!SortManager.SetOrToggleSortColumn(static_cast<EColumnSortPriority::Type>(PriorityNum), CurrentSortOrder[SortIdx].SortColumn))
				{
					// Toggle twice so mode is preserved if this isn't a new column assignation
					SortManager.SetOrToggleSortColumn(static_cast<EColumnSortPriority::Type>(PriorityNum), CurrentSortOrder[SortIdx].SortColumn);
				}				
				bPendingSortFilteredItems = true;
				PriorityNum++;
			}
		}
		else if (CurrentSortOrder.Num() == 0)
		{
			// If the current sort column is no longer relevant, revert to "Name" and resort when convenient
			SortManager.ResetSort();
			bPendingSortFilteredItems = true;
		}
	}
}

void SAssetView::OnAssetsAddedToCollection( const FCollectionNameType& Collection, TConstArrayView<FSoftObjectPath> ObjectPaths )
{
	if ( !SourcesData.Collections.Contains( Collection ) )
	{
		return;
	}

	RequestSlowFullListRefresh();
}

void SAssetView::OnAssetsRemovedFromCollection( const FCollectionNameType& Collection, TConstArrayView<FSoftObjectPath> ObjectPaths )
{
	if ( !SourcesData.Collections.Contains( Collection ) )
	{
		return;
	}

	RequestSlowFullListRefresh();
}

void SAssetView::OnCollectionRenamed( const FCollectionNameType& OriginalCollection, const FCollectionNameType& NewCollection )
{
	int32 FoundIndex = INDEX_NONE;
	if ( SourcesData.Collections.Find( OriginalCollection, FoundIndex ) )
	{
		SourcesData.Collections[ FoundIndex ] = NewCollection;
	}
}

void SAssetView::OnCollectionUpdated( const FCollectionNameType& Collection )
{
	// A collection has changed in some way, so we need to refresh our backend list
	RequestSlowFullListRefresh();
}

void SAssetView::OnFrontendFiltersChanged()
{
	RequestQuickFrontendListRefresh();

	// If we're not operating on recursively filtered data, we need to ensure a full slow
	// refresh is performed.
	if ( ShouldFilterRecursively() && !bWereItemsRecursivelyFiltered )
	{
		RequestSlowFullListRefresh();
	}
}

bool SAssetView::IsFrontendFilterActive() const
{
	return ( FrontendFilters.IsValid() && FrontendFilters->Num() > 0 );
}

bool SAssetView::PassesCurrentFrontendFilter(const FContentBrowserItem& Item) const
{
	return !FrontendFilters.IsValid() || FrontendFilters->PassesAllFilters(Item);
}

void SAssetView::SortList(bool bSyncToSelection)
{
	if ( !IsRenamingAsset() )
	{
		SortManager.SortList(FilteredAssetItems, MajorityAssetType, CustomColumns);

		// Update the thumbnails we were using since the order has changed
		bPendingUpdateThumbnails = true;

		if ( bSyncToSelection )
		{
			// Make sure the selection is in view
			const bool bFocusOnSync = false;
			SyncToSelection(bFocusOnSync);
		}

		RefreshList();
		bPendingSortFilteredItems = false;
		LastSortTime = CurrentTime;
	}
	else
	{
		bPendingSortFilteredItems = true;
	}
}

FLinearColor SAssetView::GetThumbnailHintColorAndOpacity() const
{
	//We update this color in tick instead of here as an optimization
	return ThumbnailHintColorAndOpacity;
}

TSharedRef<SWidget> SAssetView::GetViewButtonContent()
{
	SAssetView::RegisterGetViewButtonMenu();

	// Get all menu extenders for this context menu from the content browser module
	FContentBrowserModule& ContentBrowserModule = FModuleManager::GetModuleChecked<FContentBrowserModule>( TEXT("ContentBrowser") );
	const TArray<FContentBrowserMenuExtender>& MenuExtenderDelegates = ContentBrowserModule.GetAllAssetViewViewMenuExtenders();

	TArray<TSharedPtr<FExtender>> Extenders;
	for (const FContentBrowserMenuExtender& Extender : MenuExtenderDelegates)
	{
		if (Extender.IsBound())
		{
			Extenders.Add(Extender.Execute());
		}
	}

	UContentBrowserAssetViewContextMenuContext* Context = NewObject<UContentBrowserAssetViewContextMenuContext>();
	Context->AssetView = SharedThis(this);
	Context->OwningContentBrowser = OwningContentBrowser;

	TSharedPtr<FExtender> MenuExtender = FExtender::Combine(Extenders);
	FToolMenuContext MenuContext(nullptr, MenuExtender, Context);

	if (OnExtendAssetViewOptionsMenuContext.IsBound())
	{
		OnExtendAssetViewOptionsMenuContext.Execute(MenuContext);
	}

	return UToolMenus::Get()->GenerateWidget("ContentBrowser.AssetViewOptions", MenuContext);
}

void SAssetView::RegisterGetViewButtonMenu()
{
	if (!UToolMenus::Get()->IsMenuRegistered("ContentBrowser.AssetViewOptions"))
	{
		UToolMenu* Menu = UToolMenus::Get()->RegisterMenu("ContentBrowser.AssetViewOptions");
		Menu->bCloseSelfOnly = true;
		Menu->AddDynamicSection("DynamicContent", FNewToolMenuDelegate::CreateLambda([](UToolMenu* InMenu)
		{
			if (UContentBrowserAssetViewContextMenuContext* Context = InMenu->FindContext<UContentBrowserAssetViewContextMenuContext>())
			{
				if (Context->AssetView.IsValid())
				{
					Context->AssetView.Pin()->PopulateViewButtonMenu(InMenu);
				}
			}
		}));
	}
}

void SAssetView::PopulateViewButtonMenu(UToolMenu* Menu)
{
	{
		FToolMenuSection& Section = Menu->AddSection("AssetViewType", LOCTEXT("ViewTypeHeading", "View Type"));
		Section.AddMenuEntry(
			"TileView",
			LOCTEXT("TileViewOption", "Tiles"),
			LOCTEXT("TileViewOptionToolTip", "View assets as tiles in a grid."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP( this, &SAssetView::SetCurrentViewTypeFromMenu, EAssetViewType::Tile ),
				FCanExecuteAction(),
				FIsActionChecked::CreateSP( this, &SAssetView::IsCurrentViewType, EAssetViewType::Tile )
				),
			EUserInterfaceActionType::RadioButton
			);

		Section.AddMenuEntry(
			"ListView",
			LOCTEXT("ListViewOption", "List"),
			LOCTEXT("ListViewOptionToolTip", "View assets in a list with thumbnails."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP( this, &SAssetView::SetCurrentViewTypeFromMenu, EAssetViewType::List ),
				FCanExecuteAction(),
				FIsActionChecked::CreateSP( this, &SAssetView::IsCurrentViewType, EAssetViewType::List )
				),
			EUserInterfaceActionType::RadioButton
			);

		Section.AddMenuEntry(
			"ColumnView",
			LOCTEXT("ColumnViewOption", "Columns"),
			LOCTEXT("ColumnViewOptionToolTip", "View assets in a list with columns of details."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP( this, &SAssetView::SetCurrentViewTypeFromMenu, EAssetViewType::Column ),
				FCanExecuteAction(),
				FIsActionChecked::CreateSP( this, &SAssetView::IsCurrentViewType, EAssetViewType::Column )
				),
			EUserInterfaceActionType::RadioButton
			);
	}

	if(TSharedPtr<SFilterList> FilterBarPinned = FilterBar.Pin())
	{
		FToolMenuSection& Section = Menu->AddSection("FilterBar", LOCTEXT("FilterBarHeading", "Filter Display"));

		Section.AddMenuEntry(
		"VerticalLayout",
		LOCTEXT("FilterListVerticalLayout", "Vertical"),
		LOCTEXT("FilterListVerticalLayoutToolTip", "Swap to a vertical layout for the filter bar"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([FilterBarPinned]()
			{
				if(FilterBarPinned->GetFilterLayout() != EFilterBarLayout::Vertical)
				{
					FilterBarPinned->SetFilterLayout(EFilterBarLayout::Vertical);
				}
			}),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([FilterBarPinned]() { return FilterBarPinned->GetFilterLayout() == EFilterBarLayout::Vertical; })),
		EUserInterfaceActionType::RadioButton
	);

		Section.AddMenuEntry(
			"HorizontalLayout",
			LOCTEXT("FilterListHorizontalLayout", "Horizontal"),
			LOCTEXT("FilterListHorizontalLayoutToolTip", "Swap to a Horizontal layout for the filter bar"),
			FSlateIcon(),
			FUIAction(
			FExecuteAction::CreateLambda([FilterBarPinned]()
				{
					if(FilterBarPinned->GetFilterLayout() != EFilterBarLayout::Horizontal)
					{
						FilterBarPinned->SetFilterLayout(EFilterBarLayout::Horizontal);
					}
				}),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([FilterBarPinned]() { return FilterBarPinned->GetFilterLayout() == EFilterBarLayout::Horizontal; })),
			EUserInterfaceActionType::RadioButton
		);
	}

	{
		FToolMenuSection& Section = Menu->AddSection("View", LOCTEXT("ViewHeading", "View"));

		Section.AddMenuEntry(
			"ShowFolders",
			LOCTEXT("ShowFoldersOption", "Show Folders"),
			LOCTEXT("ShowFoldersOptionToolTip", "Show folders in the view as well as assets?"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP( this, &SAssetView::ToggleShowFolders ),
				FCanExecuteAction::CreateSP( this, &SAssetView::IsToggleShowFoldersAllowed ),
				FIsActionChecked::CreateSP( this, &SAssetView::IsShowingFolders )
			),
			EUserInterfaceActionType::ToggleButton
		);

		Section.AddMenuEntry(
			"ShowEmptyFolders",
			LOCTEXT("ShowEmptyFoldersOption", "Show Empty Folders"),
			LOCTEXT("ShowEmptyFoldersOptionToolTip", "Show empty folders in the view as well as assets?"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &SAssetView::ToggleShowEmptyFolders),
				FCanExecuteAction::CreateSP(this, &SAssetView::IsToggleShowEmptyFoldersAllowed),
				FIsActionChecked::CreateSP(this, &SAssetView::IsShowingEmptyFolders)
			),
			EUserInterfaceActionType::ToggleButton
		);

		Section.AddMenuEntry(
			"ShowFavorite",
			LOCTEXT("ShowFavoriteOptions", "Show Favorites"),
			LOCTEXT("ShowFavoriteOptionToolTip", "Show the favorite folders in the view?"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &SAssetView::ToggleShowFavorites),
				FCanExecuteAction::CreateSP(this, &SAssetView::IsToggleShowFavoritesAllowed),
				FIsActionChecked::CreateSP(this, &SAssetView::IsShowingFavorites)
			),
			EUserInterfaceActionType::ToggleButton
		);

		Section.AddMenuEntry(
			"FilterRecursively",
			LOCTEXT("FilterRecursivelyOption", "Filter Recursively"),
			LOCTEXT("FilterRecursivelyOptionToolTip", "Should filters apply recursively in the view?"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &SAssetView::ToggleFilteringRecursively),
				FCanExecuteAction::CreateSP(this, &SAssetView::IsToggleFilteringRecursivelyAllowed),
				FIsActionChecked::CreateSP(this, &SAssetView::IsFilteringRecursively)
			),
			EUserInterfaceActionType::ToggleButton
		);

		Section.AddMenuEntry(
			"ShowAllFolder",
			LOCTEXT("ShowAllFolderOption", "Show All Folder"),
			LOCTEXT("ShowAllFolderOptionToolTip", "Show the all folder in the view?"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &SAssetView::ToggleShowAllFolder),
				FCanExecuteAction(),
				FIsActionChecked::CreateSP(this, &SAssetView::IsShowingAllFolder)
			),
			EUserInterfaceActionType::ToggleButton
		);

		Section.AddMenuEntry(
			"OrganizeFolders",
			LOCTEXT("OrganizeFoldersOption", "Organize Folders"),
			LOCTEXT("OrganizeFoldersOptionToolTip", "Organize folders in the view?"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &SAssetView::ToggleOrganizeFolders),
				FCanExecuteAction(),
				FIsActionChecked::CreateSP(this, &SAssetView::IsOrganizingFolders)
			),
			EUserInterfaceActionType::ToggleButton
		);

		if (bShowPathViewFilters)
		{
			Section.AddSubMenu(
				"PathViewFilters",
				LOCTEXT("PathViewFilters", "Path View Filters"),
				LOCTEXT("PathViewFilters_ToolTip", "Path View Filters"),
				FNewToolMenuDelegate());
		}
	}

	{
		FToolMenuSection& Section = Menu->AddSection("Content", LOCTEXT("ContentHeading", "Content"));
		Section.AddMenuEntry(
			"ShowCppClasses",
			LOCTEXT("ShowCppClassesOption", "Show C++ Classes"),
			LOCTEXT("ShowCppClassesOptionToolTip", "Show C++ classes in the view?"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP( this, &SAssetView::ToggleShowCppContent ),
				FCanExecuteAction::CreateSP( this, &SAssetView::IsToggleShowCppContentAllowed ),
				FIsActionChecked::CreateSP( this, &SAssetView::IsShowingCppContent )
			),
			EUserInterfaceActionType::ToggleButton
		);

		Section.AddMenuEntry(
			"ShowDevelopersContent",
			LOCTEXT("ShowDevelopersContentOption", "Show Developers Content"),
			LOCTEXT("ShowDevelopersContentOptionToolTip", "Show developers content in the view?"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP( this, &SAssetView::ToggleShowDevelopersContent ),
				FCanExecuteAction::CreateSP( this, &SAssetView::IsToggleShowDevelopersContentAllowed ),
				FIsActionChecked::CreateSP( this, &SAssetView::IsShowingDevelopersContent )
				),
			EUserInterfaceActionType::ToggleButton
		);

		Section.AddMenuEntry(
			"ShowEngineFolder",
			LOCTEXT("ShowEngineFolderOption", "Show Engine Content"),
			LOCTEXT("ShowEngineFolderOptionToolTip", "Show engine content in the view?"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP( this, &SAssetView::ToggleShowEngineContent ),
				FCanExecuteAction::CreateSP( this, &SAssetView::IsToggleShowEngineContentAllowed),
				FIsActionChecked::CreateSP( this, &SAssetView::IsShowingEngineContent )
			),
			EUserInterfaceActionType::ToggleButton
		);

		Section.AddMenuEntry(
			"ShowPluginFolder",
			LOCTEXT("ShowPluginFolderOption", "Show Plugin Content"),
			LOCTEXT("ShowPluginFolderOptionToolTip", "Show plugin content in the view?"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP( this, &SAssetView::ToggleShowPluginContent ),
				FCanExecuteAction::CreateSP(this, &SAssetView::IsToggleShowPluginContentAllowed),
				FIsActionChecked::CreateSP( this, &SAssetView::IsShowingPluginContent )
			),
			EUserInterfaceActionType::ToggleButton
		);

		Section.AddMenuEntry(
			"ShowLocalizedContent",
			LOCTEXT("ShowLocalizedContentOption", "Show Localized Content"),
			LOCTEXT("ShowLocalizedContentOptionToolTip", "Show localized content in the view?"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &SAssetView::ToggleShowLocalizedContent),
				FCanExecuteAction::CreateSP(this, &SAssetView::IsToggleShowLocalizedContentAllowed),
				FIsActionChecked::CreateSP(this, &SAssetView::IsShowingLocalizedContent)
				),
			EUserInterfaceActionType::ToggleButton
			);
	}

	{
		FToolMenuSection& Section = Menu->AddSection("Search", LOCTEXT("SearchHeading", "Search"));
		Section.AddMenuEntry(
			"IncludeClassName",
			LOCTEXT("IncludeClassNameOption", "Search Asset Class Names"),
			LOCTEXT("IncludeClassesNameOptionTooltip", "Include asset type names in search criteria?  (e.g. Blueprint, Texture, Sound)"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &SAssetView::ToggleIncludeClassNames),
				FCanExecuteAction::CreateSP(this, &SAssetView::IsToggleIncludeClassNamesAllowed),
				FIsActionChecked::CreateSP(this, &SAssetView::IsIncludingClassNames)
			),
			EUserInterfaceActionType::ToggleButton
		);

		Section.AddMenuEntry(
			"IncludeAssetPath",
			LOCTEXT("IncludeAssetPathOption", "Search Asset Path"),
			LOCTEXT("IncludeAssetPathOptionTooltip", "Include entire asset path in search criteria?"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &SAssetView::ToggleIncludeAssetPaths),
				FCanExecuteAction::CreateSP(this, &SAssetView::IsToggleIncludeAssetPathsAllowed),
				FIsActionChecked::CreateSP(this, &SAssetView::IsIncludingAssetPaths)
			),
			EUserInterfaceActionType::ToggleButton
		);

		Section.AddMenuEntry(
			"IncludeCollectionName",
			LOCTEXT("IncludeCollectionNameOption", "Search Collection Names"),
			LOCTEXT("IncludeCollectionNameOptionTooltip", "Include Collection names in search criteria?"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &SAssetView::ToggleIncludeCollectionNames),
				FCanExecuteAction::CreateSP(this, &SAssetView::IsToggleIncludeCollectionNamesAllowed),
				FIsActionChecked::CreateSP(this, &SAssetView::IsIncludingCollectionNames)
			),
			EUserInterfaceActionType::ToggleButton
		);
	}

	{
		FToolMenuSection& Section = Menu->AddSection("AssetThumbnails", LOCTEXT("ThumbnailsHeading", "Thumbnails"));

		auto CreateThumbnailSizeSubMenu = [this](UToolMenu* SubMenu)
		{
			FToolMenuSection& SizeSection = SubMenu->AddSection("ThumbnailSizes");
			
			for (int32 EnumValue = (int32)EThumbnailSize::Tiny; EnumValue < (int32)EThumbnailSize::MAX; ++EnumValue)
			{
				SizeSection.AddMenuEntry(
					NAME_None,
					SAssetView::ThumbnailSizeToDisplayName((EThumbnailSize)EnumValue),
					FText::GetEmpty(),
					FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateSP(this, &SAssetView::OnThumbnailSizeChanged, (EThumbnailSize)EnumValue),
						FCanExecuteAction::CreateSP(this, &SAssetView::IsThumbnailScalingAllowed),
						FIsActionChecked::CreateSP(this, &SAssetView::IsThumbnailSizeChecked, (EThumbnailSize)EnumValue)
					),
					EUserInterfaceActionType::RadioButton
				);
			}
		};
		Section.AddEntry(FToolMenuEntry::InitSubMenu(
			"ThumbnailSize",
			LOCTEXT("ThumbnailSize", "Thumbnail Size"),
			LOCTEXT("ThumbnailSizeToolTip", "Adjust the size of thumbnails."),
			FNewToolMenuDelegate::CreateLambda(CreateThumbnailSizeSubMenu)
		));

		Section.AddMenuEntry(
			"ThumbnailEditMode",
			LOCTEXT("ThumbnailEditModeOption", "Thumbnail Edit Mode"),
			LOCTEXT("ThumbnailEditModeOptionToolTip", "Toggle thumbnail editing mode. When in this mode you can rotate the camera on 3D thumbnails by dragging them."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP( this, &SAssetView::ToggleThumbnailEditMode ),
				FCanExecuteAction::CreateSP( this, &SAssetView::IsThumbnailEditModeAllowed ),
				FIsActionChecked::CreateSP( this, &SAssetView::IsThumbnailEditMode )
				),
			EUserInterfaceActionType::ToggleButton
			);

		Section.AddMenuEntry(
			"RealTimeThumbnails",
			LOCTEXT("RealTimeThumbnailsOption", "Real-Time Thumbnails"),
			LOCTEXT("RealTimeThumbnailsOptionToolTip", "Renders the assets thumbnails in real-time"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP( this, &SAssetView::ToggleRealTimeThumbnails ),
				FCanExecuteAction::CreateSP( this, &SAssetView::CanShowRealTimeThumbnails ),
				FIsActionChecked::CreateSP( this, &SAssetView::IsShowingRealTimeThumbnails )
			),
			EUserInterfaceActionType::ToggleButton
			);
	}

	if (GetColumnViewVisibility() == EVisibility::Visible)
	{
		{
			FToolMenuSection& Section = Menu->AddSection("AssetColumns", LOCTEXT("ToggleColumnsHeading", "Columns"));

			Section.AddMenuEntry(
				"ResetColumns",
				LOCTEXT("ResetColumns", "Reset Columns"),
				LOCTEXT("ResetColumnsToolTip", "Reset all columns to be visible again."),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(this, &SAssetView::ResetColumns)),
				EUserInterfaceActionType::Button
				);

			Section.AddMenuEntry(
				"ExportColumns",
				LOCTEXT("ExportColumns", "Export to CSV"),
				LOCTEXT("ExportColumnsToolTip", "Export column data to CSV."),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(this, &SAssetView::ExportColumns)),
				EUserInterfaceActionType::Button
			);
		}
	}
}

void SAssetView::ToggleShowFolders()
{
	check(IsToggleShowFoldersAllowed());

	bool bNewState = !GetDefault<UContentBrowserSettings>()->DisplayFolders;

	if (FContentBrowserInstanceConfig* Config = GetContentBrowserConfig())
	{
		bNewState = !Config->bShowFolders;
		Config->bShowFolders = bNewState;
		UContentBrowserConfig::Get()->SaveEditorConfig();
	}

	GetMutableDefault<UContentBrowserSettings>()->DisplayFolders = bNewState;
	GetMutableDefault<UContentBrowserSettings>()->PostEditChange();
}

bool SAssetView::IsToggleShowFoldersAllowed() const
{
	return bCanShowFolders;
}

bool SAssetView::IsShowingFolders() const
{
	if (!IsToggleShowFoldersAllowed())
	{
		return false;
	}
	
	if (const FContentBrowserInstanceConfig* Config = GetContentBrowserConfig())
	{
		return Config->bShowFolders;
	}

	return GetDefault<UContentBrowserSettings>()->DisplayFolders;
}

bool SAssetView::IsShowingReadOnlyFolders() const
{
	return bCanShowReadOnlyFolders;
}

void SAssetView::ToggleShowEmptyFolders()
{
	check(IsToggleShowEmptyFoldersAllowed());

	bool bNewState = !GetDefault<UContentBrowserSettings>()->DisplayEmptyFolders;

	if (FContentBrowserInstanceConfig* Config = GetContentBrowserConfig())
	{
		bNewState = !Config->bShowEmptyFolders;
		Config->bShowEmptyFolders = bNewState;
		UContentBrowserConfig::Get()->SaveEditorConfig();
	}
	
	GetMutableDefault<UContentBrowserSettings>()->DisplayEmptyFolders = !GetDefault<UContentBrowserSettings>()->DisplayEmptyFolders;
	GetMutableDefault<UContentBrowserSettings>()->PostEditChange();
}

bool SAssetView::IsToggleShowEmptyFoldersAllowed() const
{
	return bCanShowFolders;
}

bool SAssetView::IsShowingEmptyFolders() const
{
	if (!IsToggleShowEmptyFoldersAllowed())
	{
		return false;
	}

	if (const FContentBrowserInstanceConfig* Config = GetContentBrowserConfig())
	{
		return Config->bShowEmptyFolders;
	}

	return GetDefault<UContentBrowserSettings>()->DisplayEmptyFolders;
}

void SAssetView::ToggleRealTimeThumbnails()
{
	check(CanShowRealTimeThumbnails());

	bool bNewState = !IsShowingRealTimeThumbnails();

	GetMutableDefault<UContentBrowserSettings>()->RealTimeThumbnails = bNewState;
	GetMutableDefault<UContentBrowserSettings>()->PostEditChange();
}

bool SAssetView::CanShowRealTimeThumbnails() const
{
	return bCanShowRealTimeThumbnails;
}

bool SAssetView::IsShowingRealTimeThumbnails() const
{
	if (!CanShowRealTimeThumbnails())
	{
		return false;
	}

	return GetDefault<UContentBrowserSettings>()->RealTimeThumbnails;
}

void SAssetView::ToggleShowPluginContent()
{
	check(IsToggleShowPluginContentAllowed());

	bool bNewState = !GetDefault<UContentBrowserSettings>()->GetDisplayPluginFolders();

	if (FContentBrowserInstanceConfig* EditorConfig = GetContentBrowserConfig())
	{
		bNewState = !EditorConfig->bShowPluginContent;
		EditorConfig->bShowPluginContent = bNewState;
		UContentBrowserConfig::Get()->SaveEditorConfig();
	}

	GetMutableDefault<UContentBrowserSettings>()->SetDisplayPluginFolders(bNewState);
	GetMutableDefault<UContentBrowserSettings>()->PostEditChange();
}

bool SAssetView::IsShowingPluginContent() const
{
	if (bForceShowPluginContent)
	{
		return true;
	}

	if (const FContentBrowserInstanceConfig* Config = GetContentBrowserConfig())
	{
		return Config->bShowPluginContent;
	}

	return GetDefault<UContentBrowserSettings>()->GetDisplayPluginFolders();
}

void SAssetView::ToggleShowEngineContent()
{
	check(IsToggleShowEngineContentAllowed());

	bool bNewState = !GetDefault<UContentBrowserSettings>()->GetDisplayEngineFolder();

	if (FContentBrowserInstanceConfig* EditorConfig = GetContentBrowserConfig())
	{
		bNewState = !EditorConfig->bShowEngineContent;
		EditorConfig->bShowEngineContent = bNewState;
		UContentBrowserConfig::Get()->SaveEditorConfig();
	}

	GetMutableDefault<UContentBrowserSettings>()->SetDisplayEngineFolder(bNewState);
	GetMutableDefault<UContentBrowserSettings>()->PostEditChange();
}

bool SAssetView::IsShowingEngineContent() const
{
	if (bForceShowEngineContent)
	{
		return true;
	}

	if (const FContentBrowserInstanceConfig* Config = GetContentBrowserConfig())
	{
		return Config->bShowEngineContent;
	}

	return GetDefault<UContentBrowserSettings>()->GetDisplayEngineFolder();
}

void SAssetView::ToggleShowDevelopersContent()
{
	check(IsToggleShowDevelopersContentAllowed());

	bool bNewState = !GetDefault<UContentBrowserSettings>()->GetDisplayDevelopersFolder();

	if (FContentBrowserInstanceConfig* EditorConfig = GetContentBrowserConfig())
	{
		bNewState = !EditorConfig->bShowDeveloperContent;
		EditorConfig->bShowDeveloperContent = bNewState;
		UContentBrowserConfig::Get()->SaveEditorConfig();
	}

	GetMutableDefault<UContentBrowserSettings>()->SetDisplayDevelopersFolder(bNewState);
	GetMutableDefault<UContentBrowserSettings>()->PostEditChange();
}

bool SAssetView::IsToggleShowDevelopersContentAllowed() const
{
	return bCanShowDevelopersFolder;
}

bool SAssetView::IsToggleShowEngineContentAllowed() const
{
	return !bForceShowEngineContent;
}

bool SAssetView::IsToggleShowPluginContentAllowed() const
{
	return !bForceShowPluginContent;
}

bool SAssetView::IsShowingDevelopersContent() const
{
	if (!IsToggleShowDevelopersContentAllowed())
	{
		return false;
	}

	if (const FContentBrowserInstanceConfig* Config = GetContentBrowserConfig())
	{
		return Config->bShowDeveloperContent;
	}

	return GetDefault<UContentBrowserSettings>()->GetDisplayDevelopersFolder();
}

void SAssetView::ToggleShowLocalizedContent()
{
	check(IsToggleShowLocalizedContentAllowed());

	bool bNewState = !GetDefault<UContentBrowserSettings>()->GetDisplayL10NFolder();

	if (FContentBrowserInstanceConfig* Config = GetContentBrowserConfig())
	{
		bNewState = !Config->bShowLocalizedContent;
		Config->bShowLocalizedContent = bNewState;
		UContentBrowserConfig::Get()->SaveEditorConfig();
	}

	GetMutableDefault<UContentBrowserSettings>()->SetDisplayL10NFolder(bNewState);
	GetMutableDefault<UContentBrowserSettings>()->PostEditChange();
}

bool SAssetView::IsToggleShowLocalizedContentAllowed() const
{
	return true;
}

bool SAssetView::IsShowingLocalizedContent() const
{
	if (!IsToggleShowLocalizedContentAllowed())
	{
		return false;
	}

	if (const FContentBrowserInstanceConfig* Config = GetContentBrowserConfig())
	{
		return Config->bShowLocalizedContent;
	}

	return GetDefault<UContentBrowserSettings>()->GetDisplayL10NFolder();
}

void SAssetView::ToggleShowFavorites()
{
	check(IsToggleShowFavoritesAllowed());

	bool bNewState = !GetDefault<UContentBrowserSettings>()->GetDisplayFavorites();

	if (FContentBrowserInstanceConfig* Config = GetContentBrowserConfig())
	{
		bNewState = !Config->bShowFavorites;
		Config->bShowFavorites = bNewState;
		UContentBrowserConfig::Get()->SaveEditorConfig();
	}

	GetMutableDefault<UContentBrowserSettings>()->SetDisplayFavorites(bNewState);
	GetMutableDefault<UContentBrowserSettings>()->PostEditChange();
}

bool SAssetView::IsToggleShowFavoritesAllowed() const
{
	return bCanShowFavorites;
}

bool SAssetView::IsShowingFavorites() const
{
	if (!IsToggleShowFavoritesAllowed())
	{
		return false;
	}

	if (const FContentBrowserInstanceConfig* Config = GetContentBrowserConfig())
	{
		return Config->bShowFavorites;
	}

	return GetDefault<UContentBrowserSettings>()->GetDisplayFavorites();
}

void SAssetView::ToggleDockCollections()
{
	check(IsToggleDockCollectionsAllowed()); 

	bool bNewState = !GetDefault<UContentBrowserSettings>()->GetDockCollections();

	if (FContentBrowserInstanceConfig* Config = GetContentBrowserConfig())
	{
		bNewState = !Config->bCollectionsDocked;
		Config->bCollectionsDocked = bNewState;
		UContentBrowserConfig::Get()->SaveEditorConfig();
	}

	GetMutableDefault<UContentBrowserSettings>()->SetDockCollections(bNewState);
	GetMutableDefault<UContentBrowserSettings>()->PostEditChange();
}

bool SAssetView::IsToggleDockCollectionsAllowed() const
{
	return bCanDockCollections;
}

bool SAssetView::HasDockedCollections() const
{
	if (!IsToggleDockCollectionsAllowed())
	{
		return false;
	}

	if (const FContentBrowserInstanceConfig* Config = GetContentBrowserConfig())
	{
		return Config->bCollectionsDocked;
	}

	return GetDefault<UContentBrowserSettings>()->GetDockCollections();
}

void SAssetView::ToggleShowCppContent()
{
	check(IsToggleShowCppContentAllowed());

	bool bNewState = !GetDefault<UContentBrowserSettings>()->GetDisplayCppFolders();

	if (FContentBrowserInstanceConfig* Config = GetContentBrowserConfig())
	{
		bNewState = !Config->bShowCppFolders;
		Config->bShowCppFolders = bNewState;
		UContentBrowserConfig::Get()->SaveEditorConfig();
	}

	GetMutableDefault<UContentBrowserSettings>()->SetDisplayCppFolders(bNewState);
	GetMutableDefault<UContentBrowserSettings>()->PostEditChange();
}

bool SAssetView::IsToggleShowCppContentAllowed() const
{
	return bCanShowClasses;
}

bool SAssetView::IsShowingCppContent() const
{
	if (!IsToggleShowCppContentAllowed())
	{
		return false;
	}

	if (const FContentBrowserInstanceConfig* Config = GetContentBrowserConfig())
	{
		return Config->bShowCppFolders;
	}
	
	return GetDefault<UContentBrowserSettings>()->GetDisplayCppFolders();
}

void SAssetView::ToggleIncludeClassNames()
{
	check(IsToggleIncludeClassNamesAllowed());

	bool bNewState = !GetDefault<UContentBrowserSettings>()->GetIncludeClassNames();

	if (FContentBrowserInstanceConfig* Config = GetContentBrowserConfig())
	{
		bNewState = !Config->bSearchClasses;
		Config->bSearchClasses = bNewState;
		UContentBrowserConfig::Get()->SaveEditorConfig();
	}

	GetMutableDefault<UContentBrowserSettings>()->SetIncludeClassNames(bNewState);
	GetMutableDefault<UContentBrowserSettings>()->PostEditChange();

	OnSearchOptionsChanged.ExecuteIfBound();
}

bool SAssetView::IsToggleIncludeClassNamesAllowed() const
{
	return true;
}

bool SAssetView::IsIncludingClassNames() const
{
	if (!IsToggleIncludeClassNamesAllowed())
	{
		return false;
	}

	if (const FContentBrowserInstanceConfig* Config = GetContentBrowserConfig())
	{
		return Config->bSearchClasses;
	}
	
	return GetDefault<UContentBrowserSettings>()->GetIncludeClassNames();
}

void SAssetView::ToggleIncludeAssetPaths()
{
	check(IsToggleIncludeAssetPathsAllowed());

	bool bNewState = !GetDefault<UContentBrowserSettings>()->GetIncludeAssetPaths();

	if (FContentBrowserInstanceConfig* Config = GetContentBrowserConfig())
	{
		bNewState = !Config->bSearchAssetPaths;
		Config->bSearchAssetPaths = bNewState;
		UContentBrowserConfig::Get()->SaveEditorConfig();
	}

	GetMutableDefault<UContentBrowserSettings>()->SetIncludeAssetPaths(bNewState);
	GetMutableDefault<UContentBrowserSettings>()->PostEditChange();

	OnSearchOptionsChanged.ExecuteIfBound();
}

bool SAssetView::IsToggleIncludeAssetPathsAllowed() const
{
	return true;
}

bool SAssetView::IsIncludingAssetPaths() const
{
	if (!IsToggleIncludeAssetPathsAllowed())
	{
		return false;
	}

	if (const FContentBrowserInstanceConfig* Config = GetContentBrowserConfig())
	{
		return Config->bSearchAssetPaths;
	}
	
	return GetDefault<UContentBrowserSettings>()->GetIncludeAssetPaths();
}

void SAssetView::ToggleIncludeCollectionNames()
{
	check(IsToggleIncludeCollectionNamesAllowed());

	bool bNewState = !GetDefault<UContentBrowserSettings>()->GetIncludeCollectionNames();

	if (FContentBrowserInstanceConfig* Config = GetContentBrowserConfig())
	{
		bNewState = !Config->bSearchCollections;
		Config->bSearchCollections = bNewState;
		UContentBrowserConfig::Get()->SaveEditorConfig();
	}

	GetMutableDefault<UContentBrowserSettings>()->SetIncludeCollectionNames(bNewState);
	GetMutableDefault<UContentBrowserSettings>()->PostEditChange();

	OnSearchOptionsChanged.ExecuteIfBound();
}

bool SAssetView::IsToggleIncludeCollectionNamesAllowed() const
{
	return true;
}

bool SAssetView::IsIncludingCollectionNames() const
{
	if (!IsToggleIncludeCollectionNamesAllowed())
	{
		return false;
	}
	
	if (const FContentBrowserInstanceConfig* Config = GetContentBrowserConfig())
	{
		return Config->bSearchCollections;
	}
	
	return GetDefault<UContentBrowserSettings>()->GetIncludeCollectionNames();
}

void SAssetView::SetCurrentViewType(EAssetViewType::Type NewType)
{
	if ( ensure(NewType != EAssetViewType::MAX) && NewType != CurrentViewType )
	{
		ResetQuickJump();

		CurrentViewType = NewType;
		CreateCurrentView();

		SyncToSelection();

		// Clear relevant thumbnails to render fresh ones in the new view if needed
		RelevantThumbnails.Reset();
		VisibleItems.Reset();

		if ( NewType == EAssetViewType::Tile )
		{
			CurrentThumbnailSize = TileViewThumbnailSize;
			bPendingUpdateThumbnails = true;
		}
		else if ( NewType == EAssetViewType::List )
		{
			CurrentThumbnailSize = ListViewThumbnailSize;
			bPendingUpdateThumbnails = true;
		}
		else if ( NewType == EAssetViewType::Column )
		{
			// No thumbnails, but we do need to refresh filtered items to determine a majority asset type
			MajorityAssetType = NAME_None;
			RefreshFilteredItems();
			SortList();
		}

		if (FAssetViewInstanceConfig* Config = GetAssetViewConfig())
		{
			Config->ViewType = (uint8) NewType;
			UAssetViewConfig::Get()->SaveEditorConfig();
		}
	}
}

void SAssetView::SetCurrentThumbnailSize(EThumbnailSize NewThumbnailSize)
{
	if (ThumbnailSize != NewThumbnailSize)
	{
		OnThumbnailSizeChanged(NewThumbnailSize);
	}
}

void SAssetView::SetCurrentViewTypeFromMenu(EAssetViewType::Type NewType)
{
	if (NewType != CurrentViewType)
	{
		SetCurrentViewType(NewType);
	}
}

void SAssetView::CreateCurrentView()
{
	TileView.Reset();
	ListView.Reset();
	ColumnView.Reset();

	TSharedRef<SWidget> NewView = SNullWidget::NullWidget;
	switch (CurrentViewType)
	{
		case EAssetViewType::Tile:
			TileView = CreateTileView();
			NewView = CreateShadowOverlay(TileView.ToSharedRef());
			break;
		case EAssetViewType::List:
			ListView = CreateListView();
			NewView = CreateShadowOverlay(ListView.ToSharedRef());
			break;
		case EAssetViewType::Column:
			ColumnView = CreateColumnView();
			NewView = CreateShadowOverlay(ColumnView.ToSharedRef());
			break;
	}
	
	ViewContainer->SetContent( NewView );
}

TSharedRef<SWidget> SAssetView::CreateShadowOverlay( TSharedRef<STableViewBase> Table )
{
	if (bForceHideScrollbar)
	{
		return Table;
	}

	return SNew(SScrollBorder, Table)
		[
			Table
		];
}

EAssetViewType::Type SAssetView::GetCurrentViewType() const
{
	return CurrentViewType;
}

bool SAssetView::IsCurrentViewType(EAssetViewType::Type ViewType) const
{
	return GetCurrentViewType() == ViewType;
}

void SAssetView::FocusList() const
{
	switch ( GetCurrentViewType() )
	{
		case EAssetViewType::List: FSlateApplication::Get().SetKeyboardFocus(ListView, EFocusCause::SetDirectly); break;
		case EAssetViewType::Tile: FSlateApplication::Get().SetKeyboardFocus(TileView, EFocusCause::SetDirectly); break;
		case EAssetViewType::Column: FSlateApplication::Get().SetKeyboardFocus(ColumnView, EFocusCause::SetDirectly); break;
	}
}

void SAssetView::RefreshList()
{
	switch ( GetCurrentViewType() )
	{
		case EAssetViewType::List: ListView->RequestListRefresh(); break;
		case EAssetViewType::Tile: TileView->RequestListRefresh(); break;
		case EAssetViewType::Column: ColumnView->RequestListRefresh(); break;
	}
}

void SAssetView::SetSelection(const TSharedPtr<FAssetViewItem>& Item)
{
	switch ( GetCurrentViewType() )
	{
		case EAssetViewType::List: ListView->SetSelection(Item); break;
		case EAssetViewType::Tile: TileView->SetSelection(Item); break;
		case EAssetViewType::Column: ColumnView->SetSelection(Item); break;
	}
}

void SAssetView::SetItemSelection(const TSharedPtr<FAssetViewItem>& Item, bool bSelected, const ESelectInfo::Type SelectInfo)
{
	switch ( GetCurrentViewType() )
	{
		case EAssetViewType::List: ListView->SetItemSelection(Item, bSelected, SelectInfo); break;
		case EAssetViewType::Tile: TileView->SetItemSelection(Item, bSelected, SelectInfo); break;
		case EAssetViewType::Column: ColumnView->SetItemSelection(Item, bSelected, SelectInfo); break;
	}
}

void SAssetView::RequestScrollIntoView(const TSharedPtr<FAssetViewItem>& Item)
{
	switch ( GetCurrentViewType() )
	{
		case EAssetViewType::List: ListView->RequestScrollIntoView(Item); break;
		case EAssetViewType::Tile: TileView->RequestScrollIntoView(Item); break;
		case EAssetViewType::Column: ColumnView->RequestScrollIntoView(Item); break;
	}
}

void SAssetView::OnOpenAssetsOrFolders()
{
	if (OnItemsActivated.IsBound())
	{
		OnInteractDuringFiltering();
		const TArray<FContentBrowserItem> SelectedItems = GetSelectedItems();
		OnItemsActivated.Execute(SelectedItems, EAssetTypeActivationMethod::Opened);
	}
}

void SAssetView::OnPreviewAssets()
{
	if (OnItemsActivated.IsBound())
	{
		OnInteractDuringFiltering();
		const TArray<FContentBrowserItem> SelectedItems = GetSelectedItems();
		OnItemsActivated.Execute(SelectedItems, EAssetTypeActivationMethod::Previewed);
	}
}

void SAssetView::ClearSelection(bool bForceSilent)
{
	const bool bTempBulkSelectingValue = bForceSilent ? true : bBulkSelecting;
	TGuardValue<bool> Guard(bBulkSelecting, bTempBulkSelectingValue);
	switch ( GetCurrentViewType() )
	{
		case EAssetViewType::List: ListView->ClearSelection(); break;
		case EAssetViewType::Tile: TileView->ClearSelection(); break;
		case EAssetViewType::Column: ColumnView->ClearSelection(); break;
	}
}

TSharedRef<ITableRow> SAssetView::MakeListViewWidget(TSharedPtr<FAssetViewItem> AssetItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	if ( !ensure(AssetItem.IsValid()) )
	{
		return SNew( STableRow<TSharedPtr<FAssetViewItem>>, OwnerTable );
	}

	VisibleItems.Add(AssetItem);
	bPendingUpdateThumbnails = true;

	if (AssetItem->IsFolder())
	{
		return
			SNew( SAssetListViewRow, OwnerTable )
			.OnDragDetected( this, &SAssetView::OnDraggingAssetItem )
			.Cursor( bAllowDragging ? EMouseCursor::GrabHand : EMouseCursor::Default )
			.AssetListItem(
				SNew(SAssetListItem)
					.AssetItem(AssetItem)
					.ItemHeight(this, &SAssetView::GetListViewItemHeight)
					.OnRenameBegin(this, &SAssetView::AssetRenameBegin)
					.OnRenameCommit(this, &SAssetView::AssetRenameCommit)
					.OnVerifyRenameCommit(this, &SAssetView::AssetVerifyRenameCommit)
					.OnItemDestroyed(this, &SAssetView::AssetItemWidgetDestroyed)
					.ShouldAllowToolTip(this, &SAssetView::ShouldAllowToolTips)
					.HighlightText(HighlightedText)
			);
	}
	else
	{
		TSharedPtr<FAssetThumbnail>& AssetThumbnail = RelevantThumbnails.FindOrAdd(AssetItem);
		if (!AssetThumbnail)
		{
			AssetThumbnail = MakeShared<FAssetThumbnail>(FAssetData(), ListViewThumbnailResolution, ListViewThumbnailResolution, AssetThumbnailPool);
			AssetItem->GetItem().UpdateThumbnail(*AssetThumbnail);
			AssetThumbnail->GetViewportRenderTargetTexture(); // Access the texture once to trigger it to render
		}
		
		return
			SNew( SAssetListViewRow, OwnerTable )
			.OnDragDetected( this, &SAssetView::OnDraggingAssetItem )
			.Cursor( bAllowDragging ? EMouseCursor::GrabHand : EMouseCursor::Default )
			.AssetListItem(
				SNew(SAssetListItem)
					.AssetThumbnail(AssetThumbnail)
					.AssetItem(AssetItem)
					.ThumbnailPadding((float)ListViewThumbnailPadding)
					.ItemHeight(this, &SAssetView::GetListViewItemHeight)
					.OnRenameBegin(this, &SAssetView::AssetRenameBegin)
					.OnRenameCommit(this, &SAssetView::AssetRenameCommit)
					.OnVerifyRenameCommit(this, &SAssetView::AssetVerifyRenameCommit)
					.OnItemDestroyed(this, &SAssetView::AssetItemWidgetDestroyed)
					.ShouldAllowToolTip(this, &SAssetView::ShouldAllowToolTips)
					.HighlightText(HighlightedText)
					.ThumbnailEditMode(this, &SAssetView::IsThumbnailEditMode)
					.ThumbnailLabel( ThumbnailLabel )
					.ThumbnailHintColorAndOpacity( this, &SAssetView::GetThumbnailHintColorAndOpacity )
					.AllowThumbnailHintLabel( AllowThumbnailHintLabel )
					.OnIsAssetValidForCustomToolTip(OnIsAssetValidForCustomToolTip)
					.OnGetCustomAssetToolTip(OnGetCustomAssetToolTip)
					.OnVisualizeAssetToolTip(OnVisualizeAssetToolTip)
					.OnAssetToolTipClosing(OnAssetToolTipClosing)
			);
	}
}

TSharedRef<ITableRow> SAssetView::MakeTileViewWidget(TSharedPtr<FAssetViewItem> AssetItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	if ( !ensure(AssetItem.IsValid()) )
	{
		return SNew( STableRow<TSharedPtr<FAssetViewItem>>, OwnerTable );
	}

	VisibleItems.Add(AssetItem);
	bPendingUpdateThumbnails = true;

	if (AssetItem->IsFolder())
	{
		TSharedPtr< STableRow<TSharedPtr<FAssetViewItem>> > TableRowWidget;
		SAssignNew( TableRowWidget, STableRow<TSharedPtr<FAssetViewItem>>, OwnerTable )
			.Style( FAppStyle::Get(), "ContentBrowser.AssetListView.TileTableRow" )
			.Cursor( bAllowDragging ? EMouseCursor::GrabHand : EMouseCursor::Default )
			.OnDragDetected( this, &SAssetView::OnDraggingAssetItem );

		TSharedRef<SAssetTileItem> Item =
			SNew(SAssetTileItem)
			.AssetItem(AssetItem)
			.ThumbnailPadding((float)TileViewThumbnailPadding)
			.ItemWidth(this, &SAssetView::GetTileViewItemWidth)
			.OnRenameBegin(this, &SAssetView::AssetRenameBegin)
			.OnRenameCommit(this, &SAssetView::AssetRenameCommit)
			.OnVerifyRenameCommit(this, &SAssetView::AssetVerifyRenameCommit)
			.OnItemDestroyed(this, &SAssetView::AssetItemWidgetDestroyed)
			.ShouldAllowToolTip(this, &SAssetView::ShouldAllowToolTips)
			.HighlightText( HighlightedText )
			.IsSelected(FIsSelected::CreateSP(TableRowWidget.Get(), &STableRow<TSharedPtr<FAssetViewItem>>::IsSelected))
			.IsSelectedExclusively(FIsSelected::CreateSP(TableRowWidget.Get(), &STableRow<TSharedPtr<FAssetViewItem>>::IsSelectedExclusively))
			.AddMetaData<FTagMetaData>(AssetItem->GetItem().GetItemName());

		TableRowWidget->SetContent(Item);

		return TableRowWidget.ToSharedRef();
	}
	else
	{
		TSharedPtr<FAssetThumbnail>& AssetThumbnail = RelevantThumbnails.FindOrAdd(AssetItem);
		if (!AssetThumbnail)
		{
			AssetThumbnail = MakeShared<FAssetThumbnail>(FAssetData(), TileViewThumbnailResolution, TileViewThumbnailResolution, AssetThumbnailPool);
			AssetItem->GetItem().UpdateThumbnail(*AssetThumbnail);
			AssetThumbnail->GetViewportRenderTargetTexture(); // Access the texture once to trigger it to render
		}

		TSharedPtr< STableRow<TSharedPtr<FAssetViewItem>> > TableRowWidget;
		SAssignNew( TableRowWidget, STableRow<TSharedPtr<FAssetViewItem>>, OwnerTable )
		.Style(FAppStyle::Get(), "ContentBrowser.AssetListView.TileTableRow")
		.Cursor( bAllowDragging ? EMouseCursor::GrabHand : EMouseCursor::Default )
		.OnDragDetected( this, &SAssetView::OnDraggingAssetItem );

		TSharedRef<SAssetTileItem> Item =
			SNew(SAssetTileItem)
			.AssetThumbnail(AssetThumbnail)
			.AssetItem(AssetItem)
			.ThumbnailPadding((float)TileViewThumbnailPadding)
			.CurrentThumbnailSize(this, &SAssetView::GetThumbnailSize)
			.ItemWidth(this, &SAssetView::GetTileViewItemWidth)
			.OnRenameBegin(this, &SAssetView::AssetRenameBegin)
			.OnRenameCommit(this, &SAssetView::AssetRenameCommit)
			.OnVerifyRenameCommit(this, &SAssetView::AssetVerifyRenameCommit)
			.OnItemDestroyed(this, &SAssetView::AssetItemWidgetDestroyed)
			.ShouldAllowToolTip(this, &SAssetView::ShouldAllowToolTips)
			.HighlightText( HighlightedText )
			.ThumbnailEditMode(this, &SAssetView::IsThumbnailEditMode)
			.ThumbnailLabel( ThumbnailLabel )
			.ThumbnailHintColorAndOpacity( this, &SAssetView::GetThumbnailHintColorAndOpacity )
			.AllowThumbnailHintLabel( AllowThumbnailHintLabel )
			.IsSelected(FIsSelected::CreateSP(TableRowWidget.Get(), &STableRow<TSharedPtr<FAssetViewItem>>::IsSelected))
			.IsSelectedExclusively( FIsSelected::CreateSP(TableRowWidget.Get(), &STableRow<TSharedPtr<FAssetViewItem>>::IsSelectedExclusively) )
			.OnIsAssetValidForCustomToolTip(OnIsAssetValidForCustomToolTip)
			.OnGetCustomAssetToolTip(OnGetCustomAssetToolTip)
			.OnVisualizeAssetToolTip( OnVisualizeAssetToolTip )
			.OnAssetToolTipClosing( OnAssetToolTipClosing )
			.ShowType(bShowTypeInTileView)
			.AddMetaData<FTagMetaData>(AssetItem->GetItem().GetItemName());
		
		TableRowWidget->SetContent(Item);

		return TableRowWidget.ToSharedRef();
	}
}

TSharedRef<ITableRow> SAssetView::MakeColumnViewWidget(TSharedPtr<FAssetViewItem> AssetItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	if ( !ensure(AssetItem.IsValid()) )
	{
		return SNew( STableRow<TSharedPtr<FAssetViewItem>>, OwnerTable )
			.Style(FAppStyle::Get(), "ContentBrowser.AssetListView.ColumnListTableRow");
	}

	// Update the cached custom data
	AssetItem->CacheCustomColumns(CustomColumns, false, true, false);
	
	return
		SNew( SAssetColumnViewRow, OwnerTable )
		.OnDragDetected( this, &SAssetView::OnDraggingAssetItem )
		.Cursor( bAllowDragging ? EMouseCursor::GrabHand : EMouseCursor::Default )
		.AssetColumnItem(
			SNew(SAssetColumnItem)
				.AssetItem(AssetItem)
				.OnRenameBegin(this, &SAssetView::AssetRenameBegin)
				.OnRenameCommit(this, &SAssetView::AssetRenameCommit)
				.OnVerifyRenameCommit(this, &SAssetView::AssetVerifyRenameCommit)
				.OnItemDestroyed(this, &SAssetView::AssetItemWidgetDestroyed)
				.HighlightText( HighlightedText )
				.OnIsAssetValidForCustomToolTip(OnIsAssetValidForCustomToolTip)
				.OnGetCustomAssetToolTip(OnGetCustomAssetToolTip)
				.OnVisualizeAssetToolTip( OnVisualizeAssetToolTip )
				.OnAssetToolTipClosing( OnAssetToolTipClosing )
		);
}

void SAssetView::AssetItemWidgetDestroyed(const TSharedPtr<FAssetViewItem>& Item)
{
	if(RenamingAsset.Pin().Get() == Item.Get())
	{
		/* Check if the item is in a temp state and if it is, commit using the default name so that it does not entirely vanish on the user.
		   This keeps the functionality consistent for content to never be in a temporary state */

		if (Item && Item->IsTemporary())
		{
			if (Item->IsFile())
			{
				FText OutErrorText;
				EndCreateDeferredItem(Item, Item->GetItem().GetItemName().ToString(), /*bFinalize*/true, OutErrorText);
			}
			else
			{
				DeferredItemToCreate.Reset();
			}
		}

		RenamingAsset.Reset();
	}

	if ( VisibleItems.Remove(Item) != INDEX_NONE )
	{
		bPendingUpdateThumbnails = true;
	}
}

void SAssetView::UpdateThumbnails()
{
	int32 MinItemIdx = INDEX_NONE;
	int32 MaxItemIdx = INDEX_NONE;
	int32 MinVisibleItemIdx = INDEX_NONE;
	int32 MaxVisibleItemIdx = INDEX_NONE;

	const int32 HalfNumOffscreenThumbnails = NumOffscreenThumbnails / 2;
	for ( auto ItemIt = VisibleItems.CreateConstIterator(); ItemIt; ++ItemIt )
	{
		int32 ItemIdx = FilteredAssetItems.Find(*ItemIt);
		if ( ItemIdx != INDEX_NONE )
		{
			const int32 ItemIdxLow = FMath::Max<int32>(0, ItemIdx - HalfNumOffscreenThumbnails);
			const int32 ItemIdxHigh = FMath::Min<int32>(FilteredAssetItems.Num() - 1, ItemIdx + HalfNumOffscreenThumbnails);
			if ( MinItemIdx == INDEX_NONE || ItemIdxLow < MinItemIdx )
			{
				MinItemIdx = ItemIdxLow;
			}
			if ( MaxItemIdx == INDEX_NONE || ItemIdxHigh > MaxItemIdx )
			{
				MaxItemIdx = ItemIdxHigh;
			}
			if ( MinVisibleItemIdx == INDEX_NONE || ItemIdx < MinVisibleItemIdx )
			{
				MinVisibleItemIdx = ItemIdx;
			}
			if ( MaxVisibleItemIdx == INDEX_NONE || ItemIdx > MaxVisibleItemIdx )
			{
				MaxVisibleItemIdx = ItemIdx;
			}
		}
	}

	if ( MinItemIdx != INDEX_NONE && MaxItemIdx != INDEX_NONE && MinVisibleItemIdx != INDEX_NONE && MaxVisibleItemIdx != INDEX_NONE )
	{
		// We have a new min and a new max, compare it to the old min and max so we can create new thumbnails
		// when appropriate and remove old thumbnails that are far away from the view area.
		TMap< TSharedPtr<FAssetViewItem>, TSharedPtr<FAssetThumbnail> > NewRelevantThumbnails;

		// Operate on offscreen items that are furthest away from the visible items first since the thumbnail pool processes render requests in a LIFO order.
		while (MinItemIdx < MinVisibleItemIdx || MaxItemIdx > MaxVisibleItemIdx)
		{
			const int32 LowEndDistance = MinVisibleItemIdx - MinItemIdx;
			const int32 HighEndDistance = MaxItemIdx - MaxVisibleItemIdx;

			if ( HighEndDistance > LowEndDistance )
			{
				if(FilteredAssetItems.IsValidIndex(MaxItemIdx) && FilteredAssetItems[MaxItemIdx]->IsFile())
				{
					AddItemToNewThumbnailRelevancyMap(FilteredAssetItems[MaxItemIdx], NewRelevantThumbnails);
				}
				MaxItemIdx--;
			}
			else
			{
				if(FilteredAssetItems.IsValidIndex(MinItemIdx) && FilteredAssetItems[MinItemIdx]->IsFile())
				{
					AddItemToNewThumbnailRelevancyMap(FilteredAssetItems[MinItemIdx], NewRelevantThumbnails);
				}
				MinItemIdx++;
			}
		}

		// Now operate on VISIBLE items then prioritize them so they are rendered first
		TArray< TSharedPtr<FAssetThumbnail> > ThumbnailsToPrioritize;
		for ( int32 ItemIdx = MinVisibleItemIdx; ItemIdx <= MaxVisibleItemIdx; ++ItemIdx )
		{
			if(FilteredAssetItems.IsValidIndex(ItemIdx) && FilteredAssetItems[ItemIdx]->IsFile())
			{
				TSharedPtr<FAssetThumbnail> Thumbnail = AddItemToNewThumbnailRelevancyMap( FilteredAssetItems[ItemIdx], NewRelevantThumbnails );
				if ( Thumbnail.IsValid() )
				{
					ThumbnailsToPrioritize.Add(Thumbnail);
				}
			}
		}

		// Now prioritize all thumbnails there were in the visible range
		if ( ThumbnailsToPrioritize.Num() > 0 )
		{
			AssetThumbnailPool->PrioritizeThumbnails(ThumbnailsToPrioritize, CurrentThumbnailSize, CurrentThumbnailSize);
		}

		// Assign the new map of relevant thumbnails. This will remove any entries that were no longer relevant.
		RelevantThumbnails = NewRelevantThumbnails;
	}
}

TSharedPtr<FAssetThumbnail> SAssetView::AddItemToNewThumbnailRelevancyMap(const TSharedPtr<FAssetViewItem>& Item, TMap< TSharedPtr<FAssetViewItem>, TSharedPtr<FAssetThumbnail> >& NewRelevantThumbnails)
{
	checkf(Item->IsFile(), TEXT("Only files can have thumbnails!"));

	TSharedPtr<FAssetThumbnail> Thumbnail = RelevantThumbnails.FindRef(Item);
	if (!Thumbnail)
	{
		if (!ensure(CurrentThumbnailSize > 0 && CurrentThumbnailSize <= MAX_THUMBNAIL_SIZE))
		{
			// Thumbnail size must be in a sane range
			CurrentThumbnailSize = 64;
		}

		// The thumbnail newly relevant, create a new thumbnail
		const int32 ThumbnailResolution = FMath::TruncToInt((float)CurrentThumbnailSize * MaxThumbnailScale);
		Thumbnail = MakeShared<FAssetThumbnail>(FAssetData(), ThumbnailResolution, ThumbnailResolution, AssetThumbnailPool);
		Item->GetItem().UpdateThumbnail(*Thumbnail);
		Thumbnail->GetViewportRenderTargetTexture(); // Access the texture once to trigger it to render
	}

	if (Thumbnail)
	{
		NewRelevantThumbnails.Add(Item, Thumbnail);
	}

	return Thumbnail;
}

void SAssetView::AssetSelectionChanged( TSharedPtr< FAssetViewItem > AssetItem, ESelectInfo::Type SelectInfo )
{
	if (!bBulkSelecting)
	{
		if (AssetItem)
		{
			OnItemSelectionChanged.ExecuteIfBound(AssetItem->GetItem(), SelectInfo);
		}
		else
		{
			OnItemSelectionChanged.ExecuteIfBound(FContentBrowserItem(), SelectInfo);
		}
	}
}

void SAssetView::ItemScrolledIntoView(TSharedPtr<FAssetViewItem> AssetItem, const TSharedPtr<ITableRow>& Widget )
{
	if (AssetItem->ShouldRenameWhenScrolledIntoView())
	{
		// Make sure we have window focus to avoid the inline text editor from canceling itself if we try to click on it
		// This can happen if creating an asset opens an intermediary window which steals our focus, 
		// eg, the blueprint and slate widget style class windows (TTP# 314240)
		TSharedPtr<SWindow> OwnerWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
		if (OwnerWindow.IsValid())
		{
			OwnerWindow->BringToFront();
		}

		AwaitingRename = AssetItem;
	}
}

TSharedPtr<SWidget> SAssetView::OnGetContextMenuContent()
{
	if (CanOpenContextMenu())
	{
		if (IsRenamingAsset())
		{
			RenamingAsset.Pin()->OnRenameCanceled().ExecuteIfBound();
			RenamingAsset.Reset();
		}

		OnInteractDuringFiltering();
		const TArray<FContentBrowserItem> SelectedItems = GetSelectedItems();
		return OnGetItemContextMenu.Execute(SelectedItems);
	}

	return nullptr;
}

bool SAssetView::CanOpenContextMenu() const
{
	if (!OnGetItemContextMenu.IsBound())
	{
		// You can only a summon a context menu if one is set up
		return false;
	}

	if (IsThumbnailEditMode())
	{
		// You can not summon a context menu for assets when in thumbnail edit mode because right clicking may happen inadvertently while adjusting thumbnails.
		return false;
	}

	TArray<TSharedPtr<FAssetViewItem>> SelectedItems = GetSelectedViewItems();

	// Detect if at least one temporary item was selected. If there is only a temporary item selected, then deny the context menu.
	int32 NumTemporaryItemsSelected = 0;
	int32 NumCollectionFoldersSelected = 0;
	for (const TSharedPtr<FAssetViewItem>& Item : SelectedItems)
	{
		if (Item->IsTemporary())
		{
			++NumTemporaryItemsSelected;
		}

		if (Item->IsFolder() && EnumHasAnyFlags(Item->GetItem().GetItemCategory(), EContentBrowserItemFlags::Category_Collection))
		{
			++NumCollectionFoldersSelected;
		}
	}

	// If there are only a temporary items selected, deny the context menu
	if (SelectedItems.Num() > 0 && SelectedItems.Num() == NumTemporaryItemsSelected)
	{
		return false;
	}

	// If there are any collection folders selected, deny the context menu
	if (NumCollectionFoldersSelected > 0)
	{
		return false;
	}

	return true;
}

void SAssetView::OnListMouseButtonDoubleClick(TSharedPtr<FAssetViewItem> AssetItem)
{
	if ( !ensure(AssetItem.IsValid()) )
	{
		return;
	}

	if ( IsThumbnailEditMode() )
	{
		// You can not activate assets when in thumbnail edit mode because double clicking may happen inadvertently while adjusting thumbnails.
		return;
	}

	if ( AssetItem->IsTemporary() )
	{
		// You may not activate temporary items, they are just for display.
		return;
	}

	if (OnItemsActivated.IsBound())
	{
		OnInteractDuringFiltering();
		OnItemsActivated.Execute(MakeArrayView(&AssetItem->GetItem(), 1), EAssetTypeActivationMethod::DoubleClicked);
	}
}

FReply SAssetView::OnDraggingAssetItem( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if (bAllowDragging)
	{
		OnInteractDuringFiltering();
		// Use the custom drag handler?
		if (FEditorDelegates::OnAssetDragStarted.IsBound())
		{
			TArray<FAssetData> SelectedAssets = GetSelectedAssets();
			SelectedAssets.RemoveAll([](const FAssetData& InAssetData)
			{
				return InAssetData.IsRedirector();
			});

			if (SelectedAssets.Num() > 0)
			{
				FEditorDelegates::OnAssetDragStarted.Broadcast(SelectedAssets, nullptr);
				return FReply::Handled();
			}
		}

		// Use the standard drag handler?
		if (MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
		{
			TArray<FContentBrowserItem> SelectedItems = GetSelectedItems();
			SelectedItems.RemoveAll([](const FContentBrowserItem& InItem)
			{
				return InItem.IsFolder() && EnumHasAnyFlags(InItem.GetItemCategory(), EContentBrowserItemFlags::Category_Collection);
			});

			if (TSharedPtr<FDragDropOperation> DragDropOp = DragDropHandler::CreateDragOperation(SelectedItems))
			{
				return FReply::Handled().BeginDragDrop(DragDropOp.ToSharedRef());
			}
		}
	}

	return FReply::Unhandled();
}

bool SAssetView::AssetVerifyRenameCommit(const TSharedPtr<FAssetViewItem>& Item, const FText& NewName, const FSlateRect& MessageAnchor, FText& OutErrorMessage)
{
	const FString& NewItemName = NewName.ToString();

	if (DeferredItemToCreate.IsValid() && DeferredItemToCreate->bWasAddedToView)
	{
		checkf(FContentBrowserItemKey(Item->GetItem()) == FContentBrowserItemKey(DeferredItemToCreate->ItemContext.GetItem()), TEXT("DeferredItemToCreate was still set when attempting to rename a different item!"));

		return DeferredItemToCreate->ItemContext.ValidateItem(NewItemName, &OutErrorMessage);
	}
	else if (!Item->GetItem().GetItemName().ToString().Equals(NewItemName))
	{
		return Item->GetItem().CanRename(&NewItemName, &OutErrorMessage);
	}

	return true;
}

void SAssetView::AssetRenameBegin(const TSharedPtr<FAssetViewItem>& Item, const FString& NewName, const FSlateRect& MessageAnchor)
{
	check(!RenamingAsset.IsValid());
	RenamingAsset = Item;

	OnInteractDuringFiltering();

	if (DeferredItemToCreate.IsValid())
	{
		UE_LOG(LogContentBrowser, Log, TEXT("Renaming the item being created (Deferred Item: %s)."), *Item->GetItem().GetItemName().ToString());
	}
}

void SAssetView::AssetRenameCommit(const TSharedPtr<FAssetViewItem>& Item, const FString& NewName, const FSlateRect& MessageAnchor, const ETextCommit::Type CommitType)
{
	bool bSuccess = false;
	FText ErrorMessage;
	TSharedPtr<FAssetViewItem> UpdatedItem;

	UE_LOG(LogContentBrowser, Log, TEXT("Attempting asset rename: %s -> %s"), *Item->GetItem().GetItemName().ToString(), *NewName);

	if (DeferredItemToCreate.IsValid() && DeferredItemToCreate->bWasAddedToView)
	{
		const bool bFinalize = CommitType != ETextCommit::OnCleared; // Clearing the rename box on a newly created item cancels the entire creation process

		FContentBrowserItem NewItem = EndCreateDeferredItem(Item, NewName, bFinalize, ErrorMessage);
		if (NewItem.IsValid())
		{
			bSuccess = true;

			// Add result to view
			UpdatedItem = AvailableBackendItems.Add(FContentBrowserItemKey(NewItem), MakeShared<FAssetViewItem>(NewItem));
			FilteredAssetItems.Add(UpdatedItem);
		}
	}
	else if (CommitType != ETextCommit::OnCleared && !Item->GetItem().GetItemName().ToString().Equals(NewName))
	{
		UContentBrowserDataSubsystem* ContentBrowserData = IContentBrowserDataModule::Get().GetSubsystem();
		FScopedSuppressContentBrowserDataTick TickSuppression(ContentBrowserData);

		FContentBrowserItem NewItem;
		if (Item->GetItem().CanRename(&NewName, &ErrorMessage) && Item->GetItem().Rename(NewName, &NewItem))
		{
			bSuccess = true;

			// Add result to view (the old item will be removed via the notifications, as not all data sources may have been able to perform the rename)
			UpdatedItem = AvailableBackendItems.Add(FContentBrowserItemKey(NewItem), MakeShared<FAssetViewItem>(NewItem));
			FilteredAssetItems.Add(UpdatedItem);
		}
	}
	
	if (bSuccess)
	{
		if (UpdatedItem)
		{
			// Sort in the new item
			bPendingSortFilteredItems = true;

			if (UpdatedItem->IsFile())
			{
				// Refresh the thumbnail
				if (TSharedPtr<FAssetThumbnail> AssetThumbnail = RelevantThumbnails.FindRef(Item))
				{
					if (UpdatedItem != Item)
					{
						// This item was newly created - move the thumbnail over from the temporary item
						RelevantThumbnails.Remove(Item);
						RelevantThumbnails.Add(UpdatedItem, AssetThumbnail);
						UpdatedItem->GetItem().UpdateThumbnail(*AssetThumbnail);
					}
					if (AssetThumbnail->GetAssetData().IsValid())
					{
						AssetThumbnailPool->RefreshThumbnail(AssetThumbnail);
					}
				}
			}
			
			// Sync the view
			{
				TArray<FContentBrowserItem> ItemsToSync;
				ItemsToSync.Add(UpdatedItem->GetItem());

				if (OnItemRenameCommitted.IsBound() && !bUserSearching)
				{
					// If our parent wants to potentially handle the sync, let it, but only if we're not currently searching (or it would cancel the search)
					OnItemRenameCommitted.Execute(ItemsToSync);
				}
				else
				{
					// Otherwise, sync just the view
					SyncToItems(ItemsToSync);
				}
			}
		}
	}
	else if (!ErrorMessage.IsEmpty())
	{
		// Prompt the user with the reason the rename/creation failed
		ContentBrowserUtils::DisplayMessage(ErrorMessage, MessageAnchor, SharedThis(this));
	}

	RenamingAsset.Reset();
}

bool SAssetView::IsRenamingAsset() const
{
	return RenamingAsset.IsValid();
}

bool SAssetView::ShouldAllowToolTips() const
{
	bool bIsRightClickScrolling = false;
	switch( CurrentViewType )
	{
		case EAssetViewType::List:
			bIsRightClickScrolling = ListView->IsRightClickScrolling();
			break;

		case EAssetViewType::Tile:
			bIsRightClickScrolling = TileView->IsRightClickScrolling();
			break;

		case EAssetViewType::Column:
			bIsRightClickScrolling = ColumnView->IsRightClickScrolling();
			break;

		default:
			bIsRightClickScrolling = false;
			break;
	}

	return !bIsRightClickScrolling && !IsThumbnailEditMode() && !IsRenamingAsset();
}

bool SAssetView::IsThumbnailEditMode() const
{
	return IsThumbnailEditModeAllowed() && bThumbnailEditMode;
}

bool SAssetView::IsThumbnailEditModeAllowed() const
{
	return bAllowThumbnailEditMode && GetCurrentViewType() != EAssetViewType::Column;
}

FReply SAssetView::EndThumbnailEditModeClicked()
{
	bThumbnailEditMode = false;

	return FReply::Handled();
}

FText SAssetView::GetAssetCountText() const
{
	const int32 NumAssets = FilteredAssetItems.Num();
	const int32 NumSelectedAssets = GetSelectedViewItems().Num();

	FText AssetCount = FText::GetEmpty();
	if ( NumSelectedAssets == 0 )
	{
		if ( NumAssets == 1 )
		{
			AssetCount = LOCTEXT("AssetCountLabelSingular", "1 item");
		}
		else
		{
			AssetCount = FText::Format( LOCTEXT("AssetCountLabelPlural", "{0} items"), FText::AsNumber(NumAssets) );
		}
	}
	else
	{
		if ( NumAssets == 1 )
		{
			AssetCount = FText::Format( LOCTEXT("AssetCountLabelSingularPlusSelection", "1 item ({0} selected)"), FText::AsNumber(NumSelectedAssets) );
		}
		else
		{
			AssetCount = FText::Format( LOCTEXT("AssetCountLabelPluralPlusSelection", "{0} items ({1} selected)"), FText::AsNumber(NumAssets), FText::AsNumber(NumSelectedAssets) );
		}
	}

	return AssetCount;
}

EVisibility SAssetView::GetEditModeLabelVisibility() const
{
	return IsThumbnailEditMode() ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SAssetView::GetListViewVisibility() const
{
	return GetCurrentViewType() == EAssetViewType::List ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SAssetView::GetTileViewVisibility() const
{
	return GetCurrentViewType() == EAssetViewType::Tile ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SAssetView::GetColumnViewVisibility() const
{
	return GetCurrentViewType() == EAssetViewType::Column ? EVisibility::Visible : EVisibility::Collapsed;
}

void SAssetView::ToggleThumbnailEditMode()
{
	bThumbnailEditMode = !bThumbnailEditMode;
}

void SAssetView::OnThumbnailSizeChanged(EThumbnailSize NewThumbnailSize)
{
	ThumbnailSize = NewThumbnailSize;

	if (FAssetViewInstanceConfig* Config = GetAssetViewConfig())
	{
		Config->ThumbnailSize = (uint8) NewThumbnailSize;
		UAssetViewConfig::Get()->SaveEditorConfig();
	}

	RefreshList();
}

bool SAssetView::IsThumbnailSizeChecked(EThumbnailSize InThumbnailSize) const
{
	return ThumbnailSize == InThumbnailSize;
}

float SAssetView::GetThumbnailScale() const
{
	float BaseScale;
	switch (ThumbnailSize)
	{
	case EThumbnailSize::Tiny:
		BaseScale = 0.1f;
		break;
	case EThumbnailSize::Small:
		BaseScale = 0.25f;
		break;
	case EThumbnailSize::Medium:
		BaseScale = 0.5f;
		break;
	case EThumbnailSize::Large:
		BaseScale = 0.75f;
		break;
	case EThumbnailSize::Huge:
		BaseScale = 1.0f;
		break;
	default:
		BaseScale = 0.5f;
		break;
	}

	return BaseScale * GetTickSpaceGeometry().Scale;
}

bool SAssetView::IsThumbnailScalingAllowed() const
{
	return GetCurrentViewType() != EAssetViewType::Column;
}

float SAssetView::GetTileViewTypeNameHeight() const
{
	float TypeNameHeight = 0;

	if (bShowTypeInTileView)
	{
		TypeNameHeight = 50;
	}
	else
	{
		if (ThumbnailSize == EThumbnailSize::Small)
		{
			TypeNameHeight = 25;
		}
		else if (ThumbnailSize == EThumbnailSize::Medium)
		{
			TypeNameHeight = -5;
		}
		else if (ThumbnailSize > EThumbnailSize::Medium)
		{
			TypeNameHeight = -25;
		}
		else
		{
			TypeNameHeight = -40;
		}
	}
	return TypeNameHeight;
}

float SAssetView::GetSourceControlIconHeight() const
{
	return (float)(ThumbnailSize != EThumbnailSize::Tiny && ISourceControlModule::Get().IsEnabled() && ISourceControlModule::Get().GetProvider().IsAvailable() && !bShowTypeInTileView ? 17.0 : 0.0);
}

float SAssetView::GetListViewItemHeight() const
{
	return (float)(ListViewThumbnailSize + ListViewThumbnailPadding * 2) * FMath::Lerp(MinThumbnailScale, MaxThumbnailScale, GetThumbnailScale());
}

float SAssetView::GetTileViewItemHeight() const
{
	return (((float)TileViewNameHeight + GetTileViewTypeNameHeight()) * FMath::Lerp(MinThumbnailScale, MaxThumbnailScale, GetThumbnailScale())) + GetTileViewItemBaseHeight() * FillScale + GetSourceControlIconHeight();
}

float SAssetView::GetTileViewItemBaseHeight() const
{
	return (float)(TileViewThumbnailSize + TileViewThumbnailPadding * 2) * FMath::Lerp(MinThumbnailScale, MaxThumbnailScale, GetThumbnailScale());
}

float SAssetView::GetTileViewItemWidth() const
{
	return GetTileViewItemBaseWidth() * FillScale;
}

float SAssetView::GetTileViewItemBaseWidth() const //-V524
{
	return (float)( TileViewThumbnailSize + TileViewThumbnailPadding * 2 ) * FMath::Lerp( MinThumbnailScale, MaxThumbnailScale, GetThumbnailScale() );
}

EColumnSortMode::Type SAssetView::GetColumnSortMode(const FName ColumnId) const
{
	for (int32 PriorityIdx = 0; PriorityIdx < EColumnSortPriority::Max; PriorityIdx++)
	{
		const EColumnSortPriority::Type SortPriority = static_cast<EColumnSortPriority::Type>(PriorityIdx);
		if (ColumnId == SortManager.GetSortColumnId(SortPriority))
		{
			return SortManager.GetSortMode(SortPriority);
		}
	}
	return EColumnSortMode::None;
}

EColumnSortPriority::Type SAssetView::GetColumnSortPriority(const FName ColumnId) const
{
	for (int32 PriorityIdx = 0; PriorityIdx < EColumnSortPriority::Max; PriorityIdx++)
	{
		const EColumnSortPriority::Type SortPriority = static_cast<EColumnSortPriority::Type>(PriorityIdx);
		if (ColumnId == SortManager.GetSortColumnId(SortPriority))
		{
			return SortPriority;
		}
	}
	return EColumnSortPriority::Primary;
}

void SAssetView::OnSortColumnHeader(const EColumnSortPriority::Type SortPriority, const FName& ColumnId, const EColumnSortMode::Type NewSortMode)
{
	SortManager.SetSortColumnId(SortPriority, ColumnId);
	SortManager.SetSortMode(SortPriority, NewSortMode);
	SortList();
}

EVisibility SAssetView::IsAssetShowWarningTextVisible() const
{
	return (FilteredAssetItems.Num() > 0 || bQuickFrontendListRefreshRequested) ? EVisibility::Collapsed : EVisibility::HitTestInvisible;
}

FText SAssetView::GetAssetShowWarningText() const
{
	if (AssetShowWarningText.IsSet())
	{
		return AssetShowWarningText.Get();
	}
	
	if (InitialNumAmortizedTasks > 0)
	{
		return LOCTEXT("ApplyingFilter", "Applying filter...");
	}

	FText NothingToShowText, DropText;
	if (ShouldFilterRecursively())
	{
		NothingToShowText = LOCTEXT( "NothingToShowCheckFilter", "No results, check your filter." );
	}

	if ( SourcesData.HasCollections() && !SourcesData.IsDynamicCollection() )
	{
		if (SourcesData.Collections[0].Name.IsNone())
		{
			DropText = LOCTEXT("NoCollectionSelected", "No collection selected.");
		}
		else
		{
			DropText = LOCTEXT("DragAssetsHere", "Drag and drop assets here to add them to the collection.");
		}
	}
	else if ( OnGetItemContextMenu.IsBound() )
	{
		DropText = LOCTEXT( "DropFilesOrRightClick", "Drop files here or right click to create content." );
	}
	
	return NothingToShowText.IsEmpty() ? DropText : FText::Format(LOCTEXT("NothingToShowPattern", "{0}\n\n{1}"), NothingToShowText, DropText);
}

bool SAssetView::HasSingleCollectionSource() const
{
	return ( SourcesData.Collections.Num() == 1 && SourcesData.VirtualPaths.Num() == 0 );
}

void SAssetView::SetUserSearching(bool bInSearching)
{
	if(bUserSearching != bInSearching)
	{
		RequestSlowFullListRefresh();
	}
	bUserSearching = bInSearching;
}

void SAssetView::HandleSettingChanged(FName PropertyName)
{
	if ((PropertyName == GET_MEMBER_NAME_CHECKED(UContentBrowserSettings, DisplayFolders)) ||
		(PropertyName == GET_MEMBER_NAME_CHECKED(UContentBrowserSettings, DisplayEmptyFolders)) ||
		(PropertyName == "DisplayDevelopersFolder") ||
		(PropertyName == "DisplayEngineFolder") ||
		(PropertyName == GET_MEMBER_NAME_CHECKED(UContentBrowserSettings, bDisplayContentFolderSuffix)) ||
		(PropertyName == GET_MEMBER_NAME_CHECKED(UContentBrowserSettings, bDisplayFriendlyNameForPluginFolders)) ||
		(PropertyName == NAME_None))	// @todo: Needed if PostEditChange was called manually, for now
	{
		RequestSlowFullListRefresh();
	}
}

FText SAssetView::GetQuickJumpTerm() const
{
	return FText::FromString(QuickJumpData.JumpTerm);
}

EVisibility SAssetView::IsQuickJumpVisible() const
{
	return (QuickJumpData.JumpTerm.IsEmpty()) ? EVisibility::Collapsed : EVisibility::HitTestInvisible;
}

FSlateColor SAssetView::GetQuickJumpColor() const
{
	return FAppStyle::GetColor((QuickJumpData.bHasValidMatch) ? "InfoReporting.BackgroundColor" : "ErrorReporting.BackgroundColor");
}

void SAssetView::ResetQuickJump()
{
	QuickJumpData.JumpTerm.Empty();
	QuickJumpData.bIsJumping = false;
	QuickJumpData.bHasChangedSinceLastTick = false;
	QuickJumpData.bHasValidMatch = false;
}

FReply SAssetView::HandleQuickJumpKeyDown(const TCHAR InCharacter, const bool bIsControlDown, const bool bIsAltDown, const bool bTestOnly)
{
	// Check for special characters
	if(bIsControlDown || bIsAltDown)
	{
		return FReply::Unhandled();
	}

	// Check for invalid characters
	for(int InvalidCharIndex = 0; InvalidCharIndex < UE_ARRAY_COUNT(INVALID_OBJECTNAME_CHARACTERS) - 1; ++InvalidCharIndex)
	{
		if(InCharacter == INVALID_OBJECTNAME_CHARACTERS[InvalidCharIndex])
		{
			return FReply::Unhandled();
		}
	}

	switch(InCharacter)
	{
	// Ignore some other special characters that we don't want to be entered into the buffer
	case 0:		// Any non-character key press, e.g. f1-f12, Delete, Pause/Break, etc.
				// These should be explicitly not handled so that their input bindings are handled higher up the chain.

	case 8:		// Backspace
	case 13:	// Enter
	case 27:	// Esc
		return FReply::Unhandled();

	default:
		break;
	}

	// Any other character!
	if(!bTestOnly)
	{
		QuickJumpData.JumpTerm.AppendChar(InCharacter);
		QuickJumpData.bHasChangedSinceLastTick = true;
	}

	return FReply::Handled();
}

bool SAssetView::PerformQuickJump(const bool bWasJumping)
{
	auto JumpToNextMatch = [this](const int StartIndex, const int EndIndex) -> bool
	{
		check(StartIndex >= 0);
		check(EndIndex <= FilteredAssetItems.Num());

		for(int NewSelectedItemIndex = StartIndex; NewSelectedItemIndex < EndIndex; ++NewSelectedItemIndex)
		{
			TSharedPtr<FAssetViewItem>& NewSelectedItem = FilteredAssetItems[NewSelectedItemIndex];
			const FString& NewSelectedItemName = NewSelectedItem->GetItem().GetDisplayName().ToString();
			if(NewSelectedItemName.StartsWith(QuickJumpData.JumpTerm, ESearchCase::IgnoreCase))
			{
				ClearSelection(true);
				RequestScrollIntoView(NewSelectedItem);
				ClearSelection();
				// Consider it derived from a keypress because otherwise it won't update the navigation selector
				SetItemSelection(NewSelectedItem, true, ESelectInfo::Type::OnKeyPress);
				return true;
			}
		}

		return false;
	};

	TArray<TSharedPtr<FAssetViewItem>> SelectedItems = GetSelectedViewItems();
	TSharedPtr<FAssetViewItem> SelectedItem = (SelectedItems.Num()) ? SelectedItems[0] : nullptr;

	// If we have a selection, and we were already jumping, first check to see whether 
	// the current selection still matches the quick-jump term; if it does, we do nothing
	if(bWasJumping && SelectedItem.IsValid())
	{
		const FString& SelectedItemName = SelectedItem->GetItem().GetDisplayName().ToString();
		if(SelectedItemName.StartsWith(QuickJumpData.JumpTerm, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}

	// We need to move on to the next match in FilteredAssetItems that starts with the given quick-jump term
	const int SelectedItemIndex = (SelectedItem.IsValid()) ? FilteredAssetItems.Find(SelectedItem) : INDEX_NONE;
	const int StartIndex = (SelectedItemIndex == INDEX_NONE) ? 0 : SelectedItemIndex + 1;
	
	bool ValidMatch = JumpToNextMatch(StartIndex, FilteredAssetItems.Num());
	if(!ValidMatch && StartIndex > 0)
	{
		// If we didn't find a match, we need to loop around and look again from the start (assuming we weren't already)
		return JumpToNextMatch(0, StartIndex);
	}

	return ValidMatch;
}

void SAssetView::ResetColumns()
{
	for (const SHeaderRow::FColumn &Column : ColumnView->GetHeaderRow()->GetColumns())
	{
		ColumnView->GetHeaderRow()->SetShowGeneratedColumn(Column.ColumnId, !DefaultHiddenColumnNames.Contains(Column.ColumnId.ToString()));
	}

	// This is set after updating the column visibilties, because SetShowGeneratedColumn calls OnHiddenColumnsChanged indirectly which can mess up the list
	HiddenColumnNames = DefaultHiddenColumnNames;
	ColumnView->GetHeaderRow()->RefreshColumns();
	ColumnView->RebuildList();
}

void SAssetView::ExportColumns()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();

	const void* ParentWindowWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);

	const FText Title = LOCTEXT("ExportToCSV", "Export columns as CSV...");
	const FString FileTypes = TEXT("Data Table CSV (*.csv)|*.csv");

	TArray<FString> OutFilenames;
	DesktopPlatform->SaveFileDialog(
		ParentWindowWindowHandle,
		Title.ToString(),
		TEXT(""),
		TEXT("Report.csv"),
		FileTypes,
		EFileDialogFlags::None,
		OutFilenames
	);

	if (OutFilenames.Num() > 0)
	{
		const TIndirectArray<SHeaderRow::FColumn>& Columns = ColumnView->GetHeaderRow()->GetColumns();

		TArray<FName> ColumnNames;
		for (const SHeaderRow::FColumn& Column : Columns)
		{
			ColumnNames.Add(Column.ColumnId);
		}

		FString SaveString;
		SortManager.ExportColumnsToCSV(FilteredAssetItems, ColumnNames, CustomColumns, SaveString);

		FFileHelper::SaveStringToFile(SaveString, *OutFilenames[0]);
	}
}

void SAssetView::OnHiddenColumnsChanged()
{
	// Early out if this is called before creation (due to loading config etc)
	if(!ColumnView)
	{
		return;
	}

	// We can't directly update the hidden columns list, because some columns maybe hidden, but not created yet
	TArray<FName> NewHiddenColumns = ColumnView->GetHeaderRow()->GetHiddenColumnIds();

	// So instead for each column that currently exists, we update its visibility state in the HiddenColumnNames array
	for (const SHeaderRow::FColumn& Column : ColumnView->GetHeaderRow()->GetColumns())
	{
		const bool bIsColumnVisible = NewHiddenColumns.Contains(Column.ColumnId);

		if(bIsColumnVisible)
		{
			HiddenColumnNames.AddUnique(Column.ColumnId.ToString());
		}
		else
		{
			HiddenColumnNames.Remove(Column.ColumnId.ToString());
		}
	}
	
	if (FAssetViewInstanceConfig* Config = GetAssetViewConfig())
	{
		Config->HiddenColumns.Reset();
		Algo::Transform(HiddenColumnNames, Config->HiddenColumns, [](const FString& Str) { return FName(*Str); });

		UAssetViewConfig::Get()->SaveEditorConfig();
	}
}

bool SAssetView::ShouldColumnGenerateWidget(const FString ColumnName) const
{
	return !HiddenColumnNames.Contains(ColumnName);
}

void SAssetView::ForceShowPluginFolder(bool bEnginePlugin)
{
	if (bEnginePlugin && !IsShowingEngineContent())
	{
		ToggleShowEngineContent();
	}

	if (!IsShowingPluginContent())
	{
		ToggleShowPluginContent();
	}
}

void SAssetView::OverrideShowEngineContent()
{
	if (!IsShowingEngineContent())
	{
		ToggleShowEngineContent();
	}

}

void SAssetView::OverrideShowDeveloperContent()
{
	if (!IsShowingDevelopersContent())
	{
		ToggleShowDevelopersContent();
	}
}

void SAssetView::OverrideShowPluginContent()
{
	if (!IsShowingPluginContent())
	{
		ToggleShowPluginContent();
	}
}

void SAssetView::OverrideShowLocalizedContent()
{
	if (!IsShowingLocalizedContent())
	{
		ToggleShowLocalizedContent();
	}
}

void SAssetView::HandleItemDataUpdated(TArrayView<const FContentBrowserItemDataUpdate> InUpdatedItems)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(SAssetView::HandleItemDataUpdated);

	if (InUpdatedItems.Num() == 0)
	{
		return;
	}

	const double HandleItemDataUpdatedStartTime = FPlatformTime::Seconds();

	UContentBrowserDataSubsystem* ContentBrowserData = IContentBrowserDataModule::Get().GetSubsystem();

	TArray<FContentBrowserDataCompiledFilter> CompiledDataFilters;
	if (SourcesData.IsIncludingVirtualPaths())
	{
		const bool bInvalidateFilterCache = false;
		const FContentBrowserDataFilter DataFilter = CreateBackendDataFilter(bInvalidateFilterCache);

		static const FName RootPath = "/";
		const TArrayView<const FName> DataSourcePaths = SourcesData.HasVirtualPaths() ? MakeArrayView(SourcesData.VirtualPaths) : MakeArrayView(&RootPath, 1);
		for (const FName& DataSourcePath : DataSourcePaths)
		{
			FContentBrowserDataCompiledFilter& CompiledDataFilter = CompiledDataFilters.AddDefaulted_GetRef();
			ContentBrowserData->CompileFilter(DataSourcePath, DataFilter, CompiledDataFilter);
		}
	}

	bool bRefreshView = false;
	TSet<TSharedPtr<FAssetViewItem>> ItemsPendingInplaceFrontendFilter;

	auto AddItem = [this, &ItemsPendingInplaceFrontendFilter](const FContentBrowserItemKey& InItemDataKey, FContentBrowserItemData&& InItemData)
	{
		TSharedPtr<FAssetViewItem>& ItemToUpdate = AvailableBackendItems.FindOrAdd(InItemDataKey);
		if (ItemToUpdate)
		{
			// Update the item
			ItemToUpdate->AppendItemData(MoveTemp(InItemData));

			// Update the custom column data
			ItemToUpdate->CacheCustomColumns(CustomColumns, true, true, true);

			// This item was modified, so put it in the list of items to be in-place re-tested against the active frontend filter (this can avoid a costly re-sort of the view)
			// If the item can't be queried in-place (because the item isn't in the view) then it will be added to ItemsPendingPriorityFilter instead
			ItemsPendingInplaceFrontendFilter.Add(ItemToUpdate);
		}
		else
		{
			ItemToUpdate = MakeShared<FAssetViewItem>(MoveTemp(InItemData));

			// This item is new so put it in the pending set to be processed over time
			ItemsPendingFrontendFilter.Add(ItemToUpdate);
		}
	};

	auto RemoveItem = [this, &bRefreshView, &ItemsPendingInplaceFrontendFilter](const FContentBrowserItemKey& InItemDataKey, const FContentBrowserItemData& InItemData)
	{
		const uint32 ItemDataKeyHash = GetTypeHash(InItemDataKey);

		if (const TSharedPtr<FAssetViewItem>* ItemToRemovePtr = AvailableBackendItems.FindByHash(ItemDataKeyHash, InItemDataKey))
		{
			TSharedPtr<FAssetViewItem> ItemToRemove = *ItemToRemovePtr;
			check(ItemToRemove);

			// Only fully remove this item if every sub-item is removed (items become invalid when empty)
			ItemToRemove->RemoveItemData(InItemData);
			if (ItemToRemove->GetItem().IsValid())
			{
				return;
			}

			AvailableBackendItems.RemoveByHash(ItemDataKeyHash, InItemDataKey);

			const uint32 ItemToRemoveHash = GetTypeHash(ItemToRemove);

			// Also ensure this item has been removed from the pending filter lists and the current list view data
			FilteredAssetItems.RemoveSingle(ItemToRemove);
			ItemsPendingPriorityFilter.RemoveByHash(ItemToRemoveHash, ItemToRemove);
			ItemsPendingFrontendFilter.RemoveByHash(ItemToRemoveHash, ItemToRemove);
			ItemsPendingInplaceFrontendFilter.RemoveByHash(ItemToRemoveHash, ItemToRemove);

			// Need to refresh manually after removing items, as adding relies on the pending filter lists to trigger this
			bRefreshView = true;
		}
	};

	auto GetBackendFilterCompliantItem = [this, &CompiledDataFilters](const FContentBrowserItemData& InItemData, bool& bOutPassFilter)
	{
		UContentBrowserDataSource* ItemDataSource = InItemData.GetOwnerDataSource();
		FContentBrowserItemData ItemData = InItemData;
		for (const FContentBrowserDataCompiledFilter& DataFilter : CompiledDataFilters)
		{
			// We only convert the item if this is the right filter for the data source
			if (ItemDataSource->ConvertItemForFilter(ItemData, DataFilter))
			{
				bOutPassFilter = ItemDataSource->DoesItemPassFilter(ItemData, DataFilter);

				return ItemData;
			}

			if (ItemDataSource->DoesItemPassFilter(ItemData, DataFilter))
			{
				bOutPassFilter = true;
				return ItemData;
			}
		}

		bOutPassFilter = false;
		return ItemData;
	};

	// Process the main set of updates
	for (const FContentBrowserItemDataUpdate& ItemDataUpdate : InUpdatedItems)
	{
		bool bItemPassFilter = false;
		FContentBrowserItemData ItemData = GetBackendFilterCompliantItem(ItemDataUpdate.GetItemData(), bItemPassFilter);
		const FContentBrowserItemKey ItemDataKey(ItemData);

		switch (ItemDataUpdate.GetUpdateType())
		{
		case EContentBrowserItemUpdateType::Added:
			if (bItemPassFilter)
			{
				AddItem(ItemDataKey, MoveTemp(ItemData));
			}
			break;

		case EContentBrowserItemUpdateType::Modified:
			if (bItemPassFilter)
			{
				AddItem(ItemDataKey, MoveTemp(ItemData));
			}
			else
			{
				RemoveItem(ItemDataKey, ItemData);
			}
			break;

		case EContentBrowserItemUpdateType::Moved:
			{
				const FContentBrowserItemData OldMinimalItemData(ItemData.GetOwnerDataSource(), ItemData.GetItemType(), ItemDataUpdate.GetPreviousVirtualPath(), NAME_None, FText(), nullptr);
				const FContentBrowserItemKey OldItemDataKey(OldMinimalItemData);
				RemoveItem(OldItemDataKey, OldMinimalItemData);

				if (bItemPassFilter)
				{
					AddItem(ItemDataKey, MoveTemp(ItemData));
				}
				else
				{
					checkAssetList(!AvailableBackendItems.Contains(ItemDataKey));
				}
			}
			break;

		case EContentBrowserItemUpdateType::Removed:
			RemoveItem(ItemDataKey, ItemData);
			break;

		default:
			checkf(false, TEXT("Unexpected EContentBrowserItemUpdateType!"));
			break;
		}
	}

	// Now patch in the in-place frontend filter requests (if possible)
	if (ItemsPendingInplaceFrontendFilter.Num() > 0)
	{
		FAssetViewFrontendFilterHelper FrontendFilterHelper(this);
		const bool bRunQueryFilter = OnShouldFilterAsset.IsBound() || OnShouldFilterItem.IsBound();

		for (auto It = FilteredAssetItems.CreateIterator(); It && ItemsPendingInplaceFrontendFilter.Num() > 0; ++It)
		{
			const TSharedPtr<FAssetViewItem> ItemToFilter = *It;

			if (ItemsPendingInplaceFrontendFilter.Remove(ItemToFilter) > 0)
			{
				bool bRemoveItem = false;

				// Run the query filter if required
				if (bRunQueryFilter)
				{
					const bool bPassedBackendFilter = FrontendFilterHelper.DoesItemPassQueryFilter(ItemToFilter);
					if (!bPassedBackendFilter)
					{
						bRemoveItem = true;
						AvailableBackendItems.Remove(FContentBrowserItemKey(ItemToFilter->GetItem()));
					}
				}

				// Run the frontend filter
				if (!bRemoveItem)
				{
					const bool bPassedFrontendFilter = FrontendFilterHelper.DoesItemPassFrontendFilter(ItemToFilter);
					if (!bPassedFrontendFilter)
					{
						bRemoveItem = true;
					}
				}

				// Remove this item?
				if (bRemoveItem)
				{
					bRefreshView = true;
					It.RemoveCurrent();
				}
			}
		}

		// Do we still have items that could not be in-place filtered?
		// If so, add them to ItemsPendingPriorityFilter so they are processed into the view ASAP
		if (ItemsPendingInplaceFrontendFilter.Num() > 0)
		{
			ItemsPendingPriorityFilter.Append(MoveTemp(ItemsPendingInplaceFrontendFilter));
			ItemsPendingInplaceFrontendFilter.Reset();
		}
	}

	if (bRefreshView)
	{
		RefreshList();
	}

	UE_LOG(LogContentBrowser, VeryVerbose, TEXT("AssetView - HandleItemDataUpdated completed in %0.4f seconds for %d items (%d available items)"), FPlatformTime::Seconds() - HandleItemDataUpdatedStartTime, InUpdatedItems.Num(), AvailableBackendItems.Num());
}

void SAssetView::HandleItemDataDiscoveryComplete()
{
	if (bPendingSortFilteredItems)
	{
		// If we have a sort pending, then force this to happen next frame now that discovery has finished
		LastSortTime = 0;
	}
}

void SAssetView::SetFilterBar(TSharedPtr<SFilterList> InFilterBar)
{
	FilterBar = InFilterBar;
}

void SAssetView::OnCompleteFiltering(double InAmortizeDuration)
{
	CurrentFrontendFilterTelemetry.AmortizeDuration = InAmortizeDuration;
	CurrentFrontendFilterTelemetry.bCompleted = true;
	FTelemetryRouter::Get().ProvideTelemetry(CurrentFrontendFilterTelemetry);
	CurrentFrontendFilterTelemetry = {};
}

void SAssetView::OnInterruptFiltering()
{
	if (CurrentFrontendFilterTelemetry.FilterSessionCorrelationGuid.IsValid())
	{
		CurrentFrontendFilterTelemetry.AmortizeDuration = FPlatformTime::Seconds() - AmortizeStartTime;
		CurrentFrontendFilterTelemetry.bCompleted = false;
		FTelemetryRouter::Get().ProvideTelemetry(CurrentFrontendFilterTelemetry);
		CurrentFrontendFilterTelemetry = {};
	}
}

void SAssetView::OnInteractDuringFiltering()
{
	if (CurrentFrontendFilterTelemetry.FilterSessionCorrelationGuid.IsValid() && !CurrentFrontendFilterTelemetry.TimeUntilInteraction.IsSet())
	{
		CurrentFrontendFilterTelemetry.TimeUntilInteraction = FPlatformTime::Seconds() - AmortizeStartTime;
	}
}

#undef checkAssetList
#undef ASSET_VIEW_PARANOIA_LIST_CHECKS

#undef LOCTEXT_NAMESPACE
