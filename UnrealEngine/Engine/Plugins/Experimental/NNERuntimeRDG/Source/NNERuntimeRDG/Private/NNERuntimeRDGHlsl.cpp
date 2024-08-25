// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeRDGHlsl.h"

#include "EngineAnalytics.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/SecureHash.h"
#include "NNE.h"
#include "NNEAttributeMap.h"
#include "NNEModelData.h"
#include "NNEModelOptimizerInterface.h"
#include "NNERuntimeRDG.h"
#include "NNERuntimeRDGHlslHelper.h"
#include "NNERuntimeRDGModelHlsl.h"
#ifdef NNE_UTILITIES_AVAILABLE
#include "NNEUtilitiesModelOptimizer.h"
#endif // NNE_UTILITIES_AVAILABLE
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
#include "Hlsl/NNERuntimeRDGGlobalPool.h"
#include "Hlsl/NNERuntimeRDGIdentity.h"
#include "Hlsl/NNERuntimeRDGInstanceNormalization.h"
#include "Hlsl/NNERuntimeRDGPad.h"
#include "Hlsl/NNERuntimeRDGPool.h"
#include "Hlsl/NNERuntimeRDGReduce.h"
#include "Hlsl/NNERuntimeRDGReshape.h"
#include "Hlsl/NNERuntimeRDGShape.h"
#include "Hlsl/NNERuntimeRDGSize.h"
#include "Hlsl/NNERuntimeRDGSlice.h"
#include "Hlsl/NNERuntimeRDGSoftmax.h"
#include "Hlsl/NNERuntimeRDGSqueeze.h"
#include "Hlsl/NNERuntimeRDGTranspose.h"
#include "Hlsl/NNERuntimeRDGUnsqueeze.h"
#include "Hlsl/NNERuntimeRDGUpsample.h"
#include "Hlsl/NNERuntimeRDGMatMul.h"

using namespace UE::NNERuntimeRDG::Private::Hlsl;

FGuid UNNERuntimeRDGHlslImpl::GUID = FGuid((int32)'R', (int32)'D', (int32)'G', (int32)'H');
int32 UNNERuntimeRDGHlslImpl::Version = 0x00000005;

bool UNNERuntimeRDGHlslImpl::Init()
{
	FOperatorRegistryHlsl* Registry = FOperatorRegistryHlsl::Get();
	check(Registry != nullptr);

	RegisterBatchNormalizationOperator(*Registry);
	RegisterCastOperator(*Registry);
	RegisterConvOperator(*Registry);
	RegisterConcatOperator(*Registry);
	RegisterConvTransposeOperator(*Registry);
	RegisterDropoutOperator(*Registry);
	RegisterElementWiseBinaryOperators(*Registry);
	RegisterElementWiseUnaryOperators(*Registry);
	RegisterElementWiseVariadicOperators(*Registry);
	RegisterFlattenOperator(*Registry);
	RegisterGatherOperator(*Registry);
	RegisterGemmOperator(*Registry);
	RegisterGlobalPoolOperators(*Registry);
	RegisterIdentityOperator(*Registry);
	RegisterInstanceNormalizationOperator(*Registry);
	RegisterPadOperator(*Registry);
	RegisterPoolOperators(*Registry);
	RegisterReduceOperators(*Registry);
	RegisterReshapeOperator(*Registry);
	RegisterShapeOperator(*Registry);
	RegisterSizeOperator(*Registry);
	RegisterSliceOperator(*Registry);
	RegisterSoftmaxOperator(*Registry);
	RegisterSqueezeOperator(*Registry);
	RegisterTransposeOperator(*Registry);
	RegisterUnsqueezeOperator(*Registry);
	RegisterUpsampleOperator(*Registry);
	RegisterMatMulOperator(*Registry);

	return true;
}

UNNERuntimeRDGHlslImpl::ECanCreateModelDataStatus UNNERuntimeRDGHlslImpl::CanCreateModelData(const FString& FileType, TConstArrayView<uint8> FileData, const TMap<FString, TConstArrayView<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform) const
{
#ifdef NNE_UTILITIES_AVAILABLE
	return FileType.Compare("onnx", ESearchCase::IgnoreCase) == 0 ? ECanCreateModelDataStatus::Ok : ECanCreateModelDataStatus::FailFileIdNotSupported;
#else
	UE_LOG(LogNNE, Display, TEXT("NNEUtilities is not available on this platform"));
	return ECanCreateModelDataStatus::Fail;
#endif
}

