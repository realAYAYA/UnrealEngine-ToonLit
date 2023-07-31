// Copyright Epic Games, Inc. All Rights Reserved.

#include "DebugTools.h"
#include "AddonTools.h"
#include "CurrentOS.h"
#include "TAssValueName.h"

#include "UniString.hpp"
#include "Guard.hpp"

#include <stdexcept>

BEGIN_NAMESPACE_UE_AC

// Throw a runtime_error for error code value
void ThrowGSError(int InError, const utf8_t* InFile, int InLineNo)
{
	utf8_t FormattedMessage[1024];
	snprintf(FormattedMessage, sizeof(FormattedMessage), "Error \"%s\" at \"%s:%d\"", GetErrorName(InError), InFile,
			 InLineNo);
	throw std::runtime_error(FormattedMessage);
}

// Throw a runtime_error for null pointer
void ThrowPtrNULL(const utf8_t* InFile, int InLineNo)
{
	utf8_t FormattedMessage[1024];
	snprintf(FormattedMessage, sizeof(FormattedMessage), "Pointer NULL at \"%s:%d\"", InFile, InLineNo);
	throw std::runtime_error(FormattedMessage);
}

// Throw a runtime_error for assertion fail
void ThrowAssertionFail(const utf8_t* InFile, int InLineNo)
{
	utf8_t FormattedMessage[1024];
	snprintf(FormattedMessage, sizeof(FormattedMessage), "Assertion failed at \"%s:%d\"", InFile, InLineNo);
	throw std::runtime_error(FormattedMessage);
}

static std::set< ITraceListener* > STraceListeners;
GS::Lock						   STraceListenersAccessControl;

// Insure we aren't listeneing after being deleted
ITraceListener::~ITraceListener()
{
    if (RemoveTraceListener(this) != 0)
    {
        UE_AC_DebugF("ITraceListener inheritor must be removed before\n");
    }
}

// A new trace message
void ITraceListener::NewTrace(EP2DB /* InTraceLevel */, const utf8_string& /* InMsg */)
{
    printf("ITraceListener::NewTrace - We must not come here\n");
}

// Add to set of listeners (deadlock risk if you call it from your NewTrace implementation)
void ITraceListener::AddTraceListener(ITraceListener* InTraceListener)
{
	GS::Guard< GS::Lock > lck(STraceListenersAccessControl);
	STraceListeners.insert(InTraceListener);
}

// Remove from set of listeners (deadlock risk if you call it from a NewTrace implementation)
size_t ITraceListener::RemoveTraceListener(ITraceListener* InTraceListener)
{
	GS::Guard< GS::Lock > lck(STraceListenersAccessControl);
	return STraceListeners.erase(InTraceListener);
}

// Write string to log file
void Write2Log(const utf8_string& InMsg)
{
	static bool	 bInitialized = false;
	static FILE* LogFile = nullptr;
	if (!bInitialized)
	{
		bInitialized = true;
		GS::UniString path(GetAddonDataDirectory() + UE_AC_DirSep "DatasmithArchicadExporter.log");
#if PLATFORM_WINDOWS
		LogFile = _wfopen((const wchar_t*)path.ToUStr().Get(), L"wb");
#else
		LogFile = fopen(path.ToUtf8(), "wb");
#endif
	}
	if (LogFile)
	{
		if (fwrite(InMsg.c_str(), 1, InMsg.size(), LogFile) != InMsg.size())
		{
			printf("UE_AC::Write2Log - Write error %d\n", errno);
		}
		if (fflush(LogFile) == EOF)
		{
			printf("UE_AC::Write2Log - flush error %d\n", errno);
		}
	}
}

static void ACWriteReport(const utf8_t* FormattedMessage)
{
	GS::UniString ReportMsg(FormattedMessage, CC_UTF8);
	ReportMsg.ReplaceAll("%", "%%");
	if (ReportMsg.EndsWith('\n'))
	{
		ReportMsg.DeleteLast();
	}
	ACAPI_WriteReport(ReportMsg, false);
}

// Print to debugger
void Printf2DB(EP2DB InMsgLevel, const utf8_t* FormatString, ...)
{
	try
	{
		va_list argptr;
		va_start(argptr, FormatString);
		utf8_string FormattedMessage(VStringFormat(FormatString, argptr));
		va_end(argptr);
		if (InMsgLevel <= kP2DB_Trace)
		{
			Write2Log(FormattedMessage);
		}
		if (InMsgLevel == kP2DB_Report || InMsgLevel == kP2DB_ReportAndDebug)
		{
			ACWriteReport(FormattedMessage.c_str());
		}
		if (!STraceListeners.empty())
		{
			GS::Guard< GS::Lock > lck(STraceListenersAccessControl);
			for (ITraceListener* Listener : STraceListeners)
			{
				try
				{
					Listener->NewTrace(InMsgLevel, FormattedMessage);
				}
				catch (...)
				{
#if PLATFORM_WINDOWS
					OutputDebugStringW(L"UE_AC::Printf2DB - Catch an exception from a listener\n");
#else
					printf("UE_AC::Printf2DB - Catch an exception from a listener\n");
#endif
				}
			}
		}
#if PLATFORM_WINDOWS
		std::wstring WStr(Utf8ToUtf16(FormattedMessage.c_str()));
		OutputDebugStringW(WStr.c_str());
#else
		if (fwrite(FormattedMessage.c_str(), 1, FormattedMessage.size(), stdout) != FormattedMessage.size())
		{
			printf("UE_AC::Printf2DB - Write error %d\n", errno);
		}
#endif
	}
	catch (...)
	{
#if PLATFORM_WINDOWS
		OutputDebugStringW(L"UE_AC::Printf2DB - Catch an exception\n");
#else
		printf("UE_AC::Printf2DB - Catch an exception\n");
#endif
	}
}

