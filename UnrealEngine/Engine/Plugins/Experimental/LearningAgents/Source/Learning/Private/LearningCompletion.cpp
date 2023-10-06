// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningCompletion.h"

namespace UE::Learning
{
	void FResetInstanceBuffer::Resize(const int32 InMaxInstanceNum)
	{
		MaxInstanceNum = InMaxInstanceNum;
		ResetInstanceNum = 0;
		ResetInstances.SetNumUninitialized({ MaxInstanceNum });
	}

	void FResetInstanceBuffer::SetResetInstances(const FIndexSet Instances)
	{
		ResetInstanceNum = 0;

		for (const int32 InstanceIdx : Instances)
		{
			ResetInstances[ResetInstanceNum] = InstanceIdx;
			ResetInstanceNum++;
		}

		ResetInstancesSet = ResetInstances.Slice(0, ResetInstanceNum);
		ResetInstancesSet.TryMakeSlice();
	}

	void FResetInstanceBuffer::SetResetInstancesFromCompletions(const TLearningArrayView<1, const ECompletionMode> Completions, const FIndexSet Instances)
	{
		ResetInstanceNum = 0;

		for (const int32 InstanceIdx : Instances)
		{
			if (Completions[InstanceIdx] != ECompletionMode::Running)
			{
				ResetInstances[ResetInstanceNum] = InstanceIdx;
				ResetInstanceNum++;
			}
		}

		ResetInstancesSet = ResetInstances.Slice(0, ResetInstanceNum);
		ResetInstancesSet.TryMakeSlice();
	}

	const int32 FResetInstanceBuffer::GetResetInstanceNum() const { return ResetInstanceNum; }

	const FIndexSet FResetInstanceBuffer::GetResetInstances() const { return ResetInstancesSet; }

	namespace Completion
	{
		const TCHAR* CompletionModeString(const ECompletionMode Completion)
		{
			switch (Completion)
			{
			case ECompletionMode::Running: return TEXT("Running");
			case ECompletionMode::Truncated: return TEXT("Truncated");
			case ECompletionMode::Terminated: return TEXT("Terminated");
			default: UE_LEARNING_NOT_IMPLEMENTED(); return TEXT("Unknown");
			}
		}

		ECompletionMode Or(const ECompletionMode Lhs, const ECompletionMode Rhs)
		{
			if ((Lhs == ECompletionMode::Running && Rhs != ECompletionMode::Running) ||
				(Lhs == ECompletionMode::Truncated && Rhs == ECompletionMode::Terminated))
			{
				return Rhs;
			}
			else
			{
				return Lhs;
			}
		}

		void EvaluateEndOfEpisodeCompletions(
			TLearningArrayView<1, ECompletionMode> InOutCompletions,
			const TLearningArrayView<1, const int32> EpisodeStepNums,
			const int32 EpisodeMaxStepNum,
			const ECompletionMode EpisodeEndCompletionMode,
			const FIndexSet Instances)
		{
			UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(Learning::Completion::EvaluateEndOfEpisodeCompletions);

			for (const int32 InstanceIdx : Instances)
			{
				if (InOutCompletions[InstanceIdx] == ECompletionMode::Running && EpisodeStepNums[InstanceIdx] == EpisodeMaxStepNum)
				{
					InOutCompletions[InstanceIdx] = EpisodeEndCompletionMode;
				}
			}
		}
	}
}

