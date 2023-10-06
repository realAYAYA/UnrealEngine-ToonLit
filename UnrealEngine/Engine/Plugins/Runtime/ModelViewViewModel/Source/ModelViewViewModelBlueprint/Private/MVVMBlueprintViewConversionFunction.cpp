// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVMBlueprintViewConversionFunction.h"
#include "MVVMDeveloperProjectSettings.h"

#include "Bindings/MVVMConversionFunctionHelper.h"
#include "Bindings/MVVMBindingHelper.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"

#include "EdGraphSchema_K2.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_CallFunction.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "KismetCompiler.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MVVMBlueprintViewConversionFunction)

namespace UE::MVVM::Private
{
	static FLazyName DefaultConversionFunctionName = TEXT("__ConversionFunction");
}

const UFunction* UMVVMBlueprintViewConversionFunction::GetCompiledFunction(const UClass* SelfContext) const
{
	if (NeedsWrapperGraph())
	{
		FMemberReference CompiledFunction;
		CompiledFunction.SetSelfMember(GraphName);
		return CompiledFunction.ResolveMember<UFunction>(const_cast<UClass*>(SelfContext));
	}
	return FunctionReference.ResolveMember<UFunction>(const_cast<UClass*>(SelfContext));
}

FName UMVVMBlueprintViewConversionFunction::GetCompiledFunctionName() const
{
	if (NeedsWrapperGraph())
	{
		return GraphName;
	}
	return FunctionReference.GetMemberName();
}

TVariant<const UFunction*, TSubclassOf<UK2Node>> UMVVMBlueprintViewConversionFunction::GetConversionFunction(const UClass* SelfContext) const
{
	if (FunctionNode.Get())
	{
		return TVariant<const UFunction*, TSubclassOf<UK2Node>>( TInPlaceType<TSubclassOf<UK2Node>>(), FunctionNode);
	}
	else
	{
		const UFunction* ConversionFunction = FunctionReference.ResolveMember<UFunction>(const_cast<UClass*>(SelfContext));
		return TVariant<const UFunction*, TSubclassOf<UK2Node>>( TInPlaceType<const UFunction*>(), ConversionFunction);
	}
	//return FunctionReference.ResolveMember<UFunction>(WidgetBlueprint->SkeletonGeneratedClass);
//UK2Node_CallFunction* CallFunctionNode = GetConversionFunctionNode(WidgetBlueprint, Binding, bSourceToDestination);
//if (CallFunctionNode)
//{
//	return CallFunctionNode->GetTargetFunction();
//}
}

void UMVVMBlueprintViewConversionFunction::Reset()
{
	FunctionReference = FMemberReference();
	FunctionNode = TSubclassOf<UK2Node>();
	GraphName = FName();
	bWrapperGraphTransient = false;
	SavedPins.Reset();
	CachedWrapperGraph = nullptr;
	CachedWrapperNode = nullptr;
}

void UMVVMBlueprintViewConversionFunction::InitFromFunction(UBlueprint* InContext, const UFunction* InFunction)
{
	InitFromFunction(InContext, InFunction, UE::MVVM::Private::DefaultConversionFunctionName.Resolve());
}

void UMVVMBlueprintViewConversionFunction::InitFromFunction(UBlueprint* InContext, const UFunction* InFunction, FName InGraphName)
{
	Reset();

	if (InFunction)
	{
		UClass* OwnerClass = InFunction->GetOuterUClass();
		FGuid MemberGuid;
		UBlueprint::GetGuidFromClassByFieldName<UFunction>(OwnerClass, InFunction->GetFName(), MemberGuid);

		bool bIsSelf = (InContext->GeneratedClass && InContext->GeneratedClass->IsChildOf(OwnerClass))
			|| (InContext->SkeletonGeneratedClass && InContext->SkeletonGeneratedClass->IsChildOf(OwnerClass));
		if (bIsSelf)
		{
			FunctionReference.SetSelfMember(InFunction->GetFName(), MemberGuid);
		}
		else
		{
			if (UBlueprint* VariableOwnerBP = Cast<UBlueprint>(OwnerClass->ClassGeneratedBy))
			{
				OwnerClass = VariableOwnerBP->SkeletonGeneratedClass;
			}

			FunctionReference.SetExternalMember(InFunction->GetFName(), OwnerClass, MemberGuid);
		}
		// used to be SetFromField<UFunction>(Function, SelfContext->SkeletonGeneratedClass);

		if (UE::MVVM::ConversionFunctionHelper::RequiresWrapper(InFunction))
		{
			GraphName = InGraphName;
			bWrapperGraphTransient = !GetDefault<UMVVMDeveloperProjectSettings>()->bAllowConversionFunctionGeneratedGraphInEditor;
			GetOrCreateWrapperGraphInternal(InContext, InFunction);
			SavePinValues(InContext);
		}
	}
}

