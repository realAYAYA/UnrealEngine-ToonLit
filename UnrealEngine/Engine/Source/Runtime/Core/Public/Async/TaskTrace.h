// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "HAL/Platform.h"
#include "Trace/Config.h"
#include "Trace/Trace.h"
#include "Trace/Detail/Channel.h"

namespace UE { namespace Trace { class FChannel; } }

#if !defined(UE_TASK_TRACE_ENABLED)
#if UE_TRACE_ENABLED && !IS_PROGRAM && !UE_BUILD_SHIPPING
#define UE_TASK_TRACE_ENABLED 1
#else
#define UE_TASK_TRACE_ENABLED 0
#endif
#endif

namespace ENamedThreads
{
	// Forward declare
	enum Type : int32;
}

namespace TaskTrace
{
	UE_TRACE_CHANNEL_EXTERN(TaskChannel);

	using FId = uint64;

	inline const FId InvalidId = ~FId(0);

	inline constexpr uint32 TaskTraceVersion = 1;

	FId CORE_API GenerateTaskId();

	void CORE_API Init();
	void CORE_API Created(FId TaskId, uint64 TaskSize); // optional, used only if a task was created but not launched immediately
	void CORE_API Launched(FId TaskId, const TCHAR* DebugName, bool bTracked, ENamedThreads::Type ThreadToExecuteOn, uint64 TaskSize);
	void CORE_API Scheduled(FId TaskId);
	void CORE_API SubsequentAdded(FId TaskId, FId SubsequentId);
	void CORE_API Started(FId TaskId);
	void CORE_API Finished(FId TaskId);
	void CORE_API Completed(FId TaskId);
	void CORE_API Destroyed(FId TaskId);

	struct FWaitingScope
	{
		CORE_API explicit FWaitingScope(const TArray<FId>& Tasks); // waiting for given tasks completion
		CORE_API explicit FWaitingScope(FId TaskId);
		CORE_API ~FWaitingScope();
	};

	struct FTaskTimingEventScope
	{
		CORE_API FTaskTimingEventScope(TaskTrace::FId InTaskId);
		CORE_API ~FTaskTimingEventScope();

	private:
		bool bIsActive = false;
		TaskTrace::FId TaskId = InvalidId;
	};

#if !UE_TASK_TRACE_ENABLED
	// NOOP implementation
	inline FId GenerateTaskId() { return InvalidId; }
	inline void Init() {}
	inline void Created(FId TaskId, uint64 TaskSize) {}
	inline void Launched(FId TaskId, const TCHAR* DebugName, bool bTracked, ENamedThreads::Type ThreadToExecuteOn, uint64 TaskSize) {}
	inline void Scheduled(FId TaskId) {}
	inline void SubsequentAdded(FId TaskId, FId SubsequentId) {}
	inline void Started(FId TaskId) {}
	inline void Finished(FId TaskId) {}
	inline void Completed(FId TaskId) {}
	inline void Destroyed(FId TaskId) {}
	inline FWaitingScope::FWaitingScope(const TArray<FId>& Tasks) {}
	inline FWaitingScope::FWaitingScope(FId TaskId) {}
	inline FWaitingScope::~FWaitingScope() {}
	inline FTaskTimingEventScope::FTaskTimingEventScope(TaskTrace::FId InTaskId) {}
	inline FTaskTimingEventScope::~FTaskTimingEventScope() {}
#endif // UE_TASK_TRACE_ENABLED
}

