// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVMEditorSubsystem.h"

#include "Algo/Reverse.h"
#include "Bindings/MVVMBindingHelper.h"
#include "Bindings/MVVMConversionFunctionHelper.h"
#include "Blueprint/WidgetTree.h"
#include "BlueprintActionDatabase.h"
#include "BlueprintNodeSpawner.h"
#include "Containers/Deque.h"
#include "Engine/Engine.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "K2Node_BreakStruct.h"
#include "K2Node_CallFunction.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_VariableGet.h"
#include "MVVMBlueprintView.h"
#include "MVVMSubsystem.h"
#include "MVVMWidgetBlueprintExtension_View.h"
#include "ScopedTransaction.h"
#include "Types/MVVMBindingSource.h"
#include "UObject/MetaData.h"
#include "WidgetBlueprint.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MVVMEditorSubsystem)

#define LOCTEXT_NAMESPACE "MVVMEditorSubsystem"

namespace UE::MVVM::Private
{
	void OnBindingPreEditChange(UMVVMBlueprintView* BlueprintView, FName PropertyName)
	{
		FProperty* ChangedProperty = FMVVMBlueprintViewBinding::StaticStruct()->FindPropertyByName(PropertyName);
		check(ChangedProperty != nullptr);

		FEditPropertyChain EditChain;
		EditChain.AddTail(UMVVMBlueprintView::StaticClass()->FindPropertyByName("Bindings"));
		EditChain.AddTail(ChangedProperty);
		EditChain.SetActivePropertyNode(ChangedProperty);

		BlueprintView->PreEditChange(EditChain);
	}

	void OnBindingPostEditChange(UMVVMBlueprintView* BlueprintView, FName PropertyName)
	{
		FProperty* ChangedProperty = FMVVMBlueprintViewBinding::StaticStruct()->FindPropertyByName(PropertyName);
		check(ChangedProperty != nullptr);

		FEditPropertyChain EditChain;
		EditChain.AddTail(UMVVMBlueprintView::StaticClass()->FindPropertyByName("Bindings"));
		EditChain.AddTail(ChangedProperty);
		EditChain.SetActivePropertyNode(ChangedProperty);

		FPropertyChangedEvent ChangeEvent(ChangedProperty, EPropertyChangeType::ValueSet);
		FPropertyChangedChainEvent ChainEvent(EditChain, ChangeEvent);
		BlueprintView->PostEditChangeChainProperty(ChainEvent);
	}

	UEdGraph* CreateFunctionGraph(UBlueprint* Blueprint, FName InFunctionName, EFunctionFlags ExtraFunctionFlag, const FStringView Category)
	{
		bool bIsEditable = false;

		FName UniqueFunctionName = FBlueprintEditorUtils::FindUniqueKismetName(Blueprint, InFunctionName.ToString());
		UEdGraph* FunctionGraph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, UniqueFunctionName, UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
		Blueprint->FunctionGraphs.Add(FunctionGraph);
		FunctionGraph->bEditable = bIsEditable;

		const UEdGraphSchema_K2* Schema = CastChecked<UEdGraphSchema_K2>(FunctionGraph->GetSchema());
		Schema->MarkFunctionEntryAsEditable(FunctionGraph, bIsEditable);
		Schema->CreateDefaultNodesForGraph(*FunctionGraph);

		// function entry node
		FGraphNodeCreator<UK2Node_FunctionEntry> FunctionEntryCreator(*FunctionGraph);
		UK2Node_FunctionEntry* FunctionEntry = FunctionEntryCreator.CreateNode();
		FunctionEntry->FunctionReference.SetSelfMember(FunctionGraph->GetFName());
		FunctionEntry->AddExtraFlags(ExtraFunctionFlag);
		FunctionEntry->bIsEditable = bIsEditable;
		FunctionEntry->MetaData.Category = FText::FromStringView(Category);
		FunctionEntry->NodePosX = -500;
		FunctionEntry->NodePosY = 0;
		FunctionEntryCreator.Finalize();

		FGraphNodeCreator<UK2Node_FunctionResult> FunctionResultCreator(*FunctionGraph);
		UK2Node_FunctionResult* FunctionResult = FunctionResultCreator.CreateNode();
		FunctionResult->FunctionReference.SetSelfMember(FunctionGraph->GetFName());
		FunctionResult->bIsEditable = bIsEditable;
		FunctionResult->NodePosX = 500;
		FunctionResult->NodePosY = 0;
		FunctionResultCreator.Finalize();

		return FunctionGraph;
	}

