// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundOperatorCache.h"

#include "Async/Async.h"
#include "Async/TaskGraphInterfaces.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "HAL/CriticalSection.h"
#include "HAL/IConsoleManager.h"
#include "MetasoundGeneratorModuleImpl.h"
#include "MetasoundGeneratorBuilder.h"
#include "MetasoundTrace.h"
#include "Misc/Guid.h"
#include "Misc/ScopeLock.h"
#include "Modules/ModuleManager.h"
#include "ProfilingDebugging/CountersTrace.h"
#include "Templates/UniquePtr.h"

namespace Metasound
{
#if METASOUND_OPERATORCACHEPROFILER_ENABLED
	TRACE_DECLARE_INT_COUNTER(MetaSound_OperatorPool_NumOperators, TEXT("MetaSound/OperatorPool/NumOperatorsInPool"));
	TRACE_DECLARE_FLOAT_COUNTER(MetaSound_OperatorPool_HitRatio, TEXT("MetaSound/OperatorPool/HitRatio"));
	TRACE_DECLARE_FLOAT_COUNTER(MetaSound_OperatorPool_WindowedHitRatio, TEXT("MetaSound/OperatorPool/WindowedHitRatio"));
#endif // #if METASOUND_OPERATORCACHEPROFILER_ENABLED

	namespace OperatorPoolPrivate
	{
		static bool bMetasoundPoolSyncGraphRetrieval = true;
		FAutoConsoleVariableRef CVarMetasoundPoolSyncGraphRetrieval(
			TEXT("au.MetaSound.OperatorPoolSyncGraphRetrieval"),
			bMetasoundPoolSyncGraphRetrieval,
			TEXT("Retrieves graph on the requesting thread prior to asynchronous task to create instance.\n"),
			ECVF_Default);

#if METASOUND_OPERATORCACHEPROFILER_ENABLED
		static std::atomic<uint32> CacheHitCount = 0;
		static std::atomic<uint32> CacheAttemptCount = 0;

		static float MetasoundPoolHitRateWindowSecondsCVar = 1.0f;
		FAutoConsoleVariableRef CVarMetasoundPoolHitRateWindowSeconds(
			TEXT("au.MetaSound.OperatorPoolHitRateWindowSeconds"),
			MetasoundPoolHitRateWindowSecondsCVar,
			TEXT("Control how long hit/miss results matter for the success rate reporting.\n"),
			ECVF_Default);

		double GetHitRatio()
		{
			uint32 NumHits = CacheHitCount;
			uint32 Total = CacheAttemptCount;
			if (Total > 0)
			{
				return static_cast<double>(NumHits) / static_cast<double>(Total);
			}
			else
			{
				return 0.f;
			}
		}

		FWindowedHitRate::FWindowedHitRate()
		: CurrTTLSeconds(MetasoundPoolHitRateWindowSecondsCVar)
		{
		}

		void FWindowedHitRate::Update()
		{
			if (bIsFirstUpdate)
			{
				bIsFirstUpdate = false;
				FirstUpdate();
			}

			if (CurrTTLSeconds != MetasoundPoolHitRateWindowSecondsCVar)
			{
				SetWindowLength(MetasoundPoolHitRateWindowSecondsCVar);
			}
	
			// Incorporate latest results
			// note:	there a sliver of a race condition here between the 2 values
			// 			but we should be able to afford the occasional off-by-one and
			// 			avoid mutex contention
			IntermediateResult Result( { CurrHitCount, CurrTotal });
			CurrHitCount = 0;
			CurrTotal = 0;
			RunningHitCount += Result.NumHits;
			RunningTotal += Result.Total;
			Result.TTLSeconds = CurrTTLSeconds;
			History.Emplace(MoveTemp(Result));
	
			// calculate delta time, update time
			const uint64 CurrentTimeCycles = FPlatformTime::Cycles64();
			const float DeltaTimeSeconds = FPlatformTime::ToSeconds64(CurrentTimeCycles - PreviousTimeCycles);
			PreviousTimeCycles = CurrentTimeCycles;
	
			// tick down intermediate results, remove any which have expired
			TickResults(DeltaTimeSeconds);
			
			if (RunningTotal > 0)
			{
				const float HitRatio = RunningHitCount / static_cast<float>(RunningTotal);
				TRACE_COUNTER_SET(MetaSound_OperatorPool_WindowedHitRatio, HitRatio);
			}
		}

