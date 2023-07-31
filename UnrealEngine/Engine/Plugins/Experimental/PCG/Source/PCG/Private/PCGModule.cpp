// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGModule.h"

#if WITH_EDITOR
#include "Elements/PCGDifferenceElement.h"

#include "Tests/Determinism/PCGDeterminismNativeTests.h"
#include "Tests/Determinism/PCGDifferenceDeterminismTest.h"

#endif

#include "Modules/ModuleInterface.h"

class FPCGModule final : public IModuleInterface
{
public:
	//~ IModuleInterface implementation
	virtual void StartupModule() override;

	virtual void ShutdownModule() override;

	virtual bool SupportsDynamicReloading() override
	{
		return true;
	}

	//~ End IModuleInterface implementation

private:
#if WITH_EDITOR
	void RegisterNativeElementDeterminismTests();
	void DeregisterNativeElementDeterminismTests();
#endif
};

void FPCGModule::StartupModule()
{
#if WITH_EDITOR
	PCGDeterminismTests::FNativeTestRegistry::Create();

	RegisterNativeElementDeterminismTests();
#endif
}

void FPCGModule::ShutdownModule()
{
#if WITH_EDITOR
	DeregisterNativeElementDeterminismTests();

	PCGDeterminismTests::FNativeTestRegistry::Destroy();
#endif
}

#if WITH_EDITOR

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

DEFINE_LOG_CATEGORY(LogPCG);