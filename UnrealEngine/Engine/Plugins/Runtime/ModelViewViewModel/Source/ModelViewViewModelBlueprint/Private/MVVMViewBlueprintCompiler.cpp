// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVMViewBlueprintCompiler.h"
#include "Blueprint/WidgetTree.h"
#include "Bindings/MVVMBindingHelper.h"
#include "Bindings/MVVMConversionFunctionHelper.h"
#include "Bindings/MVVMFieldPathHelper.h"
#include "Components/Widget.h"
#include "HAL/IConsoleManager.h"
#include "MVVMBlueprintView.h"
#include "MVVMBlueprintViewConversionFunction.h"
#include "MVVMDeveloperProjectSettings.h"
#include "MVVMMessageLog.h"
#include "PropertyPermissionList.h"
#include "MVVMFunctionGraphHelper.h"
#include "MVVMWidgetBlueprintExtension_View.h"
#include "Templates/ValueOrError.h"
#include "Types/MVVMBindingName.h"
#include "UObject/LinkerLoad.h"
#include "View/MVVMViewClass.h"
#include "View/MVVMViewModelContextResolver.h"

#define LOCTEXT_NAMESPACE "MVVMViewBlueprintCompiler"

namespace UE::MVVM::Private
{
FAutoConsoleVariable CVarLogViewCompliedResult(
	TEXT("MVVM.LogViewCompliedResult"),
	false,
	TEXT("After the view is compiled log the compiled bindings and sources.")
);

#if UE_BUILD_DEVELOPMENT || UE_BUILD_DEBUG
FAutoConsoleCommand CVarTestGenerateSetter(
	TEXT("MVVM.TestGenerateSetter"),
	TEXT("Generate a setter function base on the input string."),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (Args.Num() < 3)
		{
			return;
		}
		FMVVMViewBlueprintCompiler::TestGenerateSetter(Args[0], Args[1], Args[2]);
	})
);
#endif

static const FText CouldNotCreateSourceFieldPathFormat = LOCTEXT("CouldNotCreateSourceFieldPath", "Couldn't create the source field path '{0}'. {1}");

FString PropertyPathToString(const UClass* InSelfContext, const UMVVMBlueprintView* BlueprintView, const FMVVMBlueprintPropertyPath& PropertyPath)
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

	FString BasePropertyPath = PropertyPath.GetPropertyPath(InSelfContext);
	if (BasePropertyPath.Len())
	{
		Result << TEXT('.');
		Result << MoveTemp(BasePropertyPath);
	}
	return Result.ToString();
}

FText PropertyPathToText(const UClass* InSelfContext, const UMVVMBlueprintView* BlueprintView, const FMVVMBlueprintPropertyPath& PropertyPath)
{
	return FText::FromString(PropertyPathToString(InSelfContext, BlueprintView, PropertyPath));
}

FText GetViewModelIdText(const FMVVMBlueprintPropertyPath& PropertyPath)
{
	return FText::FromString(PropertyPath.GetViewModelId().ToString(EGuidFormats::DigitsWithHyphensInBraces));
}

TValueOrError<FCompiledBindingLibraryCompiler::FFieldPathHandle, FText> AddObjectFieldPath(FCompiledBindingLibraryCompiler& BindingLibraryCompiler, const UWidgetBlueprintGeneratedClass* Class, FStringView ObjectPath, UClass* ExpectedType)
{
	// Generate a path to read the value at runtime
	static const FText InvalidGetterFormat = LOCTEXT("ViewModelInvalidGetterWithReason", "Viewmodel has an invalid Getter. {0}");

	TValueOrError<TArray<FMVVMConstFieldVariant>, FText> GeneratedField = FieldPathHelper::GenerateFieldPathList(Class, ObjectPath, true);
	if (GeneratedField.HasError())
	{
		return MakeError(FText::Format(InvalidGetterFormat, GeneratedField.GetError()));
	}

	TValueOrError<FCompiledBindingLibraryCompiler::FFieldPathHandle, FText> ReadFieldPathResult = BindingLibraryCompiler.AddObjectFieldPath(GeneratedField.GetValue(), ExpectedType, true);
	if (ReadFieldPathResult.HasError())
	{
		return MakeError(FText::Format(InvalidGetterFormat, ReadFieldPathResult.GetError()));
	}

	return MakeValue(ReadFieldPathResult.StealValue());
}

