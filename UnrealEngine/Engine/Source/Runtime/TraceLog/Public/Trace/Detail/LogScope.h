// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Trace/Config.h"

#if UE_TRACE_ENABLED

#include "Writer.inl"

namespace UE {
namespace Trace {
namespace Private {

struct FWriteBuffer;
template <bool bMaybeHasAux> class TLogScope;

////////////////////////////////////////////////////////////////////////////////
class FLogScope
{
	friend class FEventNode;

public:
	template <typename EventType>
	static auto				Enter();
	template <typename EventType>
	static auto				ScopedEnter();
	template <typename EventType>
	static auto				ScopedStampedEnter();
	void*					GetPointer() const			{ return Ptr; }
	const FLogScope&		operator << (bool) const	{ return *this; }
	constexpr explicit		operator bool () const		{ return true; }

	template <typename FieldMeta, typename Type>
	struct FFieldSet;

protected:
	void					Commit() const;
	void					Commit(FWriteBuffer* __restrict LatestBuffer) const;

private:
	template <uint32 Flags>
	static auto				EnterImpl(uint32 Uid, uint32 Size);
	template <class T> inline void	EnterPrelude(uint32 Size);
	inline void				Enter(uint32 Uid, uint32 Size);
	inline void				EnterNoSync(uint32 Uid, uint32 Size);
	uint8*					Ptr;
	FWriteBuffer*			Buffer;
};

template <bool bMaybeHasAux>
class TLogScope
	: public FLogScope
{
public:
	inline void				operator += (const FLogScope&) const;
};



////////////////////////////////////////////////////////////////////////////////
class FScopedLogScope
{
public:
			~FScopedLogScope();
	void	SetActive();
	bool	bActive = false;
};

////////////////////////////////////////////////////////////////////////////////
class FScopedStampedLogScope
{
public:
			~FScopedStampedLogScope();
	void	SetActive();
	bool	bActive = false;
};

} // namespace Private
} // namespace Trace
} // namespace UE

#endif // UE_TRACE_ENABLED
