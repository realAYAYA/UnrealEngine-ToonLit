// Copyright Epic Games, Inc. All Rights Reserved.

#include "InterchangeExportModule.h"

#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "InterchangeManager.h"
#include "InterchangeTextureWriter.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"


class FInterchangeExportModule : public IInterchangeExportModule
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

IMPLEMENT_MODULE(FInterchangeExportModule, InterchangeExport)



void FInterchangeExportModule::StartupModule()
{
	auto RegisterItems = []()
	{
		UInterchangeManager& InterchangeManager = UInterchangeManager::GetInterchangeManager();
		//Register the Writers
		InterchangeManager.RegisterWriter(UInterchangeTextureWriter::StaticClass());
	};

	if (GEngine)
	{
		RegisterItems();
	}
	else
	{
		FCoreDelegates::OnPostEngineInit.AddLambda(RegisterItems);
	}
}


void FInterchangeExportModule::ShutdownModule()
{
	
}