	UK2Node_FunctionEntry* FindFunctionEntry(UEdGraph* Graph)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UK2Node_FunctionEntry* FunctionEntry = Cast<UK2Node_FunctionEntry>(Node))
			{
				return FunctionEntry;
			}
		}
		return nullptr;
	}

	UK2Node_FunctionResult* FindFunctionResult(UEdGraph* Graph)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UK2Node_FunctionResult* FunctionResult = Cast<UK2Node_FunctionResult>(Node))
			{
				return FunctionResult;
			}
		}
		return nullptr;
	}

	UEdGraphNode* FindConversionNode(UEdGraph* Graph)
	{
		UK2Node_FunctionResult* FunctionResult = UE::MVVM::Private::FindFunctionResult(Graph);
		if (!ensureMsgf(FunctionResult != nullptr, TEXT("Function result node not found in conversion function wrapper!")))
		{
			return nullptr;
		}

		if (!ensureMsgf(FunctionResult->UserDefinedPins.Num() == 1, TEXT("Function result should have exactly one return value.")))
		{
			return nullptr;
		}

		UEdGraphPin* ResultPin = FunctionResult->FindPin(FunctionResult->UserDefinedPins[0]->PinName, EGPD_Input);
		if (!ensureMsgf(ResultPin != nullptr, TEXT("Function result pin not found.")))
		{
			return nullptr;
		}

		if (!ensureMsgf(ResultPin->LinkedTo.Num() != 0, TEXT("Result pin not linked to anything")))
		{
			return nullptr;
		}

		// finally found our conversion node
		UEdGraphNode* ConversionNode = ResultPin->LinkedTo[0]->GetOwningNode();
		return ConversionNode;
	}

	UEdGraph* FindExistingConversionFunctionWrapper(UWidgetBlueprint* WidgetBlueprint, FName WrapperName)
	{
		TObjectPtr<UEdGraph>* Result = WidgetBlueprint->FunctionGraphs.FindByPredicate([WrapperName](const UEdGraph* GraphPtr) { return GraphPtr->GetFName() == WrapperName; });
		return Result ? Result->Get() : nullptr;
	}

	void RemoveConversionFunctionWrapper(UWidgetBlueprint* WidgetBlueprint, FName WrapperName)
	{
		if (UEdGraph* Graph = FindExistingConversionFunctionWrapper(WidgetBlueprint, WrapperName))
		{
			FBlueprintEditorUtils::RemoveGraph(WidgetBlueprint, Graph);
		}
	}

} //namespace Private


UMVVMBlueprintView* UMVVMEditorSubsystem::RequestView(UWidgetBlueprint* WidgetBlueprint) const
{
	UMVVMWidgetBlueprintExtension_View* Extension = UMVVMWidgetBlueprintExtension_View::RequestExtension<UMVVMWidgetBlueprintExtension_View>(WidgetBlueprint);
	if (Extension->GetBlueprintView() == nullptr)
	{
		Extension->CreateBlueprintViewInstance();
	}
	return Extension->GetBlueprintView();
}

UMVVMBlueprintView* UMVVMEditorSubsystem::GetView(const UWidgetBlueprint* WidgetBlueprint) const
{
	if (UMVVMWidgetBlueprintExtension_View* ExtensionView = UMVVMWidgetBlueprintExtension_View::GetExtension<UMVVMWidgetBlueprintExtension_View>(WidgetBlueprint))
	{
		return ExtensionView->GetBlueprintView();
	}
	return nullptr;
}

FName UMVVMEditorSubsystem::AddViewModel(UWidgetBlueprint* WidgetBlueprint, const UClass* ViewModelClass)
{
	FName Result;
	if (ViewModelClass)
	{
		if (UMVVMBlueprintView* View = GetView(WidgetBlueprint))
		{
			FString ClassName = ViewModelClass->ClassGeneratedBy != nullptr ? ViewModelClass->ClassGeneratedBy->GetName() : ViewModelClass->GetAuthoredName();
			FString ViewModelName = ClassName;
			FKismetNameValidator NameValidator(WidgetBlueprint);

			int32 Index = 1;
			while (NameValidator.IsValid(ViewModelName) != EValidatorResult::Ok)
			{
				ViewModelName = ClassName + "_";
				ViewModelName.AppendInt(Index);

				++Index;
			}

			Result = *ViewModelName;
			FMVVMBlueprintViewModelContext Context = FMVVMBlueprintViewModelContext(ViewModelClass, Result);
			if (Context.IsValid())
			{
				const FScopedTransaction Transaction(LOCTEXT("AddViewModel", "Add viewmodel"));
				View->Modify();
				View->AddViewModel(Context);
			}
			else
			{
				Result = FName();
			}
		}
	}
	return Result;
}

void UMVVMEditorSubsystem::RemoveViewModel(UWidgetBlueprint* WidgetBlueprint, FName ViewModel)
{
	if (UMVVMBlueprintView* View = GetView(WidgetBlueprint))
	{
		if (const FMVVMBlueprintViewModelContext* ViewModelContext = View->FindViewModel(ViewModel))
		{
			View->RemoveViewModel(ViewModelContext->GetViewModelId());
		}
	}
}

bool UMVVMEditorSubsystem::VerifyViewModelRename(UWidgetBlueprint* WidgetBlueprint, FName ViewModel, FName NewViewModel, FText& OutError)
{
	FKismetNameValidator Validator(WidgetBlueprint);
	EValidatorResult ValidatorResult = Validator.IsValid(NewViewModel);
	if (ValidatorResult != EValidatorResult::Ok)
	{
		if (ViewModel == NewViewModel && (ValidatorResult == EValidatorResult::AlreadyInUse || ValidatorResult == EValidatorResult::ExistingName))
		{
			// Continue successfully
		}
		else
		{
			OutError = INameValidatorInterface::GetErrorText(NewViewModel.ToString(), ValidatorResult);
			return false;
		}
	}
	return true;
}

bool UMVVMEditorSubsystem::RenameViewModel(UWidgetBlueprint* WidgetBlueprint, FName ViewModel, FName NewViewModel, FText& OutError)
{
	if (!VerifyViewModelRename(WidgetBlueprint, ViewModel, NewViewModel, OutError))
	{
		return false;
	}

	UMVVMBlueprintView* View = GetView(WidgetBlueprint);
	if (View == nullptr)
	{
		return false;
	}

	const FScopedTransaction Transaction(LOCTEXT("RenameViewModel", "Rename viewmodel"));
	View->Modify();
	return View->RenameViewModel(ViewModel, NewViewModel);
}

FMVVMBlueprintViewBinding& UMVVMEditorSubsystem::AddBinding(UWidgetBlueprint* WidgetBlueprint)
{
	UMVVMBlueprintView* View = RequestView(WidgetBlueprint);
	return View->AddDefaultBinding();
}

