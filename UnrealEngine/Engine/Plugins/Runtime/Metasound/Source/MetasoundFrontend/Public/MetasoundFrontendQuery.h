// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/SortedMap.h"
#include "Math/NumericLimits.h"
#include "MetasoundFrontendDocument.h"
#include "MetasoundFrontendRegistries.h"
#include "MetasoundFrontendArchetypeRegistry.h"
#include "Misc/TVariant.h"

/** MetaSound Frontend Query
 *
 * MetaSound Frontend Query provides a way to systematically organize and update
 * streaming data associated with the MetaSound Frontend. It is a streaming MapReduce 
 * framework for querying streams of data (https://en.wikipedia.org/wiki/MapReduce) 
 *
 * While it does not support the computational parallelism commonly found in MapReduce 
 * frameworks, it does offer:
 * 	- An encapsulated and reusable set of methods for manipulating streamed data.
 *  - Support for incremental updates (a.k.a. streamed data).
 * 	- An indexed output for efficient lookup. 
 *
 * Data within MetaSound Frontend Query is organized similarly to a NoSQL database
 * (https://en.wikipedia.org/wiki/NoSQL). Each object (FFrontendQueryEntry) is 
 * assigned a unique ID. Keys (FFrontendQueryKey) are associated with sets of entries
 * (FFrontendQueryPartition) and allow partitions to be retrieved efficiently. 
 * Each partition holds a set of entries which is determined by the steps in the 
 * query (FFrontendQuery). A FFrontendQueryKey or FFrontendQueryValue represent
 * one of multiple types by using a TVariant<>.
 *
 * A query contains a sequence of steps that get executed on streaming data. The 
 * various types of steps reflect common operations performed in MapReduce and 
 * NoSQL database queries. 
 *
 * Step Types
 * 	Stream: 	Produce a stream of FFrontendQueryValues.
 * 	Map:		Map a FFrontendQueryEntry to a partition associated with a FFrontendQueryKey.
 * 	Reduce: 	Apply an incremental summarization of a FFrontendQueryPartition.
 * 	Transform:	Alter a FFrontendQueryValue.
 * 	Filter:		Remove FFrontendQueryValues with a test function.
 * 	Score:		Calculate a score for a FFrontendQueryValue.
 * 	Sort:		Sort a FFrontendQueryPartition.
 * 	Limit:		Limit the size of a FFrontendQueryPartition.
 */

namespace Metasound
{
	/** FFrontendQueryKey allows entries to be partitioned by their key. A key
	 * can be created by default constructor, int32, FString or FName.
	 */
	struct METASOUNDFRONTEND_API FFrontendQueryKey
	{
		FFrontendQueryKey();
		explicit FFrontendQueryKey(int32 InKey);
		explicit FFrontendQueryKey(const FString& InKey);
		explicit FFrontendQueryKey(const FName& InKey);

		FFrontendQueryKey(const FFrontendQueryKey&) = default;
		FFrontendQueryKey& operator=(const FFrontendQueryKey&) = default;
		FFrontendQueryKey(FFrontendQueryKey&&) = default;
		FFrontendQueryKey& operator=(FFrontendQueryKey&&) = default;

		friend bool operator==(const FFrontendQueryKey& InLHS, const FFrontendQueryKey& InRHS);
		friend bool operator!=(const FFrontendQueryKey& InLHS, const FFrontendQueryKey& InRHS);
		friend bool operator<(const FFrontendQueryKey& InLHS, const FFrontendQueryKey& InRHS);
		friend uint32 GetTypeHash(const FFrontendQueryKey& InKey);
		
		bool IsNull() const;

	private:
		struct FNull{};

		using FKeyType = TVariant<FNull, int32, FString, FName>;
		FKeyType Key;
		uint32 Hash;
	};

	/** A FFrontendQueryValue contains data of interest. */
	using FFrontendQueryValue = TVariant<FMetasoundFrontendVersion, Frontend::FNodeRegistryTransaction, FMetasoundFrontendClass, Frontend::FInterfaceRegistryTransaction, FMetasoundFrontendInterface>;

