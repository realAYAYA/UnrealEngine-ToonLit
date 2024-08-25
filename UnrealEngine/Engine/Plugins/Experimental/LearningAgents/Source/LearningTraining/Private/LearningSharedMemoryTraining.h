// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LearningTrainer.h"

class FMonitoredProcess;
class ULearningNeuralNetworkData;

namespace UE::Learning
{
	enum class ECompletionMode : uint8;
	struct FReplayBuffer;

	namespace SharedMemoryTraining
	{
		enum class EControls : uint8
		{
			ExperienceEpisodeNum	= 0,
			ExperienceStepNum		= 1,
			ExperienceSignal		= 2,
			PolicySignal			= 3,
			CriticSignal			= 4,
			EncoderSignal			= 5,
			DecoderSignal			= 6,
			CompleteSignal			= 7,
			StopSignal				= 8,
			PingSignal				= 9,

			ControlNum				= 10,
		};

		LEARNINGTRAINING_API uint8 GetControlNum();

		LEARNINGTRAINING_API ETrainerResponse RecvNetwork(
			FMonitoredProcess* Process,
			TLearningArrayView<1, volatile int32> Controls,
			ULearningNeuralNetworkData& OutNetwork,
			const EControls Signal,
			const TLearningArrayView<1, const uint8> NetworkData,
			const float Timeout = Trainer::DefaultTimeout,
			FRWLock* NetworkLock = nullptr,
			const ELogSetting LogSettings = Trainer::DefaultLogSettings);

		LEARNINGTRAINING_API ETrainerResponse SendStop(
			TLearningArrayView<1, volatile int32> Controls);

		LEARNINGTRAINING_API bool HasPolicyOrCompleted(TLearningArrayView<1, volatile int32> Controls);

		LEARNINGTRAINING_API ETrainerResponse SendNetwork(
			FMonitoredProcess* Process,
			TLearningArrayView<1, volatile int32> Controls,
			TLearningArrayView<1, uint8> NetworkData,
			const EControls Signal,
			const ULearningNeuralNetworkData& Network,
			const float Timeout = Trainer::DefaultTimeout,
			FRWLock* NetworkLock = nullptr,
			const ELogSetting LogSettings = Trainer::DefaultLogSettings);

		LEARNINGTRAINING_API ETrainerResponse SendExperience(
			FMonitoredProcess* Process,
			TLearningArrayView<1, int32> EpisodeStarts,
			TLearningArrayView<1, int32> EpisodeLengths,
			TLearningArrayView<1, ECompletionMode> EpisodeCompletionModes,
			TLearningArrayView<2, float> EpisodeFinalObservations,
			TLearningArrayView<2, float> EpisodeFinalMemoryStates,
			TLearningArrayView<2, float> Observations,
			TLearningArrayView<2, float> Actions,
			TLearningArrayView<2, float> MemoryStates,
			TLearningArrayView<1, float> Rewards,
			TLearningArrayView<1, volatile int32> Controls,
			const FReplayBuffer& ReplayBuffer,
			const float Timeout = Trainer::DefaultTimeout,
			const ELogSetting LogSettings = Trainer::DefaultLogSettings);

		LEARNINGTRAINING_API ETrainerResponse SendExperience(
			FMonitoredProcess* Process,
			TLearningArrayView<1, int32> EpisodeStarts,
			TLearningArrayView<1, int32> EpisodeLengths,
			TLearningArrayView<2, float> Observations,
			TLearningArrayView<2, float> Actions,
			TLearningArrayView<1, volatile int32> Controls,
			const TLearningArrayView<1, const int32> EpisodeStartsExperience,
			const TLearningArrayView<1, const int32> EpisodeLengthsExperience,
			const TLearningArrayView<2, const float> ObservationExperience,
			const TLearningArrayView<2, const float> ActionExperience,
			const float Timeout = Trainer::DefaultTimeout,
			const ELogSetting LogSettings = Trainer::DefaultLogSettings);

	}
}
