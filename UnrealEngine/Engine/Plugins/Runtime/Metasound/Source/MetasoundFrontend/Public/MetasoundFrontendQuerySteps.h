// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetasoundDataReference.h"
#include "MetasoundFrontend.h"
#include "MetasoundFrontendQuery.h"
#include "MetasoundFrontendRegistries.h"
#include "Misc/Guid.h"
#include "Templates/PimplPtr.h"

namespace Metasound
{
	// Forward declare private implementation
	class FNodeClassRegistrationEventsPimpl;

	class FMapToNull : public IFrontendQueryMapStep
	{
	public:
		virtual FFrontendQueryKey Map(const FFrontendQueryEntry& InEntry) const override
		{
			return FFrontendQueryKey();
		}
	};

	/** Streams node classes that have been newly registered or unregistered since last call to Stream()
	 */
	class METASOUNDFRONTEND_API FNodeClassRegistrationEvents : public IFrontendQueryStreamStep
	{
	public:
		FNodeClassRegistrationEvents();
		virtual void Stream(TArray<FFrontendQueryValue>& OutValues) override;

	private:
		TPimplPtr<FNodeClassRegistrationEventsPimpl> Pimpl;
	};

	/** Partitions node registration events by their node registration keys. */
	class METASOUNDFRONTEND_API FMapRegistrationEventsToNodeRegistryKeys : public IFrontendQueryMapStep
	{
	public:
		virtual FFrontendQueryKey Map(const FFrontendQueryEntry& InEntry) const override;
	};

	/** Reduces registration events mapped to the same key by inspecting their add/remove state in
	 * order to determine their final state. If an item has been added more than it has been removed,
	 * then it is added to the output. Otherwise, it is omitted. */
	class METASOUNDFRONTEND_API FReduceRegistrationEventsToCurrentStatus: public IFrontendQueryReduceStep
	{
	public:
		virtual void Reduce(const FFrontendQueryKey& InKey, FFrontendQueryPartition& InOutEntries) const override;
	private:
		using FTimeType = Frontend::FNodeRegistryTransaction::FTimeType;
		static FTimeType GetTransactionTimestamp(const FFrontendQueryEntry& InEntry);
		static bool IsValidTransactionOfType(Frontend::FNodeRegistryTransaction::ETransactionType InType, const FFrontendQueryEntry* InEntry);
	};

	/** Transforms a registration event into a FMetasoundFrontendClass. */
	class METASOUNDFRONTEND_API FTransformRegistrationEventsToClasses : public IFrontendQueryTransformStep
	{
	public:
		virtual void Transform(FFrontendQueryEntry::FValue& InValue) const override;
	};

	class METASOUNDFRONTEND_API FFilterClassesByInputVertexDataType : public IFrontendQueryFilterStep
	{
	public: 
		template<typename DataType>
		FFilterClassesByInputVertexDataType()
		:	FFilterClassesByInputVertexDataType(GetMetasoundDataTypeName<DataType>())
		{
		}

		FFilterClassesByInputVertexDataType(const FName& InTypeName);

		virtual bool Filter(const FFrontendQueryEntry& InEntry) const override;

	private:
		FName InputVertexTypeName;
	};

	class METASOUNDFRONTEND_API FFilterClassesByOutputVertexDataType : public IFrontendQueryFilterStep
	{
	public: 
		template<typename DataType>
		FFilterClassesByOutputVertexDataType()
		:	FFilterClassesByOutputVertexDataType(GetMetasoundDataTypeName<DataType>())
		{
		}

		FFilterClassesByOutputVertexDataType(const FName& InTypeName);

		virtual bool Filter(const FFrontendQueryEntry& InEntry) const override;

	private:
		FName OutputVertexTypeName;
	};

	class METASOUNDFRONTEND_API FMapClassesToClassName : public IFrontendQueryMapStep
	{
	public: 
		virtual FFrontendQueryKey Map(const FFrontendQueryEntry& InEntry) const override;

	};

	class METASOUNDFRONTEND_API FFilterClassesByClassID : public IFrontendQueryFilterStep
	{
	public:
		FFilterClassesByClassID(const FGuid InClassID);

		virtual bool Filter(const FFrontendQueryEntry& InEntry) const override;

	private:
		FGuid ClassID;
	};

	class METASOUNDFRONTEND_API FMapToFullClassName : public IFrontendQueryMapStep
	{
	public:
		virtual FFrontendQueryKey Map(const FFrontendQueryEntry& InEntry) const override;
	};

	class METASOUNDFRONTEND_API FReduceClassesToHighestVersion : public IFrontendQueryReduceStep
	{
	public:
		virtual void Reduce(const FFrontendQueryKey& InKey, FFrontendQueryPartition& InOutEntries) const override;
	};

	class METASOUNDFRONTEND_API FSortClassesByVersion : public IFrontendQuerySortStep
	{
	public:
		virtual bool Sort(const FFrontendQueryEntry& InEntryLHS, const FFrontendQueryEntry& InEntryRHS) const override;
	};
}
