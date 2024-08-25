// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "CoreMinimal.h"
#include "CoreTypes.h"
#include "UObject/ObjectMacros.h"

#include "RigVMProfilingInfo.generated.h"

USTRUCT()
struct RIGVM_API FRigVMInstructionVisitInfo
{
	GENERATED_BODY()

	FRigVMInstructionVisitInfo()
	{
	}

	void Reset()
	{
		InstructionVisitedDuringLastRun.Reset();
		InstructionVisitOrder.Reset();
		FirstEntryEventInQueue = NAME_None;
	}

	inline void ResetInstructionVisitedDuringLastRun(int32 NewSize = 0) { InstructionVisitedDuringLastRun.Reset(NewSize); }
	inline void SetNumInstructionVisitedDuringLastRunZeroed(int32 Num) { InstructionVisitedDuringLastRun.SetNumZeroed(Num); }
	inline void SetInstructionVisitedDuringLastRun(int32 InstructionIndex)
	{
		if (InstructionVisitedDuringLastRun.IsValidIndex(InstructionIndex))
		{
			InstructionVisitedDuringLastRun[InstructionIndex]++;
		}
	}
	inline int32 GetInstructionVisitedCountDuringLastRun(int32 InstructionIndex) const { return InstructionVisitedDuringLastRun.IsValidIndex(InstructionIndex) ? InstructionVisitedDuringLastRun[InstructionIndex] : 0; }
	inline const TArray<int32>& GetInstructionVisitedCountDuringLastRun() const { return InstructionVisitedDuringLastRun; }

	inline void ResetInstructionVisitOrder(int32 NewSize = 0) { InstructionVisitOrder.Reset(NewSize); }
	inline void AddInstructionIndexToVisitOrder(int32 InstructionIndex) { InstructionVisitOrder.Add(InstructionIndex); }
	inline const TArray<int32>& GetInstructionVisitOrder() const { return InstructionVisitOrder; }

	inline const void SetFirstEntryEventInEventQueue(const FName& InFirstEventName) { FirstEntryEventInQueue = InFirstEventName; }
	inline const FName& GetFirstEntryEventInEventQueue() const { return FirstEntryEventInQueue; }

	void SetupInstructionTracking(int32 InInstructionCount);

private:

	// stores the number of times each instruction was visited
	TArray<int32> InstructionVisitedDuringLastRun;
	TArray<int32> InstructionVisitOrder;

	// A RigVMHost can run multiple events per evaluation, such as the Backward&Forward Solve Mode,
	// store the first event such that we know when to reset data for a new round of rig evaluation
	FName FirstEntryEventInQueue = NAME_None;

	friend class URigVMHost;
	friend struct FFirstEntryEventGuard;
};

struct RIGVM_API FFirstEntryEventGuard
{
public:
	FFirstEntryEventGuard(FRigVMInstructionVisitInfo* InVisitInfo, const FName& InFirstEvent)
		: VisitInfo(InVisitInfo)
	{
		OldEntry = VisitInfo->FirstEntryEventInQueue;
		VisitInfo->FirstEntryEventInQueue = InFirstEvent;
	}

	~FFirstEntryEventGuard()
	{
		VisitInfo->FirstEntryEventInQueue = OldEntry;
	}

	FName OldEntry;
	FRigVMInstructionVisitInfo* VisitInfo;
};

USTRUCT()
struct RIGVM_API FRigVMProfilingInfo
{
	GENERATED_BODY()

	FRigVMProfilingInfo()
	{
	}

	void Reset()
	{
		InstructionCyclesDuringLastRun.Reset();
		StartCycles = 0;
		OverallCycles = 0;
	}

	inline uint64 GetStartCycles() const {	return StartCycles;	}
	inline void SetStartCycles(uint64 InStartCycles) { StartCycles = InStartCycles; }

	inline uint64 GetOverallCycles() const { return OverallCycles; }
	inline void SetOverallCycles(uint64 Cycles) { OverallCycles = Cycles; }
	inline void AddOverallCycles(uint64 Cycles) { OverallCycles += Cycles; }

	inline void ResetInstructionCyclesDuringLastRun(int32 NewSize = 0) { InstructionCyclesDuringLastRun.Reset(NewSize); }
	inline uint64 GetInstructionCyclesDuringLastRun(int32 InstructionIndex) const
	{
		return InstructionCyclesDuringLastRun.IsValidIndex(InstructionIndex) ? InstructionCyclesDuringLastRun[InstructionIndex] : UINT64_MAX;
	}
	inline void SetInstructionCyclesDuringLastRun(int32 InstructionIndex, uint64 CyclesDuringLastRun)
	{
		if (InstructionCyclesDuringLastRun.IsValidIndex(InstructionIndex))
		{
			InstructionCyclesDuringLastRun[InstructionIndex] = CyclesDuringLastRun;
		}
	}
	inline void AddInstructionCyclesDuringLastRun(int32 InstructionIndex, uint64 CyclesDuringLastRun)
	{
		if (InstructionCyclesDuringLastRun.IsValidIndex(InstructionIndex))
		{
			InstructionCyclesDuringLastRun[InstructionIndex] += CyclesDuringLastRun;
		}
	}
	inline void InitInstructionCyclesDuringLastRunValues(int32 NewSize, uint64 DefaultValue)
	{
		InstructionCyclesDuringLastRun.SetNumUninitialized(NewSize);
		for (uint64& Value : InstructionCyclesDuringLastRun)
		{
			Value = DefaultValue;
		}
	}

	void SetupInstructionTracking(int32 InInstructionCount, bool bEnableProfiling);

	double GetLastExecutionMicroSeconds() const { return LastExecutionMicroSeconds; }
	void SetLastExecutionMicroSeconds(double InLastExecutionMicroSeconds) { LastExecutionMicroSeconds = InLastExecutionMicroSeconds; }

	void StartProfiling(bool bEnableProfiling);
	void StopProfiling();

private:

	// stores the number of times each instruction was visited
	TArray<uint64> InstructionCyclesDuringLastRun;

	uint64 StartCycles = 0;
	uint64 OverallCycles = 0;

	double LastExecutionMicroSeconds = 0.0;
};
