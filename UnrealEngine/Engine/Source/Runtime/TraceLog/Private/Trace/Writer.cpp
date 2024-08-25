// Copyright Epic Games, Inc. All Rights Reserved.

#include "Message.h"
#include "Trace/Config.h"

#if UE_TRACE_ENABLED

#include "Platform.h"
#include "Trace/Detail/Atomic.h"
#include "Trace/Detail/Channel.h"
#include "Trace/Detail/Protocol.h"
#include "Trace/Detail/Transport.h"
#include "Trace/Trace.inl"
#include "WriteBufferRedirect.h"

#include <limits.h>
#include <stdlib.h>

#if PLATFORM_WINDOWS
#	define TRACE_PRIVATE_STOMP 0 // 1=overflow, 2=underflow
#	if TRACE_PRIVATE_STOMP
#	include "Windows/AllowWindowsPlatformTypes.h"
#	include "Windows/HideWindowsPlatformTypes.h"
#	endif
#else
#	define TRACE_PRIVATE_STOMP 0
#endif

#ifndef TRACE_PRIVATE_BUFFER_SEND
#	define TRACE_PRIVATE_BUFFER_SEND 0
#endif


namespace UE {
namespace Trace {
namespace Private {

////////////////////////////////////////////////////////////////////////////////
int32			Encode(const void*, int32, void*, int32);
void			Writer_SendData(uint32, uint8* __restrict, uint32);
void			Writer_InitializeTail(int32);
void			Writer_ShutdownTail();
void			Writer_TailOnConnect();
void			Writer_InitializeSharedBuffers();
void			Writer_ShutdownSharedBuffers();
void			Writer_UpdateSharedBuffers();
void			Writer_InitializeCache();
void			Writer_ShutdownCache();
void			Writer_CacheOnConnect();
void			Writer_CallbackOnConnect();
void			Writer_InitializePool();
void			Writer_ShutdownPool();
void			Writer_DrainBuffers();
void			Writer_DrainLocalBuffers();
void			Writer_EndThreadBuffer();
uint32			Writer_GetControlPort();
void			Writer_UpdateControl();
void			Writer_InitializeControl();
void			Writer_ShutdownControl();
bool			Writer_IsTailing();
static bool		Writer_SessionPrologue();
void			Writer_FreeBlockListToPool(FWriteBuffer*, FWriteBuffer*);


////////////////////////////////////////////////////////////////////////////////
struct FTraceGuid
{
	uint32 Bits[4];	
};

////////////////////////////////////////////////////////////////////////////////
UE_TRACE_EVENT_BEGIN($Trace, NewTrace, Important|NoSync)
	UE_TRACE_EVENT_FIELD(uint64, StartCycle)
	UE_TRACE_EVENT_FIELD(uint64, CycleFrequency)
	UE_TRACE_EVENT_FIELD(uint16, Endian)
	UE_TRACE_EVENT_FIELD(uint8, PointerSize)
UE_TRACE_EVENT_END()



////////////////////////////////////////////////////////////////////////////////
static volatile bool			GInitialized;		// = false;
FStatistics						GTraceStatistics;	// = {};
TRACELOG_API uint64				GStartCycle;		// = 0;
TRACELOG_API uint32 volatile	GLogSerial;			// = 0;
// Counter of calls to Writer_WorkerUpdate to enable regular flushing of output buffers
static uint32					GUpdateCounter;		// = 0;



////////////////////////////////////////////////////////////////////////////////
// When a thread terminates we want to recover its trace buffer. To do this we
// use a thread_local object whose destructor gets called as the thread ends. On
// some C++ standard library implementations this is implemented using a thread-
// specific atexit() call and can involve taking a lock. As tracing can start
// very early or often be used during shared-object loads, there is a risk of
// a deadlock initialising the context object. The define below can be used to
// implement alternatives via a 'ThreadOnThreadExit()' symbol.
#if !defined(UE_TRACE_USE_TLS_CONTEXT_OBJECT)
#	define UE_TRACE_USE_TLS_CONTEXT_OBJECT 1
#endif

#if UE_TRACE_USE_TLS_CONTEXT_OBJECT

////////////////////////////////////////////////////////////////////////////////
struct FWriteTlsContext
{
				~FWriteTlsContext();
	uint32		GetThreadId();

private:
	uint32		ThreadId = 0;
};

////////////////////////////////////////////////////////////////////////////////
FWriteTlsContext::~FWriteTlsContext()
{
	if (AtomicLoadRelaxed(&GInitialized))
	{
		Writer_EndThreadBuffer();
	}
}

////////////////////////////////////////////////////////////////////////////////
uint32 FWriteTlsContext::GetThreadId()
{
	if (ThreadId)
	{
		return ThreadId;
	}

	static uint32 volatile Counter;
	ThreadId = AtomicAddRelaxed(&Counter, 1u) + ETransportTid::Bias;
	return ThreadId;
}

////////////////////////////////////////////////////////////////////////////////
thread_local FWriteTlsContext	GTlsContext;

////////////////////////////////////////////////////////////////////////////////
uint32 Writer_GetThreadId()
{
	return GTlsContext.GetThreadId();
}

#else // UE_TRACE_USE_TLS_CONTEXT_OBJECT

////////////////////////////////////////////////////////////////////////////////
void ThreadOnThreadExit(void (*)());

////////////////////////////////////////////////////////////////////////////////
uint32 Writer_GetThreadId()
{
	static thread_local uint32 ThreadId;
	if (ThreadId)
	{
		return ThreadId;
	}

	ThreadOnThreadExit([] () { Writer_EndThreadBuffer(); });

	static uint32 volatile Counter;
	ThreadId = AtomicAddRelaxed(&Counter, 1u) + ETransportTid::Bias;
	return ThreadId;
}

#endif // UE_TRACE_USE_TLS_CONTEXT_OBJECT


////////////////////////////////////////////////////////////////////////////////
void Writer_CreateGuid(FTraceGuid* OutGuid)
{
	// This is not thread safe. Should only be accessed from the writer thread.
	// This initialized the prng with the current timestamp. In theory two machines could initialize on the exact same time
	// producing the same sequence of guids.
	static uint64 State = TimeGetTimestamp();
	// L'Ecuyer, Pierre (1999). "Tables of Linear Congruential Generators of Different Sizes and Good Lattice Structure"
	// corrected with errata
	// Assuming m = 2e64
	constexpr uint64 C = 0x369DEA0F31A53F85;	
	constexpr uint64 I = 1ull;

	const uint64 TopBits = State * C + I;
	const uint64 BottomBits = TopBits * C + I;
	State = BottomBits;

	*(uint64*)&OutGuid->Bits[0] = TopBits;
	*(uint64*)&OutGuid->Bits[2] = BottomBits;

	constexpr uint8 Version = 0x40; //Version 4, 4 bits
	constexpr uint8 VersionMask = 0xf0;
	constexpr uint8 Variant = 0x80; //Variant 1, 2 bits
	constexpr uint8 VariantMask = 0xc0;

	uint8* Octets = (uint8*)OutGuid;
	Octets[6] = Version | (~VersionMask & Octets[6]); // Octet 9
	Octets[8] = Variant | (~VariantMask & Octets[8]); // Octet 7
}

////////////////////////////////////////////////////////////////////////////////
void*			(*AllocHook)(SIZE_T, uint32);			// = nullptr
void			(*FreeHook)(void*, SIZE_T);				// = nullptr

////////////////////////////////////////////////////////////////////////////////
void Writer_MemorySetHooks(decltype(AllocHook) Alloc, decltype(FreeHook) Free)
{
	AllocHook = Alloc;
	FreeHook = Free;
}

////////////////////////////////////////////////////////////////////////////////
void* Writer_MemoryAllocate(SIZE_T Size, uint32 Alignment)
{
	void* Ret = nullptr;

#if TRACE_PRIVATE_STOMP
	static uint8* Base;
	if (Base == nullptr)
	{
		Base = (uint8*)VirtualAlloc(0, 1ull << 40, MEM_RESERVE, PAGE_READWRITE);
	}

	static SIZE_T PageSize = 4096;
	Base += PageSize;
	uint8* NextBase = Base + ((PageSize - 1 + Size) & ~(PageSize - 1));
	VirtualAlloc(Base, SIZE_T(NextBase - Base), MEM_COMMIT, PAGE_READWRITE);
#if TRACE_PRIVATE_STOMP == 1
	Ret = NextBase - Size;
#elif TRACE_PRIVATE_STOMP == 2
	Ret = Base;
#endif
	Base = NextBase;
#else // TRACE_PRIVATE_STOMP

	if (AllocHook != nullptr)
	{
		Ret = AllocHook(Size, Alignment);
	}
	else
	{
#if defined(_MSC_VER)
		Ret = _aligned_malloc(Size, Alignment);
#elif (defined(__ANDROID_API__) && __ANDROID_API__ < 28) || defined(__APPLE__)
		posix_memalign(&Ret, Alignment, Size);
#else
		Ret = aligned_alloc(Alignment, Size);
#endif
	}
#endif // TRACE_PRIVATE_STOMP

#if TRACE_PRIVATE_STATISTICS
	AtomicAddRelaxed(&GTraceStatistics.MemoryUsed, uint64(Size));
#endif

	return Ret;
}

////////////////////////////////////////////////////////////////////////////////
void Writer_MemoryFree(void* Address, uint32 Size)
{
#if TRACE_PRIVATE_STOMP
	if (Address == nullptr)
	{
		return;
	}

	*(uint8*)Address = 0xfe;

	MEMORY_BASIC_INFORMATION MemInfo;
	VirtualQuery(Address, &MemInfo, sizeof(MemInfo));

	DWORD Unused;
	VirtualProtect(MemInfo.BaseAddress, MemInfo.RegionSize, PAGE_READONLY, &Unused);
#else // TRACE_PRIVATE_STOMP
	if (FreeHook != nullptr)
	{
		FreeHook(Address, Size);
	}
	else
	{
#if defined(_MSC_VER)
		_aligned_free(Address);
#else
		free(Address);
#endif
	}
#endif // TRACE_PRIVATE_STOMP

#if TRACE_PRIVATE_STATISTICS
	AtomicAddRelaxed(&GTraceStatistics.MemoryUsed, uint64(-int64(Size)));
#endif
}



////////////////////////////////////////////////////////////////////////////////
static UPTRINT					GDataHandle;		// = 0
static volatile UPTRINT			GPendingDataHandle;	// = 0

////////////////////////////////////////////////////////////////////////////////
#if TRACE_PRIVATE_BUFFER_SEND
static const SIZE_T GSendBufferSize = 1 << 20; // 1Mb
uint8* GSendBuffer; // = nullptr;
uint8* GSendBufferCursor; // = nullptr;
static bool Writer_FlushSendBuffer()
{
	if( GSendBufferCursor > GSendBuffer )
	{
		if (!IoWrite(GDataHandle, GSendBuffer, GSendBufferCursor - GSendBuffer))
		{
			UE_TRACE_ERRORMESSAGE(WriteError, GetLastErrorCode());
			IoClose(GDataHandle);
			GDataHandle = 0;
			return false;
		}
		GSendBufferCursor = GSendBuffer;
	}
	return true;
}
#else
static bool Writer_FlushSendBuffer() { return true; }
#endif

////////////////////////////////////////////////////////////////////////////////
static void Writer_SendDataImpl(const void* Data, uint32 Size)
{
#if TRACE_PRIVATE_STATISTICS
	GTraceStatistics.BytesSent += Size;
#endif

#if TRACE_PRIVATE_BUFFER_SEND
	// If there's not enough space for this data, flush
	if (GSendBufferCursor + Size > GSendBuffer + GSendBufferSize)
	{
		if (!Writer_FlushSendBuffer())
		{
			return;
		}
	}

	// Should rarely happen but if we're asked to send large data send it directly
	if (Size > GSendBufferSize)
	{
		if (!IoWrite(GDataHandle, Data, Size))
		{
			IoClose(GDataHandle);
			GDataHandle = 0;
		}
	}
	// Otherwise append to the buffer
	else
	{
		memcpy(GSendBufferCursor, Data, Size);
		GSendBufferCursor += Size;
	}
#else
	if (!IoWrite(GDataHandle, Data, Size))
	{
		UE_TRACE_ERRORMESSAGE(WriteError, GetLastErrorCode());
		IoClose(GDataHandle);
		GDataHandle = 0;
	}
#endif
}

////////////////////////////////////////////////////////////////////////////////
void Writer_SendDataRaw(const void* Data, uint32 Size)
{
	if (!GDataHandle)
	{
		return;
	}

	Writer_SendDataImpl(Data, Size);
}

////////////////////////////////////////////////////////////////////////////////
void Writer_SendData(uint32 ThreadId, uint8* __restrict Data, uint32 Size)
{
	static_assert(ETransport::Active == ETransport::TidPacketSync, "Active should be set to what the compiled code uses. It is used to track places that assume transport packet format");

	if (!GDataHandle)
	{
		return;
	}

	// Smaller buffers usually aren't redundant enough to benefit from being
	// compressed. They often end up being larger.
	if (Size <= 384)
	{
		Data -= sizeof(FTidPacket);
		Size += sizeof(FTidPacket);
		auto* Packet = (FTidPacket*)Data;
		Packet->ThreadId = uint16(ThreadId & FTidPacketBase::ThreadIdMask);
		Packet->PacketSize = uint16(Size);

		Writer_SendDataImpl(Data, Size);
		return;
	}

	// Buffer size is expressed as "A + B" where A is a maximum expected
	// input size (i.e. at least GPoolBlockSize) and B is LZ4 overhead as
	// per LZ4_COMPRESSBOUND.
	TTidPacketEncoded<8192 + 64> Packet;

	Packet.ThreadId = FTidPacketBase::EncodedMarker;
	Packet.ThreadId |= uint16(ThreadId & FTidPacketBase::ThreadIdMask);
	Packet.DecodedSize = uint16(Size);
	Packet.PacketSize = uint16(Encode(Data, Packet.DecodedSize, Packet.Data, sizeof(Packet.Data)));
	Packet.PacketSize += sizeof(FTidPacketEncoded);

	Writer_SendDataImpl(&Packet, Packet.PacketSize);
}

////////////////////////////////////////////////////////////////////////////////
static void Writer_DescribeEvents(FEventNode::FIter Iter)
{
	TWriteBufferRedirect<4096> TraceData;

	while (const FEventNode* Event = Iter.GetNext())
	{
		Event->Describe();

		// Flush just in case an NewEvent event will be larger than 512 bytes.
		if (TraceData.GetSize() >= (TraceData.GetCapacity() - 512))
		{
			Writer_SendData(ETransportTid::Events, TraceData.GetData(), TraceData.GetSize());
			TraceData.Reset();
		}
	}

	if (TraceData.GetSize())
	{
		Writer_SendData(ETransportTid::Events, TraceData.GetData(), TraceData.GetSize());
	}
}

////////////////////////////////////////////////////////////////////////////////
static void Writer_AnnounceChannels()
{
	FChannel::Iter Iter = FChannel::ReadNew();
	while (const FChannel* Channel = Iter.GetNext())
	{
		Channel->Announce();
	}
}

////////////////////////////////////////////////////////////////////////////////
static void Writer_DescribeAnnounce()
{
	if (!GDataHandle)
	{
		return;
	}

	Writer_AnnounceChannels();
	Writer_DescribeEvents(FEventNode::ReadNew());
}



////////////////////////////////////////////////////////////////////////////////
static int8			GSyncPacketCountdown;	// = 0
static const int8	GNumSyncPackets			= 3;
static OnConnectFunc*	GOnConnection = nullptr;
static FTraceGuid GSessionGuid; // = {0, 0, 0, 0};
static FTraceGuid GTraceGuid; // = {0, 0, 0, 0};

////////////////////////////////////////////////////////////////////////////////
static void Writer_SendSync()
{
	if (GSyncPacketCountdown <= 0 || !GDataHandle)
	{
		return;
	}

	// It is possible that some events get collected and discarded by a previous
	// update that are newer than events sent it the following update where IO
	// is established. This will result in holes in serial numbering. A few sync
	// points are sent to aid analysis in determining what are holes and what is
	// just a requirement for more data. Holes will only occur at the start.

	// Note that Sync is alias as Important/Internal as changing Bias would
	// break backwards compatibility.

	FTidPacketBase SyncPacket = { sizeof(SyncPacket), ETransportTid::Sync };
	Writer_SendDataImpl(&SyncPacket, sizeof(SyncPacket));

	--GSyncPacketCountdown;
}

////////////////////////////////////////////////////////////////////////////////
static void Writer_Close()
{
	if (GDataHandle)
	{
		Writer_FlushSendBuffer();
		IoClose(GDataHandle);
	}

	GDataHandle = 0;
}

////////////////////////////////////////////////////////////////////////////////
static bool Writer_UpdateConnection()
{
	UPTRINT PendingDataHandle = AtomicLoadRelaxed(&GPendingDataHandle);

	if (!PendingDataHandle)
	{
		return false;
	}

	// Is this a close request? So that we capture some of the events around
	// the closure we will add some inertia before enacting the close.
	static int32 CloseInertia = 0;
	if (PendingDataHandle == ~UPTRINT(0))
	{
		if (CloseInertia <= 0)
			CloseInertia = 2;

		--CloseInertia;
		if (CloseInertia <= 0)
		{
			Writer_Close();
			AtomicStoreRelaxed(&GPendingDataHandle, UPTRINT(0));
		}

		return true;
	}

	AtomicStoreRelaxed(&GPendingDataHandle, UPTRINT(0));

	// Extract send flags
	uint32 SendFlags = uint32(PendingDataHandle >> 48ull);
	PendingDataHandle &= 0x0000'ffff'ffff'ffffull;

	// Reject the pending connection if we've already got a connection
	if (GDataHandle)
	{
		IoClose(PendingDataHandle);
		return false;
	}

	// Generate Guid for new connection
	Writer_CreateGuid(&GTraceGuid);

	GDataHandle = PendingDataHandle;
	if (!Writer_SessionPrologue())
	{
		return false;
	}

	// Reset statistics.
	GTraceStatistics.BytesSent = 0;
	GTraceStatistics.BytesTraced = 0;

	// The first events we will send are ones that describe the trace's events
	FEventNode::OnConnect();
	Writer_DescribeEvents(FEventNode::ReadNew());

	// Send cached events (i.e. importants) 
	Writer_CacheOnConnect();

	// Issue on connection callback. This allows writing events that are
	// not cached but important for the cache
	Writer_CallbackOnConnect();

	// Finally write the events in the tail buffer
	if ((SendFlags & FSendFlags::ExcludeTail) == 0)
	{
		Writer_TailOnConnect();
	}

	// See Writer_SendSync() for details.
	GSyncPacketCountdown = GNumSyncPackets;

	return true;
}

////////////////////////////////////////////////////////////////////////////////
static bool Writer_SessionPrologue()
{
	if (!GDataHandle)
	{
		return false;
	}

#if TRACE_PRIVATE_BUFFER_SEND
	if (!GSendBuffer)
	{
		GSendBuffer = static_cast<uint8*>(Writer_MemoryAllocate(GSendBufferSize, 16));
	}
	GSendBufferCursor = GSendBuffer;
#endif

	// Handshake.
	struct FHandshake
	{
		uint32 Magic			= '2' | ('C' << 8) | ('R' << 16) | ('T' << 24);
		uint16 MetadataSize		= uint16(MetadataSizeSum);
		uint16 MetadataField0	= uint16(sizeof(ControlPort) | (ControlPortFieldId << 8));
		uint16 ControlPort		= uint16(Writer_GetControlPort());
		uint16 MetadataField1	= uint16(sizeof(FTraceGuid) | (SessionGuidFieldId << 8));
		uint8 SessionGuid[16];	// Avoid padding
		uint16 MetadataField2	= uint16(sizeof(FTraceGuid) | (TraceGuidFieldId << 8));
		uint8 TraceGuid[16];	// Avoid padding
		enum
		{
			MetadataSizeSum		= 2 + 2 + 2 + 16 + 2 + 16,
			Size				= MetadataSizeSum + 4 + 2,
			ControlPortFieldId	= 0,
			SessionGuidFieldId	= 1,
			TraceGuidFieldId	= 2,
		};
	};
	FHandshake Handshake;
	memcpy(&Handshake.SessionGuid, &GSessionGuid, sizeof(FTraceGuid));
	memcpy(&Handshake.TraceGuid, &GTraceGuid, sizeof(FTraceGuid));
	bool bOk = IoWrite(GDataHandle, &Handshake, FHandshake::Size);

	// Stream header
	const struct {
		uint8 TransportVersion	= ETransport::TidPacketSync;
		uint8 ProtocolVersion	= EProtocol::Id;
	} TransportHeader;
	bOk &= IoWrite(GDataHandle, &TransportHeader, sizeof(TransportHeader));

	if (!bOk)
	{
		UE_TRACE_ERRORMESSAGE(WriteError, GetLastErrorCode());
		IoClose(GDataHandle);
		GDataHandle = 0;
		return false;
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////
void Writer_CallbackOnConnect()
{
	if (!GOnConnection)
	{
		return;
	}

	// Prior to letting callbacks trace events we need to flush any pending
	// trace data to that tail. We do not want that data to be sent over the
	// wire as that would cause data to be sent out-of-order.
	UPTRINT DataHandle = GDataHandle;
	GDataHandle = 0;
	Writer_DrainLocalBuffers();
	GDataHandle = DataHandle;

	// Issue callback. We assume any events emitted here are not marked as
	// important and emitted on this thread.
	GOnConnection();
}


////////////////////////////////////////////////////////////////////////////////
static UPTRINT			GWorkerThread;		// = 0;
static volatile bool	GWorkerThreadQuit;	// = false;
static uint32			GSleepTimeInMS = 17;
static volatile uint32	GUpdateInProgress = 1;	// Don't allow updates until initialized

////////////////////////////////////////////////////////////////////////////////
static void Writer_WorkerUpdateInternal()
{
	Writer_UpdateControl();
	Writer_UpdateConnection();
	Writer_DescribeAnnounce();
	Writer_UpdateSharedBuffers();
	Writer_DrainBuffers();
	Writer_SendSync();

#if TRACE_PRIVATE_BUFFER_SEND
	const uint32 FlushSendBufferCadenceMask = 8-1; // Flush every 8 calls
	if((++GUpdateCounter & FlushSendBufferCadenceMask) == 0 && GDataHandle != 0)
	{
		Writer_FlushSendBuffer();
	}
#endif
}

////////////////////////////////////////////////////////////////////////////////
static void Writer_WorkerUpdate()
{
	if (!AtomicCompareExchangeAcquire(&GUpdateInProgress, 1u, 0u))
	{
		return;
	}
	
	Writer_WorkerUpdateInternal();

	AtomicExchangeRelease(&GUpdateInProgress, 0u);
}

////////////////////////////////////////////////////////////////////////////////
static void Writer_WorkerThread()
{
	ThreadRegister(TEXT("Trace"), 0, INT_MAX);

	while (!AtomicLoadRelaxed(&GWorkerThreadQuit))
	{
		Writer_WorkerUpdate();

		ThreadSleep(GSleepTimeInMS);
	}
}

////////////////////////////////////////////////////////////////////////////////
void Writer_WorkerCreate()
{
	if (GWorkerThread)
	{
		return;
	}

	GWorkerThread = ThreadCreate("TraceWorker", Writer_WorkerThread);
}

////////////////////////////////////////////////////////////////////////////////
static void Writer_WorkerJoin()
{
	if (!GWorkerThread)
	{
		return;
	}

	AtomicStoreRelaxed(&GWorkerThreadQuit, true);

	ThreadJoin(GWorkerThread);
	ThreadDestroy(GWorkerThread);

	Writer_WorkerUpdate();

	GWorkerThread = 0;
}



////////////////////////////////////////////////////////////////////////////////
static void Writer_InternalInitializeImpl()
{
	if (AtomicLoadRelaxed(&GInitialized))
	{
		return;
	}

	GStartCycle = TimeGetTimestamp();

	Writer_InitializeSharedBuffers();
	Writer_InitializePool();
	Writer_InitializeControl();

	AtomicStoreRelaxed(&GInitialized, true);

	UE_TRACE_LOG($Trace, NewTrace, TraceLogChannel)
		<< NewTrace.StartCycle(GStartCycle)
		<< NewTrace.CycleFrequency(TimeGetFrequency())
		<< NewTrace.Endian(uint16(0x524d))
		<< NewTrace.PointerSize(uint8(sizeof(void*)));
}

////////////////////////////////////////////////////////////////////////////////
static void Writer_InternalShutdown()
{
	if (!AtomicLoadRelaxed(&GInitialized))
	{
		return;
	}

	Writer_WorkerJoin();

	if (GDataHandle)
	{
		Writer_FlushSendBuffer();
		IoClose(GDataHandle);
		GDataHandle = 0;
	}

	Writer_ShutdownControl();
	Writer_ShutdownPool();
	Writer_ShutdownSharedBuffers();
	Writer_ShutdownCache();
	Writer_ShutdownTail();

#if TRACE_PRIVATE_BUFFER_SEND
	if (GSendBuffer)
	{
		Writer_MemoryFree(GSendBuffer, GSendBufferSize);
		GSendBuffer = nullptr;
		GSendBufferCursor = nullptr;
	}
#endif

	AtomicStoreRelaxed(&GInitialized, false);
}

////////////////////////////////////////////////////////////////////////////////
void Writer_InternalInitialize()
{
	using namespace Private;

	if (AtomicLoadRelaxed(&GInitialized))
	{
		return;
	}

	static struct FInitializer
	{
		FInitializer()
		{
			Writer_InternalInitializeImpl();
		}
		~FInitializer()
		{
			/* We'll not shut anything down here so we can hopefully capture
			 * any subsequent events. However, we will shutdown the worker
			 * thread and leave it for something else to call update() (mem
			 * tracing at time of writing). Windows will have already done
			 * this implicitly in ExitProcess() anyway. */
			Writer_WorkerJoin();
		}
	} Initializer;
}

////////////////////////////////////////////////////////////////////////////////
void Writer_Initialize(const FInitializeDesc& Desc)
{
	Writer_InitializeTail(Desc.TailSizeBytes);

	if (Desc.bUseImportantCache)
	{
		Writer_InitializeCache();
	}

	if (Desc.ThreadSleepTimeInMS != 0)
	{
		GSleepTimeInMS = Desc.ThreadSleepTimeInMS;
	}

	if (Desc.bUseWorkerThread)
	{
		Writer_WorkerCreate();
	}

	// Store the session guid if specified, otherwise generate one
	if (!Desc.SessionGuid[0] & !Desc.SessionGuid[1] & !Desc.SessionGuid[2] & !Desc.SessionGuid[3])
	{
		Writer_CreateGuid(&GSessionGuid);
	}
	else
	{
		memcpy(&GSessionGuid, &Desc.SessionGuid, sizeof(FTraceGuid));
	}

	// Store callback on connection
	GOnConnection = Desc.OnConnectionFunc;

	// Allow the worker thread to start updating 
	AtomicStoreRelease(&GUpdateInProgress, uint32(0));
}

////////////////////////////////////////////////////////////////////////////////
void Writer_Shutdown()
{
	Writer_InternalShutdown();
}

////////////////////////////////////////////////////////////////////////////////
void Writer_Update()
{
	if (!GWorkerThread)
	{
		Writer_WorkerUpdate();
	}
}

////////////////////////////////////////////////////////////////////////////////
static UPTRINT Writer_PackSendFlags(UPTRINT DataHandle, uint32 Flags)
{
	// Passing ownership of IO to the worker thread via a single 64 bit value is
	// convenient and saves a lot of machinery for something that mostly never
	// happens, or perhaps just once or twice. Here we make the assumption that
	// our supported platforms' handles are low integer file descriptor IDs or
	// addresses and thus we have some most-significant bits to use for flags.

	// Guard against the assumption being wrong.
	if (DataHandle & 0xffff'0000'0000'0000ull)
	{
		IoClose(DataHandle);
		return 0;
	}

	return DataHandle | (UPTRINT(Flags) << 48ull);
}

////////////////////////////////////////////////////////////////////////////////
bool Writer_SendTo(const ANSICHAR* Host, uint32 Flags, uint32 Port)
{
	if (AtomicLoadRelaxed(&GPendingDataHandle))
	{
		return false;
	}

	Writer_InternalInitialize();

	Port = Port ? Port : 1981;
	UPTRINT DataHandle = TcpSocketConnect(Host, uint16(Port));
	if (!DataHandle)
	{
		UE_TRACE_ERRORMESSAGE_F(ConnectError, GetLastErrorCode(), "Connecting to host (%s:%u)", Host, Port);
		return false;
	}

	DataHandle = Writer_PackSendFlags(DataHandle, Flags);
	if (!DataHandle)
	{
		UE_TRACE_MESSAGE(ConnectError, "Handle was unexpectedly using MSB flags.");
		return false;
	}

	AtomicStoreRelaxed(&GPendingDataHandle, DataHandle);
	return true;
}

////////////////////////////////////////////////////////////////////////////////
bool Writer_WriteTo(const ANSICHAR* Path, uint32 Flags)
{
	if (AtomicLoadRelaxed(&GPendingDataHandle))
	{
		return false;
	}

	Writer_InternalInitialize();

	UPTRINT DataHandle = FileOpen(Path);
	if (!DataHandle)
	{
		UE_TRACE_ERRORMESSAGE_F(FileOpenError, GetLastErrorCode(), "Opening file (%s)", Path);
		return false;
	}

	DataHandle = Writer_PackSendFlags(DataHandle, Flags);
	if (!DataHandle)
	{
		return false;
	}

	AtomicStoreRelaxed(&GPendingDataHandle, DataHandle);
	return true;
}

////////////////////////////////////////////////////////////////////////////////
struct WorkerUpdateLock
{
	WorkerUpdateLock()
	{
		CyclesPerSecond = TimeGetFrequency();
		StartSeconds = GetTime();

		while (!AtomicCompareExchangeAcquire(&GUpdateInProgress, 1u, 0u))
		{
			ThreadSleep(0);

			if (TimedOut())
			{
				break;
			}
		}
	}

	~WorkerUpdateLock()
	{
		AtomicExchangeRelease(&GUpdateInProgress, 0u);
	}

	double GetTime()
	{
		return static_cast<double>(TimeGetTimestamp()) / static_cast<double>(CyclesPerSecond);
	}

	bool TimedOut()
	{
		const double WaitTime = GetTime() - StartSeconds;
		return WaitTime > MaxWaitSeconds;
	}

	uint64 CyclesPerSecond;
	double StartSeconds;
	inline const static double MaxWaitSeconds = 1.0;
};

////////////////////////////////////////////////////////////////////////////////
template <typename Type>
struct TStashGlobal
{
	TStashGlobal(Type& Global)
		: Variable(Global)
		, Stashed(Global)
	{
		Variable = {};
	}

	TStashGlobal(Type& Global, const Type& Value)
		: Variable(Global)
		, Stashed(Global)
	{
		Variable = Value;
	}

	~TStashGlobal()
	{
		Variable = Stashed;
	}

private:
	Type& Variable;
	Type Stashed;
};

////////////////////////////////////////////////////////////////////////////////
struct FSnapshotTarget
{
	enum EType { FileTarget, HostTarget };
	EType Type;
	union
	{
		 struct
		 {
			 const ANSICHAR* Path;
		 } File;
		 struct
		 {
			 const ANSICHAR* Host;
			 uint32 Port;
		 } Host;
	};
};

////////////////////////////////////////////////////////////////////////////////
bool Writer_WriteSnapshot(const FSnapshotTarget& Target)
{
	if (!Writer_IsTailing())
	{
		return false;
	}

	WorkerUpdateLock UpdateLock;

	// We have a timeout just in case the worker thread goes off the rails
	//  We are called by diagnostic handlers like crash reporter, do not deadlock
	if (UpdateLock.TimedOut())
	{
		return false;
	}
	
	// Bring everything up to date with the active tracing connection.
	//  Any connection writes after we call the worker update
	//  will need to treat source data structures as read-only.
	//  Those structures can be stateful about what has or has not been
	//  written (cursor state, "new" event, etc) to the tracing connection.
	//  We are pre-empting the connection to write the snapshot.
	//
	// Doing a full pump of the data here is more robust than opening
	//  pathways through each data structure for immutable writes because
	//  the data structures are order-dependent and inter-referential.
	//  Not writing all state can very easily lead to bugs in either the
	//  snapshot or, even worse, the tracing connection. Parsers only
	//  have a limited tolerance to gaps/out-of-order event packets.
	Writer_WorkerUpdateInternal();

	{
		TStashGlobal DataHandle(GDataHandle);
		TStashGlobal PendingDataHandle(GPendingDataHandle);
		TStashGlobal SyncPacketCountdown(GSyncPacketCountdown, GNumSyncPackets);
		TStashGlobal TraceStatistics(GTraceStatistics);

		if (Target.Type == FSnapshotTarget::EType::FileTarget)
		{
			// Open the snapshot file 
			GDataHandle = FileOpen(Target.File.Path);
		}
		else
		{
			// Open the snapshot connection and write 
			const uint32 Port = Target.Host.Port ? Target.Host.Port : 1981;
			GDataHandle = TcpSocketConnect(Target.Host.Host, uint16(Port));
			if (!GDataHandle)
			{
				return false;
			}
			GDataHandle = Writer_PackSendFlags(GDataHandle, 0);
		}

		// Write the file header
		if (!GDataHandle || !Writer_SessionPrologue())
		{
			UE_TRACE_ERRORMESSAGE(FileOpenError, GetLastErrorCode());
			return false;
		}

		// The first events we will send are ones that describe the trace's events
		Writer_DescribeEvents(FEventNode::Read());

		// Send cached events (i.e. importants)
		Writer_CacheOnConnect();

		// Issue on connection callback. This allows writing events that are
		// not cached but important for the cache
		Writer_CallbackOnConnect();

		// Finally write the events in the tail buffer
		Writer_TailOnConnect();

		// Send sync packets to help parsers digest any out-of-order events
		GSyncPacketCountdown = GNumSyncPackets;
		while (GSyncPacketCountdown > 0)
		{
			Writer_SendSync();
		}

		Writer_Close();
	}

	return true;
}
	
////////////////////////////////////////////////////////////////////////////////
bool Writer_WriteSnapshotTo(const ANSICHAR* Path)
{
	FSnapshotTarget Target;
	Target.Type = FSnapshotTarget::EType::FileTarget;
	Target.File.Path = Path;
	return Writer_WriteSnapshot(Target);
}

////////////////////////////////////////////////////////////////////////////////
bool Writer_SendSnapshotTo(const ANSICHAR* Host, uint32 Port)
{
	FSnapshotTarget Target;
	Target.Type = FSnapshotTarget::EType::HostTarget;
	Target.Host.Host = Host;
	Target.Host.Port = Port;
	return Writer_WriteSnapshot(Target);
}
	
////////////////////////////////////////////////////////////////////////////////
bool Writer_IsTracing()
{
	return GDataHandle != 0 || GPendingDataHandle != 0;
}
	
////////////////////////////////////////////////////////////////////////////////
bool Writer_IsTracingTo(uint32 (&OutSessionGuid)[4], uint32 (&OutTraceGuid)[4])
{
	if (Writer_IsTracing())
	{
		memcpy(&OutSessionGuid, &GSessionGuid, sizeof(OutSessionGuid));
		memcpy(&OutTraceGuid, &GTraceGuid, sizeof(OutTraceGuid));
		return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////
bool Writer_Stop()
{
	if (GPendingDataHandle || !GDataHandle)
	{
		return false;
	}

	AtomicStoreRelaxed(&GPendingDataHandle, ~UPTRINT(0));
	return true;
}

} // namespace Private
} // namespace Trace
} // namespace UE

#endif // UE_TRACE_ENABLED
