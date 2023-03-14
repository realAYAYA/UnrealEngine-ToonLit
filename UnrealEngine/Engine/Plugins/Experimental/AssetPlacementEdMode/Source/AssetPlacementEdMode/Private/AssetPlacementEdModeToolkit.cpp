// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetPlacementEdModeToolkit.h"

#include "IDetailsView.h"
#include "SAssetPlacementPalette.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Toolkits/AssetEditorModeUILayer.h"
#include "PropertyEditorModule.h"
#include "Modules/ModuleManager.h"

#if !UE_IS_COOKED_EDITOR
#include "AssetPlacementEdModeModule.h"
#endif // !UE_IS_COOKED_EDITOR

#define LOCTEXT_NAMESPACE "AssetPlacementEdModeToolkit"

FAssetPlacementEdModeToolkit::FAssetPlacementEdModeToolkit()
{
	// Create the details view for the details view tab, so that it can be shared across our custom tabs
	{
		FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		FDetailsViewArgs DetailsViewArgs;
		DetailsViewArgs.bAllowSearch = false;
		DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::ENameAreaSettings::HideNameArea;
		DetailsViewArgs.bHideSelectionTip = true;

		PaletteItemDetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	}
}

void FAssetPlacementEdModeToolkit::GetToolPaletteNames(TArray<FName>& PaletteNames) const
{
	PaletteNames.Add(NAME_Default);
}


FName FAssetPlacementEdModeToolkit::GetToolkitFName() const
{
	return FName("AssetPlacementEdMode");
}

FText FAssetPlacementEdModeToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("DisplayName", "AssetPlacementEdMode Tool");
}

void FAssetPlacementEdModeToolkit::InvokeUI()
{
	FModeToolkit::InvokeUI();

	if (ModeUILayer.IsValid())
	{
		TSharedPtr<FAssetEditorModeUILayer> ModeUILayerPtr = ModeUILayer.Pin();
		AssetPaletteTab = ModeUILayerPtr->GetTabManager()->TryInvokeTab(UAssetEditorUISubsystem::BottomLeftTabID);

		PaletteItemDetailsViewTab = ModeUILayerPtr->GetTabManager()->TryInvokeTab(UAssetEditorUISubsystem::BottomRightTabID);
	}
}

void FAssetPlacementEdModeToolkit::RequestModeUITabs()
{
	FModeToolkit::RequestModeUITabs();
	if (ModeUILayer.IsValid())
	{
		TSharedPtr<FAssetEditorModeUILayer> ModeUILayerPtr = ModeUILayer.Pin();
		AssetPaletteInfo.OnSpawnTab = FOnSpawnTab::CreateSP(SharedThis(this), &FAssetPlacementEdModeToolkit::CreateAssetPalette);
		AssetPaletteInfo.TabLabel = LOCTEXT("AssetPaletteTab", "Palette");
		AssetPaletteInfo.TabTooltip = LOCTEXT("AssetPaletteTabTooltipText", "Open the asset palette tab, which contains placement mode's palette settings.");
		AssetPaletteInfo.TabIcon = GetEditorModeIcon();
		ModeUILayerPtr->SetModePanelInfo(UAssetEditorUISubsystem::BottomLeftTabID, AssetPaletteInfo);

#if !UE_IS_COOKED_EDITOR
		if (AssetPlacementEdModeUtil::AreInstanceWorkflowsEnabled())
		{
			PaletteItemDetailsViewInfo.OnSpawnTab = FOnSpawnTab::CreateSP(SharedThis(this), &FAssetPlacementEdModeToolkit::CreatePaletteItemDetailsView);
			PaletteItemDetailsViewInfo.TabLabel = LOCTEXT("PaletteItemDetailsTab", "Palette Details");
			PaletteItemDetailsViewInfo.TabTooltip = LOCTEXT("PaletteItemDetailsTabTooltipText", "Open the asset palette details tab, which allows customization of individual items in the active palette.");
			PaletteItemDetailsViewInfo.TabIcon = GetEditorModeIcon();
			ModeUILayerPtr->SetModePanelInfo(UAssetEditorUISubsystem::BottomRightTabID, PaletteItemDetailsViewInfo);
		}
#endif // !UE_IS_COOKED_EDITOR
	}
}



TSharedRef<SDockTab> FAssetPlacementEdModeToolkit::CreateAssetPalette(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		[
			SNew(SAssetPlacementPalette)
			.ItemDetailsView(PaletteItemDetailsView)
		];
}

TSharedRef<SDockTab> FAssetPlacementEdModeToolkit::CreatePaletteItemDetailsView(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
	[
		SNew(SScrollBox)
		+SScrollBox::Slot()
		[
			SNew(SOverlay)
			+SOverlay::Slot()
			[
				PaletteItemDetailsView.ToSharedRef()
			]

			+SOverlay::Slot()
			.Padding(0.0f, 4.0f, 0.0f, 0.0f)
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("PaletteDetailsViewHintText", "Select an item from the palette to edit settings."))
				.Visibility(this, &FAssetPlacementEdModeToolkit::GetDetailsViewHintTextVisibility)
				.TextStyle(FAppStyle::Get(), "HintText")
			]
		]
	];
}

EVisibility FAssetPlacementEdModeToolkit::GetDetailsViewHintTextVisibility() const
{
	if (PaletteItemDetailsView && (PaletteItemDetailsView->GetSelectedObjects().Num() == 0))
	{
		return EVisibility::HitTestInvisible;
	}
	return EVisibility::Collapsed;
}

#undef LOCTEXT_NAMESPACE
