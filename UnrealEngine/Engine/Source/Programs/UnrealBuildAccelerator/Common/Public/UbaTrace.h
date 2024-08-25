// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaFileMapping.h"
#include "UbaLogger.h"
#include "UbaProcessStats.h"
#include "UbaWorkManager.h"

namespace uba
{
	struct ProcessLogLine;
	struct ProcessStats;

	class TraceChannel
	{
	public:
		TraceChannel(Logger& logger);
		~TraceChannel();

		bool Init(const tchar* channelName = TC("Default"));
		bool Write(const tchar* traceName);
		bool Read(StringBufferBase& outTraceName);

		Logger& m_logger;
		MutexHandle m_mutex = InvalidMutexHandle;
		FileMappingHandle m_memHandle;
		void* m_mem = nullptr;
	};

	enum TraceType : u8
	{
		TraceType_SessionAdded,
		TraceType_SessionUpdate,
		TraceType_ProcessAdded,
		TraceType_ProcessExited,
		TraceType_ProcessReturned,
		TraceType_FileBeginFetch,
		TraceType_FileEndFetch,
		TraceType_FileBeginStore,
		TraceType_FileEndStore,
		TraceType_Summary,
		TraceType_BeginWork,
		TraceType_EndWork,
		TraceType_String,
		TraceType_SessionSummary,
		TraceType_ProcessEnvironmentUpdated,
		TraceType_SessionDisconnect,
		TraceType_ProxyCreated,
		TraceType_ProxyUsed,
		TraceType_FileFetchLight,
		TraceType_FileStoreLight,
		TraceType_StatusUpdate,
		TraceType_SessionNotification,
	};

	using Color = u32;
	inline Color toColor(u8 r, u8 g, u8 b) { return (r << 16) + (g << 8) + b; }

	static constexpr u32 TraceVersion = 23;
	static constexpr u32 TraceReadCompatibilityVersion = 6;

	class Trace : public WorkTracker
	{
	public:
		Trace(LogWriter& logWriter);
		~Trace();

		bool IsWriting() const { return m_memoryBegin != nullptr; }

		bool StartWrite(const tchar* namedTrace, u64 traceMemCapacity = 64*1024*1024);
		void SessionAdded(u32 sessionId, u32 clientId, const tchar* name, const tchar* info);
		void SessionUpdate(u32 sessionId, u32 connectionCount, u64 send, u64 recv, u64 lastPing, u64 memAvail, u64 memTotal, float cpuLoad);
		void SessionNotification(u32 sessionId, const tchar* text);
		void SessionSummary(u32 sessionId, const u8* data, u64 dataSize);
		void SessionDisconnect(u32 sessionId);
		void ProcessAdded(u32 sessionId, u32 processId, const tchar* description);
		void ProcessEnvironmentUpdated(u32 processId, const tchar* reason, const u8* data, u64 dataSize);
		void ProcessExited(u32 processId, u32 exitCode, const u8* data, u64 dataSize, const Vector<ProcessLogLine>& logLines);
		void ProcessReturned(u32 processId);
		void ProxyCreated(u32 clientId, const tchar* proxyName);
		void ProxyUsed(u32 clientId, const tchar* proxyName);
		void FileBeginFetch(u32 clientId, const CasKey& key, u64 size, const tchar* hint, bool detailed);
		void FileEndFetch(u32 clientId, const CasKey& key);
		void FileBeginStore(u32 clientId, const CasKey& key, u64 size, const tchar* hint, bool detailed);
		void FileEndStore(u32 clientId, const CasKey& key);
		void BeginWork(u32 workIndex, const tchar* desc);
		void EndWork(u32 workIndex);
		void StatusUpdate(u32 statusIndex, u32 statusNameIndent, const tchar* statusName, u32 statusTextIndent, const tchar* statusText, LogEntryType statusType);

		bool StopWrite(const tchar* writeFileName);

		virtual u32 TrackWorkStart(const tchar* desc) final override;
		virtual void TrackWorkEnd(u32 id) final override;

	private:
		struct WriterScope;
		u32 AddString(const tchar* string);

		LoggerWithWriter m_logger;
		TraceChannel m_channel;
		ReaderWriterLock m_memoryLock;
		FileMappingHandle m_memoryHandle;
		u8* m_memoryBegin = nullptr;
		u64 m_memoryPos = 0;
		u64 m_memoryCommitted = 0;
		u64 m_memoryCapacity = 0;
		u64 m_startTime = ~u64(0);

		ReaderWriterLock m_stringsLock;
		UnorderedMap<StringKey, u32> m_strings;

		Atomic<u32> m_workCounter;

		friend class SessionServer;
	};
}