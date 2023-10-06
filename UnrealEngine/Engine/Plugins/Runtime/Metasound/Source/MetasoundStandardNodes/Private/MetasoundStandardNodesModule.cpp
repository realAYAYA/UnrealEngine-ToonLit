// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "MetasoundGain.h"
#include "MetasoundGraphCoreModule.h"
#include "MetasoundPrimitives.h"
#include "MetasoundTime.h"
#include "MetasoundTrigger.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"


namespace Metasound 
{
	class FMetasoundStandardNodesModule : public IMetasoundGraphCoreModule
	{
		virtual void StartupModule() override
		{
			FMetasoundFrontendRegistryContainer::Get()->RegisterPendingNodes();
		}
	};
}

IMPLEMENT_MODULE(Metasound::FMetasoundStandardNodesModule, MetasoundStandardNodes);