	/** FFrontendQueryEntry represents one value in the query. It contains an ID,
	 * value and score.  */
	struct METASOUNDFRONTEND_API FFrontendQueryEntry
	{
		using FValue = FFrontendQueryValue;

		FGuid ID;
		FValue Value;
		float Score = 0.f;

		friend uint32 GetTypeHash(const FFrontendQueryEntry& InEntry);
		friend bool operator==(const FFrontendQueryEntry& InLHS, const FFrontendQueryEntry& InRHS);
	};

	/** A FFrontendQueryPartition represents a set of entries associated with a 
	 * single FFrontendQueryKey. */
	
	using FFrontendQueryPartition = TArray<FFrontendQueryEntry, TInlineAllocator<1>>;

	/** A FFrontendQuerySelection holds a map of keys to partitions. */
	using FFrontendQuerySelection = TSortedMap<FFrontendQueryKey, FFrontendQueryPartition>;

	/** Interface for an individual step in a query */
	class IFrontendQueryStep
	{
		public:
			virtual ~IFrontendQueryStep() = default;
	};

	/** Interface for a query step which streams new entries. */
	class IFrontendQueryStreamStep : public IFrontendQueryStep
	{
		public:
			virtual ~IFrontendQueryStreamStep() = default;
			virtual void Stream(TArray<FFrontendQueryValue>& OutEntries) = 0;
	};

	/** Interface for a query step which transforms an entry's value. */
	class IFrontendQueryTransformStep : public IFrontendQueryStep
	{
		public:
			virtual ~IFrontendQueryTransformStep() = default;
			virtual void Transform(FFrontendQueryEntry::FValue& InValue) const = 0;
	};

	/** Interface for a query step which maps entries to keys. */
	class IFrontendQueryMapStep : public IFrontendQueryStep
	{
		public:
			virtual ~IFrontendQueryMapStep() = default;
			virtual FFrontendQueryKey Map(const FFrontendQueryEntry& InEntry) const = 0;
	};

	/** Interface for a query step which reduces entries with the same key. */
	class IFrontendQueryReduceStep : public IFrontendQueryStep
	{
		public:
			virtual ~IFrontendQueryReduceStep() = default;
			virtual void Reduce(const FFrontendQueryKey& InKey, FFrontendQueryPartition& InOutEntries) const = 0;
	};

	/** Interface for a query step which filters entries. */
	class IFrontendQueryFilterStep : public IFrontendQueryStep
	{
		public:
			virtual ~IFrontendQueryFilterStep() = default;
			virtual bool Filter(const FFrontendQueryEntry& InEntry) const = 0;
	};

	/** Interface for a query step which scores entries. */
	class IFrontendQueryScoreStep : public IFrontendQueryStep
	{
		public:
			virtual ~IFrontendQueryScoreStep() = default;
			virtual float Score(const FFrontendQueryEntry& InEntry) const = 0;
	};

	/** Interface for a query step which sorts entries. */
	class IFrontendQuerySortStep : public IFrontendQueryStep
	{
		public:
			virtual ~IFrontendQuerySortStep() = default;
			virtual bool Sort(const FFrontendQueryEntry& InEntryLHS, const FFrontendQueryEntry& InEntryRHS) const = 0;
	};
	
	class IFrontendQueryLimitStep : public IFrontendQueryStep
	{
		public:
			virtual ~IFrontendQueryLimitStep() = default;
			virtual int32 Limit() const = 0;
	};

	/** FFrontendQueryStep wraps all the support IFrontenQueryStep interfaces 
	 * and supplies unified `ExecuteStep(...)` member function.
	 */
	class METASOUNDFRONTEND_API FFrontendQueryStep
	{
		FFrontendQueryStep() = delete;

	public:
		// Represents an incremental update to the existing data.
		struct FIncremental
		{
			// Keys that are affected by this incremental update.
			TSet<FFrontendQueryKey> ActiveKeys;
			// The selection being manipulated in the incremental update.
			FFrontendQuerySelection ActiveSelection;

			// Keys that contain active removals.
			TSet<FFrontendQueryKey> ActiveRemovalKeys;
			// Selection containing entries to remove during a merge.
			FFrontendQuerySelection ActiveRemovalSelection;
		};

