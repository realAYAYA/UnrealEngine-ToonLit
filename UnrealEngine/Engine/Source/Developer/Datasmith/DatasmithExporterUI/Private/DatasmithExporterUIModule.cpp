// Copyright Epic Games, Inc. All Rights Reserved.

#include "DatasmithExporterUIModule.h"

#include "DatasmithExporterManager.h"
#include "DirectLinkUI.h"

#include <atomic>

std::atomic<class FDatasmithExporterUIModule*> GExporterUIModule(nullptr);

IDatasmithExporterUIModule* IDatasmithExporterUIModule::Get()
{
	return GExporterUIModule;
}

void FDatasmithExporterUIModule::StartupModule()
{
	GExporterUIModule = this;

#if IS_PROGRAM
	if ( FDatasmithExporterManager::WasInitializedWithMessaging() )
	{
		DirectLinkUI = MakeUnique<FDirectLinkUI>();
	}
#endif
}

IDirectLinkUI* FDatasmithExporterUIModule::GetDirectLinkExporterUI() const
{
	return DirectLinkUI.Get();
}

