// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningSocketTraining.h"

#include "LearningProgress.h"
#include "LearningNeuralNetwork.h"
#include "LearningExperience.h"

#include "Sockets.h"
#include "HAL/PlatformProcess.h"

namespace UE::Learning::SocketTraining
{
	enum class ESignal : uint8
	{
		Invalid = 0,
		SendConfig = 1,
		SendExperience = 2,
		RecvPolicy = 3,
		SendPolicy = 4,
		RecvCritic = 5,
		SendCritic = 6,
		RecvComplete = 7,
		SendStop = 8,
	};

	ETrainerResponse WaitForConnection(FSocket& Socket, const float Timeout)
	{
		float WaitTime = 0.0f;
		const float SleepTime = 0.001f;

		while (true)
		{
			if (Socket.GetConnectionState() == ESocketConnectionState::SCS_ConnectionError)
			{
				UE_LOG(LogLearning, Error, TEXT("Socket refused connection..."));
				return ETrainerResponse::Unexpected;
			}

			if (Socket.GetConnectionState() == ESocketConnectionState::SCS_Connected)
			{
				break;
			}
			else
			{
				FPlatformProcess::Sleep(SleepTime);
				WaitTime += SleepTime;

				if (WaitTime > Timeout)
				{
					return  ETrainerResponse::Timeout;
				}
			}
		}

		return ETrainerResponse::Success;
	}

	ETrainerResponse RecvWithTimeout(FSocket& Socket, uint8* Bytes, const int32 ByteNum, const float Timeout)
	{
		float WaitTime = 0.0f;
		const float SleepTime = 0.001f;

		int32 BytesRead = 0;
		int32 TotalBytesRead = 0;

		while (true)
		{
			if (Socket.Recv(Bytes + TotalBytesRead, ByteNum - TotalBytesRead, BytesRead))
			{
				TotalBytesRead += BytesRead;

				if (TotalBytesRead == ByteNum)
				{
					return ETrainerResponse::Success;
				}
			}

			FPlatformProcess::Sleep(SleepTime);
			WaitTime += SleepTime;

			if (WaitTime > Timeout)
			{
				return ETrainerResponse::Timeout;
			}
		}
	}

	ETrainerResponse RecvPolicy(
		FSocket& Socket,
		FNeuralNetwork& OutNetwork,
		TLearningArrayView<1, uint8> OutNetworkBuffer,
		const float Timeout,
		FRWLock* NetworkLock,
		const ELogSetting LogSettings)
	{
		if (LogSettings != ELogSetting::Silent)
		{
			UE_LOG(LogLearning, Display, TEXT("Pulling Policy..."));
		}

		uint8 Signal = (uint8)ESignal::Invalid;
		ETrainerResponse Response = RecvWithTimeout(Socket, &Signal, 1, Timeout);
		if (Response != ETrainerResponse::Success) { return Response; }

		if (Signal == (uint8)ESignal::RecvComplete)
		{
			return ETrainerResponse::Completed;
		}

		if (Signal != (uint8)ESignal::RecvPolicy)
		{
			return ETrainerResponse::Unexpected;
		}

		Response = RecvWithTimeout(Socket, OutNetworkBuffer.GetData(), OutNetworkBuffer.Num(), Timeout);
		if (Response != ETrainerResponse::Success) { return Response; }

		{
			FScopeNullableWriteLock ScopeLock(NetworkLock);
			int32 Offset = 0;
			OutNetwork.DeserializeFromBytes(Offset, OutNetworkBuffer);
		}

		return ETrainerResponse::Success;
	}

	ETrainerResponse RecvCritic(
		FSocket& Socket,
		FNeuralNetwork& OutNetwork,
		TLearningArrayView<1, uint8> OutNetworkBuffer,
		const float Timeout,
		FRWLock* NetworkLock,
		const ELogSetting LogSettings)
	{
		if (LogSettings != ELogSetting::Silent)
		{
			UE_LOG(LogLearning, Display, TEXT("Pulling Critic..."));
		}

		uint8 Signal = (uint8)ESignal::Invalid;
		ETrainerResponse Response = RecvWithTimeout(Socket, &Signal, 1, Timeout);
		if (Response != ETrainerResponse::Success) { return Response; }

		if (Signal != (uint8)ESignal::RecvCritic)
		{
			return ETrainerResponse::Unexpected;
		}

		Response = RecvWithTimeout(Socket, OutNetworkBuffer.GetData(), OutNetworkBuffer.Num(), Timeout);
		if (Response != ETrainerResponse::Success) { return Response; }

		{
			FScopeNullableWriteLock ScopeLock(NetworkLock);
			int32 Offset = 0;
			OutNetwork.DeserializeFromBytes(Offset, OutNetworkBuffer);
		}

		return ETrainerResponse::Success;
	}

