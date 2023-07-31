// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimBlueprintCompilerCreationContext.h"
#include "AnimBlueprintCompiler.h"
#include "EdGraph/EdGraphSchema.h"

void FAnimBlueprintCompilerCreationContext::RegisterKnownGraphSchema(TSubclassOf<UEdGraphSchema> InGraphSchemaClass)
{
	CompilerContext->KnownGraphSchemas.AddUnique(InGraphSchemaClass);
}

