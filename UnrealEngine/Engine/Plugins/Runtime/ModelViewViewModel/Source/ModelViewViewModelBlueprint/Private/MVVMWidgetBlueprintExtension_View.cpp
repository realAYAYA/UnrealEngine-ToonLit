// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVMWidgetBlueprintExtension_View.h"
#include "MVVMBlueprintView.h"
#include "MVVMBlueprintViewConversionFunction.h"
#include "MVVMViewBlueprintCompiler.h"
#include "View/MVVMViewClass.h"

#include "FindInBlueprintManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MVVMWidgetBlueprintExtension_View)

#define LOCTEXT_NAMESPACE "MVVMBlueprintExtensionView"

namespace UE::MVVM::Private
{
	bool GAllowViewClass = true;
	static FAutoConsoleVariableRef CVarAllowViewClass(
		TEXT("MVVM.AllowViewClass"),
		GAllowViewClass,
		TEXT("Is the model view viewmodel view is allowed to be added to the generated Widget GeneratedClass."),
		ECVF_ReadOnly
	);
}

void UMVVMWidgetBlueprintExtension_View::CreateBlueprintViewInstance()
{
	BlueprintView = NewObject<UMVVMBlueprintView>(this, FName(), RF_Transactional);
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
}


void UMVVMWidgetBlueprintExtension_View::HandlePreloadObjectsForCompilation(UBlueprint* OwningBlueprint)
{
	if (IsInGameThread() && BlueprintView)
	{
		BlueprintView->ConditionalPostLoad();

		for (const FMVVMBlueprintViewModelContext& AvailableViewModel : BlueprintView->GetViewModels())
		{
			if (AvailableViewModel.GetViewModelClass())
			{
				UBlueprint::ForceLoad(AvailableViewModel.GetViewModelClass());
			}
		}
		for (FMVVMBlueprintViewBinding& Binding : BlueprintView->GetBindings())
		{
			if (Binding.Conversion.DestinationToSourceConversion)
			{
				UBlueprint::ForceLoad(Binding.Conversion.DestinationToSourceConversion);
			}
			if (Binding.Conversion.SourceToDestinationConversion)
			{
				UBlueprint::ForceLoad(Binding.Conversion.SourceToDestinationConversion);
			}
		}
	}
}


void UMVVMWidgetBlueprintExtension_View::HandleBeginCompilation(FWidgetBlueprintCompilerContext& InCreationContext)
{
	CurrentCompilerContext.Reset();
	if (BlueprintView)
	{
		BlueprintView->ResetBindingMessages();
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

		if (bCompiled && UE::MVVM::Private::GAllowViewClass)
		{
			check(ViewExtension);
			// Does it have any bindings
			if (const_cast<const UMVVMViewClass*>(ViewExtension)->GetCompiledBindings().Num() > 0)
			{
				// Test if parent also has a view
				if (Class->GetExtension<UMVVMViewClass>(true))
				{
					CurrentCompilerContext->GetCompilerContext().MessageLog.Warning(*LOCTEXT("MoreThanOneViewWarning", "There is more than one view.").ToString());
				}

				CurrentCompilerContext->AddExtension(Class, ViewExtension);
			}
		}
		else if (bCompiled)
		{
			// If we are not allowed to add the view class, add the transient flags on added conversion graph.
			for (TFieldIterator<UFunction> FunctionIter(Class, EFieldIteratorFlags::ExcludeSuper); FunctionIter; ++FunctionIter)
			{
				UFunction* Function = *FunctionIter;
				Function->SetFlags(RF_Transient);
			}
		}
	}
	else
	{
		CurrentCompilerContext->CleanTemporaries(Class);
	}
}

UMVVMWidgetBlueprintExtension_View::FSearchData UMVVMWidgetBlueprintExtension_View::HandleGatherSearchData(const UBlueprint* OwningBlueprint) const
{
	UMVVMWidgetBlueprintExtension_View::FSearchData SearchData;
	if (GetBlueprintView())
	{
		{
			TUniquePtr<FSearchArrayData> ViewModelContextSearchData = MakeUnique<FSearchArrayData>();
			ViewModelContextSearchData->Identifier = LOCTEXT("ViewmodelSearchTag", "Viewmodels");
			for (const FMVVMBlueprintViewModelContext& ViewModelContext : GetBlueprintView()->GetViewModels())
			{
				FSearchData& ViewModelSearchData = ViewModelContextSearchData->SearchSubList.AddDefaulted_GetRef();
				ViewModelSearchData.Datas.Emplace(LOCTEXT("ViewmodelGuidSearchTag", "Guid"), FText::FromString(ViewModelContext.GetViewModelId().ToString(EGuidFormats::Digits)));
				ViewModelSearchData.Datas.Emplace(FFindInBlueprintSearchTags::FiB_Name, ViewModelContext.GetDisplayName());
				ViewModelSearchData.Datas.Emplace(FFindInBlueprintSearchTags::FiB_ClassName, ViewModelContext.GetViewModelClass() ? ViewModelContext.GetViewModelClass()->GetDisplayNameText() : FText::GetEmpty());
				ViewModelSearchData.Datas.Emplace(LOCTEXT("ViewmodelCreationTypeSearchTag", "CreationType"), StaticEnum<EMVVMBlueprintViewModelContextCreationType>()->GetDisplayNameTextByValue((int64)ViewModelContext.CreationType));
			}
			SearchData.SearchArrayDatas.Add(MoveTemp(ViewModelContextSearchData));
		}
		{
			TUniquePtr<FSearchArrayData> BindingContextSearchData = MakeUnique<FSearchArrayData>();
			BindingContextSearchData->Identifier = LOCTEXT("BindingSearchTag", "Bindings");
			for (const FMVVMBlueprintViewBinding& Binding : GetBlueprintView()->GetBindings())
			{
				FSearchData& BindingSearchData = BindingContextSearchData->SearchSubList.AddDefaulted_GetRef();
				BindingSearchData.Datas.Emplace(LOCTEXT("ViewBindingSearchTag", "Binding"), FText::FromString(Binding.GetDisplayNameString(GetWidgetBlueprint())));
			}
			SearchData.SearchArrayDatas.Add(MoveTemp(BindingContextSearchData));
		}
	}
	return SearchData;
}

void UMVVMWidgetBlueprintExtension_View::SetFilterSettings(FMVVMViewBindingFilterSettings InFilterSettings)
{
	FilterSettings = InFilterSettings;
}

#if WITH_EDITORONLY_DATA
void UMVVMWidgetBlueprintExtension_View::PostInitProperties()
{
	Super::PostInitProperties();

	if (!IsTemplate())
	{
		SetFilterSettings(GetDefault<UMVVMDeveloperProjectSettings>()->FilterSettings);
	}
}
#endif

#undef LOCTEXT_NAMESPACE
