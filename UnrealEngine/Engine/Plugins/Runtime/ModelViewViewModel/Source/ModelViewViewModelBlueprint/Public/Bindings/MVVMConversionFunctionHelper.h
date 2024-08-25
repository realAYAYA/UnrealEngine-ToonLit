// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "Templates/Function.h"
#include "Templates/SubclassOf.h"
#include "Templates/ValueOrError.h"

struct FMVVMBlueprintFunctionReference;
struct FMVVMBlueprintPropertyPath;
struct FMVVMBlueprintViewBinding;

class UBlueprint;
class UClass;
class UEdGraph;
class UEdGraphNode;
class UEdGraphPin;
class UK2Node;
class UK2Node_CallFunction;
class FKismetCompilerContext;
class UMVVMBlueprintView;
namespace UE::MVVM { struct FMVVMConstFieldVariant; }

namespace UE::MVVM::ConversionFunctionHelper
{
	/** Conversion function requires a wrapper. */
	MODELVIEWVIEWMODELBLUEPRINT_API bool RequiresWrapper(const UFunction* Function);

	/** The pin is valid to use with a PropertyPath. */
	MODELVIEWVIEWMODELBLUEPRINT_API bool IsInputPin(const UEdGraphPin* Pin);
	
	/** Find the property path of a given argument in the conversion function. */
	MODELVIEWVIEWMODELBLUEPRINT_API FMVVMBlueprintPropertyPath GetPropertyPathForPin(const UBlueprint* WidgetBlueprint, const UEdGraphPin* Pin, bool bSkipResolve);
	
	/** Set the property path of a given argument in the conversion function. */
	MODELVIEWVIEWMODELBLUEPRINT_API void SetPropertyPathForPin(const UBlueprint* Blueprint, const FMVVMBlueprintPropertyPath& PropertyPath, UEdGraphPin* Pin);
	
	/** Find the property path of a given argument in the conversion function. */
	MODELVIEWVIEWMODELBLUEPRINT_API FMVVMBlueprintPropertyPath GetPropertyPathForArgument(const UBlueprint* WidgetBlueprint, const UK2Node_CallFunction* Function, FName ArgumentName, bool bSkipResolve);

	/** Create the name of the conversion function wrapper this binding should have. */
	MODELVIEWVIEWMODELBLUEPRINT_API FName CreateWrapperName(const FMVVMBlueprintViewBinding& Binding, bool bSourceToDestination);

	/**
	 * If we can create a graph to set a property/function.
	 */
	MODELVIEWVIEWMODELBLUEPRINT_API TValueOrError<void, FText> CanCreateSetterGraph(UBlueprint* WidgetBlueprint, const FMVVMBlueprintPropertyPath& PropertyPath);

	struct FCreateGraphResult
	{
		/** The new graph created. */
		UEdGraph* NewGraph = nullptr;
		/** Node that owns the pins. */
		UK2Node* WrappedNode = nullptr;
	};

	/**
	 * Create a graph to set a property/function.
	 */
	MODELVIEWVIEWMODELBLUEPRINT_API TValueOrError<FCreateGraphResult, FText> CreateSetterGraph(UBlueprint* WidgetBlueprint, FName GraphName, const UFunction* Signature, const FMVVMBlueprintPropertyPath& PropertyPath, bool bIsConst, bool bTransient, const bool bIsForEvent);

	/** */
	MODELVIEWVIEWMODELBLUEPRINT_API FCreateGraphResult CreateGraph(UBlueprint* WidgetBlueprint, FName GraphName, const UFunction* Signature, const UFunction* FunctionToWrap, bool bIsConst, bool bTransient);
	
	/** */
	MODELVIEWVIEWMODELBLUEPRINT_API FCreateGraphResult CreateGraph(UBlueprint* WidgetBlueprint, FName GraphName, const UFunction* Signature, const TSubclassOf<UK2Node> Node, bool bIsConst, bool bTransient, TFunctionRef<void(UK2Node*)> InitNodeCallback);

	/** Find the main conversion function node from the given graph. */
	MODELVIEWVIEWMODELBLUEPRINT_API UK2Node* GetWrapperNode(const UEdGraph* Graph);

	/** Find the conversion function node from the given graph. */
	MODELVIEWVIEWMODELBLUEPRINT_API UEdGraphPin* FindPin(const UEdGraph* Graph, const TArrayView<const FName> PinNames);

	/** Find the conversion function node from the given graph. */
	MODELVIEWVIEWMODELBLUEPRINT_API TArray<FName> FindPinId(const UEdGraphPin* GraphPin);

	/** Return the pin used for arguments. */
	MODELVIEWVIEWMODELBLUEPRINT_API TArray<UEdGraphPin*> FindInputPins(const UK2Node* Node);
	
	/** Return the pin used as the return value. */
	MODELVIEWVIEWMODELBLUEPRINT_API UEdGraphPin* FindOutputPin(const UK2Node* Node);
} //namespace

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "MVVMPropertyPath.h"
#endif