void UMVVMEditorSubsystem::RemoveBinding(UWidgetBlueprint* WidgetBlueprint, const FMVVMBlueprintViewBinding& Binding)
{
	if (UMVVMBlueprintView* View = GetView(WidgetBlueprint))
	{
		View->RemoveBinding(&Binding);
	}
}

UEdGraph* UMVVMEditorSubsystem::GetConversionFunctionGraph(const UWidgetBlueprint* WidgetBlueprint, const FMVVMBlueprintViewBinding& Binding, bool bSourceToDestination) const
{
	return UE::MVVM::ConversionFunctionHelper::GetGraph(WidgetBlueprint, Binding, bSourceToDestination);
}

UK2Node_CallFunction* UMVVMEditorSubsystem::GetConversionFunctionNode(const UWidgetBlueprint* WidgetBlueprint, const FMVVMBlueprintViewBinding& Binding, bool bSourceToDestination) const
{
	if (UEdGraph* Graph = UE::MVVM::ConversionFunctionHelper::GetGraph(WidgetBlueprint, Binding, bSourceToDestination))
	{
		return UE::MVVM::ConversionFunctionHelper::GetFunctionNode(Graph);
	}
	return nullptr;
}

UFunction* UMVVMEditorSubsystem::GetConversionFunction(const UWidgetBlueprint* WidgetBlueprint, const FMVVMBlueprintViewBinding& Binding, bool bSourceToDestination) const
{
	UK2Node_CallFunction* CallFunctionNode = GetConversionFunctionNode(WidgetBlueprint, Binding, bSourceToDestination);
	if (CallFunctionNode)
	{
		return CallFunctionNode->GetTargetFunction();
	}
	
	// simple conversion functions don't have a wrapper graph
	FMemberReference ConversionFunctionReference = bSourceToDestination ? Binding.Conversion.SourceToDestinationFunction : Binding.Conversion.DestinationToSourceFunction;
	return ConversionFunctionReference.ResolveMember<UFunction>(WidgetBlueprint->SkeletonGeneratedClass);
}

UEdGraphPin* UMVVMEditorSubsystem::GetConversionFunctionArgumentPin(const UWidgetBlueprint* WidgetBlueprint, const FMVVMBlueprintViewBinding& Binding, FName ParameterName, bool bSourceToDestination)
{
	UK2Node_CallFunction* CallFunctionNode = GetConversionFunctionNode(WidgetBlueprint, Binding, bSourceToDestination);
	if (CallFunctionNode == nullptr)
	{
		return nullptr;
	}

	UEdGraphPin* Pin = CallFunctionNode->FindPin(ParameterName, EGPD_Input);
	return Pin;
}


UEdGraph* UMVVMEditorSubsystem::CreateConversionFunctionWrapperGraph(UWidgetBlueprint* WidgetBlueprint, const FMVVMBlueprintViewBinding& Binding, const UFunction* ConversionFunction, bool bSourceToDestination)
{
	if (UMVVMBlueprintView* View = GetView(WidgetBlueprint))
	{
		TValueOrError<TArray<const FProperty*>, FText> ArgumentsResult = UE::MVVM::BindingHelper::TryGetArgumentsForConversionFunction(ConversionFunction);
		if (ArgumentsResult.HasValue())
		{
			TArray<const FProperty*> Arguments = ArgumentsResult.StealValue();
			if (Arguments.Num() > 1)
			{
				const FName WrapperName = UE::MVVM::ConversionFunctionHelper::CreateWrapperName(Binding, bSourceToDestination);

				UEdGraph* NewGraph = UE::MVVM::Private::CreateFunctionGraph(
					WidgetBlueprint,
					WrapperName,
					FUNC_BlueprintCallable | FUNC_BlueprintPure | FUNC_Const,
					TEXT("Conversion Functions")
				);

				UK2Node_FunctionEntry* FunctionEntry = UE::MVVM::Private::FindFunctionEntry(NewGraph);
				UK2Node_FunctionResult* FunctionResult = UE::MVVM::Private::FindFunctionResult(NewGraph);

				FGraphNodeCreator<UK2Node_CallFunction> CallFunctionCreator(*NewGraph);
				UK2Node_CallFunction* CallFunctionNode = CallFunctionCreator.CreateNode();
				CallFunctionNode->SetFromFunction(ConversionFunction);
				CallFunctionNode->NodePosX = 0;
				CallFunctionCreator.Finalize();

				UE::MVVM::ConversionFunctionHelper::MarkAsConversionFunction(CallFunctionNode, Binding);

				UEdGraphPin* FunctionEntryThenPin = FunctionEntry->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output);
				UEdGraphPin* FunctionResultExecPin = FunctionResult->GetExecPin();
			
				// hook up exec pins 
				if (!CallFunctionNode->IsNodePure())
				{
					UEdGraphPin* CallFunctionExecPin = CallFunctionNode->GetExecPin();
					UEdGraphPin* CallFunctionThenPin = CallFunctionNode->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output);

					FunctionEntryThenPin->MakeLinkTo(CallFunctionExecPin);
					CallFunctionThenPin->MakeLinkTo(FunctionResultExecPin);

					CallFunctionNode->NodePosY = 0;
				}
				else
				{
					FunctionEntryThenPin->MakeLinkTo(FunctionResultExecPin);

					CallFunctionNode->NodePosY = 100;
				}

				const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

				// create return value pin
				const FProperty* ReturnProperty = UE::MVVM::BindingHelper::GetReturnProperty(ConversionFunction);
				check(ReturnProperty);
				TSharedPtr<FUserPinInfo> PinInfo = MakeShared<FUserPinInfo>();
				K2Schema->ConvertPropertyToPinType(ReturnProperty, PinInfo->PinType);
				PinInfo->PinName = ReturnProperty->GetFName();
				PinInfo->DesiredPinDirection = EGPD_Input;
				FunctionResult->UserDefinedPins.Add(PinInfo);
				FunctionResult->ReconstructNode();

				UEdGraphPin* FunctionReturnPin = CallFunctionNode->FindPin(ReturnProperty->GetName(), EGPD_Output);
				UEdGraphPin* OutputPin = FunctionResult->FindPin(ReturnProperty->GetName());
				FunctionReturnPin->MakeLinkTo(OutputPin);

				return NewGraph;
			}
		}
	}
	return nullptr;
}

