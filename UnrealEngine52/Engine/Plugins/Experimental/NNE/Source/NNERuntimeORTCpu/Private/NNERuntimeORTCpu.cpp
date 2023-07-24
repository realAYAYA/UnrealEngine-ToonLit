// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeORTCpu.h"

#include "NNECoreAttributeMap.h"
#include "NNECoreModelData.h"
#include "NNECoreModelOptimizerInterface.h"
#include "NNEProfilingTimer.h"
#include "NNERuntimeORTCpuModel.h"
#include "NNERuntimeORTCpuUtils.h"
#include "NNEUtilsModelOptimizer.h"

FGuid UNNERuntimeORTCpuImpl::GUID = FGuid((int32)'O', (int32)'C', (int32)'P', (int32)'U');
int32 UNNERuntimeORTCpuImpl::Version = 0x00000001;

bool UNNERuntimeORTCpuImpl::CanCreateModelData(FString FileType, TConstArrayView<uint8> FileData) const
{
	return FileType.Compare("onnx", ESearchCase::IgnoreCase) == 0;
}

TArray<uint8> UNNERuntimeORTCpuImpl::CreateModelData(FString FileType, TConstArrayView<uint8> FileData)
{
	if (!CanCreateModelData(FileType, FileData))
	{
		return {};
	}

	TUniquePtr<UE::NNECore::Internal::IModelOptimizer> Optimizer = UE::NNEUtils::Internal::CreateONNXToONNXModelOptimizer();

	FNNEModelRaw InputModel;
	InputModel.Data = FileData;
	InputModel.Format = ENNEInferenceFormat::ONNX;
	FNNEModelRaw OutputModel;
	UE::NNEUtils::Internal::FOptimizerOptionsMap Options;
	if (!Optimizer->Optimize(InputModel, OutputModel, Options))
	{
		return {};
	}

	int32 GuidSize = sizeof(UNNERuntimeORTCpuImpl::GUID);
	int32 VersionSize = sizeof(UNNERuntimeORTCpuImpl::Version);
	TArray<uint8> Result;
	FMemoryWriter Writer(Result);
	Writer << UNNERuntimeORTCpuImpl::GUID;
	Writer << UNNERuntimeORTCpuImpl::Version;
	Writer.Serialize(OutputModel.Data.GetData(), OutputModel.Data.Num());
	return Result;
}

bool UNNERuntimeORTCpuImpl::CanCreateModelCPU(TObjectPtr<UNNEModelData> ModelData) const
{
	check(ModelData != nullptr);
	
	int32 GuidSize = sizeof(UNNERuntimeORTCpuImpl::GUID);
	int32 VersionSize = sizeof(UNNERuntimeORTCpuImpl::Version);
	TConstArrayView<uint8> Data = ModelData->GetModelData(GetRuntimeName());
	
	if (Data.Num() <= GuidSize + VersionSize)
	{
		return false;
	}
	
	bool bResult = FGenericPlatformMemory::Memcmp(&(Data[0]), &(UNNERuntimeORTCpuImpl::GUID), GuidSize) == 0;
	bResult &= FGenericPlatformMemory::Memcmp(&(Data[GuidSize]), &(UNNERuntimeORTCpuImpl::Version), VersionSize) == 0;
	return bResult;
}

TUniquePtr<UE::NNECore::IModelCPU> UNNERuntimeORTCpuImpl::CreateModelCPU(TObjectPtr<UNNEModelData> ModelData)
{
	check(ModelData != nullptr);
	
	if (!CanCreateModelCPU(ModelData))
	{
		return TUniquePtr<UE::NNECore::IModelCPU>();
	}

	const UE::NNERuntimeORTCpu::Private::FRuntimeConf InConf;
	UE::NNERuntimeORTCpu::Private::FModelCPU* Model = new UE::NNERuntimeORTCpu::Private::FModelCPU(&NNEEnvironmentCPU, InConf);
	TConstArrayView<uint8> Data = ModelData->GetModelData(GetRuntimeName());
	
	if (!Model->Init(Data))
	{
		delete Model;
		return TUniquePtr<UE::NNECore::IModelCPU>();
	}
	UE::NNECore::IModelCPU* IModel = static_cast<UE::NNECore::IModelCPU*>(Model);
	return TUniquePtr<UE::NNECore::IModelCPU>(IModel);
}