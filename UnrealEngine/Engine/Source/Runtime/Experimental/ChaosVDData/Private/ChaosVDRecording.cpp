// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDRecording.h"
#include "Chaos/ImplicitObject.h"

int32 FChaosVDRecording::GetAvailableSolverFramesNumber(const int32 SolverID) const
{
	if (const TArray<FChaosVDSolverFrameData>* SolverData = RecordedFramesDataPerSolver.Find(SolverID))
	{
		return SolverData->Num();
	}

	return INDEX_NONE;
}

FString FChaosVDRecording::GetSolverName(int32 SolverID)
{
	static FString DefaultName(TEXT("Invalid"));

	// Currently we don't create an entry per solver, so we need to get the name from the frame data
	// TODO: Record Solver specific data per instance and not per frame
	if (TArray<FChaosVDSolverFrameData>* SolverFramesData = RecordedFramesDataPerSolver.Find(SolverID))
	{
		if(!SolverFramesData->IsEmpty())
		{
			return (*SolverFramesData)[0].DebugName;
		}
	}

	return DefaultName;
}

FChaosVDSolverFrameData* FChaosVDRecording::GetSolverFrameData(const int32 SolverID, const int32 FrameNumber)
{
	if (TArray<FChaosVDSolverFrameData>* SolverFrames = RecordedFramesDataPerSolver.Find(SolverID))
	{
		//TODO: Find a safer way of do this. If someone stores this ptr bad things will happen
		return SolverFrames->IsValidIndex(FrameNumber) ? &(*SolverFrames)[FrameNumber] : nullptr;
	}

	return nullptr;
}

FChaosVDSolverFrameData* FChaosVDRecording::GetSolverFrameDataAtCycle(int32 SolverID, uint64 Cycle)
{
	if (TArray<FChaosVDSolverFrameData>* SolverFramesPtr = RecordedFramesDataPerSolver.Find(SolverID))
	{
		TArray<FChaosVDSolverFrameData>& SolverFrames = *SolverFramesPtr;
		int32 FrameIndex = Algo::BinarySearchBy(SolverFrames, Cycle, &FChaosVDSolverFrameData::FrameCycle);
		return SolverFrames.IsValidIndex(FrameIndex) ? &SolverFrames[FrameIndex] : nullptr;
	}

	return nullptr;
}

int32 FChaosVDRecording::GetLowestSolverFrameNumberAtCycle(int32 SolverID, uint64 Cycle)
{
	if (TArray<FChaosVDSolverFrameData>* SolverFramesPtr = RecordedFramesDataPerSolver.Find(SolverID))
	{
		TArray<FChaosVDSolverFrameData>& SolverFrames = *SolverFramesPtr;
		return Algo::LowerBoundBy(SolverFrames, Cycle, &FChaosVDSolverFrameData::FrameCycle);
	}

	return INDEX_NONE;
}

int32 FChaosVDRecording::FindFirstSolverKeyFrameNumberFromFrame(int32 SolverID, int32 StartFrameNumber)
{
	if (TArray<int32>* KeyFrameNumbersPtr = RecordedKeyFramesNumberPerSolver.Find(SolverID))
	{
		TArray<int32>& KeyFrameNumbers = *KeyFrameNumbersPtr;

		const int32 IndexFound = Algo::LowerBound(KeyFrameNumbers, StartFrameNumber);
		
		// If StartFrameNumber is larger than the last keyframe recorded
		// IndexFound will be outside of the array's bounds. In that case we want to use the last key frame available
		if (IndexFound >= KeyFrameNumbers.Num())
		{
			return KeyFrameNumbers.Last();
		}

		if (KeyFrameNumbers.IsValidIndex(IndexFound))
		{
			// Frame numbers are not repeated, so the lower bound search should give us the
			// index containing the provided "StartFrameNumber" if it was already a key frame
			const int32 FoundKeyFrame = KeyFrameNumbers[IndexFound];
			if (FoundKeyFrame == StartFrameNumber)
			{
				return FoundKeyFrame;
			}

			// If StartFrameNumber was not a keyframe, we will get the lowest index number containing a key frame number larger than "StartFrameNumber"
			// in which case we want the previous one;
			const int32 PrevKeyFrameIndex = IndexFound - 1;			
			if (KeyFrameNumbers.IsValidIndex(PrevKeyFrameIndex))
			{
				return KeyFrameNumbers[PrevKeyFrameIndex];
			}
		}		
	}

	return INDEX_NONE;
}

int32 FChaosVDRecording::GetLowestSolverFrameNumberGameFrame(int32 SolverID, int32 GameFrame)
{
	if (!GameFrames.IsValidIndex(GameFrame))
	{
		return INDEX_NONE;
	}

	if (TArray<FChaosVDSolverFrameData>* SolverFramesPtr = RecordedFramesDataPerSolver.Find(SolverID))
	{
		TArray<FChaosVDSolverFrameData>& SolverFrames = *SolverFramesPtr;
		
		return Algo::LowerBoundBy(SolverFrames,GameFrames[GameFrame].FirstCycle, &FChaosVDSolverFrameData::FrameCycle);
	}

	return INDEX_NONE;
}

