// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ComputeFramework/ShaderParamTypeDefinition.h"
#include "UObject/Interface.h"
#include "Misc/TVariant.h"

#include "IOptimusComputeKernelProvider.generated.h"

class UComputeDataInterface;
class UOptimusComputeDataInterface;
class UOptimusComponentSourceBinding;
class UOptimusKernelSource;
class UOptimusNode;
class UOptimusNodePin;
struct FOptimusPinTraversalContext;


// Maps the data interface's data binding index to the function we would like to have present
// during kernel compilation to read/write values from/to that data interface's resource.
struct FOptimus_InterfaceBinding
{
	const UComputeDataInterface* DataInterface;
	const UOptimusComponentSourceBinding* ComponentBinding;
	int32 DataInterfaceBindingIndex;
	FString BindingFunctionName;
	FString BindingFunctionNamespace;
};
using FOptimus_InterfaceBindingMap = TMap<int32 /* Kernel Index */, FOptimus_InterfaceBinding>;

// A map that goes from a value/variable node to a compute shader input parameter.
struct FOptimus_KernelParameterBinding
{
	/** The node to retrieve the value from */
	const UOptimusNode* ValueNode;
	
	/** The name of the shader parameter */ 
	FString ParameterName;
	
	/** The value type of the parameter */
	FShaderValueTypeHandle ValueType;
};
using FOptimus_KernelParameterBindingList = TArray<FOptimus_KernelParameterBinding>;

// Maps from a data interface node to the data interface that it represents.
using FOptimus_NodeToDataInterfaceMap =  TMap<const UOptimusNode*, UOptimusComputeDataInterface*>;

// Maps from an output pin to the transient data interface, used to store intermediate results,
// that it represents.
using FOptimus_PinToDataInterfaceMap = TMap<const UOptimusNodePin*, UOptimusComputeDataInterface*>;


using FOptimus_ComputeKernelResult = TVariant<UOptimusKernelSource* /* Kernel */, FText /* Error */>;

UINTERFACE()
class OPTIMUSCORE_API UOptimusComputeKernelProvider :
	public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface that provides a mechanism to identify and work with node graph owners.
 */
class OPTIMUSCORE_API IOptimusComputeKernelProvider
{
	GENERATED_BODY()

public:
	/**
	 * Return an UOptimusKernelSource object, from a compute kernel node state that implements
	 * this interface.
	 * @param InKernelSourceOuter The outer object that will own the new kernel source.
	 * @param InTraversalContext The current context being used to traverse from the graph. Used mainly to keep track of graph nesting.
	 * @param InNodeDataInterfaceMap Map from UOptimusNode objects to data interfaces.
	 * @param InLinkDataInterfaceMap
	 * @param InValueNodes
	 * @param InGraphDataInterface
	 * @param InGraphDataComponentBinding
	 * @param OutInputDataBindings
	 * @param OutOutputDataBindings
	 * @param OutExecutionDataInterface
	 */
	virtual FOptimus_ComputeKernelResult CreateComputeKernel(
		UObject* InKernelSourceOuter,
		const FOptimusPinTraversalContext& InTraversalContext,
		const FOptimus_NodeToDataInterfaceMap& InNodeDataInterfaceMap,
		const FOptimus_PinToDataInterfaceMap& InLinkDataInterfaceMap,
		const TArray<const UOptimusNode*>& InValueNodes,
		const UComputeDataInterface* InGraphDataInterface,
		const UOptimusComponentSourceBinding* InGraphDataComponentBinding,
		FOptimus_InterfaceBindingMap& OutInputDataBindings,
		FOptimus_InterfaceBindingMap& OutOutputDataBindings
		) const = 0;

	/** Returns the execution domain that this kernel should iterate over */
	virtual FName GetExecutionDomain() const = 0;

	/** Returns the input pins of the primary group only. Used for traversing up the graph to identify the component
	 *  source.
	 */
	virtual TArray<const UOptimusNodePin*> GetPrimaryGroupInputPins() const = 0; 
};
