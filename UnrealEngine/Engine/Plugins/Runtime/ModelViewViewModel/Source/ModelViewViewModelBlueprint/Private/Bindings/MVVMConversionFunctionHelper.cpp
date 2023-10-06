// Copyright Epic Games, Inc. All Rights Reserved.

#include "Bindings/MVVMConversionFunctionHelper.h"

#include "Bindings/MVVMBindingHelper.h"
#include "Components/Widget.h"
#include "Containers/Deque.h"
#include "MVVMBlueprintView.h"
#include "MVVMBlueprintViewConversionFunction.h"
#include "MVVMWidgetBlueprintExtension_View.h"
#include "UObject/MetaData.h"

#include "K2Node_BreakStruct.h"
#include "K2Node_CallFunction.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_Self.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"

namespace UE::MVVM::ConversionFunctionHelper
{

namespace Private
{
	static const FLazyName ConversionFunctionMetadataKey = "ConversionFunction";
	static const FLazyName ConversionFunctionCategory = "Conversion Functions";

	UMVVMBlueprintView* GetView(const UBlueprint* Blueprint)
	{
		const TObjectPtr<UBlueprintExtension>* ExtensionViewPtr = Blueprint->GetExtensions().FindByPredicate([](const UBlueprintExtension* Other) { return Other && Other->GetClass() == UMVVMWidgetBlueprintExtension_View::StaticClass(); });
		if (ExtensionViewPtr)
		{
			return CastChecked<UMVVMWidgetBlueprintExtension_View>(*ExtensionViewPtr)->GetBlueprintView();
		}
		return nullptr;
	}