		/* Interface for executing a step in the query. */
		struct IStepExecuter
		{
			virtual ~IStepExecuter() = default;

			// Merge new result with the existing result from this step.
			virtual void Merge(FIncremental& InOutIncremental, FFrontendQuerySelection& InOutSelection) const = 0;

			// Execute step. 
			virtual void Execute(TSet<FFrontendQueryKey>& InOutUpdatedKeys, FFrontendQuerySelection& InOutResult) const = 0;

			// Returns true if a steps result is conditioned on the composition of a partition.
			//
			// Most steps are only dependent upon individual entries, but some
			// (Reduce, Limit, Sort) are specifically dependent upon the composition
			// of the Partition. They require special handling during incremental
			// updates.
			virtual bool IsDependentOnPartitionComposition() const = 0;

			// Return true if the step can be used to process downstream removals.
			virtual bool CanProcessRemovals() const = 0;

			// Return true if the step can produce new entries. This information
			// is used to early-out on queries with no new entries. 
			virtual bool CanProduceEntries() const = 0;
		};

		using FStreamFunction = TUniqueFunction<void (TArray<FFrontendQueryValue>&)>;
		using FTransformFunction = TFunction<void (FFrontendQueryEntry::FValue&)>;
		using FMapFunction = TFunction<FFrontendQueryKey (const FFrontendQueryEntry&)>;
		using FReduceFunction = TFunction<void (const FFrontendQueryKey& InKey, FFrontendQueryPartition& InOutEntries)>;
		using FFilterFunction = TFunction<bool (const FFrontendQueryEntry&)>;
		using FScoreFunction = TFunction<float (const FFrontendQueryEntry&)>;
		using FSortFunction = TFunction<bool (const FFrontendQueryEntry& InEntryLHS, const FFrontendQueryEntry& InEntryRHS)>;
		using FLimitFunction = TFunction<int32 ()>;

		/** Create query step using TFunction or lambda. */
		FFrontendQueryStep(FStreamFunction&& InFunc);
		FFrontendQueryStep(FTransformFunction&& InFunc);
		FFrontendQueryStep(FMapFunction&& InFunc);
		FFrontendQueryStep(FReduceFunction&& InFunc);
		FFrontendQueryStep(FFilterFunction&& InFilt);
		FFrontendQueryStep(FScoreFunction&& InScore);
		FFrontendQueryStep(FSortFunction&& InSort);
		FFrontendQueryStep(FLimitFunction&& InLimit);

		/** Create a query step using a IFrontedQueryStep */
		FFrontendQueryStep(TUniquePtr<IFrontendQueryStreamStep>&& InStep);
		FFrontendQueryStep(TUniquePtr<IFrontendQueryTransformStep>&& InStep);
		FFrontendQueryStep(TUniquePtr<IFrontendQueryMapStep>&& InStep);
		FFrontendQueryStep(TUniquePtr<IFrontendQueryReduceStep>&& InStep);
		FFrontendQueryStep(TUniquePtr<IFrontendQueryFilterStep>&& InStep);
		FFrontendQueryStep(TUniquePtr<IFrontendQueryScoreStep>&& InStep);
		FFrontendQueryStep(TUniquePtr<IFrontendQuerySortStep>&& InStep);
		FFrontendQueryStep(TUniquePtr<IFrontendQueryLimitStep>&& InStep);


		// Merge an incremental result with the prior result from this step.
		void Merge(FIncremental& InIncremental, FFrontendQuerySelection& InOutSelection) const;

		// Execute step. Assume not other prior results exist.
		void Execute(TSet<FFrontendQueryKey>& InOutUpdatedKeys, FFrontendQuerySelection& InOutResult) const;

		// Returns true if a steps result is conditioned on the composition of a partition.
		//
		// Most steps are only dependent upon individual entries, but some
		// (Reduce, Limit, Sort) are specifically dependent upon the composition
		// of the Partition. They require special handling during incremental
		// updates.
		bool IsDependentOnPartitionComposition() const;

