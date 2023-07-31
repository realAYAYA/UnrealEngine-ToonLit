// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Detail/Trace.h"

////////////////////////////////////////////////////////////////////////////////
#if UE_TRACE_ENABLED
#	define UE_TRACE_IMPL(...)
#	define UE_TRACE_API			TRACELOG_API
#else
#	define UE_TRACE_IMPL(...)	{ return __VA_ARGS__; }
#	define UE_TRACE_API			inline
#endif

////////////////////////////////////////////////////////////////////////////////
namespace UE {
namespace Trace {

// Field types
enum AnsiString {};
enum WideString {};

// Reference to a definition event.
template<typename IdType>	
struct TEventRef 
{
	using ReferenceType = IdType;

	TEventRef(IdType InId, uint32 InTypeId)
		: Id(InId)
		, RefTypeId(InTypeId)
	{
	}

	IdType Id;
	uint32 RefTypeId;

	uint64 GetHash() const;

private:
	TEventRef() = delete;
};

template <>
inline uint64 TEventRef<uint8>::GetHash() const
{
	return (uint64(RefTypeId) << 32) | Id;
}

template <>
inline uint64 TEventRef<uint16>::GetHash() const
{
	return (uint64(RefTypeId) << 32) | Id;
}

template <>
inline uint64 TEventRef<uint32>::GetHash() const
{
	return (uint64(RefTypeId) << 32) | Id;
}

template <>
inline uint64 TEventRef<uint64>::GetHash() const
{
	return (uint64(RefTypeId) << 32) ^ Id;
}

typedef TEventRef<uint8> FEventRef8;
typedef TEventRef<uint16> FEventRef16;	
typedef TEventRef<uint32> FEventRef32;
typedef TEventRef<uint64> FEventRef64;

template<typename IdType>
TEventRef<IdType> MakeEventRef(IdType InId, uint32 InTypeId)
{
	return TEventRef<IdType>(InId, InTypeId);
}

using OnConnectFunc = void(void);

struct FInitializeDesc
{
#if WITH_EDITOR
	uint32			TailSizeBytes = 32 << 20;
#else
	uint32			TailSizeBytes = 4 << 20; // can be set to 0 to disable the tail buffer
#endif
	uint32			ThreadSleepTimeInMS = 0;
	bool			bUseWorkerThread	= true;
	bool			bUseImportantCache	= true;
	OnConnectFunc*	OnConnectionFunc	= nullptr;
};

typedef void*		AllocFunc(SIZE_T, uint32);
typedef void		FreeFunc(void*, SIZE_T);
typedef void		ChannelIterFunc(const ANSICHAR*, bool, void*);	

struct FStatistics
{
	uint64		BytesSent;
	uint64		BytesTraced;
	uint64		MemoryUsed;
	uint32		CacheAllocated;	// Total memory allocated in cache buffers
	uint32		CacheUsed;		// Used cache memory; Important-marked events are stored in the cache.
	uint32		CacheWaste;		// Unused memory from retired cache buffers
};

UE_TRACE_API void	SetMemoryHooks(AllocFunc Alloc, FreeFunc Free) UE_TRACE_IMPL();
UE_TRACE_API void	Initialize(const FInitializeDesc& Desc) UE_TRACE_IMPL();
UE_TRACE_API void	StartWorkerThread() UE_TRACE_IMPL();	
UE_TRACE_API void	Shutdown() UE_TRACE_IMPL();
UE_TRACE_API void	Update() UE_TRACE_IMPL();
UE_TRACE_API void	GetStatistics(FStatistics& Out) UE_TRACE_IMPL();
UE_TRACE_API bool	SendTo(const TCHAR* Host, uint32 Port=0) UE_TRACE_IMPL(false);
UE_TRACE_API bool	WriteTo(const TCHAR* Path) UE_TRACE_IMPL(false);
UE_TRACE_API bool	WriteSnapshotTo(const TCHAR* Path) UE_TRACE_IMPL(false);
UE_TRACE_API bool	IsTracing() UE_TRACE_IMPL(false);
UE_TRACE_API bool	Stop() UE_TRACE_IMPL(false);
UE_TRACE_API bool	IsChannel(const TCHAR* ChanneName) UE_TRACE_IMPL(false);
UE_TRACE_API bool	ToggleChannel(const TCHAR* ChannelName, bool bEnabled) UE_TRACE_IMPL(false);
UE_TRACE_API void	EnumerateChannels(ChannelIterFunc IterFunc, void* User) UE_TRACE_IMPL();
UE_TRACE_API void	ThreadRegister(const TCHAR* Name, uint32 SystemId, int32 SortHint) UE_TRACE_IMPL();
UE_TRACE_API void	ThreadGroupBegin(const TCHAR* Name) UE_TRACE_IMPL();
UE_TRACE_API void	ThreadGroupEnd() UE_TRACE_IMPL();

} // namespace Trace
} // namespace UE

////////////////////////////////////////////////////////////////////////////////
#define UE_TRACE_EVENT_DEFINE(LoggerName, EventName)					TRACE_PRIVATE_EVENT_DEFINE(LoggerName, EventName)
#define UE_TRACE_EVENT_BEGIN(LoggerName, EventName, ...)				TRACE_PRIVATE_EVENT_BEGIN(LoggerName, EventName, ##__VA_ARGS__)
#define UE_TRACE_EVENT_BEGIN_EXTERN(LoggerName, EventName, ...)			TRACE_PRIVATE_EVENT_BEGIN_EXTERN(LoggerName, EventName, ##__VA_ARGS__)
#define UE_TRACE_EVENT_FIELD(FieldType, FieldName)						TRACE_PRIVATE_EVENT_FIELD(FieldType, FieldName)
#define UE_TRACE_EVENT_REFERENCE_FIELD(RefLogger, RefEvent, FieldName)	TRACE_PRIVATE_EVENT_REFFIELD(RefLogger, RefEvent, FieldName)
#define UE_TRACE_EVENT_END()											TRACE_PRIVATE_EVENT_END()
#define UE_TRACE_LOG(LoggerName, EventName, ChannelsExpr, ...)			TRACE_PRIVATE_LOG(LoggerName, EventName, ChannelsExpr, ##__VA_ARGS__)
#define UE_TRACE_LOG_SCOPED(LoggerName, EventName, ChannelsExpr, ...)	TRACE_PRIVATE_LOG_SCOPED(LoggerName, EventName, ChannelsExpr, ##__VA_ARGS__)
#define UE_TRACE_LOG_SCOPED_T(LoggerName, EventName, ChannelsExpr, ...)	TRACE_PRIVATE_LOG_SCOPED_T(LoggerName, EventName, ChannelsExpr, ##__VA_ARGS__)
#define UE_TRACE_GET_DEFINITION_TYPE_ID(LoggerName, EventName)			TRACE_PRIVATE_GET_DEFINITION_TYPE_ID(LoggerName, EventName)
#define UE_TRACE_LOG_DEFINITION(LoggerName, EventName, Id, ChannelsExpr, ...) TRACE_PRIVATE_LOG_DEFINITION(LoggerName, EventName, Id, ChannelsExpr, ##__VA_ARGS__)

////////////////////////////////////////////////////////////////////////////////
#define UE_TRACE_CHANNEL(ChannelName, ...)				TRACE_PRIVATE_CHANNEL(ChannelName, ##__VA_ARGS__)
#define UE_TRACE_CHANNEL_EXTERN(ChannelName, ...)		TRACE_PRIVATE_CHANNEL_EXTERN(ChannelName, ##__VA_ARGS__)
#define UE_TRACE_CHANNEL_DEFINE(ChannelName, ...)		TRACE_PRIVATE_CHANNEL_DEFINE(ChannelName, ##__VA_ARGS__)
#define UE_TRACE_CHANNELEXPR_IS_ENABLED(ChannelsExpr)	TRACE_PRIVATE_CHANNELEXPR_IS_ENABLED(ChannelsExpr)
