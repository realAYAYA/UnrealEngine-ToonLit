// Copyright Epic Games, Inc. All Rights Reserved.

#include "AsioContext.h"

namespace UE {
namespace Trace {

////////////////////////////////////////////////////////////////////////////////
FAsioContext::FAsioContext(int32 ThreadCount)
{
	if (ThreadCount < 1)
	{
		ThreadCount = 1;
	}

	IoContext = new asio::io_context(ThreadCount);

	ThreadPool.SetNum(ThreadCount);
}

////////////////////////////////////////////////////////////////////////////////
FAsioContext::~FAsioContext()
{
	Wait();
	delete IoContext;
}

////////////////////////////////////////////////////////////////////////////////
void FAsioContext::Start()
{
	if (bRunning)
	{
		return;
	}

	for (std::thread& Thread : ThreadPool)
	{
		std::thread TempThread([this] () { IoContext->run(); });
		Thread = MoveTemp(TempThread);
	}

	bRunning = true;
}

////////////////////////////////////////////////////////////////////////////////
void FAsioContext::Stop(bool bWait)
{
	if (!bRunning)
	{
		return;
	}

	IoContext->stop();

	if (bWait)
	{
		Wait();
	}
}

////////////////////////////////////////////////////////////////////////////////
void FAsioContext::Wait()
{
	for (std::thread& Thread : ThreadPool)
	{
		if (Thread.joinable())
		{
			Thread.join();
		}
	}

	bRunning = false;
}

////////////////////////////////////////////////////////////////////////////////
asio::io_context& FAsioContext::Get()
{
	return *IoContext;
}

} // namespace Trace
} // namespace UE
