// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Asio.h"
#include "AsioTcpServer.h"
#include "AsioTickable.h"
#include "Foundation.h"

class FRecorder;
class FStore;
class FStoreCborPeer;
class FTraceRelay;

////////////////////////////////////////////////////////////////////////////////
class FStoreCborServer
	: public FAsioTcpServer
	, public FAsioTickable
{
public:
							FStoreCborServer(asio::io_context& IoContext, int32 Port, FStore& InStore, FRecorder& InRecorder);
							~FStoreCborServer();
	void					Close();
	FStore&					GetStore() const;
	FRecorder&				GetRecorder() const;
	FTraceRelay*			RelayTrace(uint32 Id);

private:
	virtual bool			OnAccept(asio::ip::tcp::socket& Socket) override;
	virtual void			OnTick() override;
	TArray<FStoreCborPeer*>	Peers;
	TArray<FTraceRelay*>	Relays;
	FStore&					Store;
	FRecorder&				Recorder;
};

/* vim: set noexpandtab : */
