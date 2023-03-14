// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAssetPlacementPalette.h"
#include "AssetPlacementPaletteItem.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SScrollBorder.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Input/SCheckBox.h"
#include "Styling/AppStyle.h"
#include "AssetThumbnail.h"

#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "PropertyCustomizationHelpers.h"
#include "AssetRegistry/AssetData.h"
#include "AssetSelection.h"
#include "Editor.h"
#include "Widgets/Layout/SScrollBox.h"

#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Input/SSearchBox.h"
#include "AssetPlacementEdModeStyle.h"
#include "AssetPlacementSettings.h"
#include "Subsystems/PlacementSubsystem.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Modes/PlacementModeSubsystem.h"

#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "Modules/ModuleManager.h"
#include "SAssetDropTarget.h"
#include "PlacementPaletteAsset.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "PlacementPaletteItem.h"
#include "Factories/AssetFactoryInterface.h"
#include "Instances/EditorPlacementSettings.h"

#if !UE_IS_COOKED_EDITOR
#include "AssetPlacementEdModeModule.h"
#endif

#define LOCTEXT_NAMESPACE "AssetPlacementMode"

////////////////////////////////////////////////
// SPlacementPalette
////////////////////////////////////////////////
void SAssetPlacementPalette::Construct(const FArguments& InArgs)
{
	ItemDetailsWidget = InArgs._ItemDetailsView;
	bItemsNeedRebuild = false;
	bIsRebuildTimerRegistered = false;
	bIsRefreshTimerRegistered = false;

	UICommandList = MakeShared<FUICommandList>();

	// Size of the thumbnail pool should be large enough to show a reasonable amount of Placement assets on screen at once,
	// otherwise some thumbnail images will appear duplicated.
	ThumbnailPool = MakeShared<FAssetThumbnailPool>(64);

	TypeFilter = MakeShared<PlacementTypeTextFilter>(PlacementTypeTextFilter::FItemToStringArray::CreateSP(this, &SAssetPlacementPalette::GetPaletteItemFilterString));

	const FText BlankText = FText::GetEmpty();

	// Load initial settings for the palette widget to use
	if (UPlacementModeSubsystem* PlacementModeSubystem = GEditor->GetEditorSubsystem<UPlacementModeSubsystem>())
	{
		if (const UAssetPlacementSettings* PlacementSettings = PlacementModeSubystem->GetModeSettingsObject())
		{
			bIsMirroringContentBrowser = !PlacementSettings->bUseContentBrowserSelection;
			OnSetPaletteAsset(PlacementSettings->GetActivePalettePath().TryLoad());
		}
	}
	SetupContentBrowserMirroring(!bIsMirroringContentBrowser);

	TSharedPtr<SWidget> TopBar = 
		SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("DetailsView.CategoryTop"))
		.BorderBackgroundColor(FLinearColor(.6f, .6f, .6f, 1.0f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Fill)
			[
				SNew(SCheckBox)
				.Type(ESlateCheckBoxType::Type::ToggleButton)
				.IsChecked(bIsMirroringContentBrowser ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
				.Style(&FAssetPlacementEdModeStyle::Get().GetWidgetStyle<FCheckBoxStyle>("ToggleButtonCheckBox"))
				.Visibility(this, &SAssetPlacementPalette::GetContentBrowserMirrorVisibility)
				.OnCheckStateChanged(this, &SAssetPlacementPalette::OnContentBrowserMirrorButtonClicked)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Placement_ToggleContentBrowserMirroring", "Mirror Content Browser Selection"))
					.Justification(ETextJustify::Type::Center)
					.TextStyle(&FAssetPlacementEdModeStyle::Get().GetWidgetStyle<FTextBlockStyle>("ButtonText"))
					.ToolTipText(LOCTEXT("Placement_ToggleContentBrowserMirroring_ToolTip", "Toggles palette to mirror the active content browser selection."))
				]
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SObjectPropertyEntryBox)
				.AllowedClass(UPlacementPaletteAsset::StaticClass())
				.ObjectPath(this, &SAssetPlacementPalette::GetPalettePath)
				.OnObjectChanged(this, &SAssetPlacementPalette::OnSetPaletteAsset)
				.ThumbnailPool(ThumbnailPool)
				.Visibility(this, &SAssetPlacementPalette::GetPaletteAssetPropertyBoxVisible)
				.CustomContentSlot()
				[
					SNew(SBox)
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.AutoWidth()
						[
							PropertyCustomizationHelpers::MakeClearButton(FSimpleDelegate::CreateSP(this, &SAssetPlacementPalette::OnResetPaletteAssetClicked), LOCTEXT("ResetPaletteAssetTooltip", "Clear the current palette asset, and use the user's local palette."), TAttribute<bool>::CreateLambda([this]() { return PalettePath.IsValid(); }))
						]
						+ SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.AutoWidth()
						.Padding(2.0f, 0.0f)
						[
							PropertyCustomizationHelpers::MakeSaveButton(FSimpleDelegate::CreateSP(this, &SAssetPlacementPalette::OnSavePaletteAssetClicked), LOCTEXT("SaveAssetPaletteTooltip", "Save changes to the current palette asset"), TAttribute<bool>::CreateLambda([this]() { return PalettePath.IsValid() || (PaletteItems.Num() > 0); }))
						]
					]
				]
			]

			+ SVerticalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(.75f)
				[
					SAssignNew(SearchBoxPtr, SSearchBox)
					.HintText(LOCTEXT("SearchPlacementPaletteHint", "Search Palette"))
					.OnTextChanged(this, &SAssetPlacementPalette::OnSearchTextChanged)
				]

				// View Options
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SComboButton)
					.ForegroundColor(FSlateColor::UseForeground())
					.ButtonStyle(FAppStyle::Get(), "ToggleButton")
					.OnGetMenuContent(this, &SAssetPlacementPalette::GetViewOptionsMenuContent)
					.ButtonContent()
					[
						SNew(SBox)
						.VAlign(VAlign_Center)
						[
							SNew(SImage)
							.Image(FAppStyle::Get().GetBrush("GenericViewButton"))
						]
					]
				]
			]
		];

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.HAlign(HAlign_Fill)
		.AutoHeight()
		[
			TopBar.ToSharedRef()
		]

		+ SVerticalBox::Slot()
		.FillHeight(1.0)
		[
			SNew(SAssetDropTarget)
			.OnAreAssetsAcceptableForDrop(this, &SAssetPlacementPalette::OnAreAssetsValidForDrop)
			.OnAssetsDropped(this, &SAssetPlacementPalette::HandlePlacementDropped)
			.bSupportsMultiDrop(true)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SBox)
					.HeightOverride_Lambda([TopBarWeakPtr = TWeakPtr<SWidget>(TopBar)]()
					{
						if (TSharedPtr<SWidget> ParentWidgetPin = TopBarWeakPtr.Pin())
						{
							return FOptionalSize(ParentWidgetPin->GetTickSpaceGeometry().GetLocalSize().Y - 1);
						}
						return FOptionalSize();
					})
				]

				+ SOverlay::Slot()
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[
						CreatePaletteViews()
					]
				]

				+ SOverlay::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Visibility(this, &SAssetPlacementPalette::GetDropHintTextVisibility)
					.Text(LOCTEXT("Placement_AddPlacementMesh", "Drop Assets Here"))
					.TextStyle(FAppStyle::Get(), "HintText")
				]
			]
		]
	];

	UpdatePalette(true);
}