	ETrainerResponse SendWithTimeout(FSocket& Socket, const uint8* Bytes, const int32 ByteNum, const float Timeout)
	{
		float WaitTime = 0.0f;
		const float SleepTime = 0.001f;

		int32 BytesSent = 0;
		int32 TotalBytesSent = 0;

		while (true)
		{
			if (Socket.Send(Bytes + TotalBytesSent, ByteNum - TotalBytesSent, BytesSent))
			{
				TotalBytesSent += BytesSent;

				if (TotalBytesSent == ByteNum)
				{
					return ETrainerResponse::Success;
				}
			}

			FPlatformProcess::Sleep(SleepTime);
			WaitTime += SleepTime;

			if (WaitTime > Timeout)
			{
				return ETrainerResponse::Timeout;
			}
		}
	}

	ETrainerResponse SendConfig(FSocket& Socket, const FString& ConfigString, const float Timeout)
	{
		const uint8 Signal = (uint8)ESignal::SendConfig;
		ETrainerResponse Response = SendWithTimeout(Socket, &Signal, 1, Timeout);
		if (Response != ETrainerResponse::Success) { return Response; }

		const FTCHARToUTF8 UTF8String(*ConfigString);
		const int32 ConfigLength = UTF8String.Length();
		
		Response = SendWithTimeout(Socket, (uint8*)&ConfigLength, sizeof(int32), Timeout);
		if (Response != ETrainerResponse::Success) { return Response; }
		
		Response = SendWithTimeout(Socket, (uint8*)UTF8String.Get(), UTF8String.Length(), Timeout);
		if (Response != ETrainerResponse::Success) { return Response; }

		return ETrainerResponse::Success;
	}

	ETrainerResponse SendStop(FSocket& Socket, const float Timeout)
	{
		const uint8 Signal = (uint8)ESignal::SendStop;
		return SendWithTimeout(Socket, &Signal, 1, Timeout);
	}

	bool HasPolicyOrCompleted(FSocket& Socket)
	{
		uint32 PendingDataSize;
		return Socket.HasPendingData(PendingDataSize);
	}

	ETrainerResponse SendPolicy(
		FSocket& Socket,
		TLearningArrayView<1, uint8> NetworkBuffer,
		const FNeuralNetwork& Network,
		const float Timeout,
		FRWLock* NetworkLock,
		const ELogSetting LogSettings)
	{
		if (LogSettings != ELogSetting::Silent)
		{
			UE_LOG(LogLearning, Display, TEXT("Pushing Policy..."));
		}

		{
			FScopeNullableReadLock ScopeLock(NetworkLock);
			int32 Offset = 0;
			Network.SerializeToBytes(Offset, NetworkBuffer);
		}

		const uint8 Signal = (uint8)ESignal::SendPolicy;
		ETrainerResponse Response = SendWithTimeout(Socket, &Signal, 1, Timeout);
		if (Response != ETrainerResponse::Success) { return Response; }

		Response = SendWithTimeout(Socket, NetworkBuffer.GetData(), NetworkBuffer.Num(), Timeout);
		if (Response != ETrainerResponse::Success) { return Response; }

		return ETrainerResponse::Success;
	}

	ETrainerResponse SendCritic(
		FSocket& Socket,
		TLearningArrayView<1, uint8> NetworkBuffer,
		const FNeuralNetwork& Network,
		const float Timeout,
		FRWLock* NetworkLock,
		const ELogSetting LogSettings)
	{
		if (LogSettings != ELogSetting::Silent)
		{
			UE_LOG(LogLearning, Display, TEXT("Pushing Critic..."));
		}

		{
			FScopeNullableReadLock ScopeLock(NetworkLock);
			int32 Offset = 0;
			Network.SerializeToBytes(Offset, NetworkBuffer);
		}

		const uint8 Signal = (uint8)ESignal::SendCritic;
		ETrainerResponse Response = SendWithTimeout(Socket, &Signal, 1, Timeout);
		if (Response != ETrainerResponse::Success) { return Response; }

		Response = SendWithTimeout(Socket, NetworkBuffer.GetData(), NetworkBuffer.Num(), Timeout);
		if (Response != ETrainerResponse::Success) { return Response; }

		return ETrainerResponse::Success;
	}

