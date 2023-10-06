// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NNERuntime.h"
#include "NNERuntimeRDG.h"
#include "Templates/UniquePtr.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/UObjectBaseUtility.h"

#include "NNERuntimeRDGHlsl.generated.h"

UCLASS()
class UNNERuntimeRDGHlslImpl : public UObject, public INNERuntime, public INNERuntimeRDG
{
	GENERATED_BODY()

public:
	static FGuid GUID;
	static int32 Version;

	UNNERuntimeRDGHlslImpl() {};
	virtual ~UNNERuntimeRDGHlslImpl() {}

	bool Init();

	virtual FString GetRuntimeName() const override { return TEXT("NNERuntimeRDGHlsl"); };
	virtual bool IsPlatformSupported(const ITargetPlatform* TargetPlatform) const override { return true; };

	virtual bool CanCreateModelData(FString FileType, TConstArrayView<uint8> FileData, FGuid FileId, const ITargetPlatform* TargetPlatform) const override;
	virtual TArray<uint8> CreateModelData(FString FileType, TConstArrayView<uint8> FileData, FGuid FileId, const ITargetPlatform* TargetPlatform) override;
	virtual FString GetModelDataIdentifier(FString FileType, TConstArrayView<uint8> FileData, FGuid FileId, const ITargetPlatform* TargetPlatform) override;

	virtual bool CanCreateModelRDG(TObjectPtr<UNNEModelData> ModelData) const override;
	virtual TUniquePtr<UE::NNE::IModelRDG> CreateModel(TObjectPtr<UNNEModelData> ModelData) override;
};