SAssetPlacementPalette::~SAssetPlacementPalette()
{
}

FReply SAssetPlacementPalette::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (UICommandList->ProcessCommandBindings(InKeyEvent))
	{
		return FReply::Handled();
	}

	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

void SAssetPlacementPalette::UpdatePalette(bool bRebuildItems)
{
	bItemsNeedRebuild |= bRebuildItems;

	if (!bIsRebuildTimerRegistered)
	{
		bIsRebuildTimerRegistered = true;
		RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SAssetPlacementPalette::UpdatePaletteItems));
	}
}

void SAssetPlacementPalette::RefreshPalette()
{
	// Do not register the refresh timer if we're pending a rebuild; rebuild should cause the palette to refresh
	if (!bIsRefreshTimerRegistered && !bIsRebuildTimerRegistered)
	{
		bIsRefreshTimerRegistered = true;
		RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SAssetPlacementPalette::RefreshPaletteItems));
	}
}

void SAssetPlacementPalette::RefreshActivePaletteViewWidget()
{
	if (ActiveViewMode == EViewMode::Thumbnail)
	{
		TileViewWidget->RequestListRefresh();
	}
	else
	{
		TreeViewWidget->RequestTreeRefresh();
	}
}

void SAssetPlacementPalette::AddPlacementType(const FAssetData& AssetData)
{
	// Try to add the item to the mode's palette
	if (UPlacementModeSubsystem* PlacementModeSubsystem = GEditor->GetEditorSubsystem<UPlacementModeSubsystem>())
	{
		UPlacementPaletteClient* PlacementInfo = PlacementModeSubsystem->GetMutableModeSettingsObject()->AddClientToActivePalette(AssetData);
		if (!PlacementInfo || !PlacementInfo->AssetPath.IsValid())
		{
			return;
		}

		// Try to load the asset async so it's ready to place
		UAssetManager::GetStreamableManager().RequestAsyncLoad(AssetData.ToSoftObjectPath());

		PaletteItems.Add(MakeShared<FAssetPlacementPaletteItemModel>(AssetData, PlacementInfo, SharedThis(this), ThumbnailPool));
	}
}

