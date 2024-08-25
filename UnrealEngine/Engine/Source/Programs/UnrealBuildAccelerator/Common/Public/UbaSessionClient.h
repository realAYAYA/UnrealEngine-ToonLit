// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaNetwork.h"
#include "UbaSession.h"

namespace uba
{
	class NetworkClient;

	struct SessionClientCreateInfo : SessionCreateInfo
	{
		SessionClientCreateInfo(Storage& s, NetworkClient& c, LogWriter& writer = g_consoleLogWriter) : SessionCreateInfo(s, writer), client(c) {}

		NetworkClient& client;
		StringBuffer<128> name;
		u32 maxProcessCount = 1;
		u32 defaultPriorityClass = 0x00004000; // BELOW_NORMAL_PRIORITY_CLASS;
		u32 outputStatsThresholdMs = 0;
		u32 maxIdleSeconds = ~0u;
		u8 memWaitLoadPercent = 80; // When memory usage goes above this percent, no new processes will be spawned until back below
		u8 memKillLoadPercent = 90; // When memory usage goes above this percent, newest processes will be killed to bring it back below
		bool dedicated = false;  // If true, server will not disconnect client when starting to run out of work.
		bool disableCustomAllocator = false;
		bool useBinariesAsVersion = false;
		bool killRandom = false;
		bool useStorage = true;
		Function<void(const ProcessHandle&)> processFinished;
	};

	class SessionClient final : public Session
	{
	public:
		SessionClient(const SessionClientCreateInfo& info);
		~SessionClient();

		bool Start();
		void Stop();
		bool Wait(u32 milliseconds = 0xFFFFFFFF, Event* wakeupEvent = nullptr);
		void SendSummary(const Function<void(Logger&)>& extraInfo);
		void SetIsTerminating(const tchar* reason = TC("Terminating"), u64 delayMs = 0); // Session stores pointer directly. Can't be temporary
		void SetMaxProcessCount(u32 count);

		u64 GetBestPing();

	private:
		bool RetrieveCasFile(CasKey& outNewKey, u64& outSize, const CasKey& casKey, const tchar* hint, bool storeUncompressed, bool allowProxy = true);

		virtual bool PrepareProcess(const ProcessStartInfo& startInfo, bool isChild, StringBufferBase& outRealApplication, const tchar*& outRealWorkingDir) override;
		virtual void* GetProcessEnvironmentVariables() override;
		virtual bool CreateFile(CreateFileResponse& out, const CreateFileMessage& msg) override;
		virtual bool DeleteFile(DeleteFileResponse& out, const DeleteFileMessage& msg) override;
		virtual bool CopyFile(CopyFileResponse& out, const CopyFileMessage& msg) override;
		virtual bool MoveFile(MoveFileResponse& out, const MoveFileMessage& msg) override;
		virtual bool Chmod(ChmodResponse& out, const ChmodMessage& msg) override;
		virtual bool CreateDirectory(CreateDirectoryResponse& out, const CreateDirectoryMessage& msg) override;
		virtual bool GetFullFileName(GetFullFileNameResponse& out, const GetFullFileNameMessage& msg) override;
		virtual bool GetListDirectoryInfo(ListDirectoryResponse& out, tchar* dirName, const StringKey& dirKey) override;
		virtual bool WriteFilesToDisk(ProcessImpl& process, WrittenFile** files, u32 fileCount) override;
		virtual bool AllocFailed(Process& process, const tchar* allocType, u32 error) override;
		virtual void PrintSessionStats(Logger& logger) override;
		virtual bool GetNextProcess(Process& process, bool& outNewProcess, NextProcessInfo& outNextProcess, u32 prevExitCode, BinaryReader& statsReader) override;
		virtual bool CustomMessage(Process& process, BinaryReader& reader, BinaryWriter& writer) override;
		virtual bool FlushWrittenFiles(ProcessImpl& process) override;
		virtual bool UpdateEnvironment(ProcessImpl& process, const tchar* reason, bool resetStats) override;
		virtual void TraceSessionUpdate() override;

		struct InternalProcessStartInfo;
		struct ModuleInfo;

