// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#define Local_GetLongPathNameW uba::GetLongPathNameW

#include "UbaFile.h"
#include "UbaDirectoryTable.h"
#include "UbaFileMapping.h"
#include "UbaProcessHandle.h"
#include "UbaProcessStartInfo.h"
#include "UbaSessionCreateInfo.h"
#include "UbaStats.h"
#include "UbaTrace.h"
#include "UbaThread.h"

namespace uba
{
	class Process;
	class ProcessHandle;
	class ProcessImpl;
	class Storage;
	class WorkManager;
	struct ProcessStartInfo;
	struct ProcessStats;
	struct InitMessage;
	struct InitResponse;
	struct CreateFileMessage;
	struct CreateFileResponse;
	struct CloseFileMessage;
	struct CloseFileResponse;
	struct DeleteFileMessage;
	struct DeleteFileResponse;
	struct CopyFileMessage;
	struct CopyFileResponse;
	struct MoveFileMessage;
	struct MoveFileResponse;
	struct ChmodResponse;
	struct ChmodMessage;
	struct GetFullFileNameMessage;
	struct GetFullFileNameResponse;
	struct CreateDirectoryMessage;
	struct CreateDirectoryResponse;
	struct ListDirectoryMessage;
	struct ListDirectoryResponse;
	struct WrittenFile;
	struct NextProcessInfo;

	class Session
	{
	public:
		ProcessHandle RunProcess(const ProcessStartInfo& startInfo, bool async = true, bool enableDetour = true); // Run process. if async is false it will not return until process is done
		void CancelAllProcessesAndWait(bool terminate = true); // Cancel all processes and wait for them to go away

		void PrintSummary(Logger& logger); // Print summary stats of session
		void RefreshDirectory(const tchar* dirName); // Tell uba a directory on disk has been changed by some other system while session is running
		void RegisterNewFile(const tchar* filePath); // Tell uba a new file on disk has been added by some other system while session is running
		void RegisterDeleteFile(const tchar* filePath); // Tell uba a file on disk has been deleted by some other system while session is running

		using CustomServiceFunction = Function<u32(Process& handle, const void* recv, u32 recvSize, void* send, u32 sendCapacity)>;
		void RegisterCustomService(CustomServiceFunction&& function); // Register a custom service (that can be communicated with from the remote agents)

		using GetNextProcessFunction = Function<bool(Process& handle, NextProcessInfo& outNextProcess, u32 prevExitCode)>;
		void RegisterGetNextProcess(GetNextProcessFunction&& function); // Register a custom service (that can be communicated with from the remote agents)

		const tchar* GetId();			// Id for session. Will be "yymmdd_hhmmss" unless SessionCreateInfo.useUniqueId is set to false
		u32 GetActiveProcessCount();	// Current active processes running inside session
		Storage& GetStorage();		// Storage (only used when remote machines are connected)
		Logger& GetLogger();			// Logger used for logging 
		LogWriter& GetLogWriter();		// LogWriter used by logger

		virtual ~Session();

	protected:
		Session(const SessionCreateInfo& info, const tchar* logPrefix, bool runningRemote, WorkManager* workManager = nullptr);
		bool Create(const SessionCreateInfo& info);

		void ValidateStartInfo(const ProcessStartInfo& startInfo);
		ProcessHandle InternalRunProcess(const ProcessStartInfo& startInfo, bool async, ProcessImpl* parent, bool enableDetour);
		void ProcessAdded(Process& process, u32 sessionId);
		void ProcessExited(ProcessImpl& process, u64 executionTime);
		void FlushDeadProcesses();
		void PrintProcessStats(ProcessStats& stats, const tchar* logName);
		void StartTrace(const tchar* traceName);
		bool StopTrace(const tchar* writeFile);
		void StopTraceThread();
		u32 GetDirectoryTableSize();
		u32 GetFileMappingSize();
		u32 GetMemoryMapAlignment(const tchar* fileName, u64 fileNameLen) const;

		SessionStats& Stats();

