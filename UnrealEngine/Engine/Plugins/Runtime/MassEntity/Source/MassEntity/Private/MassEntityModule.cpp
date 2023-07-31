// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "IMassEntityModule.h"
#include "MassEntityTypes.h"
#if WITH_UNREAL_DEVELOPER_TOOLS
#include "MessageLogModule.h"
#include "Engine/World.h"
#include "Logging/MessageLog.h"
#endif // WITH_UNREAL_DEVELOPER_TOOLS

#define LOCTEXT_NAMESPACE "Mass"

class FMassEntityModuleModule : public IMassEntityModule
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
#if WITH_UNREAL_DEVELOPER_TOOLS
	static void OnWorldCleanup(UWorld* /*World*/, bool /*bSessionEnded*/, bool /*bCleanupResources*/);
	FDelegateHandle OnWorldCleanupHandle;
#endif // WITH_UNREAL_DEVELOPER_TOOLS
};

IMPLEMENT_MODULE(FMassEntityModuleModule, MassEntity)


void FMassEntityModuleModule::StartupModule()
{
	// This code will execute after your module is loaded into memory (but after global variables are initialized, of course.)

#if WITH_UNREAL_DEVELOPER_TOOLS
	{
		FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
		FMessageLogInitializationOptions InitOptions;
		InitOptions.bShowPages = true;
		InitOptions.bShowFilters = true;
		MessageLogModule.RegisterLogListing("MassEntity", LOCTEXT("MassEntity", "MassEntity"), InitOptions);

		OnWorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddStatic(&FMassEntityModuleModule::OnWorldCleanup);
	}
#endif // WITH_UNREAL_DEVELOPER_TOOLS

#if MASS_DO_PARALLEL
	UE_LOG(LogMass, Log, TEXT("MassEntity running with MULTITHREADING support."));
#else
	UE_LOG(LogMass, Log, TEXT("MassEntity running in game thread."));
#endif // MASS_DO_PARALLEL
}


void FMassEntityModuleModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
#if WITH_UNREAL_DEVELOPER_TOOLS
	FWorldDelegates::OnWorldCleanup.Remove(OnWorldCleanupHandle);
#endif // WITH_UNREAL_DEVELOPER_TOOLS
}

#if WITH_UNREAL_DEVELOPER_TOOLS
void FMassEntityModuleModule::OnWorldCleanup(UWorld* /*World*/, bool /*bSessionEnded*/, bool /*bCleanupResources*/)
{
	// clearing out messages from the world being cleaned up
	FMessageLog("MassEntity").NewPage(FText::FromString(TEXT("MassEntity")));
}
#endif // WITH_UNREAL_DEVELOPER_TOOLS

#undef LOCTEXT_NAMESPACE 
