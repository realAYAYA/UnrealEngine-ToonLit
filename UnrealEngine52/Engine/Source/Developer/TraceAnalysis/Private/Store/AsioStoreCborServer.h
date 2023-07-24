// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Asio/Asio.h"
#include "AsioTcpServer.h"
#include "AsioTickable.h"
#include "Containers/Array.h"

namespace UE {
namespace Trace {

class FAsioRecorder;
class FAsioStore;
class FAsioStoreCborPeer;
class FAsioTraceRelay;

////////////////////////////////////////////////////////////////////////////////
class FAsioStoreCborServer
	: public FAsioTcpServer
	, public FAsioTickable
{
public:
								FAsioStoreCborServer(asio::io_context& IoContext, FAsioStore& InStore, FAsioRecorder& InRecorder);
								~FAsioStoreCborServer();
	void						Close();
	FAsioStore&					GetStore() const;
	FAsioRecorder&				GetRecorder() const;
	FAsioTraceRelay*			RelayTrace(uint32 Id);

private:
	virtual bool				OnAccept(asio::ip::tcp::socket& Socket) override;
	virtual void				OnTick() override;
	TArray<FAsioStoreCborPeer*>	Peers;
	TArray<FAsioTraceRelay*>	Relays;
	FAsioStore&					Store;
	FAsioRecorder&				Recorder;
};

} // namespace Trace
} // namespace UE