		struct BinaryModule { TString name; TString path; u32 fileAttributes = 0; bool isSystem = false; };
		bool GetBinaryModules(Vector<BinaryModule>& out, const tchar* application);
		void Free(Vector<BinaryModule>& v);
		bool IsRarelyRead(ProcessImpl& process, const StringBufferBase& fileName) const;
		bool IsRarelyReadAfterWritten(ProcessImpl& process, const tchar* fileName, u64 fileNameLen) const;
		bool IsKnownSystemFile(const tchar* applicationName);
		bool ShouldWriteToDisk(const tchar* fileName, u64 fileNameLen);
		u32 WriteDirectoryEntries(const StringKey& dirKey, tchar* dirPath, u32& outTableOffset);
		u32 AddFileMapping(StringKey fileNameKey, const tchar* fileName, const tchar* newFileName, u64 fileSize = InvalidValue);
		
		struct MemoryMap { StringBuffer<128> name; u64 size = 0; };
		bool CreateMemoryMapFromFile(MemoryMap& out, StringKey fileNameKey, const tchar* fileName, bool isCompressed, u64 alignment);
		bool CreateMemoryMapFromView(MemoryMap& out, StringKey fileNameKey, const tchar* fileName, const CasKey& casKey, u64 alignment);

		bool RegisterCreateFileForWrite(StringKey fileNameKey, const tchar* fileName, u64 fileNameLen, bool registerRealFile, u64 fileSize = 0, u64 lastWriteTime = 0);
		u32 RegisterDeleteFile(StringKey fileNameKey, const tchar* fileName);

		virtual bool PrepareProcess(const ProcessStartInfo& startInfo, bool isChild, StringBufferBase& outRealApplication, const tchar*& outRealWorkingDir);
		virtual void* GetProcessEnvironmentVariables();
		virtual void PrintSessionStats(Logger& logger);

		virtual bool GetInitResponse(InitResponse& out, const InitMessage& msg);
		virtual bool CreateFile(CreateFileResponse& out, const CreateFileMessage& msg);
		virtual bool CloseFile(CloseFileResponse& out, const CloseFileMessage& msg);
		virtual bool DeleteFile(DeleteFileResponse& out, const DeleteFileMessage& msg);
		virtual bool CopyFile(CopyFileResponse& out, const CopyFileMessage& msg);
		virtual bool MoveFile(MoveFileResponse& out, const MoveFileMessage& msg);
		virtual bool Chmod(ChmodResponse& out, const ChmodMessage& msg);
		virtual bool CreateDirectory(CreateDirectoryResponse& out, const CreateDirectoryMessage& msg);
		virtual bool GetFullFileName(GetFullFileNameResponse& out, const GetFullFileNameMessage& msg);
		virtual bool GetListDirectoryInfo(ListDirectoryResponse& out, tchar* dirName, const StringKey& dirKey);
		virtual bool WriteFilesToDisk(ProcessImpl& process, WrittenFile** files, u32 fileCount);
		virtual bool AllocFailed(Process& process, const tchar* allocType, u32 error);
		virtual bool GetNextProcess(Process& process, bool& outNewProcess, NextProcessInfo& outNextProcess, u32 prevExitCode, BinaryReader& statsReader);
		virtual bool CustomMessage(Process& process, BinaryReader& reader, BinaryWriter& writer);
		virtual void FileEntryAdded(StringKey fileNameKey, u64 lastWritten, u64 size);
		virtual bool FlushWrittenFiles(ProcessImpl& process);
		virtual bool UpdateEnvironment(ProcessImpl& process, const tchar* reason, bool resetStats);

		static constexpr CasKey CasKeyIsDirectory = { ~u64(0), ~u64(0), ~u32(0) };

		void AddEnvironmentVariableNoLock(const tchar* key, const tchar* value);
		bool WriteDirectoryEntriesInternal(DirectoryTable::Directory& dir, const StringKey& dirKey, const tchar* dirPath, bool isRefresh, u32& outTableOffset);
		void WriteDirectoryEntriesRecursive(const StringKey& dirKey, tchar* dirPath, u32& outTableOffset);
		bool CopyImports(Vector<BinaryModule>& out, const tchar* library, tchar* applicationDir, tchar* applicationDirEnd, UnorderedSet<TString>& handledImports);
		bool CreateProcessJobObject();
		void GetSystemInfo(StringBufferBase& out);
		bool GetMemoryInfo(u64& outAvailable, u64& outTotal);
		void WriteSummary(BinaryWriter& writer, const Function<void(Logger& logger)>& summaryFunc);
		float UpdateCpuLoad();

