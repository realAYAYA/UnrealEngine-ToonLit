// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXPixelMappingToolbar.h"
#include "Toolkits/DMXPixelMappingToolkit.h"
#include "DMXPixelMappingEditorCommands.h"
#include "DMXPixelMappingEditorCommon.h"
#include "DMXPixelMappingEditorStyle.h"
#include "DMXPixelMappingEditorCommon.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"

#define LOCTEXT_NAMESPACE "FDMXPixelMappingToolbar"

FDMXPixelMappingToolbar::FDMXPixelMappingToolbar(TSharedPtr<FDMXPixelMappingToolkit> InToolkit)
	: ToolkitWeakPtr(InToolkit)
{}

void FDMXPixelMappingToolbar::BuildToolbar(TSharedPtr<FExtender> Extender)
{
	TSharedPtr<FDMXPixelMappingToolkit> Toolkit = ToolkitWeakPtr.Pin();
	check(Toolkit.IsValid());

	Extender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		Toolkit->GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateSP(this, &FDMXPixelMappingToolbar::BuildToolbarCallback)
	);
}

void FDMXPixelMappingToolbar::BuildToolbarCallback(FToolBarBuilder& ToolbarBuilder)
{
	ToolbarBuilder.BeginSection("Thumbnail");
	{
		ToolbarBuilder.AddToolBarButton(FDMXPixelMappingEditorCommands::Get().SaveThumbnailImage, NAME_None,
			LOCTEXT("GenerateThumbnail", "Thumbnail"),
			LOCTEXT("GenerateThumbnailTooltip", "Generate a thumbnail image."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Cascade.SaveThumbnailImage"));
	}
	ToolbarBuilder.EndSection();

	TSharedPtr<FDMXPixelMappingToolkit> Toolkit = ToolkitWeakPtr.Pin();
	check(Toolkit.IsValid());

	ToolbarBuilder.BeginSection("Renderers");
	{
		ToolbarBuilder.AddToolBarButton(FDMXPixelMappingEditorCommands::Get().AddMapping,
			NAME_None, TAttribute<FText>(), TAttribute<FText>(),
			FSlateIcon(FDMXPixelMappingEditorStyle::GetStyleSetName(), "DMXPixelMappingEditor.AddMapping"),
			FName(TEXT("Add Mapping")));
	}
	ToolbarBuilder.EndSection();

	ToolbarBuilder.BeginSection("PlayAndStopDMX");
	{
		ToolbarBuilder.AddToolBarButton(FDMXPixelMappingEditorCommands::Get().PlayDMX,
			NAME_None, TAttribute<FText>(), TAttribute<FText>(),
			FSlateIcon(FDMXPixelMappingEditorStyle::GetStyleSetName(), "DMXPixelMappingEditor.PlayDMX"),
			FName(TEXT("Play DMX")));

		ToolbarBuilder.AddToolBarButton(FDMXPixelMappingEditorCommands::Get().StopPlayingDMX,
			NAME_None, TAttribute<FText>(), TAttribute<FText>(),
			FSlateIcon(FDMXPixelMappingEditorStyle::GetStyleSetName(), "DMXPixelMappingEditor.StopPlayingDMX"),
			FName(TEXT("Stop Playing DMX")));

		FUIAction PlayDMXOptionsAction(
			FExecuteAction(),
			FCanExecuteAction(),
			FIsActionChecked(),
			FIsActionButtonVisible::CreateLambda([this] { return !ToolkitWeakPtr.Pin()->IsPlayingDMX(); })
			);

		ToolbarBuilder.AddComboButton(
			PlayDMXOptionsAction,
			FOnGetContent::CreateSP(this, &FDMXPixelMappingToolbar::GeneratesPlayOptionsWidget),
			LOCTEXT("PlayDMXOptions", "Play DMX Options"),
			LOCTEXT("PlayDMXOptions", "Play DMX Options"),
			FSlateIcon(),
			true);
	}
	ToolbarBuilder.EndSection();

	ToolbarBuilder.BeginSection("Layout");
	{
		ToolbarBuilder.AddComboButton(
			FUIAction(),
			FOnGetContent::CreateSP(this, &FDMXPixelMappingToolbar::GenerateLayoutMenu),
			LOCTEXT("PixelMappingToolbarLayoutSettingsLabel", "Layout"),
			LOCTEXT("PixelMappingToolbarLayoutSettingsTooltip", "Layout Settings for Pixel Mapping Editors"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Layout")
		);
	}
	ToolbarBuilder.EndSection();
}

TSharedRef<SWidget> FDMXPixelMappingToolbar::GeneratesPlayOptionsWidget()
{
	TSharedPtr<FDMXPixelMappingToolkit> Toolkit = ToolkitWeakPtr.Pin();
	check(Toolkit.IsValid());

	FMenuBuilder MenuBuilder(true, Toolkit->GetToolkitCommands());

	if (!Toolkit->IsPlayingDMX())
	{
		MenuBuilder.BeginSection("bTogglePlayDMXAll");
		{
			MenuBuilder.AddMenuEntry(FDMXPixelMappingEditorCommands::Get().TogglePlayDMXAll);
		}
		MenuBuilder.EndSection();
	}

	return MenuBuilder.MakeWidget();
}

TSharedRef<SWidget> FDMXPixelMappingToolbar::GenerateLayoutMenu()
{
	TSharedPtr<FDMXPixelMappingToolkit> Toolkit = ToolkitWeakPtr.Pin();
	check(Toolkit.IsValid());

	constexpr bool bShouldCloseMenuAfterSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseMenuAfterSelection, Toolkit->GetToolkitCommands());
	
	MenuBuilder.BeginSection("Actions", LOCTEXT("ActionsSection", "Actions"));
	{
		MenuBuilder.AddMenuEntry(FDMXPixelMappingEditorCommands::Get().SizeComponentToTexture);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("LayoutSettings", LOCTEXT("LayoutSettingsSection", "Settings"));
	{
		MenuBuilder.AddMenuEntry(FDMXPixelMappingEditorCommands::Get().ToggleScaleChildrenWithParent);
		MenuBuilder.AddMenuEntry(FDMXPixelMappingEditorCommands::Get().ToggleAlwaysSelectGroup);
		MenuBuilder.AddMenuEntry(FDMXPixelMappingEditorCommands::Get().ToggleApplyLayoutScriptWhenLoaded);
		MenuBuilder.AddMenuEntry(FDMXPixelMappingEditorCommands::Get().ToggleShowComponentNames);
		MenuBuilder.AddMenuEntry(FDMXPixelMappingEditorCommands::Get().ToggleShowPatchInfo);
		MenuBuilder.AddMenuEntry(FDMXPixelMappingEditorCommands::Get().ToggleShowCellIDs);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

#undef LOCTEXT_NAMESPACE
