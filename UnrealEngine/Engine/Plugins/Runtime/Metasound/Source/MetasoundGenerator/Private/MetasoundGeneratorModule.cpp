// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundGeneratorModule.h"

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogMetasoundGenerator);

namespace Metasound
{
	class FMetasoundGeneratorModule : public IMetasoundGeneratorModule
	{
	};
}

IMPLEMENT_MODULE(Metasound::FMetasoundGeneratorModule, MetasoundGenerator);


