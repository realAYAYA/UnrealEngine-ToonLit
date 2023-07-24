// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NNECoreRuntime.h"
#include "NNECoreRuntimeGPU.h"
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

UCLASS()
class UNNERuntimeORTDmlImpl : public UObject, public INNERuntime, public INNERuntimeGPU
{
	GENERATED_BODY()

public:
	static FGuid GUID;
	static int32 Version;

	TUniquePtr<Ort::Env> ORTEnvironment;
	UNNERuntimeORTDmlImpl() {};
	virtual ~UNNERuntimeORTDmlImpl() {}

	void Init();

	virtual FString GetRuntimeName() const override { return TEXT("NNERuntimeORTDml"); };
	virtual bool IsPlatformSupported(const ITargetPlatform* TargetPlatform) const override { return true; };

	virtual bool CanCreateModelData(FString FileType, TConstArrayView<uint8> FileData) const override;
	virtual TArray<uint8> CreateModelData(FString FileType, TConstArrayView<uint8> FileData) override;

	virtual bool CanCreateModelGPU(TObjectPtr<UNNEModelData> ModelData) const override;
	virtual TUniquePtr<UE::NNECore::IModelGPU> CreateModelGPU(TObjectPtr<UNNEModelData> ModelData) override;
};