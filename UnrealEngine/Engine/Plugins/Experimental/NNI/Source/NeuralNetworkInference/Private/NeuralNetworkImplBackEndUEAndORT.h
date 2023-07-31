// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NeuralNetwork.h"
#include "Async/AsyncWork.h"

// RHI includes must happen before onnxruntime_cxx_api.h (both files include Windows.h)
#include "HAL/CriticalSection.h"

#if defined(WITH_UE_AND_ORT_SUPPORT) && defined(PLATFORM_WIN64)
// Disable NOMINMAX & WIN32_LEAN_AND_MEAN defines to avoid compiler warnings
#pragma push_macro("NOMINMAX")
#pragma push_macro("WIN32_LEAN_AND_MEAN")
#undef NOMINMAX
#undef WIN32_LEAN_AND_MEAN
#include "ID3D12DynamicRHI.h"
#pragma pop_macro("WIN32_LEAN_AND_MEAN")
#pragma pop_macro("NOMINMAX")
#endif

#include "ThirdPartyWarningDisabler.h"
NNI_THIRD_PARTY_INCLUDES_START
#undef check
#undef TEXT
#ifdef WITH_UE_AND_ORT_SUPPORT
#include "core/session/onnxruntime_cxx_api.h"
#ifdef PLATFORM_WIN64
struct OrtDmlApi;
#endif
#endif //WITH_UE_AND_ORT_SUPPORT
NNI_THIRD_PARTY_INCLUDES_END


struct UNeuralNetwork::FImplBackEndUEAndORT
{
public:
	/**
	 * InputTensors and OutputTensors represent the input and output TArray<FNeuralTensor> of the network, respectively.
	 */
	TArray<FNeuralTensor> InputTensors;
	TArray<FNeuralTensor> OutputTensors;

	FImplBackEndUEAndORT(FOnAsyncRunCompleted& InOutOnAsyncRunCompletedDelegate, ENeuralThreadMode& InDelegateThreadMode, FCriticalSection& InResoucesCriticalSection);

	~FImplBackEndUEAndORT();

	[[nodiscard]] static bool ForceCPUIfNoGPU(ENeuralDeviceType& InOutDeviceType);
	static bool IsGPUSupported();

	static bool Load(TSharedPtr<FImplBackEndUEAndORT>& InOutImplBackEndUEAndORT, FOnAsyncRunCompleted& InOutOnAsyncRunCompletedDelegate,
		ENeuralThreadMode& InOutDelegateThreadMode, FCriticalSection& InOutResoucesCriticalSection, TArray<bool>& OutAreInputTensorSizesVariable,
		const TArray<uint8>& InModelReadFromFileInBytes, const FString& InModelFullFilePath, const ENeuralDeviceType InDeviceType, const ENeuralDeviceType InInputDeviceType,
		const ENeuralDeviceType InOutputDeviceType);

	int32 CreateInferenceContext(ENeuralDeviceType InInputDeviceType, ENeuralDeviceType InOutputDeviceType);
	void DestroyInferenceContext(int32 ContextHandle);

	void Run(int32 ContextHandle);
	void Run(FRDGBuilder& GraphBuilder, int32 ContextHandle);
	void Run(const ENeuralSynchronousMode InSynchronousMode, const ENeuralDeviceType InDeviceType, const ENeuralDeviceType InInputDeviceType);

	FNeuralTensor& GetInputTensorForContext(int32 InContextHandle, int32 InTensorIndex);
	FNeuralTensor& GetOutputTensorForContext(int32 InContextHandle, int32 InTensorIndex);

#ifdef WITH_UE_AND_ORT_SUPPORT
private:
	/** Async support */
	FOnAsyncRunCompleted& OnAsyncRunCompletedDelegate;
	ENeuralThreadMode& DelegateThreadMode;
	FCriticalSection& ResoucesCriticalSection;
	/** Network-related variables */
	TUniquePtr<Ort::Env> Environment;
	TUniquePtr<Ort::Session> Session;
	TUniquePtr<Ort::AllocatorWithDefaultOptions> Allocator;
	TUniquePtr<Ort::SessionOptions> SessionOptions;
	/** Tensor-related variable: Memory allocator information */
	TUniquePtr<Ort::MemoryInfo> AllocatorInfo;
#ifdef PLATFORM_WIN64
	const OrtDmlApi* DmlApi;
	/** DirectML GPU memory information */
	TUniquePtr<Ort::MemoryInfo> DmlGPUMemoryInfo;
	/** Shared D3D12 resources with DirectML GPU execution provider */
	TArray<void*> DmlGPUResources;
#endif
	/** Actual ONNXRuntime tensors */
	TArray<Ort::Value> InputOrtTensors;
	/** Tensor names */
	TArray<const char*> InputTensorNames;
	/** Actual ONNXRuntime tensors */
	TArray<Ort::Value> OutputOrtTensors;
	/** Tensor names */
	TArray<const char*> OutputTensorNames;
	public:
	/** Has the device type been forced to CPU after requesting GPU*/
	bool bIsCPUForced = false;
	private:
	/** Has this network been run before */
	bool bHasRun = false;

