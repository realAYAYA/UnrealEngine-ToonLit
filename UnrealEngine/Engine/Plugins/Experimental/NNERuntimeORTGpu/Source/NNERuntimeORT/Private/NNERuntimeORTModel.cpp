// Copyright Epic Games, Inc. All Rights Reserved.
#include "NNERuntimeORTModel.h"
#include "NNERuntimeORT.h"
#include "NNERuntimeORTUtils.h"
#include "NNEProfilingTimer.h"

#if PLATFORM_WINDOWS

#include "GenericPlatform/GenericPlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include <unknwn.h>
#include "Microsoft/COMPointer.h"
#include "Windows/HideWindowsPlatformTypes.h"

#include "ID3D12DynamicRHI.h"
#include "DirectML.h"
#endif

NNE_THIRD_PARTY_INCLUDES_START
#undef check
#undef TEXT

#include "core/session/onnxruntime_cxx_api.h"

#include "core/providers/cpu/cpu_provider_factory.h"

#if PLATFORM_WINDOWS
#include "Windows/MinWindows.h"

#ifdef USE_DML
#include "core/providers/dml/dml_provider_factory.h"
#endif
#endif

NNE_THIRD_PARTY_INCLUDES_END

namespace UE::NNERuntimeORT::Private
{

	FModelInstanceORT::FModelInstanceORT() :
		bIsLoaded(false),
		bHasRun(false)
	{ }

	FModelInstanceORT::FModelInstanceORT(
		Ort::Env* InORTEnvironment,
		const FRuntimeConf& InRuntimeConf) :
		bIsLoaded(false),
		bHasRun(false),
		RuntimeConf(InRuntimeConf),
		ORTEnvironment(InORTEnvironment)
	{ }

	bool FModelInstanceORT::Init(TConstArrayView<uint8> ModelData)
	{
		DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FModelORT_Init"), STAT_FModelORT_Init, STATGROUP_NNE);
		
		// Get the header size
		int32 GuidSize = sizeof(UNNERuntimeORTGpuImpl::GUID);
		int32 VersionSize = sizeof(UNNERuntimeORTGpuImpl::Version);

		// Clean previous networks
		bIsLoaded = false;
		TConstArrayView<uint8> ModelBuffer = TConstArrayView<uint8>(&(ModelData.GetData()[GuidSize + VersionSize]), ModelData.Num() - GuidSize - VersionSize);

		// Checking Inference Model 
		{
			if (ModelBuffer.Num() == 0)
			{
				UE_LOG(LogNNE, Warning, TEXT("FModelORT::Load(): Input model data is empty."));
				return false;
			}

		}

#if WITH_EDITOR
		try
#endif //WITH_EDITOR
		{
			if (!InitializedAndConfigureMembers())
			{
				UE_LOG(LogNNE, Warning, TEXT("Load(): InitializedAndConfigureMembers failed."));
				return false;
			}

			{
				DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FModelORT_Init_CreateORTSession"), STAT_FModelORT_Init_CreateORTSession, STATGROUP_NNE);
				// Read model from InferenceModel
				Session = MakeUnique<Ort::Session>(*ORTEnvironment, ModelBuffer.GetData(), ModelBuffer.Num(), *SessionOptions);
			}

			if (!ConfigureTensors(true))
			{
				UE_LOG(LogNNE, Warning, TEXT("Load(): Failed to configure Inputs tensors."));
				return false;
			}

			if (!ConfigureTensors(false))
			{
				UE_LOG(LogNNE, Warning, TEXT("Load(): Failed to configure Outputs tensors."));
				return false;
			}
		}
#if WITH_EDITOR
		catch (const std::exception& Exception)
		{
			UE_LOG(LogNNE, Error, TEXT("%s"), UTF8_TO_TCHAR(Exception.what()));
			return false;
		}
#endif //WITH_EDITOR

		bIsLoaded = true;

		// Reset Stats
		ResetStats();

		return IsLoaded();
	}

	bool FModelInstanceORT::IsLoaded() const
	{
		return bIsLoaded;
	}

	bool FModelInstanceORT::InitializedAndConfigureMembers()
	{
		// Initialize 
		// Setting up ORT
		Allocator = MakeUnique<Ort::AllocatorWithDefaultOptions>();
		AllocatorInfo = MakeUnique<Ort::MemoryInfo>(Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU));

