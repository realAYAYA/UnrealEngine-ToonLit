// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NNERuntime.h"
#include "NNERuntimeRDG.h"
#include "Templates/UniquePtr.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/UObjectBaseUtility.h"

#include "NNERuntimeRDGDml.generated.h"

namespace UE::NNERuntimeRDG::Private::Dml
{
	class FDmlDeviceContext;
	bool FRuntimeDmlStartup();
}


UCLASS()
class UNNERuntimeRDGDmlImpl : public UObject, public INNERuntime, public INNERuntimeRDG
{
	GENERATED_BODY()

public:

	UNNERuntimeRDGDmlImpl():Ctx(nullptr) {};
	virtual ~UNNERuntimeRDGDmlImpl();

	bool Init(bool bRegisterOnlyOperators);

	virtual FString GetRuntimeName() const override { return TEXT("NNERuntimeRDGDml"); };

	virtual ECanCreateModelDataStatus CanCreateModelData(const FString& FileType, TConstArrayView<uint8> FileData, const TMap<FString, TConstArrayView<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform) const override;
	virtual TSharedPtr<UE::NNE::FSharedModelData> CreateModelData(const FString& FileType, TConstArrayView<uint8> FileData, const TMap<FString, TConstArrayView<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform) override;
	virtual FString GetModelDataIdentifier(const FString& FileType, TConstArrayView<uint8> FileData, const TMap<FString, TConstArrayView<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform) const override;

	virtual ECanCreateModelRDGStatus CanCreateModelRDG(const TObjectPtr<UNNEModelData> ModelData) const override;
	virtual TSharedPtr<UE::NNE::IModelRDG> CreateModelRDG(const TObjectPtr<UNNEModelData> ModelData) override;

private:
	
	UE::NNERuntimeRDG::Private::Dml::FDmlDeviceContext* Ctx;
	bool												bRegisterOnlyOperators { false };
};

