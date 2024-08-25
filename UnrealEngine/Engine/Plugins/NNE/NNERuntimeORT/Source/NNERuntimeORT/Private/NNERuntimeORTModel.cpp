// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeORTModel.h"

#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "NNERuntimeORT.h"
#include "NNERuntimeORTUtils.h"

#if PLATFORM_WINDOWS
#include "DirectML.h"
#include "ID3D12DynamicRHI.h"
#include "Windows/WindowsHWrapper.h"
#endif //PLATFORM_WINDOWS

#include "NNEUtilitiesThirdPartyWarningDisabler.h"
NNE_THIRD_PARTY_INCLUDES_START

#undef check
#undef TEXT
#include "cpu_provider_factory.h"

#if PLATFORM_WINDOWS
#include "dml_provider_factory.h"
#endif //PLATFORM_WINDOWS

NNE_THIRD_PARTY_INCLUDES_END

namespace UE::NNERuntimeORT::Private
{

	template <class ModelInterface, class TensorBinding> 
	FModelInstanceORTBase<ModelInterface, TensorBinding>::FModelInstanceORTBase(const FRuntimeConf& InRuntimeConf, TSharedPtr<Ort::Env> InEnvironment)
		: RuntimeConf(InRuntimeConf), Environment(InEnvironment)
	{}

	template <class ModelInterface, class TensorBinding> 
	bool FModelInstanceORTBase<ModelInterface, TensorBinding>::Init(TConstArrayView<uint8> ModelData)
	{
		constexpr int32 GuidSize = sizeof(UNNERuntimeORTDml::GUID);
		constexpr int32 VersionSize = sizeof(UNNERuntimeORTDml::Version);
		TConstArrayView<uint8> ModelBuffer = TConstArrayView<uint8>(&(ModelData.GetData()[GuidSize + VersionSize]), ModelData.Num() - GuidSize - VersionSize);

		if (ModelBuffer.Num() == 0)
		{
			UE_LOG(LogNNE, Error, TEXT("FModelInstanceORTBase::Init(): Input model data is empty."));
			return false;
		}

		try
		{
			if (!InitializedAndConfigureMembers())
			{
				UE_LOG(LogNNE, Error, TEXT("FModelInstanceORTBase::Init(): InitializedAndConfigureMembers failed."));
				return false;
			}

			Session = MakeUnique<Ort::Session>(*Environment, ModelBuffer.GetData(), ModelBuffer.Num(), *SessionOptions);

			if (!ConfigureTensors(true))
			{
				UE_LOG(LogNNE, Error, TEXT("FModelInstanceORTBase::Init(): Failed to configure Inputs tensors."));
				return false;
			}
			if (!ConfigureTensors(false))
			{
				UE_LOG(LogNNE, Error, TEXT("FModelInstanceORTBase::Init(): Failed to configure Outputs tensors."));
				return false;
			}
		}
		catch (const Ort::Exception& Exception)
		{
			UE_LOG(LogNNE, Error, TEXT("%s"), UTF8_TO_TCHAR(Exception.what()));
			return false;
		}
		catch (...)
		{
			UE_LOG(LogNNE, Error, TEXT("Unknown exception!"));
			return false;
		}

		return true;
	}

	static int32 ORTProfilingSessionNumber = 0;
	static TAutoConsoleVariable<bool> CVarNNERuntimeORTEnableProfiling(
		TEXT("nne.ort.enableprofiling"),
		false,
		TEXT("True if NNERuntimeORT plugin should create ORT sessions with profiling enabled.\n")
		TEXT("When profiling is enabled ORT will create standard performance tracing json files next to the editor executable.\n")
		TEXT("The files will be prefixed by 'NNERuntimeORTProfile_' and can be loaded for example using chrome://tracing.\n")
		TEXT("More information can be found at https://onnxruntime.ai/docs/performance/tune-performance/profiling-tools.html\n"),
		ECVF_Default);

	template <class ModelInterface, class TensorBinding> 
	bool FModelInstanceORTBase<ModelInterface, TensorBinding>::InitializedAndConfigureMembers()
	{
		Allocator = MakeUnique<Ort::AllocatorWithDefaultOptions>();
		AllocatorInfo = MakeUnique<Ort::MemoryInfo>(Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU));
		SessionOptions = MakeUnique<Ort::SessionOptions>();

		
		SessionOptions->SetGraphOptimizationLevel(RuntimeConf.OptimizationLevel);
		if (CVarNNERuntimeORTEnableProfiling.GetValueOnGameThread())
		{
			FString ProfilingFilePrefix("NNERuntimeORTProfile_");
			ProfilingFilePrefix += FString::FromInt(ORTProfilingSessionNumber);
			++ORTProfilingSessionNumber;
			#if PLATFORM_WINDOWS
				SessionOptions->EnableProfiling(*ProfilingFilePrefix);
			#else
				SessionOptions->EnableProfiling(TCHAR_TO_ANSI(*ProfilingFilePrefix));
			#endif
		}