// clang-format off
template <>
FAssValueName::SAssValueName TAssEnumName< APIErrCodes >::AssEnumName[] = {
	ValueName(APIERR_GENERAL),
	ValueName(APIERR_MEMFULL),
	ValueName(APIERR_CANCEL),

	ValueName(APIERR_BADID),
	ValueName(APIERR_BADINDEX),
	ValueName(APIERR_BADNAME),
	ValueName(APIERR_BADPARS),
	ValueName(APIERR_BADPOLY),
	ValueName(APIERR_BADDATABASE),
	ValueName(APIERR_BADWINDOW),
	ValueName(APIERR_BADKEYCODE),
	ValueName(APIERR_BADPLATFORMSIGN),
	ValueName(APIERR_BADPLANE),
	ValueName(APIERR_BADUSERID),
	ValueName(APIERR_BADVALUE),
	ValueName(APIERR_BADELEMENTTYPE),
	ValueName(APIERR_IRREGULARPOLY),
	ValueName(APIERR_BADEXPRESSION),

	ValueName(APIERR_NO3D),
	ValueName(APIERR_NOMORE),
	ValueName(APIERR_NOPLAN),
	ValueName(APIERR_NOLIB),
	ValueName(APIERR_NOLIBSECT),
	ValueName(APIERR_NOSEL),
	ValueName(APIERR_NOTEDITABLE),
	ValueName(APIERR_NOTSUBTYPEOF),
	ValueName(APIERR_NOTEQUALMAIN),
	ValueName(APIERR_NOTEQUALREVISION),
	ValueName(APIERR_NOTEAMWORKPROJECT),

	ValueName(APIERR_NOUSERDATA),
	ValueName(APIERR_MOREUSER),
	ValueName(APIERR_LINKEXIST),
	ValueName(APIERR_LINKNOTEXIST),
	ValueName(APIERR_WINDEXIST),
	ValueName(APIERR_WINDNOTEXIST),
	ValueName(APIERR_UNDOEMPTY),
	ValueName(APIERR_REFERENCEEXIST),
	ValueName(APIERR_NAMEALREADYUSED),

	ValueName(APIERR_ATTREXIST),
	ValueName(APIERR_DELETED),
	ValueName(APIERR_LOCKEDLAY),
	ValueName(APIERR_HIDDENLAY),
	ValueName(APIERR_INVALFLOOR),
	ValueName(APIERR_NOTMINE),
	ValueName(APIERR_NOACCESSRIGHT),
#if AC_VERSION < 24
	ValueName(APIERR_BADPROPERTYFORELEM),
	ValueName(APIERR_BADCLASSIFICATIONFORELEM),
#else
	ValueName(APIERR_BADPROPERTY),
	ValueName(APIERR_BADCLASSIFICATION),
#endif

	ValueName(APIERR_MODULNOTINSTALLED),
	ValueName(APIERR_MODULCMDMINE),
	ValueName(APIERR_MODULCMDNOTSUPPORTED),
	ValueName(APIERR_MODULCMDVERSNOTSUPPORTED),
	ValueName(APIERR_NOMODULEDATA),

	ValueName(APIERR_PAROVERLAP),
	ValueName(APIERR_PARMISSING),
	ValueName(APIERR_PAROVERFLOW),
	ValueName(APIERR_PARIMPLICIT),

	ValueName(APIERR_RUNOVERLAP),
	ValueName(APIERR_RUNMISSING),
	ValueName(APIERR_RUNOVERFLOW),
	ValueName(APIERR_RUNIMPLICIT),
	ValueName(APIERR_RUNPROTECTED),

	ValueName(APIERR_EOLOVERLAP),

	ValueName(APIERR_TABOVERLAP),

	ValueName(APIERR_NOTINIT),
	ValueName(APIERR_NESTING),
	ValueName(APIERR_NOTSUPPORTED),
	ValueName(APIERR_REFUSEDCMD),
	ValueName(APIERR_REFUSEDPAR),
	ValueName(APIERR_READONLY),
	ValueName(APIERR_SERVICEFAILED),
	ValueName(APIERR_COMMANDFAILED),
	ValueName(APIERR_NEEDSUNDOSCOPE),

	ValueName(APIERR_MISSINGCODE),
	ValueName(APIERR_MISSINGDEF),

	EnumEnd(-1)};
// clang-format on

// Return the name of error
const utf8_t* GetErrorName(GSErrCode GSErr)
{
	return TAssEnumName< APIErrCodes >::GetName((APIErrCodes)GSErr);
}

END_NAMESPACE_UE_AC
