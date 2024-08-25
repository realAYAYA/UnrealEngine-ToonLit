// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Transport.h"
#include "Analysis/StreamReader.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "TraceAnalysisDebug.h"
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
	
	enum class ETransportResult : uint8
	{
		Ok,
		Error
	};

	virtual bool			IsEmpty() const override;
	virtual void			DebugBegin() override;
	virtual void			DebugEnd() override;

	ETransportResult		Update();
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

	enum class EReadPacketResult : uint8
	{
		NeedMoreData,
		Continue,
		ReadError
	};

	EReadPacketResult		ReadPacket();
	FThreadStream*			FindOrAddThread(uint32 ThreadId, bool bAddIfNotFound);
	void					DebugUpdate();

	TArray<FThreadStream>	Threads = {
								{ {}, ETransportTid::Events },
								{ {}, ETransportTid::Importants },
							};

protected:
	uint32					Synced = 0x7fff'ffff;

#if UE_TRACE_ANALYSIS_DEBUG
private:
	uint64					TotalPacketHeaderSize = 0;
	uint64					TotalPacketSize = 0;
	uint64					TotalDecodedSize = 0;
	uint32					NumPackets = 0;
	uint32					MaxBufferSize = 0;
	uint32					MaxBufferSizeThreadId = 0;
	uint32					MaxDataSizePerBuffer = 0;
	uint32					MaxDataSizePerBufferThreadId = 0;
	TMap<uint32, uint64>	DataSizePerThread;
#endif // UE_TRACE_ANALYSIS_DEBUG
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
