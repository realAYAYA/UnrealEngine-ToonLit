// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaStringBuffer.h"
#include <algorithm>

#if !defined(UBA_IS_DETOURED_INCLUDE)
#define True_CreateFileW ::CreateFileW
#define True_CreateFileMappingW ::CreateFileMappingW
#define True_MapViewOfFile ::MapViewOfFile
#define True_UnmapViewOfFile ::UnmapViewOfFile
#endif

namespace uba
{
	inline bool IsKnownSystemFile(const tchar* applicationName)
	{
		// These dlls should _always_ exist on all machines so we can filter them out from the list of imports to copy

		static const wchar_t* knownSystemFiles[] = // Sorted lowercase
		{
			L"advapi32.dll",
			L"bcrypt.dll",
			L"bcryptprimitives.dll",
			L"combase.dll",
			L"concrt140.dll",
			L"crypt32.dll",
			L"cryptbase.dll",
			L"dbghelp.dll",
			L"dnsapi.dll",
			L"dwmapi.dll",
			L"dxgi.dll",
			L"fwpuclnt.dll",
			L"gdi32.dll",
			L"gdi32full.dll",
			L"glu32.dll",
			L"imagehlp.dll",
			L"imm32.dll",
			L"iphlpapi.dll",
			L"kernel32.dll",
			L"kernelbase.dll",
			L"msvcp_win.dll",
			L"msvcrt.dll",
			L"mswsock.dll",
			L"ncrypt.dll",
			L"netapi32.dll",
			L"nsi.dll",
			L"ntasn1.dll",
			L"ntdll.dll",
			L"ole32.dll",
			L"oleaut32.dll",
			L"ondemandconnroutehelper.dll",
			L"opengl32.dll",
			L"powrprof.dll",
			L"psapi.dll",
			L"rasadhlp.dll",
			L"rpcrt4.dll",
			L"rstrtmgr.dll",
			L"sechost.dll",
			L"setupapi.dll"
			L"shell32.dll",
			L"sspicli.dll",
			L"ucrtbase.dll",
			L"umpdc.dll",
			L"umppc17706.dll",
			L"user32.dll",
			L"userenv.dll",
			L"uxtheme.dll",
			L"version.dll",
			L"webio.dll",
			L"win32u.dll",
			L"winhttp.dll",
			L"winmm.dll",
			L"winnsi.dll",
			L"ws2_32.dll",
		};

		StringBuffer<> nameLower;
		if (auto lastSeparator = TStrrchr(applicationName, PathSeparator))
			nameLower.Append(lastSeparator + 1);
		else
			nameLower.Append(applicationName);
		nameLower.MakeLower();
		return std::binary_search(knownSystemFiles, knownSystemFiles + sizeof_array(knownSystemFiles), nameLower.data, [](const wchar_t* a, const wchar_t* b) { return TStrcmp(a, b) < 0; });
	}

	inline DWORD GetOffset(DWORD rva, PIMAGE_SECTION_HEADER psh, PIMAGE_NT_HEADERS pnt)
	{
		if(rva == 0)
			return rva;
		PIMAGE_SECTION_HEADER seh = psh;
		for(size_t i=0; i<pnt->FileHeader.NumberOfSections; ++i, ++seh)
			if (rva >= seh->VirtualAddress && rva < seh->VirtualAddress + seh->Misc.VirtualSize)
				break;
		return rva - seh->VirtualAddress + seh->PointerToRawData;
	} 

	inline bool FindImportsInMem(LPCWSTR fileName, void* mem, const Function<void(const tchar* import, bool isKnown)>& func)
	{
		auto hdrs = (PIMAGE_NT_HEADERS)(PCHAR(mem) + PIMAGE_DOS_HEADER(mem)->e_lfanew);   
		auto pSech=IMAGE_FIRST_SECTION(hdrs);
		__try
		{
			auto& dataDir = hdrs->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
			if(dataDir.Size == 0)
				return true; // No import table

			auto importDesc = (PIMAGE_IMPORT_DESCRIPTOR)((DWORD_PTR)mem + GetOffset(dataDir.VirtualAddress, pSech, hdrs));
			while(importDesc->Name != NULL)
			{
				const char* name = (PCHAR)((DWORD_PTR)mem + GetOffset(importDesc->Name, pSech, hdrs));
				StringBuffer<256> wname;
				wname.Append(name);
				func(wname.data, IsKnownSystemFile(wname.data));
				++importDesc;

			}
		}
		__except(GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
		{
			StringBuffer<> buf;
			buf.Appendf(L"Access violation reading %s", fileName);
			UbaAssert(buf.data, __FILE__, __LINE__, "", GetExceptionCode());
			return false;
		}
		return true;
	}

	inline bool FindImports(LPCWSTR fileName, const Function<void(const tchar* import, bool isKnown)>& func)
	{
		HANDLE fileHandle = True_CreateFileW(fileName, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, 0);
		if (fileHandle == INVALID_HANDLE_VALUE)
			return true;
		auto closeFileHandle = MakeGuard([&]() { CloseHandle(fileHandle); });
		HANDLE fileMapping = True_CreateFileMappingW(fileHandle, NULL, PAGE_READONLY, 0, 0, NULL);
		if (!fileMapping)
			return false;
		auto closeMappingHandle = MakeGuard([&]() { CloseHandle(fileMapping); });
		void* mem = True_MapViewOfFile(fileMapping, FILE_MAP_READ, 0, 0, 0);
		if (!mem)
			return false;
		auto unmap = MakeGuard([&]() { True_UnmapViewOfFile(mem); });

		return FindImportsInMem(fileName, mem, func);
	}
}
