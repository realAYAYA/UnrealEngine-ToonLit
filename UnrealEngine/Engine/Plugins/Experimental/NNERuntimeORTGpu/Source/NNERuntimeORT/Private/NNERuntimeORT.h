// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NNERuntime.h"
#include "NNERuntimeGPU.h"
#include "Templates/UniquePtr.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/UObjectBaseUtility.h"

#include "NNEThirdPartyWarningDisabler.h"
NNE_THIRD_PARTY_INCLUDES_START
#undef check
#undef TEXT
#include "core/session/onnxruntime_cxx_api.h"
NNE_THIRD_PARTY_INCLUDES_END

#include "NNERuntimeORT.generated.h"

UENUM()
enum class ENNERuntimeORTGpuProvider : uint8
{
	None,
	Dml,
	Cuda
};

UCLASS()
class UNNERuntimeORTGpuImpl : public UObject, public INNERuntime, public INNERuntimeGPU
{
	GENERATED_BODY()

public:
	static FGuid GUID;
	static int32 Version;
	ENNERuntimeORTGpuProvider Provider;

	TUniquePtr<Ort::Env> ORTEnvironment;
	UNNERuntimeORTGpuImpl() {};
	virtual ~UNNERuntimeORTGpuImpl() {}

	void Init(ENNERuntimeORTGpuProvider Provider);

	virtual FString GetRuntimeName() const override;
	virtual bool IsPlatformSupported(const ITargetPlatform* TargetPlatform) const override { return true; };

	virtual bool CanCreateModelData(FString FileType, TConstArrayView<uint8> FileData, FGuid FileId, const ITargetPlatform* TargetPlatform) const override;
	virtual TArray<uint8> CreateModelData(FString FileType, TConstArrayView<uint8> FileData, FGuid FileId, const ITargetPlatform* TargetPlatform) override;
	virtual FString GetModelDataIdentifier(FString FileType, TConstArrayView<uint8> FileData, FGuid FileId, const ITargetPlatform* TargetPlatform) override;

	virtual bool CanCreateModelGPU(TObjectPtr<UNNEModelData> ModelData) const override;
	virtual TUniquePtr<UE::NNE::IModelGPU> CreateModel(TObjectPtr<UNNEModelData> ModelData) override;
};