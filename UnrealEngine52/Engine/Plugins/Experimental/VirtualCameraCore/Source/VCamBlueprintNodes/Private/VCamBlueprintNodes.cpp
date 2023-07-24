// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

namespace UE::VCamBlueprintNodes::Private
{
	class FVCamBlueprintNodesModule : public IModuleInterface
	{
	public:

		virtual void StartupModule() override
		{}
	
		virtual void ShutdownModule() override
		{}
	};
}

IMPLEMENT_MODULE(UE::VCamBlueprintNodes::Private::FVCamBlueprintNodesModule, VCamBlueprintNodes);