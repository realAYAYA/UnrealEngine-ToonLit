// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGCommon.h"

#include "Containers/UnrealString.h"
#include "HAL/Platform.h"

class UPCGComponent;
class UPCGGraph;
enum class EPCGHiGenGrid : uint32;
struct FPCGContext;
struct FPCGGraphActiveTask;
struct FPCGGraphTask;
struct FPCGPinDependencyExpression;
struct FPCGStack;

namespace PCGGraphExecutionLogging
{
	bool LogEnabled();
	bool CullingLogEnabled();

	void LogGraphTask(FPCGTaskId TaskId, const FPCGGraphTask& Task, const TSet<FPCGTaskId>* SuccessorIds = nullptr);
	void LogGraphTasks(const TMap<FPCGTaskId, FPCGGraphTask>& Tasks, const TMap<FPCGTaskId, TSet<FPCGTaskId>>* TaskSuccessors = nullptr);
	void LogGraphTasks(const TArray<FPCGGraphTask>& Tasks);

	void LogGraphSchedule(const UPCGComponent* SourceComponent, const UPCGGraph* InScheduledGraph);
	void LogGraphScheduleDependency(const UPCGComponent* InComponent, const FPCGStack* InFromStack);
	void LogGraphScheduleDependencyFailed(const UPCGComponent* InComponent, const FPCGStack* InFromStack);

	void LogGraphPostSchedule(const TMap<FPCGTaskId, FPCGGraphTask>& Tasks, const TMap<FPCGTaskId, TSet<FPCGTaskId>>& TaskSuccessors);
	void LogPostProcessGraph(const UPCGComponent* InSourceComponent);

	void LogComponentCancellation(const TSet<UPCGComponent*>& CancelledComponents);

	void LogChangeOriginIgnoredForComponent(const UObject* InObject, const UPCGComponent* InComponent);

	void LogGraphExecuteFrameFinished();

	void LogTaskExecute(const FPCGGraphTask& Task);
	void LogTaskExecuteCachingDisabled(const FPCGGraphTask& Task);

	void LogTaskCullingBegin(FPCGTaskId CompletedTaskId, uint64 InactiveOutputPinBitmask, const TArray<FPCGPinId>& PinIdsToDeactivate);
	void LogTaskCullingBeginLoop(FPCGTaskId PinTaskId, uint64 PinIndex, const TArray<FPCGPinId>& PinIdsToDeactivate);
	void LogTaskCullingUpdatedPinDeps(FPCGTaskId TaskId, const FPCGPinDependencyExpression& PinDependency, bool bDependencyExpressionBecameFalse);

	void LogGridLinkageTaskExecuteStore(const FPCGContext* InContext, EPCGHiGenGrid InGenerationGrid, int32 InFromLOD, int32 InToLOD, const FString& InResourcePath);
	void LogGridLinkageTaskExecuteRetrieve(const FPCGContext* InContext, EPCGHiGenGrid InGenerationGrid, int32 InFromLOD, int32 InToLOD, const FString& InResourcePath);
	void LogGridLinkageTaskExecuteRetrieveSuccess(const FPCGContext* InContext, const UPCGComponent* InComponent, const FString& InResourcePath, int32 InDataItemCount);
	void LogGridLinkageTaskExecuteRetrieveScheduleGraph(const FPCGContext* InContext, const UPCGComponent* InScheduledComponent, const FString& InResourcePath);
	void LogGridLinkageTaskExecuteRetrieveWaitOnScheduledGraph(const FPCGContext* InContext, const UPCGComponent* InWaitOnComponent, const FString& InResourcePath);
	void LogGridLinkageTaskExecuteRetrieveWakeUp(const FPCGContext* InContext, const UPCGComponent* InWokenBy);
	void LogGridLinkageTaskExecuteRetrieveNoLocalComponent(const FPCGContext* InContext, const FString& InResourcePath);
	void LogGridLinkageTaskExecuteRetrieveNoData(const FPCGContext* InContext, const UPCGComponent* InComponent, const FString& InResourcePath);
}
