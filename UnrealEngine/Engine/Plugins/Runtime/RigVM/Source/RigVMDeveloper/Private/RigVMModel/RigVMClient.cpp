// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMModel/RigVMClient.h"
#include "Misc/TransactionObjectEvent.h"
#include "Exporters/Exporter.h"
#include "Algo/Sort.h"
#include "UnrealExporter.h"
#include "UObject/ObjectSaveContext.h"
#include "RigVMModel/RigVMControllerActions.h"
#include "EdGraph/RigVMEdGraph.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMClient)

#if WITH_EDITOR
#include "ScopedTransaction.h"
#endif

UObject* IRigVMClientHost::ResolveUserDefinedTypeById(const FString& InTypeName) const
{
	return nullptr;
}

void FRigVMClient::SetSchemaClass(TSubclassOf<URigVMSchema> InSchemaClass)
{
	check(InSchemaClass);
	
	if(InSchemaClass == SchemaClass)
	{
		return;
	}
	
	if(SchemaPtr)
	{
		if(!SchemaPtr->HasAnyFlags(RF_ClassDefaultObject))
		{
			DestroyObject(SchemaPtr);
		}
		SchemaPtr = nullptr;
	}

	SchemaClass = InSchemaClass;

	URigVMSchema* Schema = GetOrCreateSchema();
	for(auto& Pair : Controllers)
	{
		Pair.Value->SetSchema(Schema);
	}
}

void FRigVMClient::SetControllerClass(TSubclassOf<URigVMController> InControllerClass)
{
	check(InControllerClass);

	if (InControllerClass == ControllerClass)
	{
		return;
	}

	for (URigVMGraph* Model : Models)
	{
		RemoveController(Model);
	}

	ControllerClass = InControllerClass;
}

void FRigVMClient::SetOuterClientHost(UObject* InOuterClientHost, const FName& InOuterClientHostPropertyName)
{
	OuterClientHost = InOuterClientHost;
	OuterClientPropertyName = InOuterClientHostPropertyName;

	check(OuterClientHost->Implements<URigVMClientHost>());
	check(GetOuterClientProperty() != nullptr);

	// create the null graph / default controller.
	// we need this to react to notifs without a valid graph
	// such as interaction brackets
	static const URigVMGraph* NullGraph = nullptr;
	if(!Controllers.Contains(NullGraph))
	{
		CreateController(NullGraph);
	}
}

void FRigVMClient::SetFromDeprecatedData(URigVMGraph* InDefaultGraph, URigVMFunctionLibrary* InFunctionLibrary)
{
	if(GetDefaultModel() != InDefaultGraph ||
		GetFunctionLibrary() != InFunctionLibrary)
	{
		if(GetDefaultModel() == InDefaultGraph)
		{
			Models.Reset();
		}
		
		if(InFunctionLibrary == nullptr)
		{
			Swap(FunctionLibrary, InFunctionLibrary);
		}

		Reset();
		if(InDefaultGraph)
		{
			AddModel(InDefaultGraph, false);
		}
		if(InFunctionLibrary)
		{
			AddModel(InFunctionLibrary, false);
		}
	}
}

void FRigVMClient::Reset()
{
	for(URigVMGraph* Model : Models)
	{
		DestroyObject(Model);
	}
	for(auto Pair : Controllers)
	{
		DestroyObject(Pair.Value);
	}
	DestroyObject(FunctionLibrary);

	Models.Reset();
	Controllers.Reset();
	FunctionLibrary = nullptr;

	ResetActionStack();

	if(SchemaPtr)
	{
		if(!SchemaPtr->HasAnyFlags(RF_ClassDefaultObject))
		{
			DestroyObject(SchemaPtr);
		}
		SchemaPtr = nullptr;
	}
}

URigVMSchema* FRigVMClient::GetOrCreateSchema()
{
	check(SchemaClass);
	if(SchemaPtr && !IsValid(SchemaPtr))
	{
		SchemaPtr = nullptr;
	}
	if(SchemaPtr && SchemaPtr->GetClass() != SchemaClass)
	{
		SchemaPtr = nullptr;
	}
	if(SchemaPtr == nullptr)
	{
		SchemaPtr = CastChecked<URigVMSchema>(SchemaClass->GetDefaultObject(true));
	}
	return SchemaPtr;
}

URigVMGraph* FRigVMClient::GetDefaultModel() const
{
	if(Models.IsEmpty())
	{
		return nullptr;
	}
	return GetModel(0);
}

URigVMGraph* FRigVMClient::GetModel(const UEdGraph* InEdGraph) const
{
	if (InEdGraph == nullptr)
	{
		return GetDefaultModel();
	}

//#if WITH_EDITORONLY_DATA
//	if (InEdGraph == FunctionLibraryEdGraph)
//	{
//		return RigVMClient.GetFunctionLibrary();
//	}
//#endif

	const URigVMEdGraph* RigGraph = Cast< URigVMEdGraph>(InEdGraph);
	check(RigGraph);
	return GetModel(RigGraph->ModelNodePath);

}

URigVMGraph* FRigVMClient::GetModel(const FString& InNodePathOrName) const
{
	if(InNodePathOrName.IsEmpty())
	{
		return GetDefaultModel();
	}

	TArray<URigVMGraph*> ModelsAndFunctionLibrary = GetAllModels(true, false);
	for(URigVMGraph* Model : ModelsAndFunctionLibrary)
	{
		if(Model->GetNodePath() == InNodePathOrName || Model->GetName() == InNodePathOrName)
		{
			return Model;
		}

		static constexpr TCHAR NodePathPrefixFormat[] = TEXT("%s|");
		const FString NodePathPrefix = FString::Printf(NodePathPrefixFormat, *Model->GetNodePath());
		if(InNodePathOrName.StartsWith(NodePathPrefix))
		{
			const FString RemainingNodePath = InNodePathOrName.Mid(NodePathPrefix.Len());
			if(const URigVMCollapseNode* CollapseNode = Cast<URigVMCollapseNode>(Model->FindNode(RemainingNodePath)))
			{
				return CollapseNode->GetContainedGraph();
			}
		}
	}
	return nullptr;
}

