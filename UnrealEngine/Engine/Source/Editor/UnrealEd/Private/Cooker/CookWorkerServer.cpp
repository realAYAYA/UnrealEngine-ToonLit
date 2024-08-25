// Copyright Epic Games, Inc. All Rights Reserved.

#include "CookWorkerServer.h"

#include "Algo/Find.h"
#include "Commandlets/AssetRegistryGenerator.h"
#include "CompactBinaryTCP.h"
#include "Cooker/CookDirector.h"
#include "Cooker/CookPackageData.h"
#include "Cooker/CookPlatformManager.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "Math/NumericLimits.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Char.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/ScopeLock.h"
#include "PackageResultsMessage.h"
#include "PackageTracker.h"
#include "UnrealEdMisc.h"

namespace UE::Cook
{

FCookWorkerServer::FCookWorkerServer(FCookDirector& InDirector, int32 InProfileId, FWorkerId InWorkerId)
	: Director(InDirector)
	, COTFS(InDirector.COTFS)
	, ProfileId(InProfileId)
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
			ProfileId);
	}
	DetachFromRemoteProcess();
}

void FCookWorkerServer::DetachFromRemoteProcess()
{
	if (Socket != nullptr)
	{
		FCoreDelegates::OnMultiprocessWorkerDetached.Broadcast({WorkerId.GetMultiprocessId()});
	}
	Sockets::CloseSocket(Socket);
	CookWorkerHandle = FProcHandle();
	CookWorkerProcessId = 0;
	bTerminateImmediately = false;
	SendBuffer.Reset();
	ReceiveBuffer.Reset();

	if (bNeedCrashDiagnostics)
	{
		SendCrashDiagnostics();
	}
}

bool TryParseLogCategoryVerbosityMessage(FStringView Line, FName& OutCategory, ELogVerbosity::Type& OutVerbosity, FStringView& OutMessage)
{
	TPair<FStringView, ELogVerbosity::Type> VerbosityMarkers[]{
		{ TEXTVIEW(": Fatal:"), ELogVerbosity::Fatal },
		{ TEXTVIEW(": Error:"), ELogVerbosity::Error },
		{ TEXTVIEW(": Warning:"), ELogVerbosity::Warning},
		{ TEXTVIEW(": Display:"), ELogVerbosity::Display },
		{ TEXTVIEW(":"), ELogVerbosity::Log },
	};


	// Find the first colon not in brackets and look for ": <Verbosity>:". This is complicated by Log verbosity not printing out the Verbosity:
	// [2023.03.20-16.32.48:878][  0]LogCook: MessageText
	// [2023.03.20-16.32.48:878][  0]LogCook: Display: MessageText

	int32 FirstColon = INDEX_NONE;
	int32 SubExpressionLevel = 0;
	for (int32 Index = 0; Index < Line.Len(); ++Index)
	{
		switch (Line[Index])
		{
		case '[':
			++SubExpressionLevel;
			break;
		case ']':
			if (SubExpressionLevel > 0)
			{
				--SubExpressionLevel;
			}
			break;
		case ':':
			if (SubExpressionLevel == 0)
			{
				FirstColon = Index;
			}
			break;
		default:
			break;
		}
		if (FirstColon != INDEX_NONE)
		{
			break;
		}
	}
	if (FirstColon == INDEX_NONE)
	{
		return false;
	}

	FStringView RestOfLine = FStringView(Line).RightChop(FirstColon);
	for (TPair<FStringView, ELogVerbosity::Type>& VerbosityPair : VerbosityMarkers)
	{
		if (RestOfLine.StartsWith(VerbosityPair.Key, ESearchCase::IgnoreCase))
		{
			int32 CategoryEndIndex = FirstColon;
			while (CategoryEndIndex > 0 && FChar::IsWhitespace(Line[CategoryEndIndex - 1])) --CategoryEndIndex;
			int32 CategoryStartIndex = CategoryEndIndex > 0 ? CategoryEndIndex - 1 : CategoryEndIndex;
			while (CategoryStartIndex > 0 && FChar::IsAlnum(Line[CategoryStartIndex - 1])) --CategoryStartIndex;
			int32 MessageStartIndex = CategoryEndIndex + VerbosityPair.Key.Len();
			while (MessageStartIndex < Line.Len() && FChar::IsWhitespace(Line[MessageStartIndex])) ++MessageStartIndex;

			OutCategory = FName(FStringView(Line).SubStr(CategoryStartIndex, CategoryEndIndex - CategoryStartIndex));
			OutVerbosity = VerbosityPair.Value;
			OutMessage = FStringView(Line).SubStr(MessageStartIndex, Line.Len() - MessageStartIndex);
			return true;
		}
	}
	return false;
}

