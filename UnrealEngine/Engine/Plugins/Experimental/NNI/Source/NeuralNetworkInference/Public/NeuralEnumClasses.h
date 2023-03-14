// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PixelFormat.h"
#include <type_traits>
#include "NeuralEnumClasses.generated.h"

/**
 * It defines the data type (e.g., float, double, int32).
 */
UENUM()
enum class ENeuralDataType : uint8
{
	/** 32-bit floating number. */
	Float,
	/** 64-bit floating number. */
	//Double,
	/** 8-bit signed integer. */
	//Int8,
	/** 16-bit signed integer. */
	//Int16,
	/** 32-bit signed integer. */
	Int32,
	/** 64-bit signed integer. */
	Int64,
	/** 8-bit unsigned integer. */
	//UInt8,
	/** 32-bit unsigned integer. */
	UInt32,
	/** 64-bit unsigned integer. */
	UInt64,
	/** To be used in special cases, e.g., if the data type is unknown yet. */
	None
};

/**
 * It defines in which device (CPU, GPU) the desired operation (e.g., the neural network inference) is run.
 */
UENUM()
enum class ENeuralDeviceType : uint8
{
	/** The operation will occur on the CPU. */
	CPU,
	/** The operation will occur on the GPU. */
	GPU
};

/**
 * Whether the operation (e.g., UNeuralNetwork::Run()) will run in the calling thread (Synchronous) or in a background thread (Asynchronous).
 */
UENUM()
enum class ENeuralSynchronousMode : uint8
{
	/**
	 * Safer and simpler to use.
	 * The operation will run in the calling thread thus blocking that thread until completed.
	 */
	Synchronous,
	/**
	 * More complex but potentially more efficient.
	 * The operation will initialize a compute request on a background thread and return before its completion, not blocking the calling thread that
	 * called it.
	 * The user should register to a delegate which will notify them when the operation has finished asynchronously
	 * (e.g., UNeuralNetwork::GetOnAsyncRunCompletedDelegate() for asynchronous UNeuralNetwork::Run()).
	 *
	 * Very important: It takes ~1 millisecond to start the background thread. If your operation (e.g., your network inference time) runs
	 * synchronously faster than 1-2 milliseconds, using asynchronous is not recommended because it'd slow down both the main and background threads.
	 */
	Asynchronous
};

/**
 * The type of the neural tensor. E.g., a Weight tensor will be read-only and never modified, an Input tensor will be modified by the user, an
 * IntermediateNotInitialized tensor will change on each frame, etc.
 *
 * Although conceptually this could apply to both the CPU and GPU versions, in practice only the GPU performance is affected by this setting so far.
 */
UENUM()
enum class ENeuralTensorType : uint8
{
	/**
	 * Safe and generic tensor that works in every situation (e.g., ReadWrite, not volatile). However, it might not be the most efficient one for
	 * most cases.
	 */
	Generic,
	/**
	 * Input tensor of a neural network, usually copied from the CPU and usually ReadOnly (but might be modified by in-place operators like ReLU).
	 */
	Input,
	/**
	 * Intermediate tensor of a neural network (output of at least a layer and input of at least some other layer). Not copied from CPU, ReadWrite,
	 * and transient.
	 */
	IntermediateNotInitialized,
	/**
	 * Intermediate tensor that is initialized with CPU data (e.g., XWithZeros in FConvTranpose). Copied from CPU.
	 */
	IntermediateInitialized,
	/**
	 * Output tensor of a neural network. Not copied from CPU and ReadWrite.
	 */
	Output,
	/**
	 * Weights of a particular operator/layer. Copied from CPU, ReadOnly, and initialized from CPU memory.
	 */
	Weight
};

/**
 * After an asynchronous operation has finished, whether the callback functions tied to the delegate will be called from the game/main thread (highly
 * recommended) or from any thread (not fully Unreal safe).
 * This enum class is only useful for the case of ENeuralSynchronousMode::Asynchronous.
 */
UENUM()
enum class ENeuralThreadMode : uint8
{
	/**
	 * Highly recommended and default value. The callback functions tied to the delegate will be called from the game/main thread.
	 */
	GameThread,
	/**
	 * Not recommended, use at your own risk (potentially more efficient than GameThread but not UE-safe in all cases).
	 * The callback functions tied to the delegate will be called from the same thread at which the operation was asynchronously computed, thus this
	 * could happen from any thread.
	 *
	 * Main issue: U-classes (e.g., UNeuralNetwork) should not be used from non-game threads because it is not safe and could lead to issues/crashes
	 * if not properly handled by the user.
	 * E.g., the GC does not work from non-game threads so it might crash if the editor/UE is closed while accessing UNeuralNetwork information from
	 * the callback function.
	 *
	 * Thus "AnyThread" is only safe if you have guarantees that your case is fully U-class safe. E.g., the program will not be terminated while
	 * calling the callback function.
	 */
	AnyThread
};



/**
 * For a general overview of NeuralNetworkInference (NNI), including documentation and code samples, @see UNeuralNetwork, the main class of NNI.
 *
 * Auxiliary class consisting of static and auxiliary functions for ENeuralDataType.
 */
class NEURALNETWORKINFERENCE_API FNeuralDataTypeUtils
{
public:
	/**
	 * It returns the byte size of the input ENeuralDataType (e.g., 4 for ENeuralDataType::Float or ENeuralDataType::Int32).
	 */
	static int64 GetByteSize(const ENeuralDataType InDataType);

	/**
	 * It returns the data type from the type T (e.g., ENeuralDataType::Float for GetDataType<float>()).
	 */
	template <typename T>
	static ENeuralDataType GetDataType();

	/**
	 * It returns the pixel format of the input ENeuralDataType (e.g., EPixelFormat::PF_R32_FLOAT for ENeuralDataType::Float or "PF_R32_SINT" for Int32).
	 */
	static EPixelFormat GetPixelFormat(const ENeuralDataType InDataType);

	/**
	 * It returns the FString name of the input ENeuralDataType (e.g., "Float" for ENeuralDataType::Float).
	 */
	static FString ToString(const ENeuralDataType InDataType);
};



/* FNeuralDataTypeUtils templated functions
 *****************************************************************************/

template <typename T>
ENeuralDataType FNeuralDataTypeUtils::GetDataType()
{
	if (std::is_same<T, float>::value)
	{
		return ENeuralDataType::Float;
	}
	else if (std::is_same<T, int32>::value)
	{
		return ENeuralDataType::Int32;
	}
	else if (std::is_same<T, int64>::value)
	{
		return ENeuralDataType::Int64;
	}
	else if (std::is_same<T, uint32>::value)
	{
		return ENeuralDataType::UInt32;
	}
	else if (std::is_same<T, uint64>::value)
	{
		return ENeuralDataType::UInt64;
	}
	return ENeuralDataType::None;
}
