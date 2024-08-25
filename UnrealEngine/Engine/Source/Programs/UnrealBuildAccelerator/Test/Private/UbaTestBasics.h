// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#define Local_GetLongPathNameW uba::GetLongPathNameW

#include "UbaBinaryReaderWriter.h"
#include "UbaFileAccessor.h"
#include "UbaProcess.h"
#include "UbaPathUtils.h"
#include "UbaPlatform.h"
#include "UbaEvent.h"
#include "UbaFile.h"
#include "UbaLogger.h"
#include "UbaThread.h"
#include "UbaTimer.h"
#include "UbaDirectoryIterator.h"

#define VA_ARGS(...) , ##__VA_ARGS__
#define UBA_TEST_CHECK(expr, fmt, ...) if (!(expr)) return logger.Error(TC(fmt) VA_ARGS(__VA_ARGS__));


namespace uba
{
	bool TestTime(Logger& logger, const StringBufferBase& rootDir)
	{
#if 0
		LoggerWithWriter consoleLogger(g_consoleLogWriter); (void)consoleLogger;
		u64 time1 = GetSystemTimeUs();
		Sleep(1000);
		u64 time2 = GetSystemTimeUs();
		u64 ms = (time2 - time1) / 1000;
		consoleLogger.Info(TC("Slept ms: %llu"), ms);

		time1 = GetTime();
		Sleep(1000);
		time2 = GetTime();
		ms = (time2 * 1000 / GetFrequency()) - (time1 * 1000 / GetFrequency());
		consoleLogger.Info(TC("Slept ms: %llu"), ms);
#endif

		return true;
	}

	bool TestEvents(Logger& logger, const StringBufferBase& rootDir)
	{
		Event ev(true);
		Thread t([&]()
			{
				Sleep(500);
				//logger.Info(TC("Setting event"));
				ev.Set();
				Sleep(500);
				return true;
			});

		if (ev.IsSet(0))
			return false;

		//logger.Info(TC("Waiting for event"));
		if (!ev.IsSet(10000))
			return false;
		//logger.Info(TC("Event was set"));

		if (t.Wait(0))
			return false;

		if (!t.Wait(2000))
			return false;
		return true;
	}

	bool TestPaths(Logger& logger, const StringBufferBase& rootDir)
	{
		const tchar* workingDir = IsWindows ? TC("e:\\dev\\") : TC("/dev/bar/");
		tchar buffer[1024];
		u32 lengthResult;

		auto TestPath = [&](const tchar* path) { return FixPath2(path, workingDir, TStrlen(workingDir), buffer, sizeof_array(buffer), &lengthResult); };

#if PLATFORM_WINDOWS
		if (!FixPath2(TC("\"e:\\temp\""), workingDir, TStrlen(workingDir), buffer, sizeof_array(buffer), &lengthResult))
			return logger.Error(TC("FixPath2 (1) failed"));
#else

		if (!TestPath(TC("/..")))
			return logger.Error(TC("FixPath2 should have failed"));
		UBA_TEST_CHECK(Equals(buffer, TC("/")), "Should not contain ..");

		if (!FixPath2(TC("/../Foo"), workingDir, TStrlen(workingDir), buffer, sizeof_array(buffer), &lengthResult))
			return logger.Error(TC("FixPath2 should have failed"));
		UBA_TEST_CHECK(Equals(buffer, TC("/Foo")), "Should not contain ..");

		if (!FixPath2(TC("/usr/bin//clang++"), workingDir, TStrlen(workingDir), buffer, sizeof_array(buffer), &lengthResult))
			return logger.Error(TC("FixPath2 should have failed"));
		UBA_TEST_CHECK(!Contains(buffer, TC("//")), "Should not contain //");
#endif

		if (!FixPath2(TC("../Foo"), workingDir, TStrlen(workingDir), buffer, sizeof_array(buffer), &lengthResult))
			return logger.Error(TC("FixPath2 (1) failed"));
		UBA_TEST_CHECK(!Contains(buffer, TC("..")), "Should not contain ..");

		if (!FixPath2(TC("@../Foo"), workingDir, TStrlen(workingDir), buffer, sizeof_array(buffer), &lengthResult))
			return logger.Error(TC("FixPath2 (1) failed"));
		UBA_TEST_CHECK(Contains(buffer, TC("..")), "Should contain ..");

		if (!FixPath2(TC("..@/Foo"), workingDir, TStrlen(workingDir), buffer, sizeof_array(buffer), &lengthResult))
			return logger.Error(TC("FixPath2 (1) failed"));
		UBA_TEST_CHECK(Contains(buffer, TC("..")), "Should contain ..");

		return true;
	}

