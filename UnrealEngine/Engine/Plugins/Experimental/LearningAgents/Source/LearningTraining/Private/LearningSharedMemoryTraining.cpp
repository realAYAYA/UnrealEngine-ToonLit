// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningSharedMemoryTraining.h"

#include "LearningProgress.h"
#include "LearningNeuralNetwork.h"
#include "LearningExperience.h"
#include "LearningCompletion.h"

#include "HAL/PlatformProcess.h"

namespace UE::Learning::SharedMemoryTraining
{
	enum class EControls : uint8
	{
		ExperienceEpisodeNum = 0,
		ExperienceStepNum = 1,
		ExperienceSignal = 2,
		PolicySignal = 3,
		CriticSignal = 4,
		CompleteSignal = 5,
		StopSignal = 6,

		ControlNum = 8,
	};

	uint8 GetControlNum()
	{
		return (uint8)EControls::ControlNum;
	}

	ETrainerResponse SendStop(TLearningArrayView<1, volatile int32> Controls)
	{
		Controls[(uint8)EControls::StopSignal] = true;
		return ETrainerResponse::Success;
	}

	bool HasPolicyOrCompleted(TLearningArrayView<1, volatile int32> Controls)
	{
		return Controls[(uint8)EControls::PolicySignal] || Controls[(uint8)EControls::CompleteSignal];
	}

	ETrainerResponse RecvPolicy(
		TLearningArrayView<1, volatile int32> Controls,
		FNeuralNetwork& OutNetwork,
		const TLearningArrayView<1, const uint8> Policy,
		const float Timeout,
		FRWLock* NetworkLock,
		const ELogSetting LogSettings)
	{
		const float SleepTime = 0.001f;
		float WaitTime = 0.0f;

		// Wait until the policy is done being written by the sub-process
		while (!Controls[(uint8)EControls::PolicySignal])
		{
			// Check if Completed Signal has been raised
			if (Controls[(uint8)EControls::CompleteSignal])
			{
				// If so set low to confirm we have read it
				Controls[(uint8)EControls::CompleteSignal] = false;
				return ETrainerResponse::Completed;
			}

			FPlatformProcess::Sleep(SleepTime);
			WaitTime += SleepTime;

			if (WaitTime > Timeout)
			{
				return ETrainerResponse::Timeout;
			}
		}

		if (LogSettings != ELogSetting::Silent)
		{
			UE_LOG(LogLearning, Display, TEXT("Pulling Policy..."));
		}

		// Read the policy
		{
			FScopeNullableWriteLock ScopeLock(NetworkLock);
			int32 Offset = 0;
			OutNetwork.DeserializeFromBytes(Offset, Policy);
		}

		// Confirm we have read the policy
		Controls[(uint8)EControls::PolicySignal] = false;

		return ETrainerResponse::Success;
	}

	ETrainerResponse RecvCritic(
		TLearningArrayView<1, volatile int32> Controls,
		FNeuralNetwork& OutNetwork,
		const TLearningArrayView<1, const uint8> Critic,
		const float Timeout,
		FRWLock* NetworkLock,
		const ELogSetting LogSettings)
	{
		const float SleepTime = 0.001f;
		float WaitTime = 0.0f;

		// Wait until the critic is done being written by the sub-process
		while (!Controls[(uint8)EControls::CriticSignal])
		{
			FPlatformProcess::Sleep(SleepTime);
			WaitTime += SleepTime;

			if (WaitTime > Timeout)
			{
				return ETrainerResponse::Timeout;
			}
		}

		if (LogSettings != ELogSetting::Silent)
		{
			UE_LOG(LogLearning, Display, TEXT("Pulling Critic..."));
		}

		// Read the critic
		{
			FScopeNullableWriteLock ScopeLock(NetworkLock);
			int32 Offset = 0;
			OutNetwork.DeserializeFromBytes(Offset, Critic);
		}

		// Confirm we have read the critic
		Controls[(uint8)EControls::CriticSignal] = false;

		return ETrainerResponse::Success;
	}

	ETrainerResponse SendPolicy(
		TLearningArrayView<1, volatile int32> Controls,
		TLearningArrayView<1, uint8> Policy,
		const FNeuralNetwork& Network,
		const float Timeout,
		FRWLock* NetworkLock,
		const ELogSetting LogSettings)
	{
		const float SleepTime = 0.001f;
		float WaitTime = 0.0f;

		// Wait until the policy is requested by the sub-process
		while (!Controls[(uint8)EControls::PolicySignal])
		{
			FPlatformProcess::Sleep(SleepTime);
			WaitTime += SleepTime;

			if (WaitTime > Timeout)
			{
				return ETrainerResponse::Timeout;
			}
		}

		if (LogSettings != ELogSetting::Silent)
		{
			UE_LOG(LogLearning, Display, TEXT("Pushing Policy..."));
		}

		// Write the policy
		{
			FScopeNullableReadLock ScopeLock(NetworkLock);
			int32 Offset = 0;
			Network.SerializeToBytes(Offset, Policy);
		}

		// Confirm we have written the policy
		Controls[(uint8)EControls::PolicySignal] = false;

		return ETrainerResponse::Success;
	}

