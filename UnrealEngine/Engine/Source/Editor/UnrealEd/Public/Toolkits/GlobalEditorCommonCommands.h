// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/Commands/Commands.h"
#include "Framework/Docking/TabManager.h"

class IMenu;
class SWindow;

// Global editor common commands
// Note: There is no real global command concept, so these must still be registered in each editor
class UNREALED_API FGlobalEditorCommonCommands : public TCommands< FGlobalEditorCommonCommands >
{
public:
	FGlobalEditorCommonCommands();
	~FGlobalEditorCommonCommands();

	virtual void RegisterCommands() override;

	static void MapActions(TSharedRef<FUICommandList>& ToolkitCommands);

protected:
	static void OnPressedCtrlTab(TSharedPtr<FUICommandInfo> TriggeringCommand);
	static void OnSummonedAssetPicker();
	static void OnSummonedConsoleCommandBox();
	static void OnOpenContentBrowserDrawer();
	static void OnOpenOutputLogDrawer();

	static TSharedRef<SDockTab> SpawnAssetPicker(const FSpawnTabArgs& InArgs);

	static TSharedPtr<IMenu> OpenPopupMenu(TSharedRef<SWidget> WindowContents, const FVector2D& PopupDesiredSize);
public:
	TSharedPtr<FUICommandInfo> FindInContentBrowser;
	TSharedPtr<FUICommandInfo> SummonControlTabNavigation;
	TSharedPtr<FUICommandInfo> SummonControlTabNavigationAlternate;
	TSharedPtr<FUICommandInfo> SummonControlTabNavigationBackwards;
	TSharedPtr<FUICommandInfo> SummonControlTabNavigationBackwardsAlternate;
	TSharedPtr<FUICommandInfo> SummonOpenAssetDialog;
	TSharedPtr<FUICommandInfo> SummonOpenAssetDialogAlternate;
	TSharedPtr<FUICommandInfo> OpenDocumentation;
	TSharedPtr<FUICommandInfo> OpenConsoleCommandBox;
	TSharedPtr<FUICommandInfo> SelectNextConsoleExecutor;
	TSharedPtr<FUICommandInfo> OpenOutputLogDrawer;
	TSharedPtr<FUICommandInfo> OpenContentBrowserDrawer;
};

