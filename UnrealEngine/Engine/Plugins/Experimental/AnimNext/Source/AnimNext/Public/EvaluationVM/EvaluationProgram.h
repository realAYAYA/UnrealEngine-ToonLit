// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EvaluationVM/EvaluationTask.h"

#include <type_traits>

namespace UE::AnimNext
{
	struct FEvaluationVM;

	/*
	 * Evaluation Program
	 *
	 * This struct holds a sequence of evaluation tasks that form a program within our
	 * evaluation virtual machine framework. Programs are immutable once written.
	 *
	 * @see FAnimNextEvaluationTask
	 */
	struct ANIMNEXT_API FEvaluationProgram
	{
		FEvaluationProgram() = default;

		// Allow moving
		FEvaluationProgram(FEvaluationProgram&&) = default;
		FEvaluationProgram& operator=(FEvaluationProgram&&) = default;

		// Returns whether or not this program is empty
		bool IsEmpty() const;

		// Appends a new task into the program, tasks mutate state in the order they have been appended in
		// This means that child nodes need to evaluate first, tasks will usually be appended in IEvaluate::PostEvaluate
		// Tasks are moved into their final memory location, caller can allocate the task anywhere, it is no longer needed after this operation
		template<class TaskType>
		void AppendTask(TaskType&& Task);

		// Executes the current program on the provided virtual machine
		void Execute(FEvaluationVM& VM) const;

		// Returns the program as a string suitable for debug purposes
		FString ToString() const;

	private:
		// Disallow copy
		FEvaluationProgram(const FEvaluationProgram&) = delete;
		FEvaluationProgram& operator=(const FEvaluationProgram&) = delete;

		// List of tasks
		TArray<TUniquePtr<FAnimNextEvaluationTask>> Tasks;
	};

	//////////////////////////////////////////////////////////////////////////
	// Implementation of inline functions

	template<class TaskType>
	inline void FEvaluationProgram::AppendTask(TaskType&& Task)
	{
		static_assert(std::is_base_of<FAnimNextEvaluationTask, TaskType>::value, "Task type must derive from FAnimNextEvaluationTask");

		// TODO: Use a fancy allocator to ensure all tasks are contiguous and tightly packed
		TUniquePtr<FAnimNextEvaluationTask> TaskCopy = MakeUnique<TaskType>(MoveTemp(Task));
		Tasks.Add(MoveTemp(TaskCopy));
	}
}
