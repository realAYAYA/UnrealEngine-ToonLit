// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaHordeAgentManager.h"
#include "HAL/Event.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Storage/StorageClient.h"
#include "Storage/Clients/BundleStorageClient.h"
#include "Storage/Clients/MemoryStorageClient.h"
#include "Storage/Clients/FileStorageClient.h"
#include "Storage/Nodes/ChunkNode.h"
#include "Storage/Nodes/DirectoryNode.h"
#include "Storage/BlobWriter.h"
#include "UbaControllerModule.h"
#include "UbaExports.h"
#include <filesystem>
#include <fstream>

namespace UbaControllerModule
{
	static bool bHordeForwardAgentLogs = false;
	static FAutoConsoleVariableRef CVarUbaControllerHordeForwardAgentLogs(
		TEXT("r.UbaController.HordeForwardAgentLogs"),
		bHordeForwardAgentLogs,
		TEXT("Enables or disables the use of UBA as controller for distributed help in the engine.\n")
		TEXT("false: UBA will not be used as controller \n")
		TEXT("true: Distribute builds using UBA (default) is no other controller is available."),
		ECVF_ReadOnly); // Must be set on start-up, e.g. via config ini
}

FUbaHordeAgentManager::FUbaHordeAgentManager(const FString& InWorkingDir, uba::NetworkServer* InServer)
	:	WorkingDir(InWorkingDir)
	,	UbaServer(InServer)
	,	LastRequestFailTime(1)
	,	TargetCoreCount(0)
	,	EstimatedCoreCount(0)
	,	AskForAgents(true)
{
	ParseConfig();
}

FUbaHordeAgentManager::~FUbaHordeAgentManager()
{
	FScopeLock AgentsScopeLock(&AgentsLock);
	for (TUniquePtr<FHordeAgentWrapper>& Agent : Agents)
	{
		Agent->ShouldExit->Trigger();
		Agent->Thread.Join();
		FGenericPlatformProcess::ReturnSynchEventToPool(Agent->ShouldExit);
	}
}

void FUbaHordeAgentManager::SetTargetCoreCount(uint32 Count)
{
	TargetCoreCount = FMath::Min(MaxCores, Count);

	if (Url.IsEmpty())
	{
		AskForAgents = false;
	}

	while (EstimatedCoreCount < TargetCoreCount)
	{
		if (!AskForAgents)
		{
			return;
		}

		//UE_LOG(LogUbaController, Display, TEXT("Requested new agent. Estimated core count: %u, Target core count: %u"), EstimatedCoreCount.Load(), TargetCoreCount.Load());
		RequestAgent();
	}

	FScopeLock AgentsScopeLock(&AgentsLock);
	for (auto Iterator = Agents.CreateIterator(); Iterator; ++Iterator)
	{
		TUniquePtr<FHordeAgentWrapper>& Agent = *Iterator;
		if (Agent->ShouldExit->Wait(0))
		{
			Agent->Thread.Join();
			FGenericPlatformProcess::ReturnSynchEventToPool(Agent->ShouldExit);
			Iterator.RemoveCurrentSwap();
		}
	}
}

// Creates a bundle blob (one of several chunks of a file) to be uploaded to Horde
// This code has been adopted from the HordeTest project.
// See 'Engine/Source/Programs/Horde/Samples/HordeTest/Main.cpp'.
static FBlobHandleWithHash CreateHordeBundleBlob(std::ifstream& Stream, FBlobWriter& Writer, int64& OutLength, FIoHash& OutStreamHash)
{
	OutLength = 0;

	FChunkNodeWriter ChunkWriter(Writer);

	char ReadBuffer[4096];
	while (!Stream.eof())
	{
		Stream.read(ReadBuffer, sizeof(ReadBuffer));

		const int64 ReadSize = static_cast<int64>(Stream.gcount());
		if (ReadSize == 0)
		{
			break;
		}
		OutLength += ReadSize;

		ChunkWriter.Write(FMemoryView(ReadBuffer, ReadSize));
	}

	return ChunkWriter.Flush(OutStreamHash);
}

static FFileEntry CreateHordeBundleFileEntry(const std::filesystem::path& Path, FBlobWriter& Writer)
{
	std::ifstream InputStream(Path, std::ios::binary);
	check(InputStream.good());

	int64 Length = 0;
	FIoHash StreamHash;
	FBlobHandleWithHash Target = CreateHordeBundleBlob(InputStream, Writer, Length, StreamHash);

	return FFileEntry(Target, FUtf8String(Path.filename().string().c_str()), EFileEntryFlags::None, Length, StreamHash, FSharedBufferView());
}

static FDirectoryEntry CreateHordeBundleDirectoryEntry(const std::filesystem::path& Path, FBlobWriter& Writer)
{
	FDirectoryNode DirectoryNode;

	FFileEntry NewEntry = CreateHordeBundleFileEntry(Path, Writer);
	const FUtf8String Name = NewEntry.Name;
	const int64 Length = NewEntry.Length;
	DirectoryNode.NameToFile.Add(Name, MoveTemp(NewEntry));

	FBlobHandle DirectoryHandle = DirectoryNode.Write(Writer);

	return FDirectoryEntry(DirectoryHandle, FIoHash(), FUtf8String(Path.filename().string().c_str()), Length);
}

