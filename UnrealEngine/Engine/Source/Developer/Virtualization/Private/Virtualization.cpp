// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"

#include "Features/IModularFeatures.h"
#include "MessageLogModule.h"
#include "VirtualizationSourceControlUtilities.h"

#define LOCTEXT_NAMESPACE "Virtualization"

namespace UE::Virtualization
{

class FVirtualizationModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		IModularFeatures::Get().RegisterModularFeature(FName("VirtualizationSourceControlUtilities"), &SourceControlutility);

		FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
		MessageLogModule.RegisterLogListing("LogVirtualization", LOCTEXT("AssetVirtualizationLogLabel", "Asset Virtualization"));
	}

	virtual void ShutdownModule() override
	{
		if (FModuleManager::Get().IsModuleLoaded("MessageLog"))
		{
			FMessageLogModule& MessageLogModule = FModuleManager::GetModuleChecked<FMessageLogModule>("MessageLog");
			MessageLogModule.UnregisterLogListing("LogVirtualization&");
		}
		
		IModularFeatures::Get().UnregisterModularFeature(FName("VirtualizationSourceControlUtilities"), &SourceControlutility);
	}

private:
	Experimental::FVirtualizationSourceControlUtilities SourceControlutility;
};

} // namespace UE::Virtualization

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(UE::Virtualization::FVirtualizationModule, Virtualization);