void SAssetPlacementPalette::ClearPalette()
{
	if (UPlacementModeSubsystem* ModeSubsystem = GEditor->GetEditorSubsystem<UPlacementModeSubsystem>())
	{
		ModeSubsystem->GetMutableModeSettingsObject()->ClearActivePaletteItems();
	}

	if (TSharedPtr<IDetailsView> PinnedItemDetails = ItemDetailsWidget.Pin())
	{
		PinnedItemDetails->SetObject(nullptr);
	}

	PaletteItems.Empty();
	FilteredItems.Empty();
}

void SAssetPlacementPalette::OnClearPalette()
{
	ClearPalette();
	UpdatePalette(true);
}

void SAssetPlacementPalette::SetPaletteToAssetDataList(TArrayView<const FAssetData> InAssetDatas)
{
	ClearPalette();
	for (const FAssetData& SelectedAsset : InAssetDatas)
	{
		AddPlacementType(SelectedAsset);
	}
}

void SAssetPlacementPalette::OnRemoveSelectedItemsFromPalette()
{
	UPlacementModeSubsystem* ModeSubsystem = GEditor->GetEditorSubsystem<UPlacementModeSubsystem>();
	TSharedPtr<IDetailsView> PinnedItemDetails = ItemDetailsWidget.Pin();
	TArray<TWeakObjectPtr<UObject>> SelectedObjects;
	if (PinnedItemDetails)
	{
		SelectedObjects = PinnedItemDetails->GetSelectedObjects();
	}

	for (FPlacementPaletteItemModelPtr& PaletteItem : GetActiveViewWidget()->GetSelectedItems())
	{
		const FAssetPlacementUIInfoPtr PaletteInfo = PaletteItem->GetTypeUIInfo();
		
		if (ModeSubsystem)
		{
			ModeSubsystem->GetMutableModeSettingsObject()->RemoveClientFromActivePalette(PaletteInfo->AssetData);
		}

		SelectedObjects.Remove(PaletteInfo->SettingsObject);
	}

	// Update the palette's view to the updated palette
	if (ModeSubsystem)
	{
		SetPaletteItems(ModeSubsystem->GetModeSettingsObject()->GetActivePaletteItems());
	}

	// Reset the item details to valid objects
	if (PinnedItemDetails)
	{
		PinnedItemDetails->SetObjects(SelectedObjects);
	}
}