bool CreateHordeBundleFromFile(const std::filesystem::path& InputFilename, const std::filesystem::path& OutputFilename)
{
	TSharedRef<FFileStorageClient> FileStorage = MakeShared<FFileStorageClient>(OutputFilename.parent_path());
	TSharedRef<FBundleStorageClient> Storage = MakeShared<FBundleStorageClient>(FileStorage);

	TUniquePtr<FBlobWriter> Writer = Storage->CreateWriter("");
	FDirectoryEntry RootEntry = CreateHordeBundleDirectoryEntry(InputFilename, *Writer.Get());
	Writer->Flush();

	FFileStorageClient::WriteRefToFile(OutputFilename, RootEntry.Target->GetLocator());
	return true;
}

FString GetUbaBinariesPath();

void FUbaHordeAgentManager::RequestAgent()
{
	EstimatedCoreCount += 32; // We estimate a typical agent to have 32 cores

	FScopeLock AgentsScopeLock(&AgentsLock);
	FHordeAgentWrapper& Wrapper = *Agents.Emplace_GetRef(MakeUnique<FHordeAgentWrapper>());

	Wrapper.ShouldExit = FGenericPlatformProcess::GetSynchEventFromPool(true);
	Wrapper.Thread = FThread(TEXT("HordeAgent"), [this, WrapperPtr = &Wrapper]() { ThreadAgent(*WrapperPtr); });
}

