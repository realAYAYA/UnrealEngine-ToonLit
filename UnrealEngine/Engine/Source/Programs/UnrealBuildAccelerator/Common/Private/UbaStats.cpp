// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaStats.h"
#include "UbaLogger.h"

namespace uba
{
	void ProcessStats::Print(Logger& logger, u64 frequency)
	{
		logger.Info(TC("  Total              %8u %9s"), GetTotalCount(), TimeToText(GetTotalTime(), false, frequency).str);
		logger.Info(TC("  WaitOnResponse     %8u %9s"), waitOnResponse.count.load(), TimeToText(waitOnResponse.time, false, frequency).str);
		logger.Info(TC("  Host                %17s"), TimeToText(hostTotalTime, false, frequency).str);
		logger.Info(TC(""));

		struct Stat { const char* name; u64 nameLen; const Timer& timer; };
		Stat stats[] =
		{
			#define UBA_PROCESS_STAT(T, ver) { #T, sizeof(#T), T },
			UBA_PROCESS_STATS
			#undef UBA_PROCESS_STAT
		};

		const tchar empty[] = TC("                   ");
		for (Stat& s : stats)
			if (s.timer.count)
				logger.Info(TC("  %c") PERCENT_HS TC("%s %8u %9s"), ToUpper(s.name[0]), s.name + 1, empty + s.nameLen, s.timer.count.load(), TimeToText(s.timer.time, false, frequency).str);

		logger.Info(TC(""));

		logger.Info(TC("  HighestMem                  %9s"), BytesToText(usedMemory).str);
		logger.Info(TC("  Startup Time                %9s"), TimeToText(startupTime, false, frequency).str);
		logger.Info(TC("  Exit Time                   %9s"), TimeToText(exitTime, false, frequency).str);
		logger.Info(TC("  CPU Time                    %9s"), TimeToText(cpuTime, false, frequency).str);
		logger.Info(TC("  Wall Time                   %9s"), TimeToText(wallTime, false, frequency).str);
	}

	void SystemStats::Print(Logger& logger, bool writeHeader, u64 frequency)
	{
		if (writeHeader)
			logger.Info(TC("  --- Platform system stats summary ---"));

		struct Stat { const char* name; u64 nameLen; const Timer& timer; };
		Stat stats[] =
		{
			#define UBA_SYSTEM_STAT(T) { #T, sizeof(#T), T },
			UBA_SYSTEM_STATS
			#undef UBA_SYSTEM_STAT
		};

		const tchar empty[] = TC("                   ");
		for (Stat& s : stats)
			if (s.timer.count)
				logger.Info(TC("  %c") PERCENT_HS TC("%s %8u %9s"), ToUpper(s.name[0]), s.name+1, empty + s.nameLen, s.timer.count.load(), TimeToText(s.timer.time, false, frequency).str);

		if (writeHeader)
			logger.Info(TC(""));
	}

	void SystemStats::Add(const SystemStats& other)
	{
		#define UBA_SYSTEM_STAT(var) var += other.var;
		UBA_SYSTEM_STATS
		#undef UBA_SYSTEM_STAT
	}

	void SystemStats::Write(BinaryWriter& writer)
	{
		#define UBA_SYSTEM_STAT(var) uba::Write(writer, var);
		UBA_SYSTEM_STATS
		#undef UBA_SYSTEM_STAT
	}

	void SystemStats::Read(BinaryReader& reader)
	{
		#define UBA_SYSTEM_STAT(var) uba::Read(reader, var);
		UBA_SYSTEM_STATS
		#undef UBA_SYSTEM_STAT
	}

	void StorageStats::Write(BinaryWriter& writer)
	{
		#define UBA_STORAGE_STAT(type, var) uba::Write(writer, var);
		UBA_STORAGE_STATS
		#undef UBA_STORAGE_STAT
	}

	void StorageStats::Read(BinaryReader& reader)
	{
		#define UBA_STORAGE_STAT(type, var) uba::Read(reader, var);
		UBA_STORAGE_STATS
		#undef UBA_STORAGE_STAT
	}

	void StorageStats::Add(const StorageStats& other)
	{
		#define UBA_STORAGE_STAT(type, var) var += other.var;
		UBA_STORAGE_STATS
		#undef UBA_STORAGE_STAT
	}

