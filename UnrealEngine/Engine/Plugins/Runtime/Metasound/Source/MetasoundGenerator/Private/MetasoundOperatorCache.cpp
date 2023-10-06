// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundOperatorCache.h"

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "HAL/CriticalSection.h"
#include "MetasoundGeneratorModule.h"
#include "Misc/Guid.h"
#include "Misc/ScopeLock.h"
#include "ProfilingDebugging/CountersTrace.h"
#include "Templates/UniquePtr.h"

#ifndef METASOUND_OPERATORCACHEPROFILER_ENABLED
#define METASOUND_OPERATORCACHEPROFILER_ENABLED COUNTERSTRACE_ENABLED
#endif

namespace Metasound
{
#if METASOUND_OPERATORCACHEPROFILER_ENABLED
	TRACE_DECLARE_FLOAT_COUNTER(MetaSound_OperatorCache_HitRatio, TEXT("MetaSound/OperatorCache/HitRatio"));
	namespace OperatorCachePrivate
	{
		static std::atomic<uint32> CacheHitCount = 0;
		static std::atomic<uint32> CacheAttemptCount = 0;

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
	}
#endif

	FOperatorCache::FOperatorCache(const FOperatorCacheSettings& InSettings)
	: Settings(InSettings)
	{
	}

	FOperatorAndInputs FOperatorCache::ClaimCachedOperator(const FGuid& InOperatorID)
	{
		FScopeLock Lock(&CriticalSection);

		FOperatorAndInputs OpAndInputs;
		if (TArray<FOperatorAndInputs>* OperatorsWithID = Operators.Find(InOperatorID))
		{
			if (OperatorsWithID->Num() > 0)
			{
				OpAndInputs = OperatorsWithID->Pop();
				Stack.RemoveAt(Stack.FindLast(InOperatorID));
#if METASOUND_OPERATORCACHEPROFILER_ENABLED
				OperatorCachePrivate::CacheHitCount++;
#endif
			}
		}

#if METASOUND_OPERATORCACHEPROFILER_ENABLED
		OperatorCachePrivate::CacheAttemptCount++;

		TRACE_COUNTER_SET(MetaSound_OperatorCache_HitRatio, OperatorCachePrivate::GetHitRatio());
#endif
		return OpAndInputs;
	}

	void FOperatorCache::AddOperatorToCache(const FGuid& InOperatorID, TUniquePtr<IOperator>&& InOperator, FInputVertexInterfaceData&& InInputData)
	{
		if (InOperator.IsValid())
		{
			FScopeLock Lock(&CriticalSection);

			Stack.Add(InOperatorID);

			FOperatorAndInputs OpAndInputs;
			OpAndInputs.Operator = MoveTemp(InOperator);
			OpAndInputs.Inputs = MoveTemp(InInputData);

			if (TArray<FOperatorAndInputs>* OperatorArray = Operators.Find(InOperatorID))
			{
				OperatorArray->Add(MoveTemp(OpAndInputs));
			}
			else
			{
				TArray<FOperatorAndInputs> NewOperatorArray;
				NewOperatorArray.Add(MoveTemp(OpAndInputs));
				Operators.Add(InOperatorID, MoveTemp(NewOperatorArray));
			}

			TrimCache();
		}
	}

	void FOperatorCache::RemoveOperatorsWithID(const FGuid& InOperatorID)
	{
		FScopeLock Lock(&CriticalSection);
		Operators.Remove(InOperatorID);
		Stack.Remove(InOperatorID);
	}

	void FOperatorCache::SetMaxNumOperators(uint32 InMaxNumOperators)
	{
		FScopeLock Lock(&CriticalSection);
		Settings.MaxNumOperators = InMaxNumOperators;
		TrimCache();
	}

	void FOperatorCache::TrimCache()
	{
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
		}
	}
}

