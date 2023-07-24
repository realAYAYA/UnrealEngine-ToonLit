// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeRDGDml.h"

#include "NNERuntimeRDG.h"
#include "NNEUtilsModelOptimizer.h"
#include "NNECoreModelData.h"
#include "NNECoreRuntimeFormat.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Algo/Find.h"

#include "Dml/NNEDmlModel.h"

#ifdef NNE_USE_DIRECTML

#include "Dml/NNEDmlOperator.h"
#include "ID3D12DynamicRHI.h"

namespace UE::NNERuntimeRDG::Private::Dml
{

//
// Called on RDG runtime startup
//
bool FRuntimeDmlStartup()
{
	bool bIsD3D12RHI = GDynamicRHI && GDynamicRHI->GetInterfaceType() == ERHIInterfaceType::D3D12;		
	bool bLoadDirectML = true;

	if (IsRunningCommandlet() && !IsAllowCommandletRendering())
	{
		UE_LOG(LogNNE, Display, TEXT("Running inside commandlet without rendering"));
		bLoadDirectML = false;
	}

#ifdef WITH_DIRECTML
		
	if (bIsD3D12RHI && bLoadDirectML)
	{
		FString DirectMLRuntimeBinPath = FPlatformProcess::BaseDir();
		FString DirectMLDLLPaths[2];
		int32	NumPaths = 1;
		
		if (DirectMLRuntimeBinPath.IsEmpty())
		{
			DirectMLRuntimeBinPath = FPlatformProcess::GetModulesDirectory();
		}

#if defined(DIRECTML_PATH)
		DirectMLRuntimeBinPath /= FString(TEXT(PREPROCESSOR_TO_STRING(DIRECTML_PATH)));
#endif

		DirectMLDLLPaths[0] = DirectMLRuntimeBinPath / TEXT("DirectML.dll");

#ifdef WITH_DIRECTML_DEBUG
		if (GetID3D12PlatformDynamicRHI()->IsD3DDebugEnabled())
		{
			DirectMLDLLPaths[1] = DirectMLRuntimeBinPath / TEXT("DirectML.Debug.dll");
			++NumPaths;
		}
#endif

#if defined(DIRECTML_PATH)
		FPlatformProcess::PushDllDirectory(*DirectMLRuntimeBinPath);
#endif

		for (int32 Idx = 0; Idx < NumPaths; ++Idx)
		{
			if (!FPaths::FileExists(DirectMLDLLPaths[Idx]))
			{
				const FString ErrorMessage = FString::Format(TEXT("DLL file not found in \"{0}\"."),
					{ IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*DirectMLDLLPaths[Idx])});
				UE_LOG(LogNNE, Warning, TEXT("NNERuntimeRDGDml:%s"), *ErrorMessage);
				checkf(false, TEXT("%s"), *ErrorMessage);
			}

			FPlatformProcess::GetDllHandle(*DirectMLDLLPaths[Idx]);
		}

#if defined(DIRECTML_PATH)
		FPlatformProcess::PopDllDirectory(*DirectMLRuntimeBinPath);
#endif
	}
#endif // WITH_DIRECTML

	const bool bRegisterOnlyOperators = !bLoadDirectML;
	return bRegisterOnlyOperators;
}

} // namespace UE::NNERuntimeRDG::Private::Dml

#endif

using namespace UE::NNERuntimeRDG::Private::Dml;

bool UNNERuntimeRDGDmlImpl::CanCreateModelData(FString FileType, TConstArrayView<uint8> FileData) const
{
	return FileType.Compare("onnx", ESearchCase::IgnoreCase) == 0;
}

TArray<uint8> UNNERuntimeRDGDmlImpl::CreateModelData(FString FileType, TConstArrayView<uint8> FileData)
{
	if (!CanCreateModelData(FileType, FileData))
	{
		return {};
	}

	TUniquePtr<UE::NNECore::Internal::IModelOptimizer> Optimizer = UE::NNEUtils::Internal::CreateONNXToNNEModelOptimizer();

	FNNEModelRaw InputModel;
	InputModel.Data = FileData;
	InputModel.Format = ENNEInferenceFormat::ONNX;

	FNNEModelRaw OutputModel;
	if (!Optimizer->Optimize(InputModel, OutputModel, {}))
	{
		return {};
	}

	TArray<uint8> Result;
	FMemoryWriter Writer(Result);

	FGuid Guid = FModelInfo::Get()->GetGuid();
	int32 Version = FModelInfo::Get()->GetVersion();

	Writer << Guid;
	Writer << Version;
	Writer.Serialize(OutputModel.Data.GetData(), OutputModel.Data.Num());
	return Result;
};

bool UNNERuntimeRDGDmlImpl::CanCreateModelRDG(TObjectPtr<UNNEModelData> ModelData) const
{
#ifdef NNE_USE_DIRECTML
	TConstArrayView<uint8> Data = ModelData->GetModelData(GetRuntimeName());

	if (Data.Num() <= FModelInfo::Get()->GetGuidSize() + FModelInfo::Get()->GetVersionSize())
	{
		return false;
	}

	bool bResult = FModelInfo::Get()->ValidateGuidAndVersion(Data.GetData(), Data.GetData() + FModelInfo::Get()->GetGuidSize());
	return bResult;
#else
	return false;
#endif
};