TSharedRef<SWidgetSwitcher> SAssetPlacementPalette::CreatePaletteViews()
{
	const FText BlankText = FText::GetEmpty();

	// Tile View Widget
	SAssignNew(TileViewWidget, SPlacementTypeTileView)
		.ListItemsSource(&FilteredItems)
		.OnGenerateTile(this, &SAssetPlacementPalette::GenerateTile)
		.OnContextMenuOpening(this, &SAssetPlacementPalette::ConstructPlacementTypeContextMenu)
		.SelectionMode(ESelectionMode::Multi)
		.OnSelectionChanged(this, &SAssetPlacementPalette::OnPaletteSelectionChanged)
		.ItemHeight(this, &SAssetPlacementPalette::GetScaledThumbnailSize)
		.ItemWidth(this, &SAssetPlacementPalette::GetScaledThumbnailSize)
		.ItemAlignment(EListItemAlignment::LeftAligned);

	// Tree View Widget
	SAssignNew(TreeViewWidget, SPlacementTypeTreeView)
		.TreeItemsSource(&FilteredItems)
		.OnGenerateRow(this, &SAssetPlacementPalette::TreeViewGenerateRow)
		.OnGetChildren(this, &SAssetPlacementPalette::TreeViewGetChildren)
		.SelectionMode(ESelectionMode::Multi)
		.OnSelectionChanged(this, &SAssetPlacementPalette::OnPaletteSelectionChanged)
		.OnContextMenuOpening(this, &SAssetPlacementPalette::ConstructPlacementTypeContextMenu)
		.HeaderRow
		(
			SAssignNew(TreeViewHeaderRow, SHeaderRow)
			// Type
			+ SHeaderRow::Column(AssetPlacementPaletteTreeColumns::ColumnID_Type)
			.HeaderContentPadding(FMargin(10, 1, 0, 1))
			.SortMode(this, &SAssetPlacementPalette::GetMeshColumnSortMode)
			.OnSort(this, &SAssetPlacementPalette::OnTypeColumnSortModeChanged)
			.DefaultLabel(this, &SAssetPlacementPalette::GetTypeColumnHeaderText)
			.FillWidth(5.f)
		);

	// View Mode Switcher
	SAssignNew(WidgetSwitcher, SWidgetSwitcher);

	// Thumbnail View
	WidgetSwitcher->AddSlot((uint8)EViewMode::Thumbnail)
	[
		SNew(SScrollBorder, TileViewWidget.ToSharedRef())
		.Content()
		[
			TileViewWidget.ToSharedRef()
		]
	];

	// Tree View
	WidgetSwitcher->AddSlot((uint8)EViewMode::Tree)
	[
		SNew(SScrollBorder, TreeViewWidget.ToSharedRef())
		.Style(&FAssetPlacementEdModeStyle::Get().GetWidgetStyle<FScrollBorderStyle>("FoliageEditMode.TreeView.ScrollBorder"))
		.Content()
		[
			TreeViewWidget.ToSharedRef()
		]
	];

	WidgetSwitcher->SetActiveWidgetIndex((uint8)ActiveViewMode);

	return WidgetSwitcher.ToSharedRef();
}

void SAssetPlacementPalette::GetPaletteItemFilterString(FPlacementPaletteItemModelPtr PaletteItemModel, TArray<FString>& OutArray) const
{
	OutArray.Add(PaletteItemModel->GetDisplayFName().ToString());
}

void SAssetPlacementPalette::OnSearchTextChanged(const FText& InFilterText)
{
	TypeFilter->SetRawFilterText(InFilterText);
	SearchBoxPtr->SetError(TypeFilter->GetFilterErrorText());
	UpdatePalette();
}

bool SAssetPlacementPalette::ShouldFilterAsset(const FAssetData& InAssetData)
{
	UClass* Class = InAssetData.GetClass();

	if (Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists | CLASS_NotPlaceable))
	{
		return true;
	}

	if (UPlacementSubsystem* PlacementSubsystem = GEditor->GetEditorSubsystem<UPlacementSubsystem>())
	{
		return (PlacementSubsystem->FindAssetFactoryFromAssetData(InAssetData) != nullptr);
	}

	return true;
}

void SAssetPlacementPalette::OnResetPaletteAssetClicked()
{
	OnSetPaletteAsset(FAssetData());
	UpdatePalette(true);
}

void SAssetPlacementPalette::OnSavePaletteAssetClicked()
{
	if (UPlacementModeSubsystem* PlacementModeSubsystem = GEditor->GetEditorSubsystem<UPlacementModeSubsystem>())
	{
		PlacementModeSubsystem->GetMutableModeSettingsObject()->SaveActivePalette();

		// If we saved from a temporary/user palette, update the active palette now
		if (!PalettePath.IsValid())
		{
			OnSetPaletteAsset(PlacementModeSubsystem->GetModeSettingsObject()->GetActivePalettePath().TryLoad());
		}
	}
}