void FCookWorkerServer::SendCrashDiagnostics()
{
	FString LogFileName = Director.GetWorkerLogFileName(ProfileId);
	UE_LOG(LogCook, Display, TEXT("LostConnection to CookWorker %d. Log messages written after communication loss:"), ProfileId);
	FString LogText;
	int32 ReadFlags = FILEREAD_AllowWrite; // To be able to open a file for read that might be open for write from another process, we have to specify FILEREAD_AllowWrite
	bool bLoggedErrorMessage = false;
	if (!FFileHelper::LoadFileToString(LogText, *LogFileName, FFileHelper::EHashOptions::None, ReadFlags))
	{
		UE_LOG(LogCook, Warning, TEXT("No log file found for CookWorker %d."), ProfileId);
	}
	else
	{
		FString LastSentHeartbeat = FString::Printf(TEXT("%.*s %d"), HeartbeatCategoryText.Len(), HeartbeatCategoryText.GetData(),
			LastReceivedHeartbeatNumber);
		int32 StartIndex = INDEX_NONE;
		for (FStringView MarkerText : { FStringView(LastSentHeartbeat),
			HeartbeatCategoryText, TEXTVIEW("Connection to CookDirector successful") })
		{
			StartIndex = UE::String::FindLast(LogText, MarkerText);
			if (StartIndex >= 0)
			{
				break;
			}
		}
		const TCHAR* StartText = *LogText;
		FString Line;
		if (StartIndex != INDEX_NONE)
		{
			// Skip the MarkerLine
			StartText = *LogText + StartIndex;
			FParse::Line(&StartText, Line);
			if (*StartText == '\0')
			{
				// If there was no line after the MarkerLine, write out the MarkerLine
				StartText = *LogText + StartIndex;
			}
		}

		while (FParse::Line(&StartText, Line))
		{
			// Get the Category,Severity,Message out of each line and log it with that Category and Severity
			// TODO: Change the CookWorkers to write out structured logs rather than interpreting their text logs
			FName Category;
			ELogVerbosity::Type Verbosity;
			FStringView Message;
			if (!TryParseLogCategoryVerbosityMessage(Line, Category, Verbosity, Message))
			{
				Category = LogCook.GetCategoryName();
				Verbosity = ELogVerbosity::Display;
				Message = Line;
			}
			// Downgrade Fatals in our local verbosity from Fatal to Error to avoid crashing the CookDirector
			if (Verbosity == ELogVerbosity::Fatal)
			{
				Verbosity = ELogVerbosity::Error;
			}
			bLoggedErrorMessage |= Verbosity == ELogVerbosity::Error;
			FMsg::Logf(__FILE__, __LINE__, Category, Verbosity, TEXT("[CookWorker %d]: %.*s"),
				ProfileId, Message.Len(), Message.GetData());
		}
	}
	if (!CrashDiagnosticsError.IsEmpty())
	{
		if (!bLoggedErrorMessage)
		{
			UE_LOG(LogCook, Error, TEXT("%s"), *CrashDiagnosticsError);
		}
		else
		{
			// When we already logged an error from the crashed worker, log the what-went-wrong as a warning rather than an error,
			// to avoid making it seem like a separate issue.
			UE_LOG(LogCook, Warning, TEXT("%s"), *CrashDiagnosticsError);
		}
	}

	bNeedCrashDiagnostics = false;
	CrashDiagnosticsError.Empty();
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
	++PackagesAssignedFenceMarker;
	PackagesToAssign.Append(Assignments);
}

void FCookWorkerServer::AbortAllAssignments(TSet<FPackageData*>& OutPendingPackages, ECookDirectorThread TickThread)
{
	FCommunicationScopeLock ScopeLock(this, TickThread, ETickAction::Queue);
	AbortAllAssignmentsInLock(OutPendingPackages);
}

void FCookWorkerServer::AbortAllAssignmentsInLock(TSet<FPackageData*>& OutPendingPackages)
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
			SendMessageInLock(FAbortPackagesMessage(MoveTemp(PackageNames)));
		}
		OutPendingPackages.Append(MoveTemp(PendingPackages));
		PendingPackages.Empty();
	}
	OutPendingPackages.Append(PackagesToAssign);
	PackagesToAssign.Empty();
	++PackagesRetiredFenceMarker;
}

void FCookWorkerServer::AbortAssignment(FPackageData& PackageData, ECookDirectorThread TickThread,
	ENotifyRemote NotifyRemote)
{
	FPackageData* PackageDataPtr = &PackageData;
	AbortAssignments(TConstArrayView<FPackageData*>(&PackageDataPtr, 1), TickThread, NotifyRemote);
}

void FCookWorkerServer::AbortAssignments(TConstArrayView<FPackageData*> PackageDatas, ECookDirectorThread TickThread,
	ENotifyRemote NotifyRemote)
{
	FCommunicationScopeLock ScopeLock(this, TickThread, ETickAction::Queue);

	TArray<FName> PackageNamesToMessage;
	bool bSignalRemote = ConnectStatus == EConnectStatus::Connected && NotifyRemote == ENotifyRemote::NotifyRemote;
	for (FPackageData* PackageData : PackageDatas)
	{
		if (PendingPackages.Remove(PackageData))
		{
			if (bSignalRemote)
			{
				PackageNamesToMessage.Add(PackageData->GetPackageName());
			}
		}

		PackagesToAssign.Remove(PackageData);
	}
	++PackagesRetiredFenceMarker;
	if (!PackageNamesToMessage.IsEmpty())
	{
		SendMessageInLock(FAbortPackagesMessage(MoveTemp(PackageNamesToMessage)));
	}
}