URigVMGraph* FRigVMClient::GetModel(const UObject* InEditorSideObject) const
{
	check(InEditorSideObject);

	if (InEditorSideObject->Implements<URigVMEditorSideObject>())
	{
		const IRigVMEditorSideObject* UserInterfaceGraph = Cast<IRigVMEditorSideObject>(InEditorSideObject);
		return GetModel(UserInterfaceGraph->GetRigVMNodePath());
	}

	return nullptr;
}

TArray<URigVMGraph*> FRigVMClient::GetAllModels(bool bIncludeFunctionLibrary, bool bRecursive) const
{
	TArray<URigVMGraph*> AllModels = Models;
	if(bRecursive)
	{
		for(URigVMGraph* Model : Models)
		{
			AllModels.Append(Model->GetContainedGraphs(true /* recursive */));
		}
	}
	if(bIncludeFunctionLibrary && FunctionLibrary)
	{
		AllModels.Add(FunctionLibrary);
		if(bRecursive)
		{
			AllModels.Append(FunctionLibrary->GetContainedGraphs(true /* recursive */));
		}
	}
	return AllModels;
}

TArray<URigVMGraph*> FRigVMClient::GetAllModelsLeavesFirst(bool bIncludeFunctionLibrary) const
{
	TArray<URigVMGraph*> SortedModels = GetAllModels(bIncludeFunctionLibrary, true);
	URigVMController::SortGraphElementsByGraphDepth(SortedModels, true);
	return SortedModels;
}

URigVMController* FRigVMClient::GetController(int32 InIndex) const
{
	return GetController(GetModel(InIndex));
}

URigVMController* FRigVMClient::GetController(const FString& InNodePathOrName) const
{
	return GetController(GetModel(InNodePathOrName));
}

URigVMController* FRigVMClient::GetController(const URigVMGraph* InModel) const
{
	if(InModel == nullptr)
	{
		InModel = GetDefaultModel();
	}
	
	if(InModel)
	{
		const FSoftObjectPath Key(InModel);
		if(const TObjectPtr<URigVMController>* Controller = Controllers.Find(Key))
		{
			checkf(Controller->Get()->GetGraph() == InModel, TEXT("Controller %s contains unexpected graph."), *Controller->GetPathName());
			return Controller->Get();
		}
	}
	return nullptr;
}

URigVMController* FRigVMClient::GetController(const UObject* InEditorSideObject) const
{
	check(InEditorSideObject);

	if (InEditorSideObject->Implements<URigVMEditorSideObject>())
	{
		const IRigVMEditorSideObject* UserInterfaceGraph = Cast<IRigVMEditorSideObject>(InEditorSideObject);
		return GetController(UserInterfaceGraph->GetRigVMNodePath());
	}

	return nullptr;
}

URigVMController* FRigVMClient::GetOrCreateController(int32 InIndex)
{
	return GetOrCreateController(GetModel(InIndex));
}

URigVMController* FRigVMClient::GetOrCreateController(const FString& InNodePathOrName)
{
	return GetOrCreateController(GetModel(InNodePathOrName));
}

URigVMController* FRigVMClient::GetOrCreateController(const URigVMGraph* InModel)
{
	if(InModel == nullptr)
	{
		InModel = GetDefaultModel();
	}

	if(InModel)
	{
		if(URigVMController* Controller = GetController(InModel))
		{
			return Controller;
		}
		return CreateController(InModel);
	}
	return nullptr; 
}

URigVMController* FRigVMClient::GetOrCreateController(const UObject* InEditorSideObject)
{
	check(InEditorSideObject);

	if (InEditorSideObject->Implements<URigVMEditorSideObject>())
	{
		const IRigVMEditorSideObject* UserInterfaceGraph = Cast<IRigVMEditorSideObject>(InEditorSideObject);
		return GetOrCreateController(UserInterfaceGraph->GetRigVMNodePath());
	}

	return nullptr;
}

URigVMController* FRigVMClient::GetControllerByName(const FString InGraphName) const
{
	if (InGraphName.IsEmpty())
	{
		if (const URigVMGraph* DefaultModel = GetDefaultModel())
		{
			return GetController(DefaultModel);
		}
	}

	for (const URigVMGraph* Graph : GetAllModels(true, true))
	{
		if (Graph->GetName() == InGraphName || Graph->GetGraphName() == InGraphName)
		{
			return GetController(Graph);
		}
	}

	return nullptr;
}

bool FRigVMClient::RemoveController(const URigVMGraph* InModel)
{
	const FSoftObjectPath Key(InModel);
	URigVMController* Controller = GetController(InModel);
	const bool bSuccess = Controllers.Remove(Key) > 0;
	if(Controller)
	{
		Controller->SetActionStack(nullptr);
		Controller->Rename(nullptr, GetTransientPackage(), REN_ForceNoResetLoaders | REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional);
		Controller->RemoveFromRoot();
		Controller->MarkAsGarbage();
	}
	return bSuccess;
}

URigVMGraph* FRigVMClient::AddModel(const FString InName, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	const FString DesiredName = FString::Printf(TEXT("%s %s"), FRigVMClient::RigVMModelPrefix, *InName);
	return AddModel(*DesiredName, bSetupUndoRedo);
}

URigVMGraph* FRigVMClient::AddModel(const FName& InName, bool bSetupUndoRedo, const FObjectInitializer* ObjectInitializer, bool bCreateController)
{
#if WITH_EDITOR
	TSharedPtr<FScopedTransaction> Transaction;
	if(bSetupUndoRedo)
	{
		Transaction = MakeShared<FScopedTransaction>(NSLOCTEXT("RigVMClient", "AddModel", "Add new root graph"));
		GetOuter()->Modify();
	}
#endif

	const FName SafeGraphName = GetUniqueName(InName);
	URigVMGraph* Model = nullptr;
	if(ObjectInitializer)
	{
		Model = ObjectInitializer->CreateDefaultSubobject<URigVMGraph>(GetOuter(), SafeGraphName);
	}
	else
	{
		Model = NewObject<URigVMGraph>(GetOuter(), SafeGraphName);
	}
	AddModel(Model, bCreateController);

	if(bSetupUndoRedo)
	{
		GetOuter()->Modify();
		UndoRedoIndex++;

		FRigVMClientAction Action;
		Action.Type = ERigVMClientAction::ERigVMClientAction_AddModel;
		Action.NodePath = Model->GetNodePath();
		UndoStack.Push(Action);
		RedoStack.Reset();
	}
	return Model;
}

