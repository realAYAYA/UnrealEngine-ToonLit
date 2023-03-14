// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Builders/GLTFLogBuilder.h"
#include "Tasks/GLTFDelayedTask.h"

class GLTFEXPORTER_API FGLTFTaskBuilder : public FGLTFLogBuilder
{
public:

	FGLTFTaskBuilder(const FString& FileName, const UGLTFExportOptions* ExportOptions = nullptr);

	template <typename TaskType, typename... TaskArgTypes, typename = typename TEnableIf<TIsDerivedFrom<TaskType, FGLTFDelayedTask>::Value>::Type>
	bool ScheduleSlowTask(TaskArgTypes&&... Args)
	{
		return ScheduleSlowTask(MakeUnique<TaskType>(Forward<TaskArgTypes>(Args)...));
	}

	template <typename TaskType, typename = typename TEnableIf<TIsDerivedFrom<TaskType, FGLTFDelayedTask>::Value>::Type>
	bool ScheduleSlowTask(TUniquePtr<TaskType> Task)
	{
		return ScheduleSlowTask(TUniquePtr<FGLTFDelayedTask>(Task.Release()));
	}

	bool ScheduleSlowTask(TUniquePtr<FGLTFDelayedTask> Task);

	void ProcessSlowTasks(FFeedbackContext* Context = nullptr);

private:

	static FText GetPriorityMessageFormat(EGLTFTaskPriority Priority);

	int32 PriorityIndexLock;
	TMap<EGLTFTaskPriority, TArray<TUniquePtr<FGLTFDelayedTask>>> TasksByPriority;
};
