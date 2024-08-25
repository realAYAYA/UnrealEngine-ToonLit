// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Analysis/StreamReader.h"

namespace UE {
namespace Trace {

////////////////////////////////////////////////////////////////////////////////
class FTransport
{
public:
	virtual					~FTransport() {}
	void					SetReader(FStreamReader& InReader);
	template <typename RetType>
	RetType const*			GetPointer();
	template <typename RetType>
	RetType const*			GetPointer(uint32 BlockSize);
	virtual void			Advance(uint32 BlockSize);
	virtual bool			IsEmpty() const { return true; }
	virtual void			DebugBegin() {}
	virtual void			DebugEnd() {}

protected:
	virtual const uint8*	GetPointerImpl(uint32 BlockSize);

	FStreamReader*			Reader;
};

////////////////////////////////////////////////////////////////////////////////
inline void FTransport::SetReader(FStreamReader& InReader)
{
	Reader = &InReader;
}

////////////////////////////////////////////////////////////////////////////////
template <typename RetType>
inline RetType const* FTransport::GetPointer()
{
	return GetPointer<RetType>(sizeof(RetType));
}

////////////////////////////////////////////////////////////////////////////////
template <typename RetType>
inline RetType const* FTransport::GetPointer(uint32 BlockSize)
{
	return (RetType const*)GetPointerImpl(BlockSize);
}

////////////////////////////////////////////////////////////////////////////////
inline void FTransport::Advance(uint32 BlockSize)
{
	Reader->Advance(BlockSize);
}

////////////////////////////////////////////////////////////////////////////////
inline const uint8* FTransport::GetPointerImpl(uint32 BlockSize)
{
	return Reader->GetPointer(BlockSize);
}

} // namespace Trace
} // namespace UE