void UMVVMEditorSubsystem::SetSourceToDestinationConversionFunction(UWidgetBlueprint* WidgetBlueprint, FMVVMBlueprintViewBinding& Binding, const UFunction* ConversionFunction)
{
	if (UMVVMBlueprintView* View = GetView(WidgetBlueprint))
	{
		if (ConversionFunction == nullptr || IsValidConversionFunction(ConversionFunction, Binding.ViewModelPath, Binding.WidgetPath))
		{
			FScopedTransaction Transaction(LOCTEXT("SetConversionFunction", "Set Conversion Function"));

			WidgetBlueprint->Modify();

			UE::MVVM::Private::OnBindingPreEditChange(View, GET_MEMBER_NAME_CHECKED(FMVVMBlueprintViewBinding, Conversion));

			if (Binding.Conversion.SourceToDestinationWrapper != FName())
			{
				UE::MVVM::Private::RemoveConversionFunctionWrapper(WidgetBlueprint, Binding.Conversion.SourceToDestinationWrapper);
				Binding.Conversion.SourceToDestinationWrapper = FName();
			}

			if (ConversionFunction != nullptr)
			{
				UEdGraph* WrapperGraph = CreateConversionFunctionWrapperGraph(WidgetBlueprint, Binding, ConversionFunction, true);
				Binding.Conversion.SourceToDestinationFunction.SetFromField<UFunction>(ConversionFunction, WidgetBlueprint->SkeletonGeneratedClass);
				Binding.Conversion.SourceToDestinationWrapper = WrapperGraph != nullptr ? WrapperGraph->GetFName() : FName();
			}
			else
			{
				Binding.Conversion.SourceToDestinationFunction = FMemberReference();
			}

			UE::MVVM::Private::OnBindingPostEditChange(View, GET_MEMBER_NAME_CHECKED(FMVVMBlueprintViewBinding, Conversion));
		}
	}
}

void UMVVMEditorSubsystem::SetDestinationToSourceConversionFunction(UWidgetBlueprint* WidgetBlueprint, FMVVMBlueprintViewBinding& Binding, const UFunction* ConversionFunction)
{
	if (UMVVMBlueprintView* View = GetView(WidgetBlueprint))
	{
		if (ConversionFunction == nullptr || IsValidConversionFunction(ConversionFunction, Binding.WidgetPath, Binding.ViewModelPath))
		{
			FScopedTransaction Transaction(LOCTEXT("SetConversionFunction", "Set Conversion Function"));

			WidgetBlueprint->Modify();

			UE::MVVM::Private::OnBindingPreEditChange(View, GET_MEMBER_NAME_CHECKED(FMVVMBlueprintViewBinding, Conversion));

			if (Binding.Conversion.DestinationToSourceWrapper != FName())
			{
				UE::MVVM::Private::RemoveConversionFunctionWrapper(WidgetBlueprint, Binding.Conversion.DestinationToSourceWrapper);
				Binding.Conversion.DestinationToSourceWrapper = FName();
			}

			if (ConversionFunction != nullptr)
			{
				UEdGraph* WrapperGraph = CreateConversionFunctionWrapperGraph(WidgetBlueprint, Binding, ConversionFunction, true);
				Binding.Conversion.DestinationToSourceFunction.SetFromField<UFunction>(ConversionFunction, WidgetBlueprint->SkeletonGeneratedClass);
				Binding.Conversion.DestinationToSourceWrapper = WrapperGraph != nullptr ? WrapperGraph->GetFName() : FName();
			}
			else
			{
				Binding.Conversion.DestinationToSourceFunction = FMemberReference();
			}

			UE::MVVM::Private::OnBindingPostEditChange(View, GET_MEMBER_NAME_CHECKED(FMVVMBlueprintViewBinding, Conversion));
		}
	}
}

void UMVVMEditorSubsystem::SetWidgetPropertyForBinding(UWidgetBlueprint* WidgetBlueprint, FMVVMBlueprintViewBinding& Binding, FMVVMBlueprintPropertyPath Field)
{
	if (UMVVMBlueprintView* View = GetView(WidgetBlueprint))
	{
		FScopedTransaction Transaction(LOCTEXT("SetBindingProperty", "Set Binding Property"));

		UE::MVVM::Private::OnBindingPreEditChange(View, GET_MEMBER_NAME_CHECKED(FMVVMBlueprintViewBinding, WidgetPath));

		Binding.WidgetPath = Field;

		UE::MVVM::Private::OnBindingPostEditChange(View, GET_MEMBER_NAME_CHECKED(FMVVMBlueprintViewBinding, WidgetPath));
	}
}

void UMVVMEditorSubsystem::SetViewModelPropertyForBinding(UWidgetBlueprint* WidgetBlueprint, FMVVMBlueprintViewBinding& Binding, FMVVMBlueprintPropertyPath Field)
{
	if (UMVVMBlueprintView* View = GetView(WidgetBlueprint))
	{
		FScopedTransaction Transaction(LOCTEXT("SetBindingProperty", "Set Binding Property"));

		UE::MVVM::Private::OnBindingPreEditChange(View, GET_MEMBER_NAME_CHECKED(FMVVMBlueprintViewBinding, ViewModelPath));

		Binding.ViewModelPath = Field;

		UE::MVVM::Private::OnBindingPostEditChange(View, GET_MEMBER_NAME_CHECKED(FMVVMBlueprintViewBinding, ViewModelPath));
	}
}

