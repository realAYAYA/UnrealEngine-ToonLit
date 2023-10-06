// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"
#include "UObject/Object.h"
#include "NearestNeighborOptimizedNetwork.generated.h"

class UNearestNeighborOptimizedNetworkInstance;

#define NEARESTNEIGHBORMODEL_USE_ISPC INTEL_ISPC
#if !defined(NEARESTNEIGHBORMODEL_USE_ISPC)
	#define NEARESTNEIGHBORMODEL_USE_ISPC 0
#endif

USTRUCT()
struct FNearestNeighborNetworkParameter
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<float> Values;

	UPROPERTY()
	TArray<int32> Shape;
};

UENUM(BlueprintType)
enum class ENearestNeighborNetworkLayerType : uint8
{
	None,
	Gemm_Prelu,
	Gemm,
};


/** A general network layer that contains a list of parameters. The Run() method should be implemented by child classes */ 
UCLASS()
class UNearestNeighborNetworkLayer
	: public UObject
{
	GENERATED_BODY()

public:
	/** The weight matrix number of inputs (rows). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MLDeformer")
	int32 NumInputs = 0;

	/** The weight matrix number of outputs (columns). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MLDeformer")
	int32 NumOutputs = 0;

	// /** The parameters of the layer */
	UPROPERTY()
	TArray<FNearestNeighborNetworkParameter> Parameters;

	UFUNCTION(BlueprintCallable, Category = "MLDeformer")
	void AddParameter(const TArray<float>& Values, const TArray<int32>& Shape);

	virtual void Run(const float* RESTRICT InputBuffer, float* RESTRICT OutputBuffer) const;
};

/** Gemm: GEneral Matrix Multiplication: Wx + b */
UCLASS()
class UNearestNeighborNetworkLayer_Gemm_Prelu
	: public UNearestNeighborNetworkLayer
{
	GENERATED_BODY()

public:
	virtual void Run(const float* RESTRICT InputBuffer, float* RESTRICT OutputBuffer) const override;
};

UCLASS()
class UNearestNeighborNetworkLayer_Gemm
	: public UNearestNeighborNetworkLayer
{
	GENERATED_BODY()

public:
	virtual void Run(const float* RESTRICT InputBuffer, float* RESTRICT OutputBuffer) const override;
};

/**
 * The specialized neural network for the MLDeformerModel.
 * This class is used to do inference at runtime at a higher performance than using UNeuralNetwork.
 */
UCLASS()
class UNearestNeighborOptimizedNetwork
	: public UObject
{
	GENERATED_BODY()

public:
	/** Clear the network, getting rid of all layers. */
	UFUNCTION(BlueprintCallable, Category = "MLDeformer")
	virtual void Empty();

	/** 
	 * Check if the network is empty or not.
	 * If it is empty, it means it hasn't been loaded, and cannot do anything.
	 * Empty means it has no inputs, outputs or weights.
	 * @return Returns true if empty, otherwise false is returned.
	 */
	virtual bool IsEmpty() const;

	/**
	 * Load the network from a file on disk.
	 * When loading fails, the network will be emptied.
	 * @param Filename The file path to load from.
	 * @return Returns true when successfully loaded this model, otherwise false is returned.
	 */
	virtual bool Load(const FString& Filename);

	/**
	 * Create an instance of this neural network.
	 * @return A pointer to the neural network instance.
	 */
	virtual UNearestNeighborOptimizedNetworkInstance* CreateInstance();

	/**
	 * Get the number of inputs, which is the number of floats the network takes as input.
	 * @result The number of input floats to the network.
	 */
	virtual int32 GetNumInputs() const;

	/**
	 * Get the number of outputs, which is the number of floats the network will output.
	 * @return The number of floats that the network outputs.
	 */
	virtual int32 GetNumOutputs() const;


	/**
	 * Get the number of network layers.
	 * @return The number of network layer.
	 */
	const int32 GetNumLayers() const;

	/**
	 * Get a given network layer.
	 * @return A pointer to the layer, which will contain the parameters.
	 */
	UNearestNeighborNetworkLayer* GetLayer(int32 Index) const;


	/**
	 * Add a network layer.
	 * @return A pointer to the layer, which will contain the parameters.
	 */
	UFUNCTION(BlueprintCallable, Category = "MLDeformer")
	UNearestNeighborNetworkLayer* AddLayer(const int32 LayerType);

protected:
	/** The network layers */
	UPROPERTY()
	TArray<TObjectPtr<UNearestNeighborNetworkLayer>> Layers;
};

/** 
 * An instance of a UNearestNeighborOptimizedNetwork.
 * The instance holds its own input and output buffers and only reads from the network object it was instanced from.
 * This allows it to be multithreaded.
 */
UCLASS()
class UNearestNeighborOptimizedNetworkInstance
	: public UObject
{
	friend class UNearestNeighborOptimizedNetwork;

	GENERATED_BODY()

public:
	/**
	 * Get the network input buffer.
	 * @return The array view of floats that represents the network inputs.
	 */
	TArrayView<float> GetInputs();
	TArrayView<const float> GetInputs() const;

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
	const UNearestNeighborOptimizedNetwork* GetNeuralNetwork() const;

	/**
	 * Run the neural network, performing inference.
	 * This will update the values in the output buffer that you can get with the GetOutputs method.
	 * This also assumes you have set all the right input values already.
	 */
	virtual void Run();

protected:
	struct FRunSettings
	{
		float* TempInputBuffer;
		float* TempOutputBuffer;
		const float* InputBuffer;
		float* OutputBuffer;
	};

	/**
 	 * Initialize this instance using a given network object.
	 * This is automatically called by the UNearestNeighborOptimizedNetwork::CreateInstance() method.
	 * @param InNeuralNetwork The network that this is an instance of.
	 */
	void Init(UNearestNeighborOptimizedNetwork* InNeuralNetwork);

protected:
	/** The input values. */
	TArray<float> Inputs;

	/** The output values. */
	TArray<float> Outputs;

	/** A pre-allocated temp buffer for inputs. */
	TArray<float> TempInputArray;

	/** A pre-allocated temp buffer for outputs. */
	TArray<float> TempOutputArray;

	/** The neural network this is an instance of. */
	UPROPERTY(Transient)
	TObjectPtr<UNearestNeighborOptimizedNetwork> Network;
};
