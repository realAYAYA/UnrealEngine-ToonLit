// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaLogger.h"
#include "UbaBinaryReaderWriter.h"
#include "UbaFileAccessor.h"
#include "UbaPlatform.h"
#include "UbaStringBuffer.h"
#include "UbaTimer.h"

#if PLATFORM_WINDOWS
#include <io.h>
#define Fputs fputws
#else
#define Fputs fputs
#define _vsnwprintf_s(buffer,capacity,count,format,args) vsnprintf(buffer, capacity, format, args) // TODO: This will overflow
#endif


namespace uba
{
	CustomAssertHandler* g_assertHandler;

	ANALYSIS_NORETURN void UbaAssert(const tchar* text, const char* file, u32 line, const char* expr, u32 terminateCode)
	{
		static ReaderWriterLock& assertLock = *new ReaderWriterLock(); // Leak to prevent asan annoyances during shutdown when asserts happen
		SCOPED_WRITE_LOCK(assertLock, lock);

		StringBuffer<4096> b;
		WriteAssertInfo(b, text, file, line, expr, 1);

		if (g_assertHandler)
		{
			g_assertHandler(b.data);
			return;
		}

		Fputs(b.data, stdout);
		Fputs(TC("\n"), stdout);
		fflush(stdout);

#if PLATFORM_WINDOWS
#if UBA_ASSERT_MESSAGEBOX
		int ret = MessageBoxW(GetConsoleWindow(), b.data, TC("Assert"), MB_ABORTRETRYIGNORE);
		if (ret != IDABORT)
		{
			if (ret == IDRETRY)
				DebugBreak();
			return;
		}

		SetFocus(GetConsoleWindow());
		SetActiveWindow(GetConsoleWindow());
#endif
		ExitProcess(terminateCode);
#else
		exit(-1);
#endif
	}

	void SetCustomAssertHandler(CustomAssertHandler* handler)
	{
		g_assertHandler = handler;
	}

	ANALYSIS_NORETURN void FatalError(u32 code, const tchar* format, ...)
	{
		va_list arg;
		va_start(arg, format);
		tchar buffer[1024];
		int count = TSprintf_s(buffer, 1024, format, arg);
		if (count <= 0)
			TStrcpy_s(buffer, 1024, format);
		va_end(arg);
#if PLATFORM_WINDOWS
		wprintf(TC("FATAL ERROR %u: %s\n"), code, buffer);
		fflush(stdout);
		ExitProcess(code);
#else
		printf(TC("FATAL ERROR %u: %s\n"), code, buffer);
		fflush(stdout);
		exit(int(code));
#endif
	}

	void Logger::Info(const tchar* format, ...)
	{
		va_list arg;
		va_start(arg, format);
		LogArg(LogEntryType_Info, format, arg);
		va_end(arg);
	}

	void Logger::Detail(const tchar* format, ...)
	{
		va_list arg;
		va_start(arg, format);
		LogArg(LogEntryType_Detail, format, arg);
		va_end(arg);
	}

	void Logger::Debug(const tchar* format, ...)
	{
		va_list arg;
		va_start(arg, format);
		LogArg(LogEntryType_Debug, format, arg);
		va_end(arg);
	}

	void Logger::Warning(const tchar* format, ...)
	{
		va_list arg;
		va_start(arg, format);
		LogArg(LogEntryType_Warning, format, arg);
		va_end(arg);
	}

	bool Logger::Error(const tchar* format, ...)
	{
		va_list arg;
		va_start(arg, format);
		LogArg(LogEntryType_Error, format, arg);
		va_end(arg);
		return false;
	}

	void Logger::Logf(LogEntryType type, const tchar* format, ...)
	{
		va_list arg;
		va_start(arg, format);
		LogArg(type, format, arg);
		va_end(arg);
	}

	void Logger::LogArg(LogEntryType type, const tchar* format, va_list& args)
	{
		#if !PLATFORM_WINDOWS
		constexpr size_t _TRUNCATE = (size_t)-1;
		#endif

		tchar buffer[1024];
		int len = _vsnwprintf_s(buffer, sizeof_array(buffer), _TRUNCATE, format, args);
		if (len != -1)
		{
			Log(type, buffer, u32(len));
		}
		else
		{
			Vector<tchar> buf;
			buf.resize(64 * 1024);
			_vsnwprintf_s(buf.data(), buf.size(), buf.size(), format, args);
			Log(type, buf.data(), u32(buf.size()));
		}
	}

