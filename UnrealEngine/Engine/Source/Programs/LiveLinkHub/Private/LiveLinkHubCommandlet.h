// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"
#include "ILiveLinkHubModule.h"

#include "LiveLinkHubCommandlet.generated.h"

UCLASS()
class ULiveLinkHubCommandlet : public UCommandlet
{
public:
	GENERATED_BODY()

	ULiveLinkHubCommandlet()
	{
		LogToConsole = false;
		FastExit = true;
	}
	
	/** Runs the commandlet */
	virtual int32 Main(const FString& Params) override
	{
		PRIVATE_GAllowCommandletRendering = true;

		// Work around the CB file asset data source being disabled in commandlets.
		if (IModuleInterface* AssetDataSourceModule = FModuleManager::Get().GetModule("ContentBrowserAssetDataSource"))
		{
			PRIVATE_GIsRunningCommandlet = false;
			AssetDataSourceModule->StartupModule();
			PRIVATE_GIsRunningCommandlet = true;	
		}

		FModuleManager::Get().LoadModule("OutputLog");

		FModuleManager::Get().LoadModuleChecked<ILiveLinkHubModule>("LiveLinkEditor");
		FModuleManager::Get().LoadModuleChecked<ILiveLinkHubModule>("LiveLinkHub").StartLiveLinkHub();

		return 0;
	}
};
