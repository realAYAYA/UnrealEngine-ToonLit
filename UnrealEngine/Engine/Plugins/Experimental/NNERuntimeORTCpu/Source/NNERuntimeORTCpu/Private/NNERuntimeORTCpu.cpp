// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeORTCpu.h"

#include "EngineAnalytics.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/SecureHash.h"
#include "NNEAttributeMap.h"
#include "NNEModelData.h"
#include "NNEModelOptimizerInterface.h"
#include "NNEProfilingTimer.h"
#include "NNERuntimeORTCpuModel.h"
#include "NNERuntimeORTCpuUtils.h"
#include "NNEUtilsModelOptimizer.h"

FGuid UNNERuntimeORTCpuImpl::GUID = FGuid((int32)'O', (int32)'C', (int32)'P', (int32)'U');
int32 UNNERuntimeORTCpuImpl::Version = 0x00000001;

bool UNNERuntimeORTCpuImpl::CanCreateModelData(FString FileType, TConstArrayView<uint8> FileData, FGuid FileId, const ITargetPlatform* TargetPlatform) const
{
	return FileType.Compare("onnx", ESearchCase::IgnoreCase) == 0;
}

TArray<uint8> UNNERuntimeORTCpuImpl::CreateModelData(FString FileType, TConstArrayView<uint8> FileData, FGuid FileId, const ITargetPlatform* TargetPlatform)
{
	if (!CanCreateModelData(FileType, FileData, FileId, TargetPlatform))
	{
		return {};
	}

	TUniquePtr<UE::NNE::Internal::IModelOptimizer> Optimizer = UE::NNEUtils::Internal::CreateONNXToONNXModelOptimizer();

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

FString UNNERuntimeORTCpuImpl::GetModelDataIdentifier(FString FileType, TConstArrayView<uint8> FileData, FGuid FileId, const ITargetPlatform* TargetPlatform)
{
	return FileId.ToString(EGuidFormats::Digits) + "-" + UNNERuntimeORTCpuImpl::GUID.ToString(EGuidFormats::Digits) + "-" + FString::FromInt(UNNERuntimeORTCpuImpl::Version);
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

TUniquePtr<UE::NNE::IModelCPU> UNNERuntimeORTCpuImpl::CreateModel(TObjectPtr<UNNEModelData> ModelData)
{
	check(ModelData != nullptr);
	
	if (!CanCreateModelCPU(ModelData))
	{
		return TUniquePtr<UE::NNE::IModelCPU>();
	}

	TConstArrayView<uint8> Data = ModelData->GetModelData(GetRuntimeName());
	UE::NNERuntimeORTCpu::Private::FModelCPU* Model = new UE::NNERuntimeORTCpu::Private::FModelCPU(&NNEEnvironmentCPU, Data);
	UE::NNE::IModelCPU* IModel = static_cast<UE::NNE::IModelCPU*>(Model);

	if (FEngineAnalytics::IsAvailable())
	{
		TArray<FAnalyticsEventAttribute> Attributes = MakeAnalyticsEventAttributeArray(
			TEXT("PlatformName"), UGameplayStatics::GetPlatformName(),
			TEXT("HashedRuntimeName"), FMD5::HashAnsiString(*GetRuntimeName()),
			TEXT("ModelDataSize"), Data.Num()
		);
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("NeuralNetworkEngine.CreateModel"), Attributes);
	}

	return TUniquePtr<UE::NNE::IModelCPU>(IModel);
}