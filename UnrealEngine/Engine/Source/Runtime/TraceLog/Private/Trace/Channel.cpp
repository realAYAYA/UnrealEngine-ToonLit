// Copyright Epic Games, Inc. All Rights Reserved.

#include "Trace/Detail/Channel.h"
#include "Math/UnrealMathUtility.h"
#include "Trace/Trace.h"
#include "Trace/Trace.inl"
#include "Trace/Detail/Atomic.h"
#include "Trace/Detail/Channel.h"

#include <ctype.h>

#if UE_TRACE_ENABLED

namespace UE {
namespace Trace {

////////////////////////////////////////////////////////////////////////////////
struct FTraceChannel : public FChannel
{
	bool IsEnabled() const { return true; }
	explicit operator bool() const { return true; }
};

static FTraceChannel	TraceLogChannelDetail;
FChannel&				TraceLogChannel			= TraceLogChannelDetail;

///////////////////////////////////////////////////////////////////////////////
UE_TRACE_EVENT_BEGIN(Trace, ChannelAnnounce, NoSync|Important)
	UE_TRACE_EVENT_FIELD(uint32, Id)
	UE_TRACE_EVENT_FIELD(bool, IsEnabled)
	UE_TRACE_EVENT_FIELD(bool, ReadOnly)
	UE_TRACE_EVENT_FIELD(AnsiString, Name)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(Trace, ChannelToggle, NoSync|Important)
	UE_TRACE_EVENT_FIELD(uint32, Id)
	UE_TRACE_EVENT_FIELD(bool, IsEnabled)
UE_TRACE_EVENT_END()

///////////////////////////////////////////////////////////////////////////////
static FChannel* volatile	GHeadChannel;			// = nullptr;
static FChannel* volatile	GNewChannelList;		// = nullptr;
static bool 				GChannelsInitialized;

////////////////////////////////////////////////////////////////////////////////
static uint32 GetChannelHash(const ANSICHAR* Input, int32 Length)
{
	// Make channel names tolerant to ending 's' (or 'S').
	// Example: "Log", "log", "logs", "LOGS" and "LogsChannel" will all match as being the same channel.
	if (Length > 0 && (Input[Length - 1] | 0x20) == 's')
	{
		--Length;
	}

	uint32 Result = 0x811c9dc5;
	for (; Length; ++Input, --Length)
	{
		Result ^= *Input | 0x20; // a cheap ASCII-only case insensitivity.
		Result *= 0x01000193;
	}
	return Result;
}

///////////////////////////////////////////////////////////////////////////////
static uint32 GetChannelNameLength(const ANSICHAR* ChannelName)
{
	// Strip "Channel" suffix if it exists
	size_t Len = uint32(strlen(ChannelName));
	if (Len > 7)
	{
		if (strcmp(ChannelName + Len - 7, "Channel") == 0)
		{
			Len -= 7;
		}
	}

	return uint32(Len);
}



///////////////////////////////////////////////////////////////////////////////
FChannel::Iter::~Iter()
{
	if (Inner[2] == nullptr)
	{
		return;
	}

	using namespace Private;
	for (auto* Node = (FChannel*)Inner[2];; PlatformYield())
	{
		Node->Next = AtomicLoadRelaxed(&GHeadChannel);
		if (AtomicCompareExchangeRelaxed(&GHeadChannel, (FChannel*)Inner[1], Node->Next))
		{
			break;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
const FChannel* FChannel::Iter::GetNext()
{
	auto* Ret = (const FChannel*)Inner[0];
	if (Ret != nullptr)
	{
		Inner[0] = Ret->Next;
		if (Inner[0] != nullptr)
		{
			Inner[2] = Inner[0];
		}
	}
	return Ret;
}



///////////////////////////////////////////////////////////////////////////////
FChannel::Iter FChannel::ReadNew()
{
	using namespace Private;

	FChannel* List = AtomicLoadRelaxed(&GNewChannelList);
	if (List == nullptr)
	{
		return {};
	}

	while (!AtomicCompareExchangeAcquire(&GNewChannelList, (FChannel*)nullptr, List))
	{
		PlatformYield();
		List = AtomicLoadRelaxed(&GNewChannelList);
	}

	return { { List, List, List } };
}

///////////////////////////////////////////////////////////////////////////////
void FChannel::Setup(const ANSICHAR* InChannelName, const InitArgs& InArgs)
{
	using namespace Private;

	Name.Ptr = InChannelName;
	Name.Len = GetChannelNameLength(Name.Ptr);
	Name.Hash = GetChannelHash(Name.Ptr, Name.Len);
	Args = InArgs;

	// Append channel to the linked list of new channels.
	for (;; PlatformYield())
	{
		FChannel* HeadChannel = AtomicLoadRelaxed(&GNewChannelList);
		Next = HeadChannel;
		if (AtomicCompareExchangeRelease(&GNewChannelList, this, Next))
		{
			break;
		}
	}

	// If channel is initialized after the all channels are disabled (post static init)
	// this channel needs to be disabled.
	if (GChannelsInitialized)
	{
		Enabled = -1;
	}
}

///////////////////////////////////////////////////////////////////////////////
void FChannel::Announce() const
{
	UE_TRACE_LOG(Trace, ChannelAnnounce, TraceLogChannel, Name.Len * sizeof(ANSICHAR))
		<< ChannelAnnounce.Id(Name.Hash)
		<< ChannelAnnounce.IsEnabled(IsEnabled())
		<< ChannelAnnounce.ReadOnly(Args.bReadOnly)
		<< ChannelAnnounce.Name(Name.Ptr, Name.Len);
}

///////////////////////////////////////////////////////////////////////////////
void FChannel::Initialize()
{
	// All channels are initialized as enabled (zero), and act like so during
	// from process start until this method is called (i.e. when Trace is initalized).
	ToggleAll(false);
	GChannelsInitialized = true;
}

///////////////////////////////////////////////////////////////////////////////
void FChannel::ToggleAll(bool bEnabled)
{
	using namespace Private;

	FChannel* ChannelLists[] =
	{
		AtomicLoadAcquire(&GNewChannelList),
		AtomicLoadAcquire(&GHeadChannel),
	};
	for (FChannel* Channel : ChannelLists)
	{
		for (; Channel != nullptr; Channel = (FChannel*)(Channel->Next))
		{
			Channel->Toggle(bEnabled);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
void FChannel::PanicDisableAll()
{
	using namespace Private;

	FChannel* ChannelLists[] =
	{
		AtomicLoadAcquire(&GNewChannelList),
		AtomicLoadAcquire(&GHeadChannel),
	};
	for (FChannel* Channel : ChannelLists)
	{
		for (; Channel != nullptr; Channel = (FChannel*)(Channel->Next))
		{
			AtomicStoreRelaxed(&Channel->Enabled, -1);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
FChannel* FChannel::FindChannel(const ANSICHAR* ChannelName)
{
	using namespace Private;

	const uint32 ChannelNameLen = GetChannelNameLength(ChannelName);
	const uint32 ChannelNameHash = GetChannelHash(ChannelName, ChannelNameLen);

	FChannel* ChannelLists[] =
	{
		AtomicLoadAcquire(&GNewChannelList),
		AtomicLoadAcquire(&GHeadChannel),
	};
	for (FChannel* Channel : ChannelLists)
	{
		for (; Channel != nullptr; Channel = Channel->Next)
		{
			if (Channel->Name.Hash == ChannelNameHash)
			{
				return Channel;
			}
		}
	}

	return nullptr;
}

///////////////////////////////////////////////////////////////////////////////
void FChannel::EnumerateChannels(ChannelIterFunc Func, void* User) 
{
	using namespace Private;
	FChannel* ChannelLists[] =
	{
		AtomicLoadAcquire(&GNewChannelList),
		AtomicLoadAcquire(&GHeadChannel),
	};
	for (FChannel* Channel : ChannelLists)
	{
		for (; Channel != nullptr; Channel = Channel->Next)
		{
			Func(Channel->Name.Ptr, Channel->IsEnabled(), User);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
void FChannel::EnumerateChannels(ChannelIterCallback Func, void* User)
{
	using namespace Private;
	FChannel* ChannelLists[] =
	{
		AtomicLoadAcquire(&GNewChannelList),
		AtomicLoadAcquire(&GHeadChannel),
	};

	FChannelInfo Info;
	for (FChannel* Channel : ChannelLists)
	{
		for (; Channel != nullptr; Channel = Channel->Next)
		{
			Info.Name = Channel->Name.Ptr;
			Info.Desc = Channel->Args.Desc;
			Info.bIsEnabled = Channel->IsEnabled();
			Info.bIsReadOnly = Channel->Args.bReadOnly;
			bool Result = Func(Info, User);
			if (!Result)
			{
				return;
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
bool FChannel::Toggle(bool bEnabled)
{
	using namespace Private;
	AtomicStoreRelaxed(&Enabled, bEnabled ? 1 : -1);

	UE_TRACE_LOG(Trace, ChannelToggle, TraceLogChannel)
		<< ChannelToggle.Id(Name.Hash)
		<< ChannelToggle.IsEnabled(IsEnabled());

	return IsEnabled();
}

///////////////////////////////////////////////////////////////////////////////
bool FChannel::Toggle(const ANSICHAR* ChannelName, bool bEnabled)
{
	if (FChannel* Channel = FChannel::FindChannel(ChannelName))
	{
		return Channel->Toggle(bEnabled);
	}
	return false;
}

} // namespace Trace
} // namespace UE

#endif // UE_TRACE_ENABLED