	void MarkAsConversionFunction(const UK2Node* FunctionNode, const UEdGraph* Graph)
	{
		check(FunctionNode != nullptr);
		FunctionNode->GetPackage()->GetMetaData()->SetValue(FunctionNode, ConversionFunctionMetadataKey.Resolve(), TEXT(""));
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

	TTuple<UEdGraph*, UK2Node_FunctionEntry*, UK2Node_FunctionResult*> CreateGraph(UBlueprint* Blueprint, FName GraphName, bool bIsEditable, bool bAddToBlueprint)
	{
		FName UniqueFunctionName = FBlueprintEditorUtils::FindUniqueKismetName(Blueprint, GraphName.ToString());
		UEdGraph* FunctionGraph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, UniqueFunctionName, UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
		FunctionGraph->bEditable = bIsEditable;
		if (bAddToBlueprint)
		{
			Blueprint->FunctionGraphs.Add(FunctionGraph);
		}
		else
		{
			FunctionGraph->SetFlags(RF_Transient);
		}

		const UEdGraphSchema_K2* Schema = CastChecked<UEdGraphSchema_K2>(FunctionGraph->GetSchema());
		Schema->MarkFunctionEntryAsEditable(FunctionGraph, bIsEditable);
		Schema->CreateDefaultNodesForGraph(*FunctionGraph);

		// function entry node
		FGraphNodeCreator<UK2Node_FunctionEntry> FunctionEntryCreator(*FunctionGraph);
		UK2Node_FunctionEntry* FunctionEntry = FunctionEntryCreator.CreateNode();
		FunctionEntry->FunctionReference.SetSelfMember(FunctionGraph->GetFName());
		FunctionEntry->AddExtraFlags(FUNC_BlueprintCallable | FUNC_BlueprintPure | FUNC_Const | FUNC_Protected | FUNC_Final);
		FunctionEntry->bIsEditable = bIsEditable;
		FunctionEntry->MetaData.Category = FText::FromName(ConversionFunctionCategory.Resolve());
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

		return { FunctionGraph, FunctionEntry, FunctionResult };
	}
}

bool RequiresWrapper(const UFunction* ConversionFunction)
{
	if (ConversionFunction == nullptr)
	{
		return false;
	}

	TValueOrError<TArray<const FProperty*>, FText> ArgumentsResult = UE::MVVM::BindingHelper::TryGetArgumentsForConversionFunction(ConversionFunction);
	if (ArgumentsResult.HasValue())
	{
		return (ArgumentsResult.GetValue().Num() > 1);
	}
	return false;
}

FName CreateWrapperName(const FMVVMBlueprintViewBinding& Binding, bool bSourceToDestination)
{
	TStringBuilder<256> StringBuilder;
	StringBuilder << TEXT("__");
	StringBuilder << Binding.GetFName();
	StringBuilder << (bSourceToDestination ? TEXT("_SourceToDest") : TEXT("_DestToSource"));

	return FName(StringBuilder.ToString());
}

TPair<UEdGraph*, UK2Node*> CreateGraph(UBlueprint* Blueprint, FName GraphName, const UFunction* ConversionFunction, bool bTransient)
{
	bool bIsEditable = false;
	bool bAddToBlueprint = !bTransient;

	UEdGraph* FunctionGraph = nullptr;
	UK2Node_FunctionEntry* FunctionEntry = nullptr;
	UK2Node_FunctionResult* FunctionResult = nullptr;
	{
		TTuple<UEdGraph*, UK2Node_FunctionEntry*, UK2Node_FunctionResult*> NewGraph = Private::CreateGraph(Blueprint, GraphName, bIsEditable, bAddToBlueprint);
		FunctionGraph = NewGraph.Get<0>();
		FunctionEntry = NewGraph.Get<1>();
		FunctionResult = NewGraph.Get<2>();
	}

	const FProperty* ReturnProperty = UE::MVVM::BindingHelper::GetReturnProperty(ConversionFunction);
	check(ReturnProperty);

	// create return value pin
	{
		TSharedPtr<FUserPinInfo> PinInfo = MakeShared<FUserPinInfo>();
		GetDefault<UEdGraphSchema_K2>()->ConvertPropertyToPinType(ReturnProperty, PinInfo->PinType);
		PinInfo->PinName = ReturnProperty->GetFName();
		PinInfo->DesiredPinDirection = EGPD_Input;
		FunctionResult->UserDefinedPins.Add(PinInfo);
		FunctionResult->ReconstructNode();
	}
	
	UK2Node_CallFunction* CallFunctionNode = nullptr;
	{
		FGraphNodeCreator<UK2Node_CallFunction> CallFunctionCreator(*FunctionGraph);
		CallFunctionNode = CallFunctionCreator.CreateNode();
		CallFunctionNode->SetFromFunction(ConversionFunction);
		CallFunctionNode->NodePosX = 0;
		CallFunctionCreator.Finalize();
		Private::MarkAsConversionFunction(CallFunctionNode, FunctionGraph);
	}

	// Make link Entry -> CallFunction || Entry -> Return
	{
		UEdGraphPin* FunctionEntryThenPin = FunctionEntry->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output);
		UEdGraphPin* FunctionResultExecPin = FunctionResult->GetExecPin();
		
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
	}

	{
		UEdGraphPin* FunctionReturnPin = CallFunctionNode->FindPin(ReturnProperty->GetName(), EGPD_Output);
		UEdGraphPin* FunctionResultPin = FunctionResult->FindPin(ReturnProperty->GetFName(), EGPD_Input);
		check(FunctionResultPin && FunctionReturnPin);
		FunctionReturnPin->MakeLinkTo(FunctionResultPin);
	}

	return { FunctionGraph , CallFunctionNode };
}

UK2Node* GetWrapperNode(UEdGraph* Graph)
{
	if (Graph == nullptr)
	{
		return nullptr;
	}

	FName ConversionFunctionMetadataKey = Private::ConversionFunctionMetadataKey.Resolve();
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		// check if we've set any metadata on the nodes to figure out which one it is
		if (Cast<UK2Node>(Node) && Node->GetPackage()->GetMetaData()->HasValue(Node, ConversionFunctionMetadataKey))
		{
			return CastChecked<UK2Node>(Node);
		}
	}

	if (UK2Node_FunctionResult* FunctionResult = Private::FindFunctionResult(Graph))
	{
		for (UEdGraphPin* GraphPin : FunctionResult->Pins)
		{
			if (GraphPin->GetFName() != UEdGraphSchema_K2::PN_Execute && GraphPin->LinkedTo.Num() == 1)
			{
				if (UK2Node* Node = Cast<UK2Node>(GraphPin->LinkedTo[0]->GetOwningNode()))
				{
					Private::MarkAsConversionFunction(Node, Graph);
					return Node;
				}
			}
		}
	}

