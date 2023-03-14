// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MVVMPropertyPath.h"

class UClass;
class UEdGraph;
class UK2Node_CallFunction;
class UMVVMBlueprintView;
class UWidgetBlueprint;
struct FMVVMBlueprintViewBinding;

namespace UE::MVVM::ConversionFunctionHelper
{
	/** Find all BlueprintPropertyPath used by the given binding. */
	MODELVIEWVIEWMODELBLUEPRINT_API TMap<FName, FMVVMBlueprintPropertyPath> GetAllArgumentPropertyPaths(const UWidgetBlueprint* WidgetBlueprint, const FMVVMBlueprintViewBinding& Binding, bool bSourceToDestination, bool bSkipResolve);

	/** Find all BlueprintPropertyPath used by the given conversion function node. */
	MODELVIEWVIEWMODELBLUEPRINT_API TMap<FName, FMVVMBlueprintPropertyPath> GetAllArgumentPropertyPaths(const UWidgetBlueprint* WidgetBlueprint, const UK2Node_CallFunction* FunctionNode, bool bSkipResolve);
	
	/** Find the property path of a given argument in the conversion function. */
	MODELVIEWVIEWMODELBLUEPRINT_API FMVVMBlueprintPropertyPath GetPropertyPathForArgument(const UWidgetBlueprint* WidgetBlueprint, const UK2Node_CallFunction* Function, FName ArgumentName, bool bSkipResolve);

	/** Create the name of the conversion function wrapper this binding should have. */
	MODELVIEWVIEWMODELBLUEPRINT_API FName CreateWrapperName(const FMVVMBlueprintViewBinding& Binding, bool bSourceToDestination);

	/** Get an existing conversion graph for the corresponding binding if it exist. */
	MODELVIEWVIEWMODELBLUEPRINT_API UEdGraph* GetGraph(const UWidgetBlueprint* WidgetBlueprint, const FMVVMBlueprintViewBinding& Binding, bool bSourceToDestination);

	/** Find the conversion function node from the given graph. */
	MODELVIEWVIEWMODELBLUEPRINT_API UK2Node_CallFunction* GetFunctionNode(UEdGraph* Graph);

	/** Mark the given function node as the conversion function node. */
	MODELVIEWVIEWMODELBLUEPRINT_API void MarkAsConversionFunction(const UK2Node_CallFunction* FunctionNode, const FMVVMBlueprintViewBinding& Binding);
} //namespace