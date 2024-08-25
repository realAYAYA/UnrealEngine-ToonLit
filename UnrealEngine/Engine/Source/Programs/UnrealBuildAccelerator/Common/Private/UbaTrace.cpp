// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaTrace.h"
#include "UbaFileAccessor.h"
#include "UbaProcessHandle.h"

namespace uba
{
	constexpr u64 TraceMessageMaxSize = 256 * 1024;

	Trace::Trace(LogWriter& logWriter)
	:	m_logger(logWriter)
	,	m_channel(m_logger)
	{
	}

	Trace::~Trace()
	{
		if (m_memoryBegin)
			UnmapViewOfFile(m_memoryBegin, m_memoryCapacity, TC("Trace"));
		if (m_memoryHandle.IsValid())
			CloseFileMapping(m_memoryHandle);
	}

	struct Trace::WriterScope : ScopedWriteLock, BinaryWriter
	{
		WriterScope(Trace& trace) : ScopedWriteLock(trace.m_memoryLock), BinaryWriter(trace.m_memoryBegin, trace.m_memoryPos, trace.m_memoryCapacity), m_trace(trace)
		{
			isValid = EnsureMemory(TraceMessageMaxSize);
		}

		~WriterScope()
		{
			m_trace.m_memoryPos = GetPosition();
			*(u32*)m_trace.m_memoryBegin = u32(m_trace.m_memoryPos);
		}

		WriterScope(const WriterScope&) = delete;
		void operator=(const WriterScope&) = delete;

		bool EnsureMemory(u64 size)
		{
			u64 committedMemoryNeeded = AlignUp(m_trace.m_memoryPos + size, size);
			if (m_trace.m_memoryCommitted >= committedMemoryNeeded)
				return true;
			if (!MapViewCommit(m_trace.m_memoryBegin + m_trace.m_memoryCommitted, committedMemoryNeeded - m_trace.m_memoryCommitted))
				return m_trace.m_logger.Error(TC("Failed to commit memory for trace (Pos: %llu Capacity: %llu, Already Committed: %llu, Needed: %llu): %s"), m_trace.m_memoryPos, m_trace.m_memoryCapacity, m_trace.m_memoryCommitted, committedMemoryNeeded, LastErrorToText().data);
			m_trace.m_memoryCommitted = committedMemoryNeeded;
			return true;
		}

		Trace& m_trace;
		bool isValid;
	};

	bool Trace::StartWrite(const tchar* namedTrace, u64 traceMemCapacity)
	{
		m_memoryCapacity = traceMemCapacity;
		m_memoryHandle = uba::CreateMemoryMappingW(m_logger, PAGE_READWRITE|SEC_RESERVE, m_memoryCapacity, namedTrace);
		if (!m_memoryHandle.IsValid())
			return false;
		if (GetLastError() != ERROR_ALREADY_EXISTS)
			m_memoryBegin = MapViewOfFile(m_memoryHandle, FILE_MAP_WRITE, 0, m_memoryCapacity);

		if (!m_memoryBegin)
		{
			CloseFileMapping(m_memoryHandle);
			m_memoryHandle = {};
			return false;
		}

		m_memoryPos = 0;
		m_startTime = GetTime();
		u64 systemStartTimeUs = GetSystemTimeUs();

		{
			WriterScope writer(*this);
			if (!writer.isValid)
				return false;
			writer.AllocWrite(4);
			writer.WriteU32(TraceVersion);
			writer.WriteU32(GetCurrentProcessId());
			writer.Write7BitEncoded(systemStartTimeUs);
			writer.Write7BitEncoded(GetFrequency());
			writer.Write7BitEncoded(m_startTime);
		}


		if (namedTrace && m_channel.Init())
			m_channel.Write(namedTrace);

		return true;
	}

	bool Trace::StopWrite(const tchar* writeFileName)
	{
		if (!m_memoryBegin)
			return true;

		{
			WriterScope writer(*this);
			if (!writer.isValid)
				return false;
			writer.WriteByte(TraceType_Summary);
			writer.Write7BitEncoded(GetTime() - m_startTime);
		}


		if (!writeFileName || !*writeFileName)
			return true;
		FileAccessor traceFile(m_logger, writeFileName);
		if (!traceFile.CreateWrite(false, DefaultAttributes(), 0, nullptr))
			return false;
		u64 fileSize = m_memoryPos;
		if (!traceFile.Write(m_memoryBegin, fileSize))
			return false;
		if (!traceFile.Close())
			return false;
		m_logger.Info(TC("Trace file written to %s with size %s"), writeFileName, BytesToText(fileSize).str);
		
		UnmapViewOfFile(m_memoryBegin, m_memoryCapacity, TC("Trace"));
		m_memoryBegin = nullptr;
		return true;
	}

	u32 Trace::TrackWorkStart(const tchar* desc)
	{
		u32 workId = m_workCounter++;
		BeginWork(workId, desc);
		return workId;
	}

	void Trace::TrackWorkEnd(u32 id)
	{
		EndWork(id);
	}