void FRigVMClient::AddModel(URigVMGraph* InModel, bool bCreateController)
{
	check(InModel);

	if(InModel->IsA<URigVMFunctionLibrary>())
	{
		check(FunctionLibrary == nullptr);
		FunctionLibrary = Cast<URigVMFunctionLibrary>(InModel);
	}
	else
	{
		Models.Add(InModel);
	}

	InModel->SetExecuteContextStruct(GetOrCreateSchema()->GetExecuteContextStruct());

	if(bCreateController)
	{
		CreateController(InModel);
	}

	if(InModel->IsA<URigVMFunctionLibrary>())
	{
		for(URigVMGraph* Model : Models)
		{
			Model->SetDefaultFunctionLibrary(FunctionLibrary);
		}
		InModel->SetDefaultFunctionLibrary(FunctionLibrary);
	}
	else if(FunctionLibrary)
	{
		InModel->SetDefaultFunctionLibrary(FunctionLibrary);
	}

	if (GetOuter()->Implements<URigVMClientHost>())
	{
		IRigVMClientHost* ClientHost = Cast<IRigVMClientHost>(GetOuter());
		ClientHost->HandleRigVMGraphAdded(this, InModel->GetNodePath());
	}
		
	NotifyOuterOfPropertyChange();

}

URigVMFunctionLibrary* FRigVMClient::GetOrCreateFunctionLibrary(bool bSetupUndoRedo, const FObjectInitializer* ObjectInitializer, bool bCreateController)
{
	if(FunctionLibrary)
	{
		return FunctionLibrary;
	}
	
#if WITH_EDITOR
	TSharedPtr<FScopedTransaction> Transaction;
	if(bSetupUndoRedo)
	{
		Transaction = MakeShared<FScopedTransaction>(NSLOCTEXT("RigVMClient", "AddModel", "Add new root graph"));
		GetOuter()->Modify();
	}
#endif

	static constexpr TCHAR FunctionLibraryName[] = TEXT("RigVMFunctionLibrary");
	const FName SafeGraphName = GetUniqueName(FunctionLibraryName);
	URigVMFunctionLibrary* NewFunctionLibrary = nullptr;
	if(ObjectInitializer)
	{
		NewFunctionLibrary = ObjectInitializer->CreateDefaultSubobject<URigVMFunctionLibrary>(GetOuter(), SafeGraphName);
	}
	else
	{
		NewFunctionLibrary = NewObject<URigVMFunctionLibrary>(GetOuter(), SafeGraphName);
	}

	NewFunctionLibrary->GetFunctionHostObjectPathDelegate.BindLambda([this]() -> const FSoftObjectPath 
		{
			if (GetOuter()->Implements<URigVMClientHost>())
			{
				IRigVMClientHost* ClientHost = Cast<IRigVMClientHost>(GetOuter());
				return Cast<UObject>(ClientHost->GetRigVMGraphFunctionHost());				
			}
			return nullptr;
		});
	
	AddModel(NewFunctionLibrary, bCreateController);
	return NewFunctionLibrary;
}

TArray<FName> FRigVMClient::GetEntryNames() const
{
	TArray<FName> EntryNames;
	for(const URigVMGraph* Model : Models)
	{
		for(const URigVMNode* Node : Model->GetNodes())
		{
			const FName EntryName = Node->GetEventName();
			if(!EntryName.IsNone())
			{
				EntryNames.Add(EntryName);
			}
		}
	}
	return EntryNames;
}

UScriptStruct* FRigVMClient::GetExecuteContextStruct() const
{
	check(SchemaPtr);
	return SchemaPtr->GetExecuteContextStruct();
}

void FRigVMClient::SetExecuteContextStruct(UScriptStruct* InExecuteContextStruct)
{
	GetOrCreateSchema()->SetExecuteContextStruct(InExecuteContextStruct);
}

URigVMGraph* FRigVMClient::GetFocusedModel() const
{
#if WITH_EDITOR
	if (OnGetFocusedGraph().IsBound())
	{
		return OnGetFocusedGraph().Execute();
	}
#endif

	return GetDefaultModel();
}

bool FRigVMClient::RemoveModel(FString InName, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	return RemoveModel(InName, bSetupUndoRedo);
}

bool FRigVMClient::RemoveModel(const FString& InNodePathOrName, bool bSetupUndoRedo)
{
	if(URigVMGraph* Model = GetModel(InNodePathOrName))
	{
		if(Model == GetDefaultModel() && !bDefaultModelCanBeRemoved)
		{
#if WITH_EDITOR
			static constexpr TCHAR Message[] = TEXT("Cannot remove the default model.");
			FScriptExceptionHandler::Get().HandleException(ELogVerbosity::Error, Message, *FString());
#endif
			return false;
		}

		if(Model == FunctionLibrary)
		{
#if WITH_EDITOR
			static constexpr TCHAR Message[] = TEXT("Cannot remove the function library.");
			FScriptExceptionHandler::Get().HandleException(ELogVerbosity::Error, Message, *FString());
#endif
			return false;
		}

#if WITH_EDITOR
		TSharedPtr<FScopedTransaction> Transaction;
		if(bSetupUndoRedo)
		{
			Transaction = MakeShared<FScopedTransaction>(NSLOCTEXT("RigVMClient", "RemoveModel", "Remove a root graph"));
			GetOuter()->Modify();
		}
#endif

		if (GetOuter()->Implements<URigVMClientHost>())
		{
			IRigVMClientHost* ClientHost = Cast<IRigVMClientHost>(GetOuter());
			ClientHost->HandleRigVMGraphRemoved(this, InNodePathOrName);
		}

		if(bSetupUndoRedo)
		{
			GetOuter()->Modify();
			UndoRedoIndex++;

			FRigVMClientAction Action;
			Action.Type = ERigVMClientAction_RemoveModel;
			Action.NodePath = Model->GetNodePath();
			UndoStack.Push(Action);
			RedoStack.Reset();
		}

		// clean up the model
		Models.Remove(Model);
		
		// remove the controller
		verify(RemoveController(Model));

		NotifyOuterOfPropertyChange();
		return true;
	}
	return false;
}

