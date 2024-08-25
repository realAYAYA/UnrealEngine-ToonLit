// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EngineLogs.h"
#include "Engine/Blueprint.h"
#include "WidgetBlueprint.h"
#include "KismetCompiler.h"
#include "KismetCompilerModule.h"
#include "ComponentReregisterContext.h"
#include "Components/WidgetComponent.h"

class FProperty;
class UEdGraph;
class UWidget;
class UWidgetAnimation;
class UWidgetGraphSchema;

//////////////////////////////////////////////////////////////////////////
// FWidgetBlueprintCompiler 

class UMGEDITOR_API FWidgetBlueprintCompiler : public IBlueprintCompiler
{

public:
	FWidgetBlueprintCompiler();

	bool CanCompile(const UBlueprint* Blueprint) override;
	void PreCompile(UBlueprint* Blueprint, const FKismetCompilerOptions& CompileOptions) override;
	void Compile(UBlueprint* Blueprint, const FKismetCompilerOptions& CompileOptions, FCompilerResultsLog& Results) override;
	void PostCompile(UBlueprint* Blueprint, const FKismetCompilerOptions& CompileOptions) override;
	bool GetBlueprintTypesForClass(UClass* ParentClass, UClass*& OutBlueprintClass, UClass*& OutBlueprintGeneratedClass) const override;

private:

	/** The temporary variable that captures and reinstances components after compiling finishes. */
	TComponentReregisterContext<UWidgetComponent>* ReRegister;

	/**
	* The current count on the number of compiles that have occurred.  We don't want to re-register components until all
	* compiling has stopped.
	*/
	int32 CompileCount;

};

//////////////////////////////////////////////////////////////////////////
// FWidgetBlueprintCompilerContext


class UMGEDITOR_API FWidgetBlueprintCompilerContext : public FKismetCompilerContext
{
protected:
	typedef FKismetCompilerContext Super;

public:
	FWidgetBlueprintCompilerContext(UWidgetBlueprint* SourceSketch, FCompilerResultsLog& InMessageLog, const FKismetCompilerOptions& InCompilerOptions);
	virtual ~FWidgetBlueprintCompilerContext();

protected:
	void ValidateWidgetNames();

	/**
	 * Checks if the animations' bindings are valid. Shows a compiler warning if any of the animations have a null track, urging the user
	 * to delete said track to avoid performance hitches when playing said null tracks in large user widgets.
	*/
	void ValidateWidgetAnimations();

	/** Validates the Desired Focus name to make sure it's part of the Widget Tree. */
	void ValidateDesiredFocusWidgetName();

	//~ Begin FKismetCompilerContext
	virtual UEdGraphSchema_K2* CreateSchema() override;
	virtual void CreateFunctionList() override;
	virtual void SpawnNewClass(const FString& NewClassName) override;
	virtual void OnNewClassSet(UBlueprintGeneratedClass* ClassToUse) override;
	virtual void PrecompileFunction(FKismetFunctionContext& Context, EInternalCompilerFlags InternalFlags) override;
	virtual void CleanAndSanitizeClass(UBlueprintGeneratedClass* ClassToClean, UObject*& InOutOldCDO) override;
	virtual void SaveSubObjectsFromCleanAndSanitizeClass(FSubobjectCollection& SubObjectsToSave, UBlueprintGeneratedClass* ClassToClean) override;
	virtual void EnsureProperGeneratedClass(UClass*& TargetClass) override;
	virtual void CreateClassVariablesFromBlueprint() override;
	virtual void CopyTermDefaultsToDefaultObject(UObject* DefaultObject);
	virtual void FinishCompilingClass(UClass* Class) override;
	virtual bool ValidateGeneratedClass(UBlueprintGeneratedClass* Class) override;
	virtual void OnPostCDOCompiled(const UObject::FPostCDOCompiledContext& Context) override;
	//~ End FKismetCompilerContext

	void SanitizeBindings(UBlueprintGeneratedClass* Class);

	void VerifyEventReplysAreNotEmpty(FKismetFunctionContext& Context);
	void VerifyFieldNotifyFunction(FKismetFunctionContext& Context);

public:
	UWidgetBlueprint* WidgetBlueprint() const { return Cast<UWidgetBlueprint>(Blueprint); }

	void AddExtension(UWidgetBlueprintGeneratedClass* Class, UWidgetBlueprintGeneratedClassExtension* Extension);

	struct UMGEDITOR_API FCreateVariableContext
	{
	public:
		FProperty* CreateVariable(const FName Name, const FEdGraphPinType& Type) const;
		void AddGeneratedFunctionGraph(UEdGraph* Graph) const;
		UWidgetBlueprint* GetWidgetBlueprint() const;
		UE_DEPRECATED(5.4, "GetSkeletonGeneratedClass renamed to GetGeneratedClass")
		UWidgetBlueprintGeneratedClass* GetSkeletonGeneratedClass() const;
		UWidgetBlueprintGeneratedClass* GetGeneratedClass() const;
		EKismetCompileType::Type GetCompileType() const;

	private:
		friend FWidgetBlueprintCompilerContext;
		FCreateVariableContext(FWidgetBlueprintCompilerContext& InContext);
		FWidgetBlueprintCompilerContext& Context;
	};

	struct UMGEDITOR_API FCreateFunctionContext
	{
	public:
		void AddGeneratedFunctionGraph(UEdGraph*) const;
		UWidgetBlueprintGeneratedClass* GetGeneratedClass() const;

	private:
		friend FWidgetBlueprintCompilerContext;
		FCreateFunctionContext(FWidgetBlueprintCompilerContext& InContext);
		FWidgetBlueprintCompilerContext& Context;
	};

protected:
	void FixAbandonedWidgetTree(UWidgetBlueprint* WidgetBP);

	UWidgetBlueprintGeneratedClass* NewWidgetBlueprintClass;

	UWidgetTree* OldWidgetTree;

	TArray<UWidgetAnimation*> OldWidgetAnimations;

	UWidgetGraphSchema* WidgetSchema;

	// Map of properties created for widgets; to aid in debug data generation
	TMap<UWidget*, FProperty*> WidgetToMemberVariableMap;

	// Map of properties created in parent widget for bind widget validation
	TMap<UWidget*, FProperty*> ParentWidgetToBindWidgetMap;

	// Map of properties created for widget animations; to aid in debug data generation
	TMap<UWidgetAnimation*, FProperty*> WidgetAnimToMemberVariableMap;

	///----------------------------------------------------------------
};