void UMVVMBlueprintViewConversionFunction::InitializeFromWrapperGraph(UBlueprint* SelfContext, UEdGraph* Graph)
{
	Reset();

	if (UK2Node* WrapperNode = UE::MVVM::ConversionFunctionHelper::GetWrapperNode(Graph))
	{
		if (UK2Node_CallFunction* CallFunction = Cast<UK2Node_CallFunction>(WrapperNode))
		{
			FunctionReference = CallFunction->FunctionReference;
		}
		else
		{
			FunctionNode = WrapperNode->GetClass();
		}
		CachedWrapperGraph = WrapperNode->GetGraph();
		check(CachedWrapperGraph);

		GraphName = CachedWrapperGraph->GetFName();
		bWrapperGraphTransient = !GetDefault<UMVVMDeveloperProjectSettings>()->bAllowConversionFunctionGeneratedGraphInEditor;

		CachedWrapperNode = WrapperNode;
		SavePinValues(SelfContext);

		if (bWrapperGraphTransient && CachedWrapperNode)
		{
			SelfContext->FunctionGraphs.RemoveSingle(CachedWrapperGraph);
		}
	}
}

void UMVVMBlueprintViewConversionFunction::InitializeFromMemberReference(UBlueprint* SelfContext, FMemberReference MemberReference)
{
	Reset();

	FunctionReference = MemberReference;
}

UEdGraph* UMVVMBlueprintViewConversionFunction::GetOrCreateIntermediateWrapperGraph(FKismetCompilerContext& Context) const
{
	if (!NeedsWrapperGraph())
	{
		return nullptr;
	}

	if (CachedWrapperGraph)
	{
		return CachedWrapperGraph;
	}

	if (IsWrapperGraphTransient())
	{
		if (FunctionNode.Get())
		{
			ensure(false); // not supported for now
			return nullptr;
		}
		else
		{
			const UFunction* ConversionFunction = FunctionReference.ResolveMember<UFunction>(Context.NewClass);
			return GetOrCreateWrapperGraphInternal(Context, ConversionFunction);
		}
	}
	else
	{
		TObjectPtr<UEdGraph>* Found = Context.Blueprint->FunctionGraphs.FindByPredicate([GraphName = GetWrapperGraphName()](const UEdGraph* Other) { return Other->GetFName() == GraphName; });
		if (Found)
		{
			CachedWrapperGraph = *Found;
			CachedWrapperNode = UE::MVVM::ConversionFunctionHelper::GetWrapperNode(CachedWrapperGraph);
		}
		return CachedWrapperGraph;
	}
}

UEdGraph* UMVVMBlueprintViewConversionFunction::GetOrCreateWrapperGraph(UBlueprint* Blueprint) const
{
	if (!NeedsWrapperGraph())
	{
		return nullptr;
	}

	if (CachedWrapperGraph)
	{
		return CachedWrapperGraph;
	}

	if (IsWrapperGraphTransient())
	{
		if (FunctionNode.Get())
		{
			ensure(false); // not supported for now
			return nullptr;
		}
		else
		{
			const UFunction* ConversionFunction = FunctionReference.ResolveMember<UFunction>(Blueprint->SkeletonGeneratedClass);
			return GetOrCreateWrapperGraphInternal(Blueprint, ConversionFunction);
		}
	}
	else
	{
		TObjectPtr<UEdGraph>* Found = Blueprint->FunctionGraphs.FindByPredicate([GraphName = GetWrapperGraphName()](const UEdGraph* Other) { return Other->GetFName() == GraphName; });
		if (Found)
		{
			CachedWrapperGraph = *Found;
			CachedWrapperNode = UE::MVVM::ConversionFunctionHelper::GetWrapperNode(CachedWrapperGraph);
		}
		return CachedWrapperGraph;
	}
}