void UMVVMEditorSubsystem::SetUpdateModeForBinding(UWidgetBlueprint* WidgetBlueprint, FMVVMBlueprintViewBinding& Binding, EMVVMViewBindingUpdateMode Mode)
{
	if (Binding.UpdateMode != Mode)
	{
		if (UMVVMBlueprintView* View = GetView(WidgetBlueprint))
		{
			FScopedTransaction Transaction(LOCTEXT("SetUpdateMode", "Set Update Mode"));

			UE::MVVM::Private::OnBindingPreEditChange(View, GET_MEMBER_NAME_CHECKED(FMVVMBlueprintViewBinding, UpdateMode));

			Binding.UpdateMode = Mode;

			UE::MVVM::Private::OnBindingPreEditChange(View, GET_MEMBER_NAME_CHECKED(FMVVMBlueprintViewBinding, UpdateMode));
		}
	}
}

void UMVVMEditorSubsystem::SetBindingTypeForBinding(UWidgetBlueprint* WidgetBlueprint, FMVVMBlueprintViewBinding& Binding, EMVVMBindingMode Type)
{
	if (Binding.BindingType != Type)
	{
		if (UMVVMBlueprintView* View = GetView(WidgetBlueprint))
		{
			FScopedTransaction Transaction(LOCTEXT("SetBindingType", "Set Binding Type"));

			UE::MVVM::Private::OnBindingPreEditChange(View, GET_MEMBER_NAME_CHECKED(FMVVMBlueprintViewBinding, BindingType));

			Binding.BindingType = Type;

			UE::MVVM::Private::OnBindingPostEditChange(View, GET_MEMBER_NAME_CHECKED(FMVVMBlueprintViewBinding, BindingType));
		}
	}
}

void UMVVMEditorSubsystem::SetEnabledForBinding(UWidgetBlueprint* WidgetBlueprint, FMVVMBlueprintViewBinding& Binding, bool bEnabled)
{
	if (Binding.bEnabled != bEnabled)
	{
		if (UMVVMBlueprintView* View = GetView(WidgetBlueprint))
		{
			UE::MVVM::Private::OnBindingPreEditChange(View, GET_MEMBER_NAME_CHECKED(FMVVMBlueprintViewBinding, bEnabled));

			Binding.bEnabled = bEnabled;

			UE::MVVM::Private::OnBindingPostEditChange(View, GET_MEMBER_NAME_CHECKED(FMVVMBlueprintViewBinding, bEnabled));
		}
	}
}

void UMVVMEditorSubsystem::SetCompileForBinding(UWidgetBlueprint* WidgetBlueprint, FMVVMBlueprintViewBinding& Binding, bool bCompile)
{
	if (Binding.bCompile != bCompile)
	{
		if (UMVVMBlueprintView* View = GetView(WidgetBlueprint))
		{
			UE::MVVM::Private::OnBindingPreEditChange(View, GET_MEMBER_NAME_CHECKED(FMVVMBlueprintViewBinding, bCompile));

			Binding.bCompile = bCompile;

			UE::MVVM::Private::OnBindingPostEditChange(View, GET_MEMBER_NAME_CHECKED(FMVVMBlueprintViewBinding, bCompile));
		}
	}
}

bool UMVVMEditorSubsystem::IsValidConversionFunction(const UFunction* Function, const FMVVMBlueprintPropertyPath& Source, const FMVVMBlueprintPropertyPath& Destination) const
{
	TValueOrError<const FProperty*, FText> ReturnResult = UE::MVVM::BindingHelper::TryGetReturnTypeForConversionFunction(Function);
	if (ReturnResult.HasError())
	{
		return false;
	}

	const FProperty* ReturnProperty = ReturnResult.GetValue();
	if (ReturnProperty == nullptr)
	{
		return false;
	}

	TValueOrError<TArray<const FProperty*>, FText> ArgumentsResult = UE::MVVM::BindingHelper::TryGetArgumentsForConversionFunction(Function);
	if (ArgumentsResult.HasError())
	{
		return false;
	}

	const FProperty* SourceProperty = nullptr;

	TArray<UE::MVVM::FMVVMConstFieldVariant> SourceFields = Source.GetFields();
	if (SourceFields.Num() > 0)
	{
		SourceProperty = SourceFields.Last().IsProperty() ? SourceFields.Last().GetProperty() : UE::MVVM::BindingHelper::GetReturnProperty(SourceFields.Last().GetFunction());
	}

	const FProperty* DestinationProperty = nullptr;

	TArray<UE::MVVM::FMVVMConstFieldVariant> DestFields = Destination.GetFields();
	if (DestFields.Num() > 0)
	{
		DestinationProperty = DestFields.Last().IsProperty() ? DestFields.Last().GetProperty() : UE::MVVM::BindingHelper::GetFirstArgumentProperty(DestFields.Last().GetFunction());
	}

	// check that at least one source -> argument binding is compatible
	bool bAnyCompatible = false;

	const TArray<const FProperty*>& ConversionArgProperties = ArgumentsResult.GetValue();
	for (const FProperty* ArgumentProperty : ConversionArgProperties)
	{
		if (ArgumentProperty->IsA<FObjectProperty>())
		{
			// filter out any functions with UObject properties - they aren't valid conversion functions
			return false;
		}

		if (SourceProperty == nullptr ||
			UE::MVVM::BindingHelper::ArePropertiesCompatible(SourceProperty, ArgumentProperty))
		{
			bAnyCompatible = true;
		}
	}

	if (!bAnyCompatible)
	{
		return false;
	}

	// check that the return -> dest is valid
	if (DestinationProperty != nullptr &&
		!UE::MVVM::BindingHelper::ArePropertiesCompatible(ReturnProperty, DestinationProperty))
	{
		return false;
	}

	return true;
}

