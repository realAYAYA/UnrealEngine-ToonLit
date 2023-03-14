// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "APIEnvir.h"

#include "Definitions.hpp"

#include <string>

#ifndef __clang__
	#define __printflike(a, b)
#endif

BEGIN_NAMESPACE_UE_AC

// Throw a runtime_error for error code value
[[noreturn]] void ThrowGSError(int InGSError, const utf8_t* InFile, int InLineNo);

// Throw a runtime_error for null pointer
[[noreturn]] void ThrowPtrNULL(const utf8_t* InFile, int InLineNo);

// Throw a runtime_error for assertion fail
[[noreturn]] void ThrowAssertionFail(const utf8_t* InFile, int InLineNo);

// This define test GS error value and on an error, we throw an std::runtime_error
#define UE_AC_TestGSError(Error)                              \
	{                                                         \
		int GSError = (int)(Error);                           \
		if (GSError != 0)                                     \
		{                                                     \
			UE_AC::ThrowGSError(GSError, __FILE__, __LINE__); \
		}                                                     \
	}

// This define test an pointer and on NULL, we throw an std::runtime_error
#define UE_AC_TestPtr(Ptr)                           \
	{                                                \
		if ((Ptr) == nullptr)                        \
		{                                            \
			UE_AC::ThrowPtrNULL(__FILE__, __LINE__); \
		}                                            \
	}

// This define test an assertion and on failure, we throw an std::runtime_error
#define UE_AC_Assert(Assertion)                            \
	{                                                      \
		if (!(Assertion))                                  \
		{                                                  \
			UE_AC::ThrowAssertionFail(__FILE__, __LINE__); \
		}                                                  \
	}

// This define test error code and return from current function if an error occur
#define UE_AC_ReturnOnGSError(Error) \
	{                                \
		int GSError = (int)(Error);  \
		if (GSError != 0)            \
		{                            \
			return GSError;          \
		}                            \
	}

typedef enum
{
	kP2DB_Report = 1,
	kP2DB_Debug,
	kP2DB_ReportAndDebug,
	kP2DB_Trace,
	kP2DB_Verbose
} EP2DB;

// Write string to log file
void Write2Log(const utf8_string& InMsg);

// Print to debugger
void Printf2DB(EP2DB InMsgLevel, const utf8_t* FormatString, ...) __printflike(2, 3);

// Return the name of error
const utf8_t* GetErrorName(GS::GSErrCode GSError);

#define UE_AC_DEBUGF_ON 1
#define UE_AC_TRACEF_ON 1
#define UE_AC_VERBOSEF_ON 0

#define UE_AC_ReportF(...) UE_AC::Printf2DB(UE_AC::kP2DB_Report, __VA_ARGS__)

#if UE_AC_DEBUGF_ON
	#define UE_AC_DebugF(...) UE_AC::Printf2DB(UE_AC::kP2DB_Debug, __VA_ARGS__)
#else
	#define UE_AC_DebugF(...) (void)0
#endif

#if UE_AC_TRACEF_ON
	#define UE_AC_TraceF(...) UE_AC::Printf2DB(UE_AC::kP2DB_Trace, __VA_ARGS__)
#else
	#define UE_AC_TraceF(...) (void)0
#endif

#if UE_AC_VERBOSEF_ON
	#define UE_AC_VerboseF(...) UE_AC::Printf2DB(UE_AC::kP2DB_Verbose, __VA_ARGS__)
#else
	#define UE_AC_VerboseF(...) (void)0
#endif

#define UE_AC_ErrorMsgF(FormatString, ErrorCode) UE_AC_DebugF(FormatString, GetErrorName(ErrorCode));

#define UE_AC_TestErrorMsgF(FormatString, ErrorCode)            \
	{                                                           \
		GSErrCode e = (ErrorCode);                              \
		if (e != NoError)                                       \
		{                                                       \
			UE_AC_DebugF(FormatString, UE_AC::GetErrorName(e)); \
		}                                                       \
	}

// Interface to receive trace messages
class ITraceListener
{
public:
    // Insure we aren't listeneing after being deleted
    virtual ~ITraceListener();
    
    // A new trace message (deadlock risk do some Trace. i.e. something that will call Printf2DB)
    virtual void NewTrace(EP2DB InTraceLevel, const utf8_string& InMsg) = 0;
    
    // Add to set of listeners (deadlock risk if you call it from your NewTrace implementation)
    static void AddTraceListener(ITraceListener* InTraceListener);
    
    // Remove from set of listeners (deadlock risk if you call it from a NewTrace implementation)
    static size_t RemoveTraceListener(ITraceListener* InTraceListener);
};

END_NAMESPACE_UE_AC
