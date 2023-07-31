// Copyright Epic Games, Inc. All Rights Reserved.

#include "Trace/Trace.inl" // should be Config.h :(

#if UE_TRACE_ENABLED

#include "Misc/CString.h"
#include "Trace/Detail/Channel.h"


namespace UE {
namespace Trace {

namespace Private
{

////////////////////////////////////////////////////////////////////////////////
void	Writer_MemorySetHooks(AllocFunc, FreeFunc);
void	Writer_Initialize(const FInitializeDesc&);
void	Writer_WorkerCreate();
void	Writer_Shutdown();
void	Writer_Update();
bool	Writer_SendTo(const ANSICHAR*, uint32);
bool	Writer_WriteTo(const ANSICHAR*);
bool	Writer_WriteSnapshotTo(const ANSICHAR*);
bool	Writer_IsTracing();
bool	Writer_Stop();
uint32	Writer_GetThreadId();

extern FStatistics GTraceStatistics;

} // namespace Private



////////////////////////////////////////////////////////////////////////////////
template <int DestSize, typename SRC_TYPE>
static uint32 ToAnsiCheap(ANSICHAR (&Dest)[DestSize], const SRC_TYPE* Src)
{
	const SRC_TYPE* Cursor = Src;
	for (ANSICHAR& Out : Dest)
	{
		Out = ANSICHAR(*Cursor++ & 0x7f);
		if (Out == '\0')
		{
			break;
		}
	}
	Dest[DestSize - 1] = '\0';
	return uint32(UPTRINT(Cursor - Src));
};

////////////////////////////////////////////////////////////////////////////////
void SetMemoryHooks(AllocFunc Alloc, FreeFunc Free)
{
	Private::Writer_MemorySetHooks(Alloc, Free);
}

////////////////////////////////////////////////////////////////////////////////
void Initialize(const FInitializeDesc& Desc)
{
	Private::Writer_Initialize(Desc);
	FChannel::Initialize();
}

////////////////////////////////////////////////////////////////////////////////
void Shutdown()
{
	Private::Writer_Shutdown();
}

////////////////////////////////////////////////////////////////////////////////
void Update()
{
	Private::Writer_Update();
}

////////////////////////////////////////////////////////////////////////////////
void GetStatistics(FStatistics& Out)
{
	Out = Private::GTraceStatistics;
}

////////////////////////////////////////////////////////////////////////////////
bool SendTo(const TCHAR* InHost, uint32 Port)
{
	char Host[256];
	ToAnsiCheap(Host, InHost);
	return Private::Writer_SendTo(Host, Port);
}

////////////////////////////////////////////////////////////////////////////////
bool WriteTo(const TCHAR* InPath)
{
	char Path[512];
	ToAnsiCheap(Path, InPath);
	return Private::Writer_WriteTo(Path);
}

////////////////////////////////////////////////////////////////////////////////
bool WriteSnapshotTo(const TCHAR* InPath)
{
	char Path[512];
	ToAnsiCheap(Path, InPath);
	return Private::Writer_WriteSnapshotTo(Path);
}

////////////////////////////////////////////////////////////////////////////////
bool IsTracing()
{
	return Private::Writer_IsTracing();
}

////////////////////////////////////////////////////////////////////////////////
bool Stop()
{
	return Private::Writer_Stop();
}

////////////////////////////////////////////////////////////////////////////////
bool IsChannel(const TCHAR* ChannelName)
{
	ANSICHAR ChannelNameA[64];
	ToAnsiCheap(ChannelNameA, ChannelName);
	return FChannel::FindChannel(ChannelNameA) != nullptr;
}

////////////////////////////////////////////////////////////////////////////////
bool ToggleChannel(const TCHAR* ChannelName, bool bEnabled)
{
	ANSICHAR ChannelNameA[64];
	ToAnsiCheap(ChannelNameA, ChannelName);
	return FChannel::Toggle(ChannelNameA, bEnabled);
}

////////////////////////////////////////////////////////////////////////////////
void EnumerateChannels(ChannelIterFunc IterFunc, void* User)
{
	FChannel::EnumerateChannels(IterFunc, User);
}

////////////////////////////////////////////////////////////////////////////////
void StartWorkerThread()
{
	Private::Writer_WorkerCreate();
}


////////////////////////////////////////////////////////////////////////////////
UE_TRACE_CHANNEL_EXTERN(TraceLogChannel)

UE_TRACE_EVENT_BEGIN($Trace, ThreadInfo, NoSync|Important)
	UE_TRACE_EVENT_FIELD(uint32, ThreadId)
	UE_TRACE_EVENT_FIELD(uint32, SystemId)
	UE_TRACE_EVENT_FIELD(int32, SortHint)
	UE_TRACE_EVENT_FIELD(AnsiString, Name)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN($Trace, ThreadGroupBegin, NoSync|Important)
	UE_TRACE_EVENT_FIELD(AnsiString, Name)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN($Trace, ThreadGroupEnd, NoSync|Important)
UE_TRACE_EVENT_END()

////////////////////////////////////////////////////////////////////////////////
void ThreadRegister(const TCHAR* Name, uint32 SystemId, int32 SortHint)
{
	ANSICHAR NameA[96];

	uint32 ThreadId = Private::Writer_GetThreadId();
	uint32 NameLen = ToAnsiCheap(NameA, Name);
	UE_TRACE_LOG($Trace, ThreadInfo, TraceLogChannel, NameLen * sizeof(ANSICHAR))
		<< ThreadInfo.ThreadId(ThreadId)
		<< ThreadInfo.SystemId(SystemId)
		<< ThreadInfo.SortHint(SortHint)
		<< ThreadInfo.Name(NameA, NameLen);
}

////////////////////////////////////////////////////////////////////////////////
void ThreadGroupBegin(const TCHAR* Name)
{
	ANSICHAR NameA[96];

	uint32 NameLen = ToAnsiCheap(NameA, Name);
	UE_TRACE_LOG($Trace, ThreadGroupBegin, TraceLogChannel, NameLen * sizeof(ANSICHAR))
		<< ThreadGroupBegin.Name(Name, NameLen);
}

////////////////////////////////////////////////////////////////////////////////
void ThreadGroupEnd()
{
	UE_TRACE_LOG($Trace, ThreadGroupEnd, TraceLogChannel);
}

} // namespace Trace
} // namespace UE

#else

// Workaround for module not having any exported symbols
TRACELOG_API int TraceLogExportedSymbol = 0;

#endif // UE_TRACE_ENABLED