		void FWindowedHitRate::AddHit()
		{
			++CurrHitCount;
			++CurrTotal;
		}
	
		void FWindowedHitRate::AddMiss()
		{
			++CurrTotal;
		}
	
		void FWindowedHitRate::SetWindowLength(const float InNewLengthSeconds)
		{
			if (!ensure(InNewLengthSeconds))
			{
				return;
			}
	
			const float Delta = InNewLengthSeconds - CurrTTLSeconds;
			CurrTTLSeconds = InNewLengthSeconds;
	
			// Delta is positive if the new length is longer than the old length.
			// perform an "inverse" Tick to adjust the TTLs in the History
			TickResults(-Delta);
		}
	
		void FWindowedHitRate::FirstUpdate()
		{
			PreviousTimeCycles = FPlatformTime::Cycles64();
		}
	
		void FWindowedHitRate::ExpireResult(const IntermediateResult& InResultToExpire)
		{	
			RunningHitCount -= InResultToExpire.NumHits;
			RunningTotal -= InResultToExpire.Total;
		}
	
		void FWindowedHitRate::TickResults(const float DeltaTimeSeconds)
		{
			// tick down intermediate results, remove any which have expired
			const int32 NumEntries = History.Num();
			for (int i = NumEntries - 1; i >= 0; --i)
			{
				IntermediateResult& Result = History[i];
				Result.TTLSeconds -= DeltaTimeSeconds;
				if (Result.TTLSeconds < 0.f)
				{
					ExpireResult(Result);
					History.RemoveAtSwap(i); // note: Result ref is no longer valid!
				}
			}
		}
#endif // #if METASOUND_OPERATORCACHEPROFILER_ENABLED
	} // namespace OperatorPoolPrivate


	FOperatorBuildData::FOperatorBuildData(
		  FMetasoundGeneratorInitParams&& InInitParams
		, Frontend::FGraphRegistryKey InRegistryKey
		, FGuid InAssetClassID
		, int32 InNumInstances
	)
	: InitParams(InInitParams)
	, RegistryKey(InRegistryKey)
	, AssetClassID(InAssetClassID)
	, NumInstances(InNumInstances)
	{
	}


	FOperatorPool::FOperatorPool(const FOperatorPoolSettings& InSettings)
	: Settings(InSettings)
	{
	}

	FOperatorPool::~FOperatorPool()
	{
		CancelAllBuildEvents();
	}

	FOperatorAndInputs FOperatorPool::ClaimOperator(const FGuid& InOperatorID)
	{
		FScopeLock Lock(&CriticalSection);

		FOperatorAndInputs OpAndInputs;

		bool bCacheHit = false;
		if (TArray<FOperatorAndInputs>* OperatorsWithID = Operators.Find(InOperatorID))
		{
			if (OperatorsWithID->Num() > 0)
			{
				OpAndInputs = OperatorsWithID->Pop();
				Stack.RemoveAt(Stack.FindLast(InOperatorID));
#if METASOUND_OPERATORCACHEPROFILER_ENABLED
				OperatorPoolPrivate::CacheHitCount++;
				bCacheHit = true;
				TRACE_COUNTER_DECREMENT(MetaSound_OperatorPool_NumOperators);
#endif
			}
		}

#if METASOUND_OPERATORCACHEPROFILER_ENABLED
		bCacheHit? HitRateTracker.AddHit() : HitRateTracker.AddMiss();
		OperatorPoolPrivate::CacheAttemptCount++;
		TRACE_COUNTER_SET(MetaSound_OperatorPool_HitRatio, OperatorPoolPrivate::GetHitRatio());
#endif
		return OpAndInputs;
	}

