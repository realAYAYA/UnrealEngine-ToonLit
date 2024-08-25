// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Trace/Config.h"

#if UE_TRACE_ENABLED

#include "Atomic.h"
#include "EventNode.h"
#include "HAL/Platform.h" // for PLATFORM_BREAK
#include "LogScope.h"
#include "Protocol.h"
#include "Writer.inl"

namespace UE {
namespace Trace {
namespace Private {

////////////////////////////////////////////////////////////////////////////////
extern TRACELOG_API uint64			GStartCycle;
extern TRACELOG_API uint32 volatile	GLogSerial;

////////////////////////////////////////////////////////////////////////////////
inline void FLogScope::Commit() const
{
	AtomicStoreRelease((uint8**) &(Buffer->Committed), Buffer->Cursor);
}

////////////////////////////////////////////////////////////////////////////////
inline void FLogScope::Commit(FWriteBuffer* __restrict LatestBuffer) const
{
	if (LatestBuffer != Buffer)
	{
		AtomicStoreRelease((uint8**) &(LatestBuffer->Committed), LatestBuffer->Cursor);
	}

	Commit();
}

////////////////////////////////////////////////////////////////////////////////
template <uint32 Flags>
inline auto FLogScope::EnterImpl(uint32 Uid, uint32 Size)
{
	TLogScope<(Flags & FEventInfo::Flag_MaybeHasAux) != 0> Ret;
	if ((Flags & FEventInfo::Flag_NoSync) != 0)
	{
		Ret.EnterNoSync(Uid, Size);
	}
	else
	{
		Ret.Enter(Uid, Size);
	}
	return Ret;
}

////////////////////////////////////////////////////////////////////////////////
template <class HeaderType>
inline void FLogScope::EnterPrelude(uint32 Size)
{
	uint32 AllocSize = sizeof(HeaderType) + Size;

	Buffer = Writer_GetBuffer();
	if (UNLIKELY(Buffer->Cursor + AllocSize > (uint8*)Buffer))
	{
		Buffer = Writer_NextBuffer();
	}

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	if (AllocSize >= Buffer->Size)
	{
		// This situation is terminal. Someone's trying to trace an event that
		// is far too large. This should never happen as 'maximum_field_count *
		// largest_field_size' won't exceed Buffer.Size.
		PLATFORM_BREAK();
	}
#endif

	Ptr = Buffer->Cursor + sizeof(HeaderType);
	Buffer->Cursor += AllocSize;
}

////////////////////////////////////////////////////////////////////////////////
inline void FLogScope::Enter(uint32 Uid, uint32 Size)
{
	EnterPrelude<FEventHeaderSync>(Size);

	uint16 Uid16 = uint16(Uid) | int32(EKnownEventUids::Flag_TwoByteUid);
	uint32 Serial = uint32(AtomicAddRelaxed(&GLogSerial, 1u));

	// Event header FEventHeaderSync
	memcpy(Ptr - 3, &Serial, sizeof(Serial)); /* FEventHeaderSync::SerialHigh,SerialLow */
	memcpy(Ptr - 5, &Uid16,  sizeof(Uid16));  /* FEventHeaderSync::Uid */
}

////////////////////////////////////////////////////////////////////////////////
inline void FLogScope::EnterNoSync(uint32 Uid, uint32 Size)
{
	EnterPrelude<FEventHeader>(Size);

	uint16 Uid16 = uint16(Uid) | int32(EKnownEventUids::Flag_TwoByteUid);

	// Event header FEventHeader
	auto* Header = (uint16*)(Ptr);
	memcpy(Header - 1, &Uid16, sizeof(Uid16)); /* FEventHeader::Uid */
}

////////////////////////////////////////////////////////////////////////////////
template <bool bMaybeHasAux>
inline void TLogScope<bMaybeHasAux>::operator += (const FLogScope&) const
{
	if constexpr (bMaybeHasAux)
	{
		FWriteBuffer* LatestBuffer = Writer_GetBuffer();
		LatestBuffer->Cursor[0] = uint8(EKnownEventUids::AuxDataTerminal << EKnownEventUids::_UidShift);
		LatestBuffer->Cursor++;

		Commit(LatestBuffer);
	}
	else
	{
		Commit();
	}
}

////////////////////////////////////////////////////////////////////////////////
inline FScopedLogScope::~FScopedLogScope()
{
	if (!bActive)
	{
		return;
	}

	uint8 LeaveUid = uint8(EKnownEventUids::LeaveScope << EKnownEventUids::_UidShift);

	FWriteBuffer* Buffer = Writer_GetBuffer();
	if (UNLIKELY(int32((uint8*)Buffer - Buffer->Cursor) < int32(sizeof(LeaveUid))))
	{
		Buffer = Writer_NextBuffer();
	}

	Buffer->Cursor[0] = LeaveUid;
	Buffer->Cursor += sizeof(LeaveUid);

	AtomicStoreRelease((uint8**) &(Buffer->Committed), Buffer->Cursor);
}

////////////////////////////////////////////////////////////////////////////////
inline void FScopedLogScope::SetActive()
{
	bActive = true;
}



////////////////////////////////////////////////////////////////////////////////
inline FScopedStampedLogScope::~FScopedStampedLogScope()
{
	if (!bActive)
	{
		return;
	}

	uint64 Stamp = TimeGetTimestamp() - GStartCycle;

	FWriteBuffer* Buffer = Writer_GetBuffer();
	if (UNLIKELY(int32((uint8*)Buffer - Buffer->Cursor) < int32(sizeof(Stamp))))
	{
		Buffer = Writer_NextBuffer();
	}

	Stamp <<= 8;
	Stamp += uint8(EKnownEventUids::LeaveScope_TB) << EKnownEventUids::_UidShift;
	memcpy((uint64*)(Buffer->Cursor), &Stamp, sizeof(Stamp));
	Buffer->Cursor += sizeof(Stamp);

	AtomicStoreRelease((uint8**) &(Buffer->Committed), Buffer->Cursor);
}

////////////////////////////////////////////////////////////////////////////////
inline void FScopedStampedLogScope::SetActive()
{
	bActive = true;
}



////////////////////////////////////////////////////////////////////////////////
template <class EventType>
FORCENOINLINE auto FLogScope::Enter()
{
	uint32 Size = EventType::GetSize();
	uint32 Uid = EventType::GetUid();
	return EnterImpl<EventType::EventFlags>(Uid, Size);
}

////////////////////////////////////////////////////////////////////////////////
template <class EventType>
FORCENOINLINE auto FLogScope::ScopedEnter()
{
	uint8 EnterUid = uint8(EKnownEventUids::EnterScope << EKnownEventUids::_UidShift);

	FWriteBuffer* Buffer = Writer_GetBuffer();
	constexpr int32 EventHeaderSize = (EventType::EventFlags & FEventInfo::Flag_NoSync) != 0 ? sizeof(FEventHeader) : sizeof(FEventHeaderSync);
	constexpr int32 RequiredSize = sizeof(EnterUid) + EventHeaderSize + EventType::GetSize();
	if (UNLIKELY(int32((uint8*)Buffer - Buffer->Cursor) < RequiredSize))
	{
		Buffer = Writer_NextBuffer();
	}

	Buffer->Cursor[0] = EnterUid;
	Buffer->Cursor += sizeof(EnterUid);

	AtomicStoreRelease((uint8**) &(Buffer->Committed), Buffer->Cursor);

	return Enter<EventType>();
}

////////////////////////////////////////////////////////////////////////////////
template <class EventType>
FORCENOINLINE auto FLogScope::ScopedStampedEnter()
{
	uint64 Stamp;

	FWriteBuffer* Buffer = Writer_GetBuffer();
	constexpr int32 EventHeaderSize = (EventType::EventFlags & FEventInfo::Flag_NoSync) != 0 ? sizeof(FEventHeader) : sizeof(FEventHeaderSync);
	constexpr int32 RequiredSize = sizeof(Stamp) + EventHeaderSize + EventType::GetSize();
	if (UNLIKELY(int32((uint8*)Buffer - Buffer->Cursor) < RequiredSize))
	{
		Buffer = Writer_NextBuffer();
	}

	Stamp = TimeGetTimestamp() - GStartCycle;
	Stamp <<= 8;
	Stamp += uint8(EKnownEventUids::EnterScope_TB) << EKnownEventUids::_UidShift;
	memcpy((uint64*)(Buffer->Cursor), &Stamp, sizeof(Stamp));
	Buffer->Cursor += sizeof(Stamp);

	AtomicStoreRelease((uint8**) &(Buffer->Committed), Buffer->Cursor);

	return Enter<EventType>();
}



////////////////////////////////////////////////////////////////////////////////
template <typename FieldMeta, typename Type>
struct FLogScope::FFieldSet
{
	static void Impl(FLogScope* Scope, const Type& Value)
	{
		uint8* Dest = (uint8*)(Scope->Ptr) + FieldMeta::Offset;
		::memcpy(Dest, &Value, sizeof(Type));
	}
};

////////////////////////////////////////////////////////////////////////////////
template <typename FieldMeta, typename Type>
struct FLogScope::FFieldSet<FieldMeta, Type[]>
{
	static void Impl(FLogScope*, Type const* Data, int32 Num)
	{
		static const uint32 Index = FieldMeta::Index & int32(EIndexPack::NumFieldsMask);
		int32 Size = (Num * sizeof(Type)) & (FAuxHeader::SizeLimit - 1) & ~(sizeof(Type) - 1);
		Field_WriteAuxData(Index, (const uint8*)Data, Size);
	}
};

#if STATICALLY_SIZED_ARRAY_FIELDS_SUPPORT
////////////////////////////////////////////////////////////////////////////////
template <typename FieldMeta, typename Type, int32 Count>
struct FLogScope::FFieldSet<FieldMeta, Type[Count]>
{
	static void Impl(FLogScope*, Type const* Data, int32 Num=-1) = delete;
};
#endif // STATICALLY_SIZED_ARRAY_FIELDS_SUPPORT

////////////////////////////////////////////////////////////////////////////////
template <typename FieldMeta>
struct FLogScope::FFieldSet<FieldMeta, AnsiString>
{
	static void Impl(FLogScope*, const ANSICHAR* String, int32 Length=-1)
	{
		if (Length < 0)
		{
			Length = int32(strlen(String));
		}

		static const uint32 Index = FieldMeta::Index & int32(EIndexPack::NumFieldsMask);
		Field_WriteStringAnsi(Index, String, Length);
	}