	LoggerWithWriter::LoggerWithWriter(LogWriter& writer, const tchar* prefix)
	:	m_writer(writer)
	,	m_prefix(prefix)
	,	m_prefixLen(prefix ? u32(TStrlen(prefix)) : 0)
	{
	}

	void FilteredLogWriter::Log(LogEntryType type, const tchar* str, u32 strLen, const tchar* prefix, u32 prefixLen)
	{
		if (type > m_level)
			return;
		m_writer.Log(type, str, strLen, prefix, prefixLen);
	}

	class ConsoleLogWriter : public LogWriter
	{
	public:
		ConsoleLogWriter();
		virtual void BeginScope() override;
		virtual void EndScope() override;
		virtual void Log(LogEntryType type, const tchar* str, u32 strLen, const tchar* prefix = nullptr, u32 prefixLen = 0) override;
	private:
		void LogNoLock(LogEntryType type, const tchar* str, u32 strLen, const tchar* prefix, u32 prefixLen);
		ReaderWriterLock m_lock;
#if PLATFORM_WINDOWS
		HANDLE m_stdout = 0;
		u32 m_defaultAttributes = 0;
#endif
	};
	LogWriter& g_consoleLogWriter = *new ConsoleLogWriter(); // Leak to prevent asan annoyances during shutdown when asserts happen
	thread_local u32 t_consoleLogScopeCount = 0;

	class NullLogWriter : public LogWriter
	{
	public:
		virtual void BeginScope() override {}
		virtual void EndScope() override {}
		virtual void Log(LogEntryType type, const tchar* str, u32 strLen, const tchar* prefix = nullptr, u32 prefixLen = 0) override {}
	} g_nullLogWriterImpl;
	LogWriter& g_nullLogWriter = g_nullLogWriterImpl;


	ConsoleLogWriter::ConsoleLogWriter()
	{
#if PLATFORM_WINDOWS
		if (_isatty(_fileno(stdout)))
		{
			m_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
			CONSOLE_SCREEN_BUFFER_INFO csbi;
			GetConsoleScreenBufferInfo(m_stdout, &csbi);
			m_defaultAttributes = csbi.wAttributes;
		}
#endif
	}

	void ConsoleLogWriter::BeginScope()
	{
		if (!t_consoleLogScopeCount++)
			m_lock.EnterWrite();
	}

	void ConsoleLogWriter::EndScope()
	{
		if (--t_consoleLogScopeCount)
			return;
#if PLATFORM_WINDOWS
		if (!m_stdout)
#endif
			fflush(stdout);
		m_lock.LeaveWrite();
	}

	void ConsoleLogWriter::Log(LogEntryType type, const tchar* str, u32 strLen, const tchar* prefix, u32 prefixLen)
	{
		if (t_consoleLogScopeCount)
			return LogNoLock(type, str, strLen, prefix, prefixLen);
		SCOPED_WRITE_LOCK(m_lock, lock);
		LogNoLock(type, str, strLen, prefix, prefixLen);
#if PLATFORM_WINDOWS
		if (!m_stdout)
#endif
			fflush(stdout);
	}

	void ConsoleLogWriter::LogNoLock(LogEntryType type, const tchar* str, u32 strLen, const tchar* prefix, u32 prefixLen)
	{
#if PLATFORM_WINDOWS
		if (!m_stdout)
		{
			if (prefixLen)
			{
				Fputs(prefix, stdout);
				Fputs(TC(" - "), stdout);
			}
			_putws(str);
		}
		else
		{
			if (prefixLen)
			{
				WriteConsoleW(m_stdout, prefix, prefixLen, NULL, NULL);
				WriteConsoleW(m_stdout, TC(" - "), 3, NULL, NULL);
			}
			switch (type)
			{
			case LogEntryType_Warning:
				SetConsoleTextAttribute(m_stdout, FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY);
				WriteConsoleW(m_stdout, str, strLen, NULL, NULL);
				SetConsoleTextAttribute(m_stdout, (WORD)m_defaultAttributes);
				break;
			case LogEntryType_Error:
				SetConsoleTextAttribute(m_stdout, FOREGROUND_RED | FOREGROUND_INTENSITY);
				WriteConsoleW(m_stdout, str, strLen, NULL, NULL);
				SetConsoleTextAttribute(m_stdout, (WORD)m_defaultAttributes);
				break;
			default:
				WriteConsoleW(m_stdout, str, strLen, NULL, NULL);
				break;
			}
			WriteConsoleW(m_stdout, TC("\r\n"), 2, NULL, NULL);
		}
#else
		if (prefixLen)
		{
			Fputs(prefix, stdout);
			Fputs(TC(" - "), stdout);
		}
		Fputs(str, stdout);
		Fputs(TC("\n"), stdout);
#endif
	}