		return true;
	}

	template <class ModelInterface, class TensorBinding>
	bool FModelInstanceORTBase<ModelInterface, TensorBinding>::ConfigureTensors(bool bAreTensorInputs)
	{
		const uint32 NumberTensors							= bAreTensorInputs ? Session->GetInputCount() : Session->GetOutputCount();
		TArray<NNE::FTensorDesc>& SymbolicTensorDescs		= bAreTensorInputs ? NNE::Internal::FModelInstanceBase<ModelInterface>::InputSymbolicTensors : NNE::Internal::FModelInstanceBase<ModelInterface>::OutputSymbolicTensors;
		TArray<ONNXTensorElementDataType>& TensorsORTType	= bAreTensorInputs ? InputTensorsORTType	: OutputTensorsORTType;
		TArray<char*>& TensorNames							= bAreTensorInputs ? InputTensorNames		: OutputTensorNames;
		TArray<Ort::AllocatedStringPtr>& TensorNameValues	= bAreTensorInputs ? InputTensorNameValues	: OutputTensorNameValues;

		SymbolicTensorDescs.Reset();

		for (uint32 TensorIndex = 0; TensorIndex < NumberTensors; ++TensorIndex)
		{
			// Get Tensor name
			Ort::AllocatedStringPtr CurTensorName = bAreTensorInputs ? Session->GetInputNameAllocated(TensorIndex, *Allocator) : Session->GetOutputNameAllocated(TensorIndex, *Allocator);
			TensorNameValues.Emplace(MoveTemp(CurTensorName));
			TensorNames.Emplace(TensorNameValues.Last().get());

			// Get node type
			const Ort::TypeInfo CurrentTypeInfo = bAreTensorInputs ? Session->GetInputTypeInfo(TensorIndex) : Session->GetOutputTypeInfo(TensorIndex);
			const Ort::ConstTensorTypeAndShapeInfo CurrentTensorInfo = CurrentTypeInfo.GetTensorTypeAndShapeInfo();
			const ONNXTensorElementDataType ONNXTensorElementDataTypeEnum = CurrentTensorInfo.GetElementType();
			const TypeInfoORT TypeInfo = TranslateTensorTypeORTToNNE(ONNXTensorElementDataTypeEnum);

			TensorsORTType.Emplace(ONNXTensorElementDataTypeEnum);

			// Get shape
			TArray<int32, TInlineAllocator<NNE::FTensorShape::MaxRank>> ShapeData;
			ShapeData.Reserve(CurrentTensorInfo.GetShape().size());
			for (int64 CurrentTensorSize : CurrentTensorInfo.GetShape())
			{
				ShapeData.Add((int32)CurrentTensorSize);
			}

			const NNE::FSymbolicTensorShape Shape = NNE::FSymbolicTensorShape::Make(ShapeData);
			const NNE::FTensorDesc SymbolicTensorDesc = NNE::FTensorDesc::Make(FString(TensorNames.Last()), Shape, TypeInfo.DataType);

			check(SymbolicTensorDesc.GetElementByteSize() == TypeInfo.ElementSize);
			SymbolicTensorDescs.Emplace(SymbolicTensorDesc);
		}

		return true;
	}

	template <class ModelInterface, class TensorBinding> 
	typename ModelInterface::ESetInputTensorShapesStatus FModelInstanceORTBase<ModelInterface, TensorBinding>::SetInputTensorShapes(TConstArrayView<NNE::FTensorShape> InInputShapes)
	{
		using ModelInstanceBase = NNE::Internal::FModelInstanceBase<ModelInterface>;

		InputTensors.Reset();
		OutputTensors.Reset();
		ModelInstanceBase::OutputTensorShapes.Reset();

		// Verify input shape are valid for the model and set InputTensorShapes
		if (typename ModelInterface::ESetInputTensorShapesStatus Status = ModelInstanceBase::SetInputTensorShapes(InInputShapes); Status != ModelInterface::ESetInputTensorShapesStatus::Ok)
		{
			return Status;
		}

		// Setup concrete input tensor
		for (int32 i = 0; i < ModelInstanceBase::InputSymbolicTensors.Num(); ++i)
		{
			NNE::Internal::FTensor Tensor = NNE::Internal::FTensor::Make(ModelInstanceBase::InputSymbolicTensors[i].GetName(), InInputShapes[i], ModelInstanceBase::InputSymbolicTensors[i].GetDataType());
			InputTensors.Emplace(Tensor);
		}

		// Setup concrete output shapes only if all model output shapes are concretes, otherwise it will be set during Run()
		for (NNE::FTensorDesc SymbolicTensorDesc : ModelInstanceBase::OutputSymbolicTensors)
		{
			if (SymbolicTensorDesc.GetShape().IsConcrete())
			{
				NNE::Internal::FTensor Tensor = NNE::Internal::FTensor::MakeFromSymbolicDesc(SymbolicTensorDesc);
				OutputTensors.Emplace(Tensor);
				ModelInstanceBase::OutputTensorShapes.Emplace(Tensor.GetShape());
			}
		}
		if (OutputTensors.Num() != ModelInstanceBase::OutputSymbolicTensors.Num())
		{
			OutputTensors.Reset();
			ModelInstanceBase::OutputTensorShapes.Reset();
		}

		return ModelInterface::ESetInputTensorShapesStatus::Ok;
	}

	template <class ModelInterface, class TensorBinding>
	typename ModelInterface::ERunSyncStatus FModelInstanceORTBase<ModelInterface, TensorBinding>::RunSync(TConstArrayView<TensorBinding> InInputBindings, TConstArrayView<TensorBinding> InOutputBindings)
	{
		checkf(Session.IsValid(), TEXT("FModelInstanceORT::RunSync(): Called without a Session, FModelInstanceORT::Init() should have been called."));

		SCOPED_NAMED_EVENT_TEXT("FModelInstanceORTBase::RunSync", FColor::Magenta);

		// Verify the model inputs were prepared
		if (NNE::Internal::FModelInstanceBase<ModelInterface>::InputTensorShapes.Num() == 0)
		{
			UE_LOG(LogNNE, Error, TEXT("RunSync(): Input shapes are not set, please call SetInputTensorShapes."));
			return ModelInterface::ERunSyncStatus::Fail;
		}

		try
		{
			TArray<Ort::Value> InputOrtTensors;
			BindTensorsToORT(InInputBindings, InputTensors, InputTensorsORTType, *AllocatorInfo, InputOrtTensors);

			if (!OutputTensors.IsEmpty())
			{
				// If output shapes are known we can directly map preallocated output buffers
				TArray<Ort::Value> OutputOrtTensors;
				BindTensorsToORT(InOutputBindings, OutputTensors, OutputTensorsORTType, *AllocatorInfo, OutputOrtTensors);

				Session->Run(Ort::RunOptions{ nullptr },
					InputTensorNames.GetData(), &InputOrtTensors[0], InputTensorNames.Num(),
					OutputTensorNames.GetData(), &OutputOrtTensors[0], OutputTensorNames.Num());
			}
			else
			{
				TArray<Ort::Value> OutputOrtTensors;
				for (int32 i = 0; i < InOutputBindings.Num(); ++i)
				{
					OutputOrtTensors.Emplace(nullptr);
				}

				Session->Run(Ort::RunOptions{ nullptr },
					InputTensorNames.GetData(), &InputOrtTensors[0], InputTensorNames.Num(),
					OutputTensorNames.GetData(), &OutputOrtTensors[0], OutputTensorNames.Num());

				// Output shapes were resolved during inference: Copy the data back to bindings and expose output tensor shapes
				CopyFromORTToBindings(OutputOrtTensors, InOutputBindings, NNE::Internal::FModelInstanceBase<ModelInterface>::OutputSymbolicTensors, OutputTensors);
				check(NNE::Internal::FModelInstanceBase<ModelInterface>::OutputTensorShapes.IsEmpty());
				for (int32 i = 0; i < OutputTensors.Num(); ++i)
				{
					NNE::Internal::FModelInstanceBase<ModelInterface>::OutputTensorShapes.Emplace(OutputTensors[i].GetShape());
				}
			}
		}
		catch (const Ort::Exception& Exception)
		{
			UE_LOG(LogNNE, Error, TEXT("%s"), UTF8_TO_TCHAR(Exception.what()));
			return ModelInterface::ERunSyncStatus::Fail;
		}
		catch (...)
		{
			UE_LOG(LogNNE, Error, TEXT("Unknown exception!"));
			return ModelInterface::ERunSyncStatus::Fail;
		}

		return ModelInterface::ERunSyncStatus::Ok;
	}

	TSharedPtr<NNE::IModelInstanceCPU> FModelORTCpu::CreateModelInstanceCPU()
	{
		const FRuntimeConf Conf;
		FModelInstanceORTCpu* ModelInstance = new FModelInstanceORTCpu(Conf, Environment);

		check(ModelData.IsValid());
		if (!ModelInstance->Init(ModelData->GetView()))
		{
			delete ModelInstance;
			return TSharedPtr<UE::NNE::IModelInstanceCPU>();
		}

		NNE::IModelInstanceCPU* IModelInstance = static_cast<NNE::IModelInstanceCPU*>(ModelInstance);
		return TSharedPtr<NNE::IModelInstanceCPU>(IModelInstance);
	}

	FModelORTCpu::FModelORTCpu(TSharedPtr<Ort::Env> InEnvironment, const TSharedPtr<UE::NNE::FSharedModelData>& InModelData) :
		Environment(InEnvironment), ModelData(InModelData)
	{
	}

	bool FModelInstanceORTCpu::InitializedAndConfigureMembers()
	{
		if (!FModelInstanceORTBase::InitializedAndConfigureMembers())
		{
			return false;
		}

		SessionOptions->EnableCpuMemArena();

		return true;
	}

