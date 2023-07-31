// Copyright Epic Games, Inc. All Rights Reserved.

#include "BlueprintCompilerExtension.h"

class FKismetCompilerContext;

UBlueprintCompilerExtension::UBlueprintCompilerExtension(const FObjectInitializer& ObjectInitializer)
	: UObject(ObjectInitializer)
{
}

void UBlueprintCompilerExtension::BlueprintCompiled(const FKismetCompilerContext& CompilationContext, const FBlueprintCompiledData& Data)
{
	// common entry point in case we need to add logging, profiling, etc
	ProcessBlueprintCompiled(CompilationContext, Data);
}
