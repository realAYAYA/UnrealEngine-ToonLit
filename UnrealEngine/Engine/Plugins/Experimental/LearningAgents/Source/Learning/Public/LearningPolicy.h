// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LearningNeuralNetwork.h" // Included for FNeuralNetworkInferenceSettings

#include "Templates/SharedPointer.h"

namespace UE::Learning
{
	/**
	* Neural-network based policy object. Stores various settings and intermediate
	* storage required to evaluate the given network for the provided number of instances.
	*/
	struct LEARNING_API FNeuralNetworkPolicy
	{
		/**
		 * Create a new Policy Object which takes the encoded observation state, and the memory state, and outputs the encoded action state.
		 *
		 * @param InMaxInstanceNum				Maximum number of instances
		 * @param InObservationEncodedNum		Size of the encoded observation vector
		 * @param InActionEncodedNum			Size of the encoded action vector
		 * @param InMemoryStateNum				Size of the memory state
		 * @param InNeuralNetwork				Neural network object
		 * @param InInferenceSettings			Inference settings
		 */
		FNeuralNetworkPolicy(
			const int32 InMaxInstanceNum,
			const int32 InObservationEncodedNum,
			const int32 InActionEncodedNum,
			const int32 InMemoryStateNum,
			const TSharedPtr<FNeuralNetwork>& InNeuralNetwork,
			const FNeuralNetworkInferenceSettings& InInferenceSettings = FNeuralNetworkInferenceSettings());

		/**
		 * Evaluate the Policy
		 *
		 * @param OutputActionVectorsEncoded		Output encoded action vectors of shape (MaxInstanceNum, ActionEncodedNum)
		 * @param OutputMemoryState					Output updated memory state of shape (MaxInstanceNum, MemoryStateNum)
		 * @param InputObservationVectorsEncoded	Input encoded observation vectors of shape (MaxInstanceNum, ObservationEncodedNum)
		 * @param InputMemoryState					Input memory state of shape (MaxInstanceNum, MemoryStateNum)
		 * @param Instances							Set of instances to evaluate
		 */
		void Evaluate(
			TLearningArrayView<2, float> OutputActionVectorsEncoded,
			TLearningArrayView<2, float> OutputMemoryState,
			const TLearningArrayView<2, const float> InputObservationVectorsEncoded,
			const TLearningArrayView<2, const float> InputMemoryState,
			const FIndexSet Instances);

		/** Sets the NeuralNetwork and re-creates the NeuralNetworkInference object */
		void UpdateNeuralNetwork(const TSharedPtr<FNeuralNetwork>& NewNeuralNetwork);

		/** Gets the NeuralNetwork associated with this Policy */
		const TSharedPtr<FNeuralNetwork>& GetNeuralNetwork() const;

	private:

		int32 ObservationEncodedNum = 0;
		int32 ActionEncodedNum = 0;
		int32 MemoryStateNum = 0;
		TSharedPtr<FNeuralNetworkFunction> NeuralNetworkFunction;

		TLearningArray<2, float> Input;
		TLearningArray<2, float> Output;
	};
}