EVisibility SAssetPlacementPalette::GetContentBrowserMirrorVisibility() const
{
	return GetPalettePath().IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SAssetPlacementPalette::GetPaletteAssetPropertyBoxVisible() const
{
#if !UE_IS_COOKED_EDITOR
	if (AssetPlacementEdModeUtil::AreInstanceWorkflowsEnabled())
	{
		return EVisibility::Visible;
	}
#endif
	
	return EVisibility::Collapsed;
}

void SAssetPlacementPalette::OnContentBrowserMirrorButtonClicked(ECheckBoxState InState)
{
	SetupContentBrowserMirroring((InState == ECheckBoxState::Checked));

	if (UPlacementModeSubsystem* PlacementModeSubsystem = GEditor->GetEditorSubsystem<UPlacementModeSubsystem>())
	{
		PlacementModeSubsystem->GetMutableModeSettingsObject()->bUseContentBrowserSelection = bIsMirroringContentBrowser;
	}
}

void SAssetPlacementPalette::OnContentBrowserSelectionChanged(const TArray<FAssetData>& NewSelectedAssets, bool bIsPrimaryBrowser)
{
	if (bIsPrimaryBrowser)
	{
		SetPaletteToAssetDataList(NewSelectedAssets);
		UpdatePalette(true);
	}
}

void SAssetPlacementPalette::SetupContentBrowserMirroring(bool bInMirrorContentBrowser)
{
	bool bCanMirrorContentBrowser = bInMirrorContentBrowser;
	if (PalettePath.IsValid())
	{
		bCanMirrorContentBrowser = false;
	}

	if (bIsMirroringContentBrowser != bCanMirrorContentBrowser)
	{
		if (FContentBrowserModule* ContentBrowserModule = FModuleManager::GetModulePtr<FContentBrowserModule>("ContentBrowser"))
		{
			if (bInMirrorContentBrowser && bCanMirrorContentBrowser)
			{
				TArray<FAssetData> SelectedAssetDatas;
				ContentBrowserModule->Get().GetSelectedAssets(SelectedAssetDatas);
				OnContentBrowserSelectionChanged(SelectedAssetDatas, true);
				ContentBrowserModule->GetOnAssetSelectionChanged().AddSP(SharedThis(this), &SAssetPlacementPalette::OnContentBrowserSelectionChanged);
			}
			else
			{
				ContentBrowserModule->GetOnAssetSelectionChanged().RemoveAll(this);
			}
		}
	}
	bIsMirroringContentBrowser = bInMirrorContentBrowser;
}

FString SAssetPlacementPalette::GetPalettePath() const
{
	return PalettePath.ToString();
}

void SAssetPlacementPalette::OnSetPaletteAsset(const FAssetData& InAssetData)
{
	FSoftObjectPath NewAssetPath = InAssetData.ToSoftObjectPath();
	if ((NewAssetPath != PalettePath) || !InAssetData.IsValid())
	{
		UPlacementPaletteAsset* PaletteAsset = Cast<UPlacementPaletteAsset>(InAssetData.GetAsset());
		if (UPlacementModeSubsystem* PlacementModeSubsystem = GEditor->GetEditorSubsystem<UPlacementModeSubsystem>())
		{
			PlacementModeSubsystem->GetMutableModeSettingsObject()->SetPaletteAsset(PaletteAsset);

			PalettePath = NewAssetPath;
			SetPaletteItems(PlacementModeSubsystem->GetModeSettingsObject()->GetActivePaletteItems());
		}
	}
}

void SAssetPlacementPalette::SetPaletteItems(TArrayView<const TObjectPtr<UPlacementPaletteClient>> InPaletteItems)
{
	PaletteItems.Empty();
	FilteredItems.Empty();
	if (TSharedPtr<IDetailsView> PinnedDetailsView = ItemDetailsWidget.Pin())
	{
		PinnedDetailsView->SetObject(nullptr);
	}

	FAssetRegistryModule& AssetRegistry = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	TArray<FSoftObjectPath> AssetsToLoad;
	for (const UPlacementPaletteClient* PaletteItem : InPaletteItems)
	{
		if (PaletteItem)
		{
			FAssetData AssetData = AssetRegistry.Get().GetAssetByObjectPath(PaletteItem->AssetPath.GetWithoutSubPath());
			if (AssetData.IsValid())
			{
				PaletteItems.Add(MakeShared<FAssetPlacementPaletteItemModel>(AssetData, PaletteItem, SharedThis(this), ThumbnailPool));
				AssetsToLoad.Emplace(PaletteItem->AssetPath);
			}
		}
	}

	// Try to load the assets async so they're ready to place later
	UAssetManager::GetStreamableManager().RequestAsyncLoad(AssetsToLoad);

	// Update the palette view to match contained items
	UpdatePalette(true);
}

void SAssetPlacementPalette::SetViewMode(EViewMode NewViewMode)
{
	if (ActiveViewMode != NewViewMode)
	{
		ActiveViewMode = NewViewMode;
		WidgetSwitcher->SetActiveWidgetIndex((uint8)ActiveViewMode);
		
		RefreshActivePaletteViewWidget();
	}
}

bool SAssetPlacementPalette::IsActiveViewMode(EViewMode ViewMode) const
{
	return ActiveViewMode == ViewMode;
}

void SAssetPlacementPalette::ToggleShowTooltips()
{
	bShowFullTooltips = !bShowFullTooltips;
}

bool SAssetPlacementPalette::ShouldShowTooltips() const
{
	return bShowFullTooltips;
}

FText SAssetPlacementPalette::GetSearchText() const
{
	return TypeFilter->GetRawFilterText();
}

TSharedRef<SWidget> SAssetPlacementPalette::GetViewOptionsMenuContent()
{
	FMenuBuilder MenuBuilder(true, UICommandList);

	MenuBuilder.BeginSection("PlacementPaletteViewMode", LOCTEXT("ViewModeHeading", "Palette View Mode"));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("ThumbnailView", "Thumbnails"),
			LOCTEXT("ThumbnailView_ToolTip", "Display thumbnails for each Placement type in the palette."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &SAssetPlacementPalette::SetViewMode, EViewMode::Thumbnail),
				FCanExecuteAction(),
				FIsActionChecked::CreateSP(this, &SAssetPlacementPalette::IsActiveViewMode, EViewMode::Thumbnail)
				),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
			);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("ListView", "List"),
			LOCTEXT("ListView_ToolTip", "Display Placement types in the palette as a list."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &SAssetPlacementPalette::SetViewMode, EViewMode::Tree),
				FCanExecuteAction(),
				FIsActionChecked::CreateSP(this, &SAssetPlacementPalette::IsActiveViewMode, EViewMode::Tree)
				),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
			);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("PlacementPaletteViewOptions", LOCTEXT("ViewOptionsHeading", "View Options"));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("ShowTooltips", "Show Tooltips"),
			LOCTEXT("ShowTooltips_ToolTip", "Whether to show tooltips when hovering over Placement types in the palette."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &SAssetPlacementPalette::ToggleShowTooltips),
				FCanExecuteAction(),
				FIsActionChecked::CreateSP(this, &SAssetPlacementPalette::ShouldShowTooltips),
				FIsActionButtonVisible::CreateSP(this, &SAssetPlacementPalette::IsActiveViewMode, EViewMode::Tree)
				),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
			);

		MenuBuilder.AddWidget(
			SNew(SHorizontalBox)
			.Visibility(this, &SAssetPlacementPalette::GetThumbnailScaleSliderVisibility)
			+ SHorizontalBox::Slot()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ThumbnailScaleLabel", "Scale"))
			]
			+SHorizontalBox::Slot()
			[
				SNew(SSlider)
				.ToolTipText(LOCTEXT("ThumbnailScaleToolTip", "Adjust the size of thumbnails."))
				.Value(this, &SAssetPlacementPalette::GetThumbnailScale)
				.OnValueChanged(this, &SAssetPlacementPalette::SetThumbnailScale)
				.OnMouseCaptureEnd(this, &SAssetPlacementPalette::RefreshActivePaletteViewWidget)
			],
			FText(),
			/*bNoIndent=*/true
			);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

