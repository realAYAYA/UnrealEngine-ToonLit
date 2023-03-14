// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	KismetCompilerModule.cpp
=============================================================================*/

#include "KismetCompilerModule.h"
#include "Stats/Stats.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Class.h"
#include "Engine/Blueprint.h"
#include "Stats/StatsMisc.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectIterator.h"
#include "UObject/MetaData.h"
#include "Animation/AnimBlueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Kismet2/CompilerResultsLog.h"
#include "KismetCompilerMisc.h"
#include "KismetCompiler.h"

#include "Kismet2/KismetDebugUtilities.h"
#include "Kismet2/KismetReinstanceUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"

#include "UserDefinedStructureCompilerUtils.h"
#include "Engine/UserDefinedStruct.h"
#include "IMessageLogListing.h"
#include "Engine/Engine.h"

DEFINE_LOG_CATEGORY(LogK2Compiler);
DECLARE_CYCLE_STAT(TEXT("Compile Time"), EKismetCompilerStats_CompileTime, STATGROUP_KismetCompiler);
DECLARE_CYCLE_STAT(TEXT("Compile Skeleton Class"), EKismetCompilerStats_CompileSkeletonClass, STATGROUP_KismetCompiler);
DECLARE_CYCLE_STAT(TEXT("Compile Generated Class"), EKismetCompilerStats_CompileGeneratedClass, STATGROUP_KismetCompiler);

#define LOCTEXT_NAMESPACE "KismetCompiler"

//////////////////////////////////////////////////////////////////////////
// FKismet2CompilerModule - The Kismet 2 Compiler module
class FKismet2CompilerModule : public IKismetCompilerInterface
{
public:
	// Implementation of the IKismetCompilerInterface
	virtual void RefreshVariables(UBlueprint* Blueprint) override final;
	virtual void CompileStructure(class UUserDefinedStruct* Struct, FCompilerResultsLog& Results) override;
	virtual void RecoverCorruptedBlueprint(class UBlueprint* Blueprint) override;
	virtual void RemoveBlueprintGeneratedClasses(class UBlueprint* Blueprint) override;
	virtual TArray<IBlueprintCompiler*>& GetCompilers() override { return Compilers; }
	virtual void GetBlueprintTypesForClass(UClass* ParentClass, UClass*& OutBlueprintClass, UClass*& OutBlueprintGeneratedClass) const override;
	// End implementation
private:

	TArray<IBlueprintCompiler*> Compilers;
};

IMPLEMENT_MODULE( FKismet2CompilerModule, KismetCompiler );

struct FBlueprintIsBeingCompiledHelper
{
private:
	UBlueprint* Blueprint;
public:
	FBlueprintIsBeingCompiledHelper(UBlueprint* InBlueprint) : Blueprint(InBlueprint)
	{
		check(NULL != Blueprint);
		check(!Blueprint->bBeingCompiled);
		Blueprint->bBeingCompiled = true;
	}

	~FBlueprintIsBeingCompiledHelper()
	{
		Blueprint->bBeingCompiled = false;
	}
};

// Compiles a blueprint.

void FKismet2CompilerModule::CompileStructure(UUserDefinedStruct* Struct, FCompilerResultsLog& Results)
{
	Results.SetSourcePath(Struct->GetPathName());
	BP_SCOPED_COMPILER_EVENT_STAT(EKismetCompilerStats_CompileTime);
	FUserDefinedStructureCompilerUtils::CompileStruct(Struct, Results, true);
}

extern UNREALED_API FSecondsCounterData BlueprintCompileAndLoadTimerData;

