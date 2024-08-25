// Copyright Epic Games, Inc. All Rights Reserved.

#include "Features/IModularFeatures.h"
#include "Modules/ModuleManager.h"

namespace Dataflow
{
	class FDataflowAssetToolsModule : public IModuleInterface
	{
	public:
		// IModuleInterface implementation
		virtual void StartupModule() override
		{
		}

		virtual void ShutdownModule() override
		{
		}
	};
}

IMPLEMENT_MODULE(Dataflow::FDataflowAssetToolsModule, DataflowAssetTools);
