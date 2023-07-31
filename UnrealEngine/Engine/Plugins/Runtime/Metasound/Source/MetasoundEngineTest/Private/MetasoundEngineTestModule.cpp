// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

namespace Metasound 
{
	class METASOUNDENGINETEST_API FMetasoundEngineTestModule : public IModuleInterface
	{
	public:
		virtual void StartupModule() override
		{
			FModuleManager::Get().LoadModuleChecked("MetasoundFrontend");
			FModuleManager::Get().LoadModuleChecked("MetasoundStandardNodes");
			FModuleManager::Get().LoadModuleChecked("MetasoundEngine");
		}
	};
}

IMPLEMENT_MODULE(Metasound::FMetasoundEngineTestModule, MetasoundEngineTest);