UNNERuntimeRDGHlslImpl::ECanCreateModelRDGStatus UNNERuntimeRDGHlslImpl::CanCreateModelRDG(const TObjectPtr<UNNEModelData> ModelData) const
{
	int32 GuidSize = sizeof(GUID);
	int32 VersionSize = sizeof(Version);
	TSharedPtr<UE::NNE::FSharedModelData> SharedData = ModelData->GetModelData(GetRuntimeName());

	if (!SharedData.IsValid())
	{
		return ECanCreateModelRDGStatus::Fail;
	}

	TConstArrayView<uint8> Data = SharedData->GetView();

	if (Data.Num() <= GuidSize + VersionSize)
	{
		return ECanCreateModelRDGStatus::Fail;
	}
	bool bResult = FGenericPlatformMemory::Memcmp(&(Data[0]), &(GUID), GuidSize) == 0;
	bResult &= FGenericPlatformMemory::Memcmp(&(Data[GuidSize]), &(Version), VersionSize) == 0;

	return bResult ? ECanCreateModelRDGStatus::Ok : ECanCreateModelRDGStatus::Fail;
};

TSharedPtr<UE::NNE::FSharedModelData> UNNERuntimeRDGHlslImpl::CreateModelData(const FString& FileType, TConstArrayView<uint8> FileData, const TMap<FString, TConstArrayView<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform)
{
	if (CanCreateModelData(FileType, FileData, AdditionalFileData, FileId, TargetPlatform) != ECanCreateModelDataStatus::Ok)
	{
		UE_LOG(LogNNE, Warning, TEXT("UNNERuntimeRDGHlsl cannot create the model data with id %s (Filetype: %s)"), *FileId.ToString(EGuidFormats::Digits).ToLower(), *FileType);
		return {};
	}

#ifdef NNE_UTILITIES_AVAILABLE
	TUniquePtr<UE::NNE::Internal::IModelOptimizer> Optimizer = UE::NNEUtilities::Internal::CreateONNXToNNEModelOptimizer();
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

	return MakeShared<UE::NNE::FSharedModelData>(MakeSharedBufferFromArray(MoveTemp(Result)), 0);
#else //NNE_UTILITIES_AVAILABLE
	return {};
#endif //NNE_UTILITIES_AVAILABLE
};

FString UNNERuntimeRDGHlslImpl::GetModelDataIdentifier(const FString& FileType, TConstArrayView<uint8> FileData, const TMap<FString, TConstArrayView<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform) const
{
	return FileId.ToString(EGuidFormats::Digits) + "-" + UNNERuntimeRDGHlslImpl::GUID.ToString(EGuidFormats::Digits) + "-" + FString::FromInt(UNNERuntimeRDGHlslImpl::Version);
}

TSharedPtr<UE::NNE::IModelRDG> UNNERuntimeRDGHlslImpl::CreateModelRDG(const TObjectPtr<UNNEModelData> ModelData)
{
	if (CanCreateModelRDG(ModelData) != ECanCreateModelRDGStatus::Ok)
	{
		UE_LOG(LogNNE, Warning, TEXT("UNNERuntimeRDGHlsl cannot create a model from the model data with id %s"), *ModelData->GetFileId().ToString(EGuidFormats::Digits));
		return TSharedPtr<UE::NNE::IModelRDG>();
	}

	TSharedPtr<UE::NNE::FSharedModelData> Data = ModelData->GetModelData(GetRuntimeName());
	check(Data.IsValid());
	UE::NNERuntimeRDG::Private::Hlsl::FModel* Model = new UE::NNERuntimeRDG::Private::Hlsl::FModel(Data);

	if (FEngineAnalytics::IsAvailable())
	{
		TArray<FAnalyticsEventAttribute> Attributes = MakeAnalyticsEventAttributeArray(
			TEXT("PlatformName"), UGameplayStatics::GetPlatformName(),
			TEXT("HashedRuntimeName"), FMD5::HashAnsiString(*GetRuntimeName()),
			TEXT("ModelDataSize"), Data->GetView().Num()
		);
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("NeuralNetworkEngine.CreateModel"), Attributes);
	}

	return TSharedPtr<UE::NNE::IModelRDG>(Model);
}