	return nullptr;
}

namespace Private
{
	TArray<TTuple<UEdGraphNode*, UEdGraphPin*>> GetPropertyPathGraphNode(const UEdGraphPin* StartPin)
	{
		TArray<TTuple<UEdGraphNode*, UEdGraphPin*>> NodesInPath;

		auto AddNode = [&NodesInPath](const UEdGraphPin* Pin)
		{
			UEdGraphNode* Result = nullptr;
			if (Pin->Direction == EGPD_Input && Pin->LinkedTo.Num() == 1 && Pin->PinName != UEdGraphSchema_K2::PN_Execute)
			{
				Result = Pin->LinkedTo[0]->GetOwningNode();
				NodesInPath.Emplace(Result, Pin->LinkedTo[0]);
			}
			return Result;
		};

		UEdGraphNode* CurrentNode = AddNode(StartPin);
		while (CurrentNode)
		{
			TArray<UEdGraphPin*>& Pins = CurrentNode->Pins;
			CurrentNode = nullptr;
			for (UEdGraphPin* Pin : Pins)
			{
				if (UEdGraphNode* NewNode = AddNode(Pin))
				{
					CurrentNode = NewNode;
					break;
				}
			}
		}

		Algo::Reverse(NodesInPath);
		return NodesInPath;
	}