#if PLATFORM_WINDOWS

	TSharedPtr<NNE::IModelInstanceGPU> FModelORTDml::CreateModelInstanceGPU()
	{
		const FRuntimeConf Conf;
		FModelInstanceORTDml* ModelInstance = new FModelInstanceORTDml(Conf, Environment);

		check(ModelData.IsValid());
		if (!ModelInstance->Init(ModelData->GetView()))
		{
			delete ModelInstance;
			return TSharedPtr<UE::NNE::IModelInstanceGPU>();
		}

		NNE::IModelInstanceGPU* IModelInstance = static_cast<NNE::IModelInstanceGPU*>(ModelInstance);
		return TSharedPtr<NNE::IModelInstanceGPU>(IModelInstance);
	}

	FModelORTDml::FModelORTDml(TSharedPtr<Ort::Env> InEnvironment, const TSharedPtr<UE::NNE::FSharedModelData>& InModelData) :
		Environment(InEnvironment), ModelData(InModelData)
	{
	}

	bool FModelInstanceORTDml::InitializedAndConfigureMembers()
	{
		if (!FModelInstanceORTBase::InitializedAndConfigureMembers())
		{
			return false;
		}

		SessionOptions->SetExecutionMode(ExecutionMode::ORT_SEQUENTIAL);
		SessionOptions->DisableMemPattern();
		SessionOptions->DisableCpuMemArena();

		// In order to use DirectML we need D3D12
		ID3D12DynamicRHI* RHI = nullptr;

		if (!GDynamicRHI)
		{
			UE_LOG(LogNNE, Error, TEXT("Error:No RHI found, could not initialize"));
			return false;
		}

		if (IsRHID3D12() )
		{
			RHI = GetID3D12DynamicRHI();
		}
		else
		{
			if (GDynamicRHI)
			{
				UE_LOG(LogNNE, Error, TEXT("Error:%s RHI is not supported by DirectML, please use D3D12."), GDynamicRHI->GetName());
				return false;
			}
			else
			{
				UE_LOG(LogNNE, Error, TEXT("Error:No RHI found"));
				return false;
			}
		}

		check(RHI);

		const int32 DeviceIndex = 0;
		ID3D12Device* D3D12Device = RHI->RHIGetDevice(DeviceIndex);

		if (!D3D12Device)
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to get D3D12 Device from RHI for device index %d"), DeviceIndex);
			return false;
		}

		DML_CREATE_DEVICE_FLAGS DmlCreateFlags = DML_CREATE_DEVICE_FLAG_NONE;

		// Set debugging flags
		if (GRHIGlobals.IsDebugLayerEnabled)
		{
			DmlCreateFlags |= DML_CREATE_DEVICE_FLAG_DEBUG;
		}

		IDMLDevice* DmlDevice = nullptr;
		HRESULT Res = DMLCreateDevice(D3D12Device, DmlCreateFlags, IID_PPV_ARGS(&DmlDevice));

		if (FAILED(Res) || !DmlDevice)
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to create DirectML device, DMLCreateDevice error code :%x"), Res);
			return false;
		}

		ID3D12CommandQueue* CmdQ = RHI->RHIGetCommandQueue();
		
		const OrtDmlApi* DmlApi = nullptr;
		Ort::ThrowOnError(Ort::GetApi().GetExecutionProviderApi("DML", ORT_API_VERSION, reinterpret_cast<const void**>(&DmlApi)));

		if (!DmlApi)
		{
			UE_LOG(LogNNE, Error, TEXT("Ort DirectML Api not available!"));
			return false;
		}

		OrtStatusPtr Status = DmlApi->SessionOptionsAppendExecutionProvider_DML1(*SessionOptions.Get(), DmlDevice, CmdQ);
		if (Status)
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to add DirectML execution provider to OnnxRuntime session options: %s"), ANSI_TO_TCHAR(Ort::GetApi().GetErrorMessage(Status)));
			return false;
		}


		return true;
	}
#endif //PLATFORM_WINDOWS
	
} // namespace UE::NNERuntimeORT::Private