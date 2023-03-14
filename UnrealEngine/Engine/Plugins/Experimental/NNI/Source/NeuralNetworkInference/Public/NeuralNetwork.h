// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NeuralEnumClasses.h"
#include "NeuralStatistics.h"
#include "NeuralTensor.h"
#include "NeuralNetwork.generated.h"

/**
 * NeuralNetworkInference (NNI) is Unreal Engine's framework for running deep learning and neural network inference. It is focused on:
 * - Efficiency: Underlying state-of-the-art accelerators (DirectML, AVX, CoreML, etc).
 * - Ease-of-use: Simple but powerful API.
 * - Completeness: All the functionality of any state-of-the-art deep learning framework.
 *
 * UNeuralNetwork is the key class of NNI, and the main one users should interact with. It represents the deep neural model itself. It is capable of
 * loading and running inference (i.e., a forward pass) on any ONNX (Open Neural Network eXchange) model. ONNX is the industry standard for ML
 * interoperability, and all major frameworks (PyTorch, TensorFlow, MXNet, Caffe2, etc.) provide converters to ONNX.
 *
 * The following code snippets show the UNeuralNetwork basics (reading a ONNX model and running inference on it). For more detailed examples, see
 * {UE5}/Samples/MachineLearning/NNI.
 *
 * 1a Creating a new UNeuralNetwork and loading a network from an ONNX file
 *		// Create the UNeuralNetwork object
 *		UNeuralNetwork* Network = NewObject<UNeuralNetwork>((UObject*)GetTransientPackage(), UNeuralNetwork::StaticClass());
 *		// Load the ONNX model and set the device (CPU/GPU)
 *		const FString ONNXModelFilePath = TEXT("SOME_PARENT_FOLDER/SOME_ONNX_FILE_NAME.onnx");
 *		if (Network->Load(ONNXModelFilePath))
 *		{
 *			// Pick between option a or b
 *			// Option a) Set to GPU
 *			if (Network->IsGPUSupported())
 *				Network->SetDeviceType(ENeuralDeviceType::GPU);
 *			// Option b) Set to CPU
 *			Network->SetDeviceType(ENeuralDeviceType::CPU);
 *		}
 *		// Check that the network was successfully loaded
 *		else
 *		{
 *			UE_LOG(LogTemp, Warning, TEXT("UNeuralNetwork could not loaded from %s."), *ONNXModelFilePath);
 *		}
 *
 * 1b Loading an existing UNeuralNetwork from its UAsset
 *		// Load the UNeuralNetwork object from a UNeuralNetwork UAsset
 *		const FString NetworkUAssetFilePath = TEXT("'/Game/Models/ExampleNetwork/ExampleNetwork.ExampleNetwork'");
 *		UNeuralNetwork* Network = LoadObject<UNeuralNetwork>((UObject*)GetTransientPackage(), *NetworkUAssetFilePath);
 *		// Check that the network was successfully loaded
 *		ensureMsgf(Network->IsLoaded(), TEXT("UNeuralNetwork could not loaded from %s."), *NetworkUAssetFilePath);
 *		// Optionally set to CPU/GPU mode. This step is optional, if not called, it will use the device type that was saved on the loaded UAsset
 *		// Network->SetDeviceType(ENeuralDeviceType::CPU);
 *
 * 2a Running inference
 *		// Fill input neural tensor
 *		TArray<float> InArray; // Assumed initialized with data and that InArray.Num() == Network->Num()
 *		Network->SetInputFromArrayCopy(InArray); // Equivalent: Network->SetInputFromVoidPointerCopy(InArray.GetData());
 *		UE_LOG(LogTemp, Display, TEXT("Input tensor: %s."), *Network->GetInputTensor().ToString());
 *		// Run UNeuralNetwork
 *		Network->Run();
 *		// Read and print OutputTensor
 *		const FNeuralTensor& OutputTensor = Network->GetOutputTensor();
 *		UE_LOG(LogTemp, Display, TEXT("Output tensor: %s."), *OutputTensor.ToString());
 *
 * 2b Running inference more efficiently - Filling the input tensor without a TArray-to-FNeuralTensor copy
 *		// Obtain input tensor pointer
 *		float* InputDataPointer = (float*)Network->GetInputDataPointerMutable();
 *		// Fill InputDataPointer
 *		for (int64 Index = 0; Index < Network->GetInputTensor().Num(); ++Index)
 *			InputDataPointer[Index] = ...; // Assumed some preprocessing or otherwise simply use Memcpy
 *
 * 3 Networks with multiple input/output tensors
 *		- Multiple inputs: Add InTensorIndex to SetInputFromArrayCopy, SetInputFromVoidPointerCopy, GetInputTensor or GetInputDataPointerMutable
 *		  in the examples above:
 *			Network->SetInputFromArrayCopy(InputArray0, 0); // Equivalent: Network->SetInputFromVoidPointerCopy(InputArray0.GetData(), 0);
 *			Network->SetInputFromArrayCopy(InputArray1, 1); // Equivalent: Network->SetInputFromVoidPointerCopy(InputArray1.GetData(), 1);
 *		- Multiple outputs: Add InTensorIndex to GetOutputTensor(InTensorIndex) in the examples above or use GetOutputTensors() instead.
 *			const FNeuralTensor& OutputTensor0 = Network->GetOutputTensor(0);
 *			const FNeuralTensor& OutputTensor1 = Network->GetOutputTensor(1);
 */