TSharedPtr<SListView<FPlacementPaletteItemModelPtr>> SAssetPlacementPalette::GetActiveViewWidget() const
{
	if (ActiveViewMode == EViewMode::Thumbnail)
	{
		return TileViewWidget;
	}
	else if (ActiveViewMode == EViewMode::Tree)
	{
		return TreeViewWidget;
	}
	
	return nullptr;
}

EVisibility SAssetPlacementPalette::GetDropHintTextVisibility() const
{
	if (!bIsMirroringContentBrowser && (FilteredItems.Num() == 0) && !FSlateApplication::Get().IsDragDropping())
	{
		return EVisibility::HitTestInvisible;
	}

	return EVisibility::Hidden;
}

bool SAssetPlacementPalette::OnAreAssetsValidForDrop(TArrayView<FAssetData> DraggedAssets) const
{
	if (bIsMirroringContentBrowser)
	{
		return false;
	}

	UPlacementSubsystem* PlacementSubystem = GEditor->GetEditorSubsystem<UPlacementSubsystem>();
	for (const FAssetData& AssetData : DraggedAssets)
	{
		if (AssetData.GetClass()->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists | CLASS_NotPlaceable))
		{
			continue;
		}

		if (AssetData.IsValid())
		{
			return PlacementSubystem ? (PlacementSubystem->FindAssetFactoryFromAssetData(AssetData).GetInterface() != nullptr) : true;
		}
	}

	return false;
}

