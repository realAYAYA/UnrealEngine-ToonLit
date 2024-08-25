// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LearningArray.h"

namespace UE::Learning
{
	enum class ECompletionMode : uint8;
	struct FResetInstanceBuffer;

	/**
	* Buffer storing the observations, actions, and rewards of multiple instances over an episode
	*/
	struct LEARNINGTRAINING_API FEpisodeBuffer
	{
		/**
		* Resize the experience buffer
		*
		* @param InMaxInstanceNum				Maximum number of instances
		* @param InMaxStepNum					Maximum number of steps in an episode
		* @param InObservationVectorDimNum		Number of dimensions of the observation vector
		* @param InActionVectorDimNum			Number of dimensions of the action vector
		* @param InMemoryStateVectorDimNum		Number of dimensions of the memory state vector
		*/
		void Resize(
			const int32 InMaxInstanceNum,
			const int32 InMaxStepNum,
			const int32 InObservationVectorDimNum,
			const int32 InActionVectorDimNum,
			const int32 InMemoryStateVectorDimNum);

		/**
		* Reset the buffer for the given set of instances
		*/
		void Reset(const FIndexSet Instances);

		/**
		* Push new experience to the buffer
		*
		* @param InObservations					Observation vectors of shape (MaxInstanceNum, ObservationVectorDimNum)
		* @param InActions						Action vectors of shape (MaxInstanceNum, ActionVectorDimNum)
		* @param InMemoryStates					Memory state vectors (pre-evaluation) of shape (MaxInstanceNum, MemoryStateVectorDimNum)
		* @param InRewards						Rewards of shape (MaxInstanceNum)
		* @param Instances						Instances to push to buffer
		*/
		void Push(
			const TLearningArrayView<2, const float> InObservations,
			const TLearningArrayView<2, const float> InActions,
			const TLearningArrayView<2, const float> InMemoryStates,
			const TLearningArrayView<1, const float> InRewards,
			const FIndexSet Instances);

		const int32 GetMaxInstanceNum() const;
		const int32 GetMaxStepNum() const;
		const TLearningArrayView<1, const int32> GetEpisodeStepNums() const;
		const TLearningArrayView<2, const float> GetObservations(const int32 InstanceIdx) const;
		const TLearningArrayView<2, const float> GetActions(const int32 InstanceIdx) const;
		const TLearningArrayView<2, const float> GetMemoryStates(const int32 InstanceIdx) const;
		const TLearningArrayView<1, const float> GetRewards(const int32 InstanceIdx) const;

	private:

		int32 MaxInstanceNum = 0;
		int32 MaxStepNum = 0;
		TLearningArray<1, int32> EpisodeStepNums;
		TLearningArray<3, float> Observations;
		TLearningArray<3, float> Actions;
		TLearningArray<3, float> MemoryStates;
		TLearningArray<2, float> Rewards;
	};


	/**
	* Large buffer that sequentially concatenates a series of episodes in a large
	* flat array. Used to collate episodic data together from multiple instances.
	*/
	struct LEARNINGTRAINING_API FReplayBuffer
	{
		/**
		* Resizes the replay buffer.
		*
		* @param ObservationVectorDimNum		Dimensionality of observation vector
		* @param ActionVectorDimNum				Dimensionality of action vector
		* @param MemoryStateVectorDimNum		Dimensionality of memory state vector
		* @param MaxEpisodeNum					Maximum number of episodes to be stored in the buffer
		* @param MaxStepNum						Maximum number of steps to be stored in the buffer
		*/
		void Resize(
			const int32 InObservationVectorDimNum,
			const int32 InActionVectorDimNum,
			const int32 InMemoryStateVectorDimNum,
			const int32 InMaxEpisodeNum = 2048,
			const int32 InMaxStepNum = 16384);

		/**
		* Reset the replay buffer. Does not free memory - just resets episode and sample num to zero.
		*/
		void Reset();

		/**
		* Add a set of episodes to the replay buffer
		*
		* @param InEpisodeCompletionModes		Array of completion modes for each instance of shape (InstanceNum)
		* @param InEpisodeFinalObservations		Array of final observations for each instance of shape (InstanceNum, ObservationVectorDimNum)
		* @param InEpisodeFinalMemoryStates		Array of final memory states (post-evaluation) for each instance of shape (InstanceNum, MemoryStateVectorDimNum)
		* @param EpisodeBuffer					Episode buffer to add experience from
		* @param Instances						Instances to add
		* @param bAddTruncatedEpisodeWhenFull	When enabled, this will add a truncated, partial episode to the buffer when full
		*
		* @returns								True when the replay buffer is full
		*/
		bool AddEpisodes(
			const TLearningArrayView<1, const ECompletionMode> InEpisodeCompletionModes,
			const TLearningArrayView<2, const float> InEpisodeFinalObservations,
			const TLearningArrayView<2, const float> InEpisodeFinalMemoryStates,
			const FEpisodeBuffer& EpisodeBuffer,
			const FIndexSet Instances,
			const bool bAddTruncatedEpisodeWhenFull = true);

