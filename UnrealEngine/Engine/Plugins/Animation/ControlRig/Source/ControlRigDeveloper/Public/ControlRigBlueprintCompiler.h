// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMBlueprintCompiler.h"

class CONTROLRIGDEVELOPER_API FControlRigBlueprintCompiler : public FRigVMBlueprintCompiler
{
public:
	/** IBlueprintCompiler interface */
	virtual bool CanCompile(const UBlueprint* Blueprint) override;
	virtual void Compile(UBlueprint* Blueprint, const FKismetCompilerOptions& CompileOptions, FCompilerResultsLog& Results) override;
};

class CONTROLRIGDEVELOPER_API FControlRigBlueprintCompilerContext : public FRigVMBlueprintCompilerContext
{
public:
	FControlRigBlueprintCompilerContext(UBlueprint* SourceSketch, FCompilerResultsLog& InMessageLog, const FKismetCompilerOptions& InCompilerOptions)
		: FRigVMBlueprintCompilerContext(SourceSketch, InMessageLog, InCompilerOptions)
	{
	}

	// FKismetCompilerContext interface
	virtual void SpawnNewClass(const FString& NewClassName) override;
	virtual void CopyTermDefaultsToDefaultObject(UObject* DefaultObject) override;
};