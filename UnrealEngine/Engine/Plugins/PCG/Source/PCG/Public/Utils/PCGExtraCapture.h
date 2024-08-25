// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGCommon.h"
#include "UObject/WeakObjectPtr.h" // IWYU pragma: keep

class UPCGComponent;
class UPCGNode;
enum class EPCGExecutionPhase : uint8;
struct FPCGContext;
struct FPCGStack;

class IPCGElement;

namespace PCGUtils
{
#if WITH_EDITOR
	struct PCG_API FCallTime
	{
		// sum of all frames
		double ExecutionStartTime = MAX_dbl;
		double ExecutionTime = 0.0;
		double ExecutionEndTime = 0.0;
		// how many frames
		int32 ExecutionFrameCount = 0;
		double MinExecutionFrameTime = MAX_dbl;
		double MaxExecutionFrameTime = 0.0;

		double PrepareDataStartTime = MAX_dbl;
		double PrepareDataTime = 0.0;
		double PrepareDataEndTime = 0.0;
		int32 PrepareDataFrameCount = 0;

		double PostExecuteTime = 0.0;

		double PrepareDataWallTime() const { return PrepareDataEndTime - PrepareDataStartTime; }
		double ExecutionWallTime() const { return ExecutionEndTime - ExecutionStartTime; }
		double TotalTime() const { return ExecutionTime + PrepareDataTime; }
		double TotalWallTime() const { return ExecutionEndTime - PrepareDataStartTime; }
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
		const UPCGNode* Node = nullptr;
		int32 LoopIndex = INDEX_NONE;
		FString Name; // overriden name for the task, will take precedence over the node name if not empty
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

		void ResetCapturedMessages();

		using TCapturedMessageMap = TMap<TWeakObjectPtr<const UPCGNode>, TArray<FCapturedMessage>>;

		const TCapturedMessageMap& GetCapturedMessages() const { return CapturedMessages; }

		FCallTreeInfo CalculateCallTreeInfo(const UPCGComponent* Component, const FPCGStack& RootStack) const;

	private:
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
