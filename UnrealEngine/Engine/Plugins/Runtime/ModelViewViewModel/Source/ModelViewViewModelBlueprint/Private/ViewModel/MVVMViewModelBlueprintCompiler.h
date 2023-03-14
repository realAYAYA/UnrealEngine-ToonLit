// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EngineLogs.h"
#include "Engine/Blueprint.h"
#include "WidgetBlueprint.h"
#include "KismetCompiler.h"
#include "KismetCompilerModule.h"

class UEdGraph;
class UMVVMViewModelBlueprint;
class UMVVMViewModelBlueprintGeneratedClass;

//////////////////////////////////////////////////////////////////////////
// FViewModelBlueprintCompiler 

namespace UE::MVVM
{

class FViewModelBlueprintCompiler : public IBlueprintCompiler
{
public:
	bool CanCompile(const UBlueprint* Blueprint) override;
	void Compile(UBlueprint* Blueprint, const FKismetCompilerOptions& CompileOptions, FCompilerResultsLog& Results) override;
	bool GetBlueprintTypesForClass(UClass* ParentClass, UClass*& OutBlueprintClass, UClass*& OutBlueprintGeneratedClass) const override;
};

} //namespace


//////////////////////////////////////////////////////////////////////////
// FViewModelBlueprintCompilerContext

namespace UE::MVVM
{

class FViewModelBlueprintCompilerContext : public FKismetCompilerContext
{
	typedef FKismetCompilerContext Super;

public:
	using FKismetCompilerContext::FKismetCompilerContext;

private:
	UMVVMViewModelBlueprint* GetViewModelBlueprint() const;
	FProperty* FindPropertyByNameOnNewClass(FName PropertyName) const;

	//~ Begin FKismetCompilerContext Interface
	virtual void SaveSubObjectsFromCleanAndSanitizeClass(FSubobjectCollection& SubObjectsToSave, UBlueprintGeneratedClass* ClassToClean) override;
	virtual void CleanAndSanitizeClass(UBlueprintGeneratedClass* ClassToClean, UObject*& InOutOldCDO) override;
	virtual void CreateFunctionList() override;
	virtual void CreateClassVariablesFromBlueprint() override;
	virtual void SpawnNewClass(const FString& NewClassName) override;
	virtual void OnNewClassSet(UBlueprintGeneratedClass* ClassToUse) override;
	virtual void PostcompileFunction(FKismetFunctionContext& Context) override;
	virtual void FinishCompilingClass(UClass* Class) override;
	//~ End FKismetCompilerContext Interface

private:
	struct FGeneratedFunction
	{
		FName PropertyName;
		UEdGraph* SetterFunction = nullptr;
		UEdGraph* NetRepFunction = nullptr;
		FProperty* Property = nullptr;
		FProperty* SkelProperty = nullptr;
	};

	UMVVMViewModelBlueprintGeneratedClass* NewViewModelBlueprintClass;
	TArray<FGeneratedFunction> GeneratedFunctions;
};

} //namespace

