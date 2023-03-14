// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVMViewBlueprintCompiler.h"
#include "Blueprint/WidgetTree.h"
#include "Bindings/MVVMBindingHelper.h"
#include "Bindings/MVVMCompiledBindingLibraryCompiler.h"
#include "Bindings/MVVMConversionFunctionHelper.h"
#include "Bindings/MVVMFieldPathHelper.h"
#include "EdGraphSchema_K2.h"
#include "MVVMBlueprintView.h"
#include "MVVMFunctionGraphHelper.h"
#include "MVVMViewModelBase.h"
#include "MVVMWidgetBlueprintExtension_View.h"
#include "WidgetBlueprintCompiler.h"
#include "View/MVVMViewClass.h"

#define LOCTEXT_NAMESPACE "MVVMViewBlueprintCompiler"

namespace UE::MVVM::Private
{
FString PropertyPathToString(const UMVVMBlueprintView* BlueprintView, const FMVVMBlueprintPropertyPath& PropertyPath)
{
	if (PropertyPath.IsEmpty())
	{
		return FString();
	}

	TStringBuilder<512> Result;
	if (PropertyPath.IsFromViewModel())
	{
		if (const FMVVMBlueprintViewModelContext* SourceViewModelContext = BlueprintView->FindViewModel(PropertyPath.GetViewModelId()))
		{
			Result << SourceViewModelContext->GetViewModelName();
		}
		else
		{
			Result << TEXT("<none>");
		}
	}
	else if (PropertyPath.IsFromWidget())
	{
		Result << PropertyPath.GetWidgetName();
	}
	else
	{
		Result << TEXT("<none>");
	}

	FString BasePropertyPath = PropertyPath.GetBasePropertyPath();
	if (BasePropertyPath.Len())
	{
		Result << TEXT('.');
		Result << MoveTemp(BasePropertyPath);
	}
	return Result.ToString();
}

FText PropertyPathToText(const UMVVMBlueprintView* BlueprintView, const FMVVMBlueprintPropertyPath& PropertyPath)
{
	return FText::FromString(PropertyPathToString(BlueprintView, PropertyPath));
}

FText GetViewModelIdText(const FMVVMBlueprintPropertyPath& PropertyPath)
{
	return FText::FromString(PropertyPath.GetViewModelId().ToString(EGuidFormats::DigitsWithHyphensInBraces));
}

void FMVVMViewBlueprintCompiler::AddErrorForBinding(FMVVMBlueprintViewBinding& Binding, const FText& Message, FName ArgumentName) const
{
	const FText BindingName = FText::FromString(Binding.GetDisplayNameString(WidgetBlueprintCompilerContext.WidgetBlueprint()));

	FText FormattedError;
	if (!ArgumentName.IsNone())
	{
		FormattedError = FText::Format(LOCTEXT("BindingFormatWithArgument", "Binding '{0}': Argument '{1}' - {2}"), BindingName, FText::FromName(ArgumentName), Message);
	}
	else
	{
		FormattedError = FText::Format(LOCTEXT("BindingFormat", "Binding '{0}': {1}"), BindingName, Message);
	}

	WidgetBlueprintCompilerContext.MessageLog.Error(*FormattedError.ToString());
	Binding.Errors.Add(FormattedError);
}

void FMVVMViewBlueprintCompiler::AddErrorForViewModel(const FMVVMBlueprintViewModelContext& ViewModel, const FText& Message) const
{
	const FText FormattedError = FText::Format(LOCTEXT("ViewModelFormat", "Viewodel '{0}': {1}"), ViewModel.GetDisplayName(), Message);
	WidgetBlueprintCompilerContext.MessageLog.Error(*FormattedError.ToString());
}

void FMVVMViewBlueprintCompiler::AddExtension(UWidgetBlueprintGeneratedClass* Class, UMVVMViewClass* ViewExtension)
{
	WidgetBlueprintCompilerContext.AddExtension(Class, ViewExtension);
}


void FMVVMViewBlueprintCompiler::CleanOldData(UWidgetBlueprintGeneratedClass* ClassToClean, UObject* OldCDO)
{
	// Clean old View
	if (!WidgetBlueprintCompilerContext.Blueprint->bIsRegeneratingOnLoad && WidgetBlueprintCompilerContext.bIsFullCompile)
	{
		auto RenameObjectToTransientPackage = [](UObject* ObjectToRename)
		{
			const ERenameFlags RenFlags = REN_DoNotDirty | REN_ForceNoResetLoaders | REN_DontCreateRedirectors;

			ObjectToRename->Rename(nullptr, GetTransientPackage(), RenFlags);
			ObjectToRename->SetFlags(RF_Transient);
			ObjectToRename->ClearFlags(RF_Public | RF_Standalone | RF_ArchetypeObject);
			FLinkerLoad::InvalidateExport(ObjectToRename);
		};

		TArray<UObject*> Children;
		const bool bIncludeNestedObjects = false;
		ForEachObjectWithOuter(ClassToClean, [&Children](UObject* Child)
			{
				if (Cast<UMVVMViewClass>(Child))
				{
					Children.Add(Child);
				}
			}, bIncludeNestedObjects);		

		for (UObject* Child : Children)
		{
			RenameObjectToTransientPackage(Child);
		}
	}
}


void FMVVMViewBlueprintCompiler::CleanTemporaries(UWidgetBlueprintGeneratedClass* ClassToClean)
{
}


void FMVVMViewBlueprintCompiler::CreateFunctions(UMVVMBlueprintView* BlueprintView)
{
	if (!bAreSourcesCreatorValid || !bIsBindingsValid)
	{
		return;
	}

	for (const FCompilerSourceCreatorContext& SourceCreator : CompilerSourceCreatorContexts)
	{
		if (SourceCreator.SetterGraph)
		{
			if (!UE::MVVM::FunctionGraphHelper::GenerateViewModelSetter(WidgetBlueprintCompilerContext, SourceCreator.SetterGraph, SourceCreator.ViewModelContext.GetViewModelName()))
			{
				AddErrorForViewModel(SourceCreator.ViewModelContext, LOCTEXT("SetterFunctionCouldNotBeGenerated", "The setter function could not be generated."));
				continue;
			}
		}
	}
}


void FMVVMViewBlueprintCompiler::CreateVariables(const FWidgetBlueprintCompilerContext::FCreateVariableContext& Context, UMVVMBlueprintView* BlueprintView)
{
	if (!BlueprintView)
	{
		return;
	}

	if (!bAreSourcesCreatorValid || !bAreSourceContextsValid || !bIsBindingsValid)
	{
		return;
	}

	if (Context.GetCompileType() == EKismetCompileType::SkeletonOnly)
	{
		CreateWidgetMap(Context, BlueprintView);
		CreateSourceLists(Context, BlueprintView);
		CreateFunctionsDeclaration(Context, BlueprintView);
	}

	auto CreateVariable = [&Context](const FCompilerSourceContext& SourceContext) -> FProperty*
	{
		FEdGraphPinType ViewModelPinType(UEdGraphSchema_K2::PC_Object, NAME_None, SourceContext.Class, EPinContainerType::None, false, FEdGraphTerminalType());
		FProperty* ViewModelProperty = Context.CreateVariable(SourceContext.PropertyName, ViewModelPinType);
		if (ViewModelProperty != nullptr)
		{
			ViewModelProperty->SetPropertyFlags(CPF_BlueprintVisible | CPF_BlueprintReadOnly | CPF_RepSkip
				| CPF_Transient | CPF_DuplicateTransient);
			ViewModelProperty->SetPropertyFlags(SourceContext.bExposeOnSpawn ? CPF_ExposeOnSpawn : CPF_DisableEditOnInstance);

#if WITH_EDITOR
			if (!SourceContext.BlueprintSetter.IsEmpty())
			{
				ViewModelProperty->SetMetaData(FBlueprintMetadata::MD_PropertySetFunction, *SourceContext.BlueprintSetter);
			}
			if (!SourceContext.DisplayName.IsEmpty())
			{
				ViewModelProperty->SetMetaData(FBlueprintMetadata::MD_FunctionCategory, *SourceContext.DisplayName.ToString());
			}
			if (!SourceContext.CategoryName.IsEmpty())
			{
				ViewModelProperty->SetMetaData(FBlueprintMetadata::MD_FunctionCategory, *SourceContext.CategoryName);
			}
			if (SourceContext.bExposeOnSpawn)
			{
				ViewModelProperty->SetMetaData(FBlueprintMetadata::MD_ExposeOnSpawn, TEXT("true"));
			}
#endif
		}
		return ViewModelProperty;
	};

	for (FCompilerSourceContext& SourceContext : CompilerSourceContexts)
	{
		SourceContext.Field = BindingHelper::FindFieldByName(Context.GetSkeletonGeneratedClass(), FMVVMBindingName(SourceContext.PropertyName));

		// The class is not linked yet. It may not be available yet.
		if (SourceContext.Field.IsEmpty())
		{
			for (FField* Field = Context.GetSkeletonGeneratedClass()->ChildProperties; Field != nullptr; Field = Field->Next)
			{
				if (Field->GetFName() == SourceContext.PropertyName)
				{
					if (FProperty* Property = CastField<FProperty>(Field))
					{
						SourceContext.Field = FMVVMFieldVariant(Property);
						break;
					}
					else
					{
						WidgetBlueprintCompilerContext.MessageLog.Error(*FText::Format(LOCTEXT("FieldIsNotProperty", "The field for source '{0}' exists but is not a property."), SourceContext.DisplayName).ToString());
						bAreSourcesCreatorValid = false;
						continue;
					}
				}
			}
		}

		// Reuse the property if found
		if (!SourceContext.Field.IsEmpty())
		{
			if (!BindingHelper::IsValidForSourceBinding(SourceContext.Field))
			{
				WidgetBlueprintCompilerContext.MessageLog.Error(*FText::Format(LOCTEXT("FieldNotAccessibleAtRuntime", "The field for source '{0}' exists but is not accessible at runtime."), SourceContext.DisplayName).ToString());
				bAreSourcesCreatorValid = false;
				continue;
			}

			const FProperty* Property = SourceContext.Field.IsProperty() ? SourceContext.Field.GetProperty() : BindingHelper::GetReturnProperty(SourceContext.Field.GetFunction());
			const FObjectPropertyBase* ObjectProperty = CastField<const FObjectPropertyBase>(Property);
			const bool bIsCompatible = ObjectProperty && SourceContext.Class->IsChildOf(ObjectProperty->PropertyClass);
			if (!bIsCompatible)
			{
				WidgetBlueprintCompilerContext.MessageLog.Error(*FText::Format(LOCTEXT("PropertyExistsAndNotCompatible","There is already a property named '{0}' that is not compatible with the source of the same name."), SourceContext.DisplayName).ToString());
				bAreSourceContextsValid = false;
				continue;
			}
		}

		if (SourceContext.Field.IsEmpty())
		{
			SourceContext.Field = FMVVMConstFieldVariant(CreateVariable(SourceContext));
		}

		if (SourceContext.Field.IsEmpty())
		{
			WidgetBlueprintCompilerContext.MessageLog.Error(*FText::Format(LOCTEXT("VariableCouldNotBeCreated", "The variable for '{0}' could not be created."), SourceContext.DisplayName).ToString());
			bAreSourceContextsValid = false;
			continue;
		}
	}
}


void FMVVMViewBlueprintCompiler::CreateWidgetMap(const FWidgetBlueprintCompilerContext::FCreateVariableContext& Context, UMVVMBlueprintView* BlueprintView)
{
	// The widget tree is not created yet for SKEL class.
	//Context.GetGeneratedClass()->GetWidgetTreeArchetype()
	WidgetNameToWidgetPointerMap.Reset();

	TArray<UWidget*> Widgets;
	UWidgetBlueprint* WidgetBPToScan = Context.GetWidgetBlueprint();
	while (WidgetBPToScan != nullptr)
	{
		Widgets = WidgetBPToScan->GetAllSourceWidgets();
		if (Widgets.Num() != 0)
		{
			break;
		}
		WidgetBPToScan = WidgetBPToScan->ParentClass && WidgetBPToScan->ParentClass->ClassGeneratedBy ? Cast<UWidgetBlueprint>(WidgetBPToScan->ParentClass->ClassGeneratedBy) : nullptr;
	}

	for (UWidget* Widget : Widgets)
	{
		WidgetNameToWidgetPointerMap.Add(Widget->GetFName(), Widget);
	}
}


void FMVVMViewBlueprintCompiler::CreateSourceLists(const FWidgetBlueprintCompilerContext::FCreateVariableContext& Context, UMVVMBlueprintView* BlueprintView)
{
	CompilerSourceContexts.Reset();
	CompilerSourceCreatorContexts.Reset();

	TSet<FGuid> ViewModelGuids;
	TSet<FName> WidgetSources;
	for (const FMVVMBlueprintViewModelContext& ViewModelContext : BlueprintView->GetViewModels())
	{
		if (!ViewModelContext.GetViewModelId().IsValid())
		{
			AddErrorForViewModel(ViewModelContext, LOCTEXT("ViewmodelInvalidGuid", "GUID is invalid."));
			bAreSourcesCreatorValid = false;
			continue;
		}

		if (ViewModelGuids.Contains(ViewModelContext.GetViewModelId()))
		{
			AddErrorForViewModel(ViewModelContext, LOCTEXT("ViewmodelAlreadyAdded", "Identical viewmodel has already been added."));
			bAreSourcesCreatorValid = false;
			continue;
		}

		ViewModelGuids.Add(ViewModelContext.GetViewModelId());

		if (ViewModelContext.GetViewModelClass() == nullptr || !ViewModelContext.IsValid())
		{
			AddErrorForViewModel(ViewModelContext, LOCTEXT("ViewmodelInvalidClass", "Invalid class."));
			bAreSourcesCreatorValid = false;
			continue;
		}

		const bool bCreateSetterFunction = ViewModelContext.bCreateSetterFunction || ViewModelContext.CreationType == EMVVMBlueprintViewModelContextCreationType::Manual;

		int32 FoundSourceCreatorContextIndex = INDEX_NONE;
		if (Context.GetCompileType() == EKismetCompileType::SkeletonOnly)
		{
			FCompilerSourceCreatorContext SourceContext;
			SourceContext.ViewModelContext = ViewModelContext;
			SourceContext.Type = ECompilerSourceCreatorType::ViewModel;
			if (bCreateSetterFunction)
			{
				SourceContext.SetterFunctionName = TEXT("Set") + ViewModelContext.GetViewModelName().ToString();
			}
			FoundSourceCreatorContextIndex = CompilerSourceCreatorContexts.Emplace(MoveTemp(SourceContext));
		}
		else
		{
			FGuid ViewModelId = ViewModelContext.GetViewModelId();
			FoundSourceCreatorContextIndex = CompilerSourceCreatorContexts.IndexOfByPredicate([ViewModelId](const FCompilerSourceCreatorContext& Other)
				{
					return Other.ViewModelContext.GetViewModelId() == ViewModelId;
				});
		}
		checkf(FoundSourceCreatorContextIndex != INDEX_NONE, TEXT("The viewmodel was added after the skeleton was created?"));

		FCompilerSourceContext SourceVariable;
		SourceVariable.Class = ViewModelContext.GetViewModelClass();
		SourceVariable.PropertyName = ViewModelContext.GetViewModelName();
		SourceVariable.DisplayName = ViewModelContext.GetDisplayName();
		SourceVariable.CategoryName = TEXT("Viewmodel");
		SourceVariable.bExposeOnSpawn = bCreateSetterFunction;
		SourceVariable.BlueprintSetter = CompilerSourceCreatorContexts[FoundSourceCreatorContextIndex].SetterFunctionName;
		CompilerSourceContexts.Emplace(MoveTemp(SourceVariable));
	}

	bAreSourceContextsValid = bAreSourcesCreatorValid;

	UWidgetBlueprintGeneratedClass* SkeletonClass = Context.GetSkeletonGeneratedClass();

	// Only find the source first property and destination first property.
	//The full path will be tested later. We want to build the list of property needed.
	for (int32 Index = 0; Index < BlueprintView->GetNumBindings(); ++Index)
	{
		FMVVMBlueprintViewBinding* BindingPtr = BlueprintView->GetBindingAt(Index);
		if (BindingPtr == nullptr)
		{
			WidgetBlueprintCompilerContext.MessageLog.Error(*FText::Format(LOCTEXT("BindingInvalidIndex", "Internal error: Tried to fetch binding for invalid index {0}."), Index).ToString());
			bAreSourceContextsValid = false;
			continue;
		}

		FMVVMBlueprintViewBinding& Binding = *BindingPtr;
		if (!Binding.bCompile)
		{
			continue;
		}

		FMVVMViewBlueprintCompiler* Self = this;
		auto GenerateCompilerSourceContext = [Self, BlueprintView, Class = SkeletonClass, &Binding, &ViewModelGuids, &WidgetSources](const FMVVMBlueprintPropertyPath& PropertyPath, bool bViewModelPath, FName ArgumentName = FName()) -> bool
		{
			if (PropertyPath.IsFromWidget())
			{
				if (bViewModelPath)
				{
					Self->AddErrorForBinding(Binding,
							FText::Format(LOCTEXT("ExpectedViewModelPath", "Expected a viewmodel path, but received a path from widget: {0}"), 
								FText::FromName(PropertyPath.GetWidgetName())
							)
						);
				}

				if (PropertyPath.GetWidgetName() == Class->ClassGeneratedBy->GetFName())
				{
					// it's the userwidget
					return true;
				}

				// If the widget doesn't have a property, add one automatically.
				if (!WidgetSources.Contains(PropertyPath.GetWidgetName()))
				{
					WidgetSources.Add(PropertyPath.GetWidgetName());

					UWidget** WidgetPtr = Self->WidgetNameToWidgetPointerMap.Find(PropertyPath.GetWidgetName());
					if (WidgetPtr == nullptr || *WidgetPtr == nullptr)
					{
						Self->AddErrorForBinding(Binding,
							FText::Format(LOCTEXT("InvalidWidgetFormat", "Could not find the targeted widget: {0}"), 
								FText::FromName(PropertyPath.GetWidgetName())
							),
							ArgumentName
						);
						return false;
					}

					UWidget* Widget = *WidgetPtr;
					FCompilerSourceContext SourceVariable;
					SourceVariable.Class = Widget->GetClass();
					SourceVariable.PropertyName = PropertyPath.GetWidgetName();
					SourceVariable.DisplayName = FText::FromString(Widget->GetDisplayLabel());
					SourceVariable.CategoryName = TEXT("Widget");
					Self->CompilerSourceContexts.Emplace(MoveTemp(SourceVariable));
				}
			}
			else if (PropertyPath.IsFromViewModel())
			{
				const FMVVMBlueprintViewModelContext* SourceViewModelContext = BlueprintView->FindViewModel(PropertyPath.GetViewModelId());
				if (SourceViewModelContext == nullptr)
				{
					Self->AddErrorForBinding(Binding,
						FText::Format(LOCTEXT("BindingViewModelNotFound", "Could not find viewmodel with GUID {0}."), GetViewModelIdText(PropertyPath)),
						ArgumentName
					);
					return false;
				}

				if (!bViewModelPath)
				{
					Self->AddErrorForBinding(Binding,
							FText::Format(LOCTEXT("ExpectedWidgetPath", "Expected a widget path, but received a path from viewmodel: {0}"), 
								SourceViewModelContext->GetDisplayName()
							),
							ArgumentName
						);
				}

				if (!ViewModelGuids.Contains(SourceViewModelContext->GetViewModelId()))
				{
					Self->AddErrorForBinding(Binding,
						FText::Format(LOCTEXT("BindingViewModelInvalid", "Viewmodel {0} {1} was invalid."), 
							SourceViewModelContext->GetDisplayName(),
							GetViewModelIdText(PropertyPath)
						),
						ArgumentName
					);
					return false;
				}
			}
			else
			{
				Self->AddErrorForBinding(Binding,
					bViewModelPath ? LOCTEXT("ViewModelPathNotSet", "A viewmodel path is required, but not set.") :
							LOCTEXT("WidgetPathNotSet", "A widget path is required, but not set."),
					ArgumentName
				);
				return false;
			}
			return true;
		};

		const bool bIsForwardBinding = IsForwardBinding(Binding.BindingType);
		const bool bIsBackwardBinding = IsBackwardBinding(Binding.BindingType);

		if (bIsForwardBinding || bIsBackwardBinding)
		{
			TMap<FName, FMVVMBlueprintPropertyPath> ConversionFunctionArguments = ConversionFunctionHelper::GetAllArgumentPropertyPaths(WidgetBlueprintCompilerContext.WidgetBlueprint(), Binding, bIsForwardBinding, true);
			if (ConversionFunctionArguments.Num() > 0)
			{
				if (Binding.BindingType == EMVVMBindingMode::TwoWay)
				{
					Self->AddErrorForBinding(Binding, LOCTEXT("TwoWayBindingsWithConversion", "Two-way bindings are not allowed to use conversion functions."));
					bAreSourceContextsValid = false;
					continue;
				}

				// generate sources for conversion function arguments
				for (const TPair<FName, FMVVMBlueprintPropertyPath>& Arg : ConversionFunctionArguments)
				{
					if (bIsForwardBinding)
					{
						bAreSourceContextsValid &= GenerateCompilerSourceContext(Arg.Value, true, Arg.Key);
					}
					else
					{
						bAreSourceContextsValid &= GenerateCompilerSourceContext(Arg.Value, false, Arg.Key);
					}
				}

				// generate destination source
				if (bIsForwardBinding)
				{
					bAreSourceContextsValid &= GenerateCompilerSourceContext(Binding.WidgetPath, false);
				}
				else
				{
					bAreSourceContextsValid &= GenerateCompilerSourceContext(Binding.ViewModelPath, true);
				}
			}
			else
			{
				// if we aren't using a conversion function, just validate the widget and viewmodel paths
				bAreSourceContextsValid &= GenerateCompilerSourceContext(Binding.WidgetPath, false);
				bAreSourceContextsValid &= GenerateCompilerSourceContext(Binding.ViewModelPath, true);
			}
		}
	}
}


void FMVVMViewBlueprintCompiler::CreateFunctionsDeclaration(const FWidgetBlueprintCompilerContext::FCreateVariableContext& Context, UMVVMBlueprintView* BlueprintView)
{
	// Clean all previous intermediate function graph. It should stay alive. The graph lives on the Blueprint not on the class and it's used to generate the UFunction.
	{
		auto RenameObjectToTransientPackage = [](UObject* ObjectToRename)
		{
			const ERenameFlags RenFlags = REN_DontCreateRedirectors | REN_NonTransactional | REN_DoNotDirty;
			ObjectToRename->Rename(nullptr, GetTransientPackage(), RenFlags);
			ObjectToRename->SetFlags(RF_Transient);
			ObjectToRename->ClearFlags(RF_Public | RF_Standalone | RF_ArchetypeObject);
			FLinkerLoad::InvalidateExport(ObjectToRename);
		};

		for (UEdGraph* OldGraph : BlueprintView->TemporaryGraph)
		{
			if (OldGraph)
			{
				RenameObjectToTransientPackage(OldGraph);
			}
		}
		BlueprintView->TemporaryGraph.Reset();
	}

	for (FCompilerSourceCreatorContext& SourceCreator : CompilerSourceCreatorContexts)
	{
		if (!SourceCreator.SetterFunctionName.IsEmpty() && SourceCreator.Type == ECompilerSourceCreatorType::ViewModel)
		{
			ensure(SourceCreator.SetterGraph == nullptr);

			SourceCreator.SetterGraph = UE::MVVM::FunctionGraphHelper::CreateIntermediateFunctionGraph(
				WidgetBlueprintCompilerContext
				, SourceCreator.SetterFunctionName
				, (FUNC_BlueprintCallable | FUNC_Public)
				, TEXT("Viewmodel")
				, false);
			BlueprintView->TemporaryGraph.Add(SourceCreator.SetterGraph);

			if (SourceCreator.SetterGraph == nullptr || SourceCreator.SetterGraph->GetFName() != FName(*SourceCreator.SetterFunctionName))
			{
				WidgetBlueprintCompilerContext.MessageLog.Error(*FText::Format(LOCTEXT("SetterNameAlreadyExists", "The setter name {0} already exists and could not be autogenerated."), 
					FText::FromString(SourceCreator.SetterFunctionName)
					).ToString()
				);
			}

			UE::MVVM::FunctionGraphHelper::AddFunctionArgument(SourceCreator.SetterGraph, SourceCreator.ViewModelContext.GetViewModelClass(), "Viewmodel");
		}
	}
}


bool FMVVMViewBlueprintCompiler::PreCompile(UWidgetBlueprintGeneratedClass* Class, UMVVMBlueprintView* BlueprintView)
{
	if (!bAreSourcesCreatorValid || !bAreSourceContextsValid || !bIsBindingsValid)
	{
		return false;
	}

	const int32 NumBindings = BlueprintView->GetNumBindings();
	CompilerBindings.Reset(NumBindings*2);
	BindingSourceContexts.Reset(NumBindings*2);

	PreCompileBindingSources(Class, BlueprintView);
	PreCompileSourceCreators(Class, BlueprintView);
	PreCompileBindings(Class, BlueprintView);

	return bAreSourcesCreatorValid && bAreSourceContextsValid && bIsBindingsValid;
}


bool FMVVMViewBlueprintCompiler::Compile(UWidgetBlueprintGeneratedClass* Class, UMVVMBlueprintView* BlueprintView, UMVVMViewClass* ViewExtension)
{
	if (!bAreSourcesCreatorValid || !bAreSourceContextsValid || !bIsBindingsValid)
	{
		return false;
	}

	TValueOrError<FCompiledBindingLibraryCompiler::FCompileResult, FText> CompileResult = BindingLibraryCompiler.Compile();
	if (CompileResult.HasError())
	{
		WidgetBlueprintCompilerContext.MessageLog.Error(*FText::Format(LOCTEXT("BindingCompilationFailed", "The binding compilation failed. {1}"), CompileResult.GetError()).ToString());
		return false;
	}
	CompileSourceCreators(CompileResult.GetValue(), Class, BlueprintView, ViewExtension);
	CompileBindings(CompileResult.GetValue(), Class, BlueprintView, ViewExtension);

	bool bResult = bAreSourcesCreatorValid && bAreSourceContextsValid && bIsBindingsValid;
	if (bResult)
	{
		ViewExtension->BindingLibrary = MoveTemp(CompileResult.GetValue().Library);
	}

	return bResult;
}


bool FMVVMViewBlueprintCompiler::PreCompileBindingSources(UWidgetBlueprintGeneratedClass* Class, UMVVMBlueprintView* BlueprintView)
{
	const int32 NumBindings = BlueprintView->GetNumBindings();
	for (int32 Index = 0; Index < NumBindings; ++Index)
	{
		FMVVMBlueprintViewBinding* BindingPtr = BlueprintView->GetBindingAt(Index);
		if (BindingPtr == nullptr)
		{
			WidgetBlueprintCompilerContext.MessageLog.Error(*FText::Format(LOCTEXT("InvalidBindingIndex", "Internal error. Invalid binding index given."), Index).ToString());
			bIsBindingsValid = false;
			continue;
		}

		FMVVMBlueprintViewBinding& Binding = *BindingPtr;
		if (!Binding.bCompile)
		{
			continue;
		}

		auto CreateSourceContextForPropertyPath = [this, &Binding, BlueprintView, Class, Index](const FMVVMBlueprintPropertyPath& Path, bool bForwardBinding, FName ArgumentName = FName()) -> bool
		{
			const TValueOrError<FBindingSourceContext, FText> CreatedBindingSourceContext = CreateBindingSourceContext(BlueprintView, Class, Path);
			if (CreatedBindingSourceContext.HasError())
			{
				AddErrorForBinding(Binding, 
					FText::Format(LOCTEXT("PropertyPathInvalidWithReason", "The property path '{0}' is invalid. {1}"),
						PropertyPathToText(BlueprintView, Binding.ViewModelPath),
						CreatedBindingSourceContext.GetError()
					),
					ArgumentName
				);
				return false;
			}

			FBindingSourceContext BindingSourceContext = CreatedBindingSourceContext.GetValue();
			if (!IsPropertyPathValid(BindingSourceContext.PropertyPath))
			{
				AddErrorForBinding(Binding, 
					FText::Format(LOCTEXT("PropertyPathIsInvalid", "The property path '{0}' is invalid."), 
						PropertyPathToText(BlueprintView, Binding.ViewModelPath)
					),
					ArgumentName
				);
				return false;
			}

			if (BindingSourceContext.SourceClass == nullptr)
			{
				AddErrorForBinding(Binding, LOCTEXT("BindingInvalidSourceClass", "Internal error. The binding could not find its source class."), ArgumentName);
				return false;
			}

			if (!BindingSourceContext.bIsRootWidget && BindingSourceContext.CompilerSourceContextIndex == INDEX_NONE)
			{
				AddErrorForBinding(Binding, LOCTEXT("BindingInvalidSource", "Internal error. The binding could not find its source."), ArgumentName);
				return false;
			}

			BindingSourceContext.BindingIndex = Index;
			BindingSourceContext.bIsForwardBinding = bForwardBinding;

			this->BindingSourceContexts.Add(MoveTemp(BindingSourceContext));
			return true;
		};

		auto CreateSourcesForConversionFunction = [this, &Binding, &CreateSourceContextForPropertyPath](bool bForwardBinding)
		{
			TMap<FName, FMVVMBlueprintPropertyPath> ArgumentPaths = ConversionFunctionHelper::GetAllArgumentPropertyPaths(WidgetBlueprintCompilerContext.WidgetBlueprint(), Binding, bForwardBinding, true);
			for (const TPair<FName, FMVVMBlueprintPropertyPath>& Pair : ArgumentPaths)
			{
				CreateSourceContextForPropertyPath(Pair.Value, bForwardBinding, Pair.Key);
			}
		};

		if (IsForwardBinding(Binding.BindingType))
		{
			CreateSourcesForConversionFunction(true);

			if (!Binding.ViewModelPath.IsEmpty())
			{
				if (!CreateSourceContextForPropertyPath(Binding.ViewModelPath, true))
				{
					bIsBindingsValid = false;
					continue;
				}
			}
		}

		if (IsBackwardBinding(Binding.BindingType))
		{
			CreateSourcesForConversionFunction(false);

			if (!Binding.WidgetPath.IsEmpty())
			{
				if (!CreateSourceContextForPropertyPath(Binding.WidgetPath, false))
				{
					bIsBindingsValid = false;
					continue;
				}
			}
		}
	}

	return bIsBindingsValid;
}


bool FMVVMViewBlueprintCompiler::PreCompileSourceCreators(UWidgetBlueprintGeneratedClass* Class, UMVVMBlueprintView* BlueprintView)
{
	if (!bAreSourcesCreatorValid)
	{
		return false;
	}

	for (FCompilerSourceCreatorContext& SourceCreatorContext : CompilerSourceCreatorContexts)
	{
		FMVVMViewClass_SourceCreator CompiledSourceCreator;

		if (SourceCreatorContext.Type == ECompilerSourceCreatorType::ViewModel)
		{
			const FMVVMBlueprintViewModelContext& ViewModelContext = SourceCreatorContext.ViewModelContext;
			checkf(ViewModelContext.GetViewModelClass(), TEXT("The viewmodel class is invalid. It was checked in CreateSourceList"));

			if (ViewModelContext.GetViewModelClass()->HasAllClassFlags(CLASS_Deprecated))
			{
				AddErrorForViewModel(ViewModelContext, FText::Format(LOCTEXT("ViewModelTypeDeprecated", "Viewmodel type '{0}' is deprecated and should not be used. Please update it in the View Models panel."),
					ViewModelContext.GetViewModelClass()->GetDisplayNameText()
				));
			}

			if (ViewModelContext.CreationType == EMVVMBlueprintViewModelContextCreationType::Manual)
			{
			}
			else if (ViewModelContext.CreationType == EMVVMBlueprintViewModelContextCreationType::CreateInstance)
			{
				if (ViewModelContext.GetViewModelClass()->HasAllClassFlags(CLASS_Abstract))
				{
					AddErrorForViewModel(ViewModelContext, FText::Format(LOCTEXT("ViewModelTypeAbstract", "Viewmodel type '{0}' is abstract and can't be created. You can change it in the View Models panel."),
						ViewModelContext.GetViewModelClass()->GetDisplayNameText()
					));
					bAreSourcesCreatorValid = false;
					continue;
				}
			}
			else if (ViewModelContext.CreationType == EMVVMBlueprintViewModelContextCreationType::PropertyPath)
			{
				if (ViewModelContext.ViewModelPropertyPath.IsEmpty())
				{
					AddErrorForViewModel(ViewModelContext, LOCTEXT("ViewModelInvalidGetter", "Viewmodel has an invalid Getter. You can select a new one in the View Models panel."));
					bAreSourcesCreatorValid = true;
					continue;
				}

				// Generate a path to read the value at runtime
				static const FText InvalidGetterFormat = LOCTEXT("ViewModelInvalidGetterWithReason", "Viewmodel has an invalid Getter. {0}");

				TValueOrError<TArray<FMVVMConstFieldVariant>, FText> GeneratedField = FieldPathHelper::GenerateFieldPathList(Class, ViewModelContext.ViewModelPropertyPath, true);
				if (GeneratedField.HasError())
				{
					AddErrorForViewModel(ViewModelContext, FText::Format(InvalidGetterFormat, GeneratedField.GetError()));
					bAreSourcesCreatorValid = false;
					continue;
				}

				TValueOrError<FCompiledBindingLibraryCompiler::FFieldPathHandle, FText> ReadFieldPathResult = BindingLibraryCompiler.AddObjectFieldPath(GeneratedField.GetValue(), ViewModelContext.GetViewModelClass(), true);
				if (ReadFieldPathResult.HasError())
				{
					AddErrorForViewModel(ViewModelContext, FText::Format(InvalidGetterFormat	, ReadFieldPathResult.GetError()));
					bAreSourcesCreatorValid = false;
					continue;
				}

				SourceCreatorContext.ReadPropertyPath = ReadFieldPathResult.StealValue();
			}
			else if (ViewModelContext.CreationType == EMVVMBlueprintViewModelContextCreationType::GlobalViewModelCollection)
			{
				if (ViewModelContext.GlobalViewModelIdentifier.IsNone())
				{
					AddErrorForViewModel(ViewModelContext, LOCTEXT("ViewmodelInvalidGlobalIdentifier", "Viewmodel doesn't have a valid Global identifier. You can specify a new one in the Viewmodels panel."));
					bAreSourcesCreatorValid = false;
					continue;
				}
			}
			else
			{
				AddErrorForViewModel(ViewModelContext, LOCTEXT("ViewmodelInvalidCreationType", "Viewmodel doesn't have a valid creation type. You can select one in the Viewmodels panel."));
				bAreSourcesCreatorValid = false;
				continue;
			}
		}
	}

	return bAreSourcesCreatorValid;
}


bool FMVVMViewBlueprintCompiler::CompileSourceCreators(const FCompiledBindingLibraryCompiler::FCompileResult& CompileResult, UWidgetBlueprintGeneratedClass* Class, UMVVMBlueprintView* BlueprintView, UMVVMViewClass* ViewExtension)
{
	if (!bAreSourcesCreatorValid)
	{
		return false;
	}

	for (const FCompilerSourceCreatorContext& SourceCreatorContext : CompilerSourceCreatorContexts)
	{
		if (SourceCreatorContext.Type == ECompilerSourceCreatorType::ViewModel)
		{
			const FMVVMBlueprintViewModelContext& ViewModelContext = SourceCreatorContext.ViewModelContext;

			FMVVMViewClass_SourceCreator CompiledSourceCreator;

			if (ViewModelContext.CreationType == EMVVMBlueprintViewModelContextCreationType::Manual)
			{
				CompiledSourceCreator = FMVVMViewClass_SourceCreator::MakeManual(ViewModelContext.GetViewModelName(), ViewModelContext.GetViewModelClass());
			}
			else if (ViewModelContext.CreationType == EMVVMBlueprintViewModelContextCreationType::CreateInstance)
			{
				CompiledSourceCreator = FMVVMViewClass_SourceCreator::MakeInstance(ViewModelContext.GetViewModelName(), ViewModelContext.GetViewModelClass());
			}
			else if (ViewModelContext.CreationType == EMVVMBlueprintViewModelContextCreationType::PropertyPath)
			{
				const FMVVMVCompiledFieldPath* CompiledFieldPath = CompileResult.FieldPaths.Find(SourceCreatorContext.ReadPropertyPath);
				if (CompiledFieldPath == nullptr)
				{
					AddErrorForViewModel(ViewModelContext, LOCTEXT("ViewModelInvalidInitializationBindingNotGenerated", "The viewmodel initialization binding was not generated."));
					bAreSourcesCreatorValid = false;
					continue;
				}
				CompiledSourceCreator = FMVVMViewClass_SourceCreator::MakeFieldPath(ViewModelContext.GetViewModelName(), ViewModelContext.GetViewModelClass(), *CompiledFieldPath, ViewModelContext.bOptional);
			}
			else if (ViewModelContext.CreationType == EMVVMBlueprintViewModelContextCreationType::GlobalViewModelCollection)
			{
				if (ViewModelContext.GlobalViewModelIdentifier.IsNone())
				{
					AddErrorForViewModel(ViewModelContext, LOCTEXT("ViewModelInvalidGlobalIdentifier", "The viewmodel doesn't have a valid Global identifier. You can specify a new one in the Viewmodels panel."));
					bAreSourcesCreatorValid = false;
					continue;
				}

				FMVVMViewModelContext GlobalViewModelInstance;
				GlobalViewModelInstance.ContextClass = ViewModelContext.GetViewModelClass();
				GlobalViewModelInstance.ContextName = ViewModelContext.GlobalViewModelIdentifier;
				if (!GlobalViewModelInstance.IsValid())
				{
					AddErrorForViewModel(ViewModelContext, LOCTEXT("ViewModelCouldNotBeCreated", "The context for viewmodel could not be created. You can change the viewmodel in the Viewmodels panel."));
					bAreSourcesCreatorValid = false;
					continue;
				}

				CompiledSourceCreator = FMVVMViewClass_SourceCreator::MakeGlobalContext(ViewModelContext.GetViewModelName(), MoveTemp(GlobalViewModelInstance), ViewModelContext.bOptional);
			}
			else
			{
				AddErrorForViewModel(ViewModelContext, LOCTEXT("ViewModelWithoutValidCreationType", "The viewmodel doesn't have a valid creation type."));
				bAreSourcesCreatorValid = false;
				continue;
			}

			ViewExtension->SourceCreators.Add(MoveTemp(CompiledSourceCreator));
		}
	}

	return bAreSourcesCreatorValid;
}


bool FMVVMViewBlueprintCompiler::PreCompileBindings(UWidgetBlueprintGeneratedClass* Class, UMVVMBlueprintView* BlueprintView)
{
	if (!bAreSourceContextsValid || !bIsBindingsValid)
	{
		return false;
	}

	for (const FBindingSourceContext& BindingSourceContext : BindingSourceContexts)
	{
		FMVVMBlueprintViewBinding* BindingPtr = BlueprintView->GetBindingAt(BindingSourceContext.BindingIndex);
		check(BindingPtr);
		FMVVMBlueprintViewBinding& Binding = *BindingPtr;

		FMVVMViewBlueprintCompiler* Self = this;
		auto AddBinding = [Self](UWidgetBlueprintGeneratedClass* Class, TArrayView<const UE::MVVM::FMVVMConstFieldVariant> GetterFields, TArrayView<const UE::MVVM::FMVVMConstFieldVariant> SetterFields, const UFunction* ConversionFunction) -> TValueOrError<FCompilerBinding, FText>
		{
			FCompilerBinding Result;

			if (GetterFields.Num() > 0)
			{
				static const FText CouldNotCreateSourceFieldPathFormat = LOCTEXT("CouldNotCreateSourceFieldPath", "Couldn't create the source field path '{0}'. {1}");


				// Generate a path to read the value at runtime
				TValueOrError<TArray<FMVVMConstFieldVariant>, FText> GeneratedField = FieldPathHelper::GenerateFieldPathList(GetterFields, true);
				if (GeneratedField.HasError())
				{
					return MakeError(FText::Format(CouldNotCreateSourceFieldPathFormat
						, ::UE::MVVM::FieldPathHelper::ToText(GetterFields)
						, GeneratedField.GetError()));
				}

				TValueOrError<FCompiledBindingLibraryCompiler::FFieldPathHandle, FText> FieldPathResult = Self->BindingLibraryCompiler.AddFieldPath(GeneratedField.GetValue(), true);
				if (FieldPathResult.HasError())
				{
					return MakeError(FText::Format(CouldNotCreateSourceFieldPathFormat
						, ::UE::MVVM::FieldPathHelper::ToText(GetterFields)
						, FieldPathResult.GetError()));
				}
				Result.SourceRead = FieldPathResult.StealValue();
			}

			static const FText CouldNotCreateDestinationFieldPathFormat = LOCTEXT("CouldNotCreateDestinationFieldPath", "Couldn't create the destination field path '{0}'. {1}");

			{
				TValueOrError<TArray<FMVVMConstFieldVariant>, FText> GeneratedField = FieldPathHelper::GenerateFieldPathList(SetterFields, false);
				if (GeneratedField.HasError())
				{
					return MakeError(FText::Format(CouldNotCreateDestinationFieldPathFormat
						, ::UE::MVVM::FieldPathHelper::ToText(SetterFields)
						, GeneratedField.GetError()));
				}

				TValueOrError<FCompiledBindingLibraryCompiler::FFieldPathHandle, FText> FieldPathResult = Self->BindingLibraryCompiler.AddFieldPath(GeneratedField.GetValue(), false);
				if (FieldPathResult.HasError())
				{
					return MakeError(FText::Format(CouldNotCreateDestinationFieldPathFormat
						, ::UE::MVVM::FieldPathHelper::ToText(SetterFields)
						, FieldPathResult.GetError()));
				}
				Result.DestinationWrite = FieldPathResult.StealValue();
			}

			if (ConversionFunction != nullptr)
			{
				TValueOrError<FCompiledBindingLibraryCompiler::FFieldPathHandle, FText> FieldPathResult = Self->BindingLibraryCompiler.AddConversionFunctionFieldPath(Class, ConversionFunction);
				if (FieldPathResult.HasError())
				{
					return MakeError(FText::Format(LOCTEXT("CouldNotCreateConversionFunctionFieldPath", "Couldn't create the conversion function field path '{0}'. {1}")
						, FText::FromString(ConversionFunction->GetPathName())
						, FieldPathResult.GetError()));
				}
				Result.ConversionFunction = FieldPathResult.StealValue();
				Result.bIsConversionFunctionComplex = BindingHelper::IsValidForComplexRuntimeConversion(ConversionFunction);
			}

			// Sanity check.
			if (GetterFields.Num() == 0 && (ConversionFunction == nullptr || !BindingHelper::IsValidForComplexRuntimeConversion(ConversionFunction)))
			{
				return MakeError(LOCTEXT("BindingShouldHaveGetter", "The binding should have a getter."));
			}
			if (GetterFields.Num() != 0 && ConversionFunction == nullptr && BindingHelper::IsValidForComplexRuntimeConversion(ConversionFunction))
			{
				return MakeError(LOCTEXT("BindingHasConversionFunctionAndGetter", "The binding has both a complex conversion function and a getter."));
			}

			// Generate the binding
			TValueOrError<FCompiledBindingLibraryCompiler::FBindingHandle, FText> BindingResult = Self->BindingLibraryCompiler.AddBinding(Result.SourceRead, Result.DestinationWrite, Result.ConversionFunction);
			if (BindingResult.HasError())
			{
				return MakeError(BindingResult.StealError());
			}
			Result.BindingHandle = BindingResult.StealValue();

			return MakeValue(Result);
		};

		auto AddFieldId = [Self](UClass* SourceContextClass, bool bNotifyFieldValueChangedRequired, EMVVMBindingMode BindingMode, FName FieldToListenTo) -> TValueOrError<FCompiledBindingLibraryCompiler::FFieldIdHandle, FText>
		{
			if (!IsOneTimeBinding(BindingMode) && bNotifyFieldValueChangedRequired)
			{
				return Self->BindingLibraryCompiler.AddFieldId(SourceContextClass, FieldToListenTo);
			}
			return MakeValue(FCompiledBindingLibraryCompiler::FFieldIdHandle());
		};

		TValueOrError<FCompiledBindingLibraryCompiler::FFieldIdHandle, FText> AddFieldResult = AddFieldId(BindingSourceContext.SourceClass, true, Binding.BindingType, BindingSourceContext.FieldId.GetFieldName());
		if (AddFieldResult.HasError())
		{
			AddErrorForBinding(Binding, FText::Format(LOCTEXT("CouldNotCreateSource", "Could not create source. {0}"), 
				AddFieldResult.GetError()));
			bIsBindingsValid = false;
			continue;
		}

		TArray<UE::MVVM::FMVVMConstFieldVariant> SetterPath;
		{
			const FMVVMBlueprintPropertyPath& DestinationPath = BindingSourceContext.bIsForwardBinding ? Binding.WidgetPath : Binding.ViewModelPath;
			SetterPath = CreateBindingDestinationPath(BlueprintView, Class, DestinationPath);
			if (!IsPropertyPathValid(SetterPath))
			{
				AddErrorForBinding(Binding, FText::Format(LOCTEXT("PropertyPathIsInvalid", "The property path '{0}' is invalid."), 
					PropertyPathToText(BlueprintView, DestinationPath))
				);
				bIsBindingsValid = false;
				continue;
			}
		}

		FMemberReference ConversionFunctionReference = BindingSourceContext.bIsForwardBinding ? Binding.Conversion.SourceToDestinationFunction : Binding.Conversion.DestinationToSourceFunction;
		FName ConversionFunctionWrapper = BindingSourceContext.bIsForwardBinding ? Binding.Conversion.SourceToDestinationWrapper : Binding.Conversion.DestinationToSourceWrapper;
		if (!ConversionFunctionWrapper.IsNone())
		{
			ConversionFunctionReference.SetSelfMember(ConversionFunctionWrapper);
		}

		const UFunction* ConversionFunction = ConversionFunctionReference.ResolveMember<UFunction>(Class);
		if (!ConversionFunctionWrapper.IsNone() && ConversionFunction == nullptr)
		{
			AddErrorForBinding(Binding, FText::Format(LOCTEXT("ConversionFunctionNotFound", "The conversion function '{0}' could not be found."), 
				FText::FromName(ConversionFunctionReference.GetMemberName()))
			);
			bIsBindingsValid = false;
			continue;
		}

		TValueOrError<FCompilerBinding, FText> AddBindingResult = AddBinding(Class, BindingSourceContext.PropertyPath, SetterPath, ConversionFunction);
		if (AddBindingResult.HasError())
		{
			AddErrorForBinding(Binding, 
				FText::Format(LOCTEXT("CouldNotCreateBinding", "Could not create binding. {0}"), AddBindingResult.GetError())
			);
			bIsBindingsValid = false;
			continue;
		}

		FCompilerBinding NewBinding = AddBindingResult.StealValue();
		NewBinding.BindingIndex = BindingSourceContext.BindingIndex;
		NewBinding.CompilerSourceContextIndex = BindingSourceContext.CompilerSourceContextIndex;
		NewBinding.FieldIdHandle = AddFieldResult.StealValue();
		NewBinding.bSourceIsUserWidget = BindingSourceContext.bIsRootWidget;
		NewBinding.bFieldIdNeeded = !IsOneTimeBinding(Binding.BindingType);
		CompilerBindings.Emplace(NewBinding);
	}

	return bIsBindingsValid;
}


bool FMVVMViewBlueprintCompiler::CompileBindings(const FCompiledBindingLibraryCompiler::FCompileResult& CompileResult, UWidgetBlueprintGeneratedClass* Class, UMVVMBlueprintView* BlueprintView, UMVVMViewClass* ViewExtension)
{
	if (!bIsBindingsValid)
	{
		return false;
	}

	for (const FCompilerBinding& CompileBinding : CompilerBindings)
	{
		FMVVMBlueprintViewBinding& ViewBinding = *BlueprintView->GetBindingAt(CompileBinding.BindingIndex);

		FMVVMViewClass_CompiledBinding NewBinding;

		check(CompileBinding.CompilerSourceContextIndex != INDEX_NONE);
		NewBinding.SourcePropertyName = CompilerSourceContexts[CompileBinding.CompilerSourceContextIndex].PropertyName;

		const FMVVMVCompiledFieldId* CompiledFieldId = CompileResult.FieldIds.Find(CompileBinding.FieldIdHandle);
		if (CompiledFieldId == nullptr && CompileBinding.bFieldIdNeeded)
		{
			AddErrorForBinding(ViewBinding,
				FText::Format(LOCTEXT("FieldIdNotGenerated", "Could not generate field ID for property '{0}'."), FText::FromName(NewBinding.SourcePropertyName))
			);
			bIsBindingsValid = false;
			continue;
		}

		const FMVVMVCompiledBinding* CompiledBinding = CompileResult.Bindings.Find(CompileBinding.BindingHandle);
		if (CompiledBinding == nullptr)
		{
			AddErrorForBinding(ViewBinding, LOCTEXT("CompiledBindingNotGenerated", "Could not generate compiled binding."));
			bIsBindingsValid = false;
			continue;
		}

		bool bIsOptional = false;
		if (CompilerSourceCreatorContexts.IsValidIndex(CompileBinding.CompilerSourceContextIndex))
		{
			const FMVVMBlueprintViewModelContext& ModelContext = CompilerSourceCreatorContexts[CompileBinding.CompilerSourceContextIndex].ViewModelContext;
			if (ModelContext.IsValid())
			{
				bIsOptional = ModelContext.bOptional;
			}
		}

		NewBinding.FieldId = CompiledFieldId  ? *CompiledFieldId : FMVVMVCompiledFieldId();
		NewBinding.Binding = *CompiledBinding;
		NewBinding.UpdateMode = ViewBinding.UpdateMode;

		NewBinding.Flags = 0;
		NewBinding.Flags |= (ViewBinding.bEnabled) ? FMVVMViewClass_CompiledBinding::EBindingFlags::EnabledByDefault : 0;
		NewBinding.Flags |= (IsForwardBinding(ViewBinding.BindingType)) ? FMVVMViewClass_CompiledBinding::EBindingFlags::ForwardBinding : 0;
		NewBinding.Flags |= (ViewBinding.BindingType == EMVVMBindingMode::TwoWay) ? FMVVMViewClass_CompiledBinding::EBindingFlags::TwoWayBinding : 0;
		NewBinding.Flags |= (IsOneTimeBinding(ViewBinding.BindingType)) ? FMVVMViewClass_CompiledBinding::EBindingFlags::OneTime : 0;
		NewBinding.Flags |= (bIsOptional) ? FMVVMViewClass_CompiledBinding::EBindingFlags::ViewModelOptional : 0;
		NewBinding.Flags |= (CompileBinding.bIsConversionFunctionComplex) ? FMVVMViewClass_CompiledBinding::EBindingFlags::ConversionFunctionIsComplex : 0;

		ViewExtension->CompiledBindings.Emplace(MoveTemp(NewBinding));
	}

	return bIsBindingsValid;
}


const FMVVMViewBlueprintCompiler::FCompilerSourceCreatorContext* FMVVMViewBlueprintCompiler::FindViewModelSource(FGuid Id) const
{
	return CompilerSourceCreatorContexts.FindByPredicate([Id](const FCompilerSourceCreatorContext& Other)
		{
			return Other.Type == ECompilerSourceCreatorType::ViewModel ? Other.ViewModelContext.GetViewModelId() == Id : false;
		});
}


TValueOrError<FMVVMViewBlueprintCompiler::FBindingSourceContext, FText> FMVVMViewBlueprintCompiler::CreateBindingSourceContext(const UMVVMBlueprintView* BlueprintView, const UWidgetBlueprintGeneratedClass* Class, const FMVVMBlueprintPropertyPath& PropertyPath)
{
	if (PropertyPath.IsEmpty())
	{
		ensureAlways(false);
		return MakeError(LOCTEXT("EmptyPropertyPath", "Empty property path found. This is legal."));
	}

	FBindingSourceContext Result;
	if (PropertyPath.IsFromViewModel())
	{
		Result.bIsRootWidget = false;

		const FMVVMBlueprintViewModelContext* SourceViewModelContext = BlueprintView->FindViewModel(PropertyPath.GetViewModelId());
		check(SourceViewModelContext);
		const FName SourceName = SourceViewModelContext->GetViewModelName();
		Result.CompilerSourceContextIndex = CompilerSourceContexts.IndexOfByPredicate([SourceName](const FCompilerSourceContext& Other) { return Other.PropertyName == SourceName; });
		check(Result.CompilerSourceContextIndex != INDEX_NONE);

		Result.SourceClass = CompilerSourceContexts[Result.CompilerSourceContextIndex].Class;
		Result.PropertyPath = CreatePropertyPath(Class, CompilerSourceContexts[Result.CompilerSourceContextIndex].PropertyName, PropertyPath.GetFields());
	}
	else if (PropertyPath.IsFromWidget())
	{
		const FName SourceName = PropertyPath.GetWidgetName();
		Result.bIsRootWidget = SourceName == Class->ClassGeneratedBy->GetFName();
		if (Result.bIsRootWidget)
		{
			Result.CompilerSourceContextIndex = INDEX_NONE;
			Result.SourceClass = Class->ClassGeneratedBy->GetClass();
			Result.PropertyPath = CreatePropertyPath(Class, FName(), PropertyPath.GetFields());
		}
		else
		{
			Result.CompilerSourceContextIndex = CompilerSourceContexts.IndexOfByPredicate([SourceName](const FCompilerSourceContext& Other) { return Other.PropertyName == SourceName; });
			check(Result.CompilerSourceContextIndex != INDEX_NONE);
			Result.SourceClass = CompilerSourceContexts[Result.CompilerSourceContextIndex].Class;
			Result.PropertyPath = CreatePropertyPath(Class, CompilerSourceContexts[Result.CompilerSourceContextIndex].PropertyName, PropertyPath.GetFields());
		}
	}
	else
	{
		ensureAlwaysMsgf(false, TEXT("Not supported yet."));
	}

	// The path may contains another INotifyFieldValueChanged
	TValueOrError<FieldPathHelper::FParsedBindingInfo, FText> BindingInfoResult = FieldPathHelper::GetBindingInfoFromFieldPath(Class, Result.PropertyPath);
	if (BindingInfoResult.HasError())
	{
		return MakeError(BindingInfoResult.StealError());
	}

	const FieldPathHelper::FParsedBindingInfo& BindingInfo = BindingInfoResult.GetValue();
	Result.FieldId = BindingInfo.NotifyFieldId;

	const bool bCreateADynamicSourceEntry = BindingInfo.ViewModelIndex > 1
		&& BindingInfo.NotifyFieldClass != nullptr;

	// The INotifyFieldValueChanged/viewmodel is not the first property. Creates the ViaBinding source entries.
	if (bCreateADynamicSourceEntry)
	{
		Result.SourceClass = BindingInfo.NotifyFieldClass;
		Result.CompilerSourceContextIndex = INDEX_NONE;
		Result.bIsRootWidget = false;

		FString InterfacePath = FieldPathHelper::ToString(BindingInfo.NotifyFieldInterfacePath);
		int32 SourceCreatorContextIndex = CompilerSourceCreatorContexts.IndexOfByPredicate([&InterfacePath](const FCompilerSourceCreatorContext& Other)
			{
				return (Other.Type == ECompilerSourceCreatorType::ViewModel)
					? (Other.ViewModelContext.CreationType == EMVVMBlueprintViewModelContextCreationType::PropertyPath
						&& Other.ViewModelContext.ViewModelPropertyPath == InterfacePath)
					: false;
			});


		// Add if the path doesn't already exists
		if (!CompilerSourceCreatorContexts.IsValidIndex(SourceCreatorContextIndex))
		{
			return MakeError(LOCTEXT("ViewModelNotAtRoot", "The field in the path is from a viewmodel that is not the root of the path. This is not yet supported."));
		}

		Result.PropertyPath.RemoveAt(0, BindingInfo.ViewModelIndex);
		if (CompilerSourceCreatorContexts[SourceCreatorContextIndex].Type == ECompilerSourceCreatorType::ViewModel)
		{
			Result.CompilerSourceContextIndex = SourceCreatorContextIndex;
		}
	}
	else
	{
		if (Result.SourceClass != BindingInfo.NotifyFieldClass)
		{
			return MakeError(LOCTEXT("ClassDoesNotMatch", "Internal Error. The class doesn't match"));
		}
	}

	return MakeValue(Result);
}


TArray<FMVVMConstFieldVariant> FMVVMViewBlueprintCompiler::CreateBindingDestinationPath(const UMVVMBlueprintView* BlueprintView, const UWidgetBlueprintGeneratedClass* Class, const FMVVMBlueprintPropertyPath& PropertyPath) const
{
	if (PropertyPath.IsEmpty())
	{
		ensureAlwaysMsgf(false, TEXT("Empty property path found. This is legal."));
		return TArray<FMVVMConstFieldVariant>();
	}

	if (PropertyPath.IsFromViewModel())
	{
		const FMVVMBlueprintViewModelContext* SourceViewModelContext = BlueprintView->FindViewModel(PropertyPath.GetViewModelId());
		check(SourceViewModelContext);
		FName DestinationName = SourceViewModelContext->GetViewModelName();
		const int32 DestinationVariableContextIndex = CompilerSourceContexts.IndexOfByPredicate([DestinationName](const FCompilerSourceContext& Other) { return Other.PropertyName == DestinationName; });
		check(DestinationVariableContextIndex != INDEX_NONE);

		return CreatePropertyPath(Class, CompilerSourceContexts[DestinationVariableContextIndex].PropertyName, PropertyPath.GetFields());
	}
	else if (PropertyPath.IsFromWidget())
	{
		FName DestinationName = PropertyPath.GetWidgetName();
		checkf(!DestinationName.IsNone(), TEXT("The destination should have been checked and set bAreSourceContextsValid."));
		const bool bSourceIsUserWidget = DestinationName == Class->ClassGeneratedBy->GetFName();
		if (bSourceIsUserWidget)
		{
			return CreatePropertyPath(Class, FName(), PropertyPath.GetFields());
		}
		else
		{
			const int32 DestinationVariableContextIndex = CompilerSourceContexts.IndexOfByPredicate([DestinationName](const FCompilerSourceContext& Other) { return Other.PropertyName == DestinationName; });
			if (ensureAlwaysMsgf(DestinationVariableContextIndex != INDEX_NONE, TEXT("Could not find source context for destination '%s'"), *DestinationName.ToString()))
			{
				return CreatePropertyPath(Class, CompilerSourceContexts[DestinationVariableContextIndex].PropertyName, PropertyPath.GetFields());
			}
		}
	}
	else
	{
		ensureAlwaysMsgf(false, TEXT("Not supported yet."));
		return CreatePropertyPath(Class, FName(), PropertyPath.GetFields());
	}

	return TArray<FMVVMConstFieldVariant>();
}

TArray<FMVVMConstFieldVariant> FMVVMViewBlueprintCompiler::CreatePropertyPath(const UClass* Class, FName PropertyName, TArray<FMVVMConstFieldVariant> Properties) const
{
	if (PropertyName.IsNone())
	{
		return Properties;
	}

	check(Class);
	FMVVMConstFieldVariant NewProperty = BindingHelper::FindFieldByName(Class, FMVVMBindingName(PropertyName));
	Properties.Insert(NewProperty, 0);
	return Properties;
}


bool FMVVMViewBlueprintCompiler::IsPropertyPathValid(TArrayView<const FMVVMConstFieldVariant> PropertyPath) const
{
	for (const FMVVMConstFieldVariant& Field : PropertyPath)
	{
		if (Field.IsEmpty())
		{
			return false;
		}
		if (Field.IsProperty() && Field.GetProperty() == nullptr)
		{
			return false;
		}
		if (Field.IsFunction() && Field.GetFunction() == nullptr)
		{
			return false;
		}
	}
	return true;
}

} //namespace

#undef LOCTEXT_NAMESPACE