void FMVVMViewBlueprintCompiler::AddMessageForBinding(FMVVMBlueprintViewBinding& Binding, UMVVMBlueprintView* BlueprintView, const FText& MessageText, EBindingMessageType MessageType, FName ArgumentName) const
{
	const FText BindingName = FText::FromString(Binding.GetDisplayNameString(WidgetBlueprintCompilerContext.WidgetBlueprint()));

	FText FormattedError;
	if (!ArgumentName.IsNone())
	{
		FormattedError = FText::Format(LOCTEXT("BindingFormatWithArgument", "Binding '{0}': Argument '{1}' - {2}"), BindingName, FText::FromName(ArgumentName), MessageText);
	}
	else
	{
		FormattedError = FText::Format(LOCTEXT("BindingFormat", "Binding '{0}': {1}"), BindingName, MessageText);
	}

	switch (MessageType)
	{
	case EBindingMessageType::Info:
		WidgetBlueprintCompilerContext.MessageLog.Note(*FormattedError.ToString());
		break;
	case EBindingMessageType::Warning:
		WidgetBlueprintCompilerContext.MessageLog.Warning(*FormattedError.ToString());
		break;
	case EBindingMessageType::Error:
		WidgetBlueprintCompilerContext.MessageLog.Error(*FormattedError.ToString());
		break;
	default:
		break;
	}
	FBindingMessage NewMessage = { FormattedError, MessageType };
	BlueprintView->AddMessageToBinding(Binding.BindingId, NewMessage);
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
	
	// Build the list of BP destinations
	CreateBindingDestinationContexts(BlueprintView);

	if (!bAreSourcesCreatorValid || !bIsBindingsValid)
	{
		return;
	}

	// Generate the setter code
	if (GetDefault<UMVVMDeveloperProjectSettings>()->bAllowGeneratedViewModelSetter)
	{
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
	else
	{
		CreateIntermediateGraphFunctions(Context, BlueprintView);
	}

	auto CreateVariable = [&Context](const FCompilerUserWidgetPropertyContext& SourceContext) -> FProperty*
	{
		FEdGraphPinType NewPropertyPinType(UEdGraphSchema_K2::PC_Object, NAME_None, SourceContext.Class, EPinContainerType::None, false, FEdGraphTerminalType());
		FProperty* NewProperty = Context.CreateVariable(SourceContext.PropertyName, NewPropertyPinType);
		if (NewProperty != nullptr)
		{
			NewProperty->SetPropertyFlags(CPF_BlueprintVisible | CPF_RepSkip | CPF_Transient | CPF_DuplicateTransient);
			if (SourceContext.BlueprintSetter.IsEmpty())
			{
				NewProperty->SetPropertyFlags(CPF_BlueprintReadOnly);
			}
			NewProperty->SetPropertyFlags(SourceContext.bExposeOnSpawn ? CPF_ExposeOnSpawn : CPF_DisableEditOnInstance);

#if WITH_EDITOR
			if (!SourceContext.BlueprintSetter.IsEmpty())
			{
				NewProperty->SetMetaData(FBlueprintMetadata::MD_PropertySetFunction, *SourceContext.BlueprintSetter);
			}
			if (!SourceContext.DisplayName.IsEmpty())
			{
				NewProperty->SetMetaData(FBlueprintMetadata::MD_DisplayName, *SourceContext.DisplayName.ToString());
			}
			if (!SourceContext.CategoryName.IsEmpty())
			{
				NewProperty->SetMetaData(FBlueprintMetadata::MD_FunctionCategory, *SourceContext.CategoryName);
			}
			if (SourceContext.bExposeOnSpawn)
			{
				NewProperty->SetMetaData(FBlueprintMetadata::MD_ExposeOnSpawn, TEXT("true"));
			}
			//if (!SourceContext.bPublicGetter)
			//{
			//	NewProperty->SetMetaData(FBlueprintMetadata::MD_Private, TEXT("true"));
			//}
#endif
		}
		return NewProperty;
	};

	for (FCompilerUserWidgetPropertyContext& SourceContext : CompilerUserWidgetPropertyContexts)
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
	CompilerUserWidgetPropertyContexts.Reset();
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

		const bool bCreateSetterFunction = GetDefault<UMVVMDeveloperProjectSettings>()->bAllowGeneratedViewModelSetter
			&& (ViewModelContext.bCreateSetterFunction || ViewModelContext.CreationType == EMVVMBlueprintViewModelContextCreationType::Manual);

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

		FCompilerUserWidgetPropertyContext SourceVariable;
		SourceVariable.Class = ViewModelContext.GetViewModelClass();
		SourceVariable.PropertyName = ViewModelContext.GetViewModelName();
		SourceVariable.DisplayName = ViewModelContext.GetDisplayName();
		SourceVariable.CategoryName = TEXT("Viewmodel");
		SourceVariable.bExposeOnSpawn = bCreateSetterFunction;
		//SourceVariable.bPublicGetter = ViewModelContext.bCreateGetterFunction;
		SourceVariable.BlueprintSetter = CompilerSourceCreatorContexts[FoundSourceCreatorContextIndex].SetterFunctionName;
		SourceVariable.ViewModelId = ViewModelContext.GetViewModelId();
		CompilerUserWidgetPropertyContexts.Emplace(MoveTemp(SourceVariable));
	}

	bAreSourceContextsValid = bAreSourcesCreatorValid;

	UWidgetBlueprintGeneratedClass* SkeletonClass = Context.GetSkeletonGeneratedClass();
	const FName DefaultWidgetCategory = Context.GetWidgetBlueprint()->GetFName();

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
		auto GenerateCompilerSourceContext = [Self, BlueprintView, DefaultWidgetCategory, Class = SkeletonClass, &Binding, &ViewModelGuids, &WidgetSources](const FMVVMBlueprintPropertyPath& PropertyPath,  FName ArgumentName = FName()) -> bool
		{
			if (PropertyPath.IsFromWidget())
			{
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
						Self->AddMessageForBinding(Binding,
							BlueprintView,
							FText::Format(LOCTEXT("InvalidWidgetFormat", "Could not find the targeted widget: {0}"), 
								FText::FromName(PropertyPath.GetWidgetName())
							),
							EBindingMessageType::Error,
							ArgumentName
						);
						return false;
					}

					UWidget* Widget = *WidgetPtr;
					FCompilerUserWidgetPropertyContext SourceVariable;
					SourceVariable.Class = Widget->GetClass();
					SourceVariable.PropertyName = PropertyPath.GetWidgetName();
					SourceVariable.DisplayName = FText::FromString(Widget->GetDisplayLabel());
					SourceVariable.CategoryName = TEXT("Widget");
					SourceVariable.ViewModelId = FGuid();
					SourceVariable.bPublicGetter = false;
					Self->CompilerUserWidgetPropertyContexts.Emplace(MoveTemp(SourceVariable));
				}
			}
			else if (PropertyPath.IsFromViewModel())
			{
				const FMVVMBlueprintViewModelContext* SourceViewModelContext = BlueprintView->FindViewModel(PropertyPath.GetViewModelId());
				if (SourceViewModelContext == nullptr)
				{
					Self->AddMessageForBinding(Binding,
						BlueprintView,
						FText::Format(LOCTEXT("BindingViewModelNotFound", "Could not find viewmodel with GUID {0}."), GetViewModelIdText(PropertyPath)),
						EBindingMessageType::Error,
						ArgumentName
					);
					return false;
				}

				if (!ViewModelGuids.Contains(SourceViewModelContext->GetViewModelId()))
				{
					Self->AddMessageForBinding(Binding,
						BlueprintView,
						FText::Format(LOCTEXT("BindingViewModelInvalid", "Viewmodel {0} {1} was invalid."), 
							SourceViewModelContext->GetDisplayName(),
							GetViewModelIdText(PropertyPath)
						),
						EBindingMessageType::Error,
						ArgumentName
					);
					return false;
				}
			}
			else
			{
				Self->AddMessageForBinding(Binding, BlueprintView, LOCTEXT("SourcePathNotSet", "A source path is required, but not set."), EBindingMessageType::Error, ArgumentName);
				return false;
			}
			return true;
		};

		const bool bIsForwardBinding = IsForwardBinding(Binding.BindingType);
		const bool bIsBackwardBinding = IsBackwardBinding(Binding.BindingType);

		if (bIsForwardBinding || bIsBackwardBinding)
		{
			UMVVMBlueprintViewConversionFunction* ConversionFunction = Binding.Conversion.GetConversionFunction(bIsForwardBinding);
			if (ConversionFunction && ConversionFunction->NeedsWrapperGraph())
			{
				ConversionFunction->SavePinValues(Context.GetWidgetBlueprint());
				if (ConversionFunction->GetPins().Num() > 0)
				{
					if (Binding.BindingType == EMVVMBindingMode::TwoWay)
					{
						Self->AddMessageForBinding(Binding, BlueprintView, LOCTEXT("TwoWayBindingsWithConversion", "Two-way bindings are not allowed to use conversion functions."), EBindingMessageType::Error);
						bAreSourceContextsValid = false;
						continue;
					}

					// generate sources for conversion function arguments
					for (const FMVVMBlueprintPin& Pin: ConversionFunction->GetPins())
					{
						if (Pin.UsedPathAsValue())
						{
							bAreSourceContextsValid &= GenerateCompilerSourceContext(Pin.GetPath(), Pin.GetName());
						}
					}

					// generate destination source
					if (bIsForwardBinding)
					{
						bAreSourceContextsValid &= GenerateCompilerSourceContext(Binding.DestinationPath);
					}
					else
					{
						bAreSourceContextsValid &= GenerateCompilerSourceContext(Binding.SourcePath);
					}
				}
			}
			else
			{
				// if we aren't using a conversion function, just validate the widget and viewmodel paths
				bAreSourceContextsValid &= GenerateCompilerSourceContext(Binding.DestinationPath);
				bAreSourceContextsValid &= GenerateCompilerSourceContext(Binding.SourcePath);
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

	if (GetDefault<UMVVMDeveloperProjectSettings>()->bAllowGeneratedViewModelSetter)
	{
		for (FCompilerSourceCreatorContext& SourceCreator : CompilerSourceCreatorContexts)
		{
			if (!SourceCreator.SetterFunctionName.IsEmpty() && SourceCreator.Type == ECompilerSourceCreatorType::ViewModel)
			{
				ensure(SourceCreator.SetterGraph == nullptr);

				SourceCreator.SetterGraph = UE::MVVM::FunctionGraphHelper::CreateIntermediateFunctionGraph(
					WidgetBlueprintCompilerContext
					, SourceCreator.SetterFunctionName
					, (FUNC_BlueprintCallable | FUNC_Public | FUNC_Final)
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
}


void FMVVMViewBlueprintCompiler::CreateIntermediateGraphFunctions(const FWidgetBlueprintCompilerContext::FCreateVariableContext & Context, UMVVMBlueprintView * BlueprintView)
{
	// Add Generated Conversion functions to the Context.List
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

		auto AddGeneratedConversionFunction = [&Context, BindingPtr, this](bool bIsForwardBinding)
		{
			UMVVMBlueprintViewConversionFunction* ConversionFunction = BindingPtr->Conversion.GetConversionFunction(bIsForwardBinding);
			if (ConversionFunction && ConversionFunction->IsWrapperGraphTransient())
			{
				UEdGraph* WrapperGraph = ConversionFunction->GetOrCreateIntermediateWrapperGraph(WidgetBlueprintCompilerContext);
				if (WrapperGraph)
				{
					bool bAlreadyContained = WidgetBlueprintCompilerContext.Blueprint->FunctionGraphs.Contains(WrapperGraph);
					if (ensure(!bAlreadyContained))
					{
						Context.AddGeneratedFunctionGraph(WrapperGraph);
					}
				}
			}
		};

		if (IsForwardBinding(Binding.BindingType))
		{
			AddGeneratedConversionFunction(true);
		}
		if (IsBackwardBinding(Binding.BindingType))
		{
			AddGeneratedConversionFunction(false);
		}
	}
}


void FMVVMViewBlueprintCompiler::CreateBindingDestinationContexts(UMVVMBlueprintView* BlueprintView)
{
	if (!bAreSourcesCreatorValid || !bAreSourceContextsValid || !bIsBindingsValid)
	{
		return;
	}

	BindingDestinationContexts.Empty();

	// Use the Skeleton class. The class bind and not all functions are generated yet
	UWidgetBlueprintGeneratedClass* NewSkeletonClass = Cast<UWidgetBlueprintGeneratedClass>(WidgetBlueprintCompilerContext.Blueprint->SkeletonGeneratedClass);
	if (NewSkeletonClass == nullptr)
	{
		WidgetBlueprintCompilerContext.MessageLog.Error(*LOCTEXT("InvalidNewClass", "Internal error. The skeleton class is not valid.").ToString());
		return;
	}

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

		auto AddDestination = [Index, BindingPtr, BlueprintView, NewSkeletonClass, this](bool bIsForwardBinding)
		{
			FBindingDestinationContext& NewDestinationContext = BindingDestinationContexts.AddDefaulted_GetRef();
			NewDestinationContext.BindingIndex = Index;
			NewDestinationContext.bIsForwardBinding = bIsForwardBinding;

			const FMVVMBlueprintPropertyPath& DestinationPath = bIsForwardBinding ? BindingPtr->DestinationPath : BindingPtr->SourcePath;
			TArray<UE::MVVM::FMVVMConstFieldVariant> SkeletalSetterPath = CreateBindingDestinationPath(BlueprintView, NewSkeletonClass, DestinationPath);
			if (!IsPropertyPathValid(SkeletalSetterPath))
			{
				AddMessageForBinding(*BindingPtr, BlueprintView, FText::Format(LOCTEXT("PropertyPathIsInvalid", "The property path '{0}' is invalid."),
					PropertyPathToText(NewSkeletonClass, BlueprintView, DestinationPath)),
					EBindingMessageType::Error
				);
				bIsBindingsValid = false;
				return;
			}
			
			// Generate the path with property converted to BP function
			TValueOrError<TArray<FMVVMConstFieldVariant>, FText> SkeletalGeneratedField = FieldPathHelper::GenerateFieldPathList(SkeletalSetterPath, false);
			if (SkeletalGeneratedField.HasError())
			{
				AddMessageForBinding(*BindingPtr, BlueprintView, FText::Format(Private::CouldNotCreateSourceFieldPathFormat
					, ::UE::MVVM::FieldPathHelper::ToText(SkeletalSetterPath)
					, SkeletalGeneratedField.GetError())
					, EBindingMessageType::Error
				);
				bIsBindingsValid = false;
				return;
			}

			NewDestinationContext.bCanBeSetInNative = CanBeSetInNative(SkeletalGeneratedField.GetValue());

			// If the destination can't be set in cpp, we need to generate a BP function to set the value.
			if (!NewDestinationContext.bCanBeSetInNative)
			{
				const FProperty* SetterProperty = nullptr;
				if (SkeletalGeneratedField.GetValue().Num() > 0 && ensure(SkeletalGeneratedField.GetValue().Last().IsProperty()))
				{
					SetterProperty = SkeletalGeneratedField.GetValue().Last().GetProperty();
				}

				if (SetterProperty == nullptr)
				{
					AddMessageForBinding(*BindingPtr, BlueprintView, FText::Format(LOCTEXT("CantGetSetter", "Internal Error. The setter function was not created. {0}"),
						PropertyPathToText(NewSkeletonClass, BlueprintView, DestinationPath)),
						EBindingMessageType::Error
					);
					bIsBindingsValid = false;
					return;
				}

				// create a setter function to be called from native. For now we follow the convention of Setter(Conversion(Getter))
				UEdGraph* GeneratedSetterGraph = UE::MVVM::FunctionGraphHelper::CreateIntermediateFunctionGraph(WidgetBlueprintCompilerContext, FString::Printf(TEXT("__Setter_%s"), *SetterProperty->GetName()), EFunctionFlags::FUNC_None, TEXT("AutogeneratedSetter"), false);
				if (GeneratedSetterGraph == nullptr)
				{
					AddMessageForBinding(*BindingPtr, BlueprintView, FText::Format(LOCTEXT("CantCreateSetter", "Internal Error. The setter function was not created. {0}"),
						PropertyPathToText(NewSkeletonClass, BlueprintView, DestinationPath)),
						EBindingMessageType::Error
					);
					bIsBindingsValid = false;
					return;
				}

				UE::MVVM::FunctionGraphHelper::AddFunctionArgument(GeneratedSetterGraph, SetterProperty, "NewValue");

				// set GeneratedSetterFunction. Use the SkeletalSetterPath here to use the setter will be generated when the function is generated.
				if (!UE::MVVM::FunctionGraphHelper::GenerateIntermediateSetter(WidgetBlueprintCompilerContext, GeneratedSetterGraph, SkeletalSetterPath))
				{
					AddMessageForBinding(*BindingPtr, BlueprintView, FText::Format(LOCTEXT("CantGeneratedSetter", "Internal Error. The setter function was not generated. {0}"),
						PropertyPathToText(NewSkeletonClass, BlueprintView, DestinationPath)),
						EBindingMessageType::Error
					);
					bIsBindingsValid = false;
					return;
				}

				// the new path can only be set later, once the function is compiled.
				NewDestinationContext.GeneratedFunctionName = GeneratedSetterGraph->GetFName();
			}
		};

		if (IsForwardBinding(Binding.BindingType))
		{
			AddDestination(true);
		}
		if (IsBackwardBinding(Binding.BindingType))
		{
			AddDestination(false);
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
	SimpleBindingContexts.Reset();
	ComplexConversionFunctionContexts.Reset();

	FPropertyEditorPermissionList::Get().AddPermissionList(Class, FNamePermissionList(), EPropertyPermissionListRules::AllowListAllProperties);

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

	{
		ViewExtension->bInitializeSourcesOnConstruct = BlueprintView->GetSettings()->bInitializeSourcesOnConstruct;
		ViewExtension->bInitializeBindingsOnConstruct = ViewExtension->bInitializeSourcesOnConstruct ? BlueprintView->GetSettings()->bInitializeBindingsOnConstruct : false;
	}

	bool bResult = bAreSourcesCreatorValid && bAreSourceContextsValid && bIsBindingsValid;
	if (bResult)
	{
		ViewExtension->BindingLibrary = MoveTemp(CompileResult.GetValue().Library);

#if UE_WITH_MVVM_DEBUGGING
		if (CVarLogViewCompliedResult->GetBool())
		{
			FMVVMViewClass_SourceCreator::FToStringArgs CreatorsToStringArgs = FMVVMViewClass_SourceCreator::FToStringArgs::All();
			CreatorsToStringArgs.bUseDisplayName = false;
			FMVVMViewClass_CompiledBinding::FToStringArgs BindingToStringArgs = FMVVMViewClass_CompiledBinding::FToStringArgs::All();
			BindingToStringArgs.bUseDisplayName = false;
			ViewExtension->Log(CreatorsToStringArgs, BindingToStringArgs);
		}
#endif
	}

	return bResult;
}


bool FMVVMViewBlueprintCompiler::PreCompileBindingSources(UWidgetBlueprintGeneratedClass* Class, UMVVMBlueprintView* BlueprintView)
{
	const int32 NumBindings = BlueprintView->GetNumBindings(); // NB Binding can be added when creating a dynamic vm
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

		bool bIsOneTimeBinding = IsOneTimeBinding(Binding.BindingType);

		auto CreateSourceContextForPropertyPath = [this, &Binding, BlueprintView, Class, Index, bIsOneTimeBinding](const FMVVMBlueprintPropertyPath& Path, bool bForwardBinding, int32 ComplexConversionFunctionContextIndex, FName ArgumentName) -> bool
		{
			const TValueOrError<FBindingSourceContext, FText> CreatedBindingSourceContext = CreateBindingSourceContext(BlueprintView, Class, Path, bIsOneTimeBinding);
			if (CreatedBindingSourceContext.HasError())
			{
				AddMessageForBinding(Binding, BlueprintView,
					FText::Format(LOCTEXT("PropertyPathInvalidWithReason", "The property path '{0}' is invalid. {1}"),
						PropertyPathToText(Class, BlueprintView, Binding.SourcePath),
						CreatedBindingSourceContext.GetError()
					),
					EBindingMessageType::Error,
					ArgumentName
				);
				return false;
			}

			FBindingSourceContext BindingSourceContext = CreatedBindingSourceContext.GetValue();
			if (!IsPropertyPathValid(BindingSourceContext.PropertyPath))
			{
				AddMessageForBinding(Binding,
					BlueprintView,
					FText::Format(LOCTEXT("PropertyPathIsInvalid", "The property path '{0}' is invalid."), 
						PropertyPathToText(Class, BlueprintView, Binding.SourcePath)
					),
					EBindingMessageType::Error,
					ArgumentName
				);
				return false;
			}

			if (BindingSourceContext.SourceClass == nullptr)
			{
				AddMessageForBinding(Binding, BlueprintView, LOCTEXT("BindingInvalidSourceClass", "Internal error. The binding could not find its source class."), EBindingMessageType::Error, ArgumentName);
				return false;
			}

			if (!BindingSourceContext.bIsRootWidget && BindingSourceContext.UserWidgetPropertyContextIndex == INDEX_NONE && BindingSourceContext.SourceCreatorContextIndex == INDEX_NONE)
			{
				AddMessageForBinding(Binding, BlueprintView, LOCTEXT("BindingInvalidSource", "Internal error. The binding could not find its source."), EBindingMessageType::Error, ArgumentName);
				return false;
			}

			BindingSourceContext.BindingIndex = Index;
			BindingSourceContext.bIsForwardBinding = bForwardBinding;
			BindingSourceContext.ComplexConversionFunctionContextIndex = ComplexConversionFunctionContextIndex;

			this->BindingSourceContexts.Add(MoveTemp(BindingSourceContext));
			return true;
		};

		enum class ECreateSourcesForConversionFunctionResult : uint8 { Valid, Failed, Continue };
		auto CreateSourcesForConversionFunction = [this, &Binding, BlueprintView, Index, &CreateSourceContextForPropertyPath](bool bForwardBinding)
		{
			ECreateSourcesForConversionFunctionResult Result = ECreateSourcesForConversionFunctionResult::Continue;
			if (const UMVVMBlueprintViewConversionFunction* ConversionFunction = Binding.Conversion.GetConversionFunction(bForwardBinding))
			{
				if (ConversionFunction->NeedsWrapperGraph())
				{
					const int32 ComplexConversionFunctionContextIndex = ComplexConversionFunctionContexts.AddDefaulted();
					{
						FComplexConversionFunctionContext& NewConversionFunctionContext = ComplexConversionFunctionContexts[ComplexConversionFunctionContextIndex];
						NewConversionFunctionContext.BindingIndex = Index;
						NewConversionFunctionContext.bIsForwardBinding = bForwardBinding;
					}

					for (const FMVVMBlueprintPin& Pin : ConversionFunction->GetPins())
					{
						if (Pin.UsedPathAsValue())
						{
							if (Pin.GetName().IsNone())
							{
								AddMessageForBinding(Binding, BlueprintView,
									FText::Format(LOCTEXT("InvalidArgumentPathName", "The conversion function {0} has an invalid argument"), FText::FromString(Binding.GetDisplayNameString(WidgetBlueprintCompilerContext.WidgetBlueprint()))),
									EBindingMessageType::Error
								);
								Result = ECreateSourcesForConversionFunctionResult::Failed;
							}
							else
							{
								const FMVVMBlueprintPropertyPath& Path = Pin.GetPath();
								if (CreateSourceContextForPropertyPath(Path, bForwardBinding, ComplexConversionFunctionContextIndex, Pin.GetName()))
								{
									Result = ECreateSourcesForConversionFunctionResult::Valid;
								}
								else
								{
									Result = ECreateSourcesForConversionFunctionResult::Failed;
									break;
								}
							}
						}
					}

					if (Result == ECreateSourcesForConversionFunctionResult::Continue)
					{
						// The bindings doesn't have a path but could be onetime with hard codded value.
						FSimpleBindingContext& NewBindingContext = SimpleBindingContexts.AddDefaulted_GetRef();
						NewBindingContext.BindingIndex = Index;
						NewBindingContext.bIsForwardBinding = bForwardBinding;
						NewBindingContext.ComplexConversionFunctionContextIndex = ComplexConversionFunctionContextIndex;
						ComplexConversionFunctionContexts[ComplexConversionFunctionContextIndex].bNeedsValidSource = false;

						Result = ECreateSourcesForConversionFunctionResult::Valid;
					}
				}
			}
			return Result;
		};

		auto AddWarningForPropertyWithMVVMAndLegacyBinding = [this, &Binding, &BlueprintView, Class](const FMVVMBlueprintPropertyPath& Path)
		{
			if (!Path.HasPaths())
			{
				return;
			}

			// There can't be a legacy binding in the local scope, so we can skip this if the MVVM binding refers to a property in local scope.
			if (Path.HasFieldInLocalScope())
			{
				return;
			}

			TArrayView<FMVVMBlueprintFieldPath const> MVVMBindingPath = Path.GetFieldPaths();
			TArray< FDelegateRuntimeBinding > LegacyBindings = Class->Bindings;
			FName MVVMFieldName = Path.GetFieldNames(Class).Last();
			FName MVVMObjectName = Path.GetWidgetName();

			if (Path.GetFieldPaths().Last().GetBindingKind() == EBindingKind::Function)
			{
				return;
			}

			// If the first field is a UserWidget, we know this property resides in a nested UserWidget.
			if (MVVMBindingPath[0].GetParentClass(Class) && MVVMBindingPath[0].GetParentClass(Class)->IsChildOf(UUserWidget::StaticClass()) && MVVMBindingPath.Num() > 1)
			{
				if (UWidgetBlueprintGeneratedClass* NestedBPGClass = Cast<UWidgetBlueprintGeneratedClass>(MVVMBindingPath[MVVMBindingPath.Num() - 2].GetParentClass(Class)))
				{
					LegacyBindings = NestedBPGClass->Bindings;

					// We can't use Path.GetWidgetName() when we are dealing with nested UserWidgets, because it refers to the topmost UserWidget.
					MVVMObjectName = MVVMBindingPath[MVVMBindingPath.Num() - 2].GetFieldName(Class);
				}
				else
				{
					return;
				}
			}

			for (const FDelegateRuntimeBinding& LegacyBinding : LegacyBindings)
			{
				if (LegacyBinding.ObjectName == MVVMObjectName) 
				{
					if (LegacyBinding.PropertyName == MVVMFieldName)
					{
						AddMessageForBinding(Binding, BlueprintView, LOCTEXT("BindingConflictWithLegacy", "The binding is set on a property with legacy binding."), EBindingMessageType::Warning);
						break;
					}
				}
			}
		};

		// Add the forward binding. If the binding has a conversion function, use it instead of the regular binding.
		if (IsForwardBinding(Binding.BindingType))
		{
			ECreateSourcesForConversionFunctionResult ConversionFunctionResult = CreateSourcesForConversionFunction(true);
			if (ConversionFunctionResult == ECreateSourcesForConversionFunctionResult::Continue)
			{
				if (!Binding.SourcePath.IsEmpty())
				{
					if (!Binding.DestinationPath.IsEmpty())
					{
						AddWarningForPropertyWithMVVMAndLegacyBinding(Binding.DestinationPath);
					}
					if (!CreateSourceContextForPropertyPath(Binding.SourcePath, true, INDEX_NONE, FName()))
					{
						bIsBindingsValid = false;
						continue;
					}
				}
				else
				{
					AddMessageForBinding(Binding, BlueprintView, LOCTEXT("BindingEmptySourcePath", "The binding doesn't have a Source or a Conversion function."), EBindingMessageType::Error);
					bIsBindingsValid = false;
					continue;
				}
			}
			else if (ConversionFunctionResult == ECreateSourcesForConversionFunctionResult::Failed)
			{
				bIsBindingsValid = false;
				continue;
			}
		}

		// Add the backward binding. If the binding has a conversion function, use it instead of the regular binding.
		if (IsBackwardBinding(Binding.BindingType))
		{
			ECreateSourcesForConversionFunctionResult ConversionFunctionResult = CreateSourcesForConversionFunction(false);
			if (ConversionFunctionResult == ECreateSourcesForConversionFunctionResult::Continue)
			{
				if (!Binding.DestinationPath.IsEmpty())
				{
					if (!Binding.SourcePath.IsEmpty())
					{
						AddWarningForPropertyWithMVVMAndLegacyBinding(Binding.SourcePath);
					}
					if (!CreateSourceContextForPropertyPath(Binding.DestinationPath, false, INDEX_NONE, FName()))
					{
						bIsBindingsValid = false;
						continue;
					}
				}
				else
				{
					AddMessageForBinding(Binding, BlueprintView, LOCTEXT("BindingEmptyDestinationPath", "The binding doesn't have a Destination or a Conversion function."), EBindingMessageType::Error);
					bIsBindingsValid = false;
					continue;
				}
			}
			else if (ConversionFunctionResult == ECreateSourcesForConversionFunctionResult::Failed)
			{
				bIsBindingsValid = false;
				continue;
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
				AddErrorForViewModel(ViewModelContext, FText::Format(LOCTEXT("ViewModelTypeDeprecated", "Viewmodel class '{0}' is deprecated and should not be used. Please update it in the View Models panel."),
					ViewModelContext.GetViewModelClass()->GetDisplayNameText()
				));
			}

			if (!GetAllowedContextCreationType(ViewModelContext.GetViewModelClass()).Contains(ViewModelContext.CreationType))
			{
				AddErrorForViewModel(ViewModelContext, FText::Format(LOCTEXT("ViewModelContextCreationTypeInvalid", "Viewmodel '{0}' has an invalidate creation type. You can change it in the View Models panel."),
					ViewModelContext.GetViewModelClass()->GetDisplayNameText()
				));
				bAreSourcesCreatorValid = false;
				continue;
			}

			if (ViewModelContext.CreationType == EMVVMBlueprintViewModelContextCreationType::Manual)
			{
			}
			else if (ViewModelContext.CreationType == EMVVMBlueprintViewModelContextCreationType::CreateInstance)
			{
				if (ViewModelContext.GetViewModelClass()->HasAllClassFlags(CLASS_Abstract))
				{
					AddErrorForViewModel(ViewModelContext, FText::Format(LOCTEXT("ViewModelTypeAbstract", "Viewmodel class '{0}' is abstract and can't be created. You can change it in the View Models panel."),
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

				TValueOrError<FCompiledBindingLibraryCompiler::FFieldPathHandle, FText> ReadFieldPathResult = AddObjectFieldPath(BindingLibraryCompiler, Class, ViewModelContext.ViewModelPropertyPath, ViewModelContext.GetViewModelClass());
				if (ReadFieldPathResult.HasError())
				{
					AddErrorForViewModel(ViewModelContext, ReadFieldPathResult.GetError());
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
			else if (ViewModelContext.CreationType == EMVVMBlueprintViewModelContextCreationType::Resolver)
			{
				if (!ViewModelContext.Resolver)
				{
					AddErrorForViewModel(ViewModelContext, LOCTEXT("ViewmodelInvalidResolver", "Viewmodel doesn't have a valid Resolver. You can specify a new one in the Viewmodels panel."));
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
		const FMVVMBlueprintViewModelContext& ViewModelContext = SourceCreatorContext.ViewModelContext;
		FMVVMViewClass_SourceCreator CompiledSourceCreator;

		ensure(ViewModelContext.GetViewModelClass() && ViewModelContext.GetViewModelClass()->ImplementsInterface(UNotifyFieldValueChanged::StaticClass()));
		CompiledSourceCreator.ExpectedSourceType = ViewModelContext.GetViewModelClass();
		CompiledSourceCreator.PropertyName = ViewModelContext.GetViewModelName();

		bool bCanBeSet = false;
		bool CanBeEvaluated = false;
		bool bIsOptional = false;
		bool bCreateInstance = false;
		bool IsUserWidgetProperty = false;

		if (SourceCreatorContext.Type == ECompilerSourceCreatorType::ViewModel)
		{
			IsUserWidgetProperty = true;
			bCanBeSet = ViewModelContext.bCreateSetterFunction;

			if (ViewModelContext.CreationType == EMVVMBlueprintViewModelContextCreationType::Manual)
			{
				bCanBeSet = true;
				bIsOptional = true;
			}
			else if (ViewModelContext.CreationType == EMVVMBlueprintViewModelContextCreationType::CreateInstance)
			{
				bCreateInstance = true;
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

				CompiledSourceCreator.FieldPath = *CompiledFieldPath;
				bIsOptional = ViewModelContext.bOptional;
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

				CompiledSourceCreator.GlobalViewModelInstance = MoveTemp(GlobalViewModelInstance);
				bIsOptional = ViewModelContext.bOptional;
			}
			else if (ViewModelContext.CreationType == EMVVMBlueprintViewModelContextCreationType::Resolver)
			{
				UMVVMViewModelContextResolver* Resolver = DuplicateObject(ViewModelContext.Resolver.Get(), ViewExtension);
				if (!Resolver)
				{
					AddErrorForViewModel(ViewModelContext, LOCTEXT("ViewmodelFailedResolverDuplicate", "Internal error. The resolver could not be dupliated."));
					bAreSourcesCreatorValid = false;
					continue;
				}

				CompiledSourceCreator.Resolver = Resolver;
				bIsOptional = ViewModelContext.bOptional;
			}
			else
			{
				AddErrorForViewModel(ViewModelContext, LOCTEXT("ViewModelWithoutValidCreationType", "The viewmodel doesn't have a valid creation type."));
				bAreSourcesCreatorValid = false;
				continue;
			}
		}
		else if (SourceCreatorContext.Type == ECompilerSourceCreatorType::ViewModelDynamic)
		{
			const FMVVMVCompiledFieldPath* CompiledFieldPath = CompileResult.FieldPaths.Find(SourceCreatorContext.ReadPropertyPath);
			if (CompiledFieldPath == nullptr)
			{
				AddErrorForViewModel(ViewModelContext, LOCTEXT("ViewModelInvalidInitializationBindingNotGenerated", "The viewmodel initialization binding was not generated."));
				bAreSourcesCreatorValid = false;
				continue;
			}

			CanBeEvaluated = true;
			CompiledSourceCreator.FieldPath = *CompiledFieldPath;
			CompiledSourceCreator.ParentSourceName = SourceCreatorContext.DynamicParentSourceName;
		}

		CompiledSourceCreator.Flags = 0;
		CompiledSourceCreator.Flags |= bCreateInstance ? (uint8)FMVVMViewClass_SourceCreator::ESourceFlags::TypeCreateInstance : 0;
		CompiledSourceCreator.Flags |= IsUserWidgetProperty ? (uint8)FMVVMViewClass_SourceCreator::ESourceFlags::IsUserWidgetProperty : 0;
		CompiledSourceCreator.Flags |= bIsOptional ? (uint8)FMVVMViewClass_SourceCreator::ESourceFlags::IsOptional : 0;
		CompiledSourceCreator.Flags |= bCanBeSet ? (uint8)FMVVMViewClass_SourceCreator::ESourceFlags::CanBeSet : 0;
		CompiledSourceCreator.Flags |= CanBeEvaluated ? (uint8)FMVVMViewClass_SourceCreator::ESourceFlags::CanBeEvaluated : 0;

		ViewExtension->SourceCreators.Add(MoveTemp(CompiledSourceCreator));
	}

	return bAreSourcesCreatorValid;
}


bool FMVVMViewBlueprintCompiler::PreCompileBindings(UWidgetBlueprintGeneratedClass* Class, UMVVMBlueprintView* BlueprintView)
{
	if (!bAreSourceContextsValid || !bIsBindingsValid)
	{
		return false;
	}

	auto GetConversionFunction = [this, Class, BlueprintView](FMVVMBlueprintViewBinding& Binding, bool bIsForwardBinding) -> TValueOrError<const UFunction*, void>
	{
		const UFunction* ConversionFunction = nullptr;
		const UMVVMBlueprintViewConversionFunction* ViewConversionFunction = Binding.Conversion.GetConversionFunction(bIsForwardBinding);
		if (ViewConversionFunction)
		{
			ConversionFunction = ViewConversionFunction->GetCompiledFunction(Class);
			if (ConversionFunction == nullptr)
			{
				AddMessageForBinding(Binding, BlueprintView, FText::Format(LOCTEXT("ConversionFunctionNotFound", "The conversion function '{0}' could not be found."),
					FText::FromName(ViewConversionFunction->GetCompiledFunctionName())),
					EBindingMessageType::Error
				);
				bIsBindingsValid = false;
				return MakeError();
			}

			TVariant<const UFunction*, TSubclassOf<UK2Node>> FunctionOrWrapperFunction = ViewConversionFunction->GetConversionFunction(Class);
			if (FunctionOrWrapperFunction.IsType<const UFunction*>())
			{
				if (!GetDefault<UMVVMDeveloperProjectSettings>()->IsConversionFunctionAllowed(FunctionOrWrapperFunction.Get<const UFunction*>()))
				{
					AddMessageForBinding(Binding, BlueprintView, FText::Format(LOCTEXT("ConversionFunctionNotAllow", "The conversion function {0} is not allowed."),
						FText::FromName(FunctionOrWrapperFunction.Get<const UFunction*>()->GetFName())),
						EBindingMessageType::Error
					);
					bIsBindingsValid = false;
					return MakeError();
				}
			}
			else
			{
				AddMessageForBinding(Binding, BlueprintView, LOCTEXT("ConversionFunctionNodeNotAllow", "The conversion function node is not allowed."), EBindingMessageType::Error);
				bIsBindingsValid = false;
				return MakeError();
			}
		}
		return MakeValue(ConversionFunction);
	};

	auto TestExecutionMode = [this, BlueprintView](FMVVMBlueprintViewBinding& Binding)
	{
		if (Binding.bOverrideExecutionMode)
		{
			if (!GetDefault<UMVVMDeveloperProjectSettings>()->IsExecutionModeAllowed(Binding.OverrideExecutionMode))
			{
				AddMessageForBinding(Binding, BlueprintView, LOCTEXT("NotAllowedExecutionMode", "The binding has a restricted execution mode."), EBindingMessageType::Error);
				return false;
			}
		}
		return true;
	};

	auto GetSetterPath = [this, Class, BlueprintView](FMVVMBlueprintViewBinding& Binding, int32 BindingIndex, bool bIsForwardBinding) -> TValueOrError<TArray<UE::MVVM::FMVVMConstFieldVariant>, void>
	{

		const FBindingDestinationContext* DestinationContext = BindingDestinationContexts.FindByPredicate([BindingIndex, bIsForwardBinding](const FBindingDestinationContext& Other)
			{
				return Other.BindingIndex == BindingIndex && Other.bIsForwardBinding == bIsForwardBinding;
			});

		if (DestinationContext == nullptr)
		{
			AddMessageForBinding(Binding, BlueprintView, LOCTEXT("CouldNotFindDestination", "Could not find the pre compiled destination."), EBindingMessageType::Error);
			return MakeError();
		}

		TArray<UE::MVVM::FMVVMConstFieldVariant> SetterPath;
		if (!DestinationContext->bCanBeSetInNative)
		{
			check(!DestinationContext->GeneratedFunctionName.IsNone());
			UFunction* FoundFunction = Class->FindFunctionByName(DestinationContext->GeneratedFunctionName);
			if (FoundFunction == nullptr)
			{
				AddMessageForBinding(Binding, BlueprintView, LOCTEXT("CouldNotFindDestinationBPFunction", "Could not find the generated destination function."), EBindingMessageType::Error);
				return MakeError();
			}
			FMVVMBlueprintPropertyPath DestinationPPropertyPath;
			DestinationPPropertyPath.SetWidgetName(Class->ClassGeneratedBy->GetFName());
			DestinationPPropertyPath.SetPropertyPath(BlueprintView->GetOuterUMVVMWidgetBlueprintExtension_View()->GetWidgetBlueprint(), UE::MVVM::FMVVMConstFieldVariant(FoundFunction));
			SetterPath = CreateBindingDestinationPath(BlueprintView, Class, DestinationPPropertyPath);
		}
		else
		{
			const FMVVMBlueprintPropertyPath& DestinationPath = bIsForwardBinding ? Binding.DestinationPath : Binding.SourcePath;
			SetterPath = CreateBindingDestinationPath(BlueprintView, Class, DestinationPath);
		}

		if (!IsPropertyPathValid(SetterPath))
		{
			const FMVVMBlueprintPropertyPath& DestinationPath = bIsForwardBinding ? Binding.DestinationPath : Binding.SourcePath;
			AddMessageForBinding(Binding, BlueprintView, FText::Format(LOCTEXT("PropertyPathIsInvalid", "The property path '{0}' is invalid."),
				PropertyPathToText(Class, BlueprintView, DestinationPath)),
				EBindingMessageType::Error
			);
			return MakeError();
		}
		return MakeValue(SetterPath);
	};

	for (const FBindingSourceContext& BindingSourceContext : BindingSourceContexts)
	{
		FMVVMBlueprintViewBinding* BindingPtr = BlueprintView->GetBindingAt(BindingSourceContext.BindingIndex);
		check(BindingPtr);
		FMVVMBlueprintViewBinding& Binding = *BindingPtr;

		FMVVMViewBlueprintCompiler* Self = this;
		auto AddFieldId = [Self](const UClass* SourceContextClass, bool bNotifyFieldValueChangedRequired, EMVVMBindingMode BindingMode, FName FieldToListenTo) -> TValueOrError<FCompiledBindingLibraryCompiler::FFieldIdHandle, FText>
		{
			if (!IsOneTimeBinding(BindingMode) && bNotifyFieldValueChangedRequired)
			{
				return Self->BindingLibraryCompiler.AddFieldId(SourceContextClass, FieldToListenTo);
			}
			return MakeValue(FCompiledBindingLibraryCompiler::FFieldIdHandle());
		};

		if (!TestExecutionMode(Binding))
		{
			bIsBindingsValid = false;
			continue;
		}

		const bool bIsComplexBinding = BindingSourceContext.ComplexConversionFunctionContextIndex != INDEX_NONE;
		bool bFieldIdNeeded = !IsOneTimeBinding(Binding.BindingType);

		TValueOrError<FCompiledBindingLibraryCompiler::FFieldIdHandle, FText> AddFieldResult = AddFieldId(BindingSourceContext.SourceClass, true, Binding.BindingType, BindingSourceContext.FieldId.GetFieldName());
		if (AddFieldResult.HasError())
		{
			bFieldIdNeeded = false;
			// For complex binding, at least one must be true
			if (!bIsComplexBinding)
			{
				AddMessageForBinding(Binding, BlueprintView, FText::Format(LOCTEXT("CouldNotCreateSource", "Could not create source. {0}"),
					AddFieldResult.GetError()), EBindingMessageType::Error);
				bIsBindingsValid = false;
				continue;
			}
		}
		else if (bIsComplexBinding)
		{
			if (ensure(ComplexConversionFunctionContexts.IsValidIndex(BindingSourceContext.ComplexConversionFunctionContextIndex)))
			{
				ComplexConversionFunctionContexts[BindingSourceContext.ComplexConversionFunctionContextIndex].bHasValidFieldId = true;
			}
		}

		TArray<UE::MVVM::FMVVMConstFieldVariant> SetterPath;
		{
			TValueOrError<TArray<UE::MVVM::FMVVMConstFieldVariant>, void> SetterPathResult = GetSetterPath(Binding, BindingSourceContext.BindingIndex, BindingSourceContext.bIsForwardBinding);
			if (SetterPathResult.HasError())
			{
				bIsBindingsValid = false;
				continue;
			}
			SetterPath = SetterPathResult.StealValue();
		}

		const UFunction* ConversionFunction = nullptr;
		{
			TValueOrError<const UFunction*, void> ConversionFunctionResult = GetConversionFunction(Binding, BindingSourceContext.bIsForwardBinding);
			if (ConversionFunctionResult.HasError())
			{
				bIsBindingsValid = false;
				continue;
			}
			ConversionFunction = ConversionFunctionResult.StealValue();
		}

		TValueOrError<FCompiledBinding, FText> AddBindingResult = CreateCompiledBinding(Class, BindingSourceContext.PropertyPath, SetterPath, ConversionFunction, bIsComplexBinding);
		if (AddBindingResult.HasError())
		{
			AddMessageForBinding(Binding, BlueprintView,
				FText::Format(LOCTEXT("CouldNotCreateBinding", "Could not create binding. {0}"), AddBindingResult.GetError()),
				EBindingMessageType::Error
			);
			bIsBindingsValid = false;
			continue;
		}

		FCompilerBinding NewBinding;
		NewBinding.BindingIndex = BindingSourceContext.BindingIndex;
		NewBinding.UserWidgetPropertyContextIndex = BindingSourceContext.UserWidgetPropertyContextIndex;
		NewBinding.SourceCreatorContextIndex = BindingSourceContext.SourceCreatorContextIndex;
		NewBinding.ComplexConversionFunctionContextIndex = BindingSourceContext.ComplexConversionFunctionContextIndex;
		NewBinding.bSourceIsUserWidget = BindingSourceContext.bIsRootWidget;
		NewBinding.bFieldIdNeeded = bFieldIdNeeded;
		NewBinding.bIsForwardBinding = BindingSourceContext.bIsForwardBinding;

		NewBinding.CompiledBinding = AddBindingResult.StealValue();
		if (AddFieldResult.HasValue())
		{
			NewBinding.FieldIdHandle = AddFieldResult.StealValue();
		}

		CompilerBindings.Emplace(NewBinding);
	}


	for (const FSimpleBindingContext& BindingContext : SimpleBindingContexts)
	{
		FMVVMBlueprintViewBinding* BindingPtr = BlueprintView->GetBindingAt(BindingContext.BindingIndex);
		check(BindingPtr);
		FMVVMBlueprintViewBinding& Binding = *BindingPtr;

		if (!TestExecutionMode(Binding))
		{
			bIsBindingsValid = false;
			continue;
		}

		if (!IsOneTimeBinding(Binding.BindingType))
		{
			AddMessageForBinding(Binding, BlueprintView, LOCTEXT("FunctionRequiresOneTimeBinding", "There is no source. The binding must be a OneTime binding."), EBindingMessageType::Error);
			bIsBindingsValid = false;
			continue;
		}

		TArray<UE::MVVM::FMVVMConstFieldVariant> SetterPath;
		{
			TValueOrError<TArray<UE::MVVM::FMVVMConstFieldVariant>, void> SetterPathResult = GetSetterPath(Binding, BindingContext.BindingIndex, BindingContext.bIsForwardBinding);
			if (SetterPathResult.HasError())
			{
				bIsBindingsValid = false;
				continue;
			}
			SetterPath = SetterPathResult.StealValue();
		}

		const UFunction* ConversionFunction = nullptr;
		{
			TValueOrError<const UFunction*, void> ConversionFunctionResult = GetConversionFunction(Binding, BindingContext.bIsForwardBinding);
			if (ConversionFunctionResult.HasError())
			{
				bIsBindingsValid = false;
				continue;
			}
			ConversionFunction = ConversionFunctionResult.StealValue();
		}

		TArrayView<const UE::MVVM::FMVVMConstFieldVariant> GetterFields;
		const bool bIsComplexBinding = BindingContext.ComplexConversionFunctionContextIndex != INDEX_NONE;
		TValueOrError<FCompiledBinding, FText> AddBindingResult = CreateCompiledBinding(Class, GetterFields, SetterPath, ConversionFunction, bIsComplexBinding);
		if (AddBindingResult.HasError())
		{
			AddMessageForBinding(Binding, BlueprintView,
				FText::Format(LOCTEXT("CouldNotCreateBinding", "Could not create binding. {0}"), AddBindingResult.GetError()),
				EBindingMessageType::Error
			);
			bIsBindingsValid = false;
			continue;
		}

		FCompilerBinding NewBinding;
		NewBinding.BindingIndex = BindingContext.BindingIndex;
		NewBinding.bFieldIdNeeded = !IsOneTimeBinding(Binding.BindingType);
		NewBinding.bIsForwardBinding = BindingContext.bIsForwardBinding;
		NewBinding.bNeedsValidSource = false;

		NewBinding.CompiledBinding = AddBindingResult.StealValue();

		CompilerBindings.Emplace(NewBinding);
	}

	if (bIsBindingsValid)
	{
		// Confirm that at least one source is valid for each pin of a complex conversion function.
		for (const FComplexConversionFunctionContext& ComplexConversionFunctionContext : ComplexConversionFunctionContexts)
		{
			if (!ComplexConversionFunctionContext.bHasValidFieldId && ComplexConversionFunctionContext.bNeedsValidSource)
			{
				FMVVMBlueprintViewBinding* BindingPtr = BlueprintView->GetBindingAt(ComplexConversionFunctionContext.BindingIndex);
				check(BindingPtr);
				AddMessageForBinding(*BindingPtr, BlueprintView, LOCTEXT("CouldNotCreateSourceForConversionFunction", "There is no source. The binding must be a OneTime binding."), EBindingMessageType::Error);
				bIsBindingsValid = false;
			}
		}
	}

	return bIsBindingsValid;
}


bool FMVVMViewBlueprintCompiler::CompileBindings(const FCompiledBindingLibraryCompiler::FCompileResult& CompileResult, UWidgetBlueprintGeneratedClass* Class, UMVVMBlueprintView* BlueprintView, UMVVMViewClass* ViewExtension)
{
	if (!bIsBindingsValid)
	{
		return false;
	}

	IConsoleVariable* CVarDefaultExecutionMode = IConsoleManager::Get().FindConsoleVariable(TEXT("MVVM.DefaultExecutionMode"));
	if (!CVarDefaultExecutionMode)
	{
		WidgetBlueprintCompilerContext.MessageLog.Error(*LOCTEXT("CantFindDefaultExecutioMode", "The default execution mode cannot be found.").ToString());
		return false;
	}

	for (const FCompilerBinding& CompileBinding : CompilerBindings)
	{
		// PropertyBinding needs a valid CompileBinding.BindingIndex.
		check(CompileBinding.Type != ECompilerBindingType::PropertyBinding || (CompileBinding.BindingIndex >= 0 && CompileBinding.BindingIndex < BlueprintView->GetNumBindings()));
		FMVVMBlueprintViewBinding* ViewBinding = CompileBinding.Type == ECompilerBindingType::PropertyBinding
			? BlueprintView->GetBindingAt(CompileBinding.BindingIndex)
			: nullptr;


		FMVVMViewClass_CompiledBinding NewBinding;
		const bool bIsSourceSelf = CompileBinding.bSourceIsUserWidget;
		if (!bIsSourceSelf)
		{
			if (CompilerUserWidgetPropertyContexts.IsValidIndex(CompileBinding.UserWidgetPropertyContextIndex))
			{
				NewBinding.SourcePropertyName = CompilerUserWidgetPropertyContexts[CompileBinding.UserWidgetPropertyContextIndex].PropertyName;
			}
			else if (CompilerSourceCreatorContexts.IsValidIndex(CompileBinding.SourceCreatorContextIndex))
			{
				NewBinding.SourcePropertyName = CompilerSourceCreatorContexts[CompileBinding.SourceCreatorContextIndex].ViewModelContext.GetViewModelName();
			}
			else if (CompileBinding.bNeedsValidSource)
			{
				WidgetBlueprintCompilerContext.MessageLog.Error(*LOCTEXT("InvalidSourceInternal", "Internal error. The binding doesn't have a valid source.").ToString());
				bIsBindingsValid = false;
				return false;
			}
		}

		auto AddErroMessage = [this, ViewBinding, BlueprintView](const FText& ErrorMessage)
		{
				if (ViewBinding)
				{
					AddMessageForBinding(*ViewBinding, BlueprintView, ErrorMessage, EBindingMessageType::Error);
				}
				else
				{
					WidgetBlueprintCompilerContext.MessageLog.Error(*ErrorMessage.ToString());
				}
		};

		const FMVVMVCompiledFieldId* CompiledFieldId = CompileResult.FieldIds.Find(CompileBinding.FieldIdHandle);
		if (CompiledFieldId == nullptr && CompileBinding.bFieldIdNeeded)
		{
			AddErroMessage(FText::Format(LOCTEXT("FieldIdNotGenerated", "Could not generate field ID for property '{0}'."), FText::FromName(NewBinding.SourcePropertyName)));
			bIsBindingsValid = false;
			continue;
		}

		const FMVVMVCompiledBinding* CompiledBinding = CompileResult.Bindings.Find(CompileBinding.CompiledBinding.BindingHandle);
		if (CompiledBinding == nullptr && CompileBinding.Type != ECompilerBindingType::ViewModelDynamic)
		{
			AddErroMessage(LOCTEXT("CompiledBindingNotGenerated", "Could not generate compiled binding."));
			bIsBindingsValid = false;
			continue;
		}

		bool bIsOptional = false;
		if (!bIsSourceSelf)
		{
			const FMVVMBlueprintViewModelContext* ViewModelContext = nullptr;
			if (CompilerUserWidgetPropertyContexts.IsValidIndex(CompileBinding.UserWidgetPropertyContextIndex))
			{
				if (CompilerUserWidgetPropertyContexts[CompileBinding.UserWidgetPropertyContextIndex].ViewModelId.IsValid())
				{
					ViewModelContext = BlueprintView->FindViewModel(CompilerUserWidgetPropertyContexts[CompileBinding.UserWidgetPropertyContextIndex].ViewModelId);
					if (ViewModelContext == nullptr)
					{
						AddErroMessage(LOCTEXT("CompiledBindingWithInvalidIVewModelId", "Internal error: the viewmodel became invalid."));
						bIsBindingsValid = false;
						continue;
					}
				}
			}
			else if (CompilerSourceCreatorContexts.IsValidIndex(CompileBinding.SourceCreatorContextIndex))
			{
				ViewModelContext = &CompilerSourceCreatorContexts[CompileBinding.SourceCreatorContextIndex].ViewModelContext;
			}

			if (ViewModelContext)
			{
				bIsOptional = ViewModelContext->bOptional || ViewModelContext->CreationType == EMVVMBlueprintViewModelContextCreationType::Manual;
			}
		}

		if (CompileBinding.Type == ECompilerBindingType::ViewModelDynamic)
		{
			int32 FoundSourceCreatorIndex = ViewExtension->SourceCreators.IndexOfByPredicate([LookFor = CompileBinding.DynamicViewModelName](const FMVVMViewClass_SourceCreator& Other)
				{
					return Other.GetSourceName() == LookFor;
				});

			check(FoundSourceCreatorIndex < std::numeric_limits<int8>::max()); // the index is saved as a int8
			NewBinding.EvaluateSourceCreatorIndex = FoundSourceCreatorIndex;
			if (NewBinding.EvaluateSourceCreatorIndex == INDEX_NONE)
			{
				FText ErrorMessage = FText::Format(LOCTEXT("CompiledViewModelDynamicBindingNotGenerated", "Was not able to find the source for {0}."), FText::FromName(CompileBinding.DynamicViewModelName));
				WidgetBlueprintCompilerContext.MessageLog.Error(*ErrorMessage.ToString());
				bIsBindingsValid = false;
				continue;
			}
		}

		FText WrongBindingTypeInternalErrorMessage = LOCTEXT("WrongBindingTypeInternalError", "Internal Error. Wrong binding type.");
		// FieldId can be null if it's a one time binding
		NewBinding.FieldId = CompiledFieldId ? *CompiledFieldId : FMVVMVCompiledFieldId();
		NewBinding.Binding = CompiledBinding ? *CompiledBinding : FMVVMVCompiledBinding();
		NewBinding.Flags = 0;
		if (ViewBinding)
		{
			if (CompileBinding.Type != ECompilerBindingType::PropertyBinding)
			{
				WidgetBlueprintCompilerContext.MessageLog.Error(*WrongBindingTypeInternalErrorMessage.ToString());
				bIsBindingsValid = false;
				continue;
			}

			bool bExecuteAtInitialization = CompileBinding.bIsForwardBinding;
			if (bExecuteAtInitialization && CompileBinding.ComplexConversionFunctionContextIndex != INDEX_NONE)
			{
				bExecuteAtInitialization = !ComplexConversionFunctionContexts[CompileBinding.ComplexConversionFunctionContextIndex].bExecAtInitGenerated;
				ComplexConversionFunctionContexts[CompileBinding.ComplexConversionFunctionContextIndex].bExecAtInitGenerated = true;
			}

			NewBinding.ExecutionMode = ViewBinding->bOverrideExecutionMode ? ViewBinding->OverrideExecutionMode : (EMVVMExecutionMode)CVarDefaultExecutionMode->GetInt();;
			NewBinding.EditorId = ViewBinding->BindingId;

			NewBinding.Flags |= (ViewBinding->bEnabled) ? FMVVMViewClass_CompiledBinding::EBindingFlags::EnabledByDefault : 0;
			NewBinding.Flags |= (bExecuteAtInitialization) ? FMVVMViewClass_CompiledBinding::EBindingFlags::ExecuteAtInitialization : 0;
			NewBinding.Flags |= (IsOneTimeBinding(ViewBinding->BindingType)) ? FMVVMViewClass_CompiledBinding::EBindingFlags::OneTime : 0;
			NewBinding.Flags |= (bIsOptional) ? FMVVMViewClass_CompiledBinding::EBindingFlags::ViewModelOptional : 0;
			NewBinding.Flags |= (ViewBinding->bOverrideExecutionMode) ? FMVVMViewClass_CompiledBinding::EBindingFlags::OverrideExecuteMode : 0;
			NewBinding.Flags |= (bIsSourceSelf) ? FMVVMViewClass_CompiledBinding::EBindingFlags::SourceObjectIsSelf : 0;
		}
		else
		{
			if (CompileBinding.Type != ECompilerBindingType::ViewModelDynamic)
			{
				WidgetBlueprintCompilerContext.MessageLog.Error(*WrongBindingTypeInternalErrorMessage.ToString());
				bIsBindingsValid = false;
				continue;
			}
			NewBinding.ExecutionMode = EMVVMExecutionMode::Immediate;
			NewBinding.EditorId = FGuid();

			NewBinding.Flags |= FMVVMViewClass_CompiledBinding::EBindingFlags::EnabledByDefault;
			NewBinding.Flags |= (bIsOptional) ? FMVVMViewClass_CompiledBinding::EBindingFlags::ViewModelOptional : 0;
			NewBinding.Flags |= FMVVMViewClass_CompiledBinding::EBindingFlags::OverrideExecuteMode; // The mode needs to be Immediate.
			NewBinding.Flags |= (bIsSourceSelf) ? FMVVMViewClass_CompiledBinding::EBindingFlags::SourceObjectIsSelf : 0;
		}

		ViewExtension->CompiledBindings.Emplace(MoveTemp(NewBinding));
	}

	return bIsBindingsValid;
}


TValueOrError<FMVVMViewBlueprintCompiler::FCompiledBinding, FText> FMVVMViewBlueprintCompiler::CreateCompiledBinding(const UWidgetBlueprintGeneratedClass* Class, TArrayView<const UE::MVVM::FMVVMConstFieldVariant> GetterFields, TArrayView<const UE::MVVM::FMVVMConstFieldVariant> SetterFields, const UFunction* ConversionFunction, bool bIsComplexBinding)
{
	FCompiledBinding Result;

	if (ConversionFunction != nullptr)
	{
		TValueOrError<FCompiledBindingLibraryCompiler::FFieldPathHandle, FText> FieldPathResult = BindingLibraryCompiler.AddConversionFunctionFieldPath(Class, ConversionFunction);
		if (FieldPathResult.HasError())
		{
			return MakeError(FText::Format(LOCTEXT("CouldNotCreateConversionFunctionFieldPath", "Couldn't create the conversion function field path '{0}'. {1}")
				, FText::FromString(ConversionFunction->GetPathName())
				, FieldPathResult.GetError()));
		}
		Result.ConversionFunction = FieldPathResult.StealValue();
		Result.bIsConversionFunctionComplex = bIsComplexBinding;

		// Sanity check
		if (bIsComplexBinding && !BindingHelper::IsValidForComplexRuntimeConversion(ConversionFunction))
		{
			return MakeError(LOCTEXT("ConversionFunctionIsNotComplex", "Internal Error. The complex conversion function does not respect the prerequisite."));
		}
	}

	if (!Result.bIsConversionFunctionComplex)
	{
		// Generate a path to read the value at runtime
		TValueOrError<TArray<FMVVMConstFieldVariant>, FText> GeneratedField = FieldPathHelper::GenerateFieldPathList(GetterFields, true);
		if (GeneratedField.HasError())
		{
			return MakeError(FText::Format(Private::CouldNotCreateSourceFieldPathFormat
				, ::UE::MVVM::FieldPathHelper::ToText(GetterFields)
				, GeneratedField.GetError()));
		}

		TValueOrError<FCompiledBindingLibraryCompiler::FFieldPathHandle, FText> FieldPathResult = BindingLibraryCompiler.AddFieldPath(GeneratedField.GetValue(), true);
		if (FieldPathResult.HasError())
		{
			return MakeError(FText::Format(Private::CouldNotCreateSourceFieldPathFormat
				, ::UE::MVVM::FieldPathHelper::ToText(GetterFields)
				, FieldPathResult.GetError()));
		}
		Result.SourceRead = FieldPathResult.StealValue();
	}

	{
		static const FText CouldNotCreateDestinationFieldPathFormat = LOCTEXT("CouldNotCreateDestinationFieldPath", "Couldn't create the destination field path '{0}'. {1}");

		TValueOrError<TArray<FMVVMConstFieldVariant>, FText> GeneratedField = FieldPathHelper::GenerateFieldPathList(SetterFields, false);
		if (GeneratedField.HasError())
		{
			return MakeError(FText::Format(CouldNotCreateDestinationFieldPathFormat
				, ::UE::MVVM::FieldPathHelper::ToText(SetterFields)
				, GeneratedField.GetError()));
		}

		TValueOrError<FCompiledBindingLibraryCompiler::FFieldPathHandle, FText> FieldPathResult = BindingLibraryCompiler.AddFieldPath(GeneratedField.GetValue(), false);
		if (FieldPathResult.HasError())
		{
			return MakeError(FText::Format(CouldNotCreateDestinationFieldPathFormat
				, ::UE::MVVM::FieldPathHelper::ToText(SetterFields)
				, FieldPathResult.GetError()));
		}
		Result.DestinationWrite = FieldPathResult.StealValue();
	}

	// Generate the binding
	TValueOrError<FCompiledBindingLibraryCompiler::FBindingHandle, FText> BindingResult = Result.bIsConversionFunctionComplex
		? BindingLibraryCompiler.AddComplexBinding(Result.DestinationWrite, Result.ConversionFunction)
		: BindingLibraryCompiler.AddBinding(Result.SourceRead, Result.DestinationWrite, Result.ConversionFunction);
	if (BindingResult.HasError())
	{
		return MakeError(BindingResult.StealError());
	}
	Result.BindingHandle = BindingResult.StealValue();

	return MakeValue(Result);
}


const FMVVMViewBlueprintCompiler::FCompilerSourceCreatorContext* FMVVMViewBlueprintCompiler::FindViewModelSource(FGuid Id) const
{
	return CompilerSourceCreatorContexts.FindByPredicate([Id](const FCompilerSourceCreatorContext& Other)
		{
			return Other.Type == ECompilerSourceCreatorType::ViewModel ? Other.ViewModelContext.GetViewModelId() == Id : false;
		});
}


TValueOrError<FMVVMViewBlueprintCompiler::FBindingSourceContext, FText> FMVVMViewBlueprintCompiler::CreateBindingSourceContext(const UMVVMBlueprintView* BlueprintView, const UWidgetBlueprintGeneratedClass* Class, const FMVVMBlueprintPropertyPath& PropertyPath, bool bIsOneTimeBinding)
{
	if (PropertyPath.IsEmpty())
	{
		ensureAlways(false);
		return MakeError(LOCTEXT("EmptyPropertyPath", "Empty property path found. This is ilegal."));
	}

	FBindingSourceContext Result;
	if (PropertyPath.IsFromViewModel())
	{
		Result.bIsRootWidget = false;

		const FMVVMBlueprintViewModelContext* SourceViewModelContext = BlueprintView->FindViewModel(PropertyPath.GetViewModelId());
		check(SourceViewModelContext);
		const FName SourceName = SourceViewModelContext->GetViewModelName();
		Result.UserWidgetPropertyContextIndex = CompilerUserWidgetPropertyContexts.IndexOfByPredicate([SourceName](const FCompilerUserWidgetPropertyContext& Other) { return Other.PropertyName == SourceName; });
		check(Result.UserWidgetPropertyContextIndex != INDEX_NONE);

		Result.SourceClass = CompilerUserWidgetPropertyContexts[Result.UserWidgetPropertyContextIndex].Class;
		Result.PropertyPath = CreatePropertyPath(Class, CompilerUserWidgetPropertyContexts[Result.UserWidgetPropertyContextIndex].PropertyName, PropertyPath.GetFields(Class));
	}
	else if (PropertyPath.IsFromWidget())
	{
		const FName SourceName = PropertyPath.GetWidgetName();
		Result.bIsRootWidget = SourceName == Class->ClassGeneratedBy->GetFName();
		if (Result.bIsRootWidget)
		{
			Result.UserWidgetPropertyContextIndex = INDEX_NONE;
			Result.SourceClass = const_cast<UWidgetBlueprintGeneratedClass*>(Class);
			Result.PropertyPath = CreatePropertyPath(Class, FName(), PropertyPath.GetFields(Class));
		}
		else
		{
			Result.UserWidgetPropertyContextIndex = CompilerUserWidgetPropertyContexts.IndexOfByPredicate([SourceName](const FCompilerUserWidgetPropertyContext& Other) { return Other.PropertyName == SourceName; });
			if (!CompilerUserWidgetPropertyContexts.IsValidIndex(Result.UserWidgetPropertyContextIndex))
			{
				return MakeError(LOCTEXT("InvalidUserWidgetPropertyContextIndexInternal", "Internal error. UserWidgetPropertyContextIndex is invalid."));
			}
			Result.SourceClass = CompilerUserWidgetPropertyContexts[Result.UserWidgetPropertyContextIndex].Class;
			Result.PropertyPath = CreatePropertyPath(Class, CompilerUserWidgetPropertyContexts[Result.UserWidgetPropertyContextIndex].PropertyName, PropertyPath.GetFields(Class));
		}
	}
	else
	{
		ensureAlwaysMsgf(false, TEXT("Not supported yet."));
	}

	// The path may contains another INotifyFieldValueChanged
	TValueOrError<FieldPathHelper::FParsedNotifyBindingInfo, FText> BindingInfoResult = FieldPathHelper::GetNotifyBindingInfoFromFieldPath(Class, Result.PropertyPath);
	if (BindingInfoResult.HasError())
	{
		return MakeError(BindingInfoResult.StealError());
	}
	const FieldPathHelper::FParsedNotifyBindingInfo& BindingInfo = BindingInfoResult.GetValue();
	Result.FieldId = BindingInfo.NotifyFieldId;

	if (BindingInfo.ViewModelIndex < 1 && BindingInfo.NotifyFieldClass && Result.SourceClass)
	{
		if (!Result.SourceClass->IsChildOf(BindingInfo.NotifyFieldClass))
		{
			return MakeError(LOCTEXT("InvalidNotifyFieldClassInternal", "Internal error. The viewmodel class doesn't matches."));
		}
	}

	// The INotifyFieldValueChanged/viewmodel is not the first and only INotifyFieldValueChanged/viewmodel property path.
	//Create a new source in PropertyPath creator mode. Create a special binding to update the viewmodel when it changes.
	//This binding (calling this function) will use the new source.
	if (BindingInfo.ViewModelIndex >= 1 && !bIsOneTimeBinding)
	{
		if (!GetDefault<UMVVMDeveloperProjectSettings>()->bAllowLongSourcePath)
		{
			return MakeError(LOCTEXT("DynamicSourceEntryNotSupport", "Long source entry is not supported. Add the viewmodel manually."));
		}

		//NB. The for loop order is important. The order is used in UMVVMView::SetViewModelInternal when enabling the bindings
		int32 SourceCreatorContextIndex = INDEX_NONE;
		for (int32 DynamicIndex = 1; DynamicIndex <= BindingInfo.ViewModelIndex; ++DynamicIndex)
		{
			if (!Result.PropertyPath.IsValidIndex(DynamicIndex))
			{
				return MakeError(LOCTEXT("DynamicSourceEntryInternalIndex", "Internal error. The source index is not valid."));
			}

			FName NewSourceName;
			FName ParentSourceName;
			FString NewSourcePropertyPath;
			{
				TStringBuilder<512> PropertyPathBuilder;
				TStringBuilder<512> DynamicNameBuilder;
				for (int32 Index = 0; Index <= DynamicIndex; ++Index)
				{
					if (Index > 0)
					{
						PropertyPathBuilder << TEXT('.');
						DynamicNameBuilder << TEXT('_');
					}
					PropertyPathBuilder << Result.PropertyPath[Index].GetName();
					DynamicNameBuilder << Result.PropertyPath[Index].GetName();

					if (Index == DynamicIndex - 1)
					{
						ParentSourceName = FName(DynamicNameBuilder.ToString());
					}
				}

				NewSourceName = FName(DynamicNameBuilder.ToString());
				NewSourcePropertyPath = PropertyPathBuilder.ToString();
			}

			// Did we already create the new source?
			int32 PreviousSourceCreatorContextIndex = SourceCreatorContextIndex;
			SourceCreatorContextIndex = CompilerSourceCreatorContexts.IndexOfByPredicate([NewSourceName](const FCompilerSourceCreatorContext& Other)
				{
					return Other.Type == ECompilerSourceCreatorType::ViewModelDynamic
						&& Other.ViewModelContext.GetViewModelName() == NewSourceName;
				});
			if (SourceCreatorContextIndex == INDEX_NONE)
			{
				// Create the new source
				{
					const UClass* ViewModelClass = Cast<const UClass>(Result.PropertyPath[DynamicIndex+1].GetOwner());
					if (!ViewModelClass)
					{
						return MakeError(FText::GetEmpty());
					}

					FCompilerSourceCreatorContext SourceCreatorContext;
					SourceCreatorContext.Type = ECompilerSourceCreatorType::ViewModelDynamic;
					SourceCreatorContext.DynamicParentSourceName = ParentSourceName;
					SourceCreatorContext.ViewModelContext = FMVVMBlueprintViewModelContext(ViewModelClass, NewSourceName);
					SourceCreatorContext.ViewModelContext.bCreateSetterFunction = false;
					SourceCreatorContext.ViewModelContext.bOptional = false;
					SourceCreatorContext.ViewModelContext.CreationType = EMVVMBlueprintViewModelContextCreationType::PropertyPath;
					SourceCreatorContext.ViewModelContext.ViewModelPropertyPath = NewSourcePropertyPath;

					TValueOrError<FCompiledBindingLibraryCompiler::FFieldPathHandle, FText> ReadFieldPathResult = AddObjectFieldPath(BindingLibraryCompiler, Class, SourceCreatorContext.ViewModelContext.ViewModelPropertyPath, SourceCreatorContext.ViewModelContext.GetViewModelClass());
					if (ReadFieldPathResult.HasError())
					{
						return MakeError(FText::Format(LOCTEXT("DynamicSourceEntryInvalidPath", "Internal error. {0}."), ReadFieldPathResult.StealError()));
					}
					SourceCreatorContext.ReadPropertyPath = ReadFieldPathResult.StealValue();

					SourceCreatorContextIndex = CompilerSourceCreatorContexts.Add(MoveTemp(SourceCreatorContext));
				}

				// Create the binding to update the source when it changes.
				{
					FCompilerBinding NewCompilerBinding;
					NewCompilerBinding.Type = ECompilerBindingType::ViewModelDynamic;
					NewCompilerBinding.bSourceIsUserWidget = false;
					NewCompilerBinding.bIsForwardBinding = true;
					NewCompilerBinding.bFieldIdNeeded = true;
					NewCompilerBinding.DynamicViewModelName = NewSourceName;

					if (PreviousSourceCreatorContextIndex == INDEX_NONE)
					{
						NewCompilerBinding.UserWidgetPropertyContextIndex = Result.UserWidgetPropertyContextIndex;
					}
					else
					{
						NewCompilerBinding.SourceCreatorContextIndex = PreviousSourceCreatorContextIndex;
					}

					{
						const UClass* OwnerClass = Cast<const UClass>(Result.PropertyPath[DynamicIndex].GetOwner());
						if (!OwnerClass)
						{
							return MakeError(FText::GetEmpty());
						}

						TValueOrError<FCompiledBindingLibraryCompiler::FFieldIdHandle, FText> AddFieldResult = BindingLibraryCompiler.AddFieldId(OwnerClass, Result.PropertyPath[DynamicIndex].GetName());
						if (AddFieldResult.HasError())
						{
							return MakeError(AddFieldResult.StealError());
						}
						NewCompilerBinding.FieldIdHandle = AddFieldResult.StealValue();
					}

					CompilerBindings.Add(MoveTemp(NewCompilerBinding));
				}
			}
		}

		Result.SourceClass = BindingInfo.NotifyFieldClass;
		Result.FieldId = BindingInfo.NotifyFieldId;
		Result.UserWidgetPropertyContextIndex = INDEX_NONE;
		Result.SourceCreatorContextIndex = SourceCreatorContextIndex;
		Result.bIsRootWidget = false;
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
		const int32 DestinationVariableContextIndex = CompilerUserWidgetPropertyContexts.IndexOfByPredicate([DestinationName](const FCompilerUserWidgetPropertyContext& Other) { return Other.PropertyName == DestinationName; });
		check(DestinationVariableContextIndex != INDEX_NONE);

		return CreatePropertyPath(Class, CompilerUserWidgetPropertyContexts[DestinationVariableContextIndex].PropertyName, PropertyPath.GetFields(Class));
	}
	else if (PropertyPath.IsFromWidget())
	{
		FName DestinationName = PropertyPath.GetWidgetName();
		checkf(!DestinationName.IsNone(), TEXT("The destination should have been checked and set bAreSourceContextsValid."));
		const bool bSourceIsUserWidget = DestinationName == Class->ClassGeneratedBy->GetFName();
		if (bSourceIsUserWidget)
		{
			return CreatePropertyPath(Class, FName(), PropertyPath.GetFields(Class));
		}
		else
		{
			const int32 DestinationVariableContextIndex = CompilerUserWidgetPropertyContexts.IndexOfByPredicate([DestinationName](const FCompilerUserWidgetPropertyContext& Other) { return Other.PropertyName == DestinationName; });
			if (ensureAlwaysMsgf(DestinationVariableContextIndex != INDEX_NONE, TEXT("Could not find source context for destination '%s'"), *DestinationName.ToString()))
			{
				return CreatePropertyPath(Class, CompilerUserWidgetPropertyContexts[DestinationVariableContextIndex].PropertyName, PropertyPath.GetFields(Class));
			}
		}
	}
	else
	{
		ensureAlwaysMsgf(false, TEXT("Not supported yet."));
		return CreatePropertyPath(Class, FName(), PropertyPath.GetFields(Class));
	}

	return TArray<FMVVMConstFieldVariant>();
}

TArray<FMVVMConstFieldVariant> FMVVMViewBlueprintCompiler::CreatePropertyPath(const UClass* Class, FName PropertyName, TArray<FMVVMConstFieldVariant> Properties)
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


bool FMVVMViewBlueprintCompiler::IsPropertyPathValid(TArrayView<const FMVVMConstFieldVariant> PropertyPath)
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

bool FMVVMViewBlueprintCompiler::CanBeSetInNative(TArrayView<const FMVVMConstFieldVariant> PropertyPath)
{
	check(IsPropertyPathValid(PropertyPath));
	
	for (int32 Index = PropertyPath.Num() - 1; Index >= 0; --Index)
	{
		const FMVVMConstFieldVariant& Variant = PropertyPath[Index];
		// Stop the algo if the path is already a function.
		if (Variant.IsFunction())
		{
			return true;
		}

		// If the BP is defined in BP and has Net flags or FieldNotify flag, then the VaraibleSet K2Node need to be used to generate the proper byte-code.
		if (Variant.IsProperty())
		{
			// If it's an object then the path before the object doesn't matter.
			if (const FObjectPropertyBase* PropertyBase = CastField<const FObjectPropertyBase>(Variant.GetProperty()))
			{
				bool bLastPath = Index >= PropertyPath.Num() - 1;
				if (!bLastPath)
				{
					return true;
				}
			}

			if (Cast<UBlueprintGeneratedClass>(Variant.GetProperty()->GetOwnerStruct()))
			{
				if (Variant.GetProperty()->HasMetaData(FName("FieldNotify")) || Variant.GetProperty()->HasAnyPropertyFlags(CPF_Net))
				{
					return false;
				}
			}
		}
	}
	return true;
}

void FMVVMViewBlueprintCompiler::TestGenerateSetter(FStringView ObjectName, FStringView FieldPath, FStringView FunctionName)
{
#if UE_BUILD_DEVELOPMENT || UE_BUILD_DEBUG
	UWidgetBlueprint* WidgetBlueprint = nullptr;
	{
		UObject* FoundObject = FindObject<UObject>(nullptr, ObjectName.GetData(), false);
		WidgetBlueprint = Cast<UWidgetBlueprint>(FoundObject);
	}

	if (WidgetBlueprint == nullptr)
	{
		return;
	}

	UWidgetBlueprintGeneratedClass* NewSkeletonClass = Cast<UWidgetBlueprintGeneratedClass>(WidgetBlueprint->SkeletonGeneratedClass);
	TValueOrError<TArray<FMVVMConstFieldVariant>, FText> SkeletalSetterPathResult = FieldPathHelper::GenerateFieldPathList(NewSkeletonClass, FieldPath, false);
	if (SkeletalSetterPathResult.HasError())
	{
		return;
	}
	if (!IsPropertyPathValid(SkeletalSetterPathResult.GetValue()))
	{
		return;
	}

	UEdGraph* GeneratedSetterGraph = UE::MVVM::FunctionGraphHelper::CreateFunctionGraph(WidgetBlueprint, FunctionName, EFunctionFlags::FUNC_None, TEXT(""), false);
	if (GeneratedSetterGraph == nullptr)
	{
		return;
	}

	const FProperty* SetterProperty = nullptr;
	if (SkeletalSetterPathResult.GetValue().Num() > 0 && ensure(SkeletalSetterPathResult.GetValue().Last().IsProperty()))
	{
		SetterProperty = SkeletalSetterPathResult.GetValue().Last().GetProperty();
	}
	if (SetterProperty == nullptr)
	{
		return;
	}

	UE::MVVM::FunctionGraphHelper::AddFunctionArgument(GeneratedSetterGraph, SetterProperty, "NewValue");
	UE::MVVM::FunctionGraphHelper::GenerateSetter(WidgetBlueprint, GeneratedSetterGraph, SkeletalSetterPathResult.GetValue());
#endif
}

} //namespace

#undef LOCTEXT_NAMESPACE
