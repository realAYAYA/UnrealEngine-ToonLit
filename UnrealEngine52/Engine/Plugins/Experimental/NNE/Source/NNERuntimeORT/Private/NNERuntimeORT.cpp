// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeORT.h"
#include "NNERuntimeORTModel.h"
#include "NNERuntimeORTUtils.h"
#include "NNEUtilsModelOptimizer.h"
#include "NNECoreAttributeMap.h"
#include "NNECoreModelData.h"
#include "NNECoreModelOptimizerInterface.h"
#include "NNEProfilingTimer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NNERuntimeORT)

FGuid UNNERuntimeORTDmlImpl::GUID = FGuid((int32)'O', (int32)'D', (int32)'M', (int32)'L');
int32 UNNERuntimeORTDmlImpl::Version = 0x00000001;

bool UNNERuntimeORTDmlImpl::CanCreateModelData(FString FileType, TConstArrayView<uint8> FileData) const
{
	return FileType.Compare("onnx", ESearchCase::IgnoreCase) == 0;
}

TArray<uint8> UNNERuntimeORTDmlImpl::CreateModelData(FString FileType, TConstArrayView<uint8> FileData)
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
	UE::NNECore::Internal::FOptimizerOptionsMap Options;
	if (!Optimizer->Optimize(InputModel, OutputModel, Options))
	{
		return {};
	}

	int32 GuidSize = sizeof(UNNERuntimeORTDmlImpl::GUID);
	int32 VersionSize = sizeof(UNNERuntimeORTDmlImpl::Version);
	TArray<uint8> Result;
	FMemoryWriter Writer(Result);
	Writer << UNNERuntimeORTDmlImpl::GUID;
	Writer << UNNERuntimeORTDmlImpl::Version;
	Writer.Serialize(OutputModel.Data.GetData(), OutputModel.Data.Num());
	return Result;
}

void UNNERuntimeORTDmlImpl::Init()
{
	check(!ORTEnvironment.IsValid());
	ORTEnvironment = MakeUnique<Ort::Env>();
}

#if PLATFORM_WINDOWS
bool UNNERuntimeORTDmlImpl::CanCreateModelGPU(TObjectPtr<UNNEModelData> ModelData) const
{
	check(ModelData != nullptr);

	int32 GuidSize = sizeof(UNNERuntimeORTDmlImpl::GUID);
	int32 VersionSize = sizeof(UNNERuntimeORTDmlImpl::Version);
	TConstArrayView<uint8> Data = ModelData->GetModelData(GetRuntimeName());

	if (Data.Num() <= GuidSize + VersionSize)
	{
		return false;
	}

	bool bResult = FGenericPlatformMemory::Memcmp(&(Data[0]), &(UNNERuntimeORTDmlImpl::GUID), GuidSize) == 0;
	bResult &= FGenericPlatformMemory::Memcmp(&(Data[GuidSize]), &(UNNERuntimeORTDmlImpl::Version), VersionSize) == 0;
	return bResult;
}

TUniquePtr<UE::NNECore::IModelGPU> UNNERuntimeORTDmlImpl::CreateModelGPU(TObjectPtr<UNNEModelData> ModelData)
{
	check(ModelData != nullptr);
	check(ORTEnvironment.IsValid());

	if (!CanCreateModelGPU(ModelData))
	{
		return TUniquePtr<UE::NNECore::IModelGPU>();
	}

	const UE::NNERuntimeORT::Private::FRuntimeConf InConf;
	UE::NNERuntimeORT::Private::FModelORTDml* Model = new UE::NNERuntimeORT::Private::FModelORTDml(ORTEnvironment.Get(), InConf);
	TConstArrayView<uint8> Data = ModelData->GetModelData(GetRuntimeName());

	if (!Model->Init(Data))
	{
		delete Model;
		return TUniquePtr<UE::NNECore::IModelGPU>();
	}
	UE::NNECore::IModelGPU* IModel = static_cast<UE::NNECore::IModelGPU*>(Model);
	return TUniquePtr<UE::NNECore::IModelGPU>(IModel);
}

#else // PLATFORM_WINDOWS

bool UNNERuntimeORTDmlImpl::CanCreateModelGPU(TObjectPtr<UNNEModelData> ModelData) const
{
	return false;
}

TUniquePtr<UE::NNECore::IModelGPU> UNNERuntimeORTDmlImpl::CreateModelGPU(TObjectPtr<UNNEModelData> ModelData)
{
	return TUniquePtr<UE::NNECore::IModelGPU>();
}

#endif // PLATFORM_WINDOWS