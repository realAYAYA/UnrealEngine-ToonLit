// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IOptimusDeprecatedExecutionDataInterface.h"
#include "OptimusComputeDataInterface.h"
#include "ComputeFramework/ComputeDataProvider.h"
#include "DeformerDataInterfaceGroomExec.generated.h"

class FGroomExecDataInterfaceParameters;
class UGroomComponent;

UENUM()
enum class EOptimusGroomExecDomain : uint8
{
	None = 0 UMETA(Hidden),
	/** Run kernel with one thread per control point. */
	ControlPoint = 1,
	/** Run kernel with one thread per curve. */
	Curve,
};

/** Compute Framework Data Interface for executing kernels over a skinned mesh domain. */
UCLASS(Category = ComputeFramework)
class UOptimusGroomExecDataInterface :
	public UOptimusComputeDataInterface,
	public IOptimusDeprecatedExecutionDataInterface
{
	GENERATED_BODY()

public:
	//~ Begin UOptimusComputeDataInterface Interface
	FString GetDisplayName() const override;
	FName GetCategory() const override;
	bool IsVisible() const override {return false;};
	TArray<FOptimusCDIPinDefinition> GetPinDefinitions() const override;
	TSubclassOf<UActorComponent> GetRequiredComponentClass() const override;
	//~ End UOptimusComputeDataInterface Interface
	
	//~ Begin UComputeDataInterface Interface
	TCHAR const* GetClassName() const override { return TEXT("GroomExec"); }
	bool IsExecutionInterface() const override { return true; }
	void GetSupportedInputs(TArray<FShaderFunctionDefinition>& OutFunctions) const override;
	void GetShaderParameters(TCHAR const* UID, FShaderParametersMetadataBuilder& InOutBuilder, FShaderParametersMetadataAllocations& InOutAllocations) const override;
	TCHAR const* GetShaderVirtualPath() const override;
	void GetShaderHash(FString& InOutKey) const override;
	void GetHLSL(FString& OutHLSL, FString const& InDataInterfaceName) const override;
	UComputeDataProvider* CreateDataProvider(TObjectPtr<UObject> InBinding, uint64 InInputMask, uint64 InOutputMask) const override;
	//~ End UComputeDataInterface Interface

	//~ Begin IOptimusDeprecatedExecutionDataInterface Interface
	FName GetSelectedExecutionDomainName() const override;
	//~ End IOptimusDeprecatedExecutionDataInterface Interface
	
	UPROPERTY(EditAnywhere, Category = Execution)
	EOptimusGroomExecDomain Domain = EOptimusGroomExecDomain::ControlPoint;

private:
	static TCHAR const* TemplateFilePath;
};

/** Compute Framework Data Provider for executing kernels over a skinned mesh domain. */
UCLASS(BlueprintType, editinlinenew, Category = ComputeFramework)
class UOptimusGroomExecDataProvider : public UComputeDataProvider
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TObjectPtr<UGroomComponent> GroomComponent = nullptr;

	UPROPERTY()
	EOptimusGroomExecDomain Domain = EOptimusGroomExecDomain::ControlPoint;

	//~ Begin UComputeDataProvider Interface
	FComputeDataProviderRenderProxy* GetRenderProxy() override;
	//~ End UComputeDataProvider Interface
};

class FOptimusGroomExecDataProviderProxy : public FComputeDataProviderRenderProxy
{
public:
	FOptimusGroomExecDataProviderProxy(UGroomComponent* InGroomComponent, EOptimusGroomExecDomain InDomain);

	//~ Begin FComputeDataProviderRenderProxy Interface
	int32 GetDispatchThreadCount(TArray<FIntVector>& ThreadCounts) const override;
	bool IsValid(FValidationData const& InValidationData) const override;
	void GatherDispatchData(FDispatchData const& InDispatchData) override;
	//~ End FComputeDataProviderRenderProxy Interface

private:
	using FParameters = FGroomExecDataInterfaceParameters;

	UGroomComponent* GroomComponent = nullptr;
	EOptimusGroomExecDomain Domain = EOptimusGroomExecDomain::ControlPoint;
};
