// Copyright Epic Games, Inc. All Rights Reserved.

#include "Microsoft/MicrosoftPlatformStackWalk.h"

#include "Misc/Paths.h"

#include "Microsoft/AllowMicrosoftPlatformTypes.h"
	#include <psapi.h>
#include "Microsoft/HideMicrosoftPlatformTypes.h"

bool FMicrosoftPlatformStackWalk::ExtractInfoFromModule(void* ProcessHandle, void* Module, FStackWalkModuleInfo& OutInfo)
{
	MODULEINFO ModuleInfo = { 0 };
	WCHAR ModuleName[MAX_PATH] = { 0 };
	WCHAR ImageName[MAX_PATH] = { 0 };
#if PLATFORM_64BITS
	static_assert(sizeof(MODULEINFO) == 24, "Broken alignment for 64bit Windows include.");
#else
	static_assert(sizeof(MODULEINFO) == 12, "Broken alignment for 32bit Windows include.");
#endif
	GetModuleInformation(ProcessHandle, (HMODULE)Module, &ModuleInfo, sizeof(ModuleInfo));
	GetModuleFileNameEx(ProcessHandle, (HMODULE)Module, ImageName, MAX_PATH);
	GetModuleBaseName(ProcessHandle, (HMODULE)Module, ModuleName, MAX_PATH);

	const uint8* ModuleBase = reinterpret_cast<const uint8*>(ModuleInfo.lpBaseOfDll);
	const auto* DosHeader = reinterpret_cast<const IMAGE_DOS_HEADER*>(ModuleBase);
	const auto* NtHeaders = reinterpret_cast<const IMAGE_NT_HEADERS*>(ModuleBase + DosHeader->e_lfanew);
	bool bValid = DosHeader->e_magic == IMAGE_DOS_SIGNATURE && NtHeaders->Signature == IMAGE_NT_SIGNATURE;
	if (!bValid)
	{
		return false;
	}

	const IMAGE_OPTIONAL_HEADER& OptionalHeader = NtHeaders->OptionalHeader;
	OutInfo.BaseOfImage = (uint64)ModuleInfo.lpBaseOfDll;
	OutInfo.ImageSize = OptionalHeader.SizeOfImage;
	OutInfo.TimeDateStamp = NtHeaders->FileHeader.TimeDateStamp;
	FString BaseModuleName = FPaths::GetBaseFilename(ModuleName);
	FCString::Strncpy(OutInfo.ModuleName, *BaseModuleName, 32);
	FCString::Strcpy(OutInfo.LoadedImageName, ImageName);
	FCString::Strcpy(OutInfo.ImageName, ImageName);

	// Find the guid and age of the binary, used to match debug files
	const IMAGE_DATA_DIRECTORY& DebugInfoEntry = OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];
	const auto* DebugEntries = (IMAGE_DEBUG_DIRECTORY*)(ModuleBase + DebugInfoEntry.VirtualAddress);
	for (uint32 i = 0, n = DebugInfoEntry.Size / sizeof(DebugEntries[0]); i < n; ++i)
	{
		const IMAGE_DEBUG_DIRECTORY& Entry = DebugEntries[i];
		if (Entry.Type == IMAGE_DEBUG_TYPE_CODEVIEW)
		{
			struct FCodeView7
			{
				uint32  Signature;
				uint32  Guid[4];
				uint32  Age;
			};

			if (Entry.SizeOfData < sizeof(FCodeView7))
			{
				continue;
			}

			const auto* CodeView7 = (FCodeView7*)(ModuleBase + Entry.AddressOfRawData);
			if (CodeView7->Signature != 'SDSR')
			{
				continue;
			}

			static_assert(sizeof(OutInfo.PdbSig70) == sizeof(uint32) * 4);
			FMemory::Memcpy(&OutInfo.PdbSig70, CodeView7->Guid, sizeof(OutInfo.PdbSig70));
			OutInfo.PdbAge = CodeView7->Age;
			break;
		}
	}

	return true;
}