// Copyright Epic Games, Inc. All Rights Reserved.

#include "CurrentOS.h"

#include <stdexcept>

#include <Shlobj.h>

BEGIN_NAMESPACE_UE_AC

// Throw a runtime_error for last windows error
void ThrowWinError(DWORD InWinErr, const utf8_t* InFile, int InLineNo)
{
	wchar_t WinMsg[200];
	if (!FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, InWinErr,
						MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), WinMsg, sizeof(WinMsg) / sizeof(WinMsg[0]), nullptr))
	{
		WinMsg[0] = 0;
	}

	char FormattedMessage[1024];
	snprintf(FormattedMessage, sizeof(FormattedMessage), "Error %ld=\"%s\" at \"%s:%d\"", InWinErr,
			 Utf16ToUtf8(WinMsg).c_str(), InFile, InLineNo);

	throw std::runtime_error(FormattedMessage);
}

std::wstring Utf8ToUtf16(const utf8_t* InUtfString)
{
	std::wstring WCharString;
	int			 LUtf16 = MultiByteToWideChar(CP_UTF8, 0, InUtfString, -1, nullptr, 0);
	if (LUtf16 > 0)
	{
		WCharString.resize(LUtf16 - 1);
		int LUtf16_2 = MultiByteToWideChar(CP_UTF8, 0, InUtfString, -1, LPWSTR(WCharString.c_str()), LUtf16);
		if (LUtf16 != LUtf16_2)
		{
			OutputDebugStringW(L"Utf8ToUtf16 - Inconsistant lenght\n");
		}
	}
	else
	{
		OutputDebugStringW(L"Utf8ToUtf16 - Invalid Utf8 string\n");
	}
	return WCharString;
}

utf8_string Utf16ToUtf8(const wchar_t* InWCharString)
{
	utf8_string Utf8String;
	int			LUtf8 = WideCharToMultiByte(CP_UTF8, 0, InWCharString, -1, nullptr, 0, nullptr, nullptr);
	if (LUtf8 > 0)
	{
		Utf8String.resize(LUtf8 - 1);
		int LUtf8_2 =
			WideCharToMultiByte(CP_UTF8, 0, InWCharString, -1, LPSTR(Utf8String.c_str()), LUtf8, nullptr, nullptr);
		if (LUtf8 != LUtf8_2)
		{
			OutputDebugStringW(L"Utf16ToUtf8 - Inconsistant lenght\n");
		}
	}
	else
	{
		OutputDebugStringW(L"Utf16ToUtf8 - Invalid Utf8 string\n");
	}
	return Utf8String;
}

VecStrings GetPrefLanguages()
{
	VecStrings Languages;

	ULONG NumLanguages = 0;
	WCHAR PrefLanguages[10240];
	ULONG BufSize = sizeof(PrefLanguages) / sizeof(PrefLanguages[0]);
	if (GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &NumLanguages, PrefLanguages, &BufSize))
	{
		for (WCHAR* IterLanguage = PrefLanguages; *IterLanguage; IterLanguage += wcslen(IterLanguage) + 1)
		{
			Languages.push_back(Utf16ToUtf8(IterLanguage));
		}
	}
	return Languages;
}

// Return the user app support directory
GS::UniString GetApplicationSupportDirectory()
{
	WCHAR Path[MAX_PATH];
	SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, Path);
	return GS::UniString((GS::UniChar::Layout*)Path);
}

// Return the user home directory
GS::UniString GetHomeDirectory()
{
	WCHAR Path[MAX_PATH];
	SHGetFolderPathW(NULL, CSIDL_PROFILE, NULL, 0, Path);
	return GS::UniString((GS::UniChar::Layout*)Path);
}

void SetThreadName(const char* InName)
{
#if defined(DEBUG) || 1
	const DWORD MS_VC_EXCEPTION = 0x406D1388;

	#pragma pack(push, 8)
	typedef struct tagTHREADNAME_INFO
	{
		DWORD  dwType; // Must be 0x1000.
		LPCSTR szName; // Pointer to name (in user addr space).
		DWORD  dwThreadID; // Thread ID (-1=caller thread).
		DWORD  dwFlags; // Reserved for future use, must be zero.
	} THREADNAME_INFO;
	#pragma pack(pop)

	THREADNAME_INFO info;
	info.dwType = 0x1000;
	info.szName = InName;
	info.dwThreadID = (DWORD)-1;
	info.dwFlags = 0;

	// This will work only when we are debugged. The __try/__except block is there for when there are no debugger.
	__try
	{
		RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
	}
#else
	(void)InName;
#endif
}

END_NAMESPACE_UE_AC