	ETrainerResponse SendCritic(
		TLearningArrayView<1, volatile int32> Controls,
		TLearningArrayView<1, uint8> Critic,
		const FNeuralNetwork& Network,
		const float Timeout,
		FRWLock* NetworkLock,
		const ELogSetting LogSettings)
	{
		const float SleepTime = 0.001f;
		float WaitTime = 0.0f;

		// Wait until the critic is requested by the sub-process
		while (!Controls[(uint8)EControls::CriticSignal])
		{
			FPlatformProcess::Sleep(SleepTime);
			WaitTime += SleepTime;

			if (WaitTime > Timeout)
			{
				return ETrainerResponse::Timeout;
			}
		}

		if (LogSettings != ELogSetting::Silent)
		{
			UE_LOG(LogLearning, Display, TEXT("Pushing Critic..."));
		}

		// Write the critic
		{
			FScopeNullableReadLock ScopeLock(NetworkLock);
			int32 Offset = 0;
			Network.SerializeToBytes(Offset, Critic);
		}

		// Confirm we have written the critic
		Controls[(uint8)EControls::CriticSignal] = false;

		return ETrainerResponse::Success;
	}

	ETrainerResponse SendExperience(
		TLearningArrayView<1, int32> EpisodeStarts,
		TLearningArrayView<1, int32> EpisodeLengths,
		TLearningArrayView<1, ECompletionMode> EpisodeCompletionModes,
		TLearningArrayView<2, float> EpisodeFinalObservations,
		TLearningArrayView<2, float> Observations,
		TLearningArrayView<2, float> Actions,
		TLearningArrayView<1, float> Rewards,
		TLearningArrayView<1, volatile int32> Controls,
		const FReplayBuffer& ReplayBuffer,
		const float Timeout,
		const ELogSetting LogSettings)
	{
		const float SleepTime = 0.001f;
		float WaitTime = 0.0f;

		// Wait until the sub-process is done reading any experience
		while (Controls[(uint8)EControls::ExperienceSignal])
		{
			FPlatformProcess::Sleep(SleepTime);
			WaitTime += SleepTime;

			if (WaitTime > Timeout)
			{
				return ETrainerResponse::Timeout;
			}
		}

		if (LogSettings != ELogSetting::Silent)
		{
			UE_LOG(LogLearning, Display, TEXT("Pushing Experience..."));
		}

		const int32 EpisodeNum = ReplayBuffer.GetEpisodeNum();
		const int32 StepNum = ReplayBuffer.GetStepNum();

		// Write experience to the shared memory
		Array::Copy(EpisodeStarts.Slice(0, EpisodeNum), ReplayBuffer.GetEpisodeStarts());
		Array::Copy(EpisodeLengths.Slice(0, EpisodeNum), ReplayBuffer.GetEpisodeLengths());
		Array::Copy(EpisodeCompletionModes.Slice(0, EpisodeNum), ReplayBuffer.GetEpisodeCompletionModes());
		Array::Copy(EpisodeFinalObservations.Slice(0, EpisodeNum), ReplayBuffer.GetEpisodeFinalObservations());
		Array::Copy(Observations.Slice(0, StepNum), ReplayBuffer.GetObservations());
		Array::Copy(Actions.Slice(0, StepNum), ReplayBuffer.GetActions());
		Array::Copy(Rewards.Slice(0, StepNum), ReplayBuffer.GetRewards());

		// Indicate that experience is written
		Controls[(uint8)EControls::ExperienceEpisodeNum] = EpisodeNum;
		Controls[(uint8)EControls::ExperienceStepNum] = StepNum;
		Controls[(uint8)EControls::ExperienceSignal] = true;

		return ETrainerResponse::Success;
	}

	ETrainerResponse SendExperience(
		TLearningArrayView<2, float> Observations,
		TLearningArrayView<2, float> Actions,
		TLearningArrayView<1, volatile int32> Controls,
		const TLearningArrayView<2, const float> ObservationExperience,
		const TLearningArrayView<2, const float> ActionExperience,
		const float Timeout,
		const ELogSetting LogSettings)
	{
		const float SleepTime = 0.001f;
		float WaitTime = 0.0f;

		// Wait until the sub-process is done reading any experience
		while (Controls[(uint8)EControls::ExperienceSignal])
		{
			FPlatformProcess::Sleep(SleepTime);
			WaitTime += SleepTime;

			if (WaitTime > Timeout)
			{
				return ETrainerResponse::Timeout;
			}
		}

		if (LogSettings != ELogSetting::Silent)
		{
			UE_LOG(LogLearning, Display, TEXT("Pushing Experience..."));
		}

		const int32 StepNum = ObservationExperience.Num(0);

		// Write experience to the shared memory
		Array::Copy(Observations.Slice(0, StepNum), ObservationExperience);
		Array::Copy(Actions.Slice(0, StepNum), ActionExperience);

		// Confirm that experience is written
		Controls[(uint8)EControls::ExperienceStepNum] = StepNum;
		Controls[(uint8)EControls::ExperienceSignal] = true;

		return ETrainerResponse::Success;
	}

}