FName FRigVMClient::RenameModel(const FString& InNodePathOrName, const FName& InNewName, bool bSetupUndoRedo)
{
	if(URigVMGraph* Model = GetModel(InNodePathOrName))
	{
		if(Model == FunctionLibrary)
		{
#if WITH_EDITOR
			static constexpr TCHAR Message[] = TEXT("Cannot rename the function library.");
			FScriptExceptionHandler::Get().HandleException(ELogVerbosity::Error, Message, *FString());
#endif
			return NAME_None;
		}

		if(Model->GetFName() == InNewName)
		{
			return InNewName;
		}

#if WITH_EDITOR
		TSharedPtr<FScopedTransaction> Transaction;
		if(bSetupUndoRedo)
		{
			Transaction = MakeShared<FScopedTransaction>(NSLOCTEXT("RigVMClient", "RenameModel", "Rename a root graph"));
		}
#endif

		TObjectPtr<URigVMController>* Controller = Controllers.Find(Model);
		const FString OldNodePath = Model->GetNodePath();
		const FName SafeNewName = GetUniqueName(InNewName);
		Model->Rename(*SafeNewName.ToString(), nullptr, REN_ForceNoResetLoaders | REN_DontCreateRedirectors);
		const FString NewNodePath = Model->GetNodePath();
		if (Controller)
		{
			Controllers.Add(Model, *Controller);
		}

		if(bSetupUndoRedo)
		{
			GetOuter()->Modify();
			UndoRedoIndex++;

			FRigVMClientAction Action;
			Action.Type = ERigVMClientAction_RenameModel;
			Action.NodePath = OldNodePath;
			Action.OtherNodePath = NewNodePath;
			UndoStack.Push(Action);
			RedoStack.Reset();
		}

		if (GetOuter()->Implements<URigVMClientHost>())
		{
			IRigVMClientHost* ClientHost = Cast<IRigVMClientHost>(GetOuter());
			ClientHost->HandleRigVMGraphRenamed(this, OldNodePath, NewNodePath);
		}
		
		NotifyOuterOfPropertyChange();
		return SafeNewName;
	}

	return NAME_None;
}

