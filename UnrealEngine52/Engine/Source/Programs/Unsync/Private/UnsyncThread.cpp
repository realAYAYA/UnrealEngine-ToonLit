// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnsyncThread.h"

#include <chrono>

namespace unsync {

uint32 GMaxThreads = std::min<uint32>(UNSYNC_MAX_TOTAL_THREADS, std::thread::hardware_concurrency());

#if UNSYNC_USE_CONCRT
FConcurrencyPolicyScope::FConcurrencyPolicyScope(uint32 MaxConcurrency)
{
	auto Policy = Concurrency::CurrentScheduler::GetPolicy();

	const uint32 CurrentMaxConcurrency = Policy.GetPolicyValue(Concurrency::PolicyElementKey::MaxConcurrency);
	const uint32 CurrentMinConcurrency = Policy.GetPolicyValue(Concurrency::PolicyElementKey::MinConcurrency);

	MaxConcurrency = std::min(MaxConcurrency, std::thread::hardware_concurrency());
	MaxConcurrency = std::min(MaxConcurrency, CurrentMaxConcurrency);
	MaxConcurrency = std::max(1u, MaxConcurrency);

	Policy.SetConcurrencyLimits(CurrentMinConcurrency, MaxConcurrency);

	Concurrency::CurrentScheduler::Create(Policy);
}

FConcurrencyPolicyScope::~FConcurrencyPolicyScope()
{
	Concurrency::CurrentScheduler::Detach();
}

void
SchedulerSleep(uint32 Milliseconds)
{
	concurrency::event E;
	E.reset();
	E.wait(Milliseconds);
}

void
SchedulerYield()
{
	concurrency::Context::YieldExecution();
}

#else  // UNSYNC_USE_CONCRT

FConcurrencyPolicyScope::FConcurrencyPolicyScope(uint32 MaxConcurrency)
{
	// TODO
}

FConcurrencyPolicyScope::~FConcurrencyPolicyScope()
{
	// TODO
}

void
SchedulerSleep(uint32 Milliseconds)
{
	std::this_thread::sleep_for(std::chrono::milliseconds(Milliseconds));
}

void
SchedulerYield()
{
	// TODO
}

#endif	// UNSYNC_USE_CONCRT

}  // namespace unsync
