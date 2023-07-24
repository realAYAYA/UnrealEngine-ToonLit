// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NNECoreRuntime.h"
#include "NNECoreRuntimeRDG.h"
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
	virtual bool IsPlatformSupported(const ITargetPlatform* TargetPlatform) const override { return true; };

	virtual bool CanCreateModelData(FString FileType, TConstArrayView<uint8> FileData) const override;
	virtual TArray<uint8> CreateModelData(FString FileType, TConstArrayView<uint8> FileData) override;

	virtual bool CanCreateModelRDG(TObjectPtr<UNNEModelData> ModelData) const override;
	virtual TUniquePtr<UE::NNECore::IModelRDG> CreateModelRDG(TObjectPtr<UNNEModelData> ModelData) override;
private:
	UE::NNERuntimeRDG::Private::Dml::FDmlDeviceContext* Ctx;
};