	u32 Trace::AddString(const tchar* string)
	{
		if (!m_memoryBegin)
			return 0;

		u64 stringLen = TStrlen(string);

		SCOPED_WRITE_LOCK(m_stringsLock, lock);
		auto insres = m_strings.try_emplace(ToStringKeyNoCheck(string, stringLen));
		if (insres.second)
		{
			insres.first->second = u32(m_strings.size() - 1);
			WriterScope writer(*this);
			if (!writer.isValid)
				return 0;
			writer.WriteByte(TraceType_String);
			writer.WriteString(string, stringLen);
		}
		return insres.first->second;
	}

	#define BEGIN_TRACE_ENTRY(x) \
		if (!m_memoryBegin) \
			return; \
		WriterScope writer(*this); \
		if (!writer.isValid) \
			return; \
		writer.WriteByte(x); \
		writer.Write7BitEncoded(GetTime() - m_startTime);

	void Trace::SessionAdded(u32 sessionId, u32 clientId, const tchar* name, const tchar* info)
	{
		BEGIN_TRACE_ENTRY(TraceType_SessionAdded);
		writer.WriteString(name);
		writer.WriteString(info);
		writer.Write7BitEncoded(clientId);
		writer.WriteU32(sessionId);
	}

	void Trace::SessionUpdate(u32 sessionId, u32 connectionCount, u64 send, u64 recv, u64 lastPing, u64 memAvail, u64 memTotal, float cpuLoad)
	{
		BEGIN_TRACE_ENTRY(TraceType_SessionUpdate);
		writer.Write7BitEncoded(sessionId);
		writer.Write7BitEncoded(connectionCount);
		writer.Write7BitEncoded(send);
		writer.Write7BitEncoded(recv);
		writer.Write7BitEncoded(lastPing);
		writer.Write7BitEncoded(memAvail);
		writer.Write7BitEncoded(memTotal);
		writer.WriteU32(*(u32*)&cpuLoad);
	}

	void Trace::SessionNotification(u32 sessionId, const tchar* text)
	{
		BEGIN_TRACE_ENTRY(TraceType_SessionNotification);
		writer.WriteU32(sessionId);
		writer.WriteString(text);
	}

	void Trace::SessionSummary(u32 sessionId, const u8* data, u64 dataSize)
	{
		BEGIN_TRACE_ENTRY(TraceType_SessionSummary);
		writer.WriteU32(sessionId);
		writer.WriteBytes(data, dataSize);
	}

	void Trace::SessionDisconnect(u32 sessionId)
	{
		BEGIN_TRACE_ENTRY(TraceType_SessionDisconnect);
		writer.WriteU32(sessionId);
	}

	void Trace::ProcessAdded(u32 sessionId, u32 processId, const tchar* description)
	{
		BEGIN_TRACE_ENTRY(TraceType_ProcessAdded);
		writer.WriteU32(sessionId);
		writer.WriteU32(processId);
		writer.WriteString(description);
	}

	void Trace::ProcessEnvironmentUpdated(u32 processId, const tchar* reason, const u8* data, u64 dataSize)
	{
		BEGIN_TRACE_ENTRY(TraceType_ProcessEnvironmentUpdated);
		writer.WriteU32(processId);
		writer.WriteString(reason);
		writer.WriteBytes(data, dataSize);
	}

	void Trace::ProcessExited(u32 processId, u32 exitCode, const u8* data, u64 dataSize, const Vector<ProcessLogLine>& logLines)
	{
		BEGIN_TRACE_ENTRY(TraceType_ProcessExited);
		writer.WriteU32(processId);
		writer.WriteU32(exitCode);
		writer.WriteBytes(data, dataSize);
		u32 lineCounter = 0;
		for (auto& line : logLines)
		{
			if (lineCounter++ == 100) // We don't want to write the entire error in the trace stream to blow the entire buffer
				break;
			if (!writer.EnsureMemory(1 + (line.text.size()+1)*sizeof(tchar)))
				break;
			writer.WriteByte(line.type);
			writer.WriteString(line.text);
		}
		writer.WriteByte(255);
	}

	void Trace::ProcessReturned(u32 processId)
	{
		BEGIN_TRACE_ENTRY(TraceType_ProcessReturned);
		writer.WriteU32(processId);
	}

	void Trace::ProxyCreated(u32 clientId, const tchar* proxyName)
	{
		BEGIN_TRACE_ENTRY(TraceType_ProxyCreated);
		writer.Write7BitEncoded(clientId);
		writer.WriteString(proxyName);
	}

	void Trace::ProxyUsed(u32 clientId, const tchar* proxyName)
	{
		BEGIN_TRACE_ENTRY(TraceType_ProxyUsed);
		writer.Write7BitEncoded(clientId);
		writer.WriteString(proxyName);
	}

	void Trace::FileBeginFetch(u32 clientId, const CasKey& key, u64 size, const tchar* hint, bool detailed)
	{
		if (detailed)
		{
			u32 stringIndex = AddString(hint);
			BEGIN_TRACE_ENTRY(TraceType_FileBeginFetch);
			writer.Write7BitEncoded(clientId);
			writer.WriteCasKey(key);
			writer.Write7BitEncoded(size);
			writer.Write7BitEncoded(stringIndex);
		}
		else
		{
			BEGIN_TRACE_ENTRY(TraceType_FileFetchLight);
			writer.Write7BitEncoded(clientId);
			writer.Write7BitEncoded(size);
		}
	}

