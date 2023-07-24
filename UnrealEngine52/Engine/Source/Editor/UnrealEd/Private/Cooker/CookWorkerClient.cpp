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
#include "PackageTracker.h"
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
	Register(new IMPCollectorCbClientMessage<FRetractionRequestMessage>([this]
	(FMPCollectorClientMessageContext& Context, bool bReadSuccessful, FRetractionRequestMessage&& Message)
		{
			HandleRetractionMessage(Context, bReadSuccessful, MoveTemp(Message));
		}, TEXT("HandleRetractionMessage")));
	Register(new IMPCollectorCbClientMessage<FAbortPackagesMessage>([this]
	(FMPCollectorClientMessageContext& Context, bool bReadSuccessful, FAbortPackagesMessage&& Message)
		{
			HandleAbortPackagesMessage(Context, bReadSuccessful, MoveTemp(Message));
		}, TEXT("HandleAbortPackagesMessage")));
	Register(new IMPCollectorCbClientMessage<FHeartbeatMessage>([this]
	(FMPCollectorClientMessageContext& Context, bool bReadSuccessful, FHeartbeatMessage&& Message)
		{
			HandleHeartbeatMessage(Context, bReadSuccessful, MoveTemp(Message));
		}, TEXT("HandleHeartbeatMessage")));
	Register(new FAssetRegistryMPCollector(COTFS));
	Register(new FPackageWriterMPCollector(COTFS));
}