bool UMVVMEditorSubsystem::IsSimpleConversionFunction(const UFunction* Function) const
{
	TValueOrError<const FProperty*, FText> ReturnResult = UE::MVVM::BindingHelper::TryGetReturnTypeForConversionFunction(Function);
	if (ReturnResult.HasError())
	{
		return false;
	}

	const FProperty* ReturnProperty = ReturnResult.GetValue();
	if (ReturnProperty == nullptr)
	{
		return false;
	}

	TValueOrError<TArray<const FProperty*>, FText> ArgumentsResult = UE::MVVM::BindingHelper::TryGetArgumentsForConversionFunction(Function);
	if (ArgumentsResult.HasError())
	{
		return false;
	}

	return ArgumentsResult.GetValue().Num() == 1;
}

TArray<UFunction*> UMVVMEditorSubsystem::GetAvailableConversionFunctions(const UWidgetBlueprint* WidgetBlueprint, const FMVVMBlueprintPropertyPath& Source, const FMVVMBlueprintPropertyPath& Destination) const
{
	TArray<UFunction*> ConversionFunctions;

	const UMVVMSubsystem* Subsystem = GEngine->GetEngineSubsystem<UMVVMSubsystem>();

	FBlueprintActionDatabase& ActionDatabase = FBlueprintActionDatabase::Get();
	const FBlueprintActionDatabase::FActionRegistry& ActionRegistry = ActionDatabase.GetAllActions();

	for (auto It = ActionRegistry.CreateConstIterator(); It; ++It)
	{
		if (UObject* ActionObject = It->Key.ResolveObjectPtr())
		{
			for (const UBlueprintNodeSpawner* NodeSpawner : It->Value)
			{
				FBlueprintActionInfo BlueprintAction(ActionObject, NodeSpawner);
				const UFunction* Function = BlueprintAction.GetAssociatedFunction();
				if (Function != nullptr)
				{
					// functions in the widget blueprint can do anything they want, other functions have to be static functions in a BlueprintFunctionLibrary
					const UClass* FunctionClass = Function->GetOuterUClass();
					if ((FunctionClass->ClassGeneratedBy == WidgetBlueprint) ||
						(WidgetBlueprint->ParentClass && WidgetBlueprint->ParentClass->IsChildOf(FunctionClass) && Function->HasAllFunctionFlags(FUNC_Static | FUNC_BlueprintPure)) ||
						(FunctionClass->IsChildOf<UBlueprintFunctionLibrary>() && Function->HasAllFunctionFlags(FUNC_Static | FUNC_BlueprintPure)))
					{
						if (IsValidConversionFunction(Function, Source, Destination))
						{
							ConversionFunctions.Add(const_cast<UFunction*>(Function));
						}
					}
				}
			}
		}
	}

	ConversionFunctions.Sort([](const UFunction& A, const UFunction& B) -> bool
		{
			return A.GetFName().LexicalLess(B.GetFName());
		});
	return ConversionFunctions;
}

FMVVMBlueprintPropertyPath UMVVMEditorSubsystem::GetPathForConversionFunctionArgument(const UWidgetBlueprint* WidgetBlueprint, const FMVVMBlueprintViewBinding& Binding, FName ArgumentName, bool bSourceToDestination) const
{
	UEdGraph* WrapperGraph = GetConversionFunctionGraph(WidgetBlueprint, Binding, bSourceToDestination);
	if (WrapperGraph == nullptr)
	{
		if (bSourceToDestination)
		{
			return Binding.ViewModelPath;
		}
		else
		{
			return Binding.WidgetPath;
		}
	}

	const UK2Node_CallFunction* FunctionNode = GetConversionFunctionNode(WidgetBlueprint, Binding, bSourceToDestination);
	if (FunctionNode == nullptr)
	{
		return FMVVMBlueprintPropertyPath();
	}

	return UE::MVVM::ConversionFunctionHelper::GetPropertyPathForArgument(WidgetBlueprint, FunctionNode, ArgumentName, false);
}

