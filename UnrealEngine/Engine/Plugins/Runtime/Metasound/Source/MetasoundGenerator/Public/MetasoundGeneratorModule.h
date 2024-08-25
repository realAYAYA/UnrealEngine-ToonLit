// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMetasoundGenerator, Log, All);

namespace Metasound
{
	// forward
	class FOperatorPool;
	class FConcurrentInstanceCounterManager;

	class METASOUNDGENERATOR_API IMetasoundGeneratorModule : public IModuleInterface
	{
	public:
		virtual TSharedPtr<FOperatorPool> GetOperatorPool() = 0;
		virtual TSharedPtr<FConcurrentInstanceCounterManager> GetOperatorInstanceCounterManager() = 0;
	};
}


