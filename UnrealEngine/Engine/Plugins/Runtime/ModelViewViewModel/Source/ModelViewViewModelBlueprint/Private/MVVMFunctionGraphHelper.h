// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "UObject/Object.h"

class FKismetCompilerContext;
class UBlueprint;
class UEdGraph;

namespace UE::MVVM::FunctionGraphHelper
{

	/** Generate a function graph that can be viewed in the BP editor. */
	MODELVIEWVIEWMODELBLUEPRINT_API UEdGraph* CreateFunctionGraph(FKismetCompilerContext& Context, const FStringView FunctionName, EFunctionFlags ExtraFunctionFlag, const FStringView Category, bool bIsEditable);

	/** Generate a function graph that cannot be viewed in the BP editor. */
	UEdGraph* CreateIntermediateFunctionGraph(FKismetCompilerContext& Context, const FStringView FunctionName, EFunctionFlags ExtraFunctionFlag, const FStringView Category, bool bIsEditable);

	/** Add an input argument to an existing function graph. */
	bool AddFunctionArgument(UEdGraph* FunctionGraph, TSubclassOf<UObject> Argument, FName ArgumentName);

	/** Add an input argument to an existing function graph. */
	bool AddFunctionArgument(UEdGraph* FunctionGraph, FProperty* Argument, FName ArgumentName);

	/** Generate the nodes needed for to call the MVVMView::SetViewModel function. */
	bool GenerateViewModelSetter(FKismetCompilerContext& Context, UEdGraph* FunctionGraph, FName ViewModelName);

	/** Generate the nodes needed for to set the property and to call the broadcast field notify function. */
	bool GenerateViewModelFieldNotifySetter(FKismetCompilerContext& Context, UEdGraph* FunctionGraph, FProperty* Property, FName InputPinName);

	/** Generate the nodes needed to broadcast field notify function. */
	bool GenerateViewModelFielNotifyBroadcast(FKismetCompilerContext& Context, UEdGraph* FunctionGraph, FProperty* Property);

} //namespace