	ETrainerResponse SendExperience(
		FSocket& Socket,
		const FReplayBuffer& ReplayBuffer,
		const float Timeout,
		const ELogSetting LogSettings)
	{
		if (LogSettings != ELogSetting::Silent)
		{
			UE_LOG(LogLearning, Display, TEXT("Pushing Experience..."));
		}

		const int32 EpisodeNum = ReplayBuffer.GetEpisodeNum();
		const int32 StepNum = ReplayBuffer.GetStepNum();

		const uint8 Signal = (uint8)ESignal::SendExperience;
		ETrainerResponse Response = SendWithTimeout(Socket, &Signal, 1, Timeout);
		if (Response != ETrainerResponse::Success) { return Response; }

		Response = SendWithTimeout(Socket, (const uint8*)&EpisodeNum, sizeof(int32), Timeout);
		if (Response != ETrainerResponse::Success) { return Response; }
		
		Response = SendWithTimeout(Socket, (const uint8*)&StepNum, sizeof(int32), Timeout);
		if (Response != ETrainerResponse::Success) { return Response; }
		
		Response = SendWithTimeout(Socket, (const uint8*)ReplayBuffer.GetEpisodeStarts().GetData(), ReplayBuffer.GetEpisodeStarts().Num() * sizeof(int32), Timeout);
		if (Response != ETrainerResponse::Success) { return Response; }
		
		Response = SendWithTimeout(Socket, (const uint8*)ReplayBuffer.GetEpisodeLengths().GetData(), ReplayBuffer.GetEpisodeLengths().Num() * sizeof(int32), Timeout);
		if (Response != ETrainerResponse::Success) { return Response; }
		
		Response = SendWithTimeout(Socket, (const uint8*)ReplayBuffer.GetEpisodeCompletionModes().GetData(), ReplayBuffer.GetEpisodeCompletionModes().Num() * sizeof(ECompletionMode), Timeout);
		if (Response != ETrainerResponse::Success) { return Response; }
		
		Response = SendWithTimeout(Socket, (const uint8*)ReplayBuffer.GetEpisodeFinalObservations().GetData(), ReplayBuffer.GetEpisodeFinalObservations().Num() * sizeof(float), Timeout);
		if (Response != ETrainerResponse::Success) { return Response; }
		
		Response = SendWithTimeout(Socket, (const uint8*)ReplayBuffer.GetObservations().GetData(), ReplayBuffer.GetObservations().Num() * sizeof(float), Timeout);
		if (Response != ETrainerResponse::Success) { return Response; }
		
		Response = SendWithTimeout(Socket, (const uint8*)ReplayBuffer.GetActions().GetData(), ReplayBuffer.GetActions().Num() * sizeof(float), Timeout);
		if (Response != ETrainerResponse::Success) { return Response; }
		
		Response = SendWithTimeout(Socket, (const uint8*)ReplayBuffer.GetRewards().GetData(), ReplayBuffer.GetRewards().Num() * sizeof(float), Timeout);
		if (Response != ETrainerResponse::Success) { return Response; }

		return ETrainerResponse::Success;
	}

	ETrainerResponse SendExperience(
		FSocket& Socket,
		const TLearningArrayView<2, const float> ObservationExperience,
		const TLearningArrayView<2, const float> ActionExperience,
		const float Timeout,
		const ELogSetting LogSettings)
	{
		if (LogSettings != ELogSetting::Silent)
		{
			UE_LOG(LogLearning, Display, TEXT("Pushing Experience..."));
		}

		const int32 SampleNum = ObservationExperience.Num<0>();

		const uint8 Signal = (uint8)ESignal::SendExperience;
		ETrainerResponse Response = SendWithTimeout(Socket, &Signal, 1, Timeout);
		if (Response != ETrainerResponse::Success) { return Response; }

		Response = SendWithTimeout(Socket, (const uint8*)&SampleNum, sizeof(int32), Timeout);
		if (Response != ETrainerResponse::Success) { return Response; }

		Response = SendWithTimeout(Socket, (const uint8*)ObservationExperience.GetData(), ObservationExperience.Num() * sizeof(float), Timeout);
		if (Response != ETrainerResponse::Success) { return Response; }

		Response = SendWithTimeout(Socket, (const uint8*)ActionExperience.GetData(), ActionExperience.Num() * sizeof(float), Timeout);
		if (Response != ETrainerResponse::Success) { return Response; }

		return ETrainerResponse::Success;
	}

}