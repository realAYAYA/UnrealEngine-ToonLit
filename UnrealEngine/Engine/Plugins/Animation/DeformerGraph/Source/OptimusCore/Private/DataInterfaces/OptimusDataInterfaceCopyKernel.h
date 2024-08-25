// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IOptimusComputeKernelDataInterface.h"
#include "OptimusComponentSource.h"
#include "OptimusComputeDataInterface.h"
#include "OptimusDataType.h"
#include "ComputeFramework/ComputeDataProvider.h"

#include "OptimusDataInterfaceCopyKernel.generated.h"

class UOptimusComponentSource;
class UOptimusComponentSourceBinding;

UCLASS(Category = ComputeFramework)
class OPTIMUSCORE_API UOptimusCopyKernelDataInterface :
	public UComputeDataInterface,
	public IOptimusComputeKernelDataInterface
{
	GENERATED_BODY()

public:
	static const FString NumThreadsReservedName;
	
	//~ Begin UComputeDataInterface Interface
	TCHAR const* GetClassName() const override { return TEXT("CopyKernelData"); }
	bool IsExecutionInterface() const override { return true; }
	bool CanSupportUnifiedDispatch() const override { return true; }
	void GetSupportedInputs(TArray<FShaderFunctionDefinition>& OutFunctions) const override;
	void GetShaderParameters(TCHAR const* UID, FShaderParametersMetadataBuilder& InOutBuilder, FShaderParametersMetadataAllocations& InOutAllocations) const override;
	void GetShaderHash(FString& InOutKey) const override;
	void GetHLSL(FString& OutHLSL, FString const& InDataInterfaceName) const override;
	UComputeDataProvider* CreateDataProvider(TObjectPtr<UObject> InBinding, uint64 InInputMask, uint64 InOutputMask) const override;
	//~ End UComputeDataInterface Interface

	//~ Begin IOptimusComputeKernelDataInterface Interface
	void SetExecutionDomain(const FString& InExecutionDomain) override;
	void SetComponentBinding(const UOptimusComponentSourceBinding* InBinding) override;
	//~ End IOptimusComputeKernelDataInterface Interface
	
	UPROPERTY()
	TWeakObjectPtr<const UOptimusComponentSourceBinding> ComponentSourceBinding;
	
	UPROPERTY()
	FString NumThreadsExpression;
};

/** Compute Framework Data Provider for each copy kernel. */
UCLASS()
class UOptimusCopyKernelDataProvider :
	public UComputeDataProvider
{
	GENERATED_BODY()

public:
	void InitFromDataInterface(const UOptimusCopyKernelDataInterface* InDataInterface, const UObject* InBinding);
	
	//~ Begin UComputeDataProvider Interface
	FComputeDataProviderRenderProxy* GetRenderProxy() override;
	//~ End UComputeDataProvider Interface

	TWeakObjectPtr<const UActorComponent> WeakComponent = nullptr;

	TWeakObjectPtr<const UOptimusCopyKernelDataInterface> WeakDataInterface = nullptr;

protected:
	bool GetInvocationThreadCounts(
		TArray<int32>& OutInvocationThreadCount,
		int32& OutTotalThreadCount
		) const;
};

class FOptimusCopyKernelDataProviderProxy : public FComputeDataProviderRenderProxy
{
public:
	FOptimusCopyKernelDataProviderProxy(
		TArray<int32>&& InInvocationThreadCounts,
		int32 InTotalThreadCount
		);

	//~ Begin FComputeDataProviderRenderProxy Interface
	bool IsValid(FValidationData const& InValidationData) const override;
	int32 GetDispatchThreadCount(TArray<FIntVector>& InOutThreadCounts) const override;
	void GatherDispatchData(FDispatchData const& InDispatchData) override;
	//~ End FComputeDataProviderRenderProxy Interface

	TArray<int32> InvocationThreadCounts;
	int32 TotalThreadCount;
};


