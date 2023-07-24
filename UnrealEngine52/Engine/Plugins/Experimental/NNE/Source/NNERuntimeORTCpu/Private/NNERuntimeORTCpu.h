// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NNECoreRuntime.h"
#include "NNECoreRuntimeCPU.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/UObjectBaseUtility.h"

#include "NNEThirdPartyWarningDisabler.h"
NNE_THIRD_PARTY_INCLUDES_START
#undef check
#undef TEXT
#include "core/session/onnxruntime_cxx_api.h"
NNE_THIRD_PARTY_INCLUDES_END

#include "NNERuntimeORTCpu.generated.h"

UCLASS()
class UNNERuntimeORTCpuImpl : public UObject, public INNERuntime, public INNERuntimeCPU
{
	GENERATED_BODY()

public:
	static FGuid GUID;
	static int32 Version;

	Ort::Env NNEEnvironmentCPU;
	UNNERuntimeORTCpuImpl() {};
	virtual ~UNNERuntimeORTCpuImpl() {}
		
	virtual FString GetRuntimeName() const override { return TEXT("NNERuntimeORTCpu"); };
	virtual bool IsPlatformSupported(const ITargetPlatform* TargetPlatform) const override { return true; };

	virtual bool CanCreateModelData(FString FileType, TConstArrayView<uint8> FileData) const override;
	virtual TArray<uint8> CreateModelData(FString FileType, TConstArrayView<uint8> FileData) override;

	virtual bool CanCreateModelCPU(TObjectPtr<UNNEModelData> ModelData) const override;
	virtual TUniquePtr<UE::NNECore::IModelCPU> CreateModelCPU(TObjectPtr<UNNEModelData> ModelData) override;
};