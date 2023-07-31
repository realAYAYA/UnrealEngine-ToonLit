// Copyright Epic Games, Inc. All Rights Reserved.

#include "CookWorkerServer.h"

#include "Commandlets/AssetRegistryGenerator.h"
#include "CompactBinaryTCP.h"
#include "CookDirector.h"
#include "CookMPCollector.h"
#include "CookPackageData.h"
#include "CookPlatformManager.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "Math/NumericLimits.h"
#include "Misc/AssertionMacros.h"
#include "Misc/ScopeLock.h"
#include "PackageResultsMessage.h"
#include "PackageTracker.h"
#include "UnrealEdMisc.h"

namespace UE::Cook
{

FCookWorkerServer::FCookWorkerServer(FCookDirector& InDirector, FWorkerId InWorkerId)
	: Director(InDirector)
	, COTFS(InDirector.COTFS)
	, WorkerId(InWorkerId)
{
}

FCookWorkerServer::~FCookWorkerServer()
{
	FCommunicationScopeLock ScopeLock(this, ECookDirectorThread::CommunicateThread, ETickAction::Queue);

	checkf(PendingPackages.IsEmpty() && PackagesToAssign.IsEmpty(),
		TEXT("CookWorkerServer still has assigned packages when it is being destroyed; we will leak them and block the cook."));

	if (ConnectStatus == EConnectStatus::Connected || ConnectStatus == EConnectStatus::PumpingCookComplete || ConnectStatus == EConnectStatus::WaitForDisconnect)
	{
		UE_LOG(LogCook, Error, TEXT("CookWorkerServer %d was destroyed before it finished Disconnect. The remote process may linger and may interfere with writes of future packages."),
			WorkerId.GetRemoteIndex());
	}
	DetachFromRemoteProcess();
}

void FCookWorkerServer::DetachFromRemoteProcess()
{
	Sockets::CloseSocket(Socket);
	CookWorkerHandle = FProcHandle();
	CookWorkerProcessId = 0;
	bTerminateImmediately = false;
	SendBuffer.Reset();
	ReceiveBuffer.Reset();
}

void FCookWorkerServer::ShutdownRemoteProcess()
{
	Sockets::CloseSocket(Socket);
	if (CookWorkerHandle.IsValid())
	{
		FPlatformProcess::TerminateProc(CookWorkerHandle, /* bKillTree */true);
	}
	DetachFromRemoteProcess();
}

void FCookWorkerServer::AppendAssignments(TArrayView<FPackageData*> Assignments, ECookDirectorThread TickThread)
{
	FCommunicationScopeLock ScopeLock(this, TickThread, ETickAction::Queue);
	PackagesToAssign.Append(Assignments);
}

void FCookWorkerServer::AbortAssignments(TSet<FPackageData*>& OutPendingPackages, ECookDirectorThread TickThread)
{
	FCommunicationScopeLock ScopeLock(this, TickThread, ETickAction::Queue);
	AbortAssignmentsInLock(OutPendingPackages);
}

void FCookWorkerServer::AbortAssignmentsInLock(TSet<FPackageData*>& OutPendingPackages)
{
	if (PendingPackages.Num())
	{
		if (ConnectStatus == EConnectStatus::Connected)
		{
			TArray<FName> PackageNames;
			PackageNames.Reserve(PendingPackages.Num());
			for (FPackageData* PackageData : PendingPackages)
			{
				PackageNames.Add(PackageData->GetPackageName());
			}
			SendMessage(FAbortPackagesMessage(MoveTemp(PackageNames)));
		}
		OutPendingPackages.Append(MoveTemp(PendingPackages));
		PendingPackages.Empty();
	}
	OutPendingPackages.Append(PackagesToAssign);
	PackagesToAssign.Empty();
}

void FCookWorkerServer::AbortAssignment(FPackageData& PackageData, ECookDirectorThread TickThread)
{
	FCommunicationScopeLock ScopeLock(this, TickThread, ETickAction::Queue);

	if (PendingPackages.Remove(&PackageData))
	{
		if (ConnectStatus == EConnectStatus::Connected)
		{
			TArray<FName> PackageNames;
			PackageNames.Add(PackageData.GetPackageName());
			SendMessage(FAbortPackagesMessage(MoveTemp(PackageNames)));
		}
	}

	PackagesToAssign.Remove(&PackageData);
}

void FCookWorkerServer::AbortWorker(TSet<FPackageData*>& OutPendingPackages, ECookDirectorThread TickThread)
{
	FCommunicationScopeLock ScopeLock(this, TickThread, ETickAction::Tick);

	AbortAssignmentsInLock(OutPendingPackages);
	switch (ConnectStatus)
	{
	case EConnectStatus::Uninitialized: // Fall through
	case EConnectStatus::WaitForConnect:
		SendToState(EConnectStatus::LostConnection);
		break;
	case EConnectStatus::Connected: // Fall through
	case EConnectStatus::PumpingCookComplete:
	{
		SendMessage(FAbortWorkerMessage(FAbortWorkerMessage::EType::Abort));
		SendToState(EConnectStatus::WaitForDisconnect);
		break;
	}
	default:
		break;
	}
}

void FCookWorkerServer::SendToState(EConnectStatus TargetStatus)
{
	switch (TargetStatus)
	{
	case EConnectStatus::WaitForConnect:
		ConnectStartTimeSeconds = FPlatformTime::Seconds();
		ConnectTestStartTimeSeconds = ConnectStartTimeSeconds;
		break;
	case EConnectStatus::WaitForDisconnect:
		ConnectStartTimeSeconds = FPlatformTime::Seconds();
		ConnectTestStartTimeSeconds = ConnectStartTimeSeconds;
		break;
	case EConnectStatus::PumpingCookComplete:
		ConnectStartTimeSeconds = FPlatformTime::Seconds();
		ConnectTestStartTimeSeconds = ConnectStartTimeSeconds;
		break;
	case EConnectStatus::LostConnection:
		DetachFromRemoteProcess();
		break;
	default:
		break;
	}
	ConnectStatus = TargetStatus;
}

bool FCookWorkerServer::IsShuttingDown() const
{
	FScopeLock CommunicationScopeLock(&CommunicationLock);
	return ConnectStatus == EConnectStatus::PumpingCookComplete || ConnectStatus == EConnectStatus::WaitForDisconnect || ConnectStatus == EConnectStatus::LostConnection;
}

bool FCookWorkerServer::IsFlushingBeforeShutdown() const
{
	FScopeLock CommunicationScopeLock(&CommunicationLock);
	return ConnectStatus == EConnectStatus::PumpingCookComplete;
}

bool FCookWorkerServer::IsShutdownComplete() const
{
	FScopeLock CommunicationScopeLock(&CommunicationLock);
	return ConnectStatus == EConnectStatus::LostConnection;
}

bool FCookWorkerServer::HasAssignments() const
{
	FScopeLock CommunicationScopeLock(&CommunicationLock);
	return !PackagesToAssign.IsEmpty() || !PendingPackages.IsEmpty();
}

bool FCookWorkerServer::HasMessages() const
{
	FScopeLock CommunicationScopeLock(&CommunicationLock);
	return !ReceiveMessages.IsEmpty();
}

bool FCookWorkerServer::TryHandleConnectMessage(FWorkerConnectMessage& Message, FSocket* InSocket, TArray<UE::CompactBinaryTCP::FMarshalledMessage>&& OtherPacketMessages, ECookDirectorThread TickThread)
{
	FCommunicationScopeLock ScopeLock(this, TickThread, ETickAction::Tick);

	if (ConnectStatus != EConnectStatus::WaitForConnect)
	{
		return false;
	}
	check(!Socket);
	Socket = InSocket;

	SendToState(EConnectStatus::Connected);
	for (UE::CompactBinaryTCP::FMarshalledMessage& OtherMessage : OtherPacketMessages)
	{
		ReceiveMessages.Add(MoveTemp(OtherMessage));
	}
	HandleReceiveMessagesInternal();
	const FInitialConfigMessage& InitialConfigMessage = Director.GetInitialConfigMessage();
	OrderedSessionPlatforms = InitialConfigMessage.GetOrderedSessionPlatforms();
	SendMessage(InitialConfigMessage);
	return true;
}

void FCookWorkerServer::TickCommunication(ECookDirectorThread TickThread)
{
	FCommunicationScopeLock ScopeLock(this, TickThread, ETickAction::Tick);

	for (;;)
	{
		switch (ConnectStatus)
		{
		case EConnectStatus::Uninitialized:
			LaunchProcess();
			break;
		case EConnectStatus::WaitForConnect:
			TickWaitForConnect();
			if (ConnectStatus == EConnectStatus::WaitForConnect)
			{
				return; // Try again later
			}
			break;
		case EConnectStatus::Connected:
			PumpReceiveMessages();
			if (ConnectStatus == EConnectStatus::Connected)
			{
				SendPendingPackages();
				PumpSendMessages();
				return; // Tick duties complete; yield the tick
			}
			break;
		case EConnectStatus::PumpingCookComplete:
		{
			PumpReceiveMessages();
			if (ConnectStatus == EConnectStatus::PumpingCookComplete)
			{
				PumpSendMessages();
				constexpr float WaitForPumpCompleteTimeout = 10.f * 60;
				if (FPlatformTime::Seconds() - ConnectStartTimeSeconds <= WaitForPumpCompleteTimeout || IsCookIgnoreTimeouts())
				{
					return; // Try again later
				}
				UE_LOG(LogCook, Error, TEXT("CookWorker process of CookWorkerServer %d failed to finalize its cook within %.0f seconds; we will tell it to shutdown."),
					WorkerId.GetRemoteIndex(), WaitForPumpCompleteTimeout);
				SendMessage(FAbortWorkerMessage(FAbortWorkerMessage::EType::Abort));
				SendToState(EConnectStatus::WaitForDisconnect);
			}
			break;
		}
		case EConnectStatus::WaitForDisconnect:
			TickWaitForDisconnect();
			if (ConnectStatus == EConnectStatus::WaitForDisconnect)
			{
				return; // Try again later
			}
			break;
		case EConnectStatus::LostConnection:
			return; // Nothing further to do
		default:
			checkNoEntry();
			return;
		}
	}
}

void FCookWorkerServer::SignalCookComplete(ECookDirectorThread TickThread)
{
	FCommunicationScopeLock ScopeLock(this, TickThread, ETickAction::Tick);

	switch (ConnectStatus)
	{
	case EConnectStatus::Uninitialized: // Fall through
	case EConnectStatus::WaitForConnect:
		SendToState(EConnectStatus::LostConnection);
		break;
	case EConnectStatus::Connected:
		SendMessage(FAbortWorkerMessage(FAbortWorkerMessage::EType::CookComplete));
		SendToState(EConnectStatus::PumpingCookComplete);
		break;
	default:
		break; // Already in a disconnecting state
	}
}

void FCookWorkerServer::LaunchProcess()
{
	FCookDirector::FLaunchInfo LaunchInfo = Director.GetLaunchInfo(WorkerId);
	bool bShowCookWorkers = LaunchInfo.ShowWorkerOption == FCookDirector::EShowWorker::SeparateWindows;

	CookWorkerHandle = FPlatformProcess::CreateProc(*LaunchInfo.CommandletExecutable, *LaunchInfo.WorkerCommandLine,
		true /* bLaunchDetached */, !bShowCookWorkers /* bLaunchHidden */, !bShowCookWorkers /* bLaunchReallyHidden */,
		&CookWorkerProcessId, 0 /* PriorityModifier */, *FPaths::GetPath(LaunchInfo.CommandletExecutable),
		nullptr /* PipeWriteChild */);
	if (CookWorkerHandle.IsValid())
	{
		UE_LOG(LogCook, Display, TEXT("CookWorkerServer %d launched CookWorker as PID %u with commandline \"%s\"."),
			WorkerId.GetRemoteIndex(), CookWorkerProcessId, *LaunchInfo.WorkerCommandLine);
		SendToState(EConnectStatus::WaitForConnect);
	}
	else
	{
		// GetLastError information was logged by CreateProc
		UE_LOG(LogCook, Error, TEXT("CookWorkerServer %d failed to create CookWorker process. Assigned packages will be returned to the director."),
			WorkerId.GetRemoteIndex());
		SendToState(EConnectStatus::LostConnection);
	}
}

void FCookWorkerServer::TickWaitForConnect()
{
	constexpr float TestProcessExistencePeriod = 1.f;
	constexpr float WaitForConnectTimeout = 60.f * 10;

	check(!Socket); // When the Socket is assigned we leave the WaitForConnect state, and we set it to null before entering

	double CurrentTime = FPlatformTime::Seconds();
	if (CurrentTime - ConnectTestStartTimeSeconds > TestProcessExistencePeriod)
	{
		if (!FPlatformProcess::IsProcRunning(CookWorkerHandle))
		{
			UE_LOG(LogCook, Error, TEXT("CookWorkerServer %d process terminated before connecting. Assigned packages will be returned to the director."),
				WorkerId.GetRemoteIndex());
			SendToState(EConnectStatus::LostConnection);
			return;
		}
		ConnectTestStartTimeSeconds = FPlatformTime::Seconds();
	}

	if (CurrentTime - ConnectStartTimeSeconds > WaitForConnectTimeout && !IsCookIgnoreTimeouts())
	{
		UE_LOG(LogCook, Error, TEXT("CookWorkerServer %d process failed to connect within %.0f seconds. Assigned packages will be returned to the director."),
			WorkerId.GetRemoteIndex(), WaitForConnectTimeout);
		ShutdownRemoteProcess();
		SendToState(EConnectStatus::LostConnection);
		return;
	}
}

void FCookWorkerServer::TickWaitForDisconnect()
{
	constexpr float TestProcessExistencePeriod = 1.f;
	constexpr float WaitForDisconnectTimeout = 60.f * 10;

	double CurrentTime = FPlatformTime::Seconds();
	if (CurrentTime - ConnectTestStartTimeSeconds > TestProcessExistencePeriod)
	{
		if (!FPlatformProcess::IsProcRunning(CookWorkerHandle))
		{
			SendToState(EConnectStatus::LostConnection);
			return;
		}
		ConnectTestStartTimeSeconds = FPlatformTime::Seconds();
	}

	// We might have been blocked from sending the disconnect, so keep trying to flush the buffer
	UE::CompactBinaryTCP::TryFlushBuffer(Socket, SendBuffer);
	TArray<UE::CompactBinaryTCP::FMarshalledMessage> Messages;
	TryReadPacket(Socket, ReceiveBuffer, Messages);

	if (bTerminateImmediately || (CurrentTime - ConnectStartTimeSeconds > WaitForDisconnectTimeout && !IsCookIgnoreTimeouts()))
	{
		UE_CLOG(!bTerminateImmediately, LogCook, Warning,
			TEXT("CookWorker process of CookWorkerServer %d failed to disconnect within %.0f seconds; we will terminate it."),
			WorkerId.GetRemoteIndex(), WaitForDisconnectTimeout);
		ShutdownRemoteProcess();
		SendToState(EConnectStatus::LostConnection);
	}
}

void FCookWorkerServer::PumpSendMessages()
{
	UE::CompactBinaryTCP::EConnectionStatus Status = UE::CompactBinaryTCP::TryFlushBuffer(Socket, SendBuffer);
	if (Status == UE::CompactBinaryTCP::Failed)
	{
		UE_LOG(LogCook, Error, TEXT("CookWorkerServer %d failed to write to socket, we will shutdown the remote process. Assigned packages will be returned to the director."),
			WorkerId.GetRemoteIndex());
		SendToState(EConnectStatus::WaitForDisconnect);
		bTerminateImmediately = true;
	}
}

void FCookWorkerServer::SendPendingPackages()
{
	if (PackagesToAssign.IsEmpty())
	{
		return;
	}

	TArray<FAssignPackageData> AssignDatas;
	AssignDatas.Reserve(PackagesToAssign.Num());
	for (FPackageData* PackageData : PackagesToAssign)
	{
		FAssignPackageData& AssignData = AssignDatas.Emplace_GetRef();
		AssignData.ConstructData = PackageData->CreateConstructData();
		AssignData.Instigator = PackageData->GetInstigator();
	}
	PendingPackages.Append(PackagesToAssign);
	PackagesToAssign.Empty();
	SendMessage(FAssignPackagesMessage(MoveTemp(AssignDatas)));
}

void FCookWorkerServer::PumpReceiveMessages()
{
	using namespace UE::CompactBinaryTCP;
	TArray<FMarshalledMessage> Messages;
	EConnectionStatus SocketStatus = TryReadPacket(Socket, ReceiveBuffer, Messages);
	if (SocketStatus != EConnectionStatus::Okay && SocketStatus != EConnectionStatus::Incomplete)
	{
		UE_LOG(LogCook, Error, TEXT("CookWorkerServer %d failed to read from socket, we will shutdown the remote process. Assigned packages will be returned to the director."),
			WorkerId.GetRemoteIndex());
		SendToState(EConnectStatus::WaitForDisconnect);
		bTerminateImmediately = true;
		return;
	}
	for (FMarshalledMessage& Message : Messages)
	{
		ReceiveMessages.Add(MoveTemp(Message));
	}
	HandleReceiveMessagesInternal();
}

void FCookWorkerServer::HandleReceiveMessages(ECookDirectorThread TickThread)
{
	FCommunicationScopeLock ScopeLock(this, TickThread, ETickAction::Queue);
	HandleReceiveMessagesInternal();
}

void FCookWorkerServer::HandleReceiveMessagesInternal()
{
	while (!ReceiveMessages.IsEmpty())
	{
		UE::CompactBinaryTCP::FMarshalledMessage& PeekMessage = ReceiveMessages[0];

		if (PeekMessage.MessageType == FAbortWorkerMessage::MessageType)
		{
			UE::CompactBinaryTCP::FMarshalledMessage Message = ReceiveMessages.PopFrontValue();
			UE_CLOG(ConnectStatus != EConnectStatus::PumpingCookComplete && ConnectStatus != EConnectStatus::WaitForDisconnect,
				LogCook, Error, TEXT("CookWorkerServer %d remote process shut down unexpectedly. Assigned packages will be returned to the director."),
				WorkerId.GetRemoteIndex());

			SendMessage(FAbortWorkerMessage(FAbortWorkerMessage::AbortAcknowledge));
			SendToState(EConnectStatus::WaitForDisconnect);
			ReceiveMessages.Reset();
			break;
		}

		if (TickState.TickThread != ECookDirectorThread::SchedulerThread)
		{
			break;
		}

		UE::CompactBinaryTCP::FMarshalledMessage Message = ReceiveMessages.PopFrontValue();
		if (Message.MessageType == FPackageResultsMessage::MessageType)
		{
			FPackageResultsMessage ResultsMessage;
			if (!ResultsMessage.TryRead(MoveTemp(Message.Object)))
			{
				LogInvalidMessage(TEXT("FPackageResultsMessage"));
			}
			else
			{
				RecordResults(ResultsMessage);
			}
		}
		else if (Message.MessageType == FDiscoveredPackagesMessage::MessageType)
		{
			FDiscoveredPackagesMessage DiscoveredMessage;
			if (!DiscoveredMessage.TryRead(MoveTemp(Message.Object)))
			{
				LogInvalidMessage(TEXT("FDiscoveredPackagesMessage"));
			}
			else
			{
				for (FDiscoveredPackage& DiscoveredPackage : DiscoveredMessage.Packages)
				{
					AddDiscoveredPackage(MoveTemp(DiscoveredPackage));
				}
			}
		}
		else
		{
			TRefCountPtr<IMPCollector>* Collector = Director.MessageHandlers.Find(Message.MessageType);
			if (Collector)
			{
				check(*Collector);
				IMPCollector::FServerContext Context;
				Context.Platforms = OrderedSessionPlatforms;
				Context.WorkerId = WorkerId;
				(*Collector)->ReceiveMessage(Context, Message.Object);
			}
			else
			{
				UE_LOG(LogCook, Error, TEXT("CookWorkerServer received message of unknown type %s from CookWorker. Ignoring it."),
					*Message.MessageType.ToString());
			}
		}
	}
}

void FCookWorkerServer::HandleReceivedPackagePlatformMessages(FPackageData& PackageData, const ITargetPlatform* TargetPlatform, TArray<UE::CompactBinaryTCP::FMarshalledMessage>&& Messages)
{
	check(TickState.TickThread == ECookDirectorThread::SchedulerThread);

	for (UE::CompactBinaryTCP::FMarshalledMessage& Message : Messages)
	{
		if (Message.MessageType == FAssetRegistryPackageMessage::MessageType)
		{
			FAssetRegistryPackageMessage ARMessage;
			if (!ARMessage.TryRead(MoveTemp(Message.Object), PackageData, TargetPlatform))
			{
				LogInvalidMessage(TEXT("FAssetRegistryPackageMessage"));
			}
			else
			{
				FAssetRegistryGenerator* RegistryGenerator = COTFS.PlatformManager->GetPlatformData(TargetPlatform)->RegistryGenerator.Get();
				check(RegistryGenerator); // The TargetPlatform came from OrderedSessionPlatforms, and the RegistryGenerator should exist for any of those platforms
				RegistryGenerator->UpdateAssetRegistryData(PackageData, MoveTemp(ARMessage));
			}
		}
	}
}

void FCookWorkerServer::SendMessage(const UE::CompactBinaryTCP::IMessage& Message)
{
	if (TickState.TickAction == ETickAction::Tick)
	{
		UE::CompactBinaryTCP::TryWritePacket(Socket, SendBuffer, Message);
	}
	else
	{
		check(TickState.TickAction == ETickAction::Queue);
		UE::CompactBinaryTCP::QueueMessage(SendBuffer, Message);
	}
}

void FCookWorkerServer::RecordResults(FPackageResultsMessage& Message)
{
	check(TickState.TickThread == ECookDirectorThread::SchedulerThread);

	for (FPackageRemoteResult& Result : Message.Results)
	{
		FPackageData* PackageData = COTFS.PackageDatas->FindPackageDataByPackageName(Result.PackageName);
		if (!PackageData)
		{
			UE_LOG(LogCook, Warning, TEXT("CookWorkerServer %d received FPackageResultsMessage for invalid package %s. Ignoring it."),
				WorkerId.GetRemoteIndex() , *Result.PackageName.ToString());
			continue;
		}
		if (PendingPackages.Remove(PackageData) != 1)
		{
			UE_LOG(LogCook, Warning, TEXT("CookWorkerServer %d received FPackageResultsMessage for package %s which is not a pending package. Ignoring it."),
				WorkerId.GetRemoteIndex() , *Result.PackageName.ToString());
			continue;
		}
		PackageData->SetWorkerAssignment(FWorkerId::Invalid());

		// MPCOOKTODO: Refactor FSaveCookedPackageContext::FinishPlatform and ::FinishPackage so we can call them from here
		// to reduce duplication
		if (Result.SuppressCookReason == ESuppressCookReason::InvalidSuppressCookReason)
		{
			int32 NumPlatforms = OrderedSessionPlatforms.Num();
			if (Result.Platforms.Num() != NumPlatforms)
			{
				UE_LOG(LogCook, Warning, TEXT("CookWorkerServer %d received FPackageResultsMessage for package %s with an invalid number of platform results: expected %d, actual %d. Ignoring it."),
					WorkerId.GetRemoteIndex(), *Result.PackageName.ToString(), NumPlatforms, Result.Platforms.Num());
				continue;
			}
			for (int32 PlatformIndex = 0; PlatformIndex < NumPlatforms; ++PlatformIndex)
			{
				ITargetPlatform* TargetPlatform = OrderedSessionPlatforms[PlatformIndex];
				FPackageRemoteResult::FPlatformResult& PlatformResult = Result.Platforms[PlatformIndex];
				PackageData->SetPlatformCooked(TargetPlatform, PlatformResult.bSuccessful);
				// MPCOOKTODO: Call CommitRemotePackage on the PackageWriter
				HandleReceivedPackagePlatformMessages(*PackageData, TargetPlatform, MoveTemp(PlatformResult.Messages));
			}
			if (Result.bReferencedOnlyByEditorOnlyData)
			{
				COTFS.PackageTracker->UncookedEditorOnlyPackages.AddUnique(Result.PackageName);
			}
			COTFS.PromoteToSaveComplete(*PackageData, ESendFlags::QueueAddAndRemove);
		}
		else
		{
			COTFS.DemoteToIdle(*PackageData, ESendFlags::QueueAddAndRemove, Result.SuppressCookReason);
		}
	}
}

void FCookWorkerServer::LogInvalidMessage(const TCHAR* MessageTypeName)
{
	UE_LOG(LogCook, Error, TEXT("CookWorkerServer received invalidly formatted message for type %s from CookWorker. Ignoring it."),
		MessageTypeName);
}

void FCookWorkerServer::AddDiscoveredPackage(FDiscoveredPackage&& DiscoveredPackage)
{
	check(TickState.TickThread == ECookDirectorThread::SchedulerThread);

	FPackageData& PackageData = COTFS.PackageDatas->FindOrAddPackageData(DiscoveredPackage.PackageName,
		DiscoveredPackage.NormalizedFileName);
	if (PackageData.IsInProgress() || PackageData.HasAnyCookedPlatform())
	{
		// The CookWorker thought this was a new package, but the Director already knows about it; ignore the report
		return;
	}

	if (DiscoveredPackage.Instigator.Category == EInstigator::GeneratedPackage)
	{
		PackageData.SetGenerated(true);
		PackageData.SetWorkerAssignmentConstraint(GetWorkerId());
	}
	COTFS.QueueDiscoveredPackageData(PackageData, MoveTemp(DiscoveredPackage.Instigator));
}

FCookWorkerServer::FTickState::FTickState()
{
	TickThread = ECookDirectorThread::Invalid;
	TickAction = ETickAction::Invalid;
}

FCookWorkerServer::FCommunicationScopeLock::FCommunicationScopeLock(FCookWorkerServer* InServer, ECookDirectorThread TickThread, ETickAction TickAction)
	: ScopeLock(&InServer->CommunicationLock)
	, Server(*InServer)
{
	check(TickThread != ECookDirectorThread::Invalid);
	check(TickAction != ETickAction::Invalid);
	check(Server.TickState.TickThread == ECookDirectorThread::Invalid);
	Server.TickState.TickThread = TickThread;
	Server.TickState.TickAction = TickAction;
}

FCookWorkerServer::FCommunicationScopeLock::~FCommunicationScopeLock()
{
	check(Server.TickState.TickThread != ECookDirectorThread::Invalid);
	Server.TickState.TickThread = ECookDirectorThread::Invalid;
	Server.TickState.TickAction = ETickAction::Invalid;
}

FAssignPackagesMessage::FAssignPackagesMessage(TArray<FAssignPackageData>&& InPackageDatas)
	: PackageDatas(MoveTemp(InPackageDatas))
{
}

void FAssignPackagesMessage::Write(FCbWriter& Writer) const
{
	Writer << "P" << PackageDatas;
}

bool FAssignPackagesMessage::TryRead(FCbObject&& Object)
{
	return LoadFromCompactBinary(Object["P"], PackageDatas);
}

FGuid FAssignPackagesMessage::MessageType(TEXT("B7B1542B73254B679319D73F753DB6F8"));

FCbWriter& operator<<(FCbWriter& Writer, const FAssignPackageData& AssignData)
{
	Writer.BeginObject();
	Writer << "C" << AssignData.ConstructData;
	Writer << "I" << AssignData.Instigator;
	Writer.EndObject();
	return Writer;
}

bool LoadFromCompactBinary(FCbFieldView Field, FAssignPackageData& AssignData)
{
	bool bOk = LoadFromCompactBinary(Field["C"], AssignData.ConstructData);
	bOk = LoadFromCompactBinary(Field["I"], AssignData.Instigator) & bOk;
	return bOk;
}

FCbWriter& operator<<(FCbWriter& Writer, const FInstigator& Instigator)
{
	Writer.BeginObject();
	Writer << "C" << static_cast<uint8>(Instigator.Category);
	Writer << "R" << Instigator.Referencer;
	Writer.EndObject();
	return Writer;
}

bool LoadFromCompactBinary(FCbFieldView Field, FInstigator& Instigator)
{
	uint8 CategoryInt;
	bool bOk = true;
	if (LoadFromCompactBinary(Field["C"], CategoryInt) &&
		CategoryInt < static_cast<uint8>(EInstigator::Count))
	{
		Instigator.Category = static_cast<EInstigator>(CategoryInt);
	}
	else
	{
		Instigator.Category = EInstigator::InvalidCategory;
		bOk = false;
	}
	bOk = LoadFromCompactBinary(Field["R"], Instigator.Referencer) & bOk;
	return bOk;
}

FAbortPackagesMessage::FAbortPackagesMessage(TArray<FName>&& InPackageNames)
	: PackageNames(MoveTemp(InPackageNames))
{
}

void FAbortPackagesMessage::Write(FCbWriter& Writer) const
{
	Writer << "PackageNames" <<  PackageNames;
}

bool FAbortPackagesMessage::TryRead(FCbObject&& Object)
{
	return LoadFromCompactBinary(Object["PackageNames"], PackageNames);
}

FGuid FAbortPackagesMessage::MessageType(TEXT("D769F1BFF2F34978868D70E3CAEE94E7"));

FAbortWorkerMessage::FAbortWorkerMessage(EType InType)
	: Type(InType)
{
}

void FAbortWorkerMessage::Write(FCbWriter& Writer) const
{
	Writer << "Type" << (uint8)Type;
}

bool FAbortWorkerMessage::TryRead(FCbObject&& Object)
{
	Type = static_cast<EType>(Object["Type"].AsUInt8((uint8)EType::Abort));
	return true;
}

FGuid FAbortWorkerMessage::MessageType(TEXT("83FD99DFE8DB4A9A8E71684C121BE6F3"));

void FInitialConfigMessage::ReadFromLocal(const UCookOnTheFlyServer& COTFS,
	const TArray<ITargetPlatform*>& InOrderedSessionPlatforms, const FCookByTheBookOptions& InCookByTheBookOptions,
	const FCookOnTheFlyOptions& InCookOnTheFlyOptions, const FBeginCookContextForWorker& InBeginContext)
{
	InitialSettings.CopyFromLocal(COTFS);
	BeginCookSettings.CopyFromLocal(COTFS);
	BeginCookContext = InBeginContext;
	OrderedSessionPlatforms = InOrderedSessionPlatforms;
	DirectorCookMode = COTFS.GetCookMode();
	CookInitializationFlags = COTFS.GetCookFlags();
	CookByTheBookOptions = InCookByTheBookOptions;
	CookOnTheFlyOptions = InCookOnTheFlyOptions;
	bZenStore = COTFS.IsUsingZenStore();
}

void FInitialConfigMessage::Write(FCbWriter& Writer) const
{
	int32 LocalCookMode = static_cast<int32>(DirectorCookMode);
	Writer << "DirectorCookMode" << LocalCookMode;
	int32 LocalCookFlags = static_cast<int32>(CookInitializationFlags);
	Writer << "CookInitializationFlags" << LocalCookFlags;
	Writer << "ZenStore" << bZenStore;

	Writer.BeginArray("TargetPlatforms");
	for (const ITargetPlatform* TargetPlatform : OrderedSessionPlatforms)
	{
		Writer << TargetPlatform->PlatformName();
	}
	Writer.EndArray();
	Writer << "InitialSettings" << InitialSettings;
	Writer << "BeginCookSettings" << BeginCookSettings;
	Writer << "BeginCookContext" << BeginCookContext;
	Writer << "CookByTheBookOptions" << CookByTheBookOptions;
	Writer << "CookOnTheFlyOptions" << CookOnTheFlyOptions;
}

bool FInitialConfigMessage::TryRead(FCbObject&& Object)
{
	bool bOk = true;
	int32 LocalCookMode;
	bOk = LoadFromCompactBinary(Object["DirectorCookMode"], LocalCookMode) & bOk;
	DirectorCookMode = static_cast<ECookMode::Type>(LocalCookMode);
	int32 LocalCookFlags;
	bOk = LoadFromCompactBinary(Object["CookInitializationFlags"], LocalCookFlags) & bOk;
	CookInitializationFlags = static_cast<ECookInitializationFlags>(LocalCookFlags);
	bOk = LoadFromCompactBinary(Object["ZenStore"], bZenStore) & bOk;

	ITargetPlatformManagerModule& TPM(GetTargetPlatformManagerRef());
	FCbFieldView TargetPlatformsField = Object["TargetPlatforms"];
	{
		bOk = TargetPlatformsField.IsArray() & bOk;
		OrderedSessionPlatforms.Reset(TargetPlatformsField.AsArrayView().Num());
		for (FCbFieldView ElementField : TargetPlatformsField)
		{
			TStringBuilder<128> KeyName;
			if (LoadFromCompactBinary(ElementField, KeyName))
			{
				ITargetPlatform* TargetPlatform = TPM.FindTargetPlatform(KeyName.ToView());
				if (TargetPlatform)
				{
					OrderedSessionPlatforms.Add(TargetPlatform);
				}
				else
				{
					UE_LOG(LogCook, Error, TEXT("Could not find TargetPlatform \"%.*s\" received from CookDirector."),
						KeyName.Len(), KeyName.GetData());
					bOk = false;
				}

			}
			else
			{
				bOk = false;
			}
		}
	}

	bOk = LoadFromCompactBinary(Object["InitialSettings"], InitialSettings) & bOk;
	bOk = LoadFromCompactBinary(Object["BeginCookSettings"], BeginCookSettings) & bOk;
	bOk = LoadFromCompactBinary(Object["BeginCookContext"], BeginCookContext) & bOk;
	bOk = LoadFromCompactBinary(Object["CookByTheBookOptions"], CookByTheBookOptions) & bOk;
	bOk = LoadFromCompactBinary(Object["CookOnTheFlyOptions"], CookOnTheFlyOptions) & bOk;
	return bOk;
}

FGuid FInitialConfigMessage::MessageType(TEXT("340CDCB927304CEB9C0A66B5F707FC2B"));

FCbWriter& operator<<(FCbWriter& Writer, const FDiscoveredPackage& Package)
{
	Writer.BeginObject();
	Writer << "PackageName" << Package.PackageName;
	Writer << "NormalizedFileName" << Package.NormalizedFileName;
	Writer << "Instigator.Category" << static_cast<uint8>(Package.Instigator.Category);
	Writer << "Instigator.Referencer" << Package.Instigator.Referencer;
	Writer.EndObject();
	return Writer;
}

bool LoadFromCompactBinary(FCbFieldView Field, FDiscoveredPackage& OutPackage)
{
	bool bOk = LoadFromCompactBinary(Field["PackageName"], OutPackage.PackageName);
	bOk = LoadFromCompactBinary(Field["NormalizedFileName"], OutPackage.NormalizedFileName) & bOk;
	uint8 CategoryInt;
	if (LoadFromCompactBinary(Field["Instigator.Category"], CategoryInt) &&
		CategoryInt < static_cast<uint8>(EInstigator::Count))

	{
		OutPackage.Instigator.Category = static_cast<EInstigator>(CategoryInt);
	}
	else
	{
		bOk = false;
	}
	bOk = LoadFromCompactBinary(Field["Instigator.Referencer"], OutPackage.Instigator.Referencer) & bOk;
	if (!bOk)
	{
		OutPackage = FDiscoveredPackage();
	}
	return bOk;
}

void FDiscoveredPackagesMessage::Write(FCbWriter& Writer) const
{
	Writer << "Packages" << Packages;
}

bool FDiscoveredPackagesMessage::TryRead(FCbObject&& Object)
{
	return LoadFromCompactBinary(Object["Packages"], Packages);
}

FGuid FDiscoveredPackagesMessage::MessageType(TEXT("C9F5BC5C11484B06B346B411F1ED3090"));

FCbWriter& operator<<(FCbWriter& Writer, const FReplicatedLogData& Package)
{
	Writer.BeginArray();
	Writer << Package.Category;
	uint8 Verbosity = static_cast<uint8>(Package.Verbosity);
	check(Verbosity < ELogVerbosity::NumVerbosity);
	Writer << Verbosity;
	Writer << Package.Message;
	Writer.EndArray();
	return Writer;
}

bool LoadFromCompactBinary(FCbFieldView Field, FReplicatedLogData& OutPackage)
{
	bool bOk = true;
	FCbFieldViewIterator It = Field.CreateViewIterator();
	bOk = LoadFromCompactBinary(*It++, OutPackage.Category) & bOk;
	uint8 Verbosity;
	if (LoadFromCompactBinary(*It++, Verbosity) && Verbosity <= ELogVerbosity::NumVerbosity)
	{
		OutPackage.Verbosity = static_cast<ELogVerbosity::Type>(Verbosity);
	}
	else
	{
		bOk = false;
		OutPackage.Verbosity = static_cast<ELogVerbosity::Type>(0);
	}
	bOk = LoadFromCompactBinary(*It++, OutPackage.Message) & bOk;
	return bOk;
}

FGuid FLogMessagesMessageHandler::MessageType(TEXT("DB024D28203D4FBAAAF6AAD7080CF277"));

FLogMessagesMessageHandler::~FLogMessagesMessageHandler()
{
	if (bRegistered && GLog)
	{
		GLog->RemoveOutputDevice(this);
	}
}
void FLogMessagesMessageHandler::InitializeClient()
{
	check(!bRegistered);
	GLog->AddOutputDevice(this);
	bRegistered = true;
}

void FLogMessagesMessageHandler::ClientTick(FClientContext& Context)
{
	{
		FScopeLock QueueScopeLock(&QueueLock);
		Swap(QueuedLogs, QueuedLogsBackBuffer);
	}
	if (!QueuedLogsBackBuffer.IsEmpty())
	{
		FCbWriter Writer;
		Writer.BeginObject();
		Writer << "Messages" << QueuedLogsBackBuffer;
		Writer.EndObject();
		Context.AddMessage(Writer.Save().AsObject());
		QueuedLogsBackBuffer.Reset();
	}
}

void FLogMessagesMessageHandler::ReceiveMessage(FServerContext& Context, FCbObjectView InMessage)
{
	TArray<FReplicatedLogData> Messages;
	if (!LoadFromCompactBinary(InMessage["Messages"], Messages))
	{
		UE_LOG(LogCook, Error, TEXT("FLogMessagesMessageHandler received corrupted message from CookWorker"));
		return;
	}

	for (FReplicatedLogData& LogData : Messages)
	{
		GLog->CategorizedLogf(LogData.Category, LogData.Verbosity, TEXT("[CookWorker %d]: %s"),
			Context.GetWorkerId().GetRemoteIndex(), *LogData.Message);
	}
}

void FLogMessagesMessageHandler::Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity,
	const FName& Category)
{
	FScopeLock QueueScopeLock(&QueueLock);
	QueuedLogs.Add(FReplicatedLogData{ FString(V), Category, Verbosity });
}

void FLogMessagesMessageHandler::Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity,
	const FName& Category, const double Time)
{
	Serialize(V, Verbosity, Category);
}

}