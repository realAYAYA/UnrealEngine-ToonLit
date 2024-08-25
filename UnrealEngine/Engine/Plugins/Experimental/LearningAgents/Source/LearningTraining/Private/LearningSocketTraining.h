// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LearningTrainer.h"

class FSocket;
class ULearningNeuralNetworkData;

namespace UE::Learning
{
	struct FReplayBuffer;

	namespace SocketTraining
	{
		enum class ESignal : uint8
		{
			Invalid				= 0,
			SendConfig			= 1,
			SendExperience		= 2,
			RecvPolicy			= 3,
			SendPolicy			= 4,
			RecvCritic			= 5,
			SendCritic			= 6,
			RecvEncoder			= 7,
			SendEncoder			= 8,
			RecvDecoder			= 9,
			SendDecoder			= 10,
			RecvComplete		= 11,
			SendStop			= 12,
			RecvPing			= 13,
		};

		LEARNINGTRAINING_API ETrainerResponse WaitForConnection(
			FSocket& Socket, 
			const float Timeout = Trainer::DefaultTimeout);

		LEARNINGTRAINING_API ETrainerResponse RecvWithTimeout(
			FSocket& Socket, 
			uint8* Bytes, 
			const int32 ByteNum, 
			const float Timeout = Trainer::DefaultTimeout);

		LEARNINGTRAINING_API ETrainerResponse RecvNetwork(
			FSocket& Socket,
			ULearningNeuralNetworkData& OutNetwork,
			TLearningArrayView<1, uint8> OutNetworkBuffer,
			const ESignal NetworkSignal,
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

		LEARNINGTRAINING_API ETrainerResponse SendNetwork(
			FSocket& Socket,
			TLearningArrayView<1, uint8> NetworkBuffer,
			const ESignal NetworkSignal,
			const ULearningNeuralNetworkData& Network,
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
			const TLearningArrayView<1, const int32> EpisodeStartsExperience,
			const TLearningArrayView<1, const int32> EpisodeLengthsExperience,
			const TLearningArrayView<2, const float> ObservationExperience,
			const TLearningArrayView<2, const float> ActionExperience,
			const float Timeout = Trainer::DefaultTimeout,
			const ELogSetting LogSettings = Trainer::DefaultLogSettings);
	}
}