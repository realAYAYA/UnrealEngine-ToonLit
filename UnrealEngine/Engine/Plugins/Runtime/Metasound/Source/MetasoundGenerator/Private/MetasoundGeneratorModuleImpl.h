// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetasoundGeneratorModule.h"

#include "MetasoundOperatorCache.h"
#include "MetasoundInstanceCounter.h"

namespace Metasound
{
	class FMetasoundGeneratorModule : public IMetasoundGeneratorModule
	{
	public:

		virtual void StartupModule() override;
		virtual void ShutdownModule() override;

		virtual TSharedPtr<FOperatorPool> GetOperatorPool() override;
		virtual TSharedPtr<FConcurrentInstanceCounterManager> GetOperatorInstanceCounterManager() override;

	private:

		TSharedPtr<FOperatorPool> OperatorPool;
		TSharedPtr<FConcurrentInstanceCounterManager> OperatorInstanceCounterManager;
	};
}