		// Configure Session
		SessionOptions = MakeUnique<Ort::SessionOptions>();

		// Configure number threads
		SessionOptions->SetIntraOpNumThreads(RuntimeConf.NumberOfThreads);

		// ORT Setting optimizations to the fastest possible
		SessionOptions->SetGraphOptimizationLevel(RuntimeConf.OptimizationLevel); // ORT_ENABLE_ALL, ORT_ENABLE_EXTENDED, ORT_ENABLE_BASIC, ORT_DISABLE_ALL

		return true;
	}


	bool FModelInstanceORT::ConfigureTensors(const bool InIsInput)
	{
		const bool bIsInput = InIsInput;

		const uint32 NumberTensors = bIsInput ? Session->GetInputCount() : Session->GetOutputCount();

		TArray<NNE::FTensorDesc>& SymbolicTensorDescs = bIsInput ? InputSymbolicTensors : OutputSymbolicTensors;
		TArray<ONNXTensorElementDataType>& TensorsORTType = bIsInput ? InputTensorsORTType : OutputTensorsORTType;
		TArray<char*>& TensorNames = bIsInput ? InputTensorNames : OutputTensorNames;
		TArray<Ort::AllocatedStringPtr>& TensorNameValues = bIsInput ? InputTensorNameValues : OutputTensorNameValues;

		for (uint32 TensorIndex = 0; TensorIndex < NumberTensors; ++TensorIndex)
		{
			// Get Tensor name
			Ort::AllocatedStringPtr CurTensorName = bIsInput ? Session->GetInputNameAllocated(TensorIndex, *Allocator) : Session->GetOutputNameAllocated(TensorIndex, *Allocator);
			TensorNameValues.Emplace(MoveTemp(CurTensorName));
			TensorNames.Emplace(TensorNameValues.Last().get());

			// Get node type
			Ort::TypeInfo CurrentTypeInfo = bIsInput ? Session->GetInputTypeInfo(TensorIndex) : Session->GetOutputTypeInfo(TensorIndex);
			Ort::TensorTypeAndShapeInfo CurrentTensorInfo = CurrentTypeInfo.GetTensorTypeAndShapeInfo();
			const ONNXTensorElementDataType ONNXTensorElementDataTypeEnum = CurrentTensorInfo.GetElementType();
			CurrentTypeInfo.release();

			TensorsORTType.Emplace(ONNXTensorElementDataTypeEnum);

			std::pair<ENNETensorDataType, uint64> TypeAndSize = TranslateTensorTypeORTToNNE(ONNXTensorElementDataTypeEnum);

			TArray<int32> ShapeData;
			ShapeData.Reserve(CurrentTensorInfo.GetShape().size());
			for (const int64_t CurrentTensorSize : CurrentTensorInfo.GetShape())
			{
				ShapeData.Add((int32)CurrentTensorSize);
			}

			NNE::FSymbolicTensorShape Shape = NNE::FSymbolicTensorShape::Make(ShapeData);
			NNE::FTensorDesc SymbolicTensorDesc = NNE::FTensorDesc::Make(FString(TensorNames.Last()), Shape, TypeAndSize.first);

			check(SymbolicTensorDesc.GetElemByteSize() == TypeAndSize.second);
			SymbolicTensorDescs.Emplace(SymbolicTensorDesc);
		}

		return true;
	}
	
	int FModelInstanceORT::SetInputTensorShapes(TConstArrayView<NNE::FTensorShape> InInputShapes)
	{
		InputTensors.Reset();
		OutputTensors.Reset();
		OutputTensorShapes.Reset();

		// Verify input shape are valid for the model and set InputTensorShapes
		if (FModelInstanceBase<IModelInstanceGPU>::SetInputTensorShapes(InInputShapes) != 0)
		{
			return -1;
		}

		// Setup concrete input tensor
		for (int i = 0; i < InputSymbolicTensors.Num(); ++i)
		{
			NNE::Internal::FTensor Tensor = NNE::Internal::FTensor::Make(InputSymbolicTensors[i].GetName(), InInputShapes[i], InputSymbolicTensors[i].GetDataType());
			InputTensors.Emplace(Tensor);
		}

		// Here model optimization could be done now that we know the input shapes, for some models
		// this would allow to resolve output shapes here rather than during inference.

		// Setup concrete output shapes only if all model output shapes are concretes, otherwise it will be set during Run()
		for (NNE::FTensorDesc SymbolicTensorDesc : OutputSymbolicTensors)
		{
			if (SymbolicTensorDesc.GetShape().IsConcrete())
			{
				NNE::Internal::FTensor Tensor = NNE::Internal::FTensor::MakeFromSymbolicDesc(SymbolicTensorDesc);
				OutputTensors.Emplace(Tensor);
				OutputTensorShapes.Emplace(Tensor.GetShape());
			}
		}
		if (OutputTensors.Num() != OutputSymbolicTensors.Num())
		{
			OutputTensors.Reset();
			OutputTensorShapes.Reset();
		}

		return 0;
	}

	int FModelInstanceORT::RunSync(TConstArrayView<NNE::FTensorBindingGPU> InInputBindings, TConstArrayView<NNE::FTensorBindingGPU> InOutputBindings)
	{
		DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FModelORT_Run"), STAT_FModelORT_Run, STATGROUP_NNE);

		// Sanity check
		if (!bIsLoaded)
		{
			UE_LOG(LogNNE, Warning, TEXT("FModelORT::Run(): Call FModelORT::Load() to load a model first."));
			return -1;
		}

		// Verify the model inputs were prepared
		if (InputTensorShapes.Num() == 0)
		{
			UE_LOG(LogNNE, Error, TEXT("Run(): Input shapes are not set, please call SetInputTensorShapes."));
			return -1;
		}

		NNEProfiling::Internal::FTimer RunTimer;
		RunTimer.Tic();

		if (!bHasRun)
		{
			bHasRun = true;
		}

#if WITH_EDITOR
		try
#endif //WITH_EDITOR
		{
			TArray<Ort::Value> InputOrtTensors;
			BindTensorsToORT(InInputBindings, InputTensors, InputTensorsORTType, AllocatorInfo.Get(), InputOrtTensors);

			if (!OutputTensors.IsEmpty())
			{
				// If output shapes are known we can directly map preallocated output buffers
				TArray<Ort::Value> OutputOrtTensors;
				BindTensorsToORT(InOutputBindings, OutputTensors, OutputTensorsORTType, AllocatorInfo.Get(), OutputOrtTensors);

				Session->Run(Ort::RunOptions{ nullptr },
					InputTensorNames.GetData(), &InputOrtTensors[0], InputTensorNames.Num(),
					OutputTensorNames.GetData(), &OutputOrtTensors[0], OutputTensorNames.Num());
			}
			else
			{
				TArray<Ort::Value> OutputOrtTensors;
				for (int i = 0; i < InOutputBindings.Num(); ++i)
				{
					OutputOrtTensors.Emplace(nullptr);
				}

				Session->Run(Ort::RunOptions{ nullptr },
					InputTensorNames.GetData(), &InputOrtTensors[0], InputTensorNames.Num(),
					OutputTensorNames.GetData(), &OutputOrtTensors[0], OutputTensorNames.Num());

				// Output shapes were resolved during inference: Copy the data back to bindings and expose output tensor shapes
				CopyFromORTToBindings(OutputOrtTensors, InOutputBindings, OutputSymbolicTensors, OutputTensors);
				check(OutputTensorShapes.IsEmpty());
				for (int i = 0; i < OutputTensors.Num(); ++i)
				{
					OutputTensorShapes.Emplace(OutputTensors[i].GetShape());
				}
			}
		}
#if WITH_EDITOR
		catch (const std::exception& Exception)
		{
			UE_LOG(LogNNE, Error, TEXT("%s"), UTF8_TO_TCHAR(Exception.what()));
		}
#endif //WITH_EDITOR

		RunStatisticsEstimator.StoreSample(RunTimer.Toc());

		return 0;
	}

	float FModelInstanceORT::GetLastRunTimeMSec() const
	{
		return RunStatisticsEstimator.GetLastSample();
	}

	NNEProfiling::Internal::FStatistics FModelInstanceORT::GetRunStatistics() const
	{
		return RunStatisticsEstimator.GetStats();
	}

	NNEProfiling::Internal::FStatistics FModelInstanceORT::GetInputMemoryTransferStats() const
	{
		return InputTransferStatisticsEstimator.GetStats();
	}

	void FModelInstanceORT::ResetStats()
	{
		RunStatisticsEstimator.ResetStats();
		InputTransferStatisticsEstimator.ResetStats();
	}