		void ThreadTraceLoop();
		virtual void TraceSessionUpdate();

		Storage& m_storage;
		MutableLogger m_logger;

		WorkManager* m_workManager;

		StringBuffer<32> m_id;
		StringBuffer<MaxPath> m_rootDir;
		StringBuffer<MaxPath> m_sessionDir;
		StringBuffer<MaxPath> m_sessionBinDir;
		StringBuffer<MaxPath> m_sessionOutputDir;
		StringBuffer<MaxPath> m_sessionLogDir;
		StringBuffer<MaxPath> m_systemPath;
		StringBuffer<MaxPath> m_tempPath;


		bool m_runningRemote;
		bool m_disableCustomAllocator;
		bool m_allowMemoryMaps;
		bool m_shouldWriteToDisk;
		bool m_detailedTrace;
		bool m_logToFile;

		u64 m_keepOutputFileMemoryMapsThreshold;

		u32 m_uid;
		Atomic<u32> m_processIdCounter;

		MemoryBlock m_directoryTableMemory;

		FileMappingHandle m_directoryTableHandle;
		u8* m_directoryTableMem;
		DirectoryTable m_directoryTable;
	
		FileMappingHandle m_fileMappingTableHandle;
		FileMappingBuffer m_fileMappingBuffer;

		ReaderWriterLock m_fileMappingTableMemLock;
		u8* m_fileMappingTableMem;
		u32 m_fileMappingTableSize = 0;
		ReaderWriterLock m_fileMappingTableLookupLock;
		struct FileMappingEntry { ReaderWriterLock lock; FileMappingHandle mapping; u64 mappingOffset = 0; u64 size = 0; bool isDir = false; bool handled = false; bool success = false;};
		UnorderedMap<StringKey, FileMappingEntry> m_fileMappingTableLookup;

		ReaderWriterLock m_nameToHashTableHandleLock;
		static constexpr u64 NameToHashMemSize = 48*1024*1024;
		MemoryBlock m_nameToHashTableMem;

		Atomic<u64> m_fileIndexCounter = 8000000000;

		FileMappingAllocator m_processCommunicationAllocator;
		std::string m_detoursLibrary;

		ReaderWriterLock m_processStatsLock;
		ProcessStats m_processStats;

		ReaderWriterLock m_processesLock;
		UnorderedMap<u32, ProcessHandle> m_processes;
		Vector<ProcessHandle> m_deadProcesses;
		UnorderedMap<TString, Timer> m_applicationStats;

		ReaderWriterLock m_outputFilesLock;
		UnorderedMap<TString, TString> m_outputFiles;

		ReaderWriterLock m_activeFilesLock;
		struct ActiveFile { TString name; StringKey nameKey; };
		UnorderedMap<u32, ActiveFile> m_activeFiles;

		u32 m_wantsOnCloseIdCounter = 1;

		SessionStats m_stats;
		Trace m_trace;
		Event m_traceThreadEvent;
		Thread m_traceThread;
		StringBuffer<256> m_traceOutputFile;
		TString m_extraInfo;
		u64 m_maxPageSize = ~u64(0);
		u64 m_previousTotalCpuTime = 0;
		u64 m_previousIdleCpuTime = 0;
		float m_cpuLoad = 0;

		#if PLATFORM_WINDOWS
		ReaderWriterLock m_processJobObjectLock;
		HANDLE m_processJobObject = NULL;
		#endif

		ReaderWriterLock m_environmentVariablesLock;
		Vector<tchar> m_environmentVariables;
		UnorderedSet<const tchar*, HashStringNoCase, EqualStringNoCase> m_localEnvironmentVariables;

		GetNextProcessFunction m_getNextProcessFunction;
		CustomServiceFunction m_customServiceFunction;

		friend class ProcessImpl;
	};

	void GetNameFromArguments(StringBufferBase& out, const tchar* arguments, bool addCounterSuffix);