	FMVVMBlueprintPropertyPath GetPropertyPathForPin(const UBlueprint* Blueprint, const UEdGraphPin* StartPin, bool bSkipResolve)
	{
		if (StartPin->Direction != EGPD_Input || StartPin->PinName == UEdGraphSchema_K2::PN_Self)
		{
			return FMVVMBlueprintPropertyPath();
		}
		
		UMVVMBlueprintView* BlueprintView = GetView(Blueprint);
		if (BlueprintView == nullptr)
		{
			return FMVVMBlueprintPropertyPath();
		}

		FMVVMBlueprintPropertyPath ResultPath;

		auto AddRoot = [&ResultPath, BlueprintView, Blueprint, bSkipResolve](FMemberReference& Member)
		{
			if (bSkipResolve)
			{
				// if the generated class hasn't yet been generated we can blindly forge ahead and try to figure out if it's a widget or a viewmodel
				if (const FMVVMBlueprintViewModelContext* ViewModel = BlueprintView->FindViewModel(Member.GetMemberName()))
				{
					ResultPath.SetViewModelId(ViewModel->GetViewModelId());
				}
				else
				{
					ResultPath.SetWidgetName(Member.GetMemberName());
				}
			}
			else
			{
				if (const FObjectProperty* Property = CastField<FObjectProperty>(Member.ResolveMember<FProperty>(Blueprint->SkeletonGeneratedClass)))
				{
					if (Property->PropertyClass->IsChildOf<UWidget>() || Property->PropertyClass->IsChildOf<UBlueprint>())
					{
						ResultPath.SetWidgetName(Property->GetFName());
					}
					else if (Property->PropertyClass->ImplementsInterface(UNotifyFieldValueChanged::StaticClass()))
					{
						if (const FMVVMBlueprintViewModelContext* ViewModel = BlueprintView->FindViewModel(Property->GetFName()))
						{
							ResultPath.SetViewModelId(ViewModel->GetViewModelId());
						}
					}
				}
			}
		};

		auto AddPropertyPath = [&ResultPath, Blueprint](FMemberReference& MemberReference)
		{
			if (UFunction* Function = MemberReference.ResolveMember<UFunction>(Blueprint->SkeletonGeneratedClass))
			{
				ResultPath.AppendPropertyPath(Blueprint, UE::MVVM::FMVVMConstFieldVariant(Function));
			}
			else if (const FProperty* Property = MemberReference.ResolveMember<FProperty>(Blueprint->SkeletonGeneratedClass))
			{
				ResultPath.AppendPropertyPath(Blueprint, UE::MVVM::FMVVMConstFieldVariant(Property));
			}
		};

		auto AddBreakNode = [&ResultPath, Blueprint](UScriptStruct* Struct, FName PropertyName)
		{
			FProperty* FoundProperty = Struct->FindPropertyByName(PropertyName);
			if (ensure(FoundProperty))
			{
				ResultPath.AppendPropertyPath(Blueprint, UE::MVVM::FMVVMConstFieldVariant(FoundProperty));
			}
		};

		bool bFirst = true;
		TArray<TTuple<UEdGraphNode*, UEdGraphPin*>> NodesToSearch = GetPropertyPathGraphNode(StartPin);
		for (const TTuple<UEdGraphNode*, UEdGraphPin*>& NodePair : NodesToSearch)
		{
			UEdGraphNode* Node = NodePair.Get<UEdGraphNode*>();
			if (UK2Node_VariableGet* GetNode = Cast<UK2Node_VariableGet>(Node))
			{
				if (bFirst)
				{
					AddRoot(GetNode->VariableReference);
				}
				else
				{
					AddPropertyPath(GetNode->VariableReference);
				}
			}
			else if (UK2Node_CallFunction* FunctionNode = Cast<UK2Node_CallFunction>(Node))
			{
				// UK2Node_CallFunction can be native break function
				if (UFunction* Function = FunctionNode->FunctionReference.ResolveMember<UFunction>(Blueprint->SkeletonGeneratedClass))
				{
					bool bAddPropertyPath = true;
					const FProperty* ArgumentProperty = UE::MVVM::BindingHelper::GetFirstArgumentProperty(Function);
					const FStructProperty* ArgumentStructProperty = CastField<FStructProperty>(ArgumentProperty);
					if (ArgumentStructProperty && ArgumentStructProperty->Struct)
					{
						const FString& MetaData = ArgumentStructProperty->Struct->GetMetaData(FBlueprintMetadata::MD_NativeBreakFunction);
						if (MetaData.Len() > 0)
						{
							if (const UFunction* NativeBreakFunction = FindObject<UFunction>(nullptr, *MetaData, true))
							{
								AddBreakNode(ArgumentStructProperty->Struct, NodePair.Get<UEdGraphPin*>()->GetFName());
								bAddPropertyPath = false;
							}
						}
					}

					if (bAddPropertyPath)
					{
						ResultPath.AppendPropertyPath(Blueprint, UE::MVVM::FMVVMConstFieldVariant(Function));
					}
				}
			}
			else if (UK2Node_BreakStruct* StructNode = Cast<UK2Node_BreakStruct>(Node))
			{
				if (ensure(StructNode->StructType))
				{
					AddBreakNode(StructNode->StructType, NodePair.Get<UEdGraphPin*>()->GetFName());
				}
			}
			else if (UK2Node_Self* Self = Cast<UK2Node_Self>(Node))
			{
				if (bFirst)
				{
					ResultPath.SetWidgetName(Blueprint->GetFName());
				}
				else
				{
					ensure(false);
				}
			}
			bFirst = false;
		}

		return ResultPath;
	}
}

FMVVMBlueprintPropertyPath GetPropertyPathForPin(const UBlueprint* Blueprint, const UEdGraphPin* Pin, bool bSkipResolve)
{
	if (Pin == nullptr || Pin->LinkedTo.Num() == 0)
	{
		return FMVVMBlueprintPropertyPath();
	}

	return Private::GetPropertyPathForPin(Blueprint, Pin, bSkipResolve);
}

