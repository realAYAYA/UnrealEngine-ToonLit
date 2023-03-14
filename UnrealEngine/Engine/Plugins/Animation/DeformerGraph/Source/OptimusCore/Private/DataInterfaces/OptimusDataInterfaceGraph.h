// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ComputeFramework/ComputeDataInterface.h"
#include "ComputeFramework/ComputeDataProvider.h"
#include "ComputeFramework/ShaderParamTypeDefinition.h"
#include "OptimusDataInterfaceGraph.generated.h"

class UOptimusDeformerInstance;
class USkinnedMeshComponent;

/** */
USTRUCT()
struct FOptimusGraphVariableDescription
{
	GENERATED_BODY()

	UPROPERTY()
	FString	Name;

	UPROPERTY()
	FShaderValueTypeHandle ValueType;

	UPROPERTY()
	TArray<uint8> Value;

	UPROPERTY()
	int32 Offset = 0;
};

/** Compute Framework Data Interface used for marshaling compute graph parameters and variables. */
UCLASS(Category = ComputeFramework)
class UOptimusGraphDataInterface : public UComputeDataInterface
{
	GENERATED_BODY()

public:
	void Init(TArray<FOptimusGraphVariableDescription> const& InVariables);

	//~ Begin UComputeDataInterface Interface
	TCHAR const* GetClassName() const override { return TEXT("Graph"); }
	void GetSupportedInputs(TArray<FShaderFunctionDefinition>& OutFunctions) const override;
	void GetShaderParameters(TCHAR const* UID, FShaderParametersMetadataBuilder& InOutBuilder, FShaderParametersMetadataAllocations& InOutAllocations) const override;
	void GetHLSL(FString& OutHLSL, FString const& InDataInterfaceName) const override;
	UComputeDataProvider* CreateDataProvider(TObjectPtr<UObject> InBinding, uint64 InInputMask, uint64 InOutputMask) const override;
	//~ End UComputeDataInterface Interface

private:
	UPROPERTY()
	TArray<FOptimusGraphVariableDescription> Variables;

	UPROPERTY()
	int32 ParameterBufferSize = 0;
};

/** Compute Framework Data Provider for marshaling compute graph parameters and variables. */
UCLASS(BlueprintType, editinlinenew, Category = ComputeFramework)
class UOptimusGraphDataProvider : public UComputeDataProvider
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Binding)
	TObjectPtr<USkinnedMeshComponent> SkinnedMeshComponent = nullptr;

	UPROPERTY()
	TArray<FOptimusGraphVariableDescription> Variables;

	int32 ParameterBufferSize = 0;

	void SetConstant(FString const& InVariableName, TArray<uint8> const& InValue);

	//~ Begin UComputeDataProvider Interface
	FComputeDataProviderRenderProxy* GetRenderProxy() override;
	//~ End UComputeDataProvider Interface
};

class FOptimusGraphDataProviderProxy : public FComputeDataProviderRenderProxy
{
public:
	FOptimusGraphDataProviderProxy(UOptimusDeformerInstance const* DeformerInstance, TArray<FOptimusGraphVariableDescription> const& Variables, int32 ParameterBufferSize);

	//~ Begin FComputeDataProviderRenderProxy Interface
	void GatherDispatchData(FDispatchSetup const& InDispatchSetup, FCollectedDispatchData& InOutDispatchData) override;
	//~ End FComputeDataProviderRenderProxy Interface

private:
	TArray<uint8> ParameterData;
};
