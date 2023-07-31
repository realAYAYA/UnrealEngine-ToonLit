// Copyright Epic Games, Inc. All Rights Reserved.


#include "MetasoundFrontendSearchEngineEditorOnly.h"

#include "Containers/Array.h"
#include "MetasoundFrontendArchetypeRegistry.h"
#include "MetasoundFrontendDocument.h"
#include "MetasoundFrontendQuery.h"
#include "MetasoundFrontendQuerySteps.h"
#include "MetasoundFrontendRegistryTransaction.h"
#include "MetasoundFrontendSearchEngineCore.h"
#include "MetasoundTrace.h"

#if WITH_EDITORONLY_DATA

namespace Metasound
{
	namespace Frontend
	{
		FFrontendQuery FFindAllClassesQueryPolicy::CreateQuery()
		{
			using namespace SearchEngineQuerySteps;
			FFrontendQuery Query;
			Query.AddStep<FNodeClassRegistrationEvents>()
				.AddStep<FMapRegistrationEventsToNodeRegistryKeys>()
				.AddStep<FReduceRegistrationEventsToCurrentStatus>()
				.AddStep<FTransformRegistrationEventsToClasses>()
				.AddStep<FRemoveDeprecatedClasses>()
				.AddStep<FMapToNull>();
			return Query;
		}

		TArray<FMetasoundFrontendClass> FFindAllClassesQueryPolicy::BuildResult(const FFrontendQueryPartition& InPartition)
		{
			using namespace SearchEngineQuerySteps;
			return BuildArrayOfClassesFromPartition(InPartition);
		}

		FFrontendQuery FFindAllClassesIncludingAllVersionsQueryPolicy::CreateQuery()
		{
			using namespace SearchEngineQuerySteps;
			FFrontendQuery Query;
			Query.AddStep<FNodeClassRegistrationEvents>()
				.AddStep<FMapRegistrationEventsToNodeRegistryKeys>()
				.AddStep<FReduceRegistrationEventsToCurrentStatus>()
				.AddStep<FTransformRegistrationEventsToClasses>()
				.AddStep<FMapToNull>();
			return Query;
		}

		TArray<FMetasoundFrontendClass> FFindAllClassesIncludingAllVersionsQueryPolicy::BuildResult(const FFrontendQueryPartition& InPartition)
		{
			using namespace SearchEngineQuerySteps;
			return BuildArrayOfClassesFromPartition(InPartition);
		}

		FFrontendQuery FFindClassesWithNameSortedQueryPolicy::CreateQuery()
		{
			using namespace SearchEngineQuerySteps;
			FFrontendQuery Query;
			Query.AddStep<FNodeClassRegistrationEvents>()
				.AddStep<FMapRegistrationEventsToNodeRegistryKeys>()
				.AddStep<FReduceRegistrationEventsToCurrentStatus>()
				.AddStep<FTransformRegistrationEventsToClasses>()
				.AddStep<FMapClassesToClassName>()
				.AddStep<FSortClassesByVersion>();
			return Query;
		}

		TArray<FMetasoundFrontendClass> FFindClassesWithNameSortedQueryPolicy::BuildResult(const FFrontendQueryPartition& InPartition)
		{
			using namespace SearchEngineQuerySteps;
			return BuildArrayOfClassesFromPartition(InPartition);
		}

		FFrontendQuery FFindClassesWithNameUnsortedQueryPolicy::CreateQuery()
		{
			using namespace SearchEngineQuerySteps;
			FFrontendQuery Query;
			Query.AddStep<FNodeClassRegistrationEvents>()
				.AddStep<FMapRegistrationEventsToNodeRegistryKeys>()
				.AddStep<FReduceRegistrationEventsToCurrentStatus>()
				.AddStep<FTransformRegistrationEventsToClasses>()
				.AddStep<FMapClassesToClassName>();
			return Query;
		}

		TArray<FMetasoundFrontendClass> FFindClassesWithNameUnsortedQueryPolicy::BuildResult(const FFrontendQueryPartition& InPartition)
		{
			using namespace SearchEngineQuerySteps;
			return BuildArrayOfClassesFromPartition(InPartition);
		}

		FFrontendQuery FFindClassWithHighestVersionQueryPolicy::CreateQuery()
		{
			using namespace SearchEngineQuerySteps;
			FFrontendQuery Query;
			Query.AddStep<FNodeClassRegistrationEvents>()
				.AddStep<FMapRegistrationEventsToNodeRegistryKeys>()
				.AddStep<FReduceRegistrationEventsToCurrentStatus>()
				.AddStep<FTransformRegistrationEventsToClasses>()
				.AddStep<FRemoveDeprecatedClasses>()
				.AddStep<FMapToFullClassName>()
				.AddStep<FReduceClassesToHighestVersion>();
			return Query;
		}

