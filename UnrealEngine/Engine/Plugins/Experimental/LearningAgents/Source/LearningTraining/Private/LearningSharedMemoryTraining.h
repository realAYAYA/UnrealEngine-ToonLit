// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LearningTrainer.h"

namespace UE::Learning
{
	enum class ECompletionMode : uint8;
	struct FNeuralNetwork;
	struct FReplayBuffer;

	namespace SharedMemoryTraining
	{
		LEARNINGTRAINING_API uint8 GetControlNum();

		LEARNINGTRAINING_API ETrainerResponse RecvPolicy(
			TLearningArrayView<1, volatile int32> Controls,
			FNeuralNetwork& OutNetwork,
			const TLearningArrayView<1, const uint8> Policy,
			const float Timeout = Trainer::DefaultTimeout,
			FRWLock* NetworkLock = nullptr,
			const ELogSetting LogSettings = Trainer::DefaultLogSettings);

		LEARNINGTRAINING_API ETrainerResponse RecvCritic(
			TLearningArrayView<1, volatile int32> Controls,
			FNeuralNetwork& OutNetwork,
			const TLearningArrayView<1, const uint8> Critic,
			const float Timeout = Trainer::DefaultTimeout,
			FRWLock* NetworkLock = nullptr,
			const ELogSetting LogSettings = Trainer::DefaultLogSettings);

		LEARNINGTRAINING_API ETrainerResponse SendStop(
			TLearningArrayView<1, volatile int32> Controls);

		LEARNINGTRAINING_API bool HasPolicyOrCompleted(TLearningArrayView<1, volatile int32> Controls);

		LEARNINGTRAINING_API ETrainerResponse SendPolicy(
			TLearningArrayView<1, volatile int32> Controls,
			TLearningArrayView<1, uint8> Policy,
			const FNeuralNetwork& Network,
			const float Timeout = Trainer::DefaultTimeout,
			FRWLock* NetworkLock = nullptr,
			const ELogSetting LogSettings = Trainer::DefaultLogSettings);

		LEARNINGTRAINING_API ETrainerResponse SendCritic(
			TLearningArrayView<1, volatile int32> Controls,
			TLearningArrayView<1, uint8> Critic,
			const FNeuralNetwork& Network,
			const float Timeout = Trainer::DefaultTimeout,
			FRWLock* NetworkLock = nullptr,
			const ELogSetting LogSettings = Trainer::DefaultLogSettings);

		LEARNINGTRAINING_API ETrainerResponse SendExperience(
			TLearningArrayView<1, int32> EpisodeStarts,
			TLearningArrayView<1, int32> EpisodeLengths,
			TLearningArrayView<1, ECompletionMode> EpisodeCompletionModes,
			TLearningArrayView<2, float> EpisodeFinalObservations,
			TLearningArrayView<2, float> Observations,
			TLearningArrayView<2, float> Actions,
			TLearningArrayView<1, float> Rewards,
			TLearningArrayView<1, volatile int32> Controls,
			const FReplayBuffer& ReplayBuffer,
			const float Timeout = Trainer::DefaultTimeout,
			const ELogSetting LogSettings = Trainer::DefaultLogSettings);

		LEARNINGTRAINING_API ETrainerResponse SendExperience(
			TLearningArrayView<2, float> Observations,
			TLearningArrayView<2, float> Actions,
			TLearningArrayView<1, volatile int32> Controls,
			const TLearningArrayView<2, const float> ObservationExperience,
			const TLearningArrayView<2, const float> ActionExperience,
			const float Timeout = Trainer::DefaultTimeout,
			const ELogSetting LogSettings = Trainer::DefaultLogSettings);

	}
}
