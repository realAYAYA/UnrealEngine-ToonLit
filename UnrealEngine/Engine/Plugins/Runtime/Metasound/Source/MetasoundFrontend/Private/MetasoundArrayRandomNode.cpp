// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundArrayRandomNode.h"
#include "CoreMinimal.h"
#include "Internationalization/Text.h"
#include "MetasoundBuilderInterface.h"
#include "MetasoundDataFactory.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundLog.h"
#include "MetasoundNodeInterface.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundOperatorInterface.h"
#include "MetasoundPrimitives.h"
#include "MetasoundTrigger.h"
#include "Containers/CircularQueue.h"
#include "MetasoundArrayNodes.h"

#include <type_traits>


namespace Metasound
{
	FArrayRandomGet::FArrayRandomGet(int32 InSeed, int32 InMaxIndex, const TArray<float>& InWeights, int32 InNoRepeatOrder)
	{
		Init(InSeed, InMaxIndex, InWeights, InNoRepeatOrder);
	}

	void FArrayRandomGet::Init(int32 InSeed, int32 InMaxIndex, const TArray<float>& InWeights, int32 InNoRepeatOrder)
	{
		SetSeed(InSeed);
		MaxIndex = InMaxIndex;
		SetNoRepeatOrder(InNoRepeatOrder);
		check(!InNoRepeatOrder || !PreviousIndicesQueue->IsFull());
		RandomWeights = InWeights;
	}

	void FArrayRandomGet::SetSeed(int32 InSeed)
	{
		if (InSeed == INDEX_NONE)
		{
			RandomStream.Initialize(FPlatformTime::Cycles());
		}
		else
		{
			RandomStream.Initialize(InSeed);
		}

		ResetSeed();
	}

	void FArrayRandomGet::SetNoRepeatOrder(int32 InNoRepeatOrder)
	{
		// Make sure the no repeat order is between 0 and one less than our max index
		InNoRepeatOrder = FMath::Clamp(InNoRepeatOrder, 0, MaxIndex - 1);

		if (InNoRepeatOrder != NoRepeatOrder)
		{
			PreviousIndicesQueue = MakeUnique<TCircularQueue<int32>>(InNoRepeatOrder + 2);
			PreviousIndices.Reset();
			NoRepeatOrder = InNoRepeatOrder;
		}
	}

	void FArrayRandomGet::SetRandomWeights(const TArray<float>& InRandomWeights)
	{
		RandomWeights = InRandomWeights;
	}

	void FArrayRandomGet::ResetSeed()
	{
		RandomStream.Reset();
	}

	float FArrayRandomGet::ComputeTotalWeight()
	{
		float TotalWeight = 0.0f;
		if (RandomWeights.Num() > 0)
		{
			for (int32 i = 0; i < MaxIndex; ++i)
			{
				// If the index exists in previous indices, continue
				if (PreviousIndices.Contains(i))
				{
					continue;
				}

				// We modulus on the weight array to determine weights for the input array
				// I.e. if weights is 2 elements, the weights will alternate in application to the input array
				TotalWeight += RandomWeights[i % RandomWeights.Num()];
			}
		}
		return TotalWeight;
	}

