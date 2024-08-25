// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningExperience.h"

#include "LearningCompletion.h"
#include "LearningLog.h"
#include "Templates/Function.h"

namespace UE::Learning
{
	void FEpisodeBuffer::Resize(
		const int32 InMaxInstanceNum,
		const int32 InMaxStepNum,
		const int32 InObservationVectorDimNum,
		const int32 InActionVectorDimNum,
		const int32 InMemoryStateVectorDimNum)
	{
		MaxInstanceNum = InMaxInstanceNum;
		MaxStepNum = InMaxStepNum;
		EpisodeStepNums.SetNumUninitialized({ InMaxInstanceNum });
		Observations.SetNumUninitialized({ InMaxInstanceNum, InMaxStepNum, InObservationVectorDimNum });
		Actions.SetNumUninitialized({ InMaxInstanceNum, InMaxStepNum, InActionVectorDimNum });
		MemoryStates.SetNumUninitialized({ InMaxInstanceNum, InMaxStepNum, InMemoryStateVectorDimNum });
		Rewards.SetNumUninitialized({ InMaxInstanceNum, InMaxStepNum });

		Array::Zero(EpisodeStepNums);
	}

	void FEpisodeBuffer::Push(
		const TLearningArrayView<2, const float> InObservations,
		const TLearningArrayView<2, const float> InActions,
		const TLearningArrayView<2, const float> InMemoryStates,
		const TLearningArrayView<1, const float> InRewards,
		const FIndexSet Instances)
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(Learning::FEpisodeBuffer::Push);

