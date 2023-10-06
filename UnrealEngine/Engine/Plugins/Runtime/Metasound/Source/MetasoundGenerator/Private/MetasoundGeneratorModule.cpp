// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundGeneratorModule.h"
#include "MetasoundGeneratorModuleImpl.h"

#include "Containers/Array.h"
#include "HAL/IConsoleManager.h"
#include "Logging/LogMacros.h"
#include "MetasoundOperatorCache.h"
#include "Misc/CString.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogMetasoundGenerator);

static FAutoConsoleCommand CommandMetaSoundExperimentalOperatorCacheSetMaxNumOperators(
	TEXT("au.MetaSound.Experimental.OperatorCache.SetMaxNumOperators"),
	TEXT("Set the maximum number of operators in the MetaSound operator cache."),
	FConsoleCommandWithArgsDelegate::CreateStatic([](const TArray< FString >& Args)
	{
		using namespace Metasound;

		if (Args.Num() < 1)
		{
			return;
		}

		const int32 MaxNumOperators = FCString::Atoi(*Args[0]);

		if (MaxNumOperators < 0)
		{
			return;
		}

		FMetasoundGeneratorModule& Module = FModuleManager::GetModuleChecked<FMetasoundGeneratorModule>("MetasoundGenerator");
		TSharedPtr<FOperatorCache> OperatorCache = Module.GetOperatorCache();
		if (OperatorCache.IsValid())
		{
			OperatorCache->SetMaxNumOperators(static_cast<uint32>(MaxNumOperators));
			UE_LOG(LogMetasoundGenerator, Display, TEXT("Metasound operator cache size set to %d operators."), MaxNumOperators);
		}
	})
);

namespace Metasound
{
	void FMetasoundGeneratorModule::StartupModule()
	{
		FOperatorCacheSettings OperatorCacheSettings;
		OperatorCacheSettings.MaxNumOperators = 64;

		OperatorCache = MakeShared<FOperatorCache>(OperatorCacheSettings);
	}

	void FMetasoundGeneratorModule::ShutdownModule() 
	{
		OperatorCache.Reset();
	}

	TSharedPtr<FOperatorCache> FMetasoundGeneratorModule::GetOperatorCache()
	{
		return OperatorCache;
	}
}

IMPLEMENT_MODULE(Metasound::FMetasoundGeneratorModule, MetasoundGenerator);