UEdGraphPin* UMVVMBlueprintViewConversionFunction::GetOrCreateGraphPin(UBlueprint* Blueprint, FName PinName) const
{
	GetOrCreateWrapperGraph(Blueprint);
	if (CachedWrapperNode)
	{
		return CachedWrapperNode->FindPin(PinName);
	}
	return nullptr;
}

UEdGraph* UMVVMBlueprintViewConversionFunction::GetOrCreateWrapperGraphInternal(FKismetCompilerContext& Context, const UFunction* Function) const
{
	return GetOrCreateWrapperGraphInternal(Context.Blueprint, Function);
}

UEdGraph* UMVVMBlueprintViewConversionFunction::GetOrCreateWrapperGraphInternal(UBlueprint* Blueprint, const UFunction* Function) const
{
	TPair<UEdGraph*, UK2Node*> Result = UE::MVVM::ConversionFunctionHelper::CreateGraph(Blueprint, GraphName, Function, bWrapperGraphTransient);
	CachedWrapperGraph = Result.Get<0>();
	CachedWrapperNode = Result.Get<1>();
	LoadPinValuesInternal(Blueprint);
	return CachedWrapperGraph;
}

void UMVVMBlueprintViewConversionFunction::RemoveWrapperGraph(UBlueprint* Blueprint)
{
	TObjectPtr<UEdGraph>* Result = Blueprint->FunctionGraphs.FindByPredicate([WrapperName = GraphName](const UEdGraph* GraphPtr) { return GraphPtr->GetFName() == WrapperName; });
	if (Result)
	{
		FBlueprintEditorUtils::RemoveGraph(Blueprint, Result->Get());
	}
	CachedWrapperGraph = nullptr;
	CachedWrapperNode = nullptr;
}

void UMVVMBlueprintViewConversionFunction::SetGraphPin(UBlueprint* Blueprint, FName PinName, const FMVVMBlueprintPropertyPath& Path)
{
	if (!NeedsWrapperGraph())
	{
		return;
	}

	UEdGraphPin* GraphPin = GetOrCreateGraphPin(Blueprint, PinName);

	// Set the value and make the blueprint as dirty before creating the pin.
	//A property may not be created yet and the skeletal needs to be recreated.
	FMVVMBlueprintPin* Pin = SavedPins.FindByPredicate([PinName](const FMVVMBlueprintPin& Other) { return PinName == Other.GetName(); });
	if (!Pin)
	{
		Pin = &SavedPins.Add_GetRef(FMVVMBlueprintPin::CreateFromPin(Blueprint, GraphPin));
	}
	Pin->SetPath(Path);

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	UE::MVVM::ConversionFunctionHelper::SetPropertyPathForPin(Blueprint, Path, GraphPin);
}

void UMVVMBlueprintViewConversionFunction::SavePinValues(UBlueprint* Blueprint)
{
	if (CachedWrapperNode)
	{
		SavedPins.Reset();
		for (UEdGraphPin* Pin : CachedWrapperNode->Pins)
		{
			if (Pin->PinName != UEdGraphSchema_K2::PN_Self && Pin->PinName != UEdGraphSchema_K2::PN_Execute && Pin->Direction == EGPD_Input)
			{
				SavedPins.Add(FMVVMBlueprintPin::CreateFromPin(Blueprint, Pin));
			}
		}
	}
}

void UMVVMBlueprintViewConversionFunction::LoadPinValuesInternal(UBlueprint* Blueprint) const
{
	if (CachedWrapperNode)
	{
		for (const FMVVMBlueprintPin& Pin : SavedPins)
		{
			if (UEdGraphPin* GraphPin = CachedWrapperNode->FindPin(Pin.GetName()))
			{
				Pin.CopyTo(Blueprint, GraphPin);
			}
		}
	}
}