UCLASS(BlueprintType)
class NEURALNETWORKINFERENCE_API UNeuralNetwork : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Default constructor that initializes the internal class variables.
	 */
	UNeuralNetwork();

	/**
	 * Destructor defined in case this class is ever inherited.
	 */
	virtual ~UNeuralNetwork() = default;

	/**
	 * Load() + SetInputFromArrayCopy() + Run() is the simplest way to load an ONNX file, set the input tensor(s), and run inference on it. All other
	 * functions in UNeuralNetwork provide additional functionality (set the device to CPU/GPU, more complicated but faster ways to set the input,
	 * support for asynchronous run, avoid copying the memory CPU-to-GPU or GPU-to-CPU, etc.).
	 *
	 * It loads the desired network graph definition and weights from an input ONNX file. This file can be passed either a file path or as a memory
	 * buffer.
	 * @param InModelFilePath Input ONNX file path to be read. It can either be a full path or a relative path with respect to the Engine or Game
	 * project.
	 * @param InModelReadFromFileInBytes TArray buffer filled with the contents of the ONNX file. This TArray<uint8> buffer will be moved for
	 * performance reasons. @see FFileHelper::LoadFileToArray for an example of how to read a file into a TArray<uint8>.
	 * @return Whether the network was successfully loaded. Equivalent to IsLoaded(). @see IsLoaded() for more details.
	 *
	 * @see Load(), SetInputFromArrayCopy(), Run().
	 */
	bool Load(const FString& InModelFilePath);
	bool Load(TArray<uint8>& InModelReadFromFileInBytes);

	/**
	 * It returns whether a network is currently loaded. It is equivalent to the output of Load().
	 * @see Load() for more details.
	 */
	bool IsLoaded() const;


	/**
	 *  Save ONNX network to disk. 
	 * @return whether the network was successfully saved.
	 */
	bool Save(const FString& OutModelFilePath) const;

	/**
	 * Getter and setter functions for DeviceType, InputDeviceType, and OutputDeviceType:
	 * - GetDeviceType() returns DeviceType.
	 * - GetInputDeviceType() returns InputDeviceType.
	 * - GetOutputDeviceType() returns OutputDeviceType.
	 * - SetDeviceType() sets DeviceType, InputDeviceType, and OutputDeviceType. If you are setting it to GPU, check IsGPUSupported() first.
	 * @see DeviceType, InputDeviceType, OutputDeviceType, and IsGPUSupported() for more details.
	 */
	ENeuralDeviceType GetDeviceType() const;
	ENeuralDeviceType GetInputDeviceType() const;
	ENeuralDeviceType GetOutputDeviceType() const;
	void SetDeviceType(const ENeuralDeviceType InDeviceType, const ENeuralDeviceType InInputDeviceType = ENeuralDeviceType::CPU,
		const ENeuralDeviceType InOutputDeviceType = ENeuralDeviceType::CPU);

	/**
	 * Whether GPU execution (i.e., SetDeviceType(ENeuralDeviceType::GPU)) is supported for this platform:
	 * - On Windows:
	 *     - True if DX12 is enabled.
	 *     - False if DX12 is disabled. The user will need to enable DX12 on Unreal Engine to be able to run GPU on Windows or switch to CPU mode.
	 * - Non-Windows platforms: False for now (GPU not supported on non-Windows devices yet).
	 * Deprecated: I will also return true if the back end is UEOnly, but this is an unsupported and deprecated back end that should not be used.
	 * Code sample:
	 *		if (Network->IsGPUSupported())
	 *			Network->SetDeviceType(ENeuralDeviceType::GPU);
	 * @see DeviceType and SetDeviceType() for more details.
	 */
	bool IsGPUSupported() const;

	/**
	 * Getter and setter functions for SynchronousMode:
	 * - GetDeviceType() returns SynchronousMode.
	 * - SetDeviceType() sets SynchronousMode.
	 * @see SynchronousMode and GetOnAsyncRunCompletedDelegate() for more details.
	 */
	ENeuralSynchronousMode GetSynchronousMode() const;
	void SetSynchronousMode(const ENeuralSynchronousMode InSynchronousMode);

	/**
	 * FOnAsyncRunCompleted is the delegate that gets triggered when an asynchronous inference has been completed (i.e., Run() was called with
	 * NeuralSynchronousMode == ENeuralSynchronousMode::Asynchronous).
	 *
	 * GetOnAsyncRunCompletedDelegate() returns a reference of this class' FOnAsyncRunCompleted delegate, so the UNeuralNetwork's client can
	 * subscribe to its callback function.
	 *
	 * Code example enabling it:
	 * 	   AsynchronousNetwork->GetOnAsyncRunCompletedDelegate().BindUObject(this, &SomeClass::SomeFunctionToRunWhenAsyncRunFinished);
	 *     AsynchronousNetwork->SetSynchronousMode(ENeuralSynchronousMode::Asynchronous);
	 * Code example disabling it:
	 *     AsynchronousNetwork->GetOnAsyncRunCompletedDelegate().Unbind();
	 *     AsynchronousNetwork->SetSynchronousMode(ENeuralSynchronousMode::Synchronous);
	 *
	 * @see SynchronousMode(), GetSynchronousMode(), SetSynchronousMode(), GetThreadModeDelegateForAsyncRunCompleted(), SetThreadModeDelegateForAsyncRunCompleted() for more details.
	 */
	DECLARE_DELEGATE(FOnAsyncRunCompleted);
	FOnAsyncRunCompleted& GetOnAsyncRunCompletedDelegate();

	/**
	 * Getter and setter functions for ThreadModeDelegateForAsyncRunCompleted:
	 * - GetThreadModeDelegateForAsyncRunCompleted() returns ThreadModeDelegateForAsyncRunCompleted.
	 * - SetThreadModeDelegateForAsyncRunCompleted() sets ThreadModeDelegateForAsyncRunCompleted.
	 * @see ThreadModeDelegateForAsyncRunCompleted and GetOnAsyncRunCompletedDelegate() for more details.
	 */
	ENeuralThreadMode GetThreadModeDelegateForAsyncRunCompleted() const;
	void SetThreadModeDelegateForAsyncRunCompleted(const ENeuralThreadMode InThreadModeDelegateForAsyncRunCompleted);

	/**
	 * GetInputTensorNumber() and GetOutputTensorNumber() return the number of input or output tensors of this network, respectively.
	 * @see GetInputTensor(), SetInputFromArrayCopy(), SetInputFromVoidPointerCopy(), and GetInputDataPointerMutable() for other
	 * input-tensor-related functions.
	 * @see GetOutputTensor() for other output-tensor-related functions.
	 */
	int64 GetInputTensorNumber() const;
	int64 GetOutputTensorNumber() const;

	/**
	 * GetInputTensor() and GetOutputTensor() return a const (read-only) reference of the input or output FNeuralTensor(s) of the network,
	 * respectively. They allow querying tensor properties, such as size, GetInputTensor().GetSize(), or dimensions, GetInputTensor().Num().
	 * @param InTensorIndex The input/output index to query. By default (0), it queries the first input/output tensor. @see GetInputTensorNumber()
	 * and GetOutputTensorNumber() for more details.
	 */
	const FNeuralTensor& GetInputTensor(const int32 InTensorIndex = 0) const;
	const FNeuralTensor& GetOutputTensor(const int32 InTensorIndex = 0) const;

	const FNeuralTensor& GetInputTensorForContext(const int32 InContextHandle, const int32 InTensorIndex = 0) const;
	const FNeuralTensor& GetOutputTensorForContext(const int32 InContextHandle, const int32 InTensorIndex = 0) const;

	/**
	 * SetInputFromArrayCopy(), SetInputFromVoidPointerCopy(), and GetInputDataPointerMutable() are the only functions that allow modifying the
	 * network input tensor(s) values:
	 * - SetInputFromArrayCopy() and SetInputFromVoidPointerCopy() are very easy to use but less efficient (they require an intermediate and
	 *   auxiliary TArray or pointer of data). These functions copy the given InArray or data pointer into the desired input FNeuralTensor.
	 * - GetInputDataPointerMutable() is potentially more efficient because it avoids creating and copying from an intermediate TArray. This function
	 *   returns a mutable void* pointer of the desired input FNeuralTensor that can be filled by the user on-the-fly.
	 */
	void SetInputFromArrayCopy(const TArray<float>& InArray, const int32 InTensorIndex = 0);
	void SetInputFromVoidPointerCopy(const void* const InVoidPtr, const int32 InTensorIndex = 0);
	void* GetInputDataPointerMutable(const int32 InTensorIndex = 0);

	void* GetInputDataPointerMutableForContext(const int32 InContextHandle, const int32 InTensorIndex = 0);

	/**
	 * Non-computationally-efficient functions meant to be used only for debugging purposes, but should never be used on highly performant systems:
	 * - InputTensorsToCPU copies the CPU memory of the desired input tensor(s) to GPU (to debug InputDeviceType == ENeuralDeviceType::GPU).
	 * - OutputTensorsToCPU copies the GPU memory of the desired output tensor(s) back to CPU (to debug OutputDeviceType == ENeuralDeviceType::GPU).
	 * @param InTensorIndexes If empty (default value), it will apply to all output tensors.
	 */
	void InputTensorsToGPU(const TArray<int32>& InTensorIndexes = TArray<int32>());
	void OutputTensorsToCPU(const TArray<int32>& InTensorIndexes = TArray<int32>());

	/**
	 * Create an inference context. An inference context contains resources to run inference with the version of Run() that takes a context handle.
	 * Each independent instance of the NeuralNetwork requires it's own inference context.
	 * Returns a handle to the context. -1 is an invalid handle.
	 */ 
	int32 CreateInferenceContext();

	/**
	 * Destroy an inference context that was created with CreateInferenceContext()
	 */
	void DestroyInferenceContext(int32 ContextHandle);

	/**
	 * Run() executes the forward pass of the current UNeuralNetwork given an inference context created with CreateInferenceContext().
	 * This version assumes CPU inference.
	 */
	void Run(int32 ContextHandle);

	/**
	 * Run() executes the forward pass of the current UNeuralNetwork given an inference context created with CreateInferenceContext().
	 * This version assumes GPU inference and should be called on the render thread.
	 */
	void Run(class FRDGBuilder& GraphBuilder, int32 ContextHandle);

	/**
	 * Load() + SetInputFromArrayCopy() + Run() is the simplest way to load an ONNX file, set the input tensor(s), and run inference on it. All other
	 * functions in UNeuralNetwork provide additional functionality (set the device to CPU/GPU, more complicated but faster ways to set the input,
	 * support for asynchronous run, avoid copying the memory CPU-to-GPU or GPU-to-CPU, etc.).
	 *
	 * Run() executes the forward pass of the current UNeuralNetwork given the current input FNeuralTensor(s), which were previously filled
	 * with SetInputFromArrayCopy(), SetInputFromVoidPointerCopy(), or GetInputDataPointerMutable().
	 * Its output results can be retrieved with GetOutputTensor().
	 *
	 * If Run() is called asynchronously, the user is responsible of not calling SetInputFromArrayCopy/SetInputFromVoidPointerCopy until Run() is
	 * completed and its delegate (OnAsyncRunCompletedDelegate) called. Otherwise, the wrong results might be returned.
	 *
	 * @see Load(), SetInputFromArrayCopy(), Run().
	 */
	void Run();

	/**
	 * Statistics-related functions:
	 * - GetLastRunTimeMSec will provide the last inference time measured milliseconds.
	 * - GetRunStatistics returns a FNeuralStatistics with the time statistics in milliseconds (NumberSamples, Average, StdDev, Min, Max).
	 * - GetInputMemoryTransferStats returns a FNeuralStatistics with the input memory transfer statistics in milliseconds.
	 * @see FNeuralStatistics for more details.
	 */
	float GetLastRunTimeMSec() const;
	FNeuralStatistics GetRunStatistics() const;
	FNeuralStatistics GetInputMemoryTransferStats() const;
	void ResetStats();