void SAssetPlacementPalette::HandlePlacementDropped(const FDragDropEvent& DragDropEvent, TArrayView<FAssetData> DroppedAssetData)
{
	if ((DroppedAssetData.Num() == 0) || bIsMirroringContentBrowser)
	{
		return;
	}

	if (DragDropEvent.IsShiftDown())
	{
		ClearPalette();
	}

	for (auto& AssetData : DroppedAssetData)
	{
		AddPlacementType(AssetData);
	}

	UpdatePalette(true);
}

bool SAssetPlacementPalette::HasAnyItemInPalette() const
{
	return PaletteItems.Num() > 0;
}

TSharedPtr<SWidget> SAssetPlacementPalette::ConstructPlacementTypeContextMenu()
{
	FMenuBuilder MenuBuilder(true, nullptr);
	if (!bIsMirroringContentBrowser)
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("Palette_Clear", "Clear Palette"),
			LOCTEXT("Palette_ClearDesc", "Removes all items from the palette."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &SAssetPlacementPalette::OnClearPalette),
				FCanExecuteAction::CreateSP(this, &SAssetPlacementPalette::HasAnyItemInPalette))
		);

		const int32 NumSelectedItems = SAssetPlacementPalette::GetActiveViewWidget()->GetNumItemsSelected();
		MenuBuilder.AddMenuEntry(
			FText::Format(LOCTEXT("Palette_RemoveItem", "Remove {0} {0}|plural(one=item,other=items)"), FText::AsNumber(NumSelectedItems)),
			FText::Format(LOCTEXT("Palette_RemoveItemDesc", "Removes the {0} selected {0}|plural(one=item,other=items) from the palette."), FText::AsNumber(NumSelectedItems)),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &SAssetPlacementPalette::OnRemoveSelectedItemsFromPalette),
				FCanExecuteAction::CreateSP(this, &SAssetPlacementPalette::HasAnyItemInPalette),
				FIsActionChecked::CreateLambda([]() { return false; }),
				FIsActionButtonVisible::CreateLambda([NumSelectedItems]() { return (NumSelectedItems > 0); }))
		);
	}
	return MenuBuilder.MakeWidget();
}

void SAssetPlacementPalette::OnPaletteSelectionChanged(FPlacementPaletteItemModelPtr Item, ESelectInfo::Type SelectInfo)
{
	if (SelectInfo == ESelectInfo::Direct)
	{
		return;
	}

	// Sync selection to the widgets which are not active
	switch (ActiveViewMode)
	{
		case EViewMode::Thumbnail:
			TreeViewWidget->SetItemSelection(Item, TileViewWidget->IsItemSelected(Item), ESelectInfo::Direct);
		break;

		case EViewMode::Tree:
			TileViewWidget->SetItemSelection(Item, TreeViewWidget->IsItemSelected(Item), ESelectInfo::Direct);
		break;
	}

	if (TSharedPtr<IDetailsView> PinnedItemDetails = ItemDetailsWidget.Pin())
	{
		TArray<UObject*> DetailsObjects;
		for (const FPlacementPaletteItemModelPtr& SelectedItem : GetActiveViewWidget()->GetSelectedItems())
		{
			if (UInstancedPlacemenClientSettings* SelectedItemSettingsObject = SelectedItem->GetTypeUIInfo()->SettingsObject.Get())
			{
				DetailsObjects.Add(SelectedItemSettingsObject);
			}
		}

		constexpr bool bForceRefresh = true;
		PinnedItemDetails->SetObjects(DetailsObjects, true);
	}
}