		for (const int32 InstanceIdx : Instances)
		{
			UE_LEARNING_CHECKF(EpisodeStepNums[InstanceIdx] < MaxStepNum, TEXT("Episode Buffer full!"));

			Array::Copy(Observations[InstanceIdx][EpisodeStepNums[InstanceIdx]], InObservations[InstanceIdx]);
			Array::Copy(Actions[InstanceIdx][EpisodeStepNums[InstanceIdx]], InActions[InstanceIdx]);
			Array::Copy(MemoryStates[InstanceIdx][EpisodeStepNums[InstanceIdx]], InMemoryStates[InstanceIdx]);
			Rewards[InstanceIdx][EpisodeStepNums[InstanceIdx]] = InRewards[InstanceIdx];

			EpisodeStepNums[InstanceIdx]++;
		}
	}

	void FEpisodeBuffer::Reset(const FIndexSet Instances)
	{
		Array::Zero(EpisodeStepNums, Instances);
	}

	const int32 FEpisodeBuffer::GetMaxInstanceNum() const
	{
		return MaxInstanceNum;
	}

	const int32 FEpisodeBuffer::GetMaxStepNum() const
	{
		return MaxStepNum;
	}

	const TLearningArrayView<1, const int32> FEpisodeBuffer::GetEpisodeStepNums() const
	{
		return EpisodeStepNums;
	}

	const TLearningArrayView<2, const float> FEpisodeBuffer::GetObservations(const int32 InstanceIdx) const
	{
		return Observations[InstanceIdx].Slice(0, EpisodeStepNums[InstanceIdx]);
	}

	const TLearningArrayView<2, const float> FEpisodeBuffer::GetActions(const int32 InstanceIdx) const
	{
		return Actions[InstanceIdx].Slice(0, EpisodeStepNums[InstanceIdx]);
	}

	const TLearningArrayView<2, const float> FEpisodeBuffer::GetMemoryStates(const int32 InstanceIdx) const
	{
		return MemoryStates[InstanceIdx].Slice(0, EpisodeStepNums[InstanceIdx]);
	}

	const TLearningArrayView<1, const float> FEpisodeBuffer::GetRewards(const int32 InstanceIdx) const
	{
		return Rewards[InstanceIdx].Slice(0, EpisodeStepNums[InstanceIdx]);
	}

	void FReplayBuffer::Resize(
		const int32 InObservationVectorDimensionNum,
		const int32 InActionVectorDimensionNum,
		const int32 InMemoryStateVectorDimensionNum,
		const int32 InMaxEpisodeNum,
		const int32 InMaxStepNum)
	{
		MaxEpisodeNum = InMaxEpisodeNum;
		MaxStepNum = InMaxStepNum;
		EpisodeNum = 0;
		StepNum = 0;
		EpisodeStarts.SetNumUninitialized({ InMaxEpisodeNum });
		EpisodeLengths.SetNumUninitialized({ InMaxEpisodeNum });
		EpisodeCompletionModes.SetNumUninitialized({ InMaxEpisodeNum });
		EpisodeFinalObservations.SetNumUninitialized({ InMaxEpisodeNum, InObservationVectorDimensionNum });
		EpisodeFinalMemoryStates.SetNumUninitialized({ InMaxEpisodeNum, InMemoryStateVectorDimensionNum });
		Observations.SetNumUninitialized({ InMaxStepNum, InObservationVectorDimensionNum });
		Actions.SetNumUninitialized({ InMaxStepNum, InActionVectorDimensionNum });
		MemoryStates.SetNumUninitialized({ InMaxStepNum, InMemoryStateVectorDimensionNum });
		Rewards.SetNumUninitialized({ InMaxStepNum });
	}

	void FReplayBuffer::Reset()
	{
		EpisodeNum = 0;
		StepNum = 0;
	}

	bool FReplayBuffer::AddEpisodes(
		const TLearningArrayView<1, const ECompletionMode> InEpisodeCompletionModes,
		const TLearningArrayView<2, const float> InEpisodeFinalObservations,
		const TLearningArrayView<2, const float> InEpisodeFinalMemoryStates,
		const FEpisodeBuffer& EpisodeBuffer,
		const FIndexSet Instances,
		const bool bAddTruncatedEpisodeWhenFull)
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(Learning::FReplayBuffer::AddEpisodes);

		for (const int32 InstanceIdx : Instances)
		{
			UE_LEARNING_CHECKF(InEpisodeCompletionModes[InstanceIdx] != ECompletionMode::Running,
				TEXT("Tried to add experience from episode that is still running..."));

			const int32 EpisodeStepNum = EpisodeBuffer.GetEpisodeStepNums()[InstanceIdx];

			// Is there space for the full episode in the buffer?

			if (EpisodeNum < MaxEpisodeNum && StepNum + EpisodeStepNum <= MaxStepNum)
			{
				// Copy the data into the replay buffer
				Array::Copy(Observations.Slice(StepNum, EpisodeStepNum), EpisodeBuffer.GetObservations(InstanceIdx));
				Array::Copy(Actions.Slice(StepNum, EpisodeStepNum), EpisodeBuffer.GetActions(InstanceIdx));
				Array::Copy(MemoryStates.Slice(StepNum, EpisodeStepNum), EpisodeBuffer.GetMemoryStates(InstanceIdx));
				Array::Copy(Rewards.Slice(StepNum, EpisodeStepNum), EpisodeBuffer.GetRewards(InstanceIdx));

				// Write the Episode start, length, completion, and final observation
				EpisodeStarts[EpisodeNum] = StepNum;
				EpisodeLengths[EpisodeNum] = EpisodeStepNum;
				EpisodeCompletionModes[EpisodeNum] = InEpisodeCompletionModes[InstanceIdx];
				Array::Copy(EpisodeFinalObservations[EpisodeNum], InEpisodeFinalObservations[InstanceIdx]);
				Array::Copy(EpisodeFinalMemoryStates[EpisodeNum], InEpisodeFinalMemoryStates[InstanceIdx]);

				// Increment the Counts
				EpisodeNum++;
				StepNum += EpisodeStepNum;

				// Continue onto next Episode
				continue;
			}

			// Is there space for a partial episode in the buffer?

			if (bAddTruncatedEpisodeWhenFull && EpisodeNum < MaxEpisodeNum && StepNum < MaxStepNum)
			{
				const int32 PartialStepNum = MaxStepNum - StepNum;
				UE_LEARNING_CHECK(PartialStepNum > 0 && PartialStepNum < EpisodeStepNum);

				// Copy the data into the replay buffer
				Array::Copy(Observations.Slice(StepNum, PartialStepNum), EpisodeBuffer.GetObservations(InstanceIdx).Slice(0, PartialStepNum));
				Array::Copy(Actions.Slice(StepNum, PartialStepNum), EpisodeBuffer.GetActions(InstanceIdx).Slice(0, PartialStepNum));
				Array::Copy(MemoryStates.Slice(StepNum, PartialStepNum), EpisodeBuffer.GetMemoryStates(InstanceIdx).Slice(0, PartialStepNum));
				Array::Copy(Rewards.Slice(StepNum, PartialStepNum), EpisodeBuffer.GetRewards(InstanceIdx).Slice(0, PartialStepNum));

				// Write the Episode start, length, completion, and final observation
				EpisodeStarts[EpisodeNum] = StepNum;
				EpisodeLengths[EpisodeNum] = PartialStepNum;
				EpisodeCompletionModes[EpisodeNum] = ECompletionMode::Truncated;
				Array::Copy(EpisodeFinalObservations[EpisodeNum], InEpisodeFinalObservations[InstanceIdx]);
				Array::Copy(EpisodeFinalMemoryStates[EpisodeNum], InEpisodeFinalMemoryStates[InstanceIdx]);

				// Increment the Counts
				EpisodeNum++;
				StepNum += PartialStepNum;
			}

			// Otherwise buffer is full
			return true;
		}

		return (EpisodeNum == MaxEpisodeNum) || (StepNum == MaxStepNum);
	}

	const int32 FReplayBuffer::GetMaxEpisodeNum() const
	{
		return MaxEpisodeNum;
	}

	const int32 FReplayBuffer::GetMaxStepNum() const
	{
		return MaxStepNum;
	}

	const int32 FReplayBuffer::GetEpisodeNum() const
	{
		return EpisodeNum;
	}

	const int32 FReplayBuffer::GetStepNum() const
	{
		return StepNum;
	}

	const TLearningArrayView<1, const int32> FReplayBuffer::GetEpisodeStarts() const
	{
		return EpisodeStarts.Slice(0, EpisodeNum);
	}

	const TLearningArrayView<1, const int32> FReplayBuffer::GetEpisodeLengths() const
	{
		return EpisodeLengths.Slice(0, EpisodeNum);
	}

	const TLearningArrayView<1, const ECompletionMode> FReplayBuffer::GetEpisodeCompletionModes() const
	{
		return EpisodeCompletionModes.Slice(0, EpisodeNum);
	}

	const TLearningArrayView<2, const float> FReplayBuffer::GetEpisodeFinalObservations() const
	{
		return EpisodeFinalObservations.Slice(0, EpisodeNum);
	}

	const TLearningArrayView<2, const float> FReplayBuffer::GetEpisodeFinalMemoryStates() const
	{
		return EpisodeFinalMemoryStates.Slice(0, EpisodeNum);
	}

	const TLearningArrayView<2, const float> FReplayBuffer::GetObservations() const
	{
		return Observations.Slice(0, StepNum);
	}

	const TLearningArrayView<2, const float> FReplayBuffer::GetActions() const
	{
		return Actions.Slice(0, StepNum);
	}

	const TLearningArrayView<2, const float> FReplayBuffer::GetMemoryStates() const
	{
		return MemoryStates.Slice(0, StepNum);
	}

	const TLearningArrayView<1, const float> FReplayBuffer::GetRewards() const
	{
		return Rewards.Slice(0, StepNum);
	}

	namespace Experience
	{
		void GatherExperienceUntilReplayBufferFull(
			FReplayBuffer& ReplayBuffer,
			FEpisodeBuffer& EpisodeBuffer,
			FResetInstanceBuffer& ResetBuffer,
			TLearningArrayView<2, float> ObservationVectorBuffer,
			TLearningArrayView<2, float> ActionVectorBuffer,
			TLearningArrayView<2, float> PreEvaluationMemoryStateVectorBuffer,
			TLearningArrayView<2, float> MemoryStateVectorBuffer,
			TLearningArrayView<1, float> RewardBuffer,
			TLearningArrayView<1, ECompletionMode> CompletionBuffer,
			TLearningArrayView<1, ECompletionMode> EpisodeCompletionBuffer,
			TLearningArrayView<1, ECompletionMode> AllCompletionBuffer,
			const TFunctionRef<void(const FIndexSet Instances)> ResetFunction,
			const TFunctionRef<void(const FIndexSet Instances)> ObservationFunction,
			const TFunctionRef<void(const FIndexSet Instances)> PolicyFunction,
			const TFunctionRef<void(const FIndexSet Instances)> ActionFunction,
			const TFunctionRef<void(const FIndexSet Instances)> UpdateFunction,
			const TFunctionRef<void(const FIndexSet Instances)> RewardFunction,
			const TFunctionRef<void(const FIndexSet Instances)> CompletionFunction,
			const FIndexSet Instances)
		{
			UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(Learning::Experience::GatherExperienceUntilReplayBufferFull);

			// Reset Everything

			ReplayBuffer.Reset();
			EpisodeBuffer.Reset(Instances);
			ResetFunction(Instances);

			while (true)
			{
				// Encode Observations

				ObservationFunction(Instances);

				// Evaluate Policy

				PolicyFunction(Instances);

				// Decode Actions

				ActionFunction(Instances);

				// Update Environment

				UpdateFunction(Instances);

				// Compute Rewards

				RewardFunction(Instances);

				// Push to Experience Buffer

				EpisodeBuffer.Push(
					ObservationVectorBuffer,
					ActionVectorBuffer,
					PreEvaluationMemoryStateVectorBuffer,
					RewardBuffer,
					Instances);

				// Evaluate Completions

				CompletionFunction(Instances);

				Completion::EvaluateEndOfEpisodeCompletions(
					EpisodeCompletionBuffer,
					EpisodeBuffer.GetEpisodeStepNums(),
					EpisodeBuffer.GetMaxStepNum(),
					Instances);

				for (const int32 Instance : Instances)
				{
					AllCompletionBuffer[Instance] = Completion::Or(CompletionBuffer[Instance], EpisodeCompletionBuffer[Instance]);
				}

				ResetBuffer.SetResetInstancesFromCompletions(AllCompletionBuffer, Instances);

				if (ResetBuffer.GetResetInstanceNum() == 0)
				{
					continue;
				}

				// Evaluate Observations again for instances that are completed

				ObservationFunction(ResetBuffer.GetResetInstances());

				// Push completed instances to Replay Buffer and return if full

				if (ReplayBuffer.AddEpisodes(
					AllCompletionBuffer,
					ObservationVectorBuffer,
					MemoryStateVectorBuffer,
					EpisodeBuffer,
					ResetBuffer.GetResetInstances()))
				{
					return;
				}

				// Just reset Episode Buffer for instances who reached the maximum episode length
				ResetBuffer.SetResetInstancesFromCompletions(EpisodeCompletionBuffer, Instances);
				EpisodeBuffer.Reset(ResetBuffer.GetResetInstances());

				// Call Reset Function for instances which signaled a completion
				ResetBuffer.SetResetInstancesFromCompletions(CompletionBuffer, Instances);
				ResetFunction(ResetBuffer.GetResetInstances());
			}
		}

	}
}