		bool GetCasKeyForFile(CasKey& out, u32 processId, const StringBufferBase& fileName, const StringKey& fileNameKey);
		bool ReadModules(List<ModuleInfo>& outModules, u32 processId, const tchar* application);
		bool EnsureApplicationEnvironment(StringBufferBase& out, u32 processId, const tchar* application);
		bool EnsureBinaryFile(StringBufferBase& out, StringBufferBase& outVirtual, u32 processId, const StringBufferBase& fileName, const StringKey& fileNameKey, const tchar* applicationDir);
		bool WriteBinFile(StringBufferBase& out, const tchar* binaryName, const CasKey& casKey, const KeyToString& applicationDir, u32 fileAttributes);
		bool SendFiles(ProcessImpl& process, Timer& sendFiles);
		bool SendFile(WrittenFile& source, const tchar* destination, u32 processId, bool keepMappingInMemory);
		bool SendUpdateDirectoryTable(StackBinaryReader<SendMaxSize>& reader); // Note, reader is sent in to save stack space.
		bool UpdateDirectoryTableFromServer(StackBinaryReader<SendMaxSize>& reader);
		bool SendUpdateNameToHashTable(StackBinaryReader<SendMaxSize>& reader);
		bool UpdateNameToHashTableFromServer(StackBinaryReader<SendMaxSize>& reader);
		void Connect();
		void BuildEnvironmentVariables(BinaryReader& reader);
		bool SendProcessAvailable(Vector<InternalProcessStartInfo>& out, float availableWeight);
		void SendReturnProcess(u32 processId, const tchar* reason);
		void SendPing(u64 memAvail, u64 memTotal);
		void SendLogFileToServer(ProcessImpl& pi);
		void GetLogFileName(StringBufferBase& out, const tchar* logFile, const tchar* arguments);
		u32 CountLogLines(ProcessImpl& process);
		void WriteLogLines(BinaryWriter& writer, ProcessImpl& process);

		void ThreadCreateProcessLoop();

		NetworkClient& m_client;
		static constexpr u8 ServiceId = SessionServiceId;

		StringBuffer<128> m_name;
		StringBuffer<MaxPath> m_processWorkingDir;
		u32 m_sessionId = 0;
		u32 m_uiLanguage = 0;
		u32 m_defaultPriorityClass;
		u32 m_outputStatsThresholdMs = 0;
		u32 m_maxIdleSeconds = ~0u;
		u32 m_killRandomIndex = ~0u;
		u32 m_killRandomCounter = 0;
		u8 m_memWaitLoadPercent;
		u8 m_memKillLoadPercent;
		bool m_disableCustomAllocator;
		bool m_useBinariesAsVersion;
		bool m_connected = false;
		bool m_dedicated = false;
		bool m_useStorage = true;
		bool m_shouldSendLogToServer = false;
		bool m_shouldSendTraceToServer = false;
		bool m_remoteExecutionEnabled = true;
		
		Atomic<const tchar*> m_terminationReason;
		Atomic<u64> m_terminationTime;
		Atomic<u32> m_maxProcessCount;

		ReaderWriterLock m_handledApplicationEnvironmentsLock;
		UnorderedSet<TString> m_handledApplicationEnvironments;

		ReaderWriterLock m_binFileLock;
		UnorderedMap<TString, CasKey> m_writtenBinFiles;

		ReaderWriterLock m_nameToNameLookupLock;
		struct NameRec { TString name; TString virtualName; ReaderWriterLock lock; bool handled = false; };
		UnorderedMap<TString, NameRec> m_nameToNameLookup;

		struct HashRec { CasKey key; u64 serverTime = 0; ReaderWriterLock lock; };
		UnorderedMap<StringKey, HashRec> m_nameToHashLookup;
		ReaderWriterLock m_nameToHashLookupLock;
		ReaderWriterLock m_nameToHashMemLock;

		ReaderWriterLock m_directoryTableLock;
		u32 m_directoryTableMemPos = 0;
		struct ActiveUpdateDirectoryEntry;
		ActiveUpdateDirectoryEntry* m_firstEmptyWait = nullptr;
		ActiveUpdateDirectoryEntry* m_firstReadWait = nullptr;

		Event m_waitToSendEvent;
		Thread m_loopThread;
		Atomic<bool> m_loop;

		Function<void(const ProcessHandle&)> m_processFinished;

		SessionSummaryStats m_stats;

		Atomic<u64> m_bestPing;
		u64 m_lastPing = 0;
		u64 m_lastPingSendTime = 0;
	};
}
