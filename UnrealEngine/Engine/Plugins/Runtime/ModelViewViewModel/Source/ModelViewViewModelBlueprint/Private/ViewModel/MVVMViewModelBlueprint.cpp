// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewModel/MVVMViewModelBlueprint.h"
#include "ViewModel/MVVMViewModelBlueprintCompiler.h"
#include "ViewModel/MVVMViewModelBlueprintGeneratedClass.h"

#include "Kismet2/CompilerResultsLog.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MVVMViewModelBlueprint)


TSharedPtr<FKismetCompilerContext> UMVVMViewModelBlueprint::GetCompilerForViewModelBlueprint(UBlueprint* BP, FCompilerResultsLog& InMessageLog, const FKismetCompilerOptions& InCompileOptions)
{
	return TSharedPtr<FKismetCompilerContext>(new UE::MVVM::FViewModelBlueprintCompilerContext(CastChecked<UMVVMViewModelBlueprint>(BP), InMessageLog, InCompileOptions));
}


UMVVMViewModelBlueprintGeneratedClass* UMVVMViewModelBlueprint::GetViewModelBlueprintGeneratedClass() const
{
	return Cast<UMVVMViewModelBlueprintGeneratedClass>(*GeneratedClass);
}


UMVVMViewModelBlueprintGeneratedClass* UMVVMViewModelBlueprint::GetViewModelBlueprintSkeletonClass() const
{
	return Cast<UMVVMViewModelBlueprintGeneratedClass>(*SkeletonGeneratedClass);
}


bool UMVVMViewModelBlueprint::SupportsMacros() const
{
	return false;
}


bool UMVVMViewModelBlueprint::SupportsEventGraphs() const
{
	return false;
}


bool UMVVMViewModelBlueprint::SupportsDelegates() const
{
	return false;
}


bool UMVVMViewModelBlueprint::SupportsAnimLayers() const
{
	return false;
}

