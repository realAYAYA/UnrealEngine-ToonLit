// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "CoreTypes.h"
#include "Modules/ModuleInterface.h"

class FBlueprintCompileReinstancer;
class FCompilerResultsLog;
class UBlueprint;
class UBlueprintGeneratedClass;
class UClass;
class UUserDefinedEnum;
class UUserDefinedStruct;
struct FKismetCompilerOptions;

// @todo: BP2CPP_remove
PRAGMA_DISABLE_DEPRECATION_WARNINGS
struct FCompilerNativizationOptions;

PRAGMA_ENABLE_DEPRECATION_WARNINGS

#define KISMET_COMPILER_MODULENAME "KismetCompiler"

//////////////////////////////////////////////////////////////////////////
// IKismetCompilerInterface

class IBlueprintCompiler
{
public:
	virtual void PreCompile(UBlueprint* Blueprint, const FKismetCompilerOptions& CompileOptions)
	{
		PreCompile(Blueprint);
	}

	virtual bool CanCompile(const UBlueprint* Blueprint) = 0;
	virtual void Compile(UBlueprint* Blueprint, const FKismetCompilerOptions& CompileOptions, FCompilerResultsLog& Results) = 0;
	
	virtual void PostCompile(UBlueprint* Blueprint, const FKismetCompilerOptions& CompileOptions)
	{
		PostCompile(Blueprint);
	}

	virtual bool GetBlueprintTypesForClass(UClass* ParentClass, UClass*& OutBlueprintClass, UClass*& OutBlueprintGeneratedClass) const
	{
		OutBlueprintClass = nullptr;
		OutBlueprintGeneratedClass = nullptr;
		return false;
	}

protected:
	virtual void PreCompile(UBlueprint* Blueprint) { }
	virtual void PostCompile(UBlueprint* Blueprint) { }
};

class IKismetCompilerInterface : public IModuleInterface
{
public:
	/**
	 * Synchronizes Blueprint's GeneratedClass's properties with the NewVariable declarations in the blueprint
	 * Used on load to ensure that all properties are present for instances.
	 * 
	 * @param Blueprint The blueprint that may be missing variables
	 */
	virtual void RefreshVariables(UBlueprint* Blueprint)=0;

	/**
	 * Compiles a user defined structure.
	 *
	 * @param	Struct		The structure to compile.
	 * @param	Results  	The results log for warnings and errors.
	 */
	virtual void CompileStructure(class UUserDefinedStruct* Struct, FCompilerResultsLog& Results)=0;

	/**
	 * Attempts to recover a corrupted blueprint package.
	 *
	 * @param	Blueprint	The blueprint to recover.
	 */
	virtual void RecoverCorruptedBlueprint(class UBlueprint* Blueprint)=0;

	/**
	 * Clears the blueprint's generated classes, and consigns them to oblivion
	 *
	 * @param	Blueprint	The blueprint to clear the classes for
	 */
	virtual void RemoveBlueprintGeneratedClasses(class UBlueprint* Blueprint)=0;

	/**
	 * Gets a list of all compilers for blueprints.  You can register new compilers through this list.
	 */
	virtual TArray<IBlueprintCompiler*>& GetCompilers() = 0;

	/**
	 * Get the blueprint class and generated blueprint class for a particular class type.  Not every
	 * blueprint is a normal UBlueprint, like UUserWidget blueprints should be UWidgetBlueprints.
	 */
	virtual void GetBlueprintTypesForClass(UClass* ParentClass, UClass*& OutBlueprintClass, UClass*& OutBlueprintGeneratedClass) const = 0;

	// @todo: BP2CPP_remove
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UE_DEPRECATED(5.0, "This API is no longer required. Any overrides should be removed.")
	virtual void GenerateCppCodeForEnum(UUserDefinedEnum* UDEnum, const FCompilerNativizationOptions& NativizationOptions, FString& OutHeaderCode, FString& OutCPPCode) {}
	UE_DEPRECATED(5.0, "This API is no longer required. Any overrides should be removed.")
	virtual void GenerateCppCodeForStruct(UUserDefinedStruct* UDStruct, const FCompilerNativizationOptions& NativizationOptions, FString& OutHeaderCode, FString& OutCPPCode) {}
	// Generate a wrapper class, that helps accessing non-native properties and calling non-native functions
	UE_DEPRECATED(5.0, "This API is no longer required. Any overrides should be removed.")
	virtual FString GenerateCppWrapper(UBlueprintGeneratedClass* BPGC, const FCompilerNativizationOptions& NativizationOptions) { return FString(); }
PRAGMA_ENABLE_DEPRECATION_WARNINGS
};


