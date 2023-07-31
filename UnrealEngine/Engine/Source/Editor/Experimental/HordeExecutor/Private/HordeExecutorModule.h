// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HordeExecutor.h"
#include "Logging/LogMacros.h"
#include "Modules/ModuleInterface.h"

HORDEEXECUTOR_API DECLARE_LOG_CATEGORY_EXTERN(LogHordeExecutor, Display, All);


namespace UE::RemoteExecution
{
	class FHordeExecutorModule : public IModuleInterface
	{
	public:
		/** IModuleInterface implementation */
		virtual void StartupModule() override;

		virtual void ShutdownModule() override;

		virtual bool SupportsDynamicReloading() override;

	private:
		FHordeExecutor HordeExecution;
	};
}
