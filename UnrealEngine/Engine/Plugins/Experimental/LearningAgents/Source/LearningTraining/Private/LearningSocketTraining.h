// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LearningTrainer.h"

class FSocket;

namespace UE::Learning
{
	struct FNeuralNetwork;
	struct FReplayBuffer;

	namespace SocketTraining
	{
		LEARNINGTRAINING_API ETrainerResponse WaitForConnection(
			FSocket& Socket, 
			const float Timeout = Trainer::DefaultTimeout);

		LEARNINGTRAINING_API ETrainerResponse RecvWithTimeout(
			FSocket& Socket, 
			uint8* Bytes, 
			const int32 ByteNum, 
			const float Timeout = Trainer::DefaultTimeout);

		LEARNINGTRAINING_API ETrainerResponse RecvPolicy(
			FSocket& Socket,
			FNeuralNetwork& OutNetwork,
			TLearningArrayView<1, uint8> OutNetworkBuffer,
			const float Timeout = Trainer::DefaultTimeout,
			FRWLock* NetworkLock = nullptr,
			const ELogSetting LogSettings = Trainer::DefaultLogSettings);

		LEARNINGTRAINING_API ETrainerResponse RecvCritic(
			FSocket& Socket,
			FNeuralNetwork& OutNetwork,
			TLearningArrayView<1, uint8> OutNetworkBuffer,
			const float Timeout = Trainer::DefaultTimeout,
			FRWLock* NetworkLock = nullptr,
			const ELogSetting LogSettings = Trainer::DefaultLogSettings);

		LEARNINGTRAINING_API ETrainerResponse SendWithTimeout(
			FSocket& Socket, 
			const uint8* Bytes, 
			const int32 ByteNum, 
			const float Timeout = Trainer::DefaultTimeout);

		LEARNINGTRAINING_API ETrainerResponse SendConfig(
			FSocket& Socket, 
			const FString& ConfigString, 
			const float Timeout = Trainer::DefaultTimeout);

		LEARNINGTRAINING_API ETrainerResponse SendStop(
			FSocket& Socket, 
			const float Timeout = Trainer::DefaultTimeout);

		LEARNINGTRAINING_API bool HasPolicyOrCompleted(FSocket& Socket);

		LEARNINGTRAINING_API ETrainerResponse SendPolicy(
			FSocket& Socket,
			TLearningArrayView<1, uint8> NetworkBuffer,
			const FNeuralNetwork& Network,
			const float Timeout = Trainer::DefaultTimeout,
			FRWLock* NetworkLock = nullptr,
			const ELogSetting LogSettings = Trainer::DefaultLogSettings);

		LEARNINGTRAINING_API ETrainerResponse SendCritic(
			FSocket& Socket,
			TLearningArrayView<1, uint8> NetworkBuffer,
			const FNeuralNetwork& Network,
			const float Timeout = Trainer::DefaultTimeout,
			FRWLock* NetworkLock = nullptr,
			const ELogSetting LogSettings = Trainer::DefaultLogSettings);

		LEARNINGTRAINING_API ETrainerResponse SendExperience(
			FSocket& Socket,
			const FReplayBuffer& ReplayBuffer,
			const float Timeout = Trainer::DefaultTimeout,
			const ELogSetting LogSettings = Trainer::DefaultLogSettings);

		LEARNINGTRAINING_API ETrainerResponse SendExperience(
			FSocket& Socket,
			const TLearningArrayView<2, const float> ObservationExperience,
			const TLearningArrayView<2, const float> ActionExperience,
			const float Timeout = Trainer::DefaultTimeout,
			const ELogSetting LogSettings = Trainer::DefaultLogSettings);
	}
}