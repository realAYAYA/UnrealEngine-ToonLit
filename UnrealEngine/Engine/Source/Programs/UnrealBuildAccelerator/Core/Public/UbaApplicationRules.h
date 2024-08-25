// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaStringBuffer.h"

namespace uba
{

	class ApplicationRules
	{
	public:
		// This means that process can run entirely without console. (win32 flag DETACHED_PROCESS)
		// Uba stub out console interaction. This is an optimization that is entirely optional
		virtual bool AllowDetach()
		{
			return false;
		}

		// Some kernel functions are not detoured and can't handle detoured handles (pipe system for example)
		// This function can be used to prevent uba from detouring handles. Defaults to not detour pipes
		virtual bool CanDetour(const tchar* file)
		{
			if constexpr (IsWindows)
			{
				if (file[0] == '\\' && file[1] == '\\') // This might be too aggressive but will cover pipes etc.. might need revisit
					return false;
			}
			else
			{

			}
			return true;
		}

		// Throw-away means that the file is temporary and will not be used after process exists. (By default these are kept in memory and never touch disk)
		virtual bool IsThrowAway(const tchar* fileName, u32 fileNameLen)
		{
			return false;
		}

		// Keep file in memory
		// If this returns true it means that file will be kept in memory and never touch disk.
		virtual bool KeepInMemory(const tchar* fileName, u32 fileNameLen, const tchar* systemTemp)
		{
			return IsThrowAway(fileName, fileNameLen);
		}

		// For files that are kept in memory but shared between process (temporary files where one process write and another read)
		// This only works if file is never read outside of the same process hierarchy
		virtual bool NeedsSharedMemory(const tchar* file)
		{
			return false;
		}

		// Max file size if using memory files.
		virtual u64 FileTypeMaxSize(const StringBufferBase& file, bool isSystemOrTempFile)
		{
			return 8ull * 1024 * 1024 * 1024;
		}

		// Outputfile means that it is kept in memory and then sent back to session process which can decide to write it to disk or send it over network
		virtual bool IsOutputFile(const tchar* file, u64 fileLen)
		{
			return false;
		}

		// If returns false this means that all GetFileAttribute etc will return file-not-found on this file
		virtual bool CanExist(const tchar* file)
		{
			return true;
		}

		// Return true if the file is only read by this process or very rarely read more than once
		// This is an optimization to not store the file in the mapping table since it will not be read again and would just take up space
		virtual bool IsRarelyRead(const StringBufferBase& file)
		{
			return false;
		}

		// Return true if the file is never/rarely read after it was written.
		// This is an optimization where the written file is not kept in file mappings after written
		virtual bool IsRarelyReadAfterWritten(const tchar* fileName, u64 fileNameLen) const
		{
			return false;
		}

		virtual bool AllowStorageProxy(const StringBufferBase& file)
		{
			return !IsRarelyRead(file);
		}

		// Enable vectored exception handler. This can't be enabled for all processes because some of them actually allow
		// access violations etc and then catch them.
		virtual bool EnableVectoredExceptionHandler()
		{
			return false;
		}

		virtual bool AllowMiMalloc()
		{
			return true;// !IsRunningWine();
		}

		virtual bool AllowLoadLibrary(const tchar* libraryName)
		{
			if (Contains(libraryName, TC("nvinject.dll")))
				return false;
			return true;
		}

		virtual bool SuppressLogLine(const tchar* logLine, u32 logLineLen)
		{
			return false;
		}

		virtual bool IsExitCodeSuccess(u32 exitCode)
		{
			return true;
		}

		virtual void RepairMalformedLibPath(const tchar* path)
		{
			// Do nothing
		}
	};


	struct RulesRec { const tchar* app; ApplicationRules* rules; };

	const RulesRec* GetApplicationRules();
}
