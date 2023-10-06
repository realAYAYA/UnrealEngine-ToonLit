// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ContainersFwd.h"
#include "CoreTypes.h"
#include "HAL/PreprocessorHelpers.h"
#include "Misc/Build.h"
#include "ProfilingDebugging/FormatArgsTrace.h"
#include "Trace/Config.h"

#if !defined(MISCTRACE_ENABLED)
#if UE_TRACE_ENABLED && !UE_BUILD_SHIPPING
#define MISCTRACE_ENABLED 1
#else
#define MISCTRACE_ENABLED 0
#endif
#endif

enum ETraceFrameType
{
	TraceFrameType_Game,
	TraceFrameType_Rendering,

	TraceFrameType_Count
};

struct FTraceUtils
{
	static void Encode7bit(uint64 Value, uint8*& BufferPtr)
	{
		// Writes 1 to 10 bytes for uint64 and 1 to 5 bytes for uint32.
		do
		{
			uint8 HasMoreBytes = (uint8)((Value > uint64(0x7F)) << 7);
			*(BufferPtr++) = (uint8)(Value & 0x7F) | HasMoreBytes;
			Value >>= 7;
		} while (Value > 0);
	}

	static void EncodeZigZag(int64 Value, uint8*& BufferPtr)
	{
		Encode7bit((Value << 1) ^ (Value >> 63), BufferPtr);
	}
};

#if MISCTRACE_ENABLED

class FName;

struct FMiscTrace
{
	CORE_API static void OutputBookmarkSpec(const void* BookmarkPoint, const ANSICHAR* File, int32 Line, const TCHAR* Format);
	template <typename... Types>
	static void OutputBookmark(const void* BookmarkPoint, Types... FormatArgs)
	{
		uint8 FormatArgsBuffer[4096];
		uint16 FormatArgsSize = FFormatArgsTrace::EncodeArguments(FormatArgsBuffer, FormatArgs...);
		if (FormatArgsSize)
		{
			OutputBookmarkInternal(BookmarkPoint, FormatArgsSize, FormatArgsBuffer);
		}
	}

	CORE_API static void OutputBeginRegion(const TCHAR* RegionName);
	CORE_API static void OutputEndRegion(const TCHAR* RegionName);
	
	CORE_API static void OutputBeginFrame(ETraceFrameType FrameType);
	CORE_API static void OutputEndFrame(ETraceFrameType FrameType);

	CORE_API static void OutputScreenshot(const TCHAR* Name, uint64 Cycle, uint32 Width, uint32 Height, TArray64<uint8> Data);
	CORE_API static bool ShouldTraceScreenshot();
	CORE_API static bool ShouldTraceBookmark();

private:
	CORE_API static void OutputBookmarkInternal(const void* BookmarkPoint, uint16 EncodedFormatArgsSize, uint8* EncodedFormatArgs);
	
};

#define TRACE_BOOKMARK(Format, ...) \
	static bool PREPROCESSOR_JOIN(__BookmarkPoint, __LINE__); \
	if (!PREPROCESSOR_JOIN(__BookmarkPoint, __LINE__)) \
	{ \
		FMiscTrace::OutputBookmarkSpec(&PREPROCESSOR_JOIN(__BookmarkPoint, __LINE__), __FILE__, __LINE__, Format); \
		PREPROCESSOR_JOIN(__BookmarkPoint, __LINE__) = true; \
	} \
	FMiscTrace::OutputBookmark(&PREPROCESSOR_JOIN(__BookmarkPoint, __LINE__), ##__VA_ARGS__);

#define TRACE_BEGIN_REGION(RegionName) \
	FMiscTrace::OutputBeginRegion(RegionName);

#define TRACE_END_REGION(RegionName) \
	FMiscTrace::OutputEndRegion(RegionName);

#define TRACE_BEGIN_FRAME(FrameType) \
	FMiscTrace::OutputBeginFrame(FrameType);

#define TRACE_END_FRAME(FrameType) \
	FMiscTrace::OutputEndFrame(FrameType);

#define TRACE_SCREENSHOT(Name, Cycle, Width, Height, Data) \
	FMiscTrace::OutputScreenshot(Name, Cycle, Width, Height, Data);

#define SHOULD_TRACE_SCREENSHOT() \
	FMiscTrace::ShouldTraceScreenshot()

#define SHOULD_TRACE_BOOKMARK() \
	FMiscTrace::ShouldTraceBookmark()

#else

#define TRACE_BOOKMARK(...)
#define TRACE_BEGIN_REGION(...)
#define TRACE_END_REGION(...)
#define TRACE_BEGIN_FRAME(...)
#define TRACE_END_FRAME(...)
#define TRACE_SCREENSHOT(...)
#define SHOULD_TRACE_SCREENSHOT(...) false
#define SHOULD_TRACE_BOOKMARK(...) false

#endif