		// Return true if the step can be used to process downstream removals.
		bool CanProcessRemovals() const;

		// Return true if the step can produce new entries. This information
		// is used to early-out on queries with no new entries. 
		bool CanProduceEntries() const;

	private:
		TUniquePtr<IStepExecuter> StepExecuter;
	};

	/** FFrontendQuery contains a set of query steps which produce a FFrontendQuerySelectionView */
	class METASOUNDFRONTEND_API FFrontendQuery
	{
	public:

		using FStreamFunction = FFrontendQueryStep::FStreamFunction;
		using FTransformFunction = FFrontendQueryStep::FTransformFunction;
		using FMapFunction = FFrontendQueryStep::FMapFunction;
		using FReduceFunction = FFrontendQueryStep::FReduceFunction;
		using FFilterFunction = FFrontendQueryStep::FFilterFunction;
		using FScoreFunction = FFrontendQueryStep::FScoreFunction;
		using FSortFunction = FFrontendQueryStep::FSortFunction;
		using FLimitFunction = FFrontendQueryStep::FLimitFunction;

		FFrontendQuery();
		FFrontendQuery(FFrontendQuery&&) = default;
		FFrontendQuery& operator=(FFrontendQuery&&) = default;

		FFrontendQuery(const FFrontendQuery&) = delete;
		FFrontendQuery& operator=(const FFrontendQuery&) = delete;


		/** Add a step to the query. */
		template<typename StepType, typename... ArgTypes>
		FFrontendQuery& AddStep(ArgTypes&&... Args)
		{
			return AddStep(MakeUnique<FFrontendQueryStep>(MakeUnique<StepType>(Forward<ArgTypes>(Args)...)));
		}

		template<typename FuncType>
		FFrontendQuery& AddFunctionStep(FuncType&& InFunc)
		{
			return AddStep(MakeUnique<FFrontendQueryStep>(MoveTemp(InFunc)));
		}

		FFrontendQuery& AddStreamLambdaStep(FStreamFunction&& InFunc);
		FFrontendQuery& AddTransformLambdaStep(FTransformFunction&& InFunc);
		FFrontendQuery& AddMapLambdaStep(FMapFunction&& InFunc);
		FFrontendQuery& AddReduceLambdaStep(FReduceFunction&& InFunc);
		FFrontendQuery& AddFilterLambdaStep(FFilterFunction&& InFunc);
		FFrontendQuery& AddScoreLambdaStep(FScoreFunction&& InFunc);
		FFrontendQuery& AddSortLambdaStep(FSortFunction&& InFunc);
		FFrontendQuery& AddLimitLambdaStep(FLimitFunction&& InFunc);

		/** Add a step to the query. */
		FFrontendQuery& AddStep(TUniquePtr<FFrontendQueryStep>&& InStep);

		/** Calls all steps in the query and returns the selection. */
		const FFrontendQuerySelection& Update(TSet<FFrontendQueryKey>& OutUpdatedKeys);
		const FFrontendQuerySelection& Update();

		/** Returns the current result. */
		const FFrontendQuerySelection& GetSelection() const;

	private:
		using FIncremental = FFrontendQueryStep::FIncremental;

		void UpdateInternal(TSet<FFrontendQueryKey>& OutUpdatedKeys);
		void MergeInternal(FFrontendQueryStep& Step, FIncremental& InOutIncremental, FFrontendQuerySelection& InOutMergedSelection);

		TSharedRef<FFrontendQuerySelection, ESPMode::ThreadSafe> Result;

		struct FStepInfo
		{
			TUniquePtr<FFrontendQueryStep> Step;
			FFrontendQuerySelection OutputCache;
			bool bMergeAndCacheOutput = false;
			bool bProcessRemovals = false;
		};

		void AppendPartitions(const TSet<FFrontendQueryKey>& InKeysToAppend, const FFrontendQuerySelection& InSelection, TSet<FFrontendQueryKey>& OutKeysModified, FFrontendQuerySelection& OutSelection) const;

		TArray<FStepInfo> Steps;
		int32 FinalEntryProducingStepIndex = INDEX_NONE;
	};
}
