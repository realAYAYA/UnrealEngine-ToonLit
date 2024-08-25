// Copyright Epic Games, Inc. All Rights Reserved.

#include "CustomDetailsViewModule.h"
#include "SCustomDetailsView.h"

class FCustomDetailsViewModule : public ICustomDetailsViewModule
{
	//~ Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface

	//~ Begin ICustomDetailsViewModule
	virtual TSharedRef<ICustomDetailsView> CreateCustomDetailsView(const FCustomDetailsViewArgs& InArgs) override;
	//~ End ICustomDetailsViewModule
};

IMPLEMENT_MODULE(FCustomDetailsViewModule, CustomDetailsView)

void FCustomDetailsViewModule::StartupModule()
{
}

void FCustomDetailsViewModule::ShutdownModule()
{
}

TSharedRef<ICustomDetailsView> FCustomDetailsViewModule::CreateCustomDetailsView(const FCustomDetailsViewArgs& InArgs)
{
	// log in case there's a misconfiguration where keyframe handling is set but global extensions is not set to true
	ensureMsgf(InArgs.bAllowGlobalExtensions || !InArgs.KeyframeHandler.IsValid()
		, TEXT("A Keyframe Handler was specified, but bAllowGlobalExtensions is false"));

	return SNew(SCustomDetailsView, InArgs);
}