TUniquePtr<UE::NNECore::IModelRDG> UNNERuntimeRDGDmlImpl::CreateModelRDG(TObjectPtr<UNNEModelData> ModelData)
{
#ifdef NNE_USE_DIRECTML
	TUniquePtr<FModel>	Model;

	if (!CanCreateModelRDG(ModelData))
	{
		return Model;
	}

	// Create the model and initialize it with the data not including the header
	Model = MakeUnique<FModel>();
	TConstArrayView<uint8> Data = ModelData->GetModelData(GetRuntimeName());

	if (!Model->Init(Data, Ctx))
	{
		Model.Reset();
	}

	return Model;
#else
	return TUniquePtr<UE::NNECore::IModelRDG>();
#endif
}

UNNERuntimeRDGDmlImpl::~UNNERuntimeRDGDmlImpl()
{
#ifdef NNE_USE_DIRECTML
	if (Ctx)
	{
		delete Ctx;
		Ctx = nullptr;
	}
#endif
}

bool UNNERuntimeRDGDmlImpl::Init(bool bRegisterOnlyOperators)
{
#ifdef NNE_USE_DIRECTML
	
	if (bRegisterOnlyOperators)
	{
		UE_LOG(LogNNE, Display, TEXT("Registering only operators"));
		return true;
	}

	HRESULT Res;

	// In order to use DirectML we need D3D12
	ID3D12DynamicRHI* RHI = nullptr;

	if (GDynamicRHI && GDynamicRHI->GetInterfaceType() == ERHIInterfaceType::D3D12)
	{
		RHI = GetID3D12PlatformDynamicRHI();

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

	check(Ctx == nullptr);
	Ctx = new FDmlDeviceContext();
	Ctx->DeviceIndex = 0;
	Ctx->D3D12Device = RHI->RHIGetDevice(Ctx->DeviceIndex);

#if PLATFORM_WINDOWS
	ID3D12Device5* D3D12Device5 = nullptr;

	Res = Ctx->D3D12Device->QueryInterface(&D3D12Device5);
	if (D3D12Device5)
	{
		uint32	NumCommands = 0;

		Res = D3D12Device5->EnumerateMetaCommands(&NumCommands, nullptr);
		if (NumCommands)
		{
			UE_LOG(LogNNE, Verbose, TEXT("D3D12 Meta commands:%u"), NumCommands);

			TArray<D3D12_META_COMMAND_DESC>	MetaCmds;

			MetaCmds.SetNumUninitialized(NumCommands);

			Res = D3D12Device5->EnumerateMetaCommands(&NumCommands, MetaCmds.GetData());
			for (uint32 Idx = 0; Idx < NumCommands; ++Idx)
			{
				const D3D12_META_COMMAND_DESC& Desc = MetaCmds[Idx];

				UE_LOG(LogNNE, Verbose, TEXT("   %s"), Desc.Name);
			}
		}
	}
#endif

	DML_CREATE_DEVICE_FLAGS DmlCreateFlags = DML_CREATE_DEVICE_FLAG_NONE;

	// Set debugging flags
#ifdef WITH_DIRECTML_DEBUG
	if (RHI->IsD3DDebugEnabled())
	{
		DmlCreateFlags |= DML_CREATE_DEVICE_FLAG_DEBUG;
	}
#endif

	Res = DMLCreateDevice(Ctx->D3D12Device, DmlCreateFlags, DML_PPV_ARGS(&(Ctx->Device)));
	if (!Ctx->Device)
	{
		UE_LOG(LogNNE, Warning, TEXT("Failed to create DirectML device, res:%x"), Res);
		return false;
	}

	DML_FEATURE_QUERY_TENSOR_DATA_TYPE_SUPPORT	Fp16Query = { DML_TENSOR_DATA_TYPE_FLOAT16 };
	DML_FEATURE_DATA_TENSOR_DATA_TYPE_SUPPORT	Fp16Supported = {};

	Ctx->Device->CheckFeatureSupport(DML_FEATURE_TENSOR_DATA_TYPE_SUPPORT, sizeof(Fp16Query), &Fp16Query, sizeof(Fp16Supported), &Fp16Supported);

	DML_FEATURE_LEVEL					FeatureLevels[] = { DML_FEATURE_LEVEL_5_0 };
	DML_FEATURE_QUERY_FEATURE_LEVELS	FeatureLevelQuery = { UE_ARRAY_COUNT(FeatureLevels), FeatureLevels };
	DML_FEATURE_DATA_FEATURE_LEVELS		FeatureLevelSupported = {};

	Res = Ctx->Device->CheckFeatureSupport(DML_FEATURE_FEATURE_LEVELS, sizeof(FeatureLevelQuery), &FeatureLevelQuery, sizeof(FeatureLevelSupported), &FeatureLevelSupported);
	if (FAILED(Res) || FeatureLevelSupported.MaxSupportedFeatureLevel < DML_FEATURE_LEVEL_5_0)
	{
		UE_LOG(LogNNE, Warning, TEXT("DirectML feature level %x not supported"), FeatureLevels[0]);
		return false;
	}

	Res = Ctx->Device->CreateCommandRecorder(DML_PPV_ARGS(&Ctx->CmdRec));
	if (!Ctx->CmdRec)
	{
		UE_LOG(LogNNE, Warning, TEXT("Failed to create DML command recorder, res:%x"), Res);
		return false;
	}

	return true;
#else
	return false;
#endif
}