	using FileAccess = u8;

	enum : u8
	{
		FileAccess_Read = 1,
		FileAccess_Write = 2,
		FileAccess_ReadWrite = 3
	};

	struct InitMessage
	{
	};

	struct InitResponse
	{
		u64 directoryTableHandle = 0;
		u32 directoryTableSize = 0;
		u32 directoryTableCount = 0;
		u64 mappedFileTableHandle = 0;
		u32 mappedFileTableSize = 0;
		u32 mappedFileTableCount = 0;
	};

	struct CreateFileMessage
	{
		ProcessImpl& process;
		StringBuffer<> fileName;
		StringKey fileNameKey;
		FileAccess access;
	};

	struct CreateFileResponse
	{
		StringBuffer<> fileName;
		StringBuffer<> virtualFileName;
		u64 size = InvalidValue;
		u32 closeId = 0;
		u32 mappedFileTableSize = 0;
		u32 directoryTableSize = 0;
	};

	struct CloseFileMessage
	{
		ProcessImpl& process;
		StringBuffer<> fileName;
		StringKey newNameKey;
		StringBuffer<> newName;
		u32 closeId = 0;
		u32 attributes = 0;
		bool deleteOnClose = false;
		bool success = true;
		u64 mappingHandle = 0;
		u64 mappingWritten = 0;
	};

	struct CloseFileResponse
	{
		u32 directoryTableSize = 0;
	};

	struct DeleteFileMessage
	{
		ProcessImpl& process;
		StringBuffer<> fileName;
		StringKey fileNameKey;
		u32 closeId = 0;
	};

	struct DeleteFileResponse
	{
		bool result = false;
		u32 errorCode = ~0u;
		u32 directoryTableSize = 0;
	};

	struct CopyFileMessage
	{
		ProcessImpl& process;
		StringKey fromKey;
		StringBuffer<> fromName;
		StringKey toKey;
		StringBuffer<> toName;
	};

	struct CopyFileResponse
	{
		StringBuffer<> fromName;
		StringBuffer<> toName;
		u32 closeId = 0;
		u32 errorCode = ~0u;
		u32 directoryTableSize = 0;
	};

	struct MoveFileMessage
	{
		ProcessImpl& process;
		StringKey fromKey;
		StringBuffer<> fromName;

		StringKey toKey;
		StringBuffer<> toName;

		u32 flags = 0;
	};

	struct MoveFileResponse
	{
		bool result = false;
		u32 errorCode = ~0u;
		u32 directoryTableSize = 0;
	};

	struct ChmodMessage
	{
		ProcessImpl& process;
		StringKey fileNameKey;
		StringBuffer<> fileName;
		u32 fileMode = 0;
	};

	struct ChmodResponse
	{
		u32 errorCode = ~0u;
	};

	struct GetFullFileNameMessage
	{
		ProcessImpl& process;
		StringBuffer<> fileName;
		StringKey fileNameKey;
	};

	struct GetFullFileNameResponse
	{
		StringBuffer<> fileName;
		StringBuffer<> virtualFileName;
		u32 mappedFileTableSize = 0;
	};

	struct CreateDirectoryMessage
	{
		StringKey nameKey;
		StringBuffer<> name;
	};

	struct CreateDirectoryResponse
	{
		bool result;
		u32 errorCode;
	};

	struct ListDirectoryMessage
	{
		StringBuffer<> directoryName;
		StringKey directoryNameKey;
	};

	struct ListDirectoryResponse
	{
		u32 tableOffset = 0;
		u32 tableSize = 0;
	};

	struct WrittenFile
	{
		ProcessImpl* owner = nullptr;
		StringKey key;
		TString name;
		FileMappingHandle mappingHandle;
		u64 mappingWritten = 0;
		u64 originalMappingHandle = 0;
		u64 lastWriteTime = 0;
		u32 attributes = 0;
	};

	struct NextProcessInfo
	{
		TString arguments;
		TString workingDir;
		TString description;
		TString logFile;
	};

}

template<> struct std::hash<uba::ProcessHandle> { size_t operator()(const uba::ProcessHandle& g) const { return g.GetHash(); } };
