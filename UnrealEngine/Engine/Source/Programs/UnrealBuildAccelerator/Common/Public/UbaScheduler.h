// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaMemory.h"
#include "UbaProcessStartInfo.h"
#include "UbaThread.h"

namespace uba
{
	class Process;
	class SessionServer;
	struct NextProcessInfo;
	struct ProcessStartInfoHolder;

	struct SchedulerCreateInfo
	{
		SchedulerCreateInfo(SessionServer& s) : session(s) {}

		SessionServer& session;
		u32 maxLocalProcessors = ~0u; // Max local processors to use. ~0u means it will use all processors
		bool enableProcessReuse = false; // If this is true, the system will allow processes to be reused when they're asking for it.
		bool forceRemote = false; // Force all processes that can run remotely to run remotely.
		bool forceNative = false; // Force all processes to run native (not detoured)
	};

	struct EnqueueProcessInfo
	{
		EnqueueProcessInfo(const ProcessStartInfo& i) : info(i) {}

		const ProcessStartInfo& info;
		
		float weight = 1.0f; // Weight of process. This is used towards max local processors. If a process is multithreaded it is likely it's weight should be more than 1.0
		bool canDetour = true; // If true, uba will detour the process. If false it will just create pipes for std out and then run the process as-is.
		bool canExecuteRemotely = true; // If true, this process can run on other machines, if false it will always be executed locally

		const void* knownInputs = nullptr; // knownInputs is a memory block with null terminated tchar strings followed by an empty null terminated string to end.
		u32 knownInputsBytes = 0; // knownInputsBytes is the total size in bytes of knownInputs
		u32 knownInputsCount = 0; // knownInputsCount is the number of strings in the memory block

		const u32* dependencies = nullptr; // An array of u32 holding indicies to processes this process depends on. Index is a rolling number returned by EnqueueProcess
		u32 dependencyCount = 0; // Number of elements in dependencies
	};

	class Scheduler
	{
	public:
		Scheduler(const SchedulerCreateInfo& info);
		~Scheduler();

		void Start(); // Start scheduler thread. Should be called before server starts listen to connections if using remote help
		void Stop(); // Will wait on all active processes and then exit.
		void SetMaxLocalProcessors(u32 maxLocalProcessors); // Set max local processes

		u32 EnqueueProcess(const EnqueueProcessInfo& info); // Returns index of process. Index is a rolling number

		void GetStats(u32& outQueued, u32& outActiveLocal, u32& outActiveRemote, u32& outFinished);

		bool EnqueueFromFile(const tchar* yamlFilename); // Enqueue actions from file. Example of format of file is at the end of this file

		void SetProcessFinishedCallback(const Function<void(const ProcessHandle&)>& processFinished); // Set callback 

	private:
		struct ExitProcessInfo;
		struct ProcessStartInfo2;

		void ThreadLoop();
		void RemoteProcessReturned(Process& process);
		void RemoteSlotAvailable();
		void ProcessExited(ExitProcessInfo* info, const ProcessHandle& handle);
		u32 PopProcess(bool isLocal);
		bool RunQueuedProcess(bool isLocal);
		bool HandleReuseMessage(Process& process, NextProcessInfo& outNextProcess, u32 prevExitCode);
		void ExitProcess(ExitProcessInfo& info, Process& process, u32 exitCode);
		void SkipProcess(ProcessStartInfo2& info);
		void UpdateQueueCounter(int offset);
		void UpdateActiveProcessCounter(bool isLocal, int offset);
		void FinishProcess(const ProcessHandle& handle);

		SessionServer& m_session;
		u32 m_maxLocalProcessors;
	
		enum ProcessStatus : u8
		{
			ProcessStatus_Queued,
			ProcessStatus_Running,
			ProcessStatus_Success,
			ProcessStatus_Failed,
			ProcessStatus_Skipped,
		};

		
		struct ProcessEntry
		{
			ProcessStartInfo2* info;
			u32* dependencies;
			u32 dependencyCount;
			ProcessStatus status;
			u8 canDetour;
			u8 canExecuteRemotely;
		};

		ReaderWriterLock m_processEntriesLock;
		Vector<ProcessEntry> m_processEntries;
		u32 m_processEntriesStart = 0;

		Function<void(const ProcessHandle&)> m_processFinished;

		Event m_updateThreadLoop;
		Thread m_thread;
		Atomic<bool> m_loop;
		bool m_enableProcessReuse;
		bool m_forceRemote;
		bool m_forceNative;

		float m_activeLocalProcessWeight = 0.0f;

		Atomic<u32> m_queuedProcesses;
		Atomic<u32> m_activeLocalProcesses;
		Atomic<u32> m_activeRemoteProcesses;
		Atomic<u32> m_finishedProcesses;

		Scheduler(const Scheduler&) = delete;
		void operator=(const Scheduler&) = delete;
	};
}

#if 0
// Example of yaml file with processes that can be queued up in scheduler
// id - Not used right now. Number does not matter because id will be a rolling number
// app - Application to execute
// arg - Arguments to application
// dir - Working directory
// desc - Description of process
// weight - How much cpu this process is using. Optional. Defaults to 1.0
// remote - Decides if this process can execute remotely. Optional. Defaults to true
// detour - Decides if this process can be detoured. Optional. Defaults to true.
// dep - Dependencies. An array of indices to other processes

processes:
  - id: 0
    app: C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Tools\MSVC\14.38.33130\bin\Hostx64\x64\cl.exe
    arg: @"..\Intermediate\Build\Win64\x64\UnrealPak\Development\Core\SharedPCH.Core.Cpp20.h.obj.rsp"
    dir: E:\dev\fn\Engine\Source
    desc: SharedPCH.Core.Cpp20.cpp
    weight: 1.25
    remote: false

  - id: 44
    app: C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Tools\MSVC\14.38.33130\bin\Hostx64\x64\cl.exe
    arg: @"..\Intermediate\Build\Win64\x64\UnrealPak\Development\Json\Module.Json.cpp.obj.rsp"
    dir: E:\dev\fn\Engine\Source
    desc: Module.Json.cpp
    weight: 1.5
    dep: [0]

  - id: 337
    app: E:\dev\fn\Engine\Binaries\ThirdParty\DotNet\6.0.302\windows\dotnet.exe
    arg: "E:\dev\fn\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll" -Mode=WriteMetadata -Input="E:\dev\fn\Engine\Intermediate\Build\Win64\x64\UnrealPak\Development\TargetMetadata.dat" -Version=2
    dir: E:\dev\fn\Engine\Source
    desc: UnrealPak.target
    detour: false
    dep: [336, 0]

#endif