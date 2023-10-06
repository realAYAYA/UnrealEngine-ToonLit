// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"

struct FMVVMBlueprintPropertyPath;
struct FMVVMBlueprintViewBinding;

class UBlueprint;
class UClass;
class UEdGraph;
class UEdGraphPin;
class UK2Node;
class UK2Node_CallFunction;
class FKismetCompilerContext;
class UMVVMBlueprintView;

namespace UE::MVVM::ConversionFunctionHelper
{
	/** Conversion function requires a wrapper. */
	MODELVIEWVIEWMODELBLUEPRINT_API bool RequiresWrapper(const UFunction* Function);

	/** Find all BlueprintPropertyPath used by the given binding. */
	MODELVIEWVIEWMODELBLUEPRINT_API TMap<FName, FMVVMBlueprintPropertyPath> GetAllArgumentPropertyPaths(const UBlueprint* WidgetBlueprint, const FMVVMBlueprintViewBinding& Binding, bool bSourceToDestination, bool bSkipResolve);

	/** Find all BlueprintPropertyPath used by the given conversion function node. */
	MODELVIEWVIEWMODELBLUEPRINT_API TMap<FName, FMVVMBlueprintPropertyPath> GetAllArgumentPropertyPaths(const UBlueprint* WidgetBlueprint, const UK2Node_CallFunction* FunctionNode, bool bSkipResolve);
	
	/** Find the property path of a given argument in the conversion function. */
	MODELVIEWVIEWMODELBLUEPRINT_API FMVVMBlueprintPropertyPath GetPropertyPathForPin(const UBlueprint* WidgetBlueprint, const UEdGraphPin* Pin, bool bSkipResolve);
	
	/** Set the property path of a given argument in the conversion function. */
	MODELVIEWVIEWMODELBLUEPRINT_API void SetPropertyPathForPin(const UBlueprint* Blueprint, const FMVVMBlueprintPropertyPath& Path, UEdGraphPin* Pin);
	
	/** Find the property path of a given argument in the conversion function. */
	MODELVIEWVIEWMODELBLUEPRINT_API FMVVMBlueprintPropertyPath GetPropertyPathForArgument(const UBlueprint* WidgetBlueprint, const UK2Node_CallFunction* Function, FName ArgumentName, bool bSkipResolve);

	/** Create the name of the conversion function wrapper this binding should have. */
	MODELVIEWVIEWMODELBLUEPRINT_API FName CreateWrapperName(const FMVVMBlueprintViewBinding& Binding, bool bSourceToDestination);

	/** */
	MODELVIEWVIEWMODELBLUEPRINT_API TPair<UEdGraph*, UK2Node*> CreateGraph(UBlueprint* WidgetBlueprint, FName GraphName, const UFunction* Function, bool bTransient);

	/** Find the conversion function node from the given graph. */
	MODELVIEWVIEWMODELBLUEPRINT_API UK2Node* GetWrapperNode(UEdGraph* Graph);
} //namespace

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "MVVMPropertyPath.h"
#endif