void FCookWorkerServer::AbortWorker(TSet<FPackageData*>& OutPendingPackages, ECookDirectorThread TickThread)
{
	FCommunicationScopeLock ScopeLock(this, TickThread, ETickAction::Tick);

	AbortAllAssignmentsInLock(OutPendingPackages);
	switch (ConnectStatus)
	{
	case EConnectStatus::Uninitialized: // Fall through
	case EConnectStatus::WaitForConnect:
		SendToState(EConnectStatus::LostConnection);
		break;
	case EConnectStatus::Connected: // Fall through
	case EConnectStatus::PumpingCookComplete:
	{
		SendMessageInLock(FAbortWorkerMessage(FAbortWorkerMessage::EType::Abort));
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

bool FCookWorkerServer::IsConnected() const
{
	FScopeLock CommunicationScopeLock(&CommunicationLock);
	return ConnectStatus == EConnectStatus::Connected;
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

int32 FCookWorkerServer::NumAssignments() const
{
	FScopeLock CommunicationScopeLock(&CommunicationLock);
	return PackagesToAssign.Num() + PendingPackages.Num();
}

bool FCookWorkerServer::HasMessages() const
{
	FScopeLock CommunicationScopeLock(&CommunicationLock);
	return !ReceiveMessages.IsEmpty();
}

int32 FCookWorkerServer::GetLastReceivedHeartbeatNumber() const
{
	FScopeLock CommunicationScopeLock(&CommunicationLock);
	return LastReceivedHeartbeatNumber;

}
void FCookWorkerServer::SetLastReceivedHeartbeatNumberInLock(int32 InHeartbeatNumber)
{
	LastReceivedHeartbeatNumber = InHeartbeatNumber;
}

int32 FCookWorkerServer::GetPackagesAssignedFenceMarker() const
{
	FScopeLock CommunicationScopeLock(&CommunicationLock);
	return PackagesAssignedFenceMarker;
}

int32 FCookWorkerServer::GetPackagesRetiredFenceMarker() const
{
	FScopeLock CommunicationScopeLock(&CommunicationLock);
	return PackagesRetiredFenceMarker;
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
	UE_LOG(LogCook, Display, TEXT("CookWorker %d connected after %.3fs."), ProfileId,
		static_cast<float>(FPlatformTime::Seconds() - ConnectStartTimeSeconds));
	for (UE::CompactBinaryTCP::FMarshalledMessage& OtherMessage : OtherPacketMessages)
	{
		ReceiveMessages.Add(MoveTemp(OtherMessage));
	}
	HandleReceiveMessagesInternal();
	const FInitialConfigMessage& InitialConfigMessage = Director.GetInitialConfigMessage();
	OrderedSessionPlatforms = InitialConfigMessage.GetOrderedSessionPlatforms();
	OrderedSessionAndSpecialPlatforms.Reset(OrderedSessionPlatforms.Num() + 1);
	OrderedSessionAndSpecialPlatforms.Append(OrderedSessionPlatforms);
	OrderedSessionAndSpecialPlatforms.Add(CookerLoadingPlatformKey);
	SendMessageInLock(InitialConfigMessage);
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
					ProfileId, WaitForPumpCompleteTimeout);
				SendMessageInLock(FAbortWorkerMessage(FAbortWorkerMessage::EType::Abort));
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

void FCookWorkerServer::SignalHeartbeat(ECookDirectorThread TickThread, int32 HeartbeatNumber)
{
	FCommunicationScopeLock ScopeLock(this, TickThread, ETickAction::Tick);

	switch (ConnectStatus)
	{
	case EConnectStatus::Connected:
		SendMessageInLock(FHeartbeatMessage(HeartbeatNumber));
		break;
	default:
		break;
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
		SendMessageInLock(FAbortWorkerMessage(FAbortWorkerMessage::EType::CookComplete));
		SendToState(EConnectStatus::PumpingCookComplete);
		break;
	default:
		break; // Already in a disconnecting state
	}
}

void FCookWorkerServer::LaunchProcess()
{
	FCookDirector::FLaunchInfo LaunchInfo = Director.GetLaunchInfo(WorkerId, ProfileId);
	bool bShowCookWorkers = LaunchInfo.ShowWorkerOption == FCookDirector::EShowWorker::SeparateWindows;

	CookWorkerHandle = FPlatformProcess::CreateProc(*LaunchInfo.CommandletExecutable, *LaunchInfo.WorkerCommandLine,
		true /* bLaunchDetached */, !bShowCookWorkers /* bLaunchHidden */, !bShowCookWorkers /* bLaunchReallyHidden */,
		&CookWorkerProcessId, 0 /* PriorityModifier */, *FPaths::GetPath(LaunchInfo.CommandletExecutable),
		nullptr /* PipeWriteChild */);
	if (CookWorkerHandle.IsValid())
	{
		UE_LOG(LogCook, Display, TEXT("CookWorkerServer %d launched CookWorker as WorkerId %d and PID %u with commandline \"%s\"."),
			ProfileId, WorkerId.GetRemoteIndex(), CookWorkerProcessId, *LaunchInfo.WorkerCommandLine);
		FCoreDelegates::OnMultiprocessWorkerCreated.Broadcast({WorkerId.GetMultiprocessId()});
		SendToState(EConnectStatus::WaitForConnect);
	}
	else
	{
		// GetLastError information was logged by CreateProc
		CrashDiagnosticsError = FString::Printf(TEXT("CookWorkerCrash: Failed to create process for CookWorker %d. Assigned packages will be returned to the director."),
			ProfileId);
		bNeedCrashDiagnostics = true;
		SendToState(EConnectStatus::LostConnection);
	}
}

void FCookWorkerServer::TickWaitForConnect()
{
	constexpr float TestProcessExistencePeriod = 1.f;
	constexpr float WaitForConnectTimeout = 60.f * 20;

	check(!Socket); // When the Socket is assigned we leave the WaitForConnect state, and we set it to null before entering

	double CurrentTime = FPlatformTime::Seconds();
	if (CurrentTime - ConnectTestStartTimeSeconds > TestProcessExistencePeriod)
	{
		if (!FPlatformProcess::IsProcRunning(CookWorkerHandle))
		{
			CrashDiagnosticsError = FString::Printf(TEXT("CookWorkerCrash: CookWorker %d process terminated before connecting. Assigned packages will be returned to the director."),
				ProfileId);
			bNeedCrashDiagnostics = true;
			SendToState(EConnectStatus::LostConnection);
			return;
		}
		ConnectTestStartTimeSeconds = FPlatformTime::Seconds();
	}

	if (CurrentTime - ConnectStartTimeSeconds > WaitForConnectTimeout && !IsCookIgnoreTimeouts())
	{
		CrashDiagnosticsError = FString::Printf(TEXT("CookWorkerCrash: CookWorker %d process failed to connect within %.0f seconds. Assigned packages will be returned to the director."),
			ProfileId, WaitForConnectTimeout);
		bNeedCrashDiagnostics = true;
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
			ProfileId, WaitForDisconnectTimeout);
		ShutdownRemoteProcess();
		SendToState(EConnectStatus::LostConnection);
	}
}

void FCookWorkerServer::PumpSendMessages()
{
	UE::CompactBinaryTCP::EConnectionStatus Status = UE::CompactBinaryTCP::TryFlushBuffer(Socket, SendBuffer);
	if (Status == UE::CompactBinaryTCP::EConnectionStatus::Failed)
	{
		UE_LOG(LogCook, Error, TEXT("CookWorkerCrash: CookWorker %d failed to write to socket, we will shutdown the remote process. Assigned packages will be returned to the director."),
			ProfileId);
		bNeedCrashDiagnostics = true;
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
	LLM_SCOPE_BYTAG(Cooker_MPCook);

	TArray<FAssignPackageData> AssignDatas;
	AssignDatas.Reserve(PackagesToAssign.Num());
	TBitArray<> SessionPlatformNeedsCook;

	for (FPackageData* PackageData : PackagesToAssign)
	{
		FAssignPackageData& AssignData = AssignDatas.Emplace_GetRef();
		AssignData.ConstructData = PackageData->CreateConstructData();
		AssignData.Instigator = PackageData->GetInstigator();
		SessionPlatformNeedsCook.Init(false, OrderedSessionPlatforms.Num());
		int32 PlatformIndex = 0;
		for (const ITargetPlatform* SessionPlatform : OrderedSessionPlatforms)
		{
			FPackagePlatformData* PlatformData = PackageData->FindPlatformData(SessionPlatform);
			SessionPlatformNeedsCook[PlatformIndex++] = PlatformData && PlatformData->NeedsCooking(SessionPlatform);
		}
		AssignData.NeedCookPlatforms = FDiscoveredPlatformSet(SessionPlatformNeedsCook);
	}
	PendingPackages.Append(PackagesToAssign);
	PackagesToAssign.Empty();
	FAssignPackagesMessage AssignPackagesMessage(MoveTemp(AssignDatas));
	AssignPackagesMessage.OrderedSessionPlatforms = OrderedSessionPlatforms;
	SendMessageInLock(MoveTemp(AssignPackagesMessage));
}

void FCookWorkerServer::PumpReceiveMessages()
{
	using namespace UE::CompactBinaryTCP;
	LLM_SCOPE_BYTAG(Cooker_MPCook);
	TArray<FMarshalledMessage> Messages;
	EConnectionStatus SocketStatus = TryReadPacket(Socket, ReceiveBuffer, Messages);
	if (SocketStatus != EConnectionStatus::Okay && SocketStatus != EConnectionStatus::Incomplete)
	{
		CrashDiagnosticsError = FString::Printf(TEXT("CookWorkerCrash: CookWorker %d failed to read from socket, we will shutdown the remote process. Assigned packages will be returned to the director."),
			ProfileId);
		bNeedCrashDiagnostics = true;
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
			if (ConnectStatus != EConnectStatus::PumpingCookComplete && ConnectStatus != EConnectStatus::WaitForDisconnect)
			{
				CrashDiagnosticsError = FString::Printf(TEXT("CookWorkerCrash: CookWorker %d remote process shut down unexpectedly. Assigned packages will be returned to the director."),
					ProfileId);
				bNeedCrashDiagnostics = true;
			}
			SendMessageInLock(FAbortWorkerMessage(FAbortWorkerMessage::AbortAcknowledge));
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
			if (!ResultsMessage.TryRead(Message.Object))
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
			DiscoveredMessage.OrderedSessionAndSpecialPlatforms = OrderedSessionAndSpecialPlatforms;
			if (!DiscoveredMessage.TryRead(Message.Object))
			{
				LogInvalidMessage(TEXT("FDiscoveredPackagesMessage"));
			}
			else
			{
				for (FDiscoveredPackageReplication& DiscoveredPackage : DiscoveredMessage.Packages)
				{
					QueueDiscoveredPackage(MoveTemp(DiscoveredPackage));
				}
			}
		}
		else
		{
			TRefCountPtr<IMPCollector>* Collector = Director.Collectors.Find(Message.MessageType);
			if (Collector)
			{
				check(*Collector);
				FMPCollectorServerMessageContext Context;
				Context.Server = this;
				Context.Platforms = OrderedSessionPlatforms;
				Context.WorkerId = WorkerId;
				Context.ProfileId = ProfileId;
				(*Collector)->ServerReceiveMessage(Context, Message.Object);
			}
			else
			{
				UE_LOG(LogCook, Error, TEXT("CookWorkerServer received message of unknown type %s from CookWorker. Ignoring it."),
					*Message.MessageType.ToString());
			}
		}
	}
}

void FCookWorkerServer::HandleReceivedPackagePlatformMessages(FPackageData& PackageData, const ITargetPlatform* TargetPlatform,
	TArray<UE::CompactBinaryTCP::FMarshalledMessage>&& Messages)
{
	check(TickState.TickThread == ECookDirectorThread::SchedulerThread);
	if (Messages.IsEmpty())
	{
		return;
	}

	FMPCollectorServerMessageContext Context;
	Context.Platforms = OrderedSessionPlatforms;
	Context.PackageName = PackageData.GetPackageName();
	Context.TargetPlatform = TargetPlatform;
	Context.Server = this;
	Context.ProfileId = ProfileId;
	Context.WorkerId = WorkerId;

	for (UE::CompactBinaryTCP::FMarshalledMessage& Message : Messages)
	{
		TRefCountPtr<IMPCollector>* Collector = Director.Collectors.Find(Message.MessageType);
		if (Collector)
		{
			check(*Collector);
			(*Collector)->ServerReceiveMessage(Context, Message.Object);
		}
		else
		{
			UE_LOG(LogCook, Error, TEXT("CookWorkerServer received PackageMessage of unknown type %s from CookWorker. Ignoring it."),
				*Message.MessageType.ToString());
		}
	}
}

void FCookWorkerServer::SendMessage(const IMPCollectorMessage& Message, ECookDirectorThread TickThread)
{
	FCommunicationScopeLock ScopeLock(this, TickThread, ETickAction::Tick);
	SendMessageInLock(Message);
}

void FCookWorkerServer::SendMessageInLock(const IMPCollectorMessage& Message)
{
	if (TickState.TickAction == ETickAction::Tick)
	{
		UE::CompactBinaryTCP::TryWritePacket(Socket, SendBuffer, MarshalToCompactBinaryTCP(Message));
	}
	else
	{
		check(TickState.TickAction == ETickAction::Queue);
		UE::CompactBinaryTCP::QueueMessage(SendBuffer, MarshalToCompactBinaryTCP(Message));
	}
}

void FCookWorkerServer::RecordResults(FPackageResultsMessage& Message)
{
	check(TickState.TickThread == ECookDirectorThread::SchedulerThread);

	bool bRetiredAnyPackages = false;
	for (FPackageRemoteResult& Result : Message.Results)
	{
		FPackageData* PackageData = COTFS.PackageDatas->FindPackageDataByPackageName(Result.GetPackageName());
		if (!PackageData)
		{
			UE_LOG(LogCook, Warning, TEXT("CookWorkerServer %d received FPackageResultsMessage for invalid package %s. Ignoring it."),
				ProfileId, *Result.GetPackageName().ToString());
			continue;
		}
		if (PendingPackages.Remove(PackageData) != 1)
		{
			UE_LOG(LogCook, Display, TEXT("CookWorkerServer %d received FPackageResultsMessage for package %s which is not a pending package. Ignoring it."),
				ProfileId, *Result.GetPackageName().ToString());
			continue;
		}
		bRetiredAnyPackages = true;
		PackageData->SetWorkerAssignment(FWorkerId::Invalid(), ESendFlags::QueueNone);

		// MPCOOKTODO: Refactor FSaveCookedPackageContext::FinishPlatform and ::FinishPackage so we can call them from here
		// to reduce duplication
		if (Result.GetSuppressCookReason() == ESuppressCookReason::NotSuppressed)
		{
			int32 NumPlatforms = OrderedSessionPlatforms.Num();
			if (Result.GetPlatforms().Num() != NumPlatforms)
			{
				UE_LOG(LogCook, Warning,
					TEXT("CookWorkerServer %d received FPackageResultsMessage for package %s with an invalid number of platform results: expected %d, actual %d. Ignoring it."),
					ProfileId, *Result.GetPackageName().ToString(), NumPlatforms, Result.GetPlatforms().Num());
				continue;
			}

			HandleReceivedPackagePlatformMessages(*PackageData, nullptr, Result.ReleaseMessages());
			for (int32 PlatformIndex = 0; PlatformIndex < NumPlatforms; ++PlatformIndex)
			{
				ITargetPlatform* TargetPlatform = OrderedSessionPlatforms[PlatformIndex];
				FPackageRemoteResult::FPlatformResult& PlatformResult = Result.GetPlatforms()[PlatformIndex];
				FPackagePlatformData& ExistingData = PackageData->FindOrAddPlatformData(TargetPlatform);
				if (!ExistingData.NeedsCooking(TargetPlatform))
				{
					if (PlatformResult.GetCookResults() != ECookResult::Invalid)
					{
						UE_LOG(LogCook, Display,
							TEXT("CookWorkerServer %d received FPackageResultsMessage for package %s, platform %s, but that platform has already been cooked. Ignoring the results for that platform."),
							ProfileId, *Result.GetPackageName().ToString(), *TargetPlatform->PlatformName());
					}
					continue;
				}
				else
				{
					if (PlatformResult.GetCookResults() != ECookResult::Invalid)
					{
						PackageData->SetPlatformCooked(TargetPlatform, PlatformResult.GetCookResults());
					}
					HandleReceivedPackagePlatformMessages(*PackageData, TargetPlatform, PlatformResult.ReleaseMessages());
				}
			}
			COTFS.RecordExternalActorDependencies(Result.GetExternalActorDependencies());
			if (Result.IsReferencedOnlyByEditorOnlyData())
			{
				COTFS.PackageTracker->UncookedEditorOnlyPackages.AddUnique(Result.GetPackageName());
			}
			COTFS.PromoteToSaveComplete(*PackageData, ESendFlags::QueueAddAndRemove);
		}
		else
		{
			COTFS.DemoteToIdle(*PackageData, ESendFlags::QueueAddAndRemove, Result.GetSuppressCookReason());
		}
	}
	Director.ResetFinalIdleHeartbeatFence();
	if (bRetiredAnyPackages)
	{
		++PackagesRetiredFenceMarker;
	}
}

void FCookWorkerServer::LogInvalidMessage(const TCHAR* MessageTypeName)
{
	UE_LOG(LogCook, Error, TEXT("CookWorkerServer received invalidly formatted message for type %s from CookWorker. Ignoring it."),
		MessageTypeName);
}

void FCookWorkerServer::QueueDiscoveredPackage(FDiscoveredPackageReplication&& DiscoveredPackage)
{
	check(TickState.TickThread == ECookDirectorThread::SchedulerThread);

	FPackageDatas& PackageDatas = *COTFS.PackageDatas;
	FInstigator& Instigator = DiscoveredPackage.Instigator;
	FDiscoveredPlatformSet& Platforms = DiscoveredPackage.Platforms;
	FPackageData& PackageData = PackageDatas.FindOrAddPackageData(DiscoveredPackage.PackageName,
		DiscoveredPackage.NormalizedFileName);

	TArray<const ITargetPlatform*, TInlineAllocator<ExpectedMaxNumPlatforms>> BufferPlatforms;
	TConstArrayView<const ITargetPlatform*> DiscoveredPlatforms;
	if (!COTFS.bSkipOnlyEditorOnly)
	{
		DiscoveredPlatforms = OrderedSessionAndSpecialPlatforms;
	}
	else
	{
		DiscoveredPlatforms = Platforms.GetPlatforms(COTFS, &Instigator, OrderedSessionAndSpecialPlatforms, &BufferPlatforms);
	}

	if (Instigator.Category != EInstigator::ForceExplorableSaveTimeSoftDependency &&
		PackageData.HasReachablePlatforms(DiscoveredPlatforms))
	{
		// The CookWorker thought there were some new reachable platforms, but the Director already knows about
		// all of them; ignore the report
		return;
	}
	if (COTFS.bSkipOnlyEditorOnly &&
		Instigator.Category == EInstigator::Unsolicited &&
		Platforms.GetSource() == EDiscoveredPlatformSet::CopyFromInstigator &&
		PackageData.FindOrAddPlatformData(CookerLoadingPlatformKey).IsReachable())
	{
		// The CookWorker thought this package was new (previously unreachable even by editoronly references),
		// and it is not marked as a known used-in-game or editor-only issue, so it fell back to reporting it
		// as used-in-game-because-its-not-a-known-issue (see UCookOnTheFlyServer::ProcessUnsolicitedPackages's
		// use of PackageData->FindOrAddPlatformData(CookerLoadingPlatformKey).IsReachable()).
		// But we only do that fall back for unexpected packages not found by the search of editor-only AssetRegistry
		// dependencies. And this package was found by that search; the director has already marked it as reachable by
		// editoronly references. Correct the heuristic: ignore the unmarked load because the load is expected as an
		// editor-only reference.
		return;
	}

	if (Instigator.Category == EInstigator::GeneratedPackage)
	{
		PackageData.SetGenerated(true);
		PackageData.SetWorkerAssignmentConstraint(GetWorkerId());
	}
	Director.ResetFinalIdleHeartbeatFence();
	Platforms.ConvertFromBitfield(OrderedSessionAndSpecialPlatforms);
	COTFS.QueueDiscoveredPackageOnDirector(PackageData, MoveTemp(Instigator), MoveTemp(Platforms), false /* bUrgent */);
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

UE::CompactBinaryTCP::FMarshalledMessage MarshalToCompactBinaryTCP(const IMPCollectorMessage& Message)
{
	UE::CompactBinaryTCP::FMarshalledMessage Marshalled;
	Marshalled.MessageType = Message.GetMessageType();
	FCbWriter Writer;
	Writer.BeginObject();
	Message.Write(Writer);
	Writer.EndObject();
	Marshalled.Object = Writer.Save().AsObject();
	return Marshalled;
}

FAssignPackagesMessage::FAssignPackagesMessage(TArray<FAssignPackageData>&& InPackageDatas)
	: PackageDatas(MoveTemp(InPackageDatas))
{
}

void FAssignPackagesMessage::Write(FCbWriter& Writer) const
{
	Writer.BeginArray("P");
	for (const FAssignPackageData& PackageData : PackageDatas)
	{
		WriteToCompactBinary(Writer, PackageData, OrderedSessionPlatforms);
	}
	Writer.EndArray();
}

bool FAssignPackagesMessage::TryRead(FCbObjectView Object)
{
	bool bOk = true;
	PackageDatas.Reset();
	for (FCbFieldView PackageField : Object["P"])
	{
		FAssignPackageData& PackageData = PackageDatas.Emplace_GetRef();
		if (!LoadFromCompactBinary(PackageField, PackageData, OrderedSessionPlatforms))
		{
			PackageDatas.Pop();
			bOk = false;
		}
	}
	return bOk;
}

FGuid FAssignPackagesMessage::MessageType(TEXT("B7B1542B73254B679319D73F753DB6F8"));

void WriteToCompactBinary(FCbWriter& Writer, const FAssignPackageData& AssignData, 
	TConstArrayView<const ITargetPlatform*> OrderedSessionPlatforms)
{
	Writer.BeginArray();
	Writer << AssignData.ConstructData;
	Writer << AssignData.Instigator;
	WriteToCompactBinary(Writer, AssignData.NeedCookPlatforms, OrderedSessionPlatforms);
	Writer.EndArray();
}

bool LoadFromCompactBinary(FCbFieldView Field, FAssignPackageData& AssignData,
	TConstArrayView<const ITargetPlatform*> OrderedSessionPlatforms)
{
	FCbFieldViewIterator It = Field.CreateViewIterator();
	bool bOk = true;
	bOk = LoadFromCompactBinary(*It++, AssignData.ConstructData) & bOk;
	bOk = LoadFromCompactBinary(*It++, AssignData.Instigator) & bOk;
	bOk = LoadFromCompactBinary(*It++, AssignData.NeedCookPlatforms, OrderedSessionPlatforms) & bOk;
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

bool FAbortPackagesMessage::TryRead(FCbObjectView Object)
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

bool FAbortWorkerMessage::TryRead(FCbObjectView Object)
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

bool FInitialConfigMessage::TryRead(FCbObjectView Object)
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

void WriteToCompactBinary(FCbWriter& Writer, const FDiscoveredPackageReplication& Package,
	TConstArrayView<const ITargetPlatform*> OrderedSessionAndSpecialPlatforms)
{
	Writer.BeginObject();
	Writer << "PackageName" << Package.PackageName;
	Writer << "NormalizedFileName" << Package.NormalizedFileName;
	Writer << "Instigator.Category" << static_cast<uint8>(Package.Instigator.Category);
	Writer << "Instigator.Referencer" << Package.Instigator.Referencer;
	Writer.SetName("Platforms");
	WriteToCompactBinary(Writer, Package.Platforms, OrderedSessionAndSpecialPlatforms);
	Writer.EndObject();
}

bool LoadFromCompactBinary(FCbFieldView Field, FDiscoveredPackageReplication& OutPackage,
	TConstArrayView<const ITargetPlatform*> OrderedSessionAndSpecialPlatforms)
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
	bOk = LoadFromCompactBinary(Field["Platforms"], OutPackage.Platforms, OrderedSessionAndSpecialPlatforms) & bOk;
	if (!bOk)
	{
		OutPackage = FDiscoveredPackageReplication();
	}
	return bOk;
}

void FDiscoveredPackagesMessage::Write(FCbWriter& Writer) const
{
	Writer.BeginArray("Packages");
	for (const FDiscoveredPackageReplication& Package : Packages)
	{
		WriteToCompactBinary(Writer, Package, OrderedSessionAndSpecialPlatforms);
	}
	Writer.EndArray();
}

bool FDiscoveredPackagesMessage::TryRead(FCbObjectView Object)
{
	bool bOk = true;
	Packages.Reset();
	for (FCbFieldView PackageField : Object["Packages"])
	{
		FDiscoveredPackageReplication& Package = Packages.Emplace_GetRef();
		if (!LoadFromCompactBinary(PackageField, Package, OrderedSessionAndSpecialPlatforms))
		{
			Packages.Pop();
			bOk = false;
		}
	}
	return bOk;
}

FGuid FDiscoveredPackagesMessage::MessageType(TEXT("C9F5BC5C11484B06B346B411F1ED3090"));

FCbWriter& operator<<(FCbWriter& Writer, const FReplicatedLogData& Package)
{
	Writer.BeginArray();
	Writer << Package.Category;
	uint8 Verbosity = static_cast<uint8>(Package.Verbosity);
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
	if (LoadFromCompactBinary(*It++, Verbosity))
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

void FLogMessagesMessageHandler::ClientTick(FMPCollectorClientTickContext& Context)
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

void FLogMessagesMessageHandler::ServerReceiveMessage(FMPCollectorServerMessageContext& Context, FCbObjectView InMessage)
{
	TArray<FReplicatedLogData> Messages;
	if (!LoadFromCompactBinary(InMessage["Messages"], Messages))
	{
		UE_LOG(LogCook, Error, TEXT("FLogMessagesMessageHandler received corrupted message from CookWorker"));
		return;
	}

	for (FReplicatedLogData& LogData : Messages)
	{
		if (LogData.Category == LogCookName && LogData.Message.Contains(HeartbeatCategoryText))
		{
			// Do not spam heartbeat messages into the CookDirector log
			continue;
		}

		FMsg::Logf(__FILE__, __LINE__, LogData.Category, LogData.Verbosity, TEXT("[CookWorker %d]: %s"),
			Context.GetProfileId(), *LogData.Message);
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

FGuid FHeartbeatMessage::MessageType(TEXT("C08FFAF07BF34DD3A2FFB8A287CDDE83"));

FHeartbeatMessage::FHeartbeatMessage(int32 InHeartbeatNumber)
	: HeartbeatNumber(InHeartbeatNumber)
{
}

void FHeartbeatMessage::Write(FCbWriter& Writer) const
{
	Writer << "H" << HeartbeatNumber;
}

bool FHeartbeatMessage::TryRead(FCbObjectView Object)
{
	return LoadFromCompactBinary(Object["H"], HeartbeatNumber);
}

FPackageWriterMPCollector::FPackageWriterMPCollector(UCookOnTheFlyServer& InCOTFS)
	: COTFS(InCOTFS)
{

}

void FPackageWriterMPCollector::ClientTickPackage(FMPCollectorClientTickPackageContext& Context)
{
	for (const FMPCollectorClientTickPackageContext::FPlatformData& PlatformData : Context.GetPlatformDatas())
	{
		if (PlatformData.CookResults == ECookResult::Invalid)
		{
			continue;
		}
		ICookedPackageWriter& PackageWriter = COTFS.FindOrCreatePackageWriter(PlatformData.TargetPlatform);
		TFuture<FCbObject> ObjectFuture = PackageWriter.WriteMPCookMessageForPackage(Context.GetPackageName());
		Context.AddAsyncPlatformMessage(PlatformData.TargetPlatform, MoveTemp(ObjectFuture));
	}
}

void FPackageWriterMPCollector::ServerReceiveMessage(FMPCollectorServerMessageContext& Context, FCbObjectView Message)
{
	FName PackageName = Context.GetPackageName();
	const ITargetPlatform* TargetPlatform = Context.GetTargetPlatform();
	check(PackageName.IsValid() && TargetPlatform);

	ICookedPackageWriter& PackageWriter = COTFS.FindOrCreatePackageWriter(TargetPlatform);
	if (!PackageWriter.TryReadMPCookMessageForPackage(PackageName, Message))
	{
		UE_LOG(LogCook, Error, TEXT("CookWorkerServer received invalidly formatted PackageWriter message from CookWorker %d. Ignoring it."),
			Context.GetProfileId());
	}
}

FGuid FPackageWriterMPCollector::MessageType(TEXT("D2B1CE3FD26644AF9EC28FBADB1BD331"));

}