	void FOperatorPool::AddOperator(const FGuid& InOperatorID, TUniquePtr<IOperator>&& InOperator, FInputVertexInterfaceData&& InInputData)
	{
		AddOperator(InOperatorID, { MoveTemp(InOperator), MoveTemp(InInputData) });
	}

	void FOperatorPool::AddOperator(const FGuid& InOperatorID, FOperatorAndInputs && OperatorAndInputs)
	{
		if (!OperatorAndInputs.Operator.IsValid())
		{
			return;
		}

		FScopeLock Lock(&CriticalSection);
		Stack.Add(InOperatorID);
#if METASOUND_OPERATORCACHEPROFILER_ENABLED
		TRACE_COUNTER_INCREMENT(MetaSound_OperatorPool_NumOperators);
#endif // #if METASOUND_OPERATORCACHEPROFILER_ENABLED

		if (TArray<FOperatorAndInputs>* OperatorArray = Operators.Find(InOperatorID))
		{
			// add to existing array
			OperatorArray->Add(MoveTemp(OperatorAndInputs));
		}
		else
		{
			// create a new array and add it to the map
			TArray<FOperatorAndInputs> NewOperatorArray;
			NewOperatorArray.Add(MoveTemp(OperatorAndInputs));
			Operators.Add(InOperatorID, MoveTemp(NewOperatorArray));
		}

		Trim();
	}

	void FOperatorPool::BuildAndAddAsync(TUniqueFunction<void()>&& InBuildFunc)
	{
		struct FBuildAndAddOpTask : public FAsyncGraphTaskBase
		{
			TUniqueFunction<void()> TaskFunc;

			FBuildAndAddOpTask(TUniqueFunction<void()>&& InTaskFunc)
				: TaskFunc(MoveTemp(InTaskFunc))
			{
			}

			void DoTask(ENamedThreads::Type, const FGraphEventRef& EventRef)
			{
				TaskFunc();

				const FName ModuleName = TEXT("MetasoundGenerator");
				if (FModuleManager::Get().IsModuleLoaded(ModuleName))
				{
					FMetasoundGeneratorModule& Module = FModuleManager::GetModuleChecked<FMetasoundGeneratorModule>(ModuleName);
					TSharedPtr<FOperatorPool> Pool = Module.GetOperatorPool();

					if (Pool.IsValid() && !Pool->IsStopping())
					{
						Pool->RemoveBuildEvent(EventRef);
					}
				}
			}

			ENamedThreads::Type GetDesiredThread()
			{
				return ENamedThreads::AnyThread;
			}
		};

		FGraphEventRef EventRef = TGraphTask<FBuildAndAddOpTask>::CreateTask()
			.ConstructAndDispatchWhenReady(MoveTemp(InBuildFunc));

		{
			FScopeLock Lock(&CriticalSection);
			ActiveBuildEvents.Add(MoveTemp(EventRef));
		}
	}

	void FOperatorPool::CancelAllBuildEvents()
	{
		if (!ActiveBuildEvents.IsEmpty())
		{
			UE_LOG(LogMetasoundGenerator, Display, TEXT("Cancelling active MetaSound Cache Pool Operator build requests..."));

			bStopping.store(true);
			for (FGraphEventRef& EventRef : ActiveBuildEvents)
			{
				if (EventRef.IsValid())
				{
					EventRef->Wait();
				}
			}
			ActiveBuildEvents.Reset();
			bStopping.store(false);
		}
	}

	void FOperatorPool::RemoveBuildEvent(const FGraphEventRef& InEventRef)
	{
		FScopeLock Lock(&CriticalSection);
		ActiveBuildEvents.Remove(InEventRef);
	}

