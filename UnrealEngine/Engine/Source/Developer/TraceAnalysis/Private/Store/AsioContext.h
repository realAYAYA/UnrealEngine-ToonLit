// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Asio/Asio.h"
#include "Containers/Array.h"

#include <thread>

namespace UE {
namespace Trace {

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

} // namespace Trace
} // namespace UE
