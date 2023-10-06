// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetasoundGeneratorModule.h"

#include "MetasoundOperatorCache.h"

namespace Metasound
{
	class FMetasoundGeneratorModule : public IMetasoundGeneratorModule
	{
	public:

		virtual void StartupModule() override;
		virtual void ShutdownModule() override;

		TSharedPtr<FOperatorCache> GetOperatorCache();

	private:

		TSharedPtr<FOperatorCache> OperatorCache;
	};
}




