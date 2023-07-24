// Copyright Epic Games, Inc. All Rights Reserved.
#include "NNERuntimeORTModel.h"
#include "NNERuntimeORT.h"
#include "NNERuntimeORTUtils.h"
#include "NNEProfilingTimer.h"

#if PLATFORM_WINDOWS

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

	FModelORT::FModelORT() :
		bIsLoaded(false),
		bHasRun(false)
	{ }

	FModelORT::FModelORT(
		Ort::Env* InORTEnvironment,
		const FRuntimeConf& InRuntimeConf) :
		bIsLoaded(false),
		bHasRun(false),
		RuntimeConf(InRuntimeConf),
		ORTEnvironment(InORTEnvironment)
	{ }

	bool FModelORT::Init(TConstArrayView<uint8> ModelData)
	{
		DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FModelORT_Init"), STAT_FModelORT_Init, STATGROUP_NNE);
		
		// Get the header size
		int32 GuidSize = sizeof(UNNERuntimeORTDmlImpl::GUID);
		int32 VersionSize = sizeof(UNNERuntimeORTDmlImpl::Version);

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

	bool FModelORT::IsLoaded() const
	{
		return bIsLoaded;
	}

	bool FModelORT::InitializedAndConfigureMembers()
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


	bool FModelORT::ConfigureTensors(const bool InIsInput)
	{
		const bool bIsInput = InIsInput;

		const uint32 NumberTensors = bIsInput ? Session->GetInputCount() : Session->GetOutputCount();

		TArray<NNECore::FTensorDesc>& SymbolicTensorDescs = bIsInput ? InputSymbolicTensors : OutputSymbolicTensors;
		TArray<ONNXTensorElementDataType>& TensorsORTType = bIsInput ? InputTensorsORTType : OutputTensorsORTType;
		TArray<const char*>& TensorNames = bIsInput ? InputTensorNames : OutputTensorNames;

		for (uint32 TensorIndex = 0; TensorIndex < NumberTensors; ++TensorIndex)
		{
			// Get Tensor name
			const char* CurTensorName = bIsInput ? Session->GetInputName(TensorIndex, *Allocator) : Session->GetOutputName(TensorIndex, *Allocator);
			TensorNames.Emplace(CurTensorName);

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

			NNECore::FSymbolicTensorShape Shape = NNECore::FSymbolicTensorShape::Make(ShapeData);
			NNECore::FTensorDesc SymbolicTensorDesc = NNECore::FTensorDesc::Make(FString(CurTensorName), Shape, TypeAndSize.first);

			check(SymbolicTensorDesc.GetElemByteSize() == TypeAndSize.second);
			SymbolicTensorDescs.Emplace(SymbolicTensorDesc);
		}

		return true;
	}
	
	int FModelORT::SetInputTensorShapes(TConstArrayView<NNECore::FTensorShape> InInputShapes)
	{
		InputTensors.Reset();
		OutputTensors.Reset();
		OutputTensorShapes.Reset();

		// Verify input shape are valid for the model and set InputTensorShapes
		if (FModelBase<IModelGPU>::SetInputTensorShapes(InInputShapes) != 0)
		{
			return -1;
		}

		// Setup concrete input tensor
		for (int i = 0; i < InputSymbolicTensors.Num(); ++i)
		{
			NNECore::Internal::FTensor Tensor = NNECore::Internal::FTensor::Make(InputSymbolicTensors[i].GetName(), InInputShapes[i], InputSymbolicTensors[i].GetDataType());
			InputTensors.Emplace(Tensor);
		}

		// Here model optimization could be done now that we know the input shapes, for some models
		// this would allow to resolve output shapes here rather than during inference.

		// Setup concrete output shapes only if all model output shapes are concretes, otherwise it will be set during Run()
		for (NNECore::FTensorDesc SymbolicTensorDesc : OutputSymbolicTensors)
		{
			if (SymbolicTensorDesc.GetShape().IsConcrete())
			{
				NNECore::Internal::FTensor Tensor = NNECore::Internal::FTensor::MakeFromSymbolicDesc(SymbolicTensorDesc);
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

	int FModelORT::RunSync(TConstArrayView<NNECore::FTensorBindingGPU> InInputBindings, TConstArrayView<NNECore::FTensorBindingGPU> InOutputBindings)
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

	float FModelORT::GetLastRunTimeMSec() const
	{
		return RunStatisticsEstimator.GetLastSample();
	}

	NNEProfiling::Internal::FStatistics FModelORT::GetRunStatistics() const
	{
		return RunStatisticsEstimator.GetStats();
	}

	NNEProfiling::Internal::FStatistics FModelORT::GetInputMemoryTransferStats() const
	{
		return InputTransferStatisticsEstimator.GetStats();
	}

	void FModelORT::ResetStats()
	{
		RunStatisticsEstimator.ResetStats();
		InputTransferStatisticsEstimator.ResetStats();
	}

#if PLATFORM_WINDOWS
	bool FModelORTDml::InitializedAndConfigureMembers()
	{
		if (!FModelORT::InitializedAndConfigureMembers())
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
		if (RHI->IsD3DDebugEnabled())
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

		//OrtSessionOptionsAppendExecutionProvider_DML(*SessionOptions.Get(), ORTConfiguration.DeviceId);

		OrtStatusPtr Status = OrtSessionOptionsAppendExecutionProviderEx_DML(*SessionOptions.Get(), DmlDevice, CmdQ);
		if (Status)
		{
			UE_LOG(LogNNE, Warning, TEXT("Failed to initialize session options for ORT Dml EP: %s"), ANSI_TO_TCHAR(Ort::GetApi().GetErrorMessage(Status)));
			return false;
		}

		return true;
	}
#endif
	
} // namespace UE::NNERuntimeORT::Private