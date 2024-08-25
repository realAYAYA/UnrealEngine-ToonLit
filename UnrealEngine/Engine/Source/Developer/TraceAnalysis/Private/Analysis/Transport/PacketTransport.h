// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Transport.h"

namespace UE {
namespace Trace {

////////////////////////////////////////////////////////////////////////////////
class FPacketTransport
	: public FTransport
{
public:
	virtual					~FPacketTransport();
	virtual void			Advance(uint32 BlockSize) override;
	virtual bool			IsEmpty() const override;
	virtual void			DebugBegin() override;
	virtual void			DebugEnd() override;

protected:
	virtual const uint8*	GetPointerImpl(uint32 BlockSize) override;

private:
	struct					FPacketNode;
	bool					GetNextBatch();
	FPacketNode*			AllocateNode();
	static const uint32		MaxPacketSize = 8192;
	FPacketNode*			ActiveList = nullptr;
	FPacketNode*			PendingList = nullptr;
	FPacketNode*			FreeList = nullptr;
};

} // namespace Trace
} // namespace UE
