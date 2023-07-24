// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Transport.h"
#include "Analysis/StreamReader.h"
#include "Containers/Array.h"
#include "Trace/Detail/Transport.h"

namespace UE {
namespace Trace {

class FStreamReader;

////////////////////////////////////////////////////////////////////////////////
class FTidPacketTransport
	: public FTransport
{
public:
	typedef UPTRINT ThreadIter;

	void					Update();
	uint32					GetThreadCount() const;
	FStreamReader*			GetThreadStream(uint32 Index);
	uint32					GetThreadId(uint32 Index) const;
	uint32					GetSyncCount() const;

private:
	struct FThreadStream
	{
		FStreamBuffer		Buffer;
		uint32				ThreadId;
	};

	bool					ReadPacket();
	FThreadStream*			FindOrAddThread(uint32 ThreadId, bool bAddIfNotFound);
	TArray<FThreadStream>	Threads = {
								{ {}, ETransportTid::Events },
								{ {}, ETransportTid::Importants },
							};

protected:
	uint32					Synced = 0x7fff'ffff;
};

////////////////////////////////////////////////////////////////////////////////
inline uint32 FTidPacketTransport::GetSyncCount() const
{
	return Synced;
}



////////////////////////////////////////////////////////////////////////////////
class FTidPacketTransportSync
	: public FTidPacketTransport
{
public:
	FTidPacketTransportSync()
	{
		Synced = 0;
	}
};

} // namespace Trace
} // namespace UE
