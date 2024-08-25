// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaLogWriter.h"
#include "UbaSynchronization.h"
#include <stdarg.h>

#define UBA_DEBUG_LOGGER 0

namespace uba
{
	struct LogEntry
	{
		LogEntryType type;
		const tchar* string;
	};

	class Logger
	{
	public:
		Logger() {}
		bool Error(const tchar* format, ...);
		void Warning(const tchar* format, ...);
		void Info(const tchar* format, ...);
		void Detail(const tchar* format, ...);
		void Debug(const tchar* format, ...);
		void Logf(LogEntryType type, const tchar* format, ...);
		void LogArg(LogEntryType type, const tchar* format, va_list& args);

		virtual void BeginScope() = 0;
		virtual void EndScope() = 0;
		virtual void Log(LogEntryType type, const tchar* str, u32 strLen) = 0;
		virtual ~Logger() {}
	};

	class LoggerWithWriter : public Logger
	{
	public:
		LoggerWithWriter(LogWriter& writer, const tchar* prefix = nullptr);
		virtual void BeginScope() { m_writer.BeginScope(); }
		virtual void EndScope() { m_writer.EndScope(); }
		virtual void Log(LogEntryType type, const tchar* str, u32 strLen) { m_writer.Log(type, str, strLen, m_prefix, m_prefixLen); }

		LogWriter& m_writer;
		const tchar* m_prefix;
		u32 m_prefixLen;
	};

	struct MutableLogger : public LoggerWithWriter
	{
		MutableLogger(LogWriter& writer, const tchar* prefix) : LoggerWithWriter(writer, prefix) {}
		virtual void Log(LogEntryType type, const tchar* str, u32 strLen) override { if (!isMuted) LoggerWithWriter::Log(type, str, strLen); }
		Atomic<bool> isMuted;
	};

	class FilteredLogWriter : public LogWriter
	{
	public:
		FilteredLogWriter(LogWriter& writer, LogEntryType level = LogEntryType_Detail) : m_writer(writer), m_level(level) {}
		virtual void BeginScope() override { m_writer.BeginScope(); }
		virtual void EndScope() override { m_writer.EndScope(); }
		virtual void Log(LogEntryType type, const tchar* str, u32 strLen, const tchar* prefix = nullptr, u32 prefixLen = 0) override;
	private:
		LogWriter& m_writer;
		LogEntryType m_level;
	};

	struct BytesToText
	{
		BytesToText(u64 bytes);
		operator const tchar* () const { return str; };
		tchar str[32];
	};


	#if UBA_DEBUG_LOGGER
	bool StartDebugLogger(Logger& outerLogger, const tchar* fileName);
	void StopDebugLogger();
	extern LoggerWithWriter g_debugLogger;
	#endif

	void PrintContentionSummary(class Logger& logger);
}
