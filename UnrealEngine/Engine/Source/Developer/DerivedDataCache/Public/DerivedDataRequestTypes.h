// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/StringFwd.h"

#define UE_API DERIVEDDATACACHE_API

class FCbFieldView;
class FCbWriter;
enum class EQueuedWorkPriority : uint8;

namespace UE::DerivedData
{

/** Priority for scheduling a request. */
enum class EPriority : uint8
{
	/**
	 * Lowest is the minimum priority for asynchronous requests, and primarily for requests that are
	 * speculative in nature, while minimizing impact on other requests.
	 */
	Lowest,
	/**
	 * Low is intended for requests that are below the default priority, but are used for operations
	 * that the program will execute now rather than at an unknown time in the future.
	 */
	Low,
	/**
	 * Normal is intended as the default request priority.
	 */
	Normal,
	/**
	 * High is intended for requests that are on the critical path, but are not required to maintain
	 * interactivity of the program.
	 */
	High,
	/**
	 * Highest is the maximum priority for asynchronous requests, and intended for requests that are
	 * required to maintain interactivity of the program.
	 */
	Highest,
	/**
	 * Blocking is to be used only when the thread making the request will wait on completion of the
	 * request before doing any other work. Requests at this priority level will be processed before
	 * any request at a lower priority level. This priority permits a request to be processed on the
	 * thread making the request. Waiting on a request may increase its priority to this level.
	 */
	Blocking,
};

/** Append a non-empty text version of the priority to the builder. */
UE_API FAnsiStringBuilderBase& operator<<(FAnsiStringBuilderBase& Builder, EPriority Priority);
UE_API FWideStringBuilderBase& operator<<(FWideStringBuilderBase& Builder, EPriority Priority);
UE_API FUtf8StringBuilderBase& operator<<(FUtf8StringBuilderBase& Builder, EPriority Priority);

/** Try to parse a priority from text written by operator<<. */
UE_API bool TryLexFromString(EPriority& OutPriority, FUtf8StringView String);
UE_API bool TryLexFromString(EPriority& OutPriority, FWideStringView String);

UE_API FCbWriter& operator<<(FCbWriter& Writer, EPriority Priority);
UE_API bool LoadFromCompactBinary(FCbFieldView Field, EPriority& OutPriority, EPriority Default = EPriority::Normal);

/** Converts to a queued work priority. Asserts on invalid priority values. */
UE_API EQueuedWorkPriority ConvertToQueuedWorkPriority(EPriority Priority);
/** Converts from a queued work priority. Asserts on invalid priority values. */
UE_API EPriority ConvertFromQueuedWorkPriority(EQueuedWorkPriority Priority);

/** Status of a request that has completed. */
enum class EStatus : uint8
{
	/** The request completed successfully. Any requested data is available. */
	Ok,
	/** The request completed unsuccessfully. Any requested data is not available. */
	Error,
	/** The request was canceled before it completed. Any requested data is not available. */
	Canceled,
};

/** Append a non-empty text version of the status to the builder. */
UE_API FAnsiStringBuilderBase& operator<<(FAnsiStringBuilderBase& Builder, EStatus Status);
UE_API FWideStringBuilderBase& operator<<(FWideStringBuilderBase& Builder, EStatus Status);
UE_API FUtf8StringBuilderBase& operator<<(FUtf8StringBuilderBase& Builder, EStatus Status);

/** Try to parse a status from text written by operator<<. */
UE_API bool TryLexFromString(EStatus& OutStatus, FUtf8StringView String);
UE_API bool TryLexFromString(EStatus& OutStatus, FWideStringView String);

UE_API FCbWriter& operator<<(FCbWriter& Writer, EStatus Status);
UE_API bool LoadFromCompactBinary(FCbFieldView Field, EStatus& OutStatus, EStatus Default = EStatus::Ok);

} // UE::DerivedData

#undef UE_API
