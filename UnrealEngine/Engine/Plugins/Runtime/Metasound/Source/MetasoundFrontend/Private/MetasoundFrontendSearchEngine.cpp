// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundFrontendSearchEngine.h"
#include "MetasoundFrontendSearchEngineCore.h"
#include "MetasoundFrontendSearchEngineEditorOnly.h"

#include "Algo/MaxElement.h"
#include "MetasoundFrontendArchetypeRegistry.h"
#include "MetasoundFrontendQuery.h"
#include "MetasoundFrontendQuerySteps.h"
#include "MetasoundFrontendRegistryTransaction.h"
#include "MetasoundTrace.h"
#include "MetasoundAssetManager.h"

namespace Metasound
{
	namespace Frontend
	{
		ISearchEngine& ISearchEngine::Get()
		{
#if WITH_EDITORONLY_DATA
			static FSearchEngineEditorOnly SearchEngine;
#else
			static FSearchEngineCore SearchEngine;
#endif
			return SearchEngine;
		}
	}
}