protected:
	/**
	 * The neural device type of the network. It defines whether the network will use CPU or GPU acceleration hardware during inference (on Run).
	 * If SetDeviceType() is never called, the default device (EDeviceType::GPU) will be used.
	 * @see ENeuralDeviceType, InputDeviceType, OutputDeviceType for more details.
	 */
	UPROPERTY(EditAnywhere, Category = "Neural Network Inference")
	ENeuralDeviceType DeviceType;

	/**
	 * It defines whether Run() will expect the input data in CPU memory (Run will upload the memory to the GPU) or GPU memory (no upload needed).
	 * If DeviceType == CPU, InputDeviceType and OutputDeviceType values are ignored and assumed to be set to CPU.
	 * @see ENeuralDeviceType, DeviceType, OutputDeviceType for more details.
	 */
	UPROPERTY(EditAnywhere, Category = "Neural Network Inference")
	ENeuralDeviceType InputDeviceType;

	/**
	 * It defines whether Run() will return the output data in CPU memory (Run will download the memory to the CPU) or GPU (no download needed).
	 * If DeviceType == CPU, InputDeviceType and OutputDeviceType values are ignored and assumed to be set to CPU.
	 * @see ENeuralDeviceType, DeviceType, InputDeviceType for more details.
	 */
	UPROPERTY(EditAnywhere, Category = "Neural Network Inference")
	ENeuralDeviceType OutputDeviceType;

	/**
	 * It defines whether UNeuralNetwork::Run() will block the thread until completed (Synchronous), or whether it will run on a background thread,
	 * not blocking the calling thread (Asynchronous).
	 */
	UPROPERTY(Transient, EditAnywhere, Category = "Neural Network Inference")
	ENeuralSynchronousMode SynchronousMode;

	/**
	 * If SynchronousMode is Asynchronous, this variable will define whether the callback delegate is called from the game thread (highly
	 * recommended) or from any available thread (not fully thread safe).
	 * - If this variable is set to ENeuralThreadMode::GameThread, the FOnAsyncRunCompleted delegate will be triggered from the main thread only.
	 * - If this variable is set to ENeuralThreadMode::AnyThread, the FOnAsyncRunCompleted delegate could be triggered from any thread.
	 * @see SynchronousMode for more details.
	 */
	UPROPERTY(Transient, EditAnywhere, Category = "Neural Network Inference")
	ENeuralThreadMode ThreadModeDelegateForAsyncRunCompleted;

	/**
	 * Original model file path from which this UNeuralNetwork was loaded from.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	FString ModelFullFilePath;

private:
	/**
	 * Whether this UNeuralNetwork instance has loaded a valid network model already, i.e., whether Load() was called and returned true.
	 */
	UPROPERTY(Transient, VisibleAnywhere, Category = "Neural Network Inference")
	bool bIsLoaded;

	/**
	 * A buffer of memory representing the ONNX model. It is equivalent to the serialization of the ONNX file.
	 * During Load(InModelFilePath), the ONNX file is read and stored as this buffer of raw bytes (TArray<uint8>).
	 */
	UPROPERTY()
	TArray<uint8> ModelReadFromFileInBytes;

	/**
	 * Whether some of the FNeuralTensor of InputTensor have flexible/variable dimensions. E.g., this is useful for variable batch size.
	 */
	UPROPERTY(Transient, VisibleAnywhere, Category = "Neural Network Inference")
	TArray<bool> AreInputTensorSizesVariable;

	/**
	 * Mutex to avoid issues or crashes due to the asynchronous Run() being run at the same time than any other non-const function.
	 * @see UNeuralNetwork::Run() implementation for more details.
	 */
	FCriticalSection ResoucesCriticalSection;

	/**
	 * @see FOnAsyncRunCompleted and GetOnAsyncRunCompletedDelegate to understand OnAsyncRunCompletedDelegate.
	 */
	FOnAsyncRunCompleted OnAsyncRunCompletedDelegate;

	/**
	 * Statistics-related members used for GetLastRunTimeMSec(), GetRunStatistics(), GetInputMemoryTransferStats(), ResetStats().
	 */
	FNeuralStatisticsEstimator RunStatisticsEstimator;
	FNeuralStatisticsEstimator InputTransferStatisticsEstimator;

	/**
	 * Struct pointers containing the UE-and-ORT-based (ImplBackEndUEAndORT) and only-UE-based (ImplBackEndUEOnly) back end implementations.
	 * PIMPL idiom to minimize memory when not using each back end (only 1 is used at the time) and to hide 3rd party dependencies.
	 * http://www.cppsamples.com/common-tasks/pimpl.html
	 */
	struct FImplBackEndUEAndORT;
	TSharedPtr<FImplBackEndUEAndORT> ImplBackEndUEAndORT;
	struct FImplBackEndUEOnly;
	TSharedPtr<FImplBackEndUEOnly> ImplBackEndUEOnly;

	/**
	 * It loads the existing network graph definition and weights from the members of this UNeuralNetwork instance.
	 * @return Whether the network was successfully loaded.
	 */
	bool Load();

	/**
	 * GetInputTensorMutable() and GetOutputTensorMutable() are the private and mutable version of GetInputTensor() and GetOutputTensor(),
	 * respectively.
	 * Most FNeuralTensor properties are considered constant and fixed after calling UNeuralNetwork::Load(), other than the actual data values
	 * (i.e., UnderlyingUInt8ArrayData) and some GPU properties. Thus, use these 2 functions with extreme precaution and be sure not to modify any
	 * other FNeuralNetwork properties.
	 * @see GetInputTensor() and GetOutputTensor() for more details.
	 */
	FNeuralTensor& GetInputTensorMutable(const int32 InTensorIndex = 0);
	FNeuralTensor& GetOutputTensorMutable(const int32 InTensorIndex = 0);

