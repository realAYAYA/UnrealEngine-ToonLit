// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Asio.h"
#include "Foundation.h"

#include <thread>

////////////////////////////////////////////////////////////////////////////////
class FAsioContext
{
public:
							FAsioContext(int32 ThreadCount);
							~FAsioContext();
	asio::io_context&		Get();
	void					Start();
	void					Stop(bool bWait=false);
	void					Wait();

private:
	asio::io_context*		IoContext;
	TArray<std::thread>		ThreadPool;
	bool					bRunning = false;
};

/* vim: set noexpandtab : */