	bool TestFiles(Logger& logger, const StringBufferBase& rootDir)
	{
		FileAccessor fileHandle(logger, TC("UbaTestFile"));
		if (!fileHandle.CreateWrite())
			return logger.Error(TC("Failed to create file for write"));

		u8 byte = 'H';
		if (!fileHandle.Write(&byte, 1))
			return false;

		if (!fileHandle.Close())
			return false;

		FileHandle fileHandle2;
		if (!OpenFileSequentialRead(logger, TC("UbaTestFile"), fileHandle2))
			return logger.Error(TC("Failed to create file for read"));

		u64 writeTime = 0;
		if (!GetFileLastWriteTime(writeTime, fileHandle2))
			return logger.Error(TC("Failed to get last written time"));

		u64 systemTime = GetSystemTimeAsFileTime();
		if (systemTime < writeTime)
			return logger.Error(TC("System time is lower than last written time"));
		if (GetFileTimeAsSeconds(systemTime) - GetFileTimeAsSeconds(writeTime) > 3)
			return logger.Error(TC("System time or last written time is wrong (system: %llu, write: %llu, diffInSec: %llu)"), systemTime, writeTime, GetFileTimeAsSeconds(systemTime) - GetFileTimeAsSeconds(writeTime));


		u8 byte2 = 0;
		if (!ReadFile(logger, TC("UbaTestFile"), fileHandle2, &byte2, 1))
			return false;

		if (!CloseFile(TC("UbaTestFile"), fileHandle2))
			return false;

		FileHandle fileHandle3;
		if (!OpenFileSequentialRead(logger, TC("NonExistingFile"), fileHandle3, false))
			return logger.Error(TC("OpenFileSequentialRead failed with non existing file"));
		if (fileHandle3 != InvalidFileHandle)
			return logger.Error(TC("OpenFileSequentialRead found file that doesn't exist"));

		if (RemoveDirectoryW(TC("TestDir")))
			return logger.Error(TC("Did not fail to remove non-existing TestDir (or were things not cleaned before test)"));
		else if (GetLastError() != ERROR_FILE_NOT_FOUND)
			return logger.Error(TC("GetLastError did not return correct error failing to remove non-existing directory TestDir"));

		if (!CreateDirectoryW(TC("TestDir")))
			return logger.Error(TC("Failed to create dir"));

		FileHandle fileHandle4;
		if (OpenFileSequentialRead(logger, TC("TestDir"), fileHandle4))
			return logger.Error(TC("This should return fail"));

		if (!RemoveDirectoryW(TC("TestDir")))
			return logger.Error(TC("Fail to remove TestDir"));

		u64 size = 0;
		if (!FileExists(logger, TC("UbaTestFile"), &size) || size != 1)
			return logger.Error(TC("UbaTestFile not found"));

		DeleteFileW(TC("UbaTestFile2"));

		if (DeleteFileW(TC("UbaTestFile2")))
			return logger.Error(TC("Did not fail to delete non-existing UbaTestFile2 (or were things not cleaned before test)"));
		else if (GetLastError() != ERROR_FILE_NOT_FOUND)
			return logger.Error(TC("GetLastError did not return correct error failing to delete non-existing file UbaTestFile2"));

		if (!CreateHardLinkW(TC("UbaTestFile2"), TC("UbaTestFile")))
			return logger.Error(TC("Failed to create hardlink from UbaTestFile to UbaTestFile2"));

		if (!DeleteFileW(TC("UbaTestFile")))
			return logger.Error(TC("Failed to delete UbaTestFile"));

		if (FileExists(logger, TC("UbaTestFile")))
			return logger.Error(TC("Found non-existing file UbaTestFile"));

		// CreateHardLinkW is a symbolic link on non-windows.. need to revisit
		#if PLATFORM_WINDOWS
		if (!FileExists(logger, TC("UbaTestFile2")))
			return logger.Error(TC("Failed to find file UbaTestFile2"));

		StringBuffer<> currentDir;
		if (!GetCurrentDirectoryW(currentDir))
			return logger.Error(TC("GetCurrentDirectoryW failed"));

		bool foundFile = false;
		if (!TraverseDir(logger, currentDir.data, [&](const DirectoryEntry& de) { foundFile |= TStrcmp(de.name, TC("UbaTestFile2")) == 0; }, true))
			return logger.Error(TC("Failed to TraverseDir '.'"));

		if (!foundFile)
			return logger.Error(TC("Did not find UbaTestFile2 with TraverseDir"));

		if (!DeleteFileW(TC("UbaTestFile2")))
			return false;
		#endif

		LoggerWithWriter nullLogger(g_nullLogWriter);
		if (TraverseDir(nullLogger, TC("TestDir2"), [&](const DirectoryEntry&) {}, true))
			return logger.Error(TC("TraverseDir failed to report fail on non existing dir"));

		return true;
	}

