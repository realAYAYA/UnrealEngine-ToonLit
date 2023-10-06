// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"
#include "UObject/Object.h"
#include "NeuralMorphModel.h"
#include "NeuralMorphNetwork.generated.h"

class UNeuralMorphNetworkInstance;


/** A fully connected layer, which contains the weights and biases for those connections. */ 
UCLASS()
class NEURALMORPHMODEL_API UNeuralMorphMLPLayer
	: public UObject
{
	GENERATED_BODY()

public:
	/** The weight matrix number of inputs (rows). */
	UPROPERTY()
	int32 NumInputs = 0;

	/** The weight matrix number of outputs (columns). */
	UPROPERTY()
	int32 NumOutputs = 0;

	/** The number of instances of inputs and outputs. */
	UPROPERTY()
	int32 Depth = 1;

	/** A 2D array of weights. The number of weights equals NumRows x NumColumns x Depth. */
	UPROPERTY()
	TArray<float> Weights;

	/** The biases. The number of biases will be the same as the number of columns multiplied by the depth. */
	UPROPERTY()
	TArray<float> Biases;
};


/**
 * An MLP neural network.
 */
UCLASS()
class NEURALMORPHMODEL_API UNeuralMorphMLP
	: public UObject
{
	GENERATED_BODY()

public:
	/** Clear the network, getting rid of all weights and biases. */
	void Empty();

	/** 
	 * Check if the network is empty or not.
	 * If it is empty, it means it hasn't been loaded, and cannot do anything.
	 * Empty means it has no layers.
	 * @return Returns true if empty, otherwise false is returned.
	 */
	bool IsEmpty() const;

	/**
	 * Load the network from a file on disk.
	 * When loading fails, the network will be emptied.
	 * @param Archive The archive to load from.
	 * @return Returns true when successfully loaded this model, otherwise false is returned.
	 */
	bool Load(FArchive& FileReader);

	/**
	 * Get the number of inputs, which is the number of floats the network takes as input.
	 * @result The number of input floats to the network.
	 */
	int32 GetNumInputs() const;

	/**
	 * Get the number of outputs, which is the number of floats the network will output.
	 * @return The number of floats that the network outputs.
	 */
	int32 GetNumOutputs() const;

	/**
	 * Get the number of network layers.
	 * This equals to the number of hidden layers plus one.
	 * @return The number of network layers.
	 */
	int32 GetNumLayers() const;

	/**
	 * Get a given network layer.
	 * @return A reference to the layer, which will contain the weights and biases.
	 */
	UNeuralMorphMLPLayer& GetLayer(int32 Index) const;

	/**
	 * Get the maximum number of inputs for all layers in the network.
	 * This is used to pre-allocate a temp buffer used to pass the data through the layers.
	 * @return The maximum number of inputs of all layers.
	 */
	int32 GetMaxNumLayerInputs() const;

	/**
	 * Get the maximum number of outputs for all layers in the network.
	 * This is used to pre-allocate a temp buffer used to pass the data through the layers.
	 * @return The maximum number of outputs of all layers.
	 */
	int32 GetMaxNumLayerOutputs() const;

private:
	/**
	 * Load an MLP fully connected layer from a given archive.
	 * @param Archive The archive to read from.
	 */
	UNeuralMorphMLPLayer* LoadNetworkLayer(FArchive& Archive);

private:
	/** The network weights and biases of the main network. */
	UPROPERTY()
	TArray<TObjectPtr<UNeuralMorphMLPLayer>> Layers;
};


/**
 * The specialized neural network for the Neural Morph Model.
 * This class is used to do inference at runtime at a higher performance than using UNeuralNetwork.
 * The reason why it is faster is because it is a highly specialized network for this specific model.
 * When the model is a local mode based model, there can be actually two neural networks inside this: one main network and a bone/curve group network.
 * The group network uses groups of bones or curves to generate morph targets, rather than individual bones/curves.
 */