	// Returns the next random weighted value in the array indices. 
	int32 FArrayRandomGet::NextValue()
	{
		// First compute the total size of the weights
		bool bHasWeights = RandomWeights.Num() > 0;
		float TotalWeight = 0.0f;
		if (bHasWeights)
		{
			TotalWeight = ComputeTotalWeight();
			if (TotalWeight == 0.0f && PreviousIndices.Num() > 0)
			{
				PreviousIndices.Reset();
				PreviousIndicesQueue->Empty();
				TotalWeight = ComputeTotalWeight();
			}

			// Weights might have been set with all 0.0s. If that's the case, we treat as if there were no weights set.
			bHasWeights = (TotalWeight > 0.0f);
		}

		if (!bHasWeights)
		{
			TotalWeight = (float)(FMath::Max(MaxIndex - PreviousIndices.Num(), 1));
		}
		check(TotalWeight > 0.0f);
	

		// Make a random choice based on the total weight
		float Choice = RandomStream.FRandRange(0.0f, TotalWeight);
		// Now find the index this choice matches up to
		TotalWeight = 0.0f;
		int32 ChosenIndex = INDEX_NONE;
		for (int32 i = 0; i < MaxIndex; ++i)
		{
			if (PreviousIndices.Contains(i))
			{
				continue;
			}

			float NextTotalWeight = TotalWeight;
			if (bHasWeights)
			{
				check(RandomWeights.Num() > 0);
				NextTotalWeight += RandomWeights[i % RandomWeights.Num()];
			}
			else
			{
				NextTotalWeight += 1.0f;
			}

			if (Choice >= TotalWeight && Choice < NextTotalWeight)
			{
				ChosenIndex = i;
				break;
			}
			TotalWeight = NextTotalWeight;
		}
		check(ChosenIndex != INDEX_NONE);

		// Dequeue and remove the oldest previous index
		if (NoRepeatOrder > 0)
		{
			if (PreviousIndices.Num() == NoRepeatOrder)
			{
				check(PreviousIndicesQueue->Count() == PreviousIndices.Num());
				int32 OldPrevIndex;
				PreviousIndicesQueue->Dequeue(OldPrevIndex);
				PreviousIndices.Remove(OldPrevIndex);
				check(PreviousIndices.Num() == NoRepeatOrder - 1);
			}

			check(PreviousIndices.Num() < NoRepeatOrder);
			check(!PreviousIndicesQueue->IsFull());

			bool bSuccess = PreviousIndicesQueue->Enqueue(ChosenIndex);
			check(bSuccess);
			check(!PreviousIndices.Contains(ChosenIndex));
			PreviousIndices.Add(ChosenIndex);
			check(PreviousIndicesQueue->Count() == PreviousIndices.Num());
		}

		return ChosenIndex;
	}

	FSharedStateRandomGetManager& FSharedStateRandomGetManager::Get()
	{
		static FSharedStateRandomGetManager RGM;
		return RGM;
	}

	inline bool ShouldStompSharedState(const FArrayRandomGet& InRandomGet, const InitSharedStateArgs InArgs)
	{
		return
			InArgs.bIsPreviewSound										// If it's a preview sound
			|| InRandomGet.GetMaxIndex() != InArgs.NumElements			// Size of Array has changed
			|| InRandomGet.GetNoRepeatOrder() != InArgs.NoRepeatOrder;	// Repeat order has changed
	}

	void FSharedStateRandomGetManager::InitSharedState(InitSharedStateArgs& InArgs)
	{
		FScopeLock Lock(&CritSect);

		// Look if there's already shared state. In certain cases we should stomp it by re-adding...
		if (const TUniquePtr<FArrayRandomGet>* FoundExisting = RandomGets.Find(InArgs.SharedStateId))
		{
			if (FoundExisting && FoundExisting->IsValid())
			{
				const FArrayRandomGet& Existing = *FoundExisting->Get();
				if (!ShouldStompSharedState(Existing, InArgs))
				{
					return;
				}
			}
		}

		// Add state/Stomp existing.
		RandomGets.Add(InArgs.SharedStateId, MakeUnique<FArrayRandomGet>(InArgs.Seed, InArgs.NumElements, InArgs.Weights, InArgs.NoRepeatOrder));
	}

	int32 FSharedStateRandomGetManager::NextValue(const FGuid& InSharedStateId)
	{
		FScopeLock Lock(&CritSect);
		TUniquePtr<FArrayRandomGet>* RG = RandomGets.Find(InSharedStateId);
		return (*RG)->NextValue();
	}

	void FSharedStateRandomGetManager::SetSeed(const FGuid& InSharedStateId, int32 InSeed)
	{
		FScopeLock Lock(&CritSect);
		TUniquePtr<FArrayRandomGet>* RG = RandomGets.Find(InSharedStateId);
		(*RG)->SetSeed(InSeed);
	}

	void FSharedStateRandomGetManager::SetNoRepeatOrder(const FGuid& InSharedStateId, int32 InNoRepeatOrder)
	{
		FScopeLock Lock(&CritSect);
		TUniquePtr<FArrayRandomGet>* RG = RandomGets.Find(InSharedStateId);
		(*RG)->SetNoRepeatOrder(InNoRepeatOrder);
	}

	void FSharedStateRandomGetManager::SetRandomWeights(const FGuid& InSharedStateId, const TArray<float>& InRandomWeights)
	{
		FScopeLock Lock(&CritSect);
		TUniquePtr<FArrayRandomGet>* RG = RandomGets.Find(InSharedStateId);
		(*RG)->SetRandomWeights(InRandomWeights);
	}

	void FSharedStateRandomGetManager::ResetSeed(const FGuid& InSharedStateId)
	{
		FScopeLock Lock(&CritSect);
		TUniquePtr<FArrayRandomGet>* RG = RandomGets.Find(InSharedStateId);
		(*RG)->ResetSeed();
	}

}