	LastErrorToText::LastErrorToText() : LastErrorToText(GetLastError())
	{
	}

	LastErrorToText::LastErrorToText(u32 lastError)
	{
#if PLATFORM_WINDOWS
		size_t size = ::FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, lastError, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), data, capacity, NULL);
		if (!size)
			AppendValue(lastError);
		else
			Resize(size - 2);
#else
		Append(strerror(int(lastError)));
#endif
	}

	BytesToText::BytesToText(u64 bytes)
	{
		if (bytes < 1000 * 1000)
			TSprintf_s(str, 32, TC("%.1fkb"), double(bytes) / 1000ull);
		else if (bytes < 1000ull * 1000 * 1000)
			TSprintf_s(str, 32, TC("%.1fmb"), double(bytes) / (1000ull * 1000));
		else
			TSprintf_s(str, 32, TC("%.1fgb"), double(bytes) / (1000ull * 1000 * 1000));
	}

#if UBA_DEBUG_LOGGER
	thread_local u32 t_debugLogScopeCount = 0;

	class DebugLogWriter : public LogWriter
	{
	public:
		virtual void BeginScope() override
		{
			if (!m_file)
				return;
			if (!t_debugLogScopeCount++)
				m_logLock.EnterWrite();
		}

		virtual void EndScope() override
		{
			if (!m_file)
				return;
			if (--t_debugLogScopeCount)
				return;
			//

			m_logLock.LeaveWrite();
		}

		virtual void Log(LogEntryType type, const tchar* str, u32 strLen, const tchar* prefix = nullptr, u32 prefixLen = 0) override
		{
			if (!m_file)
				return;
			if (t_debugLogScopeCount)
				return LogNoLock(type, str, strLen, prefix, prefixLen);
			SCOPED_WRITE_LOCK(m_logLock, lock);
			LogNoLock(type, str, strLen, prefix, prefixLen);
		}

		void LogNoLock(LogEntryType type, const tchar* str, u32 strLen, const tchar* prefix, u32 prefixLen)
		{
			#if PLATFORM_WINDOWS
			u8 buffer[2048];
			BinaryWriter writer(buffer);
			writer.WriteString(str, strLen);
			BinaryReader reader(buffer);
			u64 charLen = reader.Read7BitEncoded();
			m_file->Write(reader.GetPositionData(), charLen);
			#else
			m_file->Write(str, strLen);
			#endif
		}

		TString m_fileName;
		FileAccessor* m_file = nullptr;
		ReaderWriterLock m_logLock;

	} g_debugLogWriter;

	bool StartDebugLogger(Logger& outerLogger, const tchar* fileName)
	{
		g_debugLogWriter.m_fileName = fileName;
		auto fa = new FileAccessor(outerLogger, g_debugLogWriter.m_fileName.c_str());
		if (!fa->CreateWrite())
		{
			delete fa;
			return false;
		}

		#if PLATFORM_WINDOWS
		unsigned char utf8BOM[] = { 0xef,0xbb,0xbf }; 
		fa->Write(utf8BOM, sizeof(utf8BOM));
		#endif

		g_debugLogWriter.m_file = fa;
		return true;
	}

	void StopDebugLogger()
	{
		if (!g_debugLogWriter.m_file)
			return;
		g_debugLogWriter.m_file->Close();
		delete g_debugLogWriter.m_file;
		g_debugLogWriter.m_file = nullptr;
	}

	LoggerWithWriter g_debugLogger(g_debugLogWriter);
#endif

	#if UBA_TRACK_CONTENTION
	List<ContentionTracker>& GetContentionTrackerList();
	#endif


	void PrintContentionSummary(Logger& logger)
	{
	#if UBA_TRACK_CONTENTION
		logger.Info(TC("Contention summary:"));
		List<ContentionTracker*> list;
		for (auto& ct : GetContentionTrackerList())
			if (TimeToMs(ct.time) > 1)
				list.push_back(&ct);
		list.sort([](const ContentionTracker* a, const ContentionTracker* b)
			{
				if (a->time != b->time)
					return a->time > b->time;
				return a < b;
			});

		for (auto& ct : list)
		{
			StringBuffer<512> fn;
			fn.Append(ct->file);
			StringBuffer<256> s;
			s.Append(TC("  ")).AppendFileName(fn.data).Append(':').AppendValue(ct->line).Append(TC(" - ")).AppendValue(ct->count).Append(TC(" calls in ")).Append(TimeToText(ct->time).str);
			logger.Info(s.data);
		}
	#endif
	}
}
