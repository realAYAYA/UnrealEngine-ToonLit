// Copyright Epic Games, Inc. All Rights Reserved.

#if (defined(__AUTORTFM) && __AUTORTFM)
#include "Utils.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#include <dbghelp.h>
#else
#include <execinfo.h>
#endif

#include "Containers/StringConv.h"
#include "Misc/CoreMiscDefines.h"

DEFINE_LOG_CATEGORY(LogAutoRTFM)

namespace AutoRTFM
{

void PrettyAbort(const char* const File, const unsigned Line, const char* const Function, const char* const Expression)
{
    UE_LOG(LogAutoRTFM, Fatal, TEXT("%s:%d:%s: assertion %s failed."), ANSI_TO_TCHAR(File), Line, ANSI_TO_TCHAR(Function), ANSI_TO_TCHAR(Expression));

#if PLATFORM_WINDOWS
	__assume(false);
#else
	__builtin_unreachable();
#endif // PLATFORM_WINDOWS
}

FString GetFunctionDescription(void* FunctionPtr)
{
#if PLATFORM_WINDOWS
    // This is gross, but it works. It's possible for someone to have SymInitialized before. But if they had, then this
    // will just fail. Also, this function is called in cases where we're failing, so it's ok if we do dirty things.
    SymInitialize(GetCurrentProcess(), nullptr, true);

    DWORD64 Displacement = 0;
    DWORD64 Address = reinterpret_cast<DWORD64>(FunctionPtr);
    char Buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
    PSYMBOL_INFO Symbol = reinterpret_cast<PSYMBOL_INFO>(Buffer);
    Symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    Symbol->MaxNameLen = MAX_SYM_NAME;
    if (SymFromAddr(GetCurrentProcess(), Address, &Displacement, Symbol))
    {
        return Symbol->Name;
    }
    else
    {
        return TEXT("<error getting description>");
    }
#else // PLATFORM_WINDOWS -> so !PLATFORM_WINDOWS
    char** const symbols = backtrace_symbols(&FunctionPtr, 1);
    FString Name(ANSI_TO_TCHAR(*symbols));
    free(symbols);
    return Name;
#endif // PLATFORM_WINDOWS -> so !PLATFORM_WINDOWS
}

} // namespace AutoRTFM

#endif // defined(__AUTORTFM) && __AUTORTFM