void FKismet2CompilerModule::RefreshVariables(UBlueprint* Blueprint)
{
#if 0 // disabled because this is a costly ensure:
	//  ensure that nothing is using this class cause that would be super bad:
	TArray<UObject*> Instances;
	GetObjectsOfClass(*(Blueprint->GeneratedClass), Instances);
	ensure(Instances.Num() == 0 || Instances.Num() == 1);
#endif

	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	FCompilerResultsLog MessageLog;

	bool bMissingProperty = false;
	TArray<int32> MissingVariables;

	for (int32 VarIndex = 0; VarIndex < Blueprint->NewVariables.Num(); ++VarIndex)
	{
		FProperty* ExistingVariable = Blueprint->GeneratedClass->FindPropertyByName(Blueprint->NewVariables[VarIndex].VarName);
		if( ExistingVariable == nullptr || ExistingVariable->GetOwner<UObject>() != Blueprint->GeneratedClass)
		{
			MissingVariables.Add(VarIndex);
		}
	}

	if(MissingVariables.Num() != 0)
	{
		UObject* OldCDO = Blueprint->GeneratedClass->ClassDefaultObject;
		auto Reinstancer = FBlueprintCompileReinstancer::Create(*Blueprint->GeneratedClass);
		ensure(OldCDO->GetClass() != *Blueprint->GeneratedClass);
		// move old cdo aside:
		if(OldCDO)
		{
			OldCDO->Rename(NULL, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional | REN_ForceNoResetLoaders);
		}
		Blueprint->GeneratedClass->ClassDefaultObject = nullptr;

		// add missing properties:
		for( int32 MissingVarIndex : MissingVariables)
		{
			FProperty* NewProperty = FKismetCompilerUtilities::CreatePropertyOnScope(
				Blueprint->GeneratedClass, 
				Blueprint->NewVariables[MissingVarIndex].VarName, 
				Blueprint->NewVariables[MissingVarIndex].VarType, 
				Blueprint->GeneratedClass, 
				CPF_None,
				K2Schema,
				MessageLog);

			if(NewProperty)
			{
				NewProperty->PropertyLinkNext = Blueprint->GeneratedClass->PropertyLink;
				Blueprint->GeneratedClass->PropertyLink = NewProperty;
			}
		}
		
		Blueprint->GeneratedClass->Bind();
		Blueprint->GeneratedClass->StaticLink(true);
		// regenerate CDO:
		Blueprint->GeneratedClass->GetDefaultObject();

		// cpfuo:
		UEngine::FCopyPropertiesForUnrelatedObjectsParams Params;
		Params.bNotifyObjectReplacement = true;
		UEngine::CopyPropertiesForUnrelatedObjects(OldCDO, Blueprint->GeneratedClass->ClassDefaultObject, Params);
	}
}

// Recover a corrupted blueprint
void FKismet2CompilerModule::RecoverCorruptedBlueprint(class UBlueprint* Blueprint)
{
	UPackage* Package = Blueprint->GetOutermost();

	// Get rid of any stale classes
	for (FThreadSafeObjectIterator ObjIt; ObjIt; ++ObjIt)
	{
		UObject* TestObject = *ObjIt;
		if (TestObject->GetOuter() == Package)
		{
			// This object is in the blueprint package; is it expected?
			if (UClass* TestClass = Cast<UClass>(TestObject))
			{
				if ((TestClass != Blueprint->SkeletonGeneratedClass) && (TestClass != Blueprint->GeneratedClass))
				{
					// Unexpected UClass
					FKismetCompilerUtilities::ConsignToOblivion(TestClass, false);
				}
			}
		}
	}

	CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS );
}

void FKismet2CompilerModule::RemoveBlueprintGeneratedClasses(class UBlueprint* Blueprint)
{
	if (Blueprint != NULL)
	{
		if (Blueprint->GeneratedClass != NULL)
		{
			FKismetCompilerUtilities::ConsignToOblivion(Blueprint->GeneratedClass, Blueprint->bIsRegeneratingOnLoad);
			Blueprint->GeneratedClass = NULL;
		}

		if (Blueprint->SkeletonGeneratedClass != NULL)
		{
			FKismetCompilerUtilities::ConsignToOblivion(Blueprint->SkeletonGeneratedClass, Blueprint->bIsRegeneratingOnLoad);
			Blueprint->SkeletonGeneratedClass = NULL;
		}
	}
}

void FKismet2CompilerModule::GetBlueprintTypesForClass(UClass* ParentClass, UClass*& OutBlueprintClass, UClass*& OutBlueprintGeneratedClass) const
{
	for ( IBlueprintCompiler* Compiler : Compilers )
	{
		if ( Compiler->GetBlueprintTypesForClass(ParentClass, OutBlueprintClass, OutBlueprintGeneratedClass) )
		{
			return;
		}
	}

	OutBlueprintClass = UBlueprint::StaticClass();
	OutBlueprintGeneratedClass = UBlueprintGeneratedClass::StaticClass();
}

#undef LOCTEXT_NAMESPACE