void UMVVMEditorSubsystem::SetPathForConversionFunctionArgument(UWidgetBlueprint* WidgetBlueprint, FMVVMBlueprintViewBinding& Binding, FName ArgumentName, const FMVVMBlueprintPropertyPath& Path, bool bSourceToDestination) const
{
	UEdGraph* ConversionFunctionGraph = GetConversionFunctionGraph(WidgetBlueprint, Binding, bSourceToDestination);
	if (ConversionFunctionGraph == nullptr)
	{
		// simple conversion function
		if (bSourceToDestination)
		{
			Binding.ViewModelPath = Path;
		}
		else
		{
			Binding.WidgetPath = Path;
		}

		return;
	}

	UEdGraphNode* ConversionNode = UE::MVVM::Private::FindConversionNode(ConversionFunctionGraph);
	if (!ensure(ConversionNode != nullptr))
	{
		return;
	}

	const UEdGraphSchema_K2* Schema = CastChecked<UEdGraphSchema_K2>(ConversionFunctionGraph->GetSchema());

	// find the argument pin with the name ArgumentName
	UEdGraphPin* ArgumentPin = nullptr;
	int32 ArgumentIndex = 0;
	for (UEdGraphPin* Pin : ConversionNode->Pins)
	{
		if (Pin->Direction == EGPD_Input && Pin->PinName != UEdGraphSchema_K2::PN_Execute)
		{
			if (Pin->PinName == ArgumentName)
			{
				ArgumentPin = Pin;
				break;
			}

			++ArgumentIndex;
		}
	}

	if (!ensure(ArgumentPin != nullptr))
	{
		return;
	}

	// delete all linked nodes
	TArray<UEdGraphPin*> PinsToClear;
	PinsToClear.Add(ArgumentPin);

	TArray<UEdGraphNode*> NodesToRemove;

	while (PinsToClear.Num() > 0)
	{
		UEdGraphPin* Pin = PinsToClear[0];
		PinsToClear.RemoveAt(0);

		PinsToClear.Append(Pin->LinkedTo);
		Pin->BreakAllPinLinks();

		// rename the node out of the way
		UEdGraphNode* Node = Pin->GetOwningNode();
		if (Node != ConversionNode)
		{
			NodesToRemove.AddUnique(Node);
		}
	}

	for (UEdGraphNode* Node : NodesToRemove)
	{
		ConversionFunctionGraph->RemoveNode(Node, true);
	}

	if (Path.IsEmpty())
	{
		// we were given an empty path and we already cleaned up the path 
		return;
	}

	TArray<UE::MVVM::FMVVMConstFieldVariant> Fields = Path.GetFields();

	float PosX = ConversionNode->NodePosX - 300 * (Fields.Num() + 1);
	const float PosY = ConversionNode->NodePosY + ArgumentIndex * 100;

	UEdGraphPin* PreviousExecPin = UE::MVVM::Private::FindFunctionEntry(ConversionFunctionGraph)->FindPin(UEdGraphSchema_K2::PN_Then);
	UEdGraphPin* PreviousDataPin = nullptr;
	UClass* PreviousClass = nullptr;

	// create the root property getter node, ie. the Widget/ViewModel
	const FProperty* RootProperty = nullptr;
	if (Path.IsFromWidget())
	{
		RootProperty = WidgetBlueprint->SkeletonGeneratedClass->FindPropertyByName(Path.GetWidgetName());
	}
	else if (Path.IsFromViewModel())
	{
		UMVVMBlueprintView* View = GetView(WidgetBlueprint);
		const FMVVMBlueprintViewModelContext* Context = View->FindViewModel(Path.GetViewModelId());

		RootProperty = WidgetBlueprint->SkeletonGeneratedClass->FindPropertyByName(Context->GetViewModelName());
	}

	if (RootProperty != nullptr)
	{
		FGraphNodeCreator<UK2Node_VariableGet> RootGetterCreator(*ConversionFunctionGraph);
		UK2Node_VariableGet* RootGetterNode = RootGetterCreator.CreateNode();
		RootGetterNode->NodePosX = PosX;
		RootGetterNode->NodePosY = PosY;
		RootGetterNode->VariableReference.SetFromField<FProperty>(RootProperty, true, WidgetBlueprint->SkeletonGeneratedClass);
		RootGetterCreator.Finalize();

		PreviousDataPin = RootGetterNode->Pins[0];

		PreviousClass = CastField<FObjectProperty>(RootProperty)->PropertyClass;

		PosX += 300;
	}
	else
	{
		ensureMsgf(false, TEXT("Could not resolve root property!"));
		return;
	}

	// create all the subsequent nodes in the path
	for (int32 Index = 0; Index < Fields.Num(); ++Index)
	{
		if (!ensureMsgf(PreviousClass != nullptr, TEXT("Previous class not set!")))
		{
			return;
		}

		UEdGraphNode* NewNode = nullptr;
		const UE::MVVM::FMVVMConstFieldVariant& Field = Fields[Index];
		const bool bLastField = (Index == Fields.Num() - 1);

		if (Field.IsProperty())
		{
			const FProperty* Property = Field.GetProperty();

			// for structs in the middle of a path, we need to use a break node
			const FStructProperty* StructProperty = CastField<FStructProperty>(Property);
			if (!bLastField && StructProperty != nullptr)
			{
				FGraphNodeCreator<UK2Node_BreakStruct> BreakCreator(*ConversionFunctionGraph);
				UK2Node_BreakStruct* BreakNode = BreakCreator.CreateNode();
				BreakNode->AllocateDefaultPins();
				BreakCreator.Finalize();

				NewNode = BreakNode;

				PreviousClass = nullptr;
			}
			else if (PreviousClass != nullptr)
			{
				FGraphNodeCreator<UK2Node_VariableGet> GetterCreator(*ConversionFunctionGraph);
				UK2Node_VariableGet* GetterNode = GetterCreator.CreateNode();
				GetterNode->SetFromProperty(Property, false, PreviousClass);
				GetterNode->AllocateDefaultPins();
				GetterCreator.Finalize();

				NewNode = GetterNode;

				if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property))
				{
					PreviousClass = ObjectProperty->PropertyClass;
				}
				else
				{
					PreviousClass = nullptr;
				}
			}
		}
		else if (Field.IsFunction())
		{
			const UFunction* Function = Field.GetFunction();

			FGraphNodeCreator<UK2Node_CallFunction> CallFunctionCreator(*ConversionFunctionGraph);
			UK2Node_CallFunction* FunctionNode = CallFunctionCreator.CreateNode();
			FunctionNode->SetFromFunction(Function);
			FunctionNode->AllocateDefaultPins();
			CallFunctionCreator.Finalize();

			NewNode = FunctionNode;

			const FProperty* ReturnProperty = UE::MVVM::BindingHelper::GetReturnProperty(Function);
			if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(ReturnProperty))
			{
				PreviousClass = ObjectProperty->PropertyClass;
			}
			else
			{
				PreviousClass = nullptr;
			}
		}
		else
		{
			ensureMsgf(false, TEXT("Invalid path, empty field in path."));
			return;
		}
		
		// first create new connections
		for (UEdGraphPin* Pin : NewNode->Pins)
		{
			if (Pin->Direction == EGPD_Input)
			{
				if (Pin->PinName == UEdGraphSchema_K2::PN_Execute)
				{
					PreviousExecPin->BreakAllPinLinks();
					Pin->MakeLinkTo(PreviousExecPin);
				}
				else if (Schema->ArePinsCompatible(PreviousDataPin, Pin, PreviousClass))
				{
					Pin->MakeLinkTo(PreviousDataPin);
				}
			}
		}

		// then update our previous pin pointers
		for (UEdGraphPin* Pin : NewNode->Pins)
		{
			if (Pin->Direction == EGPD_Output)
			{
				if (Pin->PinName == UEdGraphSchema_K2::PN_Then)
				{
					PreviousExecPin = Pin;
				}
				else
				{
					PreviousDataPin = Pin;
				}
			}
		}

		NewNode->NodePosX = PosX;
		NewNode->NodePosY = PosY;

		PosX += 300; 
	}

	// finish by linking to our ultimate destinations
	UEdGraphPin* LastExecPin = ConversionNode->FindPin(UEdGraphSchema_K2::PN_Execute);
	if (LastExecPin == nullptr)
	{
		LastExecPin = UE::MVVM::Private::FindFunctionResult(ConversionFunctionGraph)->FindPin(UEdGraphSchema_K2::PN_Execute);
	}
	PreviousExecPin->MakeLinkTo(LastExecPin);
	PreviousDataPin->MakeLinkTo(ArgumentPin);
}

