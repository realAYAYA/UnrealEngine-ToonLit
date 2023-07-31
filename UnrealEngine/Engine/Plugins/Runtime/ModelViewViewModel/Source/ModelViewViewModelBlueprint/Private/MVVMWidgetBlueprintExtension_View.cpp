// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVMWidgetBlueprintExtension_View.h"
#include "Blueprint/WidgetTree.h"
#include "MVVMBlueprintView.h"
#include "MVVMViewBlueprintCompiler.h"
#include "MVVMViewModelBase.h"
#include "WidgetBlueprintCompiler.h"
#include "View/MVVMViewClass.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MVVMWidgetBlueprintExtension_View)


void UMVVMWidgetBlueprintExtension_View::CreateBlueprintViewInstance()
{
	BlueprintView = NewObject<UMVVMBlueprintView>(this);
	BlueprintViewChangedDelegate.Broadcast();
}


void UMVVMWidgetBlueprintExtension_View::DestroyBlueprintViewInstance()
{
	BlueprintView = nullptr;
	BlueprintViewChangedDelegate.Broadcast();
}


void UMVVMWidgetBlueprintExtension_View::PostLoad()
{
	Super::PostLoad();
	//for (FMVVMViewBindingEditorData& Data : ViewBindings)
	//{
	//	Data.Resolve();
	//}
}


void UMVVMWidgetBlueprintExtension_View::HandlePreloadObjectsForCompilation(UBlueprint* OwningBlueprint)
{
	if (BlueprintView)
	{
		BlueprintView->ConditionalPostLoad();

		for (const FMVVMBlueprintViewBinding& Bindings : BlueprintView->GetBindings())
		{
			//UBlueprint::ForceLoad(Bindings.ViewModelPath);
			//UBlueprint::ForceLoad(Bindings.WidgetPath);
		}
		for (const FMVVMBlueprintViewModelContext& AvailableViewModel : BlueprintView->GetViewModels())
		{
			if (AvailableViewModel.GetViewModelClass())
			{
				UBlueprint::ForceLoad(AvailableViewModel.GetViewModelClass());
			}
			//UBlueprint::ForceLoad(ViewModelPropertyPath);
		}
	}
}


void UMVVMWidgetBlueprintExtension_View::HandleBeginCompilation(FWidgetBlueprintCompilerContext& InCreationContext)
{
	CurrentCompilerContext.Reset();
	if (BlueprintView)
	{
		for (FMVVMBlueprintViewBinding& Binding : BlueprintView->GetBindings())
		{
			Binding.Errors.Reset();
		}

		CurrentCompilerContext = MakePimpl<UE::MVVM::Private::FMVVMViewBlueprintCompiler>(InCreationContext);
	}
}


void UMVVMWidgetBlueprintExtension_View::HandleEndCompilation()
{
	CurrentCompilerContext.Reset();
}


void UMVVMWidgetBlueprintExtension_View::HandleCleanAndSanitizeClass(UWidgetBlueprintGeneratedClass* ClassToClean, UObject* OldCDO)
{
	Super::HandleCleanAndSanitizeClass(ClassToClean, OldCDO);

	if (CurrentCompilerContext)
	{
		CurrentCompilerContext->CleanOldData(ClassToClean, OldCDO);
	}
}


void UMVVMWidgetBlueprintExtension_View::HandleCreateClassVariablesFromBlueprint(const FWidgetBlueprintCompilerContext::FCreateVariableContext& Context)
{
	Super::HandleCreateClassVariablesFromBlueprint(Context);

	CurrentCompilerContext->CreateVariables(Context, GetBlueprintView());
}


void UMVVMWidgetBlueprintExtension_View::HandleCreateFunctionList()
{
	Super::HandleCreateFunctionList();

	if (CurrentCompilerContext)
	{
		CurrentCompilerContext->CreateFunctions(BlueprintView);
	}
}


void UMVVMWidgetBlueprintExtension_View::HandleFinishCompilingClass(UWidgetBlueprintGeneratedClass* Class)
{
	Super::HandleFinishCompilingClass(Class);

	check(CurrentCompilerContext);

	if (CurrentCompilerContext->GetCompilerContext().bIsFullCompile)
	{
		UMVVMViewClass* ViewExtension = nullptr;
		bool bCompiled = false;
		if (CurrentCompilerContext->PreCompile(Class, BlueprintView))
		{
			ViewExtension = NewObject<UMVVMViewClass>(Class);
			bCompiled = CurrentCompilerContext->Compile(Class, BlueprintView, ViewExtension);
		}

		CurrentCompilerContext->CleanTemporaries(Class);

		if (bCompiled)
		{
			check(ViewExtension);
			CurrentCompilerContext->AddExtension(Class, ViewExtension);
		}
	}
	else
	{
		CurrentCompilerContext->CleanTemporaries(Class);
	}
}

