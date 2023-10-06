// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGCommon.h"
#include "UObject/WeakObjectPtr.h" // IWYU pragma: keep

class UPCGComponent;
class UPCGNode;
enum class EPCGExecutionPhase : uint8;
struct FPCGContext;

class IPCGElement;

namespace PCGUtils
{
#if WITH_EDITOR
	struct PCG_API FCallTime
	{
		// sum of all frames
		double ExecutionTime = 0;
		// how many frames
		int32 ExecutionFrameCount = 0;
		double MinExecutionFrameTime = MAX_dbl;
		double MaxExecutionFrameTime = 0.0;
		double PrepareDataTime = 0.0;
		double PostExecuteTime = 0.0;
	};

	struct PCG_API FCapturedMessage
	{
		int32 Index = 0;
		FName Namespace;
		FString Message;
		ELogVerbosity::Type Verbosity;
	};

	struct PCG_API FCallTreeInfo
	{
		FPCGTaskId TaskId;
		FString Name; // name of the task, Node may be nullptr for generated tasks
		const UPCGNode* Node = nullptr;
		FCallTime CallTime;

		TArray<FCallTreeInfo> Children;
	};

	struct FScopedCall : public FOutputDevice
	{
		FScopedCall(const IPCGElement& InOwner, FPCGContext* InContext);
		virtual ~FScopedCall();

		const IPCGElement& Owner;
		FPCGContext* Context;
		double StartTime;
		EPCGExecutionPhase Phase;
		const uint32 ThreadID;
		TArray<FCapturedMessage> CapturedMessages;

		// FOutputDevice
		virtual bool IsMemoryOnly() const override { return true; }
		virtual bool CanBeUsedOnMultipleThreads() const override { return true; }
		virtual bool CanBeUsedOnAnyThread() const override { return true; }
		virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category) override;
	};

	class PCG_API FExtraCapture
	{
	public:
		void Update(const FScopedCall& InScopedCall);

		void ResetTimers();
		void ResetCapturedMessages();

		using TTimersMap = TMap<FPCGTaskId, FCallTime>;
		using TCapturedMessageMap = TMap<TWeakObjectPtr<const UPCGNode>, TArray<FCapturedMessage>>;

		const TTimersMap& GetTimers() const { return Timers; }
		const TCapturedMessageMap& GetCapturedMessages() const { return CapturedMessages; }

		FCallTreeInfo CalculateCallTreeInfo(const UPCGComponent* Component) const;

	private:
		TTimersMap Timers;
		TCapturedMessageMap CapturedMessages;
		mutable FCriticalSection Lock;
	};
#else
	struct FScopedCall
	{
		FScopedCall(const IPCGElement& InOwner, FPCGContext* InContext) {}
	};
#endif // WITH_EDITOR
}

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "PCGContext.h"
#include "UObject/UnrealType.h"
#endif
