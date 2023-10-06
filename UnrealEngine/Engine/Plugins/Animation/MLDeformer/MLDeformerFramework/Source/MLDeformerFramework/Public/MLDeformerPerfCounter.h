// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"


namespace UE::MLDeformer
{
	class MLDEFORMERFRAMEWORK_API FMLDeformerPerfCounter
	{
	public:
		FMLDeformerPerfCounter();

		// Main methods.
		void SetHistorySize(int32 NumHistoryItems);
		void Reset();
		void BeginSample();
		void EndSample();

		// Get statistics.
		int32 GetCycles() const;
		int32 GetCyclesMin() const;
		int32 GetCyclesMax() const;
		int32 GetCyclesAverage() const;
		int32 GetNumSamples() const;
		int32 GetHistorySize() const;

	private:
		int32 StartTime = 0;
		int32 NumSamples = 0;
		int32 Cycles = 0;
		int32 CyclesMin = 0;
		int32 CyclesMax = 0;
		TArray<int32> CycleHistory;
	};
}