		const int32 GetMaxEpisodeNum() const;
		const int32 GetMaxStepNum() const;
		const int32 GetEpisodeNum() const;
		const int32 GetStepNum() const;
		const TLearningArrayView<1, const int32> GetEpisodeStarts() const;
		const TLearningArrayView<1, const int32> GetEpisodeLengths() const;
		const TLearningArrayView<1, const ECompletionMode> GetEpisodeCompletionModes() const;
		const TLearningArrayView<2, const float> GetEpisodeFinalObservations() const;
		const TLearningArrayView<2, const float> GetEpisodeFinalMemoryStates() const;
		const TLearningArrayView<2, const float> GetObservations() const;
		const TLearningArrayView<2, const float> GetActions() const;
		const TLearningArrayView<2, const float> GetMemoryStates() const;
		const TLearningArrayView<1, const float> GetRewards() const;

	private:

		int32 MaxEpisodeNum = 0;
		int32 MaxStepNum = 0;

		int32 EpisodeNum = 0;
		int32 StepNum = 0;

		TLearningArray<1, int32> EpisodeStarts;
		TLearningArray<1, int32> EpisodeLengths;
		TLearningArray<1, ECompletionMode> EpisodeCompletionModes;
		TLearningArray<2, float> EpisodeFinalObservations;
		TLearningArray<2, float> EpisodeFinalMemoryStates;
		TLearningArray<2, float> Observations;
		TLearningArray<2, float> Actions;
		TLearningArray<2, float> MemoryStates;
		TLearningArray<1, float> Rewards;
	};

	namespace Experience
	{
		/**
		* Resets, and then runs experience gathering until the provided replay buffer is full
		*
		* @param ReplayBuffer							Replay Buffer
		* @param EpisodeBuffer							Episode Buffer
		* @param ResetBuffer							Reset Buffer
		* @param ObservationVectorBuffer				Buffer to read/write observation vectors into
		* @param ActionVectorBuffer						Buffer to read/write action vectors into
		* @param PreEvaluationMemoryStateVectorBuffer	Buffer to read/write pre-evaluation memory state vectors into
		* @param MemoryStateVectorBuffer				Buffer to read/write (post-evaluation) memory state vectors into
		* @param RewardBuffer							Buffer to read/write rewards into
		* @param CompletionBuffer						Buffer to read/write completions into
		* @param EpisodeCompletionBuffer				Additional buffer to record completions from full episode buffers
		* @param AllCompletionBuffer					Additional buffer to record all completions from full episodes and normal completions
		* @param ResetFunction							Function to run for resetting the environment
		* @param ObservationFunction					Function to run for evaluating observations
		* @param PolicyFunction							Function to run generating actions from observations
		* @param ActionFunction							Function to run for evaluating actions
		* @param UpdateFunction							Function to run for updating the environment
		* @param RewardFunction							Function to run for evaluating rewards
		* @param CompletionFunction						Function to run for evaluating completions
		* @param Instances								Set of instances to gather experience for
		*/
		LEARNINGTRAINING_API void GatherExperienceUntilReplayBufferFull(
			FReplayBuffer& ReplayBuffer,
			FEpisodeBuffer& EpisodeBuffer,
			FResetInstanceBuffer& ResetBuffer,
			TLearningArrayView<2, float> ObservationVectorBuffer,
			TLearningArrayView<2, float> ActionVectorBuffer,
			TLearningArrayView<2, float> PreEvaluationMemoryStateVectorBuffer,
			TLearningArrayView<2, float> MemoryStateVectorBuffer,
			TLearningArrayView<1, float> RewardBuffer,
			TLearningArrayView<1, ECompletionMode> CompletionBuffer,
			TLearningArrayView<1, ECompletionMode> EpisodeCompletionBuffer,
			TLearningArrayView<1, ECompletionMode> AllCompletionBuffer,
			const TFunctionRef<void(const FIndexSet Instances)> ResetFunction,
			const TFunctionRef<void(const FIndexSet Instances)> ObservationFunction,
			const TFunctionRef<void(const FIndexSet Instances)> PolicyFunction,
			const TFunctionRef<void(const FIndexSet Instances)> ActionFunction,
			const TFunctionRef<void(const FIndexSet Instances)> UpdateFunction,
			const TFunctionRef<void(const FIndexSet Instances)> RewardFunction,
			const TFunctionRef<void(const FIndexSet Instances)> CompletionFunction,
			const FIndexSet Instances);
	}
};