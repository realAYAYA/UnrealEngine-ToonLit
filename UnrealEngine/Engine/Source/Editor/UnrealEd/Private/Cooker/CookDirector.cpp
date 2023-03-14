// Copyright Epic Games, Inc. All Rights Reserved.

#include "CookDirector.h"

#include "Async/Fundamental/Scheduler.h"
#include "CompactBinaryTCP.h"
#include "CookMPCollector.h"
#include "CookPackageData.h"
#include "CookPlatformManager.h"
#include "CookWorkerServer.h"
#include "CookOnTheSide/CookOnTheFlyServer.h"
#include "CoreGlobals.h"
#include "LoadBalanceCookBurden.h"
#include "HAL/PlatformMisc.h"
#include "HAL/RunnableThread.h"
#include "Math/NumericLimits.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Serialization/CompactBinary.h"
#include "Serialization/CompactBinaryWriter.h"
#include "ShaderCompiler.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "String/ParseTokens.h"
#include "UnrealEdMisc.h"

extern CORE_API int32 GNumForegroundWorkers; // TaskGraph.cpp

namespace UE::Cook
{

FCookDirector::FCookDirector(UCookOnTheFlyServer& InCOTFS)
	: RunnableShunt(*this) 
	, COTFS(InCOTFS)
{
	WorkersStalledStartTimeSeconds = MAX_flt;
	WorkersStalledWarnTimeSeconds = MAX_flt;
	ShutdownEvent->Reset();

	ParseConfig();
	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get();
	if (!SocketSubsystem)
	{
		UE_LOG(LogCook, Error, TEXT("CookDirector initialization failure: platform does not support network sockets. CookWorkers will be disabled."));
	}
	else
	{
		UE_LOG(LogCook, Display, TEXT("CookMultiprocess is enabled with %d CookWorker processes."), RequestedCookWorkerCount);
	}

	Register(new FLogMessagesMessageHandler());
}

void FCookDirector::ParseConfig()
{
	const TCHAR* CommandLine = FCommandLine::Get();
	FString Text;

	// CookWorkerCount
	RequestedCookWorkerCount = 3;
	GConfig->GetInt(TEXT("CookSettings"), TEXT("CookWorkerCount"), RequestedCookWorkerCount, GEditorIni);
	FParse::Value(CommandLine, TEXT("-CookWorkerCount="), RequestedCookWorkerCount);

	// CookDirectorListenPort
	WorkerConnectPort = Sockets::COOKDIRECTOR_DEFAULT_REQUEST_CONNECTION_PORT;
	FParse::Value(CommandLine, TEXT("-CookDirectorListenPort="), WorkerConnectPort);

	// ShowCookWorker
	if (!FParse::Value(CommandLine, TEXT("-ShowCookWorker="), Text))
	{
		if (FParse::Param(CommandLine, TEXT("ShowCookWorker")))
		{
			Text = TEXT("SeparateWindows");
		}
	}
	if (Text == TEXT("CombinedLogs")) { ShowWorkerOption = EShowWorker::CombinedLogs; }
	else if (Text == TEXT("SeparateLogs")) { ShowWorkerOption = EShowWorker::SeparateLogs; }
	else if (Text == TEXT("SeparateWindows")) { ShowWorkerOption = EShowWorker::SeparateWindows; }
	else
	{
		if (!Text.IsEmpty())
		{
			UE_LOG(LogCook, Warning, TEXT("Invalid selection \"%s\" for -ShowCookWorker."), *Text);
		}
		ShowWorkerOption = EShowWorker::CombinedLogs;
	}

	// LoadBalanceAlgorithm
	LoadBalanceAlgorithm = ELoadBalanceAlgorithm::CookBurden;
	if (FParse::Value(CommandLine, TEXT("-CookLoadBalance="), Text))
	{
		if (Text == TEXT("Striped")) { LoadBalanceAlgorithm = ELoadBalanceAlgorithm::Striped; }
		else if (Text == TEXT("CookBurden")) { LoadBalanceAlgorithm = ELoadBalanceAlgorithm::CookBurden; }
		else
		{
			UE_LOG(LogCook, Warning, TEXT("Invalid selection \"%s\" for -CookLoadBalance."), *Text);
		}
	}
}

FCookDirector::~FCookDirector()
{
	StopCommunicationThread();

	TSet<FPackageData*> AbortedAssignments;
	for (TPair<int32, TRefCountPtr<FCookWorkerServer>>& Pair : RemoteWorkers)
	{
		Pair.Value->AbortWorker(AbortedAssignments, ECookDirectorThread::SchedulerThread);
	}
	for (FPackageData* PackageData : AbortedAssignments)
	{
		check(PackageData->IsInProgress()); // Packages that were assigned to workers should be in the AssignedToWorker state
		PackageData->SetWorkerAssignment(FWorkerId::Invalid());
		PackageData->SendToState(UE::Cook::EPackageState::Request, ESendFlags::QueueAddAndRemove);
	}
	RemoteWorkers.Empty();
	PendingConnections.Empty();
	Sockets::CloseSocket(WorkerConnectSocket);
}

void FCookDirector::LaunchCommunicationThread()
{
	if (!CommunicationThread && FPlatformProcess::SupportsMultithreading())
	{
		CommunicationThread = FRunnableThread::Create(&RunnableShunt, TEXT("FCookDirector"), 0, TPri_Normal);
	}
}

void FCookDirector::StopCommunicationThread()
{
	ShutdownEvent->Trigger();
	if (CommunicationThread)
	{
		CommunicationThread->WaitForCompletion();
		delete CommunicationThread;
		CommunicationThread = nullptr;
	}
	ShutdownEvent->Reset();
}

uint32 FCookDirector::RunCommunicationThread()
{
	constexpr float TickPeriod = 1.f;
	constexpr float MinSleepTime = 0.001f;
	for (;;)
	{
		double StartTime = FPlatformTime::Seconds();
		TickCommunication(ECookDirectorThread::CommunicateThread);

		double CurrentTime = FPlatformTime::Seconds();
		float RemainingDuration = StartTime + TickPeriod - CurrentTime;
		uint32 WaitTimeMilliseconds = static_cast<uint32>(RemainingDuration * .001f);
		if (ShutdownEvent->Wait(WaitTimeMilliseconds))
		{
			break;
		}
	}
	return 0;
}

uint32 FCookDirector::FRunnableShunt::Run()
{
	return Director.RunCommunicationThread();
}

void FCookDirector::FRunnableShunt::Stop()
{
	Director.ShutdownEvent->Trigger();
}

void FCookDirector::StartCook(const FBeginCookContext& InBeginContext)
{
	BeginCookContext.Set(InBeginContext);
}

void FCookDirector::AssignRequests(TArrayView<UE::Cook::FPackageData*> Requests, TArray<FWorkerId>& OutAssignments,
	TMap<FPackageData*, TArray<FPackageData*>>&& RequestGraph)
{
	ActivateMachineResourceReduction();

	TArray<TRefCountPtr<FCookWorkerServer>> SortedWorkers;
	int32 MaxRemoteIndex = -1;
	FScopeLock CommunicationScopeLock(&CommunicationLock);
	{
		InitializeWorkers();

		// Convert the Map of RemoteWorkers into an array sorted by WorkerIndex
		SortedWorkers.Reserve(RemoteWorkers.Num());
		for (TPair<int32, TRefCountPtr<FCookWorkerServer>>& Pair : RemoteWorkers)
		{
			SortedWorkers.Add(Pair.Value);
			MaxRemoteIndex = FMath::Max(Pair.Key, MaxRemoteIndex);
		}
	}
	if (SortedWorkers.IsEmpty())
	{
		OutAssignments.SetNum(Requests.Num());
		for (FWorkerId& Assignment : OutAssignments)
		{
			Assignment = FWorkerId::Local();
		}
		return;
	}
	check(MaxRemoteIndex >= 0);
	SortedWorkers.Sort([](const TRefCountPtr<FCookWorkerServer>& A, const TRefCountPtr<FCookWorkerServer>& B)
		{ return A->GetWorkerId() < B->GetWorkerId(); });

	// Call the LoadBalancing algorithm to split the requests among the LocalWorker and RemoteWorkers
	LoadBalance(SortedWorkers, Requests, MoveTemp(RequestGraph), OutAssignments);

	// Split the output array of WorkerId assignments into a batch for each of the RemoteWorkers 
	TArray<TArray<FPackageData*>> RemoteBatches; // Indexed by WorkerId.GetRemoteIndex()
	TArray<bool> RemoteIndexIsValid; // Indexed by WorkerId.GetRemoteIndex()
	RemoteBatches.SetNum(MaxRemoteIndex+1);
	RemoteIndexIsValid.Init(false, MaxRemoteIndex+1);
	for (FCookWorkerServer* Worker : SortedWorkers)
	{
		RemoteIndexIsValid[Worker->GetWorkerId().GetRemoteIndex()] = true;
	}

	for (int32 RequestIndex = 0; RequestIndex < Requests.Num(); ++RequestIndex)
	{
		FWorkerId WorkerId = OutAssignments[RequestIndex];
		// Override the loadbalancer's assignment if the Package has a WorkerAssignmentConstraint
		// This allows us to guarantee that generated packages will be cooked on the worker that cooked
		// their generator package
		FWorkerId WorkerIdConstraint = Requests[RequestIndex]->GetWorkerAssignmentConstraint();
		if (WorkerIdConstraint.IsValid())
		{
			OutAssignments[RequestIndex] = WorkerIdConstraint;
			WorkerId = WorkerIdConstraint;
		}

		if (!WorkerId.IsLocal())
		{
			uint8 RemoteIndex = WorkerId.GetRemoteIndex();
			check(RemoteIndex < RemoteBatches.Num());
			if (!RemoteIndexIsValid[RemoteIndex])
			{
				UE_LOG(LogCook, Error, TEXT("Package %s can only be cooked by CookWorkerServer %d, but this worker has disconnected. The package can not be cooked."),
					*Requests[RequestIndex]->GetPackageName().ToString(), RemoteIndex);
				OutAssignments[RequestIndex] = FWorkerId::Invalid();
				continue;
			}
			TArray<FPackageData*>& RemoteBatch = RemoteBatches[RemoteIndex];
			if (RemoteBatch.Num() == 0)
			{
				RemoteBatch.Reserve(2 * Requests.Num() / (SortedWorkers.Num() + 1));
			}
			RemoteBatch.Add(Requests[RequestIndex]);
		}
	}

	// MPCOOKTODO: Sort each batch from leaf to root

	// Assign each batch to the FCookWorkerServer in RemoteWorkers;
	// the CookWorkerServer's tick will handle sending the message to the remote process
	for (const TRefCountPtr<FCookWorkerServer>& RemoteWorker : SortedWorkers)
	{
		RemoteWorker->AppendAssignments(RemoteBatches[RemoteWorker->GetWorkerId().GetRemoteIndex()], ECookDirectorThread::SchedulerThread);
	}

	bIsFirstAssignment = false;
}

void FCookDirector::RemoveFromWorker(FPackageData& PackageData)
{
	TArray<FCookWorkerServer*> Workers;
	{
		FScopeLock CommunicationScopeLock(&CommunicationLock);
		for (TPair<int32, TRefCountPtr<FCookWorkerServer>>& Pair : RemoteWorkers)
		{
			Workers.Add(Pair.Value);
		}
	}
	for (FCookWorkerServer* Worker : Workers)
	{
		Worker->AbortAssignment(PackageData, ECookDirectorThread::SchedulerThread);
	}
}

void FCookDirector::TickFromSchedulerThread()
{
	if (!CommunicationThread)
	{
		TickCommunication(ECookDirectorThread::SchedulerThread);
	}

	TArray<TRefCountPtr<FCookWorkerServer>, TInlineAllocator<16>> WorkersWithMessage;
	{
		FScopeLock CommunicationScopeLock(&CommunicationLock);
		for (TPair<int32, TRefCountPtr<FCookWorkerServer>>& Pair : RemoteWorkers)
		{
			if (Pair.Value->HasMessages())
			{
				WorkersWithMessage.Add(Pair.Value);
			}
		}
		for (TPair<FCookWorkerServer*, TRefCountPtr<FCookWorkerServer>>& Pair : ShuttingDownWorkers)
		{
			if (Pair.Value && Pair.Value->HasMessages())
			{
				WorkersWithMessage.Add(Pair.Value);
			}
		}

	}
	bool bIsStalled = COTFS.IsMultiprocessLocalWorkerIdle() && !COTFS.PackageDatas->GetAssignedToWorkerSet().IsEmpty();
	for (TRefCountPtr<FCookWorkerServer>& Worker : WorkersWithMessage)
	{
		Worker->HandleReceiveMessages(ECookDirectorThread::SchedulerThread);
		bIsStalled = false;
	}
	WorkersWithMessage.Empty();

	SetWorkersStalled(bIsStalled);
}

void FCookDirector::TickCommunication(ECookDirectorThread TickThread)
{
	bool bHasShutdownWorkers = false;
	TickWorkerConnects(TickThread);
	TArray<TRefCountPtr<FCookWorkerServer>, TInlineAllocator<16>> LocalRemoteWorkers;
	{
		FScopeLock CommunicationScopeLock(&CommunicationLock);
		for (const TPair<int32, TRefCountPtr<FCookWorkerServer>>& Pair: RemoteWorkers)
		{
			LocalRemoteWorkers.Add(Pair.Value);
		}
		if (!RemoteWorkers.IsEmpty())
		{
			bWorkersActive = true;
		}
		else
		{
			bWorkersActive = false;
			for (const TPair <FCookWorkerServer*, TRefCountPtr<FCookWorkerServer>>& Pair : ShuttingDownWorkers)
			{
				FCookWorkerServer* RemoteWorker = Pair.Key;
				check(RemoteWorker->IsShuttingDown());
				bWorkersActive = bWorkersActive || RemoteWorker->IsFlushingBeforeShutdown();
			}
		}
		bHasShutdownWorkers = !ShuttingDownWorkers.IsEmpty();
	}

	for (TRefCountPtr<FCookWorkerServer>& RemoteWorker: LocalRemoteWorkers)
	{
		RemoteWorker->TickCommunication(TickThread);
		if (RemoteWorker->IsShuttingDown())
		{
			FScopeLock CommunicationScopeLock(&CommunicationLock);
			TRefCountPtr<FCookWorkerServer>& Existing = ShuttingDownWorkers.FindOrAdd(RemoteWorker.GetReference());
			check(!Existing); // We should not be able to send the same pointer into ShuttingDown twice
			bHasShutdownWorkers = true;
		}
	}

	if (bHasShutdownWorkers)
	{
		TickWorkerShutdowns(TickThread);
	}
}

void FCookDirector::PumpCookComplete(bool& bCompleted)
{
	{
		FScopeLock CommunicationScopeLock(&CommunicationLock);
		if (!bCookCompleteSent)
		{
			bool bAllIdle = true;
			for (TPair<int32, TRefCountPtr<FCookWorkerServer>>& Pair : RemoteWorkers)
			{
				FCookWorkerServer& RemoteWorker = *Pair.Value;
				// MPCOOKTODO: Messages sent from the CookWorkers about discovered packages might come in after
				// the last message sent about completed saves. These discovered packages can cause the cook to fall
				// back to incomplete. Don't send CookComplete messages to the CookWorkers until all CookWorkers have
				// reported they are idle and have no further messages to send.
				if (RemoteWorker.HasAssignments())
				{
					bAllIdle = false;
					break;
				}
			}
			if (bAllIdle)
			{
				for (TPair<int32, TRefCountPtr<FCookWorkerServer>>& Pair : RemoteWorkers)
				{
					FCookWorkerServer& RemoteWorker = *Pair.Value;
					RemoteWorker.SignalCookComplete(ECookDirectorThread::SchedulerThread);
					check(RemoteWorker.IsShuttingDown());
				}
				bCookCompleteSent = true;
			}
		}
		bCompleted = !bWorkersActive;
	}
	TickFromSchedulerThread();
}

void FCookDirector::ShutdownCookSession()
{
	StopCommunicationThread();

	// Cancel any inprogress workers and move them to the Shutdown list
	for (;;)
	{
		TRefCountPtr<FCookWorkerServer> RemoteWorker;
		{
			FScopeLock CommunicationScopeLock(&CommunicationLock);
			if (RemoteWorkers.IsEmpty())
			{
				break;
			}
			RemoteWorker = TMap<int32, TRefCountPtr<FCookWorkerServer>>::TIterator(RemoteWorkers).Value();
		}
		AbortWorker(RemoteWorker->GetWorkerId(), ECookDirectorThread::SchedulerThread);
	}

	// Immediately shutdown any gracefully shutting down workers
	TArray<TRefCountPtr<FCookWorkerServer>, TInlineAllocator<16>> WorkersNeedingAbort;
	{
		FScopeLock CommunicationScopeLock(&CommunicationLock);
		for (TPair<FCookWorkerServer*, TRefCountPtr<FCookWorkerServer>>& Pair : ShuttingDownWorkers)
		{
			check(Pair.Value); // The Value was set by AbortWorker for any new entries, and all old entries guarantee the value is set
			if (Pair.Key->IsFlushingBeforeShutdown())
			{
				WorkersNeedingAbort.Add(Pair.Value);
			}
		}
	}
	for (TRefCountPtr<FCookWorkerServer>& RemoteWorker : WorkersNeedingAbort)
	{
		TSet<FPackageData*> UnusedPendingPackages;
		RemoteWorker->AbortWorker(UnusedPendingPackages, ECookDirectorThread::SchedulerThread);
	}

	// Wait for all the shutdowns to complete
	for (;;)
	{
		TickWorkerShutdowns(ECookDirectorThread::SchedulerThread);
		{
			FScopeLock CommunicationScopeLock(&CommunicationLock);
			if (ShuttingDownWorkers.IsEmpty())
			{
				break;
			}
		}
		constexpr float SleepSeconds = 0.010f;
		FPlatformProcess::Sleep(SleepSeconds);
	}

	// Kill any connections that had just been made and not yet assigned to a Server
	PendingConnections.Reset();

	// Restore the FCookDirector to its original state so that it is ready for a new session
	bWorkersInitialized = false;
	bIsFirstAssignment = true;
	bCookCompleteSent = false;
	bWorkersActive = false;
}

void FCookDirector::Register(IMPCollector* Collector)
{
	TRefCountPtr<IMPCollector>& Existing = MessageHandlers.FindOrAdd(Collector->GetMessageType());
	if (Existing)
	{
		UE_LOG(LogCook, Error, TEXT("Duplicate IMPCollectors registered. Guid: %s, Existing: %s, Registering: %s. Keeping the Existing."),
			*Collector->GetMessageType().ToString(), Existing->GetDebugName(), Collector->GetDebugName());
		return;
	}
	Existing = Collector;
}

void FCookDirector::Unregister(IMPCollector* Collector)
{
	TRefCountPtr<IMPCollector> Existing;
	MessageHandlers.RemoveAndCopyValue(Collector->GetMessageType(), Existing);
	if (Existing && Existing.GetReference() != Collector)
	{
		UE_LOG(LogCook, Error, TEXT("Duplicate IMPCollector during Unregister. Guid: %s, Existing: %s, Unregistering: %s. Ignoring the Unregister."),
			*Collector->GetMessageType().ToString(), Existing->GetDebugName(), Collector->GetDebugName());
		MessageHandlers.Add(Collector->GetMessageType(), MoveTemp(Existing));
	}
}

void FCookDirector::SetWorkersStalled(bool bInWorkersStalled)
{
	if (bInWorkersStalled != bWorkersStalled)
	{
		bWorkersStalled = bInWorkersStalled;
		if (bWorkersStalled)
		{
			const double CurrentTime = FPlatformTime::Seconds();
			WorkersStalledStartTimeSeconds = CurrentTime;
			WorkersStalledWarnTimeSeconds = CurrentTime + GCookProgressWarnBusyTime;
		}
		else
		{
			WorkersStalledStartTimeSeconds = MAX_flt;
			WorkersStalledWarnTimeSeconds = MAX_flt;
		}
	}
	else if (bWorkersStalled)
	{
		const double CurrentTime = FPlatformTime::Seconds();
		if (CurrentTime >= WorkersStalledWarnTimeSeconds)
		{
			UE_LOG(LogCook, Warning, TEXT("Cooker has been blocked with no results from remote CookWorkers for %.0f seconds."),
				(float)(CurrentTime - WorkersStalledStartTimeSeconds));
			WorkersStalledWarnTimeSeconds = CurrentTime + GCookProgressWarnBusyTime;
		}
	}
}

FCookDirector::FPendingConnection::FPendingConnection(FPendingConnection&& Other)
{
	Swap(Socket, Other.Socket);
	Buffer = MoveTemp(Other.Buffer);
}

FCookDirector::FPendingConnection::~FPendingConnection()
{
	Sockets::CloseSocket(Socket);
}

FSocket* FCookDirector::FPendingConnection::DetachSocket()
{
	FSocket* Result = Socket;
	Socket = nullptr;
	return Result;
}

void FWorkerConnectMessage::Write(FCbWriter& Writer) const
{
	Writer << "RemoteIndex" << RemoteIndex;
}

bool FWorkerConnectMessage::TryRead(FCbObject&& Object)
{
	RemoteIndex = Object["RemoteIndex"].AsInt32(-1);
	return RemoteIndex >= 0;
}

FGuid FWorkerConnectMessage::MessageType(TEXT("302096E887DA48F7B079FAFAD0EE5695"));

bool FCookDirector::TryCreateWorkerConnectSocket()
{
	if (WorkerConnectSocket)
	{
		return true;
	}
	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get();
	if (!SocketSubsystem)
	{
		// Error was already logged in the constructor
		return false;
	}

	FString ErrorReason;
	TSharedPtr<FInternetAddr> ListenAddr;
	WorkerConnectSocket = Sockets::CreateListenSocket(WorkerConnectPort, ListenAddr, WorkerConnectAuthority,
		TEXT("FCookDirector-WorkerConnect"), ErrorReason);
	if (!WorkerConnectSocket)
	{
		UE_LOG(LogCook, Error, TEXT("CookDirector could not create listen socket, CookWorkers will be disabled. Reason: %s."),
			*ErrorReason);
		return false;
	}
	return true;
}


void FCookDirector::InitializeWorkers()
{
	if (bWorkersInitialized)
	{
		return;
	}
	bWorkersInitialized = true;

	check(!CommunicationThread);
	check(RemoteWorkers.IsEmpty());
	bool bSucceeded = false;
	ON_SCOPE_EXIT
	{
		if (!bSucceeded)
		{
			bWorkersActive = false;
		}
	};

	if (!TryCreateWorkerConnectSocket())
	{
		return;
	}

	RemoteWorkers.Reserve(RequestedCookWorkerCount);
	for (int32 RemoteIndex = 0; RemoteIndex < RequestedCookWorkerCount; ++RemoteIndex)
	{
		RemoteWorkers.Add(RemoteIndex, new FCookWorkerServer(*this, FWorkerId::FromRemoteIndex(RemoteIndex)));
	}
	bWorkersActive = true;

	ConstructReadonlyThreadVariables();
	ShutdownEvent->Reset();
	LaunchCommunicationThread();
	bSucceeded = true;
}

void FCookDirector::RecreateWorkers()
{
	// TODO: Finish implementing the recreation of workers that have crashed

	// Find any unused RemoteIndex less than the maximum used RemoteIndex
	FScopeLock CommunicationScopeLock(&CommunicationLock);
	if (RemoteWorkers.Num() >= RequestedCookWorkerCount || !WorkerConnectSocket)
	{
		return;
	}

	TArray<uint8> UnusedRemoteIndexes;
	RemoteWorkers.KeySort(TLess<>());
	uint8 NextPossiblyOpenIndex = 0;
	for (TPair<int32, TRefCountPtr<FCookWorkerServer>>& Pair : RemoteWorkers)
	{
		check(NextPossiblyOpenIndex <= Pair.Key);
		while (NextPossiblyOpenIndex != Pair.Key)
		{
			UnusedRemoteIndexes.Add(NextPossiblyOpenIndex++);
		}
	}

	// Add RemoteWorkers, pulling the RemoteIndex id from the UnusedRemoteIndexes if any exist
	// otherwise use the next integer because all indexes up to RemoteWorkers.Num() are in use.
	while (RemoteWorkers.Num() < RequestedCookWorkerCount)
	{
		uint8 RemoteIndex;
		if (UnusedRemoteIndexes.Num())
		{
			RemoteIndex = UnusedRemoteIndexes[0];
			UnusedRemoteIndexes.RemoveAtSwap(0);
		}
		else
		{
			RemoteIndex = RemoteWorkers.Num();
		}
		RemoteWorkers.Add(RemoteIndex, new FCookWorkerServer(*this, FWorkerId::FromRemoteIndex(RemoteIndex)));
		bWorkersActive = true;
	}

}
void FCookDirector::ActivateMachineResourceReduction()
{
	if (bHasReducedMachineResources)
	{
		return;
	}
	bHasReducedMachineResources = true;

	// Add MemoryInFree if it's not already set
	if (COTFS.MemoryMinFreeVirtual > 0 || COTFS.MemoryMinFreePhysical > 0)
	{
		// When running a multiprocess cook, we remove the MemoryMaxUsed triggers and allow GCing based solely on MemoryMinFree
		COTFS.MemoryMaxUsedVirtual = 0;
		COTFS.MemoryMaxUsedPhysical = 0;
	}
	else
	{
		// If MemoryMinFree is not set, then keep MemoryMaxUsed but reduce it by the number of CookWorkers.
		constexpr float FixedOverheadFraction = 0.10f;
		float TargetRatio = (FixedOverheadFraction + (1 - FixedOverheadFraction) / RequestedCookWorkerCount);
		COTFS.MemoryMaxUsedPhysical = TargetRatio * COTFS.MemoryMaxUsedPhysical;
		COTFS.MemoryMaxUsedVirtual = TargetRatio * COTFS.MemoryMaxUsedVirtual;
	}

	UE_LOG(LogCook, Display, TEXT("CookMultiprocess changed CookSettings for Memory: MemoryMaxUsedVirtual %dMiB, MemoryMaxUsedPhysical %dMiB,")
		TEXT("MemoryMinFreeVirtual % dMiB, MemoryMinFreePhysical % dMiB"),
		COTFS.MemoryMaxUsedVirtual / 1024 / 1024, COTFS.MemoryMaxUsedPhysical / 1024 / 1024,
		COTFS.MemoryMinFreeVirtual / 1024 / 1024, COTFS.MemoryMinFreePhysical / 1024 / 1024);

	// Set CoreLimit for updating workerthreads in this process and passing to the commandline for workers
	int32 NumProcesses = RequestedCookWorkerCount + 1;
	int32 NumberOfCores = FPlatformMisc::NumberOfCores();
	int32 HyperThreadCount = FPlatformMisc::NumberOfCoresIncludingHyperthreads();
	int32 NumberOfHyperThreadsPerCore = HyperThreadCount / NumberOfCores;
	CoreLimit = FMath::Max(NumberOfCores / NumProcesses, 1);
	int32 CoreIncludingHyperthreadsLimit = CoreLimit * NumberOfHyperThreadsPerCore;
	int32 NumberOfWorkers = FMath::Max(CoreLimit - 1, 1) * NumberOfHyperThreadsPerCore;

	// Update the number of Cores and WorkerThreads for this process
	check(IsInGameThread());
	int32 NumBackgroundWorkers = FMath::Max(1, NumberOfWorkers - FMath::Min<int32>(GNumForegroundWorkers, NumberOfWorkers));
	int32 NumForegroundWorkers = FMath::Max(1, NumberOfWorkers - NumBackgroundWorkers);
	LowLevelTasks::FScheduler::Get().RestartWorkers(NumForegroundWorkers, NumBackgroundWorkers);

	// Update the number of ShaderCompilerWorkers that can be launched
	GShaderCompilingManager->OnMachineResourcesChanged(CoreLimit, CoreIncludingHyperthreadsLimit);

	UE_LOG(LogCook, Display, TEXT("CookMultiprocess changed number of cores from %d to %d."),
		NumberOfCores, FPlatformMisc::NumberOfCores());
	UE_LOG(LogCook, Display, TEXT("CookMultiprocess changed number of hyperthreads from %d to %d."),
		HyperThreadCount, FPlatformMisc::NumberOfCoresIncludingHyperthreads());
}

void FCookDirector::TickWorkerConnects(ECookDirectorThread TickThread)
{
	using namespace UE::CompactBinaryTCP;

	if (!WorkerConnectSocket)
	{
		return;
	}

	bool bReadReady;
	while (WorkerConnectSocket->HasPendingConnection(bReadReady) && bReadReady)
	{
		FSocket* WorkerSocket = WorkerConnectSocket->Accept(TEXT("Client Connection"));
		if (!WorkerSocket)
		{
			UE_LOG(LogCook, Warning, TEXT("Pending connection failed to create a ClientSocket."));
		}
		else
		{
			WorkerSocket->SetNonBlocking(true);
			PendingConnections.Add(FPendingConnection(WorkerSocket));
		}
	}

	for (TArray<FPendingConnection>::TIterator Iter(PendingConnections); Iter; ++Iter)
	{
		FPendingConnection& Conn = *Iter;
		TArray<FMarshalledMessage> Messages;
		EConnectionStatus Status;
		Status = TryReadPacket(Conn.Socket, Conn.Buffer, Messages);
		if (Status != EConnectionStatus::Okay)
		{
			UE_LOG(LogCook, Warning, TEXT("Pending connection failed before sending a WorkerPacket: %s"), DescribeStatus(Status));
			Iter.RemoveCurrent();
		}
		if (Messages.Num() == 0)
		{
			continue;
		}
		FPendingConnection LocalConn(MoveTemp(Conn));
		Iter.RemoveCurrent();

		if (Messages[0].MessageType != FWorkerConnectMessage::MessageType)
		{
			UE_LOG(LogCook, Warning, TEXT("Pending connection sent a different message before sending a connection message. MessageType: %s. Connection will be ignored."),
				*Messages[0].MessageType.ToString());
			continue;
		}
		FWorkerConnectMessage Message;
		if (!Message.TryRead(MoveTemp(Messages[0].Object)))
		{
			UE_LOG(LogCook, Warning, TEXT("Pending connection sent an invalid Connection Message. Connection will be ignored."));
			continue;
		}
		TRefCountPtr<FCookWorkerServer> RemoteWorker;
		{
			FScopeLock CommunicationScopeLock(&CommunicationLock);
			TRefCountPtr<FCookWorkerServer>* RemoteWorkerPtr;
			RemoteWorkerPtr = RemoteWorkers.Find(Message.RemoteIndex);
			if (!RemoteWorkerPtr)
			{
				TStringBuilder<256> ValidIndexes;
				if (RemoteWorkers.Num())
				{
					RemoteWorkers.KeySort(TLess<>());
					for (TPair<int32, TRefCountPtr<FCookWorkerServer>>& Pair : RemoteWorkers)
					{
						ValidIndexes.Appendf(TEXT("%d,"), Pair.Key);
					}
					ValidIndexes.RemoveSuffix(1); // Remove the terminating comma
				}
				UE_LOG(LogCook, Warning, TEXT("Pending connection sent a Connection Message with invalid RemoteIndex %d. ValidIndexes = {%s}. Connection will be ignored."),
					Message.RemoteIndex, *ValidIndexes);
				continue;
			}
			RemoteWorker = *RemoteWorkerPtr;
		}

		FSocket* LocalSocket = LocalConn.DetachSocket();
		Messages.RemoveAt(0);
		if (!RemoteWorker->TryHandleConnectMessage(Message, LocalSocket, MoveTemp(Messages), TickThread))
		{
			UE_LOG(LogCook, Warning, TEXT("Pending connection sent a Connection Message with an already in-use RemoteIndex. Connection will be ignored."));
			Sockets::CloseSocket(LocalSocket);
			continue;
		}
	}
}

void FCookDirector::TickWorkerShutdowns(ECookDirectorThread TickThread)
{
	// Move any newly shutting down workers from RemoteWorkers
	TArray<TRefCountPtr<FCookWorkerServer>> NewShutdowns;
	TArray<TRefCountPtr<FCookWorkerServer>, TInlineAllocator<16>> LocalRemoteWorkers;
	{
		FScopeLock RemoteWorkersScopeLock(&CommunicationLock);
		for (TPair<FCookWorkerServer*, TRefCountPtr<FCookWorkerServer>>& Pair : ShuttingDownWorkers)
		{
			if (!Pair.Value)
			{
				NewShutdowns.Emplace(Pair.Key);
			}
			else
			{
				LocalRemoteWorkers.Add(Pair.Value);
			}
		}
	}
	if (!NewShutdowns.IsEmpty())
	{
		for (FCookWorkerServer* NewShutdown : NewShutdowns)
		{
			AbortWorker(NewShutdown->GetWorkerId(), TickThread);
			{
				FScopeLock CommunicationScopeLock(&CommunicationLock);
				check(ShuttingDownWorkers.FindOrAdd(NewShutdown).IsValid()); // Abort worker should have set the value
			}
			LocalRemoteWorkers.Emplace(NewShutdown);
		}
	}

	TArray<TRefCountPtr<FCookWorkerServer>, TInlineAllocator<16>> CompletedWorkers;
	for (TRefCountPtr<FCookWorkerServer>& RemoteWorker : LocalRemoteWorkers)
	{
		RemoteWorker->TickCommunication(TickThread);
		if (RemoteWorker->IsShutdownComplete())
		{
			CompletedWorkers.Add(RemoteWorker);
		}
	}
	LocalRemoteWorkers.Empty();

	if (!CompletedWorkers.IsEmpty())
	{
		FScopeLock CommunicationScopeLock(&CommunicationLock);
		for (TRefCountPtr<FCookWorkerServer>& CompletedWorker : CompletedWorkers)
		{
			ShuttingDownWorkers.Remove(CompletedWorker.GetReference());
		}
	}
	CompletedWorkers.Empty();
}

FString FCookDirector::GetWorkerCommandLine(FWorkerId WorkerId)
{
	FString CommandLine = FCommandLine::Get();

	const TCHAR* ProjectName = FApp::GetProjectName();
	checkf(ProjectName && ProjectName[0], TEXT("Expected UnrealEditor to be running with a non-empty project name"));
	TArray<FString> Tokens;
	UE::String::ParseTokensMultiple(CommandLine, { ' ', '\t', '\r', '\n' }, [&Tokens](FStringView Token)
		{
			if (Token.StartsWith(TEXT("-run=")) ||
				Token == TEXT("-CookOnTheFly") ||
				Token == TEXT("-CookWorker") ||
				Token == TEXT("-CookMultiProcess") ||
				Token == TEXT("-CookSingleProcess") ||
				Token.StartsWith(TEXT("-TargetPlatform")) ||
				Token.StartsWith(TEXT("-CookCultures")) ||
				Token.StartsWith(TEXT("-CookDirectorCount=")) ||
				Token.StartsWith(TEXT("-CookDirectorHost=")) ||
				Token.StartsWith(TEXT("-CookWorkerId=")) ||
				Token.StartsWith(TEXT("-ShowCookWorker")) ||
				Token.StartsWith(TEXT("-CoreLimit")) ||
				Token.StartsWith(TEXT("-PhysicalCoreLimit")) ||
				Token.StartsWith(TEXT("-CoreLimitHyperThreads"))
				)
			{
				return;
			}
			Tokens.Add(FString(Token));
		}, UE::String::EParseTokensOptions::SkipEmpty);
	if (Tokens[0] != ProjectName)
	{
		Tokens.Insert(ProjectName, 0);
	}
	Tokens.Insert(TEXT("-run=cook"), 1);
	Tokens.Insert(TEXT("-cookworker"), 2);
	check(!WorkerConnectAuthority.IsEmpty()); // This should have been constructed in TryCreateWorkerConnectSocket before any CookWorkerServers could exist to call GetWorkerCommandLine
	Tokens.Add(FString::Printf(TEXT("-CookDirectorHost=%s"), *WorkerConnectAuthority));
	Tokens.Add(FString::Printf(TEXT("-CookWorkerId=%d"), WorkerId.GetRemoteIndex()));
	if (CoreLimit > 0)
	{
		Tokens.Add(FString::Printf(TEXT("-PhysicalCoreLimit=%d"), CoreLimit));
	}

	return FString::Join(Tokens, TEXT(" "));
}

bool FDirectorConnectionInfo::TryParseCommandLine()
{
	if (!FParse::Value(FCommandLine::Get(), TEXT("-CookDirectorHost="), HostURI))
	{
		UE_LOG(LogCook, Error, TEXT("CookWorker startup failed: no CookDirector specified on commandline."));
		return false;
	}
	if (!FParse::Value(FCommandLine::Get(), TEXT("-CookWorkerId="), RemoteIndex))
	{
		UE_LOG(LogCook, Error, TEXT("CookWorker startup failed: no CookWorkerId specified on commandline."));
		return false;
	}
	return true;
}

void FCookDirector::LoadBalance(TConstArrayView<TRefCountPtr<FCookWorkerServer>> SortedWorkers, TArrayView<FPackageData*> Requests,
	TMap<FPackageData*, TArray<FPackageData*>>&& RequestGraph, TArray<FWorkerId>& OutAssignments)
{
	TArray<FWorkerId> AllWorkers;
	int32 NumAllWorkers = SortedWorkers.Num() + 1;
	AllWorkers.Reserve(NumAllWorkers);
	AllWorkers.Add(FWorkerId::Local());
	for (FCookWorkerServer* Worker : SortedWorkers)
	{
		AllWorkers.Add(Worker->GetWorkerId());
	}
	OutAssignments.Reset(Requests.Num());
	bool bLogResults = bIsFirstAssignment;

	switch (LoadBalanceAlgorithm)
	{
	case ELoadBalanceAlgorithm::Striped:
		return LoadBalanceStriped(AllWorkers, Requests, MoveTemp(RequestGraph), OutAssignments, bLogResults);
	case ELoadBalanceAlgorithm::CookBurden:
		return LoadBalanceCookBurden(AllWorkers, Requests, MoveTemp(RequestGraph), OutAssignments, bLogResults);
	}
	checkNoEntry();
	return LoadBalanceCookBurden(AllWorkers, Requests, MoveTemp(RequestGraph), OutAssignments, bLogResults);
}

void FCookDirector::AbortWorker(FWorkerId WorkerId, ECookDirectorThread TickThread)
{
	check(!WorkerId.IsLocal());
	int32 Index = WorkerId.GetRemoteIndex();
	TRefCountPtr<FCookWorkerServer> RemoteWorker;
	{
		FScopeLock RemoteWorkersScopeLock(&CommunicationLock);
		RemoteWorkers.RemoveAndCopyValue(Index, RemoteWorker);
		if (!RemoteWorker)
		{
			return;
		}
	}
	TSet<FPackageData*> PackagesToReassign;
	RemoteWorker->AbortAssignments(PackagesToReassign, TickThread);
	if (!RemoteWorker->IsShuttingDown())
	{
		RemoteWorker->AbortWorker(PackagesToReassign, TickThread);
	}
	for (FPackageData* PackageData : PackagesToReassign)
	{
		check(PackageData->IsInProgress()); // Packages that were assigned to a worker should be in the AssignedToWorker state
		PackageData->SetWorkerAssignment(FWorkerId::Invalid());
		PackageData->SendToState(UE::Cook::EPackageState::Request, ESendFlags::QueueAddAndRemove);
	}
	{
		FScopeLock RemoteWorkersScopeLock(&CommunicationLock);
		TRefCountPtr<FCookWorkerServer>& Existing = ShuttingDownWorkers.FindOrAdd(RemoteWorker.GetReference());
		check(!Existing); // We should not be able to abort a worker twice because we removed it from RemoteWorkers above
		Existing = MoveTemp(RemoteWorker);
	}
}

void FCookDirector::ConstructReadonlyThreadVariables()
{
	IsCookIgnoreTimeouts(); // The global variables are read-only; call them now to initialize them
	CommandletExecutablePath = FUnrealEdMisc::Get().GetProjectEditorBinaryPath();

	InitialConfigMessage = MakeUnique<FInitialConfigMessage>();
	const TArray<const ITargetPlatform*>& SessionPlatforms = COTFS.PlatformManager->GetSessionPlatforms();
	TArray<ITargetPlatform*> OrderedSessionPlatforms;
	OrderedSessionPlatforms.Reset(SessionPlatforms.Num());
	for (const ITargetPlatform* TargetPlatform : SessionPlatforms)
	{
		OrderedSessionPlatforms.Add(const_cast<ITargetPlatform*>(TargetPlatform));
	}
	InitialConfigMessage->ReadFromLocal(COTFS, OrderedSessionPlatforms,
		*COTFS.CookByTheBookOptions, *COTFS.CookOnTheFlyOptions, BeginCookContext);
}

const FInitialConfigMessage& FCookDirector::GetInitialConfigMessage()
{
	return *InitialConfigMessage;
}

FCookDirector::FLaunchInfo FCookDirector::GetLaunchInfo(FWorkerId WorkerId)
{
	FLaunchInfo Info;
	Info.ShowWorkerOption = GetShowWorkerOption();
	Info.CommandletExecutable = CommandletExecutablePath;
	Info.WorkerCommandLine = GetWorkerCommandLine(WorkerId);
	return Info;
}


}
