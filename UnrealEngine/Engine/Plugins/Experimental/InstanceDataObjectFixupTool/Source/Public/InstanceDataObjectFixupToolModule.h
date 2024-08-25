// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Modules/ModuleManager.h"
#include "Widgets/Docking/SDockTab.h"

class INSTANCEDATAOBJECTFIXUPTOOL_API FInstanceDataObjectFixupToolModule : public FDefaultModuleImpl
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	bool OpenInstanceDataObjectFixupTool() const;
	static FInstanceDataObjectFixupToolModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FInstanceDataObjectFixupToolModule>("InstanceDataObjectFixupTool");
	}

	// opens the fixup tool 
	TSharedRef<SDockTab> CreateInstanceDataObjectFixupTab(const FSpawnTabArgs& TabArgs, TConstArrayView<TObjectPtr<UObject>> InstanceDataObjects) const;
};