void SAssetPlacementPalette::OnShowPlacementTypeInCB()
{
	TArray<FAssetData> FilteredAssets;
	for (FPlacementPaletteItemModelPtr& PaletteItem : FilteredItems)
	{
		if (PaletteItem.IsValid() && PaletteItem->GetTypeUIInfo().IsValid())
		{
			FilteredAssets.Add(PaletteItem->GetTypeUIInfo()->AssetData);
		}
	}

	if (FilteredAssets.Num())
	{
		GEditor->SyncBrowserToObjects(FilteredAssets);
	}
}

// THUMBNAIL VIEW

TSharedRef<ITableRow> SAssetPlacementPalette::GenerateTile(FPlacementPaletteItemModelPtr Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SAssetPlacementPaletteItemTile, OwnerTable, Item);

	// Refresh the palette to ensure that thumbnails are correct
	RefreshPalette();
}

float SAssetPlacementPalette::GetScaledThumbnailSize() const
{
	const FInt32Interval& SizeRange = PlacementPaletteConstants::ThumbnailSizeRange;
	return SizeRange.Min + SizeRange.Size() * GetThumbnailScale();
}

float SAssetPlacementPalette::GetThumbnailScale() const
{
	return PaletteThumbnailScale;
}

void SAssetPlacementPalette::SetThumbnailScale(float InScale)
{
	PaletteThumbnailScale = FMath::Clamp(InScale, 0.f, 1.f);
}

EVisibility SAssetPlacementPalette::GetThumbnailScaleSliderVisibility() const
{
	return (ActiveViewMode == EViewMode::Thumbnail) ? EVisibility::Visible : EVisibility::Collapsed;
}

// TREE VIEW

TSharedRef<ITableRow> SAssetPlacementPalette::TreeViewGenerateRow(FPlacementPaletteItemModelPtr Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SAssetPlacementPaletteItemRow, OwnerTable, Item);
}

void SAssetPlacementPalette::TreeViewGetChildren(FPlacementPaletteItemModelPtr Item, TArray<FPlacementPaletteItemModelPtr>& OutChildren)
{
	// Items do not have any children
}

FText SAssetPlacementPalette::GetTypeColumnHeaderText() const
{
	return LOCTEXT("PlacementTypeHeader", "Asset Type");
}

EColumnSortMode::Type SAssetPlacementPalette::GetMeshColumnSortMode() const
{
	return ActiveSortOrder;
}

void SAssetPlacementPalette::OnTypeColumnSortModeChanged(EColumnSortPriority::Type InPriority, const FName& InColumnName, EColumnSortMode::Type InSortMode)
{
	if (ActiveSortOrder == InSortMode)
	{
		return;
	}

	ActiveSortOrder = InSortMode;

	if (ActiveSortOrder != EColumnSortMode::None)
	{
		auto CompareEntry = [this](const FPlacementPaletteItemModelPtr& A, const FPlacementPaletteItemModelPtr& B)
		{
			bool CompareResult = (A->GetDisplayFName().GetComparisonIndex().CompareLexical(B->GetDisplayFName().GetComparisonIndex()) <= 0);
			return (ActiveSortOrder == EColumnSortMode::Ascending) ? CompareResult : !CompareResult;
		};

		PaletteItems.Sort(CompareEntry);
	}
}

EActiveTimerReturnType SAssetPlacementPalette::UpdatePaletteItems(double InCurrentTime, float InDeltaTime)
{
	if (bItemsNeedRebuild)
	{
		bItemsNeedRebuild = false;
	}

	// Update the filtered items
	FilteredItems.Empty();
	for (auto& Item : PaletteItems)
	{
		if (TypeFilter->PassesFilter(Item))
		{
			FilteredItems.Add(Item);
		}
	}

	// Refresh the appropriate view
	RefreshActivePaletteViewWidget();

	bIsRebuildTimerRegistered = false;
	return EActiveTimerReturnType::Stop;
}

EActiveTimerReturnType SAssetPlacementPalette::RefreshPaletteItems(double InCurrentTime, float InDeltaTime)
{
	// Do not refresh the palette if we're waiting on a rebuild
	if (!bItemsNeedRebuild)
	{
		RefreshActivePaletteViewWidget();
	}

	bIsRefreshTimerRegistered = false;
	return EActiveTimerReturnType::Stop;
}

#undef LOCTEXT_NAMESPACE
