// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaProcessStats.h"

namespace uba
{
	class Logger;

	inline void Write(BinaryWriter& writer, const AtomicU64& v) { writer.Write7BitEncoded(v.load()); }
	inline void Read(BinaryReader& reader, AtomicU64& v) { v = reader.Read7BitEncoded(); }

	#define UBA_STORAGE_STATS \
		UBA_STORAGE_STAT(Timer, calculateCasKey) \
		UBA_STORAGE_STAT(Timer, copyOrLink) \
		UBA_STORAGE_STAT(Timer, copyOrLinkWait) \
		UBA_STORAGE_STAT(Timer, ensureCas) \
		UBA_STORAGE_STAT(Timer, sendCas) \
		UBA_STORAGE_STAT(Timer, recvCas) \
		UBA_STORAGE_STAT(Timer, compressWrite) \
		UBA_STORAGE_STAT(Timer, compressSend) \
		UBA_STORAGE_STAT(Timer, decompressRecv) \
		UBA_STORAGE_STAT(Timer, decompressToMem) \
		UBA_STORAGE_STAT(Timer, handleOverflow) \
		UBA_STORAGE_STAT(AtomicU64, sendCasBytesRaw) \
		UBA_STORAGE_STAT(AtomicU64, sendCasBytesComp) \
		UBA_STORAGE_STAT(AtomicU64, recvCasBytesRaw) \
		UBA_STORAGE_STAT(AtomicU64, recvCasBytesComp) \
		UBA_STORAGE_STAT(Timer, createCas) \
		UBA_STORAGE_STAT(AtomicU64, createCasBytesRaw) \
		UBA_STORAGE_STAT(AtomicU64, createCasBytesComp) \

    struct StorageStats
	{
		#define UBA_STORAGE_STAT(type, var) type var;
		UBA_STORAGE_STATS
		#undef UBA_STORAGE_STAT

		void Write(BinaryWriter& writer);
		void Read(BinaryReader& reader);
		void Add(const StorageStats& other);
		void Print(Logger& logger, u64 frequency = GetFrequency());
		static StorageStats* GetCurrent();
	};

	struct StorageStatsScope
	{
		StorageStatsScope(StorageStats& stats);
		~StorageStatsScope();
		StorageStatsScope(const StorageStatsScope&) = delete;
		void operator=(const StorageStatsScope&) = delete;
		StorageStats& stats;
	};

	#define UBA_SESSION_STATS \
		UBA_SESSION_STAT(Timer, getFileMsg, 0) \
		UBA_SESSION_STAT(Timer, getBinaryMsg, 0) \
		UBA_SESSION_STAT(Timer, sendFileMsg, 0) \
		UBA_SESSION_STAT(Timer, listDirMsg, 0) \
		UBA_SESSION_STAT(Timer, getDirsMsg, 0) \
		UBA_SESSION_STAT(Timer, getHashesMsg, 8) \
		UBA_SESSION_STAT(Timer, deleteFileMsg, 0) \
		UBA_SESSION_STAT(Timer, copyFileMsg, 16) \
		UBA_SESSION_STAT(Timer, createDirMsg, 0) \
		UBA_SESSION_STAT(Timer, waitGetFileMsg, 10) \
		UBA_SESSION_STAT(Timer, createMmapFromFile, 12) \
		UBA_SESSION_STAT(Timer, waitMmapFromFile, 12) \

    struct SessionStats
	{
		#define UBA_SESSION_STAT(type, var, ver) type var;
		UBA_SESSION_STATS
		#undef UBA_SESSION_STAT

		void Write(BinaryWriter& writer);
		void Read(BinaryReader& reader, u32 version);
		void Add(const SessionStats& other);
		void Print(Logger& logger, u64 frequency = GetFrequency());
		static SessionStats* GetCurrent();
	};

	struct SessionStatsScope
	{
		SessionStatsScope(SessionStats& stats);
		~SessionStatsScope();
		SessionStatsScope(const SessionStatsScope&) = delete;
		void operator=(const SessionStatsScope&) = delete;
		SessionStats& stats;
	};

	#define UBA_SESSION_SUMMARY_STATS \
		UBA_SESSION_SUMMARY_STAT(Timer, storageRetrieve) \
		UBA_SESSION_SUMMARY_STAT(Timer, storageSend) \
		UBA_SESSION_SUMMARY_STAT(Timer, connectMsg) \
		UBA_SESSION_SUMMARY_STAT(Timer, getApplicationMsg) \
		UBA_SESSION_SUMMARY_STAT(Timer, procAvailableMsg) \
		UBA_SESSION_SUMMARY_STAT(Timer, procFinishedMsg) \
		UBA_SESSION_SUMMARY_STAT(Timer, procReturnedMsg) \
		UBA_SESSION_SUMMARY_STAT(Timer, pingMsg) \
		UBA_SESSION_SUMMARY_STAT(Timer, customMsg) \
		UBA_SESSION_SUMMARY_STAT(u64, waitMemPressure) \
		UBA_SESSION_SUMMARY_STAT(u64, killCount) \

    struct SessionSummaryStats
	{
		SessionStats stats;

		#define UBA_SESSION_SUMMARY_STAT(type, var) type var;
		UBA_SESSION_SUMMARY_STATS
		#undef UBA_SESSION_SUMMARY_STAT

		SessionSummaryStats() : waitMemPressure(0), killCount(0) {}
		void Write(BinaryWriter& writer);
		void Read(BinaryReader& reader, u32 version);
		void Print(Logger& logger, u64 frequency = GetFrequency());
	};

}