	/**
	 * Storage for input and output tensors for ONNX backend.
	 * GPU memory is allocated by RDG/RHI and bound to Ort handles.
	 * Because RHI can defrag the RDG pool at any time we need to call UpdateGPUAllocations() for each frame we use.
	 */
	struct FInferenceContext
	{
		TArray<const char*> InputTensorNames;
		TArray<FNeuralTensor> InputTensors;
		TArray<Ort::Value> InputOrtTensors;

		TArray<const char*> OutputTensorNames;
		TArray<FNeuralTensor> OutputTensors;
		TArray<Ort::Value> OutputOrtTensors;
		TArray<void*> OutputDmlAllocation;

		bool Init(Ort::Session* Session, Ort::AllocatorWithDefaultOptions* Allocator, Ort::MemoryInfo* AllocatorInfo, ENeuralDeviceType InInputDeviceType, ENeuralDeviceType InOutputDeviceType);
		void UpdateGPUAllocations(FRDGBuilder& GraphBuilder);
		void Release();
#ifdef PLATFORM_WIN64
		void BindDMLAllocations(OrtDmlApi const* InDmlApi, Ort::MemoryInfo* InMemoryInfo);
		void ReleaseDMLAllocations(OrtDmlApi const* InDmlApi);
#endif

	};
	/** Array of allocated inference contexts. */
	TSparseArray<FInferenceContext> Contexts;

	// Helper class to run session as an async task
	class FNeuralNetworkAsyncTask : public FNonAbandonableTask
	{
		friend class FAsyncTask<FNeuralNetworkAsyncTask>;
	public:
		FNeuralNetworkAsyncTask(UNeuralNetwork::FImplBackEndUEAndORT* InBackEnd);
		void SetRunSessionArgs(const ENeuralSynchronousMode InSyncMode, const ENeuralDeviceType InDeviceType,
			const ENeuralDeviceType InInputDeviceType);
	protected:
		void DoWork();
		// This next section of code needs to be here. Not important as to why.
		FORCEINLINE TStatId GetStatId() const
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(FNeuralNetworkAsyncTask, STATGROUP_ThreadPoolAsyncTasks);
		}
	private:
		UNeuralNetwork::FImplBackEndUEAndORT* BackEnd;
		// Variables that could change on each inference run
		ENeuralSynchronousMode SyncMode;
		ENeuralDeviceType DeviceType;
		ENeuralDeviceType InputDeviceType;
	};
	TUniquePtr<FAsyncTask<FNeuralNetworkAsyncTask>> NeuralNetworkAsyncTask;

	void EnsureAsyncTaskCompletion() const;

	static bool InitializedAndConfigureMembers(TSharedPtr<FImplBackEndUEAndORT>& InOutImplBackEndUEAndORT, FOnAsyncRunCompleted& InOutOnAsyncRunCompletedDelegate,
		ENeuralThreadMode& InOutDelegateThreadMode, FCriticalSection& InOutResoucesCriticalSection, const FString& InModelFullFilePath, const ENeuralDeviceType InDeviceType);

	bool ConfigureMembers(const ENeuralDeviceType InDeviceType);

	bool ConfigureTensors(TArray<FNeuralTensor>& OutTensors, TArray<bool>* OutAreInputTensorSizesVariable, const ENeuralDeviceType InDeviceType,
		const ENeuralDeviceType InInputDeviceType, const ENeuralDeviceType InOutputDeviceType);

	bool SetTensorsFromNetwork(TArray<FNeuralTensor>& OutTensors, TArray<const char*>& InTensorNames, TArray<ENeuralDataType>& InTensorDataTypes,
		TArray<TArray<int64>>& InSizes, TArray<ENeuralTensorType>& InTensorTypes, const bool bIsInput,
		const ENeuralDeviceType InInputDeviceType, const ENeuralDeviceType InOutputDeviceType);

	static void LinkTensorToONNXRuntime(FNeuralTensor& InTensor, Ort::Value& OutOrtTensor, Ort::MemoryInfo& InOutAllocatorInfo);
#ifdef PLATFORM_WIN64
	static void* LinkTensorResourceToONNXRuntime(OrtDmlApi const* InDmlApi, Ort::MemoryInfo* InMemoryInfo, FNeuralTensor& InOutTensor, Ort::Value& InOutOrtTensor, void* InD3DResource);
#endif

	void RunSessionAsync(const ENeuralDeviceType InDeviceType, const ENeuralDeviceType InInputDeviceType);
	void RunSessionSync(const ENeuralDeviceType InDeviceType, const ENeuralDeviceType InInputDeviceType);
	void RunSessionImpl(const ENeuralDeviceType InDeviceType, const ENeuralDeviceType InInputDeviceType);

	void ClearResources();

	void WarnOnCPUForced(bool bInShouldOpenMessageLog);


#endif //WITH_UE_AND_ORT_SUPPORT
};