TArray<UE::MVVM::FBindingSource> UMVVMEditorSubsystem::GetBindableWidgets(const UWidgetBlueprint* WidgetBlueprint) const
{
	TArray<UE::MVVM::FBindingSource> Sources;

	const UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree;
	if (WidgetTree == nullptr)
	{
		return Sources;
	}

	TArray<UWidget*> AllWidgets;
	WidgetTree->GetAllWidgets(AllWidgets);

	Sources.Reserve(AllWidgets.Num() + 1);

	// Add current widget as a possible binding source
	if (UClass* BPClass = WidgetBlueprint->GeneratedClass)
	{
		TArray<FMVVMAvailableBinding> Bindings = GEngine->GetEngineSubsystem<UMVVMSubsystem>()->GetAvailableBindings(BPClass, WidgetBlueprint->GeneratedClass);
		if (Bindings.Num() > 0)
		{
			// at least one valid property, add it to our list
			UE::MVVM::FBindingSource Source;
			Source.Name = WidgetBlueprint->GetFName();
			Source.DisplayName = FText::FromName(WidgetBlueprint->GetFName());
			Source.Class = BPClass;
			Sources.Add(Source);
		}
	}

	for (const UWidget* Widget : AllWidgets)
	{
		TArray<FMVVMAvailableBinding> Bindings = GEngine->GetEngineSubsystem<UMVVMSubsystem>()->GetAvailableBindings(Widget->GetClass(), WidgetBlueprint->GeneratedClass);
		if (Bindings.Num() > 0)
		{
			// at least one valid property, add it to our list
			UE::MVVM::FBindingSource Source;
			Source.Name = Widget->GetFName();
			Source.DisplayName = Widget->GetLabelText();
			Source.Class = Widget->GetClass();
			Sources.Add(Source);
		}
	}

	return Sources;
}

TArray<UE::MVVM::FBindingSource> UMVVMEditorSubsystem::GetAllViewModels(const UWidgetBlueprint* WidgetBlueprint) const
{
	TArray<UE::MVVM::FBindingSource> Sources;

	if (UMVVMBlueprintView* View = GetView(WidgetBlueprint))
	{
		const TArrayView<const FMVVMBlueprintViewModelContext> ViewModels = View->GetViewModels();
		Sources.Reserve(ViewModels.Num());

		for (const FMVVMBlueprintViewModelContext& ViewModel : ViewModels)
		{
			UE::MVVM::FBindingSource Source;
			Source.ViewModelId = ViewModel.GetViewModelId();
			Source.DisplayName = ViewModel.GetDisplayName();
			Source.Class = ViewModel.GetViewModelClass();
			Sources.Add(Source);
		}
	}

	return Sources;
}

TArray<FMVVMAvailableBinding> UMVVMEditorSubsystem::GetChildViewModels(TSubclassOf<UObject> Class, TSubclassOf<UObject> Accessor)
{
	if (Class.Get() == nullptr)
	{
		return TArray<FMVVMAvailableBinding>();
	}

	TArray<FMVVMAvailableBinding> ViewModelAvailableBindingsList = GEngine->GetEngineSubsystem<UMVVMSubsystem>()->GetAvailableBindings(Class, Accessor);
	ViewModelAvailableBindingsList.RemoveAllSwap([Class](const FMVVMAvailableBinding& Value)
		{
			UE::MVVM::FMVVMFieldVariant Variant = UE::MVVM::BindingHelper::FindFieldByName(Class.Get(), Value.GetBindingName());
			const FProperty* Property = nullptr;
			if (Variant.IsProperty())
			{
				Property = Variant.GetProperty();
			}
			else if (Variant.IsFunction() && Variant.GetFunction())
			{
				Property = UE::MVVM::BindingHelper::GetReturnProperty(Variant.GetFunction());
			}

			if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
			{
				return !ObjectProperty->PropertyClass->ImplementsInterface(UNotifyFieldValueChanged::StaticClass());
			}
			return true;
		});

	return ViewModelAvailableBindingsList;
}

#undef LOCTEXT_NAMESPACE