public: // It should really say "private-for-user, public-for-auxiliary-NNI-classes"
	/**
	 * Internal function not needed by the user.
	 * Used to create custom networks without an ONNX file for QA testing in FOperatorTester::TestOperator().
	 */
	bool Load(TArray<FNeuralTensor>& InTensors, const TArray<FNeuralTensor*>& InInputTensors, const TArray<FNeuralTensor*>& InOutputTensors,
		const TArray<TSharedPtr<class FNeuralOperator>>& InOperators);

	/**
	 * Internal enum class that should not be used by the user.
	 * It enumerates the different types of back ends. Use Auto, which will find and use the optimal back end for each platform.
	 */
	enum class ENeuralBackEnd : uint8
	{
		/**
		 * Highly optimized UnrealEngine-and-ONNXRuntime-based back end, ideal for those platforms that support it. WITH_UE_AND_ORT_SUPPORT is the
		 * C++ define macro that checks whether UEAndORT support exists for the current platform.
		 */
		UEAndORT,
		/**
		 * Slower than the UEAndORT back end maintained for now for historical reasons. There is no benefit in using this back end.
		 */
		UEOnly,
		/** Recommended value. It will use the efficient UEAndORT if supported by the platform, and try to fall back to UEOnly otherwise. */
		Auto
	};

	/**
	 * Internal functions that should not be used by the user.
	 * Getter and setter functions for BackEnd.
	 * GetBackEnd()/GetBackEndForCurrentPlatform():
	 * - If BackEnd == Auto, GetBackEnd() will return Auto and GetBackEndForCurrentPlatform() will return the actual BackEnd being used for the
	 *   current platform (UEAndORT or UEOnly).
	 * - If BackEnd != Auto, GetBackEnd() and GetBackEndForCurrentPlatform() will both return the same value (UEAndORT or UEOnly).
	 * SetBackEnd() will modify both BackEnd and BackEndForCurrentPlatform and return IsLoaded().
	 * SetBackEnd() is NOT THREAD SAFE, make sure that other non-const functions such as Run() are not running when SetBackEnd is called.
	 * @see ENeuralBackEnd for more details.
	 */
	UE_DEPRECATED(5.0, "Do not use, ENeuralBackEnd will be removed in future versions and only UEAndORT back end will be supported.")
	ENeuralBackEnd GetBackEnd() const;
	UE_DEPRECATED(5.0, "Do not use, ENeuralBackEnd will be removed in future versions and only UEAndORT back end will be supported.")
	ENeuralBackEnd GetBackEndForCurrentPlatform() const;
	UE_DEPRECATED(5.0, "Do not use, ENeuralBackEnd will be removed in future versions and only UEAndORT back end will be supported.")
	bool SetBackEnd(const ENeuralBackEnd InBackEnd);

