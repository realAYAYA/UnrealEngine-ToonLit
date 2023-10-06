// Copyright Epic Games, Inc. All Rights Reserved.
#include "ProfilingDebugging/MiscTrace.h"

#if MISCTRACE_ENABLED

#include "Trace/Trace.inl"
#include "Misc/CString.h"
#include "HAL/PlatformTLS.h"
#include "HAL/PlatformTime.h"

UE_TRACE_CHANNEL(FrameChannel)
UE_TRACE_CHANNEL(BookmarkChannel)
UE_TRACE_CHANNEL(RegionChannel)
UE_TRACE_CHANNEL(ScreenshotChannel)

UE_TRACE_EVENT_BEGIN(Misc, BookmarkSpec, NoSync|Important)
	UE_TRACE_EVENT_FIELD(const void*, BookmarkPoint)
	UE_TRACE_EVENT_FIELD(int32, Line)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, FormatString)
	UE_TRACE_EVENT_FIELD(UE::Trace::AnsiString, FileName)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(Misc, Bookmark)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	UE_TRACE_EVENT_FIELD(const void*, BookmarkPoint)
	UE_TRACE_EVENT_FIELD(uint8[], FormatArgs)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(Misc, RegionBegin)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, RegionName)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(Misc, RegionEnd)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, RegionName)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(Misc, BeginFrame)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	UE_TRACE_EVENT_FIELD(uint8, FrameType)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(Misc, EndFrame)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	UE_TRACE_EVENT_FIELD(uint8, FrameType)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(Misc, ScreenshotHeader)
	UE_TRACE_EVENT_FIELD(uint32, Id)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, Name)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	UE_TRACE_EVENT_FIELD(uint32, Width)
	UE_TRACE_EVENT_FIELD(uint32, Height)
	UE_TRACE_EVENT_FIELD(uint32, TotalChunkNum)
	UE_TRACE_EVENT_FIELD(uint32, Size)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(Misc, ScreenshotChunk)
	UE_TRACE_EVENT_FIELD(uint32, Id)
	UE_TRACE_EVENT_FIELD(uint32, ChunkNum)
	UE_TRACE_EVENT_FIELD(uint16, Size)
	UE_TRACE_EVENT_FIELD(uint8[], Data)
UE_TRACE_EVENT_END()

void FMiscTrace::OutputBookmarkSpec(const void* BookmarkPoint, const ANSICHAR* File, int32 Line, const TCHAR* Format)
{
	uint16 FileNameLen = uint16(strlen(File));
	uint16 FormatStringLen = uint16(FCString::Strlen(Format));

	uint32 DataSize = (FileNameLen * sizeof(ANSICHAR)) + (FormatStringLen * sizeof(TCHAR));
	UE_TRACE_LOG(Misc, BookmarkSpec, BookmarkChannel, DataSize)
		<< BookmarkSpec.BookmarkPoint(BookmarkPoint)
		<< BookmarkSpec.Line(Line)
		<< BookmarkSpec.FormatString(Format, FormatStringLen)
		<< BookmarkSpec.FileName(File, FileNameLen);
}

void FMiscTrace::OutputBeginRegion(const TCHAR* RegionName)
{
	UE_TRACE_LOG(Misc, RegionBegin, RegionChannel)
		<< RegionBegin.Cycle(FPlatformTime::Cycles64())
		<< RegionBegin.RegionName(RegionName);
}

void FMiscTrace::OutputEndRegion(const TCHAR* RegionName)
{
	UE_TRACE_LOG(Misc, RegionEnd, RegionChannel)
		<< RegionEnd.Cycle(FPlatformTime::Cycles64())
		<< RegionEnd.RegionName(RegionName);
}

void FMiscTrace::OutputBookmarkInternal(const void* BookmarkPoint, uint16 EncodedFormatArgsSize, uint8* EncodedFormatArgs)
{
	UE_TRACE_LOG(Misc, Bookmark, BookmarkChannel)
		<< Bookmark.Cycle(FPlatformTime::Cycles64())
		<< Bookmark.BookmarkPoint(BookmarkPoint)
		<< Bookmark.FormatArgs(EncodedFormatArgs, EncodedFormatArgsSize);
}

void FMiscTrace::OutputBeginFrame(ETraceFrameType FrameType)
{
	if (!UE_TRACE_CHANNELEXPR_IS_ENABLED(FrameChannel))
	{
		return;
	}

	uint64 Cycle = FPlatformTime::Cycles64();
	UE_TRACE_LOG(Misc, BeginFrame, FrameChannel)
		<< BeginFrame.Cycle(Cycle)
		<< BeginFrame.FrameType((uint8)FrameType);
}

void FMiscTrace::OutputEndFrame(ETraceFrameType FrameType)
{
	if (!UE_TRACE_CHANNELEXPR_IS_ENABLED(FrameChannel))
	{
		return;
	}

	uint64 Cycle = FPlatformTime::Cycles64();
	UE_TRACE_LOG(Misc, EndFrame, FrameChannel)
		<< EndFrame.Cycle(Cycle)
		<< EndFrame.FrameType((uint8)FrameType);
}

void FMiscTrace::OutputScreenshot(const TCHAR* Name, uint64 Cycle, uint32 Width, uint32 Height, TArray64<uint8> Data)
{
	static std::atomic<uint32> ScreenshotId = 0;

	const uint32 DataSize = (uint32) Data.Num();
	const uint32 MaxChunkSize = TNumericLimits<uint16>::Max();
	uint32 ChunkNum = (DataSize + MaxChunkSize - 1) / MaxChunkSize;

	uint32 Id = ScreenshotId.fetch_add(1);
	UE_TRACE_LOG(Misc, ScreenshotHeader, ScreenshotChannel)
		<< ScreenshotHeader.Id(Id)
		<< ScreenshotHeader.Name(Name, uint16(FCString::Strlen(Name)))
		<< ScreenshotHeader.Cycle(Cycle)
		<< ScreenshotHeader.Width(Width)
		<< ScreenshotHeader.Height(Height)
		<< ScreenshotHeader.TotalChunkNum(ChunkNum)
		<< ScreenshotHeader.Size(DataSize);

	uint32 RemainingSize = DataSize;
	for (uint32 Index = 0; Index < ChunkNum; ++Index)
	{
		uint16 Size = (uint16) FMath::Min(RemainingSize, MaxChunkSize);

		uint8* ChunkData = Data.GetData() + MaxChunkSize * Index;
		UE_TRACE_LOG(Misc, ScreenshotChunk, ScreenshotChannel)
			<< ScreenshotChunk.Id(Id)
			<< ScreenshotChunk.ChunkNum(Index)
			<< ScreenshotChunk.Size(Size)
			<< ScreenshotChunk.Data(ChunkData, Size);

		RemainingSize -= Size;
	}

	check(RemainingSize == 0);
}

bool FMiscTrace::ShouldTraceScreenshot()
{
	return UE_TRACE_CHANNELEXPR_IS_ENABLED(ScreenshotChannel);
}

bool FMiscTrace::ShouldTraceBookmark()
{
	return UE_TRACE_CHANNELEXPR_IS_ENABLED(BookmarkChannel);
}
#endif
