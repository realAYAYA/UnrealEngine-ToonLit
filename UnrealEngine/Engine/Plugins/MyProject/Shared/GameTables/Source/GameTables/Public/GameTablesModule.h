// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class UGameTables;

class GAMETABLES_API FGameTablesModule : public IModuleInterface
{
	
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	
	static FGameTablesModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FGameTablesModule>("GameTables");
	}

	UGameTables* GetGameTables();

private:

	UGameTables* GameTables = nullptr;
};