void FUbaHordeAgentManager::ThreadAgent(FHordeAgentWrapper& Wrapper)
{
	FEvent& ShouldExit = *Wrapper.ShouldExit;
	TUniquePtr<FUbaHordeAgent> Agent;
	bool bSuccess = false;

	ON_SCOPE_EXIT
	{
		if (Agent)
		{
			Agent->CloseConnection();
		}

		ShouldExit.Trigger();
	};

	int MachineCoreCount = 0;

	{
		ON_SCOPE_EXIT{ EstimatedCoreCount -= 32; };

		FScopeLock ScopeLock(&UbaAgentBundleFilePathLock);
		if (UbaAgentBundleFilePath.IsEmpty())
		{
			const FString UbaAgentFilePath = FPaths::Combine(GetUbaBinariesPath(), TEXT("UbaAgent.exe"));
			UbaAgentBundleFilePath = FPaths::Combine(WorkingDir, TEXT("UbaAgent.Bundle.ref"));

			if (!CreateHordeBundleFromFile(*UbaAgentFilePath, *UbaAgentBundleFilePath))
			{
				UE_LOG(LogUbaController, Error, TEXT("Failed to create Horde bundle for UbaAgent executable: %s"), *UbaAgentFilePath);
				AskForAgents = false;
				return;
			}
			UE_LOG(LogUbaController, Display, TEXT("Created Horde bundle for UbaAgent executable: %s"), *UbaAgentFilePath);
		}

		if (!HordeMetaClient)
		{
			// Create Horde meta client right before we need it to make sure the CVar for the server URL has been read by now
			HordeMetaClient = MakeUnique<FUbaHordeMetaClient>(Url, Oidc);
			if (!HordeMetaClient->RefreshHttpClient())
			{
				UE_LOG(LogUbaController, Error, TEXT("Failed to create HttpClient for UbaAgent"));
				AskForAgents = false;
				return;
			}
		}

		if (!AskForAgents)
		{
			return;
		}

		if (LastRequestFailTime == 0)
		{
			ScopeLock.Unlock();
		}
		else
		{
			// Try to reduce pressure on horde by not asking for machines more frequent than every 5 seconds if failed to retrieve last time
			uint64 CurrentTime = FPlatformTime::Cycles64();
			uint32 MsSinceLastFail = uint32((CurrentTime - LastRequestFailTime) * FPlatformTime::GetSecondsPerCycle() * 1000);
			if (MsSinceLastFail < 5000)
			{
				if (ShouldExit.Wait(5000 - MsSinceLastFail))
				{
					return;
				}
			}
		}

		TSharedPtr<FUbaHordeMetaClient::HordeMachinePromise, ESPMode::ThreadSafe> Promise = HordeMetaClient->RequestMachine(Pool);
		if (!Promise)
		{
			//UE_LOG(LogUbaController, Error, TEXT("Failed to create Horde bundle for UbaAgent executable: %s"), *UbaAgentFilePath);
			return;
		}
		TFuture<TTuple<FHttpResponsePtr, FHordeRemoteMachineInfo>> Future = Promise->GetFuture();
		Future.Wait();
		FHordeRemoteMachineInfo MachineInfo = Future.Get().Value;

		// If the machine couldn't be assigned, just ignore this agent slot
		if (MachineInfo.Ip == TEXT(""))
		{
			if (!LastRequestFailTime)
			{
				UE_LOG(LogUbaHorde, Verbose, TEXT("No resources available in Horde. Will keep retrying until %u cores are used (Currently have %u)"), TargetCoreCount.Load(), ActiveCoreCount.Load());
			}
			LastRequestFailTime = FPlatformTime::Cycles64();
			return;
		}

		LastRequestFailTime = 0;

		ScopeLock.Unlock();

		if (ShouldExit.Wait(0))
		{
			return;
		}

		Agent = MakeUnique<FUbaHordeAgent>(MachineInfo);

		if (!Agent->BeginCommunication())
		{
			return;
		}

		TArray<uint8> Locator;
		if (!FFileHelper::LoadFileToArray(Locator, *UbaAgentBundleFilePath))
		{
			UE_LOG(LogUbaController, Error, TEXT("Cannot launch Horde processes for UBA controller because bundle path could not be found: %s"), *UbaAgentBundleFilePath);
			return;
		}

		Locator.Add('\0');

		FString BundleDirectory = FPaths::GetPath(UbaAgentBundleFilePath);

		if (ShouldExit.Wait(0))
		{
			return;
		}

		if (!Agent->UploadBinaries(BundleDirectory, reinterpret_cast<const char*>(Locator.GetData())))
		{
			return;
		}

		uint32 ListenPort = 7001;

		// Start the UBA Agent that will connect to us, requesting for work
		const std::string ListenPortArg = "-listen=" + std::to_string(ListenPort);

		const char* UbaAgentArgs[] =
		{
			ListenPortArg.c_str(),
			"-nopoll",				// -nopoll recommended when running on remote Horde agents to make sure they exit after completion. Otherwise, it keeps running.
			"-listenTimeout=5",		// Agent will wait 5 seconds for this thread to connect (Server_AddClient does the connect)
			"-quiet",				// Skip all the agent logging that would be sent over to here
			"-maxidle=15",			// After 15 seconds of idling agent will automatically disconnect
		};

		// If the machine does not run Windows, enable the compatibility layer Wine to run UbaAgent.exe on POSIX systems
		const bool bRunsWindowsOS = Agent->GetMachineInfo().bRunsWindowOS;
		const bool bUseWine = !bRunsWindowsOS;

		if (ShouldExit.Wait(0))
		{
			return;
		}

		Agent->Execute("UbaAgent.exe", UbaAgentArgs, UE_ARRAY_COUNT(UbaAgentArgs), nullptr, nullptr, 0, bUseWine);

		// Add this machine as client to the remote agent
		const FString& IpAddress = Agent->GetMachineInfo().Ip;
		auto IpAddressStr = StringCast<uba::tchar>(*IpAddress);
		const bool bAddClientSuccess = Server_AddClient(UbaServer, IpAddressStr.Get(), ListenPort, nullptr);

		if (!bAddClientSuccess)
		{
			UE_LOG(LogUbaController, Display, TEXT("Server_AddClient(%s:%d) failed"), *IpAddress, ListenPort);
			return;
		}

		// Log remote execution
		FString UbaAgentCmdArgs = TEXT("UbaAgent.exe");
		for (const char* Arg : UbaAgentArgs)
		{
			UbaAgentCmdArgs += TEXT(" ");
			UbaAgentCmdArgs += ANSI_TO_TCHAR(Arg);
		}
		UE_LOG(LogUbaController, Log, TEXT("Remote execution on Horde machine [%s:%d]: %s"), *IpAddress, ListenPort, *UbaAgentCmdArgs);

		MachineCoreCount = MachineInfo.LogicalCores;
		EstimatedCoreCount += MachineCoreCount;
		ActiveCoreCount += MachineCoreCount;
	}

	while (Agent->IsValid() && !ShouldExit.Wait(100))
	{
		Agent->Poll(UbaControllerModule::bHordeForwardAgentLogs);
	}

	ActiveCoreCount -= MachineCoreCount;
	EstimatedCoreCount -= MachineCoreCount;
}

void FUbaHordeAgentManager::ParseConfig()
{
	// Try to read authentication provider identifier.
	FString HordeConfig;
	if (GConfig->GetString(TEXT("UbaController"), TEXT("Horde"), HordeConfig, GEngineIni))
	{
		HordeConfig.TrimStartInline();
		HordeConfig.TrimEndInline();
		HordeConfig.RemoveFromStart(TEXT("("));
		HordeConfig.RemoveFromEnd(TEXT(")"));

		if (FParse::Value(*HordeConfig, TEXT("Url="), Url))
		{
			UE_LOG(LogUbaHorde, Log, TEXT("Found UBA controller Url: \"%s\""), *Url);
		}

		if (FParse::Value(*HordeConfig, TEXT("Pool="), Pool))
		{
			UE_LOG(LogUbaHorde, Log, TEXT("Found UBA controller Pool: \"%s\""), *Pool);
		}

		if (FParse::Value(*HordeConfig, TEXT("Oidc="), Oidc))
		{
			UE_LOG(LogUbaHorde, Log, TEXT("Found UBA controller Oidc: \"%s\""), *Oidc);
		}

		if (FParse::Value(*HordeConfig, TEXT("MaxCores="), MaxCores))
		{
			UE_LOG(LogUbaHorde, Log, TEXT("Found UBA controller MaxCores: \"%u\""), MaxCores);
		}
	}
}