void SetPropertyPathForPin(const UBlueprint* Blueprint, const FMVVMBlueprintPropertyPath& Path, UEdGraphPin* PathPin)
{
	if (PathPin == nullptr)
	{
		return;
	}

	UEdGraphNode* ConversionNode = PathPin->GetOwningNode();
	UEdGraph* FunctionGraph = ConversionNode ? ConversionNode->GetGraph() : nullptr;
	const UEdGraphSchema* Schema = FunctionGraph ? FunctionGraph->GetSchema() : nullptr;

	UK2Node_FunctionEntry* ConverionFunctionEntry = ConversionNode ? Private::FindFunctionEntry(FunctionGraph) : nullptr;
	UK2Node_FunctionResult* ConverionFunctionResult = ConversionNode ? Private::FindFunctionResult(FunctionGraph) : nullptr;

	// Remove previous nodes
	{
		TArray<TTuple<UEdGraphNode*, UEdGraphPin*>> AllNodesForPath = Private::GetPropertyPathGraphNode(PathPin);
		for (const TTuple<UEdGraphNode*, UEdGraphPin*>& Pair : AllNodesForPath)
		{
			UEdGraphNode* Node = Pair.Get<UEdGraphNode*>();
			FunctionGraph->RemoveNode(Node, true);
		}
	}

	if (!FunctionGraph || !ConversionNode || !ConverionFunctionEntry || !ConverionFunctionResult)
	{
		return;
	}

	// Add new nodes
	if (!Path.IsEmpty())
	{
		const int32 ArgumentIndex = ConversionNode->Pins.IndexOfByPredicate([PathPin](const UEdGraphPin* Other){ return Other == PathPin; });
		if (!ensure(ConversionNode->Pins.IsValidIndex(ArgumentIndex)))
		{
			return;
		}

		TArray<UE::MVVM::FMVVMConstFieldVariant> Fields = Path.GetFields(Blueprint->SkeletonGeneratedClass);
		float PosX = ConversionNode->NodePosX - 300 * (Fields.Num() + 1);
		const float PosY = ConversionNode->NodePosY + ArgumentIndex * 100;

		UEdGraphPin* PreviousDataPin = nullptr;
		UClass* PreviousClass = nullptr;

		// create the root property getter node, ie. the Widget/ViewModel
		{
			const FProperty* RootProperty = nullptr;
			bool bCreateSelfNodeForRootProperty = false;
			{
				if (Path.IsFromWidget())
				{
					RootProperty = Blueprint->SkeletonGeneratedClass->FindPropertyByName(Path.GetWidgetName());
					bCreateSelfNodeForRootProperty = Blueprint->GetFName() == Path.GetWidgetName();
				}
				else if (Path.IsFromViewModel())
				{
					UMVVMBlueprintView* View = Private::GetView(Blueprint);
					const FMVVMBlueprintViewModelContext* Context = View->FindViewModel(Path.GetViewModelId());
					RootProperty = Blueprint->SkeletonGeneratedClass->FindPropertyByName(Context->GetViewModelName());
				}
			}

			if (bCreateSelfNodeForRootProperty)
			{
				FGraphNodeCreator<UK2Node_Self> RootGetterCreator(*FunctionGraph);
				UK2Node_Self* RootSelfNode = RootGetterCreator.CreateNode();
				RootSelfNode->NodePosX = PosX;
				RootSelfNode->NodePosY = PosY;
				RootGetterCreator.Finalize();

				PreviousDataPin = RootSelfNode->FindPinChecked(UEdGraphSchema_K2::PSC_Self);
				PreviousClass = Blueprint->SkeletonGeneratedClass;
			}
			else
			{
				if (RootProperty == nullptr)
				{
					ensureMsgf(false, TEXT("Could not resolve root property!"));
					return;
				}

				FGraphNodeCreator<UK2Node_VariableGet> RootGetterCreator(*FunctionGraph);
				UK2Node_VariableGet* RootGetterNode = RootGetterCreator.CreateNode();
				RootGetterNode->NodePosX = PosX;
				RootGetterNode->NodePosY = PosY;
				RootGetterNode->VariableReference.SetFromField<FProperty>(RootProperty, true, Blueprint->SkeletonGeneratedClass);
				RootGetterCreator.Finalize();

				PreviousDataPin = RootGetterNode->Pins[0];
				PreviousClass = CastField<FObjectProperty>(RootProperty)->PropertyClass;
			}

			PosX += 300;
		}

		// create all the subsequent nodes in the path
		const FProperty* PreviousProperty = nullptr;
		for (int32 Index = 0; Index < Fields.Num(); ++Index)
		{
			UEdGraphNode* NewNode = nullptr;
			const UE::MVVM::FMVVMConstFieldVariant& Field = Fields[Index];

			auto CanNewConnections = [Schema](UEdGraphPin* Pin, UEdGraphPin* PreviousDataPin, const UClass* Context)
			{
				return Pin->Direction == EGPD_Input
					&& Pin->PinName != UEdGraphSchema_K2::PN_Execute
					&& Schema->ArePinsCompatible(PreviousDataPin, Pin, Context);
			};

			auto FindNewOutputPin = [](UEdGraphNode* NewNode) -> UEdGraphPin*
			{
				// then update our previous pin pointers
				for (UEdGraphPin* Pin : NewNode->Pins)
				{
					if (Pin->Direction == EGPD_Output)
					{
						if (Pin->PinName != UEdGraphSchema_K2::PN_Then)
						{
							return Pin;
						}
					}
				}
				return nullptr;
			};

			UEdGraphPin* NewPreviousDataPin = nullptr;
			if (Field.IsProperty())
			{
				const FProperty* Property = Field.GetProperty();

				// for struct in the middle of a path, we need to use a break node
				if (const FStructProperty* PreviousStructProperty = CastField<FStructProperty>(PreviousProperty))
				{
					const FString& MetaData = PreviousStructProperty->Struct->GetMetaData(FBlueprintMetadata::MD_NativeBreakFunction);
					if (MetaData.Len() > 0)
					{
						const UFunction* Function = FindObject<UFunction>(nullptr, *MetaData, true);
						ensure(Function);
						FGraphNodeCreator<UK2Node_CallFunction> MakeStructCreator(*FunctionGraph);
						UK2Node_CallFunction* FunctionNode = MakeStructCreator.CreateNode(false);
						FunctionNode->SetFromFunction(Function);
						MakeStructCreator.Finalize();

						NewNode = FunctionNode;
						PreviousProperty = UE::MVVM::BindingHelper::GetReturnProperty(Function);
					}
					else
					{
						FGraphNodeCreator<UK2Node_BreakStruct> BreakCreator(*FunctionGraph);
						UK2Node_BreakStruct* BreakNode = BreakCreator.CreateNode();
						BreakNode->StructType = PreviousStructProperty->Struct;
						BreakNode->AllocateDefaultPins();
						BreakCreator.Finalize();

						NewNode = BreakNode;
						PreviousProperty = Property;
					}

					NewPreviousDataPin = nullptr;
					for (UEdGraphPin* Pin : NewNode->Pins)
					{
						if (Pin->Direction == EGPD_Output)
						{
							if (Pin->PinName == Property->GetFName())
							{
								NewPreviousDataPin = Pin;
								break;
							}
						}
					}

					if (!NewPreviousDataPin)
					{
						ensure(false);
						return;
					}
				}
				else if (PreviousClass != nullptr)
				{
					FGraphNodeCreator<UK2Node_VariableGet> GetterCreator(*FunctionGraph);
					UK2Node_VariableGet* GetterNode = GetterCreator.CreateNode();
					GetterNode->SetFromProperty(Property, false, PreviousClass);
					GetterNode->AllocateDefaultPins();
					GetterCreator.Finalize();

					NewNode = GetterNode;
					PreviousProperty = Property;
					NewPreviousDataPin = FindNewOutputPin(NewNode);
				}
			}
			else if (Field.IsFunction())
			{
				if (CastField<FStructProperty>(PreviousProperty))
				{
					ensure(false);
					return;
				}

				const UFunction* Function = Field.GetFunction();

				FGraphNodeCreator<UK2Node_CallFunction> CallFunctionCreator(*FunctionGraph);
				UK2Node_CallFunction* FunctionNode = CallFunctionCreator.CreateNode();
				FunctionNode->SetFromFunction(Function);
				FunctionNode->AllocateDefaultPins();
				CallFunctionCreator.Finalize();

				NewNode = FunctionNode;
				PreviousProperty = UE::MVVM::BindingHelper::GetReturnProperty(Function);
				NewPreviousDataPin = FindNewOutputPin(NewNode);
			}
			else
			{
				ensureMsgf(false, TEXT("Invalid path, empty field in path."));
				return;
			}

			// create new data connections
			for (UEdGraphPin* Pin : NewNode->Pins)
			{
				if (CanNewConnections(Pin, PreviousDataPin, PreviousClass))
				{
					Pin->MakeLinkTo(PreviousDataPin);
				}
			}

			if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(PreviousProperty))
			{
				PreviousClass = ObjectProperty->PropertyClass;
			}
			else
			{
				PreviousClass = nullptr;
			}

			// then update our previous pin pointers
			PreviousDataPin = NewPreviousDataPin;

			NewNode->NodePosX = PosX;
			NewNode->NodePosY = PosY;

			PosX += 300;
		}

		// Link the last data pin to the Conversation Function Pin
		PreviousDataPin->MakeLinkTo(PathPin);
	}

	// Link Then / Exec pin
	{
		check(ConversionNode && ConverionFunctionEntry && ConverionFunctionResult);

		UEdGraphPin* ThenPin = ConverionFunctionEntry->FindPinChecked(UEdGraphSchema_K2::PN_Then);
		check(ThenPin);
		ThenPin->BreakAllPinLinks();

		for (UEdGraphPin* Pin : ConversionNode->Pins)
		{
			TArray<TTuple<UEdGraphNode*, UEdGraphPin*>> AllNodesForPath = Private::GetPropertyPathGraphNode(Pin);
			for (const TTuple<UEdGraphNode*, UEdGraphPin*>& Pair : AllNodesForPath)
			{
				UEdGraphNode* PathNode = Pair.Get<UEdGraphNode*>();
				if (UK2Node_CallFunction* CallFunction = Cast<UK2Node_CallFunction>(PathNode))
				{
					// if it not a pure node
					if (UEdGraphPin* ExecPin = CallFunction->FindPin(UEdGraphSchema_K2::PN_Execute))
					{
						ThenPin->BreakAllPinLinks();
						ThenPin->MakeLinkTo(ExecPin);
						ThenPin = CallFunction->FindPinChecked(UEdGraphSchema_K2::PN_Then);
					}
				}
			}
		}
		UEdGraphPin* LastExecPin = ConverionFunctionResult->FindPinChecked(UEdGraphSchema_K2::PN_Execute);
		ThenPin->MakeLinkTo(LastExecPin);
	}
}