FCookWorkerClient::~FCookWorkerClient()
{
	if (ConnectStatus == EConnectStatus::Connected ||
		(EConnectStatus::FlushAndAbortFirst <= ConnectStatus && ConnectStatus <= EConnectStatus::FlushAndAbortLast))
	{
		UE_LOG(LogCook, Warning, TEXT("CookWorker was destroyed before it finished Disconnect. The CookDirector may be missing some information."));
	}
	Sockets::CloseSocket(ServerSocket);

	// Before destructing, wait on all of the Futures that could have async access to *this from a TaskThread
	TArray<FPendingResultNeedingAsyncWork> LocalPendingResultsNeedingAsyncWork;
	{
		FScopeLock PendingResultsScopeLock(&PendingResultsLock);
		for (TPair<FPackageRemoteResult*, FPendingResultNeedingAsyncWork>& Pair : PendingResultsNeedingAsyncWork)
		{
			LocalPendingResultsNeedingAsyncWork.Add(MoveTemp(Pair.Value));
		}
		PendingResultsNeedingAsyncWork.Empty();
	}
	for (FPendingResultNeedingAsyncWork& PendingResult : LocalPendingResultsNeedingAsyncWork)
	{
		PendingResult.CompletionFuture.Get();
	}
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

bool FCookWorkerClient::GetInitializationIsZenStore()
{
	check(InitialConfigMessage); // Should only be called after TryConnect and before DoneWithInitialSettings
	return InitialConfigMessage->IsZenStore();
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
	if (Reason == ESuppressCookReason::RetractedByCookDirector)
	{
		return;
	}
	TUniquePtr<FPackageRemoteResult> ResultOwner(new FPackageRemoteResult());
	FName PackageName = PackageData.GetPackageName();
	ResultOwner->SetPackageName(PackageName);
	ResultOwner->SetSuppressCookReason(Reason);
	// Set the platforms, use the default values for each platform (e.g. bSuccessful=false)
	ResultOwner->SetPlatforms(OrderedSessionPlatforms);

	ReportPackageMessage(PackageName, MoveTemp(ResultOwner));
}

void FCookWorkerClient::ReportPromoteToSaveComplete(FPackageData& PackageData)
{
	TUniquePtr<FPackageRemoteResult> ResultOwner(new FPackageRemoteResult());
	FPackageRemoteResult* Result = ResultOwner.Get();

	FName PackageName = PackageData.GetPackageName();
	Result->SetPackageName(PackageName);
	Result->SetSuppressCookReason(ESuppressCookReason::InvalidSuppressCookReason);
	Result->SetPlatforms(OrderedSessionPlatforms);

	int32 NumPlatforms = OrderedSessionPlatforms.Num();
	for (int32 PlatformIndex = 0; PlatformIndex < NumPlatforms; ++PlatformIndex)
	{
		ITargetPlatform* TargetPlatform = OrderedSessionPlatforms[PlatformIndex];
		FPackageRemoteResult::FPlatformResult& PlatformResults = Result->GetPlatforms()[PlatformIndex];
		FPackageData::FPlatformData& PackagePlatformData = PackageData.FindOrAddPlatformData(TargetPlatform);
		PlatformResults.SetSuccessful(PackagePlatformData.bCookSucceeded);
	}

	ReportPackageMessage(PackageName, MoveTemp(ResultOwner));
}

void FCookWorkerClient::ReportPackageMessage(FName PackageName, TUniquePtr<FPackageRemoteResult>&& ResultOwner)
{
	FPackageRemoteResult* Result = ResultOwner.Get();

	TArray<FMPCollectorClientTickPackageContext::FPlatformData, TInlineAllocator<1>> ContextPlatformDatas;
	ContextPlatformDatas.Reserve(Result->GetPlatforms().Num());
	for (FPackageRemoteResult::FPlatformResult& PlatformResult : Result->GetPlatforms())
	{
		ContextPlatformDatas.Add(FMPCollectorClientTickPackageContext::FPlatformData
			{ PlatformResult.GetPlatform(), PlatformResult.IsSuccessful() });
	}
	FMPCollectorClientTickPackageContext Context;
	Context.PackageName = PackageName;
	Context.Platforms = OrderedSessionPlatforms;
	Context.PlatformDatas = ContextPlatformDatas;

	for (const TPair<FGuid, TRefCountPtr<IMPCollector>>& CollectorPair : Collectors)
	{
		IMPCollector* Collector = CollectorPair.Value.GetReference();
		Collector->ClientTickPackage(Context);
		const FGuid& MessageType = CollectorPair.Key;
		for (TPair<const ITargetPlatform*, FCbObject>& MessagePair : Context.Messages)
		{
			const ITargetPlatform* TargetPlatform = MessagePair.Key;
			FCbObject Object = MoveTemp(MessagePair.Value);
			if (!TargetPlatform)
			{
				Result->AddPackageMessage(MessageType, MoveTemp(Object));
			}
			else
			{
				Result->AddPlatformMessage(TargetPlatform, MessageType, MoveTemp(Object));
			}
		}
		Context.Messages.Reset();
		for (TPair<const ITargetPlatform*, TFuture<FCbObject>>& MessagePair : Context.AsyncMessages)
		{
			const ITargetPlatform* TargetPlatform = MessagePair.Key;
			TFuture<FCbObject> ObjectFuture = MoveTemp(MessagePair.Value);
			if (!TargetPlatform)
			{
				Result->AddAsyncPackageMessage(MessageType, MoveTemp(ObjectFuture));
			}
			else
			{
				Result->AddAsyncPlatformMessage(TargetPlatform, MessageType, MoveTemp(ObjectFuture));
			}
		}
		Context.AsyncMessages.Reset();
	}

	++(Result->GetUserRefCount()); // Used to test whether the async Future still needs to access *this
	TFuture<void> CompletionFuture = Result->GetCompletionFuture().Then(
	[this, Result](TFuture<int>&& OldFuture)
	{
		FScopeLock PendingResultsScopeLock(&PendingResultsLock);
		FPendingResultNeedingAsyncWork PendingResult;
		PendingResultsNeedingAsyncWork.RemoveAndCopyValue(Result, PendingResult);

		// Result might have not been added into PendingResultsNeedingAsyncWork yet, and also could have
		// been removed by cancellation from e.g. CookWorkerClient destructor.
		if (PendingResult.PendingResult)
		{
			PendingResults.Add(MoveTemp(PendingResult.PendingResult));
		}
		--(Result->GetUserRefCount());
	});

	{
		FScopeLock PendingResultsScopeLock(&PendingResultsLock);
		if (Result->GetUserRefCount() == 0)
		{
			// Result->GetCompletionFuture() has already been called
			check(Result->IsComplete());
			PendingResults.Add(MoveTemp(ResultOwner));
		}
		else
		{
			PendingResultsNeedingAsyncWork.Add(Result,
				FPendingResultNeedingAsyncWork{ MoveTemp(ResultOwner), MoveTemp(CompletionFuture) });
		}
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
	if (!InitialConfigMessage->TryRead(Messages[0].Object))
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
	FPackageResultsMessage Message;
	{
		FScopeLock PendingResultsScopeLock(&PendingResultsLock);
		if (!PendingResults.IsEmpty())
		{
			Message.Results.Reserve(PendingResults.Num());
			for (TUniquePtr<FPackageRemoteResult>& Result : PendingResults)
			{
				Message.Results.Add(MoveTemp(*Result));
			}
			PendingResults.Reset();
		}
	}
	if (!Message.Results.IsEmpty())
	{
		SendMessage(Message);
	}

	if (!PendingDiscoveredPackages.IsEmpty())
	{
		FDiscoveredPackagesMessage DiscoveredMessage;
		DiscoveredMessage.Packages = MoveTemp(PendingDiscoveredPackages);
		SendMessage(DiscoveredMessage);
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
				AbortMessage.TryRead(Message.Object);
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
				if (!AssignPackagesMessage.TryRead(Message.Object))
				{
					LogInvalidMessage(TEXT("FAssignPackagesMessage"));
				}
				else
				{
					AssignPackages(AssignPackagesMessage);
				}
			}
			else
			{
				TRefCountPtr<IMPCollector>* Collector = Collectors.Find(Message.MessageType);
				if (Collector)
				{
					check(*Collector);
					FMPCollectorClientMessageContext Context;
					Context.Platforms = OrderedSessionPlatforms;
					(*Collector)->ClientReceiveMessage(Context, Message.Object);
				}
				else
				{
					UE_LOG(LogCook, Error, TEXT("CookWorkerClient received message of unknown type %s from CookDirector. Ignoring it."),
						*Message.MessageType.ToString());
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
			COTFS.CookAsCookWorkerFinished();
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
					check(ConnectStatus == EConnectStatus::LostConnection);
					// Fall through to LostConnection
					break;
				}
				else
				{
					return; // Exit the Pump loop for now and keep waiting
				}
			}
			else
			{
				check(ConnectStatus == EConnectStatus::LostConnection);
				// Fall through to LostConnection
				break;
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

	if (!Message.PackageDatas.IsEmpty())
	{
		// Clear the SoftGC diagnostic ExpectedNeverLoadPackages because we have new assigned packages
		// that we didn't consider during SoftGC
		COTFS.PackageTracker->ClearExpectedNeverLoadPackages();
	}
}

void FCookWorkerClient::Register(IMPCollector* Collector)
{
	TRefCountPtr<IMPCollector>& Existing = Collectors.FindOrAdd(Collector->GetMessageType());
	if (Existing)
	{
		UE_LOG(LogCook, Error, TEXT("Duplicate IMPCollectors registered. Guid: %s, Existing: %s, Registering: %s. Keeping the Existing."),
			*Collector->GetMessageType().ToString(), Existing->GetDebugName(), Collector->GetDebugName());
		return;
	}
	Existing = Collector;
}

void FCookWorkerClient::Unregister(IMPCollector* Collector)
{
	TRefCountPtr<IMPCollector> Existing;
	Collectors.RemoveAndCopyValue(Collector->GetMessageType(), Existing);
	if (Existing && Existing.GetReference() != Collector)
	{
		UE_LOG(LogCook, Error, TEXT("Duplicate IMPCollector during Unregister. Guid: %s, Existing: %s, Unregistering: %s. Ignoring the Unregister."),
			*Collector->GetMessageType().ToString(), Existing->GetDebugName(), Collector->GetDebugName());
		Collectors.Add(Collector->GetMessageType(), MoveTemp(Existing));
	}
}

void FCookWorkerClient::TickCollectors(FTickStackData& StackData, bool bFlush)
{
	if (StackData.LoopStartTime < NextTickCollectorsTimeSeconds && !bFlush)
	{
		return;
	}

	if (!Collectors.IsEmpty())
	{
		FMPCollectorClientTickContext Context;
		Context.Platforms = OrderedSessionPlatforms;
		Context.bFlush = bFlush;
		TArray<UE::CompactBinaryTCP::FMarshalledMessage> MarshalledMessages;

		for (const TPair<FGuid, TRefCountPtr<IMPCollector>>& Pair: Collectors)
		{
			IMPCollector* Collector = Pair.Value.GetReference();
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

void FCookWorkerClient::HandleAbortPackagesMessage(FMPCollectorClientMessageContext& Context, bool bReadSuccessful,
	FAbortPackagesMessage&& Message)
{
	if (!bReadSuccessful)
	{
		LogInvalidMessage(TEXT("AbortPackagesMessage"));
	}
	for (FName PackageName : Message.PackageNames)
	{
		FPackageData* PackageData = COTFS.PackageDatas->FindPackageDataByPackageName(PackageName);
		if (PackageData)
		{
			COTFS.DemoteToIdle(*PackageData, ESendFlags::QueueAddAndRemove, ESuppressCookReason::RetractedByCookDirector);
		}
	}
}

void FCookWorkerClient::HandleRetractionMessage(FMPCollectorClientMessageContext& Context, bool bReadSuccessful,
	FRetractionRequestMessage&& Message)
{
	if (!bReadSuccessful)
	{
		LogInvalidMessage(TEXT("RetractionRequestMessage"));
		return;
	}

	TArray<FName> PackageNames;
	COTFS.GetPackagesToRetract(Message.RequestedCount, PackageNames);
	for (FName PackageName : PackageNames)
	{
		FPackageData* PackageData = COTFS.PackageDatas->FindPackageDataByPackageName(PackageName);
		check(PackageData);
		COTFS.DemoteToIdle(*PackageData, ESendFlags::QueueAddAndRemove, ESuppressCookReason::RetractedByCookDirector);
	}

	FRetractionResultsMessage ResultsMessage;
	ResultsMessage.ReturnedPackages = MoveTemp(PackageNames);
	SendMessage(ResultsMessage);
}

void FCookWorkerClient::HandleHeartbeatMessage(FMPCollectorClientMessageContext& Context, bool bReadSuccessful,
	FHeartbeatMessage&& Message)
{
	if (!bReadSuccessful)
	{
		LogInvalidMessage(TEXT("HeartbeatMessage"));
		return;
	}

	SendMessage(FHeartbeatMessage(Message.HeartbeatNumber));
}

} // namespace UE::Cook