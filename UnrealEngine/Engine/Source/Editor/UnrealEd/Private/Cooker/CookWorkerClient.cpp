// Copyright Epic Games, Inc. All Rights Reserved.

#include "CookWorkerClient.h"

#include "CompactBinaryTCP.h"
#include "CookDirector.h"
#include "CookMPCollector.h"
#include "CookPackageData.h"
#include "CookPlatformManager.h"
#include "CookTypes.h"
#include "CookWorkerServer.h"
#include "CoreGlobals.h"
#include "HAL/PlatformTime.h"
#include "PackageResultsMessage.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "WorkerRequestsRemote.h"

namespace UE::Cook
{

namespace CookWorkerClient
{
constexpr float WaitForConnectReplyTimeout = 60.f;
}

FCookWorkerClient::FCookWorkerClient(UCookOnTheFlyServer& InCOTFS)
	: COTFS(InCOTFS)
{
	FLogMessagesMessageHandler* Handler = new FLogMessagesMessageHandler();
	Handler->InitializeClient();
	Register(Handler);
}

FCookWorkerClient::~FCookWorkerClient()
{
	if (ConnectStatus == EConnectStatus::Connected ||
		(EConnectStatus::FlushAndAbortFirst <= ConnectStatus && ConnectStatus <= EConnectStatus::FlushAndAbortLast))
	{
		UE_LOG(LogCook, Warning, TEXT("CookWorkerServer %d was destroyed before it finished Disconnect. The CookDirector may be missing some information."));
	}
	Sockets::CloseSocket(ServerSocket);
}

bool FCookWorkerClient::TryConnect(FDirectorConnectionInfo&& ConnectInfo)
{
	EPollStatus Status;
	for (;;)
	{
		Status = PollTryConnect(ConnectInfo);
		if (Status != EPollStatus::Incomplete)
		{
			break;
		}
		constexpr float SleepTime = 0.01f; // 10 ms
		FPlatformProcess::Sleep(SleepTime);
	}
	return Status == EPollStatus::Success;
}

void FCookWorkerClient::TickFromSchedulerThread(FTickStackData& StackData)
{
	if (ConnectStatus == EConnectStatus::Connected)
	{
		PumpReceiveMessages();
		if (ConnectStatus == EConnectStatus::Connected)
		{
			SendPendingResults();
			PumpSendMessages();
			TickCollectors(StackData, false /* bFlush */);
		}
	}
	else
	{
		PumpDisconnect(StackData);
	}
}

bool FCookWorkerClient::IsDisconnecting() const
{
	return ConnectStatus == EConnectStatus::LostConnection ||
		(EConnectStatus::FlushAndAbortFirst <= ConnectStatus && ConnectStatus <= EConnectStatus::FlushAndAbortLast);
}

bool FCookWorkerClient::IsDisconnectComplete() const
{
	return ConnectStatus == EConnectStatus::LostConnection;
}

ECookInitializationFlags FCookWorkerClient::GetCookInitializationFlags()
{
	check(InitialConfigMessage); // Should only be called after TryConnect and before DoneWithInitialSettings
	return InitialConfigMessage->GetCookInitializationFlags();
}
FInitializeConfigSettings&& FCookWorkerClient::ConsumeInitializeConfigSettings()
{
	check(InitialConfigMessage); // Should only be called after TryConnect and before DoneWithInitialSettings
	return InitialConfigMessage->ConsumeInitializeConfigSettings();
}
FBeginCookConfigSettings&& FCookWorkerClient::ConsumeBeginCookConfigSettings()
{
	check(InitialConfigMessage); // Should only be called after TryConnect and before DoneWithInitialSettings
	return InitialConfigMessage->ConsumeBeginCookConfigSettings();
}
FCookByTheBookOptions&& FCookWorkerClient::ConsumeCookByTheBookOptions()
{
	check(InitialConfigMessage); // Should only be called after TryConnect and before DoneWithInitialSettings
	return InitialConfigMessage->ConsumeCookByTheBookOptions();
}
const FBeginCookContextForWorker& FCookWorkerClient::GetBeginCookContext()
{
	check(InitialConfigMessage); // Should only be called after TryConnect and before DoneWithInitialSettings
	return InitialConfigMessage->GetBeginCookContext();
}
FCookOnTheFlyOptions&& FCookWorkerClient::ConsumeCookOnTheFlyOptions()
{
	check(InitialConfigMessage); // Should only be called after TryConnect and before DoneWithInitialSettings
	return InitialConfigMessage->ConsumeCookOnTheFlyOptions();
}
const TArray<ITargetPlatform*>& FCookWorkerClient::GetTargetPlatforms() const
{
	return OrderedSessionPlatforms;
}
void FCookWorkerClient::DoneWithInitialSettings()
{
	InitialConfigMessage.Reset();
}

void FCookWorkerClient::ReportDemoteToIdle(const FPackageData& PackageData, ESuppressCookReason Reason)
{
	FPackageRemoteResult& Result = PendingResults.Emplace_GetRef();
	Result.PackageName = PackageData.GetPackageName();
	Result.SuppressCookReason = Reason;
}

void FCookWorkerClient::ReportPromoteToSaveComplete(FPackageData& PackageData)
{
	TUniquePtr<FPackageRemoteResult> MovedResult = MoveTemp(PackageData.GetPackageRemoteResult());
	FPackageRemoteResult* Result;
	if (MovedResult)
	{
		Result = &PendingResults.Emplace_GetRef(MoveTemp(*MovedResult));
	} 
	else
	{
		Result = &PendingResults.Emplace_GetRef();
	}

	Result->PackageName = PackageData.GetPackageName();
	Result->SuppressCookReason = ESuppressCookReason::InvalidSuppressCookReason;

	// Sort the platforms to match the OrderedSessionPlatforms order, and add any missing platforms
	int32 NumPlatforms = OrderedSessionPlatforms.Num();
	bool bAlreadySorted = true;
	for (int32 PlatformIndex = 0; PlatformIndex < NumPlatforms; ++PlatformIndex)
	{
		if (OrderedSessionPlatforms[PlatformIndex] != Result->Platforms[PlatformIndex].Platform)
		{
			bAlreadySorted = false;
			break;
		}
	}
	if (!bAlreadySorted)
	{
		TArray<FPackageRemoteResult::FPlatformResult, TInlineAllocator<1>> SortedPlatforms;
		SortedPlatforms.SetNum(NumPlatforms);
		for (FPackageRemoteResult::FPlatformResult& ExistingResult : Result->Platforms)
		{
			int32 PlatformIndex = OrderedSessionPlatforms.IndexOfByKey(ExistingResult.Platform);
			check(PlatformIndex != INDEX_NONE && SortedPlatforms[PlatformIndex].Platform == nullptr); // Only platforms in the session platforms should have been added, and there should not be duplicates
			SortedPlatforms[PlatformIndex] = MoveTemp(ExistingResult);
		}
		for (int32 PlatformIndex = 0; PlatformIndex < NumPlatforms; ++PlatformIndex)
		{
			FPackageRemoteResult::FPlatformResult& Sorted = SortedPlatforms[PlatformIndex];
			if (!Sorted.Platform)
			{
				Sorted.Platform = OrderedSessionPlatforms[PlatformIndex];
				Sorted.bSuccessful = false;
			}
		}
		Swap(Result->Platforms, SortedPlatforms);
	}

	for (int32 PlatformIndex = 0; PlatformIndex < NumPlatforms; ++PlatformIndex)
	{
		ITargetPlatform* TargetPlatform = OrderedSessionPlatforms[PlatformIndex];
		FPackageRemoteResult::FPlatformResult& PlatformResults = Result->Platforms[PlatformIndex];
		FPackageData::FPlatformData& PackagePlatformData = PackageData.FindOrAddPlatformData(TargetPlatform);
		PlatformResults.bSuccessful = PackagePlatformData.bCookSucceeded;
	}
}

void FCookWorkerClient::ReportDiscoveredPackage(const FPackageData& PackageData, const FInstigator& Instigator)
{
	FDiscoveredPackage& Discovered = PendingDiscoveredPackages.Emplace_GetRef();
	Discovered.PackageName = PackageData.GetPackageName();
	Discovered.NormalizedFileName = PackageData.GetFileName();
	Discovered.Instigator = Instigator;
}

EPollStatus FCookWorkerClient::PollTryConnect(const FDirectorConnectionInfo& ConnectInfo)
{
	for (;;)
	{
		switch (ConnectStatus)
		{
		case EConnectStatus::Connected:
			return EPollStatus::Success;
		case EConnectStatus::Uninitialized:
			CreateServerSocket(ConnectInfo);
			break;
		case EConnectStatus::PollWriteConnectMessage:
			PollWriteConnectMessage();
			if (ConnectStatus == EConnectStatus::PollWriteConnectMessage)
			{
				return EPollStatus::Incomplete;
			}
			break;
		case EConnectStatus::PollReceiveConfigMessage:
			PollReceiveConfigMessage();
			if (ConnectStatus == EConnectStatus::PollReceiveConfigMessage)
			{
				return EPollStatus::Incomplete;
			}
			break;
		case EConnectStatus::LostConnection:
			return EPollStatus::Error;
		default:
			return EPollStatus::Error;
		}
	}
}

void FCookWorkerClient::CreateServerSocket(const FDirectorConnectionInfo& ConnectInfo)
{
	using namespace CompactBinaryTCP;

	ConnectStartTimeSeconds = FPlatformTime::Seconds();
	DirectorURI = ConnectInfo.HostURI;

	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get();
	if (!SocketSubsystem)
	{
		UE_LOG(LogCook, Error, TEXT("CookWorker initialization failure: platform does not support network sockets, cannot connect to CookDirector."));
		SendToState(EConnectStatus::LostConnection);
		return;
	}

	DirectorAddr = Sockets::GetAddressFromStringWithPort(DirectorURI);
	if (!DirectorAddr)
	{
		UE_LOG(LogCook, Error, TEXT("CookWorker initialization failure: could not convert -CookDirectorHost=%s into an address, cannot connect to CookDirector."),
			*DirectorURI);
		SendToState(EConnectStatus::LostConnection);
		return;
	}

	UE_LOG(LogCook, Display, TEXT("Connecting to CookDirector at %s..."), *DirectorURI);

	ServerSocket = Sockets::ConnectToHost(*DirectorAddr, TEXT("FCookWorkerClient-WorkerConnect"));
	if (!ServerSocket)
	{
		UE_LOG(LogCook, Error, TEXT("CookWorker initialization failure: Could not connect to CookDirector."));
		SendToState(EConnectStatus::LostConnection);
		return;
	}

	constexpr float WaitForConnectTimeout = 60.f * 10;
	float ConditionalTimeoutSeconds = IsCookIgnoreTimeouts() ? MAX_flt : WaitForConnectTimeout;
	bool bServerSocketReady = ServerSocket->Wait(ESocketWaitConditions::WaitForWrite, FTimespan::FromSeconds(ConditionalTimeoutSeconds));
	if (!bServerSocketReady)
	{
		UE_LOG(LogCook, Error, TEXT("CookWorker initialization failure: Timed out after %.0f seconds trying to connect to CookDirector."),
			ConditionalTimeoutSeconds);
		SendToState(EConnectStatus::LostConnection);
		return;
	}

	FWorkerConnectMessage ConnectMessage;
	ConnectMessage.RemoteIndex = ConnectInfo.RemoteIndex;
	EConnectionStatus Status = TryWritePacket(ServerSocket, SendBuffer, ConnectMessage);
	if (Status == EConnectionStatus::Incomplete)
	{
		SendToState(EConnectStatus::PollWriteConnectMessage);
		return;
	}
	else if (Status != EConnectionStatus::Okay)
	{
		UE_LOG(LogCook, Error, TEXT("CookWorker initialization failure: could not send ConnectMessage."));
		SendToState(EConnectStatus::LostConnection);
		return;
	}
	LogConnected();

	SendToState(EConnectStatus::PollReceiveConfigMessage);
}

void FCookWorkerClient::PollWriteConnectMessage()
{
	using namespace CompactBinaryTCP;

	EConnectionStatus Status = TryFlushBuffer(ServerSocket, SendBuffer);
	if (Status == EConnectionStatus::Incomplete)
	{
		if (FPlatformTime::Seconds() - ConnectStartTimeSeconds > CookWorkerClient::WaitForConnectReplyTimeout &&
			!IsCookIgnoreTimeouts())
		{
			UE_LOG(LogCook, Error, TEXT("CookWorker initialization failure: timed out waiting for %fs to send ConnectMessage."),
				CookWorkerClient::WaitForConnectReplyTimeout);
			SendToState(EConnectStatus::LostConnection);
		}
		return;
	}
	else if (Status != EConnectionStatus::Okay)
	{
		UE_LOG(LogCook, Error, TEXT("CookWorker initialization failure: could not send ConnectMessage."));
		SendToState(EConnectStatus::LostConnection);
		return;
	}
	LogConnected();
	SendToState(EConnectStatus::PollReceiveConfigMessage);
}

void FCookWorkerClient::PollReceiveConfigMessage()
{
	using namespace UE::CompactBinaryTCP;
	TArray<FMarshalledMessage> Messages;
	EConnectionStatus SocketStatus = TryReadPacket(ServerSocket, ReceiveBuffer, Messages);
	if (SocketStatus != EConnectionStatus::Okay && SocketStatus != EConnectionStatus::Incomplete)
	{
		UE_LOG(LogCook, Error, TEXT("CookWorker initialization failure: failed to read from socket."));
		SendToState(EConnectStatus::LostConnection);
		return;
	}
	if (Messages.Num() == 0)
	{
		if (FPlatformTime::Seconds() - ConnectStartTimeSeconds > CookWorkerClient::WaitForConnectReplyTimeout &&
			!IsCookIgnoreTimeouts())
		{
			UE_LOG(LogCook, Error, TEXT("CookWorker initialization failure: timed out waiting for %fs to receive InitialConfigMessage."),
				CookWorkerClient::WaitForConnectReplyTimeout);
			SendToState(EConnectStatus::LostConnection);
		}
		return;
	}
	
	if (Messages[0].MessageType != FInitialConfigMessage::MessageType)
	{
		UE_LOG(LogCook, Warning, TEXT("CookWorker initialization failure: Director sent a different message before sending an InitialConfigMessage. MessageType: %s."),
			*Messages[0].MessageType.ToString());
		SendToState(EConnectStatus::LostConnection);
		return;
	}
	check(!InitialConfigMessage);
	InitialConfigMessage = MakeUnique<FInitialConfigMessage>();
	if (!InitialConfigMessage->TryRead(MoveTemp(Messages[0].Object)))
	{
		UE_LOG(LogCook, Warning, TEXT("CookWorker initialization failure: Director sent an invalid InitialConfigMessage."));
		SendToState(EConnectStatus::LostConnection);
		return;
	}
	DirectorCookMode = InitialConfigMessage->GetDirectorCookMode();
	OrderedSessionPlatforms = InitialConfigMessage->GetOrderedSessionPlatforms();

	UE_LOG(LogCook, Display, TEXT("Initialization from CookDirector complete."));
	SendToState(EConnectStatus::Connected);
	Messages.RemoveAt(0);
	HandleReceiveMessages(MoveTemp(Messages));
}

void FCookWorkerClient::LogConnected()
{
	UE_LOG(LogCook, Display, TEXT("Connection to CookDirector successful."));
}

void FCookWorkerClient::PumpSendMessages()
{
	UE::CompactBinaryTCP::EConnectionStatus Status = UE::CompactBinaryTCP::TryFlushBuffer(ServerSocket, SendBuffer);
	if (Status == UE::CompactBinaryTCP::Failed)
	{
		UE_LOG(LogCook, Error, TEXT("CookWorkerClient failed to write message to Director. We will abort the CookAsCookWorker commandlet."));
		SendToState(EConnectStatus::LostConnection);
	}
}

void FCookWorkerClient::SendPendingResults()
{
	if (!PendingResults.IsEmpty())
	{
		FPackageResultsMessage Message;
		Message.Results = MoveTemp(PendingResults);
		SendMessage(Message);
		PendingResults.Reset();
	}
	if (!PendingDiscoveredPackages.IsEmpty())
	{
		FDiscoveredPackagesMessage Message;
		Message.Packages = MoveTemp(PendingDiscoveredPackages);
		SendMessage(Message);
		PendingDiscoveredPackages.Reset();
	}
}

void FCookWorkerClient::PumpReceiveMessages()
{
	using namespace UE::CompactBinaryTCP;
	TArray<FMarshalledMessage> Messages;
	EConnectionStatus SocketStatus = TryReadPacket(ServerSocket, ReceiveBuffer, Messages);
	if (SocketStatus != EConnectionStatus::Okay && SocketStatus != EConnectionStatus::Incomplete)
	{
		UE_LOG(LogCook, Error, TEXT("CookWorkerClient failed to read from Director. We will abort the CookAsCookWorker commandlet."));
		SendToState(EConnectStatus::LostConnection);
		return;
	}
	HandleReceiveMessages(MoveTemp(Messages));
}

void FCookWorkerClient::HandleReceiveMessages(TArray<UE::CompactBinaryTCP::FMarshalledMessage>&& Messages)
{
	for (UE::CompactBinaryTCP::FMarshalledMessage& Message : Messages)
	{
		if (EConnectStatus::FlushAndAbortFirst <= ConnectStatus && ConnectStatus <= EConnectStatus::FlushAndAbortLast)
		{
			if (Message.MessageType == FAbortWorkerMessage::MessageType)
			{
				UE_LOG(LogCook, Display, TEXT("CookWorkerClient received AbortWorker message from Director. Terminating flush and shutting down."));
				SendToState(EConnectStatus::LostConnection);
				break;
			}
			UE_LOG(LogCook, Error, TEXT("CookWorkerClient received message %s from Director after receiving Abort message. Message will be ignored."),
			*Message.MessageType.ToString());
		}
		else
		{
			if (Message.MessageType == FAbortWorkerMessage::MessageType)
			{
				FAbortWorkerMessage AbortMessage;
				AbortMessage.TryRead(MoveTemp(Message.Object));
				if (AbortMessage.Type == FAbortWorkerMessage::EType::CookComplete)
				{
					UE_LOG(LogCook, Display, TEXT("CookWorkerClient received CookComplete message from Director. Flushing messages and shutting down."));
					SendToState(EConnectStatus::FlushAndAbortFirst);
				}
				else
				{
					UE_LOG(LogCook, Display, TEXT("CookWorkerClient received AbortWorker message from Director. Shutting down."));
					SendToState(EConnectStatus::LostConnection);
					break;
				}
			}
			else if (Message.MessageType == FInitialConfigMessage::MessageType)
			{
				UE_LOG(LogCook, Warning, TEXT("CookWorkerClient received unexpected repeat of InitialConfigMessage. Ignoring it."));
			}
			else if (Message.MessageType == FAssignPackagesMessage::MessageType)
			{
				FAssignPackagesMessage AssignPackagesMessage;
				if (!AssignPackagesMessage.TryRead(MoveTemp(Message.Object)))
				{
					LogInvalidMessage(TEXT("FAssignPackagesMessage"));
				}
				else
				{
					AssignPackages(AssignPackagesMessage);
				}
			}
		}
	}
}

void FCookWorkerClient::PumpDisconnect(FTickStackData& StackData)
{
	for (;;)
	{
		switch (ConnectStatus)
		{
		case EConnectStatus::FlushAndAbortFirst:
		{
			TickCollectors(StackData, true /* bFlush */);
			// Add code here for any waiting we need to do for the local CookOnTheFlyServer to gracefully shutdown
			SendMessage(FAbortWorkerMessage(FAbortWorkerMessage::EType::Abort));
			SendToState(EConnectStatus::WaitForAbortAcknowledge);
			break;
		}
		case EConnectStatus::WaitForAbortAcknowledge:
		{
			using namespace UE::CompactBinaryTCP;
			PumpReceiveMessages();
			if (ConnectStatus == EConnectStatus::WaitForAbortAcknowledge)
			{
				PumpSendMessages();

				constexpr float WaitForDisconnectTimeout = 60.f;
				if (FPlatformTime::Seconds() - ConnectStartTimeSeconds > WaitForDisconnectTimeout && !IsCookIgnoreTimeouts())
				{
					UE_LOG(LogCook, Warning, TEXT("Timedout after %.0fs waiting to send disconnect message to CookDirector."),
						WaitForDisconnectTimeout);
					SendToState(EConnectStatus::LostConnection);
					return;
				}
				return; // Exit the Pump loop for now and keep waiting
			}
			else
			{
				check(ConnectStatus == EConnectStatus::LostConnection);
				return;
			}
		}
		case EConnectStatus::LostConnection:
		{
			StackData.bCookCancelled = true;
			StackData.ResultFlags |= UCookOnTheFlyServer::COSR_YieldTick;
			return;
		}
		default:
			return;
		}
	}
}

void FCookWorkerClient::SendMessage(const UE::CompactBinaryTCP::IMessage& Message)
{
	UE::CompactBinaryTCP::TryWritePacket(ServerSocket, SendBuffer, Message);
}

void FCookWorkerClient::SendToState(EConnectStatus TargetStatus)
{
	switch (TargetStatus)
	{
	case EConnectStatus::FlushAndAbortFirst:
		ConnectStartTimeSeconds = FPlatformTime::Seconds();
		break;
	case EConnectStatus::LostConnection:
		Sockets::CloseSocket(ServerSocket);
		break;
	}
	ConnectStatus = TargetStatus;
}

void FCookWorkerClient::LogInvalidMessage(const TCHAR* MessageTypeName)
{
	UE_LOG(LogCook, Error, TEXT("CookWorkerClient received invalidly formatted message for type %s from CookDirector. Ignoring it."),
		MessageTypeName);
}

void FCookWorkerClient::AssignPackages(FAssignPackagesMessage& Message)
{
	for (FAssignPackageData& AssignData: Message.PackageDatas)
	{
		FPackageData& PackageData = COTFS.PackageDatas->FindOrAddPackageData(AssignData.ConstructData.PackageName,
			AssignData.ConstructData.NormalizedFileName);
		// If already InProgress, ignore the duplicate package silently
		if (PackageData.IsInProgress())
		{
			return;
		}

		// We do not want CookWorkers to explore dependencies in CookRequestCluster because the Director did it already.
		// Mmark the PackageDatas we get from the Director as already explored.
		for (const ITargetPlatform* TargetPlatform : OrderedSessionPlatforms)
		{
			FPackageData::FPlatformData& PlatformData = PackageData.FindOrAddPlatformData(TargetPlatform);
			PlatformData.bExplored = true;
		}

		PackageData.SetRequestData(OrderedSessionPlatforms , false /* bInIsUrgent */, FCompletionCallback(),
			FInstigator(AssignData.Instigator));
		PackageData.SendToState(EPackageState::Request, ESendFlags::QueueAddAndRemove);
	}
}

void FCookWorkerClient::Register(IMPCollector* Collector)
{
	CollectorsToTick.AddUnique(Collector);
}

void FCookWorkerClient::Unregister(IMPCollector* Collector)
{
	CollectorsToTick.RemoveSwap(Collector);
}

void FCookWorkerClient::TickCollectors(FTickStackData& StackData, bool bFlush)
{
	if (StackData.LoopStartTime < NextTickCollectorsTimeSeconds && !bFlush)
	{
		return;
	}

	if (!CollectorsToTick.IsEmpty())
	{
		IMPCollector::FClientContext Context;
		Context.Platforms = OrderedSessionPlatforms;
		Context.bFlush = bFlush;
		TArray<UE::CompactBinaryTCP::FMarshalledMessage> MarshalledMessages;

		for (IMPCollector* Collector : CollectorsToTick)
		{
			Collector->ClientTick(Context);
			if (!Context.Messages.IsEmpty())
			{
				FGuid MessageType = Collector->GetMessageType();
				for (FCbObject& Object : Context.Messages)
				{
					MarshalledMessages.Add({ MessageType, MoveTemp(Object) });
				}
				Context.Messages.Reset();
			}
		}

		if (!MarshalledMessages.IsEmpty())
		{
			UE::CompactBinaryTCP::TryWritePacket(ServerSocket, SendBuffer, MoveTemp(MarshalledMessages));
		}
	}

	constexpr float TickCollectorsPeriodSeconds = 10.f;
	NextTickCollectorsTimeSeconds = FPlatformTime::Seconds() + TickCollectorsPeriodSeconds;
}

}