int32 FChaosVDRecording::GetLowestGameFrameAtSolverFrameNumber(int32 SolverID, int32 SolverFrame)
{
	if (TArray<FChaosVDSolverFrameData>* SolverFramesPtr = RecordedFramesDataPerSolver.Find(SolverID))
	{
		TArray<FChaosVDSolverFrameData>& SolverFrames = *SolverFramesPtr;

		if (SolverFrames.IsValidIndex(SolverFrame))
		{
			return Algo::LowerBoundBy(GameFrames, SolverFrames[SolverFrame].FrameCycle, &FChaosVDGameFrameData::FirstCycle);
		}	
	}

	return INDEX_NONE;
}

void FChaosVDRecording::AddKeyFrameNumberForSolver(const int32 SolverID, int32 FrameNumber)
{
	if (TArray<int32>* KeyFrameNumber = RecordedKeyFramesNumberPerSolver.Find(SolverID))
	{
		KeyFrameNumber->Add(FrameNumber);
	}
	else
	{
		RecordedKeyFramesNumberPerSolver.Add(SolverID, { FrameNumber });
	}
}

void FChaosVDRecording::AddFrameForSolver(const int32 SolverID, FChaosVDSolverFrameData&& InFrameData)
{
	int32 FrameNumber;
	const bool bIsKeyFrame = InFrameData.bIsKeyFrame;
	if (TArray<FChaosVDSolverFrameData>* SolverFrames = RecordedFramesDataPerSolver.Find(SolverID))
	{
		FrameNumber = SolverFrames->Num();

		SolverFrames->Add(MoveTemp(InFrameData));	
	}
	else
	{	
		FrameNumber = 0;
		RecordedFramesDataPerSolver.Add(SolverID, { MoveTemp(InFrameData) });
	}

	if (bIsKeyFrame)
	{
		AddKeyFrameNumberForSolver(SolverID, FrameNumber);
	}

	OnRecordingUpdated().Broadcast();
}

void FChaosVDRecording::AddGameFrameData(FChaosVDGameFrameData&& InFrameData)
{
	GameFrames.Add(MoveTemp(InFrameData));
}

FChaosVDGameFrameData* FChaosVDRecording::GetGameFrameDataAtCycle(uint64 Cycle)
{
	int32 FrameIndex = Algo::BinarySearchBy(GameFrames, Cycle, &FChaosVDGameFrameData::FirstCycle);

	if (FrameIndex != INDEX_NONE)
	{
		return &GameFrames[FrameIndex];
	}

	return nullptr;
}

FChaosVDGameFrameData* FChaosVDRecording::GetGameFrameData(int32 FrameNumber)
{
	return GameFrames.IsValidIndex(FrameNumber) ? &GameFrames[FrameNumber] : nullptr;
}

FChaosVDGameFrameData* FChaosVDRecording::GetLastGameFrameData()
{
	return GameFrames.Num() > 0 ? &GameFrames.Last() : nullptr;
}

int32 FChaosVDRecording::GetLowestGameFrameNumberAtCycle(uint64 Cycle)
{
	return Algo::LowerBoundBy(GameFrames, Cycle, &FChaosVDGameFrameData::FirstCycle);
}

void FChaosVDRecording::GetAvailableSolverIDsAtGameFrameNumber(int32 FrameNumber, TArray<int32>& OutSolversID)
{
	if (!GameFrames.IsValidIndex(FrameNumber))
	{
		return;
	}
	
	FChaosVDGameFrameData& FrameData = GameFrames[FrameNumber];

	OutSolversID.Reserve(RecordedFramesDataPerSolver.Num());

	for (const TPair<int32, TArray<FChaosVDSolverFrameData>>& SolverFramesWithIDPair : RecordedFramesDataPerSolver)
	{
		if (SolverFramesWithIDPair.Value.IsEmpty())
		{
			continue;
		}

		if (SolverFramesWithIDPair.Value.Num() == 1 && SolverFramesWithIDPair.Value[0].FrameCycle < FrameData.FirstCycle)
		{
			OutSolversID.Add(SolverFramesWithIDPair.Key);
		}
		else
		{
			if (FrameData.FirstCycle > SolverFramesWithIDPair.Value[0].FrameCycle && FrameData.FirstCycle < SolverFramesWithIDPair.Value.Last().FrameCycle)
			{
				OutSolversID.Add(SolverFramesWithIDPair.Key);
			}
		}	
	}
}

void FChaosVDRecording::AddImplicitObject(const uint32 ID, const TSharedPtr<Chaos::FImplicitObject>& InImplicitObject)
{
	if (!ImplicitObjects.Contains(ID))
	{
		AddImplicitObject_Internal(ID, InImplicitObject);
	}
}

void FChaosVDRecording::AddImplicitObject(const uint32 ID, const Chaos::FImplicitObject* InImplicitObject)
{
	if (!ImplicitObjects.Contains(ID))
	{
		// Only take ownership after we know we will add it to the map
		const TSharedPtr<const Chaos::FImplicitObject> SharedImplicit(InImplicitObject);
		AddImplicitObject_Internal(ID, SharedImplicit);
	}
}

void FChaosVDRecording::AddImplicitObject_Internal(uint32 ID, const TSharedPtr<const Chaos::FImplicitObject>& InImplicitObject)
{
	ImplicitObjects.Add(ID, InImplicitObject);
	GeometryDataLoaded.Broadcast(InImplicitObject, ID);
}