	void FOperatorPool::BuildAndAddOperator(TUniquePtr<FOperatorBuildData> InBuildData)
	{
		using namespace OperatorPoolPrivate;

		if (bStopping.load())
		{
			return;
		}

		if (!ensure(InBuildData))
		{
			return;
		}

		TSharedPtr<const Metasound::FGraph> Graph;
		if (bMetasoundPoolSyncGraphRetrieval)
		{
			// get the metasound graph and add to init params (might wait for async registration to complete)
			Graph = FMetasoundFrontendRegistryContainer::Get()->GetGraph(InBuildData->RegistryKey);
			if (!Graph.IsValid())
			{
				UE_LOG(LogMetasoundGenerator, Error, TEXT("Failed to retrieve graph '%s' synchronously when attempting to BuildAndAddOperator to pool"), *InBuildData->RegistryKey.ToString());
				return;
			}
		}

		// Build operations should never keep the operator pool alive as this can delay app shutdown arbitrarily.
		TWeakPtr<FOperatorPool> WeakOpPool = AsShared();
		BuildAndAddAsync([Graph, PreCacheData = MoveTemp(InBuildData), WeakOpPool]()
		{
			using namespace OperatorPoolPrivate;

			METASOUND_LLM_SCOPE;
			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::FOperatorPool::AsyncOperatorPrecache)

			if (!ensure(PreCacheData))
			{
				return;
			}

			if (Graph.IsValid())
			{
				PreCacheData->InitParams.Graph = Graph;
			}
			else
			{
				if (bMetasoundPoolSyncGraphRetrieval)
				{
					return;
				}
				else
				{
					// get the metasound graph and add to init params (might wait for async registration to complete)
					PreCacheData->InitParams.Graph = FMetasoundFrontendRegistryContainer::Get()->GetGraph(PreCacheData->RegistryKey);
				}
			}

			if (!PreCacheData->InitParams.Graph)
			{
				UE_LOG(LogMetasoundGenerator, Error, TEXT("Failed to retrieve graph '%s' async when attempting to BuildAndAddOperator to pool"), *PreCacheData->RegistryKey.ToString());
				return;
			}

			const int32 NumInstances = PreCacheData->NumInstances;
			for (int32 i = 0; i < NumInstances; ++i)
			{
				TSharedPtr<FOperatorPool> OperatorPool = WeakOpPool.Pin();
				if (!OperatorPool.IsValid() || OperatorPool->IsStopping())
				{
					break;
				}

				FBuildResults BuildResults;
				FOperatorAndInputs OperatorAndInputs = GeneratorBuilder::BuildGraphOperator(PreCacheData->InitParams.OperatorSettings, PreCacheData->InitParams, BuildResults);
				GeneratorBuilder::LogBuildErrors(PreCacheData->InitParams.MetaSoundName, BuildResults);

				const FGuid& GraphID = PreCacheData->InitParams.Graph->GetInstanceID();
				OperatorPool->AddOperator(GraphID, MoveTemp(OperatorAndInputs));
				OperatorPool->AddAssetIdToGraphIdLookUp(PreCacheData->AssetClassID, GraphID);
			}
		});
	}

	void FOperatorPool::TouchOperators(const FGuid& InOpeoratorID, const int32& NumToTouch)
	{
		const int32 NumToMove = FMath::Min(NumToTouch, GetNumCachedOperatorsWithID(InOpeoratorID));
		if (!NumToMove)
		{
			return;
		}

		FScopeLock Lock(&CriticalSection);

		// add to the "top" (end)
		for (int32 i = 0; i < NumToMove; ++i)
		{
			Stack.Add(InOpeoratorID);
		}

		// remove from the "bottom" (begining)
		for (int32 i = 0; i < NumToMove; ++i)
		{
			Stack.RemoveSingle(InOpeoratorID);
		}

	}

	void FOperatorPool::TouchOperatorsViaAssetClassID(const FGuid& InAssetClassID, const int32& NumToTouch)
	{
		FScopeLock Lock(&CriticalSection);
		FGuid* GraphIdPtr = AssetIdToGraphIdLookUp.Find(InAssetClassID);
		if (GraphIdPtr)
		{
			TouchOperators(*GraphIdPtr, NumToTouch);
		}
	}

	void FOperatorPool::RemoveOperatorsWithID(const FGuid& InOperatorID)
	{
		FScopeLock Lock(&CriticalSection);
		Operators.Remove(InOperatorID);
		const int32 NumRemoved = Stack.Remove(InOperatorID);

#if METASOUND_OPERATORCACHEPROFILER_ENABLED
		TRACE_COUNTER_SUBTRACT(MetaSound_OperatorPool_NumOperators, int64(NumRemoved));
#endif // #if METASOUND_OPERATORCACHEPROFILER_ENABLED
	}

	void FOperatorPool::RemoveOperatorsWithAssetClassID(const FGuid& InAssetClassID)
	{
		FScopeLock Lock(&CriticalSection);
		FGuid* GraphIdPtr = AssetIdToGraphIdLookUp.Find(InAssetClassID);
		if (GraphIdPtr)
		{
			RemoveOperatorsWithID(*GraphIdPtr);
			AssetIdToGraphIdLookUp.Remove(InAssetClassID);
		}
	}
		
	void FOperatorPool::SetMaxNumOperators(uint32 InMaxNumOperators)
	{
		FScopeLock Lock(&CriticalSection);
		Settings.MaxNumOperators = InMaxNumOperators;
		Trim();
	}

	int32 FOperatorPool::GetNumCachedOperatorsWithID(const FGuid& InOperatorID) const
	{
		FScopeLock Lock(&CriticalSection);
		if (TArray<FOperatorAndInputs> const* OperatorsWithID = Operators.Find(InOperatorID))
		{
			return OperatorsWithID->Num();
		}

		return 0;
	}

	int32 FOperatorPool::GetNumCachedOperatorsWithAssetClassID(const FGuid& InAssetClassID) const
	{
		FScopeLock Lock(&CriticalSection);
		if (const FGuid* GraphIdPtr = AssetIdToGraphIdLookUp.Find(InAssetClassID))
		{
			return GetNumCachedOperatorsWithID(*GraphIdPtr);
		}

		return 0;
	}

	void FOperatorPool::AddAssetIdToGraphIdLookUp(const FGuid& InAssetClassID, const FGuid& InOperatorID)
	{
		FScopeLock Lock(&CriticalSection);
		AssetIdToGraphIdLookUp.Add(InAssetClassID, InOperatorID);
	}