FMVVMBlueprintPropertyPath GetPropertyPathForArgument(const UBlueprint* WidgetBlueprint, const UK2Node_CallFunction* FunctionNode, FName ArgumentName, bool bSkipResolve)
{
	const UEdGraphPin* ArgumentPin = FunctionNode->FindPin(ArgumentName, EGPD_Input);
	return GetPropertyPathForPin(WidgetBlueprint, ArgumentPin, bSkipResolve);
}

TMap<FName, FMVVMBlueprintPropertyPath> GetAllArgumentPropertyPaths(const UBlueprint* Blueprint, const UK2Node_CallFunction* FunctionNode, bool bSkipResolve)
{
	check(FunctionNode);

	TMap<FName, FMVVMBlueprintPropertyPath> Paths;
	for (const UEdGraphPin* Pin : FunctionNode->GetAllPins())
	{
		FMVVMBlueprintPropertyPath Path = Private::GetPropertyPathForPin(Blueprint, Pin, bSkipResolve);
		if (!Path.IsEmpty())
		{
			Paths.Add(Pin->PinName, Path);
		}
	}

	return Paths;
}

TMap<FName, FMVVMBlueprintPropertyPath> GetAllArgumentPropertyPaths(const UBlueprint* Blueprint, const FMVVMBlueprintViewBinding& Binding, bool bSourceToDestination, bool bSkipResolve)
{
	if (UMVVMBlueprintViewConversionFunction* ConversionFunction = Binding.Conversion.GetConversionFunction(bSourceToDestination))
	{
		if (UEdGraph* ConversionFunctionGraph = ConversionFunction->GetWrapperGraph())
		{
			if (UK2Node_CallFunction* ConversionNode = Cast<UK2Node_CallFunction>(ConversionFunctionHelper::GetWrapperNode(ConversionFunctionGraph)))
			{
				return GetAllArgumentPropertyPaths(Blueprint, ConversionNode, bSkipResolve);
			}
		}
	}

	return TMap<FName, FMVVMBlueprintPropertyPath>();
}

} //namespace UE::MVVM
