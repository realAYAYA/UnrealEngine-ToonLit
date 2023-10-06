// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGModule.h"

#include "PCGEngineSettings.h"

#include "Modules/ModuleManager.h"

#if WITH_EDITOR
#include "Elements/PCGDifferenceElement.h"
#include "ISettingsModule.h"
#include "Tests/Determinism/PCGDeterminismNativeTests.h"
#include "Tests/Determinism/PCGDifferenceDeterminismTest.h"
#endif

#define LOCTEXT_NAMESPACE "FPCGModule"

class FPCGModule final : public IModuleInterface
{
public:
	//~ IModuleInterface implementation

#if WITH_EDITOR
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
#endif

	virtual bool SupportsDynamicReloading() override
	{
		return true;
	}

	//~ End IModuleInterface implementation

private:
#if WITH_EDITOR
	void RegisterSettings();
	void UnregisterSettings();

	void RegisterNativeElementDeterminismTests();
	void DeregisterNativeElementDeterminismTests();
#endif
};

#if WITH_EDITOR
void FPCGModule::StartupModule()
{
	RegisterSettings();

	PCGDeterminismTests::FNativeTestRegistry::Create();

	RegisterNativeElementDeterminismTests();
}

void FPCGModule::ShutdownModule()
{
	UnregisterSettings();

	DeregisterNativeElementDeterminismTests();

	PCGDeterminismTests::FNativeTestRegistry::Destroy();
}

void FPCGModule::RegisterSettings()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->RegisterSettings("Project", "Plugins", "PCG",
			LOCTEXT("PCGEngineSettingsName", "PCG"),
			LOCTEXT("PCGEngineSettingsDescription", "Configure PCG."),
			GetMutableDefault<UPCGEngineSettings>());
	}
}

void FPCGModule::UnregisterSettings()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Project", "Plugins", "PCG");
	}
}

void FPCGModule::RegisterNativeElementDeterminismTests()
{
	PCGDeterminismTests::FNativeTestRegistry::RegisterTestFunction(UPCGDifferenceSettings::StaticClass(), PCGDeterminismTests::DifferenceElement::RunTestSuite);
	// TODO: Add other native test functions
}

void FPCGModule::DeregisterNativeElementDeterminismTests()
{
	PCGDeterminismTests::FNativeTestRegistry::DeregisterTestFunction(UPCGDifferenceSettings::StaticClass());
}

#endif

IMPLEMENT_MODULE(FPCGModule, PCG);

PCG_API DEFINE_LOG_CATEGORY(LogPCG);

#undef LOCTEXT_NAMESPACE