	bool TestMemoryBlock(Logger& logger, const StringBufferBase& rootDir)
	{
		MemoryBlock block(1024 * 1024);
		u64* mem = (u64*)block.Allocate(8, 1, TC("Foo"));
		*mem = 0x1234;
		block.Free(mem);
		return true;
	}

	bool TestParseArguments(Logger& logger, const StringBufferBase& rootDir)
	{
		Vector<TString> arguments;
		ParseArguments(arguments, TC("foo bar"));
		UBA_TEST_CHECK(arguments.size() == 2, "ParseArguments 1 failed (%llu)", arguments.size());

		Vector<TString> arguments2;
		ParseArguments(arguments2, TC("\"foo\" bar"));
		UBA_TEST_CHECK(arguments2.size() == 2, "ParseArguments 2 failed");

		Vector<TString> arguments3;
		ParseArguments(arguments3, TC("\"foo meh\" bar"));
		UBA_TEST_CHECK(arguments3.size() == 2, "ParseArguments 3 failed");
		UBA_TEST_CHECK(Contains(arguments3[0].data(), TC(" ")), "ParseArguments 3 failed");

		Vector<TString> arguments4;
		ParseArguments(arguments4, TC("\"app\" @\"rsp\""));
		UBA_TEST_CHECK(arguments4.size() == 2, "ParseArguments 4 failed");
		UBA_TEST_CHECK(!Contains(arguments4[1].data(), TC("\"")), "ParseArguments 4 failed");

		Vector<TString> arguments5;
		ParseArguments(arguments5, TC("\"app\" @\"rsp foo\""));
		UBA_TEST_CHECK(arguments5.size() == 2, "ParseArguments 4 failed");
		UBA_TEST_CHECK(!Contains(arguments5[1].data(), TC("\"")), "ParseArguments 5 failed");
		UBA_TEST_CHECK(Contains(arguments5[1].data(), TC(" ")), "ParseArguments 5 failed");

		Vector<TString> arguments6;
		ParseArguments(arguments6, TC("\"app\"\"1\" @\"rsp foo\""));
		UBA_TEST_CHECK(arguments6.size() == 2, "ParseArguments 6 failed");
		UBA_TEST_CHECK(Equals(arguments6[0].data(), TC("app1")), "ParseArguments 6 failed");

		Vector<TString> arguments7;
		ParseArguments(arguments7, TC("app \" \\\"foo\\\" bar\""));
		UBA_TEST_CHECK(arguments7.size() == 2, "ParseArguments 7 failed");
		UBA_TEST_CHECK(Contains(arguments7[1].data(), TC("\"")), "ParseArguments 7 failed");
		return true;
	}

	bool TestBinaryWriter(Logger& logger, const StringBufferBase& rootDir)
	{
		auto testString = [&](const tchar* str)
		{
			u8 mem[1024];
			BinaryWriter writer(mem);
			writer.WriteString(str);
			BinaryReader reader(mem);
			TString s = reader.ReadString();
			if (s.size() != TStrlen(str))
				return logger.Error(TC("Serialized string '%s' has wrong strlen"), str);
			if (s != str)
				return logger.Error(TC("Serialized string '%s' is different from source"), str);
			return true;
		};

		if (!testString(TC("Foo")))
			return false;

		#if PLATFORM_WINDOWS
		tchar str1[] = { 54620, 44544, 0 };
		if (!testString(str1))
			return false;
		tchar str2[] = { 'f', 54620, 'o', 44544, 0 };
		if (!testString(str2))
			return false;
		#endif

		return true;
	}
}
