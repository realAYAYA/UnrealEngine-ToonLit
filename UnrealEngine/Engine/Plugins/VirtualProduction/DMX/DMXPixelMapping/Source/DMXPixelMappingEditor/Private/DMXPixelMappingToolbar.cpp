// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXPixelMappingToolbar.h"
#include "Framework/Commands/UICommandList.h"
#include "Toolkits/DMXPixelMappingToolkit.h"
#include "DMXPixelMappingEditorCommands.h"
#include "DMXPixelMappingEditorStyle.h"

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
	ToolbarBuilder.BeginSection("Renderers");
	{
		ToolbarBuilder.AddToolBarButton(FDMXPixelMappingEditorCommands::Get().AddMapping,
			NAME_None, 
			TAttribute<FText>(), 
			TAttribute<FText>(),
			FSlateIcon(FDMXPixelMappingEditorStyle::GetStyleSetName(), "DMXPixelMappingEditor.AddMapping"),
			FName(TEXT("Add Source")));
	}
	ToolbarBuilder.EndSection();

	ToolbarBuilder.BeginSection("PlayAndStopDMX");
	{
		ToolbarBuilder.AddToolBarButton(FDMXPixelMappingEditorCommands::Get().PlayDMX,
			NAME_None, 
			TAttribute<FText>(), 
			TAttribute<FText>(),
			FSlateIcon(FDMXPixelMappingEditorStyle::GetStyleSetName(), "DMXPixelMappingEditor.PlayDMX"),
			FName(TEXT("Play DMX")));

		ToolbarBuilder.AddToolBarButton(FDMXPixelMappingEditorCommands::Get().StopPlayingDMX,
			NAME_None, 
			TAttribute<FText>(), 
			TAttribute<FText>(),
			FSlateIcon(FDMXPixelMappingEditorStyle::GetStyleSetName(), "DMXPixelMappingEditor.StopPlayingDMX"),
			FName(TEXT("Stop Playing DMX")));
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

	MenuBuilder.BeginSection("EditorSettings", LOCTEXT("EditorSettingsSection", "Editor Settings"));
	{
		MenuBuilder.AddMenuEntry(FDMXPixelMappingEditorCommands::Get().ToggleScaleChildrenWithParent);
		MenuBuilder.AddMenuEntry(FDMXPixelMappingEditorCommands::Get().ToggleAlwaysSelectGroup);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("DisplaySettings", LOCTEXT("DisplaySettingsSection", "Display Settings"));
	{
		MenuBuilder.AddMenuEntry(FDMXPixelMappingEditorCommands::Get().ToggleShowComponentNames);
		MenuBuilder.AddMenuEntry(FDMXPixelMappingEditorCommands::Get().ToggleShowPatchInfo);
		MenuBuilder.AddMenuEntry(FDMXPixelMappingEditorCommands::Get().ToggleShowMatrixCells);
		MenuBuilder.AddMenuEntry(FDMXPixelMappingEditorCommands::Get().ToggleShowCellIDs);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("EditorSettings", LOCTEXT("LayoutSettingsSection", "Layout Settings"));
	{
		MenuBuilder.AddMenuEntry(FDMXPixelMappingEditorCommands::Get().ToggleApplyLayoutScriptWhenLoaded);
	}

	return MenuBuilder.MakeWidget();
}

#undef LOCTEXT_NAMESPACE
