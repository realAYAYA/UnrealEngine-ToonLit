// Copyright Epic Games, Inc. All Rights Reserved.

#include "N10XSourceCodeAccessModule.h"

#include "Modules/ModuleManager.h"
#include "Features/IModularFeatures.h"
#include "N10XSourceCodeAccessor.h"

IMPLEMENT_MODULE( F10XSourceCodeAccessModule, N10XSourceCodeAccess );

F10XSourceCodeAccessModule::F10XSourceCodeAccessModule()
	: SourceCodeAccessor(MakeShareable(new F10XSourceCodeAccessor()))
{
}

void F10XSourceCodeAccessModule::StartupModule()
{
	SourceCodeAccessor->Startup();

	IModularFeatures::Get().RegisterModularFeature(TEXT("SourceCodeAccessor"), &SourceCodeAccessor.Get() );
}

void F10XSourceCodeAccessModule::ShutdownModule()
{
	IModularFeatures::Get().UnregisterModularFeature(TEXT("SourceCodeAccessor"), &SourceCodeAccessor.Get());

	SourceCodeAccessor->Shutdown();
}

F10XSourceCodeAccessor& F10XSourceCodeAccessModule::GetAccessor()
{
	return SourceCodeAccessor.Get();
}
