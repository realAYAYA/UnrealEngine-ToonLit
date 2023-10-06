// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "Logging/LogVerbosity.h"
#include "Templates/Function.h"
#include "TraceServices/Containers/Tables.h"
#include "TraceServices/Model/AnalysisSession.h"
#include "UObject/NameTypes.h"

namespace TraceServices
{
class IUntypedTable;

struct FLogCategoryInfo
{
	const TCHAR* Name = nullptr;
	ELogVerbosity::Type DefaultVerbosity;
};

struct FLogMessageInfo
{
	uint64 Index;
	double Time;
	const FLogCategoryInfo* Category = nullptr;
	const TCHAR* File = nullptr;
	const TCHAR* Message = nullptr;
	int32 Line;
	ELogVerbosity::Type Verbosity;
};

class ILogProvider
	: public IProvider
{
public:
	virtual ~ILogProvider() = default;
	virtual uint64 GetMessageCount() const = 0;
	virtual void EnumerateMessages(double IntervalStart, double IntervalEnd, TFunctionRef<void(const FLogMessageInfo&)> Callback) const = 0;
	virtual void EnumerateMessagesByIndex(uint64 Start, uint64 End, TFunctionRef<void(const FLogMessageInfo&)> Callback) const = 0;
	virtual bool ReadMessage(uint64 Index, TFunctionRef<void(const FLogMessageInfo&)> Callback) const = 0;
	virtual uint64 GetCategoryCount() const = 0;
	virtual void EnumerateCategories(TFunctionRef<void(const FLogCategoryInfo&)> Callback) const = 0;
	virtual const IUntypedTable& GetMessagesTable() const = 0;
};

class IEditableLogProvider
	: public IEditableProvider
{
public:
	virtual ~IEditableLogProvider() = default;

	/*
	* Register a new log message category.
	*
	* @return The category identity.
	*/
	virtual uint64 RegisterCategory() = 0;

	/*
	* Fetch the data structure for a log category.
	*
	* @param CategoryPointer	The unique identity (memory address) of the category instrumentation.
	*
	* @return A reference to the category structure.
	*/
	virtual FLogCategoryInfo& GetCategory(uint64 CategoryPointer) = 0;

	/*
	* Update a log message's Category information.
	*
	* @param LogPoint			The log message to update.
	* @param InCategoryPointer	The category.
	*/
	virtual void UpdateMessageCategory(uint64 LogPoint, uint64 InCategoryPointer) = 0;

	/*
	* Update a log message's format string.
	*
	* @param LogPoint			The log message to update.
	* @param InFormatString		The format string whose memory is stored in the Session.
	*/
	virtual void UpdateMessageFormatString(uint64 LogPoint, const TCHAR* InFormatString) = 0;

	/*
	* Update a log message's file location.
	*
	* @param LogPoint	The log message to update.
	* @param InFile		The file path of the message's location.
	* @param InLine		The line number of the message's location.
	*/
	virtual void UpdateMessageFile(uint64 LogPoint, const TCHAR* InFile, int32 InLine) = 0;

	/*
	* Update a log message's verbosity.
	*
	* @param LogPoint		The log message to update.
	* @param InVerbosity	The verbosity of the message.
	*/
	virtual void UpdateMessageVerbosity(uint64 LogPoint, ELogVerbosity::Type InVerbosity) = 0;

	/*
	* Update a log message's information.
	*
	* @param LogPoint			The log message to update.
	* @param InCategoryPointer	The category.
	* @param InFormatString		The format string whose memory is stored in the Session.
	* @param InFile				The file path of the message's location.
	* @param InLine				The line number of the message's location.
	* @param InVerbosity		The verbosity of the message.
	*/
	virtual void UpdateMessageSpec(uint64 LogPoint, uint64 InCategoryPointer, const TCHAR* InFormatString, const TCHAR* InFile, int32 InLine, ELogVerbosity::Type InVerbosity) = 0;

	/*
	* Append a new instance of a message from the trace session.
	*
	* @param LogPoint	The unique identity (memory address) of the message instrumentation.
	* @param Time		The time in seconds of the event.
	* @param FormatArgs	The arguments to use in conjunction with the spec's FormatString.
	*/
	virtual void AppendMessage(uint64 LogPoint, double Time, const uint8* FormatArgs) = 0;

	/*
	* Append a new instance of a message from the trace session.
	*
	* @param LogPoint	The unique identity (memory address) of the message instrumentation.
	* @param Time		The time in seconds of the event.
	* @param Text		The the message text.
	*/
	virtual void AppendMessage(uint64 LogPoint, double Time, const TCHAR* Text) = 0;
};

TRACESERVICES_API FName GetLogProviderName();
TRACESERVICES_API const ILogProvider& ReadLogProvider(const IAnalysisSession& Session);
TRACESERVICES_API IEditableLogProvider& EditLogProvider(IAnalysisSession& Session);
TRACESERVICES_API void FormatString(TCHAR* OutputString, uint32 OutputStringCount, const TCHAR* FormatString, const uint8* FormatArgs);

} // namespace TraceServices