		FMetasoundFrontendClass FFindClassWithHighestVersionQueryPolicy::BuildResult(const FFrontendQueryPartition& InPartition)
		{
			using namespace SearchEngineQuerySteps;
			return BuildSingleClassFromPartition(InPartition);
		}

		FFrontendQuery FFindAllInterfacesQueryPolicy::CreateQuery()
		{
			using namespace SearchEngineQuerySteps;
			FFrontendQuery Query;
			Query.AddStep<FInterfaceRegistryTransactionSource>()
				.AddStep<FMapInterfaceRegistryTransactionsToInterfaceRegistryKeys>()
				.AddStep<FReduceInterfaceRegistryTransactionsToCurrentStatus>()
				.AddStep<FTransformInterfaceRegistryTransactionToInterface>()
				.AddStep<FMapInterfaceToInterfaceNameAndMajorVersion>()
				.AddStep<FReduceInterfacesToHighestVersion>()
				.AddStep<FMapToNull>();
			return Query;
		}

		TArray<FMetasoundFrontendInterface> FFindAllInterfacesQueryPolicy::BuildResult(const FFrontendQueryPartition& InPartition)
		{
			using namespace SearchEngineQuerySteps;
			return BuildArrayOfInterfacesFromPartition(InPartition);
		}

		FFrontendQuery FFindAllInterfacesIncludingAllVersionsQueryPolicy::CreateQuery()
		{
			using namespace SearchEngineQuerySteps;
			FFrontendQuery Query;
			Query.AddStep<FInterfaceRegistryTransactionSource>()
				.AddStep<FMapInterfaceRegistryTransactionsToInterfaceRegistryKeys>()
				.AddStep<FReduceInterfaceRegistryTransactionsToCurrentStatus>()
				.AddStep<FTransformInterfaceRegistryTransactionToInterface>()
				.AddStep<FMapToNull>();
			return Query;
		}

		TArray<FMetasoundFrontendInterface> FFindAllInterfacesIncludingAllVersionsQueryPolicy::BuildResult(const FFrontendQueryPartition& InPartition)
		{
			using namespace SearchEngineQuerySteps;
			return BuildArrayOfInterfacesFromPartition(InPartition);
		}

		void FSearchEngineEditorOnly::Prime()
		{
			Super::Prime();
			{
				METASOUND_LLM_SCOPE;
				METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(metasound::FSearchEngineEditorOnly::Prime);

				FindAllClassesQuery.Prime();
				FindAllClassesIncludingAllVersionsQuery.Prime();
				FindClassesWithNameUnsortedQuery.Prime();
				FindClassesWithNameSortedQuery.Prime();
				FindClassWithHighestVersionQuery.Prime();
				FindAllInterfacesIncludingAllVersionsQuery.Prime();
				FindAllInterfacesQuery.Prime();
			}
		}

		TArray<FMetasoundFrontendClass> FSearchEngineEditorOnly::FindAllClasses(bool bInIncludeAllVersions)
		{
			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(metasound::FSearchEngineEditorOnly::FindAllClasses);

			FFrontendQueryKey NullKey;
			if (bInIncludeAllVersions)
			{
				return FindAllClassesIncludingAllVersionsQuery.UpdateAndFindResult(NullKey);
			}
			else
			{
				return FindAllClassesQuery.UpdateAndFindResult(NullKey);
			}
		}

		TArray<FMetasoundFrontendClass> FSearchEngineEditorOnly::FindClassesWithName(const FMetasoundFrontendClassName& InName, bool bInSortByVersion)
		{
			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(metasound::FSearchEngineEditorOnly::FindClassesWithName);

			const FFrontendQueryKey Key(InName.GetFullName());
			if (bInSortByVersion)
			{
				return FindClassesWithNameSortedQuery.UpdateAndFindResult(Key);
			}
			else 
			{
				return FindClassesWithNameUnsortedQuery.UpdateAndFindResult(Key);
			}
		}

		bool FSearchEngineEditorOnly::FindClassWithHighestVersion(const FMetasoundFrontendClassName& InName, FMetasoundFrontendClass& OutClass)
		{
			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(metasound::FSearchEngineEditorOnly::FindClassWithHighestVersion);

			const FFrontendQueryKey Key{InName.GetFullName()};
			return FindClassWithHighestVersionQuery.UpdateAndFindResult(Key, OutClass);
		}

		TArray<FMetasoundFrontendInterface> FSearchEngineEditorOnly::FindAllInterfaces(bool bInIncludeAllVersions)
		{
			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(metasound::FSearchEngineEditorOnly::FindAllInterfaces);

			const FFrontendQueryKey NullKey;
			if (bInIncludeAllVersions)
			{
				return FindAllInterfacesIncludingAllVersionsQuery.UpdateAndFindResult(NullKey);
			}
			else
			{
				return FindAllInterfacesQuery.UpdateAndFindResult(NullKey);
			}
		}

	}
}

#endif