UCLASS()
class NEURALMORPHMODEL_API UNeuralMorphNetwork
	: public UObject
{
	GENERATED_BODY()

public:
	/** Clear the network, getting rid of all weights and biases. */
	void Empty();

	/** 
	 * Check if the network is empty or not.
	 * If it is empty, it means it hasn't been loaded, and cannot do anything.
	 * Empty means it has no inputs, outputs or weights.
	 * @return Returns true if empty, otherwise false is returned.
	 */
	bool IsEmpty() const;

	/**
	 * Load the network from a file on disk.
	 * When loading fails, the network will be emptied.
	 * @param Filename The file path to load from.
	 * @return Returns true when successfully loaded this model, otherwise false is returned.
	 */
	bool Load(const FString& Filename);

	/**
	 * Create an instance of this neural network.
	 * @return A pointer to the neural network instance.
	 */
	UNeuralMorphNetworkInstance* CreateInstance();

	/**
	 * Get the number of network inputs.
	 * This is the sum of the number of main network inputs and group network inputs.
	 * @return The number of inputs.
	 */
	int32 GetNumInputs() const;

	/**
	 * Get the number of network outputs.
	 * This is the sum of the number of main network outputs and group network outputs.
	 * @return The number of outputs.
	 */
	int32 GetNumOutputs() const;

	/**
	 * Get the number of bones that are input to the network.
	 * @return The number of bones.
	 */
	int32 GetNumBones() const;

	/**
	 * Get the number of curves that are input to the network.
	 * @return The number of curves.
	 */
	int32 GetNumCurves() const;

	/**
	 * Get the number of morph targets per bone. This will only be valid if the GetMode() method returns the local model mode.
	 * For global mode networks, this will return 0.
	 * @return The number of morph targets per bone.
	 */
	int32 GetNumMorphsPerBone() const;

	/**
	 * Get the number of groups.
	 * A group is a collection of bones or curves that together generate a number of morph targets.
	 * Each group must have the same number of items.
	 * @result The number of groups.
	 */
	int32 GetNumGroups() const;

	/**
	 * Get the number of items per group.
	 * This indicates how many bones or curves each group has.
	 * Every group must have the same number of items.
	 * @return The number of items per group.
	 */
	int32 GetNumItemsPerGroup() const;

	/**
	 * Get the mode that the model was trained for, either global or local mode.
	 * @return The mode the model was trained for.
	 */
	ENeuralMorphMode GetMode() const;

	/**
	 * Get the means of each input, used for normalizing the input values.
	 * @return The array of mean values, one value for each network input.
	 */
	const TArrayView<const float> GetInputMeans() const;

	/**
	 * Get the standard deviations of each input, used for normalizing the input values.
	 * @return The array that contains the standard deviations, one value for each network input.
	 */
	const TArrayView<const float> GetInputStds() const;

	/** 
	 * Get the number of floats used to represent a single curve value.
	 * @return The number of float values per curve.
	 */
	int32 GetNumFloatsPerCurve() const;

	/**
	 * Get the main MLP.
	 * This is the MLP (or actually a Multi-MLP) that contains a small network for every bone or curve.
	 * Each small network will output a set of morph target weights.
	 * @return A pointer to the main MLP.
	 */
	UNeuralMorphMLP* GetMainMLP() const;

	/**
	 * Get the MLP used for group of bones and curves.
	 * This MLP (or actually a Multi-MLP) contains small networks for the bone or curve groups.
	 * Each group will generate a set of morph target weights.
	 * @return A pointer to the MLP used for the bone and curve groups.
	 */
	UNeuralMorphMLP* GetGroupMLP() const;

private:
	/**
	 * Load an MLP network from the currently already opened archive.
	 * @param Archive The archive to load from.
	 * @return Returns a pointer to the loaded MLP, or a nullptr in case of a failure.
	 */
	UNeuralMorphMLP* LoadMLP(FArchive& FileReader);

private:
	/** The MLP that acts as main network. */
	UPROPERTY()
	TObjectPtr<UNeuralMorphMLP> MainMLP;

	/** The MLP for the bone and curve groups, when in local mode. */
	UPROPERTY()
	TObjectPtr<UNeuralMorphMLP> GroupMLP;

	/** The means of the input values, used to normalize inputs. */
	UPROPERTY()
	TArray<float> InputMeans;

	/** The standard deviation of the input values, used to normalize inputs. */
	UPROPERTY()
	TArray<float> InputStd;

	/** The mode of the network, either local or global. */
	UPROPERTY()
	ENeuralMorphMode Mode = ENeuralMorphMode::Global;

	/** The number of morph targets per bone, if set Mode == Local, otherwise ignored. */
	UPROPERTY()
	int32 NumMorphsPerBone = 0;

	/** The number of bones that were input. */
	UPROPERTY()
	int32 NumBones = 0;

	/** The number of curves that were input. */
	UPROPERTY()
	int32 NumCurves = 0;

	/** The number of floats per curve. */
	UPROPERTY()
	int32 NumFloatsPerCurve = 1;

	/** The number of groups. This is a group of bones/curves that create their own morphs. */
	UPROPERTY()
	int32 NumGroups = 0;

	/** The number of items (bones/curves) per group. */
	UPROPERTY()
	int32 NumItemsPerGroup = 0;
};

