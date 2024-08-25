// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RenderGraphDefinitions.h"
#include "OptimusComputeDataInterface.h"
#include "HairStrandsInterface.h"
#include "ComputeFramework/ComputeDataProvider.h"
#include "DeformerDataInterfaceGroom.generated.h"

class FGroomDataInterfaceParameters;
class UGroomComponent;

/** Compute Framework Data Interface for reading groom. */
UCLASS(Category = ComputeFramework)
class UOptimusGroomDataInterface : public UOptimusComputeDataInterface
{
	GENERATED_BODY()

public:
	//~ Begin UOptimusComputeDataInterface Interface
	FString GetDisplayName() const override;
	TArray<FOptimusCDIPinDefinition> GetPinDefinitions() const override;
	TSubclassOf<UActorComponent> GetRequiredComponentClass() const override;
	//~ End UOptimusComputeDataInterface Interface
	
	//~ Begin UComputeDataInterface Interface
	TCHAR const* GetClassName() const override { return TEXT("Groom"); }
	void GetSupportedInputs(TArray<FShaderFunctionDefinition>& OutFunctions) const override;
	void GetShaderParameters(TCHAR const* UID, FShaderParametersMetadataBuilder& InOutBuilder, FShaderParametersMetadataAllocations& InOutAllocations) const override;
	TCHAR const* GetShaderVirtualPath() const override;
	void GetShaderHash(FString& InOutKey) const override;
	void GetHLSL(FString& OutHLSL, FString const& InDataInterfaceName) const override;
	UComputeDataProvider* CreateDataProvider(TObjectPtr<UObject> InBinding, uint64 InInputMask, uint64 InOutputMask) const override;
	//~ End UComputeDataInterface Interface

private:
	static TCHAR const* TemplateFilePath;
};

/** Compute Framework Data Provider for reading groom. */
UCLASS(BlueprintType, editinlinenew, Category = ComputeFramework)
class UOptimusGroomDataProvider : public UComputeDataProvider
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Binding)
	TObjectPtr<UGroomComponent> Groom = nullptr;

	//~ Begin UComputeDataProvider Interface
	FComputeDataProviderRenderProxy* GetRenderProxy() override;
	//~ End UComputeDataProvider Interface
};

class FOptimusGroomDataProviderProxy : public FComputeDataProviderRenderProxy
{
public:
	FOptimusGroomDataProviderProxy(UGroomComponent* InGroomComponent);

	//~ Begin FComputeDataProviderRenderProxy Interface
	bool IsValid(FValidationData const& InValidationData) const override;
	void AllocateResources(FRDGBuilder& GraphBuilder) override;
	void GatherDispatchData(FDispatchData const& InDispatchData) override;
	//~ End FComputeDataProviderRenderProxy Interface

private:
	using FParameters = FGroomDataInterfaceParameters;

	UGroomComponent* GroomComponent = nullptr; // Should it be HairInstance instead?
	TArray<FHairStrandsInstanceResourceParameters> Resources;
	TArray<FHairStrandsInstanceInterpolationParameters> Interpolations;
	FRDGBufferSRVRef FallbackByteAddressSRV;
	FRDGBufferSRVRef FallbackStructuredSRV;
};
