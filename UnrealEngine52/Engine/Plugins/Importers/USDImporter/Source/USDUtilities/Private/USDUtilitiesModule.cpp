// Copyright Epic Games, Inc. All Rights Reserved.

#include "USDUtilitiesModule.h"

#include "USDMemory.h"

#include "Modules/ModuleManager.h"

#if WITH_EDITOR
#include "MessageLogModule.h"
#endif // WITH_EDITOR

class FUsdUtilitiesModule : public IUsdUtilitiesModule
{
	virtual void StartupModule() override
	{
#if WITH_EDITOR
		LLM_SCOPE_BYTAG(Usd);

		FMessageLogInitializationOptions InitOptions;
		FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked< FMessageLogModule >( TEXT("MessageLog") );
		MessageLogModule.RegisterLogListing( TEXT( "USD" ), NSLOCTEXT( "USDUtilitiesModule", "USDLogListing", "USD" ), InitOptions );
#endif // WITH_EDITOR
	}

	virtual void ShutdownModule() override
	{
#if WITH_EDITOR
		FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked< FMessageLogModule >( TEXT("MessageLog") );
		MessageLogModule.UnregisterLogListing( TEXT( "USD" ) );
#endif // WITH_EDITOR
	}
};

IMPLEMENT_MODULE_USD( FUsdUtilitiesModule, USDUtilities );