#if PLATFORM_WINDOWS

	TUniquePtr<UE::NNE::IModelInstanceGPU> FModelORTDml::CreateModelInstance()
	{
		const FRuntimeConf InConf;
		FModelInstanceORTDml* ModelInstance = new FModelInstanceORTDml(ORTEnvironment, InConf);

		if (!ModelInstance->Init(ModelData))
		{
			delete ModelInstance;
			return TUniquePtr<UE::NNE::IModelInstanceGPU>();
		}

		UE::NNE::IModelInstanceGPU* IModelInstance = static_cast<UE::NNE::IModelInstanceGPU*>(ModelInstance);
		return TUniquePtr<UE::NNE::IModelInstanceGPU>(IModelInstance);
	}

	FModelORTDml::FModelORTDml(Ort::Env* InORTEnvironment, TConstArrayView<uint8> InModelData) :
		ORTEnvironment(InORTEnvironment),
		ModelData(InModelData)
	{

	}

	TUniquePtr<UE::NNE::IModelInstanceGPU> FModelORTCuda::CreateModelInstance()
	{
		const FRuntimeConf InConf;
		FModelInstanceORTCuda* ModelInstance = new FModelInstanceORTCuda(ORTEnvironment, InConf);

		if (!ModelInstance->Init(ModelData))
		{
			delete ModelInstance;
			return TUniquePtr<UE::NNE::IModelInstanceGPU>();
		}

		UE::NNE::IModelInstanceGPU* IModelInstance = static_cast<UE::NNE::IModelInstanceGPU*>(ModelInstance);
		return TUniquePtr<UE::NNE::IModelInstanceGPU>(IModelInstance);
	}

	FModelORTCuda::FModelORTCuda(Ort::Env* InORTEnvironment, TConstArrayView<uint8> InModelData) :
		ORTEnvironment(InORTEnvironment),
		ModelData(InModelData)
	{

	}

	bool FModelInstanceORTDml::InitializedAndConfigureMembers()
	{
		if (!FModelInstanceORT::InitializedAndConfigureMembers())
		{
			return false;
		}

		SessionOptions->DisableCpuMemArena();

		HRESULT res;

		// In order to use DirectML we need D3D12
		ID3D12DynamicRHI* RHI = nullptr;

		if (GDynamicRHI && GDynamicRHI->GetInterfaceType() == ERHIInterfaceType::D3D12)
		{
			RHI = static_cast<ID3D12DynamicRHI*>(GDynamicRHI);

			if (!RHI)
			{
				UE_LOG(LogNNE, Warning, TEXT("Error:%s RHI is not supported by DirectML"), GDynamicRHI->GetName());
				return false;
			}
		}
		else
		{
			if (GDynamicRHI)
			{
				UE_LOG(LogNNE, Warning, TEXT("Error:%s RHI is not supported by DirectML"), GDynamicRHI->GetName());
				return false;
			}
			else
			{
				UE_LOG(LogNNE, Warning, TEXT("Error:No RHI found"));
				return false;
			}
		}

		int DeviceIndex = 0;
		ID3D12Device* D3D12Device = RHI->RHIGetDevice(DeviceIndex);

		DML_CREATE_DEVICE_FLAGS DmlCreateFlags = DML_CREATE_DEVICE_FLAG_NONE;

		// Set debugging flags
		if (GRHIGlobals.IsDebugLayerEnabled)
		{
			DmlCreateFlags |= DML_CREATE_DEVICE_FLAG_DEBUG;
		}

		IDMLDevice* DmlDevice;

		res = DMLCreateDevice(D3D12Device, DmlCreateFlags, IID_PPV_ARGS(&DmlDevice));
		if (!DmlDevice)
		{
			UE_LOG(LogNNE, Warning, TEXT("Failed to create DML device"));
			return false;
		}

		ID3D12CommandQueue* CmdQ = RHI->RHIGetCommandQueue();

		OrtStatusPtr Status = OrtSessionOptionsAppendExecutionProviderEx_DML(*SessionOptions.Get(), DmlDevice, CmdQ);
		if (Status)
		{
			UE_LOG(LogNNE, Warning, TEXT("Failed to initialize session options for ORT Dml EP: %s"), ANSI_TO_TCHAR(Ort::GetApi().GetErrorMessage(Status)));
			return false;
		}


		return true;
	}

	bool FModelInstanceORTCuda::InitializedAndConfigureMembers()
	{
		if (!FModelInstanceORT::InitializedAndConfigureMembers())
		{
			return false;
		}

#if PLATFORM_WINDOWS
		FString CudnnPath(TEXT("CUDNN\\v"));
		if (!FPlatformMisc::GetEnvironmentVariable(TEXT("PATH")).Contains(CudnnPath + TEXT(PREPROCESSOR_TO_STRING(ONNXRUNTIMEEDITOR_CUDNN_VERSION))))
		{
			// Other version of Cudnn should work too https://docs.nvidia.com/deeplearning/cudnn/support-matrix/index.html 
			// however we have seen instability so we enforce the version we test with.
			UE_LOG(LogNNE, Warning, TEXT("Can't find Cudnn %s in PATH. Please ensure the exact version is installed"), TEXT(PREPROCESSOR_TO_STRING(ONNXRUNTIMEEDITOR_CUDNN_VERSION)));
			UE_LOG(LogNNE, Warning, TEXT("The installation guide for Cudnn can be found here https://docs.nvidia.com/deeplearning/cudnn/install-guide/index.html."));
			return false;
		}
#endif 

		SessionOptions->EnableCpuMemArena();

		//Notes: Atm we do not offer multi gpu capability/configuration
		int DeviceId = 0;
		OrtStatusPtr Status = OrtSessionOptionsAppendExecutionProvider_CUDA(*SessionOptions.Get(), DeviceId);
		if (Status)
		{
			UE_LOG(LogNNE, Warning, TEXT("Failed to add the Cuda Execution provider to Onnx runtime %s session.\n"
				"Please ensure Cuda %s and Cudnn %s are installed, see https://onnxruntime.ai/docs/execution-providers/CUDA-ExecutionProvider.html.\n"
				"OnnxRuntime error: %s."), 
				TEXT(PREPROCESSOR_TO_STRING(ONNXRUNTIMEEDITOR_VERSION)),
				TEXT(PREPROCESSOR_TO_STRING(ONNXRUNTIMEEDITOR_CUDA_VERSION)),
				TEXT(PREPROCESSOR_TO_STRING(ONNXRUNTIMEEDITOR_CUDNN_VERSION)),
				ANSI_TO_TCHAR(Ort::GetApi().GetErrorMessage(Status)));

#if PLATFORM_WINDOWS
			void* DllHandle = FPlatformProcess::GetDllHandle(TEXT("cudnn64_8.dll"));
			if (DllHandle == nullptr)
			{
				UE_LOG(LogNNE, Warning, TEXT("Cudnn64_8.dll can't be loaded. Please check Cudnn installation in particular the PATH setup and accessibility."));
				UE_LOG(LogNNE, Warning, TEXT("The installation guide for Cudnn can be found here https://docs.nvidia.com/deeplearning/cudnn/install-guide/index.html."));
			}
			else
			{
				FPlatformProcess::FreeDllHandle(DllHandle);
			}

			DllHandle = FPlatformProcess::GetDllHandle(TEXT("cudart64_110.dll"));
			if (DllHandle == nullptr)
			{
				UE_LOG(LogNNE, Warning, TEXT("cudart64_110.dll can't be loaded. Please check Cuda toolkit installation in particular the PATH setup and accessibility."));
				UE_LOG(LogNNE, Warning, TEXT("The installation guide for Cuda toolkit can be found here https://docs.nvidia.com/cuda/cuda-installation-guide-microsoft-windows/."));
			}
			else
			{
				FPlatformProcess::FreeDllHandle(DllHandle);
			}
#endif
			return false;
		}

		return true;
	}
#endif
	
} // namespace UE::NNERuntimeORT::Private