	void StorageStats::Print(Logger& logger, u64 frequency)
	{
		if (calculateCasKey.count)
			logger.Info(TC("  CalculateCasKeys     %6u %9s"), calculateCasKey.count.load(), TimeToText(calculateCasKey.time, false, frequency).str);
		if (ensureCas.count)
			logger.Info(TC("  EnsureCas            %6u %9s"), ensureCas.count.load(), TimeToText(ensureCas.time, false, frequency).str);
		if (recvCas.count)
		{
			logger.Info(TC("  ReceiveCas           %6u %9s"), recvCas.count.load(), TimeToText(recvCas.time, false, frequency).str);
			logger.Info(TC("     Bytes Raw/Comp %9s %9s"), BytesToText(recvCasBytesRaw).str, BytesToText(recvCasBytesComp).str);
			if (decompressRecv.count)
				logger.Info(TC("     Decompress        %6u %9s"), decompressRecv.count.load(), TimeToText(decompressRecv.time, false, frequency).str);
		}
		if (sendCas.count)
		{
			logger.Info(TC("  SendCas              %6u %9s"), sendCas.count.load(), TimeToText(sendCas.time, false, frequency).str);
			logger.Info(TC("     Bytes Raw/Comp %9s %9s"), BytesToText(sendCasBytesRaw).str, BytesToText(sendCasBytesComp).str);
			logger.Info(TC("     Compress          %6u %9s"), compressSend.count.load(), TimeToText(compressSend.time, false, frequency).str);
		}
		if (createCas.count)
		{
			logger.Info(TC("  CreateCas            %6u %9s"), createCas.count.load(), TimeToText(createCas.time, false, frequency).str);
			logger.Info(TC("     Bytes Raw/Comp %9s %9s"), BytesToText(createCasBytesRaw).str, BytesToText(createCasBytesComp).str);
			logger.Info(TC("     Compress          %6u %9s"), compressWrite.count.load(), TimeToText(compressWrite.time, false, frequency).str);
		}
		if (copyOrLink.count)
			logger.Info(TC("  CopyOrLink           %6u %9s"), copyOrLink.count.load(), TimeToText(copyOrLink.time, false, frequency).str);
		if (copyOrLinkWait.count)
			logger.Info(TC("  CopyOrLinkWait       %6u %9s"), copyOrLinkWait.count.load(), TimeToText(copyOrLinkWait.time, false, frequency).str);
		if (decompressToMem.count)
			logger.Info(TC("  DecompressToMem      %6u %9s"), decompressToMem.count.load(), TimeToText(decompressToMem.time, false, frequency).str);
	}

	thread_local StorageStats* t_storageStats;

	StorageStats* StorageStats::GetCurrent()
	{
		return t_storageStats;
	}


	StorageStatsScope::StorageStatsScope(StorageStats& s) : stats(s)
	{
		t_storageStats = &stats;
	}

	StorageStatsScope::~StorageStatsScope()
	{
		t_storageStats = nullptr;
	}


	void SessionStats::Write(BinaryWriter& writer)
	{
		#define UBA_SESSION_STAT(type, var, ver) uba::Write(writer, var);
		UBA_SESSION_STATS
		#undef UBA_SESSION_STAT
	}

	void SessionStats::Read(BinaryReader& reader, u32 version)
	{
		#define UBA_SESSION_STAT(type, var, ver) if (ver <= version) uba::Read(reader, var);
		UBA_SESSION_STATS
		#undef UBA_SESSION_STAT
	}

	void SessionStats::Add(const SessionStats& other)
	{
		#define UBA_SESSION_STAT(type, var, ver) var += other.var;
		UBA_SESSION_STATS
		#undef UBA_SESSION_STAT
	}

	template<typename T>
	void LogStat(Logger& logger, const char* name, const T&, u64 frequency) {}

	void LogStat(Logger& logger, const char* name, const Timer& timer, u64 frequency)
	{
		if (!timer.count)
			return;
		const tchar empty[] = TC("                   ");
		logger.Info(TC("  %c") PERCENT_HS TC("%s %8u %9s"), ToUpper(name[0]), name+1, empty + strlen(name)+1, timer.count.load(), TimeToText(timer.time, false, frequency).str);
	}

	void SessionStats::Print(Logger& logger, u64 frequency)
	{
		#define UBA_SESSION_STAT(type, var, ver) LogStat(logger, #var, var, frequency);
		UBA_SESSION_STATS
		#undef UBA_SESSION_STAT
	}

	thread_local SessionStats* t_sessionStats;

	SessionStats* SessionStats::GetCurrent()
	{
		return t_sessionStats;
	}


	SessionStatsScope::SessionStatsScope(SessionStats& s) : stats(s)
	{
		t_sessionStats = &stats;
	}

	SessionStatsScope::~SessionStatsScope()
	{
		t_sessionStats = nullptr;
	}

	void SessionSummaryStats::Write(BinaryWriter& writer)
	{
		stats.Write(writer);
		#define UBA_SESSION_SUMMARY_STAT(type, var) uba::Write(writer, var);
		UBA_SESSION_SUMMARY_STATS
		#undef UBA_SESSION_SUMMARY_STAT
	}

	void SessionSummaryStats::Read(BinaryReader& reader, u32 version)
	{
		stats.Read(reader, version);
		#define UBA_SESSION_SUMMARY_STAT(type, var) uba::Read(reader, var);
		UBA_SESSION_SUMMARY_STATS
		#undef UBA_SESSION_SUMMARY_STAT
	}

	void SessionSummaryStats::Print(Logger& logger, u64 frequency)
	{
		#define UBA_SESSION_SUMMARY_STAT(T, V) LogStat(logger, #V, V, frequency);
		UBA_SESSION_SUMMARY_STATS
		#undef UBA_SESSION_SUMMARY_STAT
		stats.Print(logger, frequency);
		logger.Info(TC("  MemoryPressureWait          %9s"), TimeToText(waitMemPressure, false, frequency).str);
		logger.Info(TC("  ProcessesKilled             %9llu"), killCount);
		logger.Info(TC(""));
	}
}