/** 
 * An instance of a UNeuralMorphNetwork.
 * The instance holds its own input and output buffers and only reads from the network object it was instanced from.
 * This allows it to be multithreaded.
 */
UCLASS()
class NEURALMORPHMODEL_API UNeuralMorphNetworkInstance
	: public UObject
{
	friend class UNeuralMorphNetwork;

	GENERATED_BODY()

public:
	/**
	 * Get the network input buffer.
	 * @return The array view of floats that represents the main network inputs.
	 */
	TArrayView<float> GetInputs();
	TArrayView<const float> GetInputs() const;

	/**
	 * Get the inputs for the group MLP.
	 * @return The array view of floats that represent the group network inputs.
	 */
	TArrayView<float> GetGroupInputs();
	TArrayView<const float> GetGroupInputs() const;

	/**
	 * Get the network output buffer.
	 * @return The array view of floats that represents the network outputs.
	 */
	TArrayView<float> GetOutputs();
	TArrayView<const float> GetOutputs() const;

	/**
	 * Get the neural network this is an instance of.
	 * @return A reference to the neural network.
	 */
	const UNeuralMorphNetwork& GetNeuralNetwork() const;

	/**
	 * Run the neural network, performing inference.
	 * This will update the values in the output buffer that you can get with the GetOutputs method.
	 * This also assumes you have set all the right input values already.
	 */
	void Run();

private:
	struct FRunSettings
	{
		float* TempInputBuffer = nullptr;
		float* TempOutputBuffer = nullptr;
		const float* InputBuffer = nullptr;
		float* OutputBuffer = nullptr;
	};

	/**
 	 * Run inference using the global model mode, which is one fully connected MLP.
	 * @param RunSettings The settings used to run the model. This specifies a set of buffers.
	 */
	void RunGlobalModel(const FRunSettings& RunSettings);

	/**
 	 * Run a local mlp, which internally is acting like a multi-mlp (one mlp per bone or curve).
	 * @param MLP The MLP to run.
	 * @param RunSettings The settings used to run the model. This specifies a set of buffers.
	 */
	void RunLocalMLP(UNeuralMorphMLP& MLP, const FRunSettings& RunSettings);

	/**
 	 * Initialize this instance using a given network object.
	 * This is automatically called by the UNeuralMorphNetwork::CreateInstance() method.
	 * @param InNeuralNetwork The network that this is an instance of.
	 */
	void Init(UNeuralMorphNetwork* InNeuralNetwork);

private:
	/** The input values for the main MLP. */
	TArray<float> Inputs;

	/** The input values for the group MLP. */
	TArray<float> GroupInputs;

	/** The output values. */
	TArray<float> Outputs;

	/** A pre-allocated temp buffer for inputs. */
	TArray<float> TempInputArray;

	/** A pre-allocated temp buffer for outputs. */
	TArray<float> TempOutputArray;

	/** The neural network this is an instance of. */
	UPROPERTY(Transient)
	TObjectPtr<UNeuralMorphNetwork> Network;
};