#if METASOUND_OPERATORCACHEPROFILER_ENABLED
	void FOperatorPool::UpdateHitRateTracker()
	{
		HitRateTracker.Update();
	}
#endif // #if METASOUND_OPERATORCACHEPROFILER_ENABLED

	void FOperatorPool::Trim()
	{
		FScopeLock Lock(&CriticalSection);
		int32 NumToTrim = Stack.Num() - Settings.MaxNumOperators;
		if (NumToTrim > 0)
		{
			UE_LOG(LogMetasoundGenerator, Verbose, TEXT("Trimming %d operators"), NumToTrim);
			for (int32 i = 0; i < NumToTrim; i++)
			{
				UE_LOG(LogMetasoundGenerator, VeryVerbose, TEXT("Trimming operator with ID %s"), *LexToString(Stack[i]));
				// Destructor called by TUniquePtr going out of scope.
				TArray<FOperatorAndInputs>* OperatorArray = Operators.Find(Stack[i]);
				if (ensure(OperatorArray))
				{
					if (ensure(OperatorArray->Num() > 0))
					{
						OperatorArray->Pop();
						if (OperatorArray->Num() == 0)
						{
							Operators.Remove(Stack[i]);
						}
					}
				}
			}
			Stack.RemoveAt(0, NumToTrim);
#if METASOUND_OPERATORCACHEPROFILER_ENABLED
			TRACE_COUNTER_DECREMENT(MetaSound_OperatorPool_NumOperators);
#endif // #if METASOUND_OPERATORCACHEPROFILER_ENABLED
		}

		// todo: prune AssetIdToGraphIdLookUp?
	}
} // namespace Metasound