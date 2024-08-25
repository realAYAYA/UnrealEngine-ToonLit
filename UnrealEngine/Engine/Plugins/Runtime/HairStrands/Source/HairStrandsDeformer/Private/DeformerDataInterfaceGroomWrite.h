// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OptimusComputeDataInterface.h"
#include "ComputeFramework/ComputeDataProvider.h"
#include "DeformerDataInterfaceGroomWrite.generated.h"

class FGroomWriteDataInterfaceParameters;
struct FHairGroupInstance;
class FRDGBuffer;
class FRDGBufferSRV;
class FRDGBufferUAV;
class UGroomComponent;

/** Compute Framework Data Interface for writing skinned mesh. */
UCLASS(Category = ComputeFramework)
class UOptimusGroomWriteDataInterface : public UOptimusComputeDataInterface
{
	GENERATED_BODY()

public:
	//~ Begin UOptimusComputeDataInterface Interface
	FString GetDisplayName() const override;
	FName GetCategory() const override;
	TArray<FOptimusCDIPinDefinition> GetPinDefinitions() const override;
	TSubclassOf<UActorComponent> GetRequiredComponentClass() const override;
	//~ End UOptimusComputeDataInterface Interface
	
	//~ Begin UComputeDataInterface Interface
	TCHAR const* GetClassName() const override { return TEXT("GroomWrite"); }
	void GetSupportedInputs(TArray<FShaderFunctionDefinition>& OutFunctions) const override;
	void GetSupportedOutputs(TArray<FShaderFunctionDefinition>& OutFunctions) const override;
	void GetShaderParameters(TCHAR const* UID, FShaderParametersMetadataBuilder& InOutBuilder, FShaderParametersMetadataAllocations& InOutAllocations) const override;
	TCHAR const* GetShaderVirtualPath() const override;
	void GetShaderHash(FString& InOutKey) const override;
	void GetHLSL(FString& OutHLSL, FString const& InDataInterfaceName) const override;
	UComputeDataProvider* CreateDataProvider(TObjectPtr<UObject> InBinding, uint64 InInputMask, uint64 InOutputMask) const override;
	//~ End UComputeDataInterface Interface

private:
	static TCHAR const* TemplateFilePath;
};

/** Compute Framework Data Provider for writing skinned mesh. */
UCLASS(BlueprintType, editinlinenew, Category = ComputeFramework)
class UOptimusGroomWriteDataProvider : public UComputeDataProvider
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Binding)
	TObjectPtr<UGroomComponent> GroomComponent = nullptr;

	uint64 OutputMask = 0;

	//~ Begin UComputeDataProvider Interface
	FComputeDataProviderRenderProxy* GetRenderProxy() override;
	//~ End UComputeDataProvider Interface
};

class FOptimusGroomWriteDataProviderProxy : public FComputeDataProviderRenderProxy
{
public:
	FOptimusGroomWriteDataProviderProxy(UGroomComponent* InGroomComponent, uint64 InOutputMask);

	//~ Begin FComputeDataProviderRenderProxy Interface
	bool IsValid(FValidationData const& InValidationData) const override;
	void AllocateResources(FRDGBuilder& GraphBuilder) override;
	void GatherDispatchData(FDispatchData const& InDispatchData) override;
	//~ End FComputeDataProviderRenderProxy Interface

private:
	using FParameters = FGroomWriteDataInterfaceParameters;

	TArray<FHairGroupInstance*> Instances;
	uint64 OutputMask = 0;

	struct FResources
	{
		FRDGBufferSRV* PositionOffsetBufferSRV = nullptr;
		FRDGBufferSRV* PositionBufferSRV = nullptr;
		FRDGBufferUAV* PositionBufferUAV = nullptr;
		FRDGBufferSRV* PositionBufferSRV_fallback = nullptr;
		FRDGBufferUAV* PositionBufferUAV_fallback = nullptr;

		FRDGBufferUAV* PointAttributeBufferUAV = nullptr;
		FRDGBufferUAV* CurveAttributeBufferUAV = nullptr;
		FRDGBufferUAV* AttributeBufferUAV_fallback = nullptr;
	};
	TArray<FResources> Resources;
};