#if WITH_EDITOR
	/**
	 * Internal and Editor-only functions not needed by the user.
	 * They provide importing data and options used for loading the network.
	 */
	TObjectPtr<class UAssetImportData> GetAssetImportData() const;
	TObjectPtr<class UAssetImportData> GetAndMaybeCreateAssetImportData();
#endif // WITH_EDITOR

private:
	/**
	 * The back end to be used (it could and should be set to Auto). @see ENeuralBackEnd for more details.
	 */
	ENeuralBackEnd BackEnd;

	/**
	 * The actual back end currently being used (it cannot be set to Auto).
	 * If BackEnd != Auto, BackEndForCurrentPlatform will be equal to BackEnd.
	 * Otherwise, BackEndForCurrentPlatform will be set to the optimal BackEnd given the current platform.
	 * @see ENeuralBackEnd for more details.
	 */
	ENeuralBackEnd BackEndForCurrentPlatform;

#if WITH_EDITORONLY_DATA
	/** Importing data and options used for loading the network. */
	UPROPERTY(Instanced)
	TObjectPtr<class UAssetImportData> AssetImportData;
#endif // WITH_EDITORONLY_DATA

#if WITH_EDITOR
	/** Editor-only function: Re-import asset with editor data (imported file). */
	void ReimportAssetFromEditorData();
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR

	//~UObject interface
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
	virtual void Serialize(FArchive& Archive) override;
	//~End of UObject interface
};