	void Trace::FileEndFetch(u32 clientId, const CasKey& key)
	{
		BEGIN_TRACE_ENTRY(TraceType_FileEndFetch);
		writer.Write7BitEncoded(clientId);
		writer.WriteCasKey(key);
	}

	void Trace::FileBeginStore(u32 clientId, const CasKey& key, u64 size, const tchar* hint, bool detailed)
	{
		if (detailed)
		{
			u32 stringIndex = AddString(hint);
			BEGIN_TRACE_ENTRY(TraceType_FileBeginStore);
			writer.Write7BitEncoded(clientId);
			writer.WriteCasKey(key);
			writer.Write7BitEncoded(size);
			writer.Write7BitEncoded(stringIndex);
		}
		else
		{
			BEGIN_TRACE_ENTRY(TraceType_FileStoreLight);
			writer.Write7BitEncoded(clientId);
			writer.Write7BitEncoded(size);
		}
	}

	void Trace::FileEndStore(u32 clientId, const CasKey& key)
	{
		BEGIN_TRACE_ENTRY(TraceType_FileEndStore);
		writer.Write7BitEncoded(clientId);
		writer.WriteCasKey(key);
	}

	void Trace::BeginWork(u32 workIndex, const tchar* desc)
	{
		u32 stringIndex = AddString(desc);
		BEGIN_TRACE_ENTRY(TraceType_BeginWork);
		writer.Write7BitEncoded(workIndex);
		writer.Write7BitEncoded(stringIndex);
	}

	void Trace::EndWork(u32 workIndex)
	{
		BEGIN_TRACE_ENTRY(TraceType_EndWork);
		writer.Write7BitEncoded(workIndex);
	}

	void Trace::StatusUpdate(u32 statusIndex, u32 statusNameIndent, const tchar* statusName, u32 statusTextIndent, const tchar* statusText, LogEntryType statusType)
	{
		BEGIN_TRACE_ENTRY(TraceType_StatusUpdate);
		writer.Write7BitEncoded(statusIndex);
		writer.Write7BitEncoded(statusNameIndent);
		writer.WriteString(statusName);
		writer.Write7BitEncoded(statusTextIndent);
		writer.WriteString(statusText);
		writer.WriteByte(statusType);
	}

	TraceChannel::TraceChannel(Logger& logger) : m_logger(logger)
	{
	}

	bool TraceChannel::Init(const tchar* channelName)
	{
		#if PLATFORM_WINDOWS
		StringBuffer<245> channelMutex;
		channelMutex.Append(TC("Uba")).Append(channelName).Append(TC("Channel"));
		m_memHandle = uba::CreateMemoryMappingW(m_logger, PAGE_READWRITE, 256, channelName);
		if (!m_memHandle.IsValid())
		{
			MessageBox(NULL, TC("Failed to create file mapping for trace channel"), TC("UbaVisualizer"), 0);
			return false;
		}
		bool isCreator = GetLastError() != ERROR_ALREADY_EXISTS;

		auto mhg = MakeGuard([&]() { CloseFileMapping(m_memHandle); m_memHandle = {}; });

		m_mem = MapViewOfFile(m_memHandle, FILE_MAP_WRITE, 0, 256);
		if (!m_mem)
		{
			MessageBox(NULL, TC("Failed to map file mapping for uba trace channel"), TC("UbaVisualizer"), 0);
			return false;
		}

		if (isCreator)
			*(tchar*)m_mem = 0;

		auto mg = MakeGuard([&]() { UnmapViewOfFile(m_mem, 256, channelMutex.data); m_mem = nullptr; });

		channelMutex.Append(channelName).Append(TC("Mutex"));
		m_mutex = CreateMutexW(false, channelMutex.data);
		if (!m_mutex)
			return false;

		mg.Cancel();
		mhg.Cancel();
		#endif
		return true;
	}

	TraceChannel::~TraceChannel()
	{
		#if PLATFORM_WINDOWS
		if (m_mem)
			::UnmapViewOfFile(m_mem);
		if (m_memHandle.IsValid())
			CloseFileMapping(m_memHandle);
		if (m_mutex)
			CloseHandle((HANDLE)m_mutex);
		#endif
	}

	bool TraceChannel::Write(const tchar* traceName)
	{
		#if PLATFORM_WINDOWS
		WaitForSingleObject((HANDLE)m_mutex, INFINITE);
		TStrcpy_s((tchar*)m_mem, 256, traceName);
		ReleaseMutex((HANDLE)m_mutex);
		#endif
		return true;
	}

	bool TraceChannel::Read(StringBufferBase& outTraceName)
	{
		#if PLATFORM_WINDOWS
		WaitForSingleObject((HANDLE)m_mutex, INFINITE);
		outTraceName.Append((tchar*)m_mem);
		ReleaseMutex((HANDLE)m_mutex);
		#endif
		return true;
	}
}