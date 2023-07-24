// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeRDGHlsl.h"
#include "NNECoreAttributeMap.h"
#include "NNECoreModelData.h"
#include "NNECoreModelOptimizerInterface.h"
#include "NNERuntimeRDG.h"
#include "NNERuntimeRDGHlslHelper.h"
#include "NNERuntimeRDGModelHlsl.h"
#include "NNEUtilsModelOptimizer.h"
#include "Hlsl/NNERuntimeRDGBatchNormalization.h"
#include "Hlsl/NNERuntimeRDGCast.h"
#include "Hlsl/NNERuntimeRDGConv.h"
#include "Hlsl/NNERuntimeRDGConcat.h"
#include "Hlsl/NNERuntimeRDGConvTranspose.h"
#include "Hlsl/NNERuntimeRDGDropout.h"
#include "Hlsl/NNERuntimeRDGElementWiseBinary.h"
#include "Hlsl/NNERuntimeRDGElementWiseUnary.h"
#include "Hlsl/NNERuntimeRDGElementWiseVariadic.h"
#include "Hlsl/NNERuntimeRDGFlatten.h"
#include "Hlsl/NNERuntimeRDGGather.h"
#include "Hlsl/NNERuntimeRDGGemm.h"
#include "Hlsl/NNERuntimeRDGIdentity.h"
#include "Hlsl/NNERuntimeRDGInstanceNormalization.h"
#include "Hlsl/NNERuntimeRDGPad.h"
#include "Hlsl/NNERuntimeRDGReshape.h"
#include "Hlsl/NNERuntimeRDGShape.h"
#include "Hlsl/NNERuntimeRDGSize.h"
#include "Hlsl/NNERuntimeRDGSlice.h"
#include "Hlsl/NNERuntimeRDGSqueeze.h"
#include "Hlsl/NNERuntimeRDGUnsqueeze.h"
#include "Hlsl/NNERuntimeRDGUpsample.h"
#include "Hlsl/NNERuntimeRDGMatMul.h"

using namespace UE::NNERuntimeRDG::Private::Hlsl;

FGuid UNNERuntimeRDGHlslImpl::GUID = FGuid((int32)'R', (int32)'D', (int32)'G', (int32)'H');
int32 UNNERuntimeRDGHlslImpl::Version = 0x00000001;

bool UNNERuntimeRDGHlslImpl::Init()
{
	FOperatorRegistryHlsl* registry = FOperatorRegistryHlsl::Get();
	check(registry != nullptr);

	RegisterBatchNormalizationOperator(*registry);
	RegisterCastOperator(*registry);
	RegisterConvOperator(*registry);
	RegisterConcatOperator(*registry);
	RegisterConvTransposeOperator(*registry);
	RegisterDropoutOperator(*registry);
	RegisterElementWiseBinaryOperators(*registry);
	RegisterElementWiseUnaryOperators(*registry);
	RegisterElementWiseVariadicOperators(*registry);
	RegisterFlattenOperator(*registry);
	RegisterGatherOperator(*registry);
	RegisterGemmOperator(*registry);
	RegisterIdentityOperator(*registry);
	RegisterInstanceNormalizationOperator(*registry);
	RegisterPadOperator(*registry);
	RegisterReshapeOperator(*registry);
	RegisterShapeOperator(*registry);
	RegisterSizeOperator(*registry);
	RegisterSliceOperator(*registry);
	RegisterSqueezeOperator(*registry);
	RegisterUnsqueezeOperator(*registry);
	RegisterUpsampleOperator(*registry);
	RegisterMatMulOperator(*registry);

	return true;
}

bool UNNERuntimeRDGHlslImpl::CanCreateModelData(FString FileType, TConstArrayView<uint8> FileData) const
{
	return FileType.Compare("onnx", ESearchCase::IgnoreCase) == 0;
}

bool UNNERuntimeRDGHlslImpl::CanCreateModelRDG(TObjectPtr<UNNEModelData> ModelData) const
{
	int32 GuidSize = sizeof(GUID);
	int32 VersionSize = sizeof(Version);
	TConstArrayView<uint8> Data = ModelData->GetModelData(GetRuntimeName());
	
	if (Data.Num() <= GuidSize + VersionSize)
	{
		return false;
	}
	bool bResult = FGenericPlatformMemory::Memcmp(&(Data[0]), &(GUID), GuidSize) == 0;
	bResult &= FGenericPlatformMemory::Memcmp(&(Data[GuidSize]), &(Version), VersionSize) == 0;
	return bResult;
};

TArray<uint8> UNNERuntimeRDGHlslImpl::CreateModelData(FString FileType, TConstArrayView<uint8> FileData)
{
	if (!CanCreateModelData(FileType, FileData))
	{
		return {};
	}

	TUniquePtr<UE::NNECore::Internal::IModelOptimizer> Optimizer = UE::NNEUtils::Internal::CreateONNXToNNEModelOptimizer();
	Optimizer->AddValidator(MakeShared<FModelValidatorHlsl>());

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
	
	Writer << GUID;
	Writer << Version;
	Writer.Serialize(OutputModel.Data.GetData(), OutputModel.Data.Num());
	return Result;
};

TUniquePtr<UE::NNECore::IModelRDG> UNNERuntimeRDGHlslImpl::CreateModelRDG(TObjectPtr<UNNEModelData> ModelData)
{
	if (!CanCreateModelRDG(ModelData))
	{
		return TUniquePtr<UE::NNECore::IModelRDG>();
	}

	UE::NNERuntimeRDG::Private::Hlsl::FModel* Model = new UE::NNERuntimeRDG::Private::Hlsl::FModel();
	TConstArrayView<uint8> Data = ModelData->GetModelData(GetRuntimeName());
	
	if (!Model->Init(Data))
	{
		delete Model;
		return TUniquePtr<UE::NNECore::IModelRDG>();
	}
	return TUniquePtr<UE::NNECore::IModelRDG>(Model);
}
