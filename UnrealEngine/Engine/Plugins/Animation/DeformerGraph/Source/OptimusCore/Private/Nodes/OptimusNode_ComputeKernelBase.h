// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IOptimusComputeKernelProvider.h"
#include "OptimusDataDomain.h"
#include "OptimusDataType.h"
#include "OptimusNode.h"

#include "OptimusNode_ComputeKernelBase.generated.h"

class UComputeSource;


UCLASS(Abstract)
class UOptimusNode_ComputeKernelBase :
	public UOptimusNode, 
	public IOptimusComputeKernelProvider
{
	GENERATED_BODY()
	
public:
	/** Implement this to return the HLSL kernel's function name */
	virtual FString GetKernelName() const PURE_VIRTUAL(UOptimusNode_ComputeKernelBase::GetKernelName, return FString();)

	/** Implement this to return the HLSL kernel's function name */
	virtual FIntVector GetGroupSize() const PURE_VIRTUAL(UOptimusNode_ComputeKernelBase::GetGroupSize, return FIntVector();)

	/** Implement this to return the complete HLSL code for this kernel */
	virtual FString GetKernelSourceText() const PURE_VIRTUAL(UOptimusNode_ComputeKernelBase::GetKernelSourceText, return FString();)

	/** Implement this to return additional source code that the kernel requires. */
	virtual TArray<TObjectPtr<UComputeSource>> GetAdditionalSources() const { return {}; }
	
	// IOptimusComputeKernelProvider
	FOptimus_ComputeKernelResult CreateComputeKernel(
		UObject* InKernelSourceOuter,
		const FOptimusPinTraversalContext& InTraversalContext,
		const FOptimus_NodeToDataInterfaceMap& InNodeDataInterfaceMap,
		const FOptimus_PinToDataInterfaceMap& InLinkDataInterfaceMap,
		const TArray<const UOptimusNode*>& InValueNodes,
		const UComputeDataInterface* InGraphDataInterface,
		const UOptimusComponentSourceBinding* InGraphDataComponentBinding,
		FOptimus_InterfaceBindingMap& OutInputDataBindings,
		FOptimus_InterfaceBindingMap& OutOutputDataBindings
	) const override;

	FName GetExecutionDomain() const override PURE_VIRTUAL(UOptimusNode_ComputeKernelBase::GetExecutionDomain, return NAME_None; );
	TArray<const UOptimusNodePin*> GetPrimaryGroupInputPins() const override PURE_VIRTUAL(UOptimusNode_ComputeKernelBase::GetPrimaryGroupInputPins, return {}; );  

	// -- UOptimusNode overrides
	TOptional<FText> ValidateForCompile() const override;
	
protected:
	static TArray<FString> GetIndexNamesFromDataDomainLevels(
		const TArray<FName> &InLevelNames
		)
	{
		TArray<FString> IndexNames;
	
		for (FName DomainName: InLevelNames)
		{
			IndexNames.Add(FString::Printf(TEXT("%sIndex"), *DomainName.ToString()));
		}
		return IndexNames;
	}

	static FString GetCookedKernelSource(
		const FString& InObjectPathName,
		const FString& InShaderSource,
		const FString& InKernelName,
		FIntVector InGroupSize
		);
	
	
private:
	TOptional<FText> ProcessInputPinForComputeKernel(
		const FOptimusPinTraversalContext& InTraversalContext,
		const UOptimusNodePin* InInputPin,
		const FString& InGroupName,
		const FOptimus_NodeToDataInterfaceMap& InNodeDataInterfaceMap,
		const FOptimus_PinToDataInterfaceMap& InLinkDataInterfaceMap,
		const TArray<const UOptimusNode*>& InValueNodes,
		const UComputeDataInterface* InGraphDataInterface,
		const UOptimusComponentSourceBinding* InGraphDataComponentBinding,
		UOptimusKernelSource* InKernelSource,
		TArray<FString>& OutGeneratedFunctions,
		FOptimus_InterfaceBindingMap& OutInputDataBindings
		) const;

	void ProcessOutputPinForComputeKernel(
		const FOptimusPinTraversalContext& InTraversalContext,
		const UOptimusNodePin* InOutputPin,
		const FOptimus_NodeToDataInterfaceMap& InNodeDataInterfaceMap,
		const FOptimus_PinToDataInterfaceMap& InLinkDataInterfaceMap,
		UOptimusKernelSource* InKernelSource,
		TArray<FString>& OutGeneratedFunctions,
		FOptimus_InterfaceBindingMap& OutOutputDataBindings
		) const;

};

