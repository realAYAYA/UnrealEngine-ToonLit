// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LearningArray.h"

namespace UE::Learning
{
	/**
	* Completion Mode
	*/
	enum class ECompletionMode : uint8
	{
		// Episode is still running
		Running = 0,

		// Episode ended early but was still in progress. Value function will be used to estimate final return.
		Truncated = 1,

		// Episode ended early and zero reward was expected for all future steps.
		Terminated = 2,
	};

	/**
	* This object stores essentially a list of instances that need to be reset.
	* It can be filled based on the completion status or manually.
	*/
	struct LEARNING_API FResetInstanceBuffer
	{
		void Resize(const int32 InMaxInstanceNum);

		void SetResetInstances(const FIndexSet Instances);
		void SetResetInstancesFromCompletions(const TLearningArrayView<1, const ECompletionMode> Completions, const FIndexSet Instances);
		const int32 GetResetInstanceNum() const;
		const FIndexSet GetResetInstances() const;

	private:

		int32 MaxInstanceNum = 0;
		int32 ResetInstanceNum = 0;
		FIndexSet ResetInstancesSet;
		TLearningArray<1, int32, TInlineAllocator<1>> ResetInstances;
	};

	namespace Completion
	{
		/**
		* Converts a ECompletionMode into a string.
		*/
		LEARNING_API const TCHAR* CompletionModeString(const ECompletionMode Completion);

		/**
		* Takes the logical Or of completions. More specifically, if either completion is `Terminated` that 
		* will be the result of this operator, otherwise either completion being `Truncated` takes priority 
		* over either being `Running`. Put simply: Terminated > Truncated > Running
		*/
		LEARNING_API ECompletionMode Or(const ECompletionMode Lhs, const ECompletionMode Rhs);

		/**
		* Set completions for all instances whose episode has reached the max number of steps.
		*
		* @param InOutCompletions				Output buffer to write completions to
		* @param EpisodeStepNums				Number of steps taken by each instance
		* @param EpisodeMaxStepNum				Maximum number of allowed steps
		* @param EpisodeEndCompletionMode		Completion mode to use for instances who reach the end of the episode
		* @param Instances						Instances to process
		*
		*/
		LEARNING_API void EvaluateEndOfEpisodeCompletions(
			TLearningArrayView<1, ECompletionMode> InOutCompletions,
			const TLearningArrayView<1, const int32> EpisodeStepNums,
			const int32 EpisodeMaxStepNum,
			const ECompletionMode EpisodeEndCompletionMode,
			const FIndexSet Instances);

	}
}


