// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningSocketTraining.h"

#include "LearningProgress.h"
#include "LearningNeuralNetwork.h"
#include "LearningExperience.h"

#include "Sockets.h"
#include "HAL/PlatformProcess.h"

namespace UE::Learning::SocketTraining
{
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

	ETrainerResponse RecvNetwork(
		FSocket& Socket,
		ULearningNeuralNetworkData& OutNetwork,
		TLearningArrayView<1, uint8> OutNetworkBuffer,
		const ESignal NetworkSignal,
		const float Timeout,
		FRWLock* NetworkLock,
		const ELogSetting LogSettings)
	{
		UE_LEARNING_CHECK(OutNetworkBuffer.Num() == OutNetwork.GetSnapshotByteNum());

		if (LogSettings != ELogSetting::Silent)
		{
			UE_LOG(LogLearning, Display, TEXT("Pulling Network..."));
		}

		uint8 Signal = (uint8)ESignal::Invalid;
		ETrainerResponse Response = ETrainerResponse::Unexpected;

		while (true)
		{
			Response = RecvWithTimeout(Socket, &Signal, 1, Timeout);
			if (Response != ETrainerResponse::Success) { return Response; }

			if (Signal == (uint8)ESignal::RecvComplete)
			{
				return ETrainerResponse::Completed;
			}

			if (Signal == (uint8)ESignal::RecvPing)
			{
				continue;
			}

			if (Signal != (uint8)NetworkSignal)
			{
				return ETrainerResponse::Unexpected;
			}

			break;
		}

		Response = RecvWithTimeout(Socket, OutNetworkBuffer.GetData(), OutNetworkBuffer.Num(), Timeout);
		if (Response != ETrainerResponse::Success) { return Response; }

		bool bSuccess = false;
		{
			FScopeNullableWriteLock ScopeLock(NetworkLock);

			if (OutNetworkBuffer.Num() != OutNetwork.GetSnapshotByteNum())
			{
				UE_LOG(LogLearning, Error, TEXT("Error receiving network. Incorrect buffer size. Buffer is %i bytes, expected %i."), OutNetworkBuffer.Num(), OutNetwork.GetSnapshotByteNum());
				bSuccess = false;
			}
			else
			{
				if (!OutNetwork.LoadFromSnapshot(MakeArrayView(OutNetworkBuffer.GetData(), OutNetworkBuffer.Num())))
				{
					UE_LOG(LogLearning, Error, TEXT("Error receiving network. Invalid Format."));
					bSuccess = false;
				}
				else
				{
					bSuccess = true;
				}
			}
		}

		return bSuccess ? ETrainerResponse::Success : ETrainerResponse::Unexpected;
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

	ETrainerResponse SendNetwork(
		FSocket& Socket,
		TLearningArrayView<1, uint8> NetworkBuffer,
		const ESignal NetworkSignal,
		const ULearningNeuralNetworkData& Network,
		const float Timeout,
		FRWLock* NetworkLock,
		const ELogSetting LogSettings)
	{
		if (LogSettings != ELogSetting::Silent)
		{
			UE_LOG(LogLearning, Display, TEXT("Pushing Network..."));
		}

		bool bSuccess = false;
		{
			FScopeNullableReadLock ScopeLock(NetworkLock);
			if (NetworkBuffer.Num() != Network.GetSnapshotByteNum())
			{
				UE_LOG(LogLearning, Error, TEXT("Error sending network. Incorrect buffer size. Buffer is %i bytes, expected %i."), NetworkBuffer.Num(), Network.GetSnapshotByteNum());
				bSuccess = false;
			}
			else
			{
				Network.SaveToSnapshot(MakeArrayView(NetworkBuffer.GetData(), NetworkBuffer.Num()));
				bSuccess = true;
			}
		}

		const uint8 Signal = (uint8)NetworkSignal;
		ETrainerResponse Response = SendWithTimeout(Socket, &Signal, 1, Timeout);
		if (Response != ETrainerResponse::Success) { return Response; }

		Response = SendWithTimeout(Socket, NetworkBuffer.GetData(), NetworkBuffer.Num(), Timeout);
		if (Response != ETrainerResponse::Success) { return Response; }

		return bSuccess ? Response : ETrainerResponse::Unexpected;
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
		
		Response = SendWithTimeout(Socket, (const uint8*)ReplayBuffer.GetEpisodeFinalMemoryStates().GetData(), ReplayBuffer.GetEpisodeFinalMemoryStates().Num() * sizeof(float), Timeout);
		if (Response != ETrainerResponse::Success) { return Response; }

		Response = SendWithTimeout(Socket, (const uint8*)ReplayBuffer.GetObservations().GetData(), ReplayBuffer.GetObservations().Num() * sizeof(float), Timeout);
		if (Response != ETrainerResponse::Success) { return Response; }
		
		Response = SendWithTimeout(Socket, (const uint8*)ReplayBuffer.GetActions().GetData(), ReplayBuffer.GetActions().Num() * sizeof(float), Timeout);
		if (Response != ETrainerResponse::Success) { return Response; }
		
		Response = SendWithTimeout(Socket, (const uint8*)ReplayBuffer.GetMemoryStates().GetData(), ReplayBuffer.GetMemoryStates().Num() * sizeof(float), Timeout);
		if (Response != ETrainerResponse::Success) { return Response; }

		Response = SendWithTimeout(Socket, (const uint8*)ReplayBuffer.GetRewards().GetData(), ReplayBuffer.GetRewards().Num() * sizeof(float), Timeout);
		if (Response != ETrainerResponse::Success) { return Response; }

		return ETrainerResponse::Success;
	}

	ETrainerResponse SendExperience(
		FSocket& Socket,
		const TLearningArrayView<1, const int32> EpisodeStartsExperience,
		const TLearningArrayView<1, const int32> EpisodeLengthsExperience,
		const TLearningArrayView<2, const float> ObservationExperience,
		const TLearningArrayView<2, const float> ActionExperience,
		const float Timeout,
		const ELogSetting LogSettings)
	{
		if (LogSettings != ELogSetting::Silent)
		{
			UE_LOG(LogLearning, Display, TEXT("Pushing Experience..."));
		}

		const int32 EpisodeNum = EpisodeStartsExperience.Num<0>();
		const int32 StepNum = ObservationExperience.Num<0>();

		const uint8 Signal = (uint8)ESignal::SendExperience;
		ETrainerResponse Response = SendWithTimeout(Socket, &Signal, 1, Timeout);
		if (Response != ETrainerResponse::Success) { return Response; }

		Response = SendWithTimeout(Socket, (const uint8*)&EpisodeNum, sizeof(int32), Timeout);
		if (Response != ETrainerResponse::Success) { return Response; }

		Response = SendWithTimeout(Socket, (const uint8*)&StepNum, sizeof(int32), Timeout);
		if (Response != ETrainerResponse::Success) { return Response; }

		Response = SendWithTimeout(Socket, (const uint8*)EpisodeStartsExperience.GetData(), EpisodeStartsExperience.Num() * sizeof(float), Timeout);
		if (Response != ETrainerResponse::Success) { return Response; }

		Response = SendWithTimeout(Socket, (const uint8*)EpisodeLengthsExperience.GetData(), EpisodeLengthsExperience.Num() * sizeof(float), Timeout);
		if (Response != ETrainerResponse::Success) { return Response; }

		Response = SendWithTimeout(Socket, (const uint8*)ObservationExperience.GetData(), ObservationExperience.Num() * sizeof(float), Timeout);
		if (Response != ETrainerResponse::Success) { return Response; }

		Response = SendWithTimeout(Socket, (const uint8*)ActionExperience.GetData(), ActionExperience.Num() * sizeof(float), Timeout);
		if (Response != ETrainerResponse::Success) { return Response; }

		return ETrainerResponse::Success;
	}

}