void FRigVMClient::PostTransacted(const FTransactionObjectEvent& TransactionEvent)
{
	IRigVMClientHost* ClientHost = CastChecked<IRigVMClientHost>(GetOuter());

	if (TransactionEvent.GetEventType() == ETransactionObjectEventType::UndoRedo)
	{
		auto PerformAction = [this, ClientHost](const FRigVMClientAction& InAction, bool bUndo)
		{
			switch(InAction.Type)
			{
				case ERigVMClientAction_AddModel:
				case ERigVMClientAction_RemoveModel:
				{
					if(InAction.Type == ERigVMClientAction_RemoveModel)
					{
						bUndo = !bUndo;
					}
						
					if(URigVMGraph* Model = GetModel(InAction.NodePath))
					{
						if(bUndo)
						{
							ClientHost->HandleRigVMGraphRemoved(this, InAction.NodePath);
						}
						else
						{
							URigVMController* Controller = GetOrCreateController(Model);
							ClientHost->HandleRigVMGraphAdded(this, InAction.NodePath);
							if(Controller)
							{
								Controller->ResendAllNotifications();
							}
						}
					}
					break;
				}
				case ERigVMClientAction_RenameModel:
				{
					const FString NodePathA = bUndo ? InAction.OtherNodePath : InAction.NodePath;
					const FString NodePathB = bUndo ? InAction.NodePath : InAction.OtherNodePath;
					FString NodeNameB = NodePathB;
					NodeNameB.Split(TEXT("|"), nullptr, &NodeNameB, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
					NodeNameB.RemoveFromEnd(TEXT("::"));

					RenameModel(NodePathA, *NodeNameB, false);
					break;
				}
			}
		};
		
		while(UndoStack.Num() != UndoRedoIndex)
		{
			// do we need to undo - or redo?
			if(UndoStack.Num() > UndoRedoIndex)
			{
				FRigVMClientAction Action = UndoStack.Pop();
				PerformAction(Action, true);
				RedoStack.Push(Action);
			}
			else
			{
				FRigVMClientAction Action = RedoStack.Pop();
				PerformAction(Action, false);
				UndoStack.Push(Action);
			}
		}
	}
}

void FRigVMClient::OnCollapseNodeRenamed(const URigVMCollapseNode* InCollapseNode)
{
	TMap<FSoftObjectPath, TObjectPtr<URigVMController>> OldControllers = Controllers;
	for(const TPair<FSoftObjectPath, TObjectPtr<URigVMController>>& Pair : OldControllers)
	{
		if(URigVMController* Controller = Pair.Value)
		{
			if(const URigVMGraph* Graph = Controller->GetGraph())
			{
				if(Graph->IsInOuter(InCollapseNode))
				{
					const FSoftObjectPath& OldObjectPath = Pair.Key;
					const FSoftObjectPath NewObjectPath(Graph);

					if(OldObjectPath != NewObjectPath)
					{
						Controllers.Add(NewObjectPath, Controller);
						Controllers.Remove(OldObjectPath);
					}
				}
			}
		}
	}
}

void FRigVMClient::OnCollapseNodeRemoved(const URigVMCollapseNode* InCollapseNode)
{
	TMap<FSoftObjectPath, TObjectPtr<URigVMController>> OldControllers = Controllers;
	for(const TPair<FSoftObjectPath, TObjectPtr<URigVMController>>& Pair : OldControllers)
	{
		if(URigVMController* Controller = Pair.Value)
		{
			if(const URigVMGraph* Graph = Controller->GetGraph())
			{
				if(Graph->IsInOuter(InCollapseNode))
				{
					RemoveController(Graph);
				}
			}
		}
	}
}

URigVMNode* FRigVMClient::FindNode(const FString& InNodePathOrName) const
{
	for(const URigVMGraph* Model : Models)
	{
		if(URigVMNode* Node = Model->FindNode(InNodePathOrName))
		{
			return Node;
		}
	}
	if(FunctionLibrary)
	{
		return FunctionLibrary->FindNode(InNodePathOrName);
	}
	return nullptr;
}

URigVMPin* FRigVMClient::FindPin(const FString& InPinPath) const
{
	for(const URigVMGraph* Model : Models)
	{
		if(URigVMPin* Pin = Model->FindPin(InPinPath))
		{
			return Pin;
		}
	}
	if(FunctionLibrary)
	{
		return FunctionLibrary->FindPin(InPinPath);
	}
	return nullptr;
}

UObject* FRigVMClient::GetOuter() const
{
	UObject* Outer = OuterClientHost.Get();
	check(Outer);
	return Outer;
}

FProperty* FRigVMClient::GetOuterClientProperty() const
{
	return GetOuter()->GetClass()->FindPropertyByName(OuterClientPropertyName);
}

void FRigVMClient::NotifyOuterOfPropertyChange(EPropertyChangeType::Type ChangeType) const
{
	if(bSuspendNotifications)
	{
		return;
	}
	FProperty* Property = GetOuterClientProperty();
	FPropertyChangedEvent PropertyChangedEvent(Property, ChangeType);
	GetOuter()->PostEditChangeProperty(PropertyChangedEvent);
}

URigVMController* FRigVMClient::CreateController(const URigVMGraph* InModel)
{
	const FString ModelName = InModel ? InModel->GetName() : TEXT("__NullGraph");
	const FName SafeControllerName = GetUniqueName(*FString::Printf(TEXT("%s_Controller"), *ModelName));
	URigVMController* Controller = NewObject<URigVMController>(GetOuter(), ControllerClass, SafeControllerName);
	Controllers.Add(InModel, Controller);
	Controller->SetSchema(GetOrCreateSchema());
	Controller->SetActionStack(GetOrCreateActionStack());
	if(InModel)
	{
		Controller->SetGraph((URigVMGraph*)InModel);
	}
	Controller->OnModified().AddLambda([this](ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject)
	{
		HandleGraphModifiedEvent(InNotifType, InGraph, InSubject);
	});

	if (GetOuter()->Implements<URigVMClientHost>())
	{
		IRigVMClientHost* ClientHost = Cast<IRigVMClientHost>(GetOuter());
		ClientHost->HandleConfigureRigVMController(this, Controller);
	}

	if(InModel)
	{
		Controller->RemoveStaleNodes();
	}
	return Controller;
}

URigVMActionStack* FRigVMClient::GetOrCreateActionStack()
{
	if(UObject* Outer = GetOuter())
	{
		if (ActionStack && ActionStack->GetOuter() != Outer)
		{
			ResetActionStack();
		}
		
		if(ActionStack == nullptr)
		{
			ActionStack = NewObject<URigVMActionStack>(Outer, TEXT("ActionStack"));
		}
	}
	return ActionStack;
}

void FRigVMClient::ResetActionStack()
{
	DestroyObject(ActionStack);
	ActionStack = nullptr;
}

void FRigVMClient::SetSchema(URigVMSchema* InSchema)
{
	check(InSchema);
	SchemaPtr = InSchema;
	SchemaClass = InSchema->GetClass();
}

FName FRigVMClient::GetUniqueName(const FName& InDesiredName) const
{
	return GetUniqueName(GetOuter(), InDesiredName);
}

FName FRigVMClient::GetUniqueName(UObject* InOuter, const FName& InDesiredName)
{
	return URigVMSchema::GetUniqueName(*InDesiredName.ToString(), [InOuter](const FName& InName) -> bool
	{
		return FindObjectWithOuter(InOuter, nullptr, InName) == nullptr; 
	}, false, true);
}

void FRigVMClient::DestroyObject(UObject* InObject)
{
	if(InObject)
	{
		static int32 ObjectIndexToBeDestroyed = 0;
		static constexpr TCHAR ObjectNameFormat[] = TEXT("RigVMClient_ObjectToBeDestroyed_%d");
		const FString NewObjectName = FString::Printf(ObjectNameFormat, ObjectIndexToBeDestroyed++);
		InObject->Rename(*NewObjectName, GetTransientPackage(), REN_ForceNoResetLoaders | REN_DontCreateRedirectors);
		if(!InObject->IsRooted())
		{
			InObject->MarkAsGarbage();
		}
	}
}

uint32 FRigVMClient::GetStructureHash() const
{
	uint32 Hash = 0;
	const TArray<URigVMGraph*> ModelsAndFunctionLibrary = GetAllModels(true, true);
	for(const URigVMGraph* Model : ModelsAndFunctionLibrary)
	{
		Hash = HashCombine(Hash, Model->GetStructureHash());
	}
	return Hash;
}

uint32 FRigVMClient::GetSerializedStructureHash() const
{
	uint32 Hash = 0;
	const TArray<URigVMGraph*> ModelsAndFunctionLibrary = GetAllModels(true, true);
	for(const URigVMGraph* Model : ModelsAndFunctionLibrary)
	{
		Hash = HashCombine(Hash, Model->GetSerializedStructureHash());
	}
	return Hash;
}

FRigVMClientPatchResult FRigVMClient::PatchModelsOnLoad()
{
	FRigVMClientPatchResult Result;
	
	TArray<URigVMGraph*> AllModels = GetAllModelsLeavesFirst(true);
	TGuardValue<bool> ClientIgnoreModificationsGuard(bIgnoreModelNotifications, true);
	for(URigVMGraph* Model : AllModels)
	{
		Model->PostLoad();
		
		URigVMController* Controller = GetOrCreateController(Model);
		TGuardValue<bool> GuardSuspendTemplateComputation(Controller->bSuspendTemplateComputation, true);
		TGuardValue<bool> GuardIsTransacting(Controller->bIsTransacting, true);

		Result.Merge(Controller->PatchRerouteNodesOnLoad());
		Result.Merge(Controller->PatchUnitNodesOnLoad());
		Result.Merge(Controller->PatchDispatchNodesOnLoad());
		Result.Merge(Controller->PatchBranchNodesOnLoad());
		Result.Merge(Controller->PatchIfSelectNodesOnLoad());
		Result.Merge(Controller->PatchArrayNodesOnLoad());
		Result.Merge(Controller->PatchReduceArrayFloatDoubleConvertsionsOnLoad());
		Result.Merge(Controller->PatchInvalidLinksOnWildcards());
		Result.Merge(Controller->PatchExecutePins());
		Result.Merge(Controller->PatchLazyPins());

		if (URigVMCollapseNode* CollapseNode = Model->GetTypedOuter<URigVMCollapseNode>())
		{
			Result.Merge(Controller->PatchFunctionsWithInvalidReturnPaths());	
		}
	}

	return Result;
}

void FRigVMClient::ProcessDetachedLinks()
{
	TArray<URigVMGraph*> AllModels = GetAllModels(true, true);
	for(URigVMGraph* Model : AllModels)
	{
		URigVMController* Controller = GetOrCreateController(Model);
		Controller->ProcessDetachedLinks();
	}
}

void FRigVMClient::PostDuplicateHost(const FString& InOldPathName, const FString& InNewPathName)
{
	// this also takes care of wiping invalid schemas on the client
	URigVMSchema* Schema = GetOrCreateSchema();
	
	TArray<URigVMGraph*> AllModels = GetAllModels(true, true);
	for(URigVMGraph* Model : AllModels)
	{
		URigVMController* Controller = GetOrCreateController(Model);
		Controller->SetSchema(Schema);
		Controller->PostDuplicateHost(InOldPathName, InNewPathName);
	}
}

void FRigVMClient::PreSave(FObjectPreSaveContext ObjectSaveContext)
{
	if (!ObjectSaveContext.IsCooking())
	{
		if (IRigVMClientHost* ClientHost = Cast<IRigVMClientHost>(GetOuter()))
		{
			if (IRigVMGraphFunctionHost* FunctionHost = ClientHost->GetRigVMGraphFunctionHost())
			{
				if (FRigVMGraphFunctionStore* Store = FunctionHost->GetRigVMGraphFunctionStore())
				{
					for (URigVMNode* Node : FunctionLibrary->GetNodes())
					{
						if (URigVMLibraryNode* LibraryNode = Cast<URigVMLibraryNode>(Node))
						{
							FRigVMGraphFunctionIdentifier Identifier = LibraryNode->GetFunctionIdentifier();
							if (Store->IsFunctionPublic(Identifier))
							{
								UpdateGraphFunctionSerializedGraph(LibraryNode);
							}
						}
					}
				}
			}
		}
	}
}

void FRigVMClient::HandleGraphModifiedEvent(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject)
{
	if (bIgnoreModelNotifications)
	{
		return;
	}
	
#if WITH_EDITOR

	IRigVMClientHost* ClientHost = Cast<IRigVMClientHost>(GetOuter());
	IRigVMGraphFunctionHost* FunctionHost = nullptr;
	FRigVMGraphFunctionStore* FunctionStore = nullptr;
	if (ClientHost)
	{
		FunctionHost = ClientHost->GetRigVMGraphFunctionHost();
		if (FunctionHost)
		{
			FunctionStore = FunctionHost->GetRigVMGraphFunctionStore();
		}
	}

	switch (InNotifType)
	{
		case ERigVMGraphNotifType::NodeAdded: // A node has been added to the graph (Subject == URigVMNode)
		{
			// A node was added directly into the function library
			if(InGraph->IsA<URigVMFunctionLibrary>())
			{
				if (URigVMCollapseNode* CollapseNode = Cast<URigVMCollapseNode>(InSubject))
				{
					if (GetOuter()->Implements<URigVMClientHost>())
					{
						if (FunctionStore && FunctionHost)
						{
							FunctionStore->AddFunction(CollapseNode->GetFunctionHeader(FunctionHost), false);
						}
					}
				}
			}
			// A node was added into the contained graph of a function
			else if(URigVMLibraryNode* LibraryNode = Cast<URigVMNode>(InSubject)->FindFunctionForNode())
			{
				DirtyGraphFunctionCompilationData(LibraryNode);
				if (URigVMFunctionReferenceNode* FunctionReference = Cast<URigVMFunctionReferenceNode>(InSubject))
				{
					UpdateDependenciesForFunction(LibraryNode);
					UpdateExternalVariablesForFunction(LibraryNode);
				}
			}
			break;	
		}
		case ERigVMGraphNotifType::NodeRemoved: // A node has been removed from the graph (Subject == URigVMNode)
		{		
			if(InSubject->GetOuter()->IsA<URigVMFunctionLibrary>())
			{
				if (URigVMCollapseNode* CollapseNode = Cast<URigVMCollapseNode>(InSubject))
				{
					if (GetOuter()->Implements<URigVMClientHost>())
					{
						if (FunctionStore && FunctionHost)
						{
							FunctionStore->RemoveFunction(FRigVMGraphFunctionIdentifier (Cast<UObject>(FunctionHost), CollapseNode));
						}
					}
				}
			}
			// A node was added into the contained graph of a function
			else if(URigVMLibraryNode* LibraryNode = Cast<URigVMNode>(InSubject)->FindFunctionForNode())
			{
				DirtyGraphFunctionCompilationData(LibraryNode);
				if (URigVMFunctionReferenceNode* FunctionReference = Cast<URigVMFunctionReferenceNode>(InSubject))
				{
					UpdateDependenciesForFunction(LibraryNode);
					UpdateExternalVariablesForFunction(LibraryNode);
				}
			}
			break;	
		}
		case ERigVMGraphNotifType::VariableAdded: // A variable has been added (Subject == URigVMVariableNode)
		case ERigVMGraphNotifType::VariableRemoved: // A variable has been removed (Subject == URigVMVariableNode)
		case ERigVMGraphNotifType::VariableRenamed: // A variable has been renamed (Subject == URigVMVariableNode)
		case ERigVMGraphNotifType::VariableRemappingChanged: // A function reference node's remapping has changed (Subject == URigVMFunctionReferenceNode)
		{
			if(URigVMLibraryNode* LibraryNode = Cast<URigVMNode>(InSubject)->FindFunctionForNode())
			{
				DirtyGraphFunctionCompilationData(LibraryNode);
				UpdateExternalVariablesForFunction(LibraryNode);
			}
			break;
		}
		case ERigVMGraphNotifType::NodeRenamed: // A node has been renamed in the graph (Subject == URigVMNode)
		{
			if(InSubject->GetOuter()->IsA<URigVMFunctionLibrary>())
			{
				if (URigVMCollapseNode* CollapseNode = Cast<URigVMCollapseNode>(InSubject))
				{
					URigVMBuildData* BuildData = URigVMBuildData::Get();
					if (IRigVMGraphFunctionHost* Host = Cast<IRigVMGraphFunctionHost>(CollapseNode->GetFunctionIdentifier().HostObject.ResolveObject()))
					{
						if (FRigVMGraphFunctionData* Data = Host->GetRigVMGraphFunctionStore()->FindFunctionByName(CollapseNode->GetPreviousFName()))
						{
							const FRigVMGraphFunctionIdentifier PreviousFunctionId = Data->Header.LibraryPointer;
							Data->Header = CollapseNode->GetFunctionHeader();

							if (const FRigVMFunctionReferenceArray* FunctionReferencesPtr = BuildData->GraphFunctionReferences.Find(PreviousFunctionId))
							{
								const FRigVMFunctionReferenceArray& FunctionReferences = *FunctionReferencesPtr;
								BuildData->Modify();
								FRigVMFunctionReferenceArray& NewFunctionReferences = BuildData->GraphFunctionReferences.Add(Data->Header.LibraryPointer, FunctionReferences);
								BuildData->GraphFunctionReferences.Remove(PreviousFunctionId);
								BuildData->MarkPackageDirty();

								for (int32 i=0; i<NewFunctionReferences.Num(); ++i)
								{
									if (!NewFunctionReferences[i].IsValid())
									{
										NewFunctionReferences[i].LoadSynchronous();
									}
									if (NewFunctionReferences[i].IsValid())
									{
										NewFunctionReferences[i]->ReferencedFunctionHeader = Data->Header;
										NewFunctionReferences[i]->MarkPackageDirty();
									}
								}
							}

							UpdateFunctionReferences(Data->Header, true, false);
							UpdateGraphFunctionData(CollapseNode);
						}
					}
				}
			}
			break;
		}
		case ERigVMGraphNotifType::NodeColorChanged: // A node's color has changed (Subject == URigVMNode)
		case ERigVMGraphNotifType::NodeCategoryChanged: // A node's category has changed (Subject == URigVMNode)
		case ERigVMGraphNotifType::NodeKeywordsChanged: // A node's keywords have changed (Subject == URigVMNode)
		case ERigVMGraphNotifType::NodeDescriptionChanged: // A node's description has changed (Subject == URigVMNode)
		{
			if(InSubject->GetOuter()->IsA<URigVMFunctionLibrary>())
			{
				if (URigVMCollapseNode* CollapseNode = Cast<URigVMCollapseNode>(InSubject))
				{
					UpdateGraphFunctionData(CollapseNode);
				}
			}
			break;
		}
		
		case ERigVMGraphNotifType::PinAdded: // A pin has been added to a given node (Subject == URigVMPin)
		case ERigVMGraphNotifType::PinRemoved: // A pin has been removed from a given node (Subject == URigVMPin)
		case ERigVMGraphNotifType::PinRenamed: // A pin has been renamed (Subject == URigVMPin)
		case ERigVMGraphNotifType::PinArraySizeChanged: // An array pin's size has changed (Subject == URigVMPin)
		case ERigVMGraphNotifType::PinDefaultValueChanged: // A pin's default value has changed (Subject == URigVMPin)
		case ERigVMGraphNotifType::PinDirectionChanged: // A pin's direction has changed (Subject == URigVMPin)
		case ERigVMGraphNotifType::PinTypeChanged: // A pin's data type has changed (Subject == URigVMPin)
		case ERigVMGraphNotifType::PinIndexChanged: // A pin's index has changed (Subject == URigVMPin)
		case ERigVMGraphNotifType::PinWatchedChanged: // A pin's watch state has changed (Subject == URigVMPin)
		{
			if (URigVMPin* Pin = Cast<URigVMPin>(InSubject))
			{
				if (URigVMNode* Node = Cast<URigVMNode>(Pin->GetNode()))
				{
					if (URigVMLibraryNode* LibraryNode = Node->FindFunctionForNode())
					{
						DirtyGraphFunctionCompilationData(LibraryNode);
					}
					if(Node->GetOuter()->IsA<URigVMFunctionLibrary>())
					{
						if (URigVMCollapseNode* CollapseNode = Cast<URigVMCollapseNode>(Node))
						{
							UpdateGraphFunctionData(CollapseNode);
						}
					}
				}
			}
			break;
		}

		case ERigVMGraphNotifType::LinkAdded: // A link has been added (Subject == URigVMLink)
		case ERigVMGraphNotifType::LinkRemoved: // A link has been removed (Subject == URigVMLink)
		{
			if (URigVMLink* Link = Cast<URigVMLink>(InSubject))
			{
				if (URigVMNode* OuterNode = Cast<URigVMNode>(Link->GetGraph()->GetOuter()))
				{
					if (URigVMLibraryNode* LibraryNode = OuterNode->FindFunctionForNode())
					{
						DirtyGraphFunctionCompilationData(LibraryNode);
					}
				}
			}

			break;
		}

		case ERigVMGraphNotifType::FunctionAccessChanged: // A function was made public/private
		{
			if (URigVMLibraryNode* LibraryNode = Cast<URigVMLibraryNode>(InSubject))
			{
				if (URigVMFunctionLibrary* Library = Cast<URigVMFunctionLibrary>(InGraph))
				{
					bool bIsPublic = Library->IsFunctionPublic(LibraryNode->GetFName());
					if (FRigVMGraphFunctionStore* Store = FindFunctionStore(LibraryNode))
					{
						Store->MarkFunctionAsPublic(LibraryNode->GetFunctionIdentifier(), bIsPublic);
					}
				}
			}
			break;
		}
		
		
		default:
		{
			break;
		}
	}
	
#endif
}

FRigVMGraphFunctionStore* FRigVMClient::FindFunctionStore(const URigVMLibraryNode* InLibraryNode)
{
	if (IRigVMClientHost* ClientHost = InLibraryNode->GetImplementingOuter<IRigVMClientHost>())
	{
		if (IRigVMGraphFunctionHost* FunctionHost = ClientHost->GetRigVMGraphFunctionHost())
		{
			return FunctionHost->GetRigVMGraphFunctionStore();
		}
	}
	return nullptr;
}

bool FRigVMClient::UpdateFunctionReferences(const FRigVMGraphFunctionHeader& Header, bool bUpdateDependencies, bool bUpdateExternalVariables)
{
	URigVMBuildData* BuildData = URigVMBuildData::Get();
	if (const FRigVMFunctionReferenceArray* FunctionReferenceArray = BuildData->FindFunctionReferences(Header.LibraryPointer))
	{
		for (int32 i=0; i<FunctionReferenceArray->Num(); ++i)
		{
			const TSoftObjectPtr<URigVMFunctionReferenceNode>& Reference = FunctionReferenceArray->FunctionReferences[i];

			// Only update references that are loaded
			// Other references will be updated when they are loaded
			if (Reference.IsValid())
			{
				URigVMFunctionReferenceNode* Node = Reference.Get();

				Node->Modify();
				Node->ReferencedFunctionHeader = Header;

				if (bUpdateDependencies || bUpdateExternalVariables)
				{
					if (URigVMLibraryNode* LibraryNode = Node->FindFunctionForNode())
					{
						IRigVMClientHost* OtherClientHost = LibraryNode->GetImplementingOuter<IRigVMClientHost>();
						if (bUpdateDependencies)
						{
							OtherClientHost->GetRigVMClient()->UpdateDependenciesForFunction(LibraryNode);
						}
						if (bUpdateExternalVariables)
						{								
							OtherClientHost->GetRigVMClient()->UpdateExternalVariablesForFunction(LibraryNode);
						}
					}
				}
				Node->MarkPackageDirty();
			}
		}
	}
	return true;
}

bool FRigVMClient::UpdateGraphFunctionData(const URigVMLibraryNode* InLibraryNode)
{
	if (GetOuter()->Implements<URigVMClientHost>())
	{
		if (FRigVMGraphFunctionStore* Store = FindFunctionStore(InLibraryNode))
		{
			if (FRigVMGraphFunctionData* Data = Store->UpdateFunctionInterface(InLibraryNode->GetFunctionHeader()))
			{
				UpdateFunctionReferences(Data->Header, false, false);
				return true;
			}
		}
	}	
	
	return false;	
}

bool FRigVMClient::UpdateExternalVariablesForFunction(const URigVMLibraryNode* InLibraryNode)
{
	if (FRigVMGraphFunctionStore* Store = FindFunctionStore(InLibraryNode))
	{
		FRigVMGraphFunctionIdentifier Identifier = InLibraryNode->GetFunctionIdentifier();
		if (Store->UpdateExternalVariables(Identifier, InLibraryNode->GetExternalVariables()))
		{
			const FRigVMGraphFunctionData* Data = Store->FindFunction(Identifier);
			UpdateFunctionReferences(Data->Header, false, true);
			return true;
		}
	}
	
	return false;
}

bool FRigVMClient::UpdateDependenciesForFunction(const URigVMLibraryNode* InLibraryNode)
{
	if (FRigVMGraphFunctionStore* Store = FindFunctionStore(InLibraryNode))
	{
		TMap<FRigVMGraphFunctionIdentifier, uint32> Dependencies = InLibraryNode->GetDependencies();
		FRigVMGraphFunctionIdentifier Identifier = InLibraryNode->GetFunctionIdentifier();
		if (Store->UpdateDependencies(Identifier, Dependencies))
		{
			const FRigVMGraphFunctionData* Data = Store->FindFunction(Identifier);
			UpdateFunctionReferences(Data->Header, true, false);
			return true;
		}
	}
	
	return false;
}

bool FRigVMClient::DirtyGraphFunctionCompilationData(URigVMLibraryNode* InLibraryNode)
{
	if (FRigVMGraphFunctionStore* Store = FindFunctionStore(InLibraryNode))
	{
		FRigVMGraphFunctionIdentifier Identifier = InLibraryNode->GetFunctionIdentifier();
		if (const FRigVMGraphFunctionData* Data = Store->FindFunction(Identifier))
		{
			Store->RemoveFunctionCompilationData(Identifier);

			// References to this function will check if the compilation hash matches, and will recompile if they
			// see a different compilation hash. No need to dirty their compilation data.
			
			return true;
		}
	}

	return false;
}

bool FRigVMClient::UpdateGraphFunctionSerializedGraph(URigVMLibraryNode* InLibraryNode)
{
	if (FRigVMGraphFunctionStore* Store = FindFunctionStore(InLibraryNode))
	{
		FRigVMGraphFunctionIdentifier Identifier = InLibraryNode->GetFunctionIdentifier();
		if (FRigVMGraphFunctionData* Data = Store->FindFunction(Identifier))
		{
			FStringOutputDevice Archive;
			const FExportObjectInnerContext Context;
			UExporter::ExportToOutputDevice(&Context, InLibraryNode, NULL, Archive, TEXT("copy"), 0, PPF_ExportsNotFullyQualified | PPF_Copy | PPF_Delimited, false, nullptr);
			Data->SerializedCollapsedNode = Archive;
			return true;
		}
	}
	return false;
}

bool FRigVMClient::IsFunctionPublic(URigVMLibraryNode* InLibraryNode)
{
	if (FRigVMGraphFunctionStore* Store = FindFunctionStore(InLibraryNode))
	{
		return Store->IsFunctionPublic(InLibraryNode->GetFunctionIdentifier());
	}
	return false;
}
