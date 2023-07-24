// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "IWebBrowserSingleton.h"
#include "Internationalization/Text.h"

class FMenuBuilder;
class FSpawnTabArgs;
class FToolBarBuilder;
class SDockTab;
class SWebBrowser;
class SWindow;
class UBrowserBinding;


class FBridgeUIManagerImpl;
class FArguments;
class SCompoundWidget;

class FBridgeUIManagerImpl : public TSharedFromThis<FBridgeUIManagerImpl>
{
public:
	void Initialize();
	void Shutdown();
	void HandleBrowserUrlChanged(const FText& Url);
	TSharedPtr<SWebBrowser> WebBrowserWidget;
	FCreateBrowserWindowSettings WindowSettings;
	TSharedPtr<IWebBrowserWindow> Browser;
	TSharedPtr<SDockTab> LocalBrowserDock;
	TSharedPtr<SWindow> DragDropWindow;

private:
	void SetupMenuItem();
	void CreateWindow();
	TSharedRef<SDockTab> CreateBridgeTab(const FSpawnTabArgs& Args);
	void FillToolbar(FToolBarBuilder& ToolbarBuilder);
	void AddPluginMenu(FMenuBuilder& MenuBuilder);

protected:
	const FText BridgeTabDisplay = FText::FromString("Bridge");
	const FText BridgeToolTip = FText::FromString("Launch Megascans Bridge");
};

class  FBridgeUIManager
{
public:
	static void Initialize();
	static void Shutdown();
	static TSharedPtr<FBridgeUIManagerImpl> Instance;
	static UBrowserBinding* BrowserBinding;
};

