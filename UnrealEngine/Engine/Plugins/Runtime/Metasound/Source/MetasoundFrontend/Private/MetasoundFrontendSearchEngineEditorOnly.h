// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "MetasoundFrontendDocument.h"
#include "MetasoundFrontendQuery.h"
#include "MetasoundFrontendSearchEngine.h"
#include "MetasoundFrontendSearchEngineCore.h"

#if WITH_EDITORONLY_DATA

namespace Metasound
{
	namespace Frontend
	{
		// Policy for finding all registered metasound classes. 
		struct FFindAllClassesQueryPolicy
		{
			using ResultType = TArray<FMetasoundFrontendClass>;

			static FFrontendQuery CreateQuery();
			static ResultType BuildResult(const FFrontendQueryPartition& InPartition);
		};

		// Policy for finding all registered metasound classes, including deprecated classes. 
		struct FFindAllClassesIncludingAllVersionsQueryPolicy
		{
			using ResultType = TArray<FMetasoundFrontendClass>;

			static FFrontendQuery CreateQuery();
			static ResultType BuildResult(const FFrontendQueryPartition& InPartition);
		};

		// Policy for finding all registered metasound classes sorted by version 
		// and indexed by name.
		struct FFindClassesWithNameSortedQueryPolicy
		{
			using ResultType = TArray<FMetasoundFrontendClass>;

			static FFrontendQuery CreateQuery();
			static ResultType BuildResult(const FFrontendQueryPartition& InPartition);
		};

		// Policy for finding all registered metasound classes indexed by name.
		struct FFindClassesWithNameUnsortedQueryPolicy
		{
			using ResultType = TArray<FMetasoundFrontendClass>;

			static FFrontendQuery CreateQuery();
			static ResultType BuildResult(const FFrontendQueryPartition& InPartition);
		};

		// Policy for finding highest versioned metasound class by name.
		struct FFindClassWithHighestVersionQueryPolicy
		{
			using ResultType = FMetasoundFrontendClass;

			static FFrontendQuery CreateQuery();
			static ResultType BuildResult(const FFrontendQueryPartition& InPartition);
		};

		// Policy for finding all registered interfaces. 
		struct FFindAllInterfacesQueryPolicy
		{
			using ResultType = TArray<FMetasoundFrontendInterface>;

			static FFrontendQuery CreateQuery();
			static ResultType BuildResult(const FFrontendQueryPartition& InPartition);
		};

		// Policy for finding all registered interfaces (including deprecated). 
		struct FFindAllInterfacesIncludingAllVersionsQueryPolicy
		{
			using ResultType = TArray<FMetasoundFrontendInterface>;

			static FFrontendQuery CreateQuery();
			static TArray<FMetasoundFrontendInterface> BuildResult(const FFrontendQueryPartition& InPartition);
		};

		// To minimize runtime memory costs during gampeplay, some search engine
		// queries are only exposed when the editor only data is available. This
		// class supports the editor only functionality of the search engine.
		class FSearchEngineEditorOnly : public FSearchEngineCore
		{
			FSearchEngineEditorOnly(const FSearchEngineEditorOnly&) = delete;
			FSearchEngineEditorOnly(FSearchEngineEditorOnly&&) = delete;
			FSearchEngineEditorOnly& operator=(const FSearchEngineEditorOnly&) = delete;
			FSearchEngineEditorOnly& operator=(FSearchEngineEditorOnly&&) = delete;

			using Super = FSearchEngineCore;

		public:
			FSearchEngineEditorOnly() = default;
			virtual ~FSearchEngineEditorOnly() = default;

			virtual void Prime() override;
			virtual TArray<FMetasoundFrontendClass> FindAllClasses(bool bInIncludeAllVersions) override;
			virtual TArray<FMetasoundFrontendClass> FindClassesWithName(const FMetasoundFrontendClassName& InName, bool bInSortByVersion) override;
			virtual bool FindClassWithHighestVersion(const FMetasoundFrontendClassName& InName, FMetasoundFrontendClass& OutClass) override;
			virtual TArray<FMetasoundFrontendInterface> FindAllInterfaces(bool bInIncludeAllVersions) override;

		private:

			TSearchEngineQuery<FFindAllClassesQueryPolicy> FindAllClassesQuery;
			TSearchEngineQuery<FFindAllClassesIncludingAllVersionsQueryPolicy> FindAllClassesIncludingAllVersionsQuery;
			TSearchEngineQuery<FFindClassesWithNameUnsortedQueryPolicy> FindClassesWithNameUnsortedQuery;
			TSearchEngineQuery<FFindClassesWithNameSortedQueryPolicy> FindClassesWithNameSortedQuery;
			TSearchEngineQuery<FFindClassWithHighestVersionQueryPolicy> FindClassWithHighestVersionQuery;
			TSearchEngineQuery<FFindAllInterfacesQueryPolicy> FindAllInterfacesQuery;
			TSearchEngineQuery<FFindAllInterfacesIncludingAllVersionsQueryPolicy> FindAllInterfacesIncludingAllVersionsQuery;
		};
	}
}

#endif
