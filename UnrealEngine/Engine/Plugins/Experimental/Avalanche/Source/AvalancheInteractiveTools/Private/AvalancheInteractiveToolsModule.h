// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IAvalancheInteractiveToolsModule.h"

class FUICommandInfo;

DECLARE_LOG_CATEGORY_EXTERN(LogAvaInteractiveTools, Log, All);

class FAvalancheInteractiveToolsModule : public IAvalancheInteractiveToolsModule
{
public:
	static FAvalancheInteractiveToolsModule& Get()
	{
		static const FName ModuleName = TEXT("AvalancheInteractiveTools");
		return FModuleManager::LoadModuleChecked<FAvalancheInteractiveToolsModule>(ModuleName);
	}

	static FAvalancheInteractiveToolsModule* GetPtr()
	{
		static const FName ModuleName = TEXT("AvalancheInteractiveTools");
		return FModuleManager::GetModulePtr<FAvalancheInteractiveToolsModule>(ModuleName);
	}

	//~ Begin IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface interface

	//~ Begin IAvalancheInteractiveToolsModule
	virtual void RegisterCategory(FName InCategoryName, TSharedPtr<FUICommandInfo> InCategoryCommand, 
		int32 InPlacementModeSortPriority = NoPlacementCategory) override;
	virtual void RegisterTool(FName InCategory, FAvaInteractiveToolsToolParameters&& InToolParams) override;
	virtual const TMap<FName, TSharedPtr<FUICommandInfo>>& GetCategories() override;
	virtual const TArray<FAvaInteractiveToolsToolParameters>* GetTools(FName InCategory) override;
	virtual bool HasActiveTool() const override;
	//~ End IAvalancheInteractiveToolsModule

	void OnToolActivated();
	void OnToolDeactivated();

private:
	TMap<FName, TSharedPtr<FUICommandInfo>> Categories;
	TMap<FName, TArray<FAvaInteractiveToolsToolParameters>> Tools;
	bool bHasActiveTool = false;

	void OnPostEngineInit();
	void BroadcastRegisterCategories();
	void RegisterDefaultCategories();
	void BroadcastRegisterTools();
	void RegisterDefaultTools();
	void OnPlacementCategoryRefreshed(FName InCategory);
};
