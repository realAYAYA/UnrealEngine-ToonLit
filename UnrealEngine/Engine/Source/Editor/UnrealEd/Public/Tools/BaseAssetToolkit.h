// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Object.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Templates/SharedPointer.h"
#include "Framework/Docking/TabManager.h"
#include "AssetEditorViewportLayout.h"

class SDockTab;
class FSpawnTabArgs;
class SEditorViewport;
class FLayoutExtender;
class FPreviewScene;
class FEditorViewportClient;
class UAssetEditor;

class UNREALED_API FBaseAssetToolkit : public FAssetEditorToolkit
{
public:
	FBaseAssetToolkit(UAssetEditor* InOwningAssetEditor);

	~FBaseAssetToolkit();
	virtual FName GetToolkitFName() const override
	{
		return NAME_None;
	}

	virtual FText GetBaseToolkitName() const override
	{
		return FText::GetEmpty();
	}
	virtual FString GetWorldCentricTabPrefix() const override
	{
		return TEXT("");
	}
	virtual FLinearColor GetWorldCentricTabColorScale() const override
	{
		return FLinearColor::White;
	}
	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;


	virtual const TSharedRef<FTabManager::FLayout> GetDefaultLayout() const;
	virtual void CreateWidgets();
	virtual void SetEditingObject(class UObject* InObject);
	virtual void CreateEditorModeManager() override;

public:
	static const FName ViewportTabID;
	static const FName DetailsTabID;

protected:
	virtual void RegisterToolbar();
	virtual AssetEditorViewportFactoryFunction GetViewportDelegate();
	virtual TSharedPtr<FEditorViewportClient> CreateEditorViewportClient() const;
	TSharedRef<SDockTab> SpawnTab_Viewport(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Details(const FSpawnTabArgs& Args);

protected:
	/** Property View */
	TSharedPtr<class IDetailsView> DetailsView;
	// Tracking the active viewports in this editor.
	TSharedPtr<class FEditorViewportTabContent> ViewportTabContent;
	/** Storage for our viewport creation function that will be passed to the viewport layout system*/
	AssetEditorViewportFactoryFunction ViewportDelegate;
	TSharedPtr<FEditorViewportClient> ViewportClient;
	/** Extender for adding to the default layout for this asset editor */
	TSharedPtr<FLayoutExtender> LayoutExtender;
	TSharedPtr<FTabManager::FLayout> StandaloneDefaultLayout;
	FString LayoutAppendix;
	UAssetEditor* OwningAssetEditor;
};