	static void Impl(FLogScope*, const WIDECHAR* String, int32 Length=-1)
	{
		if (Length < 0)
		{
			Length = 0;
			for (const WIDECHAR* c = String; *c; ++c, ++Length);
		}

		static const uint32 Index = FieldMeta::Index & int32(EIndexPack::NumFieldsMask);
		Field_WriteStringAnsi(Index, String, Length);
	}
};

////////////////////////////////////////////////////////////////////////////////
template <typename FieldMeta>
struct FLogScope::FFieldSet<FieldMeta, WideString>
{
	static void Impl(FLogScope*, const WIDECHAR* String, int32 Length=-1)
	{
		if (Length < 0)
		{
			Length = 0;
			for (const WIDECHAR* c = String; *c; ++c, ++Length);
		}

		static const uint32 Index = FieldMeta::Index & int32(EIndexPack::NumFieldsMask);
		Field_WriteStringWide(Index, String, Length);
	}
};

////////////////////////////////////////////////////////////////////////////////
template <typename FieldMeta, typename DefinitionType>
struct FLogScope::FFieldSet<FieldMeta, TEventRef<DefinitionType>>
{
	static void Impl(FLogScope* Scope, const TEventRef<DefinitionType>& Reference)
	{
		FFieldSet<FieldMeta, DefinitionType>::Impl(Scope, Reference.Id);
	}
};

} // namespace Private
} // namespace Trace
} // namespace UE

#endif // UE_TRACE_ENABLED
