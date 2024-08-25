// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaBinaryReaderWriter.h"
#include "UbaTimer.h"

namespace uba
{
	class Logger;

	inline void Write(BinaryWriter& writer, const Timer& timer) { writer.Write7BitEncoded(timer.time); writer.Write7BitEncoded(timer.count); }
	inline void Write(BinaryWriter& writer, u64 v) { writer.Write7BitEncoded(v); }
	inline void Write(BinaryWriter& writer, u32 v) { writer.Write7BitEncoded(v); }
	inline void Read(BinaryReader& reader, Timer& timer) { timer.time = reader.Read7BitEncoded(); timer.count = (u32)reader.Read7BitEncoded(); }
	inline void Read(BinaryReader& reader, u64& v) { v = reader.Read7BitEncoded(); }
	inline void Read(BinaryReader& reader, u32& v) { v = u32(reader.Read7BitEncoded()); }

	struct ProcessStats
	{
		Timer waitOnResponse;

#define UBA_PROCESS_STATS \
		UBA_PROCESS_STAT(attach, 0) \
		UBA_PROCESS_STAT(detach, 0) \
		UBA_PROCESS_STAT(init, 0) \
		UBA_PROCESS_STAT(createFile, 0) \
		UBA_PROCESS_STAT(closeFile, 0) \
		UBA_PROCESS_STAT(getFullFileName, 0) \
		UBA_PROCESS_STAT(deleteFile, 0) \
		UBA_PROCESS_STAT(moveFile, 0) \
		UBA_PROCESS_STAT(chmod, 17) \
		UBA_PROCESS_STAT(copyFile, 0) \
		UBA_PROCESS_STAT(createProcess, 0) \
		UBA_PROCESS_STAT(updateTables, 0) \
		UBA_PROCESS_STAT(listDirectory, 0) \
		UBA_PROCESS_STAT(createTempFile, 0) \
		UBA_PROCESS_STAT(openTempFile, 0) \
		UBA_PROCESS_STAT(virtualAllocFailed, 0) \
		UBA_PROCESS_STAT(log, 0) \
		UBA_PROCESS_STAT(sendFiles, 0) \
		UBA_PROCESS_STAT(writeFiles, 19) \


		#define UBA_PROCESS_STAT(T, ver) Timer T;
		UBA_PROCESS_STATS
		#undef UBA_PROCESS_STAT

		AtomicU64 startupTime = 0;
		AtomicU64 exitTime = 0;

		// Don't add in GetTotalTime()
		AtomicU64 wallTime = 0;
		AtomicU64 cpuTime = 0;

		AtomicU64 usedMemory = 0;

		AtomicU64 hostTotalTime;

		void Print(Logger& logger, u64 frequency = GetFrequency());

		u64 GetTotalTime()
		{
			return 
			#define UBA_PROCESS_STAT(T, ver) + T.time
			UBA_PROCESS_STATS
			#undef UBA_PROCESS_STAT
				;
		}

		u32 GetTotalCount()
		{
			return 
			#define UBA_PROCESS_STAT(T, ver) + T.count
				UBA_PROCESS_STATS
			#undef UBA_PROCESS_STAT
				;
		}

		void Read(BinaryReader& reader, u32 version)
		{
			uba::Read(reader, waitOnResponse);

			#define UBA_PROCESS_STAT(T, ver) if (ver <= version) uba::Read(reader, T);
			UBA_PROCESS_STATS
			#undef UBA_PROCESS_STAT

			startupTime = reader.ReadU64();
			exitTime = reader.ReadU64();
			wallTime = reader.ReadU64();
			cpuTime = reader.ReadU64();
			usedMemory = reader.ReadU32();
			hostTotalTime = reader.ReadU64();
		}

		void Write(BinaryWriter& writer) const
		{
			uba::Write(writer, waitOnResponse);

			#define UBA_PROCESS_STAT(T, ver) uba::Write(writer, T);
			UBA_PROCESS_STATS
			#undef UBA_PROCESS_STAT

			writer.WriteU64(startupTime);
			writer.WriteU64(exitTime);
			writer.WriteU64(wallTime);
			writer.WriteU64(cpuTime);
			writer.WriteU32(u32(usedMemory));
			writer.WriteU64(hostTotalTime);
		}

		void Add(const ProcessStats& other)
		{
			waitOnResponse.Add(other.waitOnResponse);
			
			#define UBA_PROCESS_STAT(T, ver) T.Add(other.T);
			UBA_PROCESS_STATS
			#undef UBA_PROCESS_STAT

			startupTime += other.startupTime;
			exitTime += other.exitTime;
			wallTime += other.wallTime;
			cpuTime += other.cpuTime;
			usedMemory = Max(usedMemory, other.usedMemory);
			hostTotalTime += other.hostTotalTime;
		}
	};

	#define UBA_SYSTEM_STATS \
		UBA_SYSTEM_STAT(createFile) \
		UBA_SYSTEM_STAT(closeFile) \
		UBA_SYSTEM_STAT(writeFile) \
		UBA_SYSTEM_STAT(readFile) \
		UBA_SYSTEM_STAT(setFileInfo) \
		UBA_SYSTEM_STAT(createFileMapping) \
		UBA_SYSTEM_STAT(mapViewOfFile) \
		UBA_SYSTEM_STAT(unmapViewOfFile) \
		UBA_SYSTEM_STAT(getFileTime) \
		UBA_SYSTEM_STAT(closeHandle) \


	struct SystemStats
	{
		#define UBA_SYSTEM_STAT(T) ExtendedTimer T;
		UBA_SYSTEM_STATS
		#undef UBA_SYSTEM_STAT

		void Read(BinaryReader& reader);
		void Write(BinaryWriter& writer);
		void Print(Logger& logger, bool writeHeader, u64 frequency = GetFrequency());
		void Add(const SystemStats& other);
		static SystemStats& GetCurrent();
		static SystemStats& GetGlobal();
	};

	struct SystemStatsScope
	{
		SystemStatsScope(SystemStats& stats);
		~SystemStatsScope();

		SystemStats& stats;
	};
}
