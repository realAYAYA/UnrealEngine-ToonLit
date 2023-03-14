// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMModel/RigVMClient.h"
#include "Misc/TransactionObjectEvent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMClient)

#if WITH_EDITOR
#include "ScopedTransaction.h"
#endif

void FRigVMClient::SetOuterClientHost(UObject* InOuterClientHost, const FName& InOuterClientHostPropertyName)
{
	OuterClientHost = InOuterClientHost;
	OuterClientPropertyName = InOuterClientHostPropertyName;

	check(OuterClientHost->Implements<URigVMClientHost>());
	check(GetOuterClientProperty() != nullptr);
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
}

URigVMGraph* FRigVMClient::GetDefaultModel() const
{
	if(Models.IsEmpty())
	{
		return nullptr;
	}
	return GetModel(0);
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
			if(const URigVMLibraryNode* LibraryNode = Cast<URigVMLibraryNode>(Model->FindNode(RemainingNodePath)))
			{
				return LibraryNode->GetContainedGraph();
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

bool FRigVMClient::RemoveModel(const FString& InNodePathOrName, bool bSetupUndoRedo)
{
	if(URigVMGraph* Model = GetModel(InNodePathOrName))
	{
		if(Model == GetDefaultModel())
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

		// remove the controller
		if(URigVMController* Controller = GetController(Model))
		{
			verify(Controller == Controllers.FindAndRemoveChecked(Model));
		}

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

		const FString OldNodePath = Model->GetNodePath();
		const FName SafeNewName = GetUniqueName(InNewName);
		Model->Rename(*SafeNewName.ToString(), nullptr, REN_ForceNoResetLoaders | REN_DontCreateRedirectors);
		const FString NewNodePath = Model->GetNodePath();

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

void FRigVMClient::OnSubGraphRenamed(const FSoftObjectPath& OldObjectPath, const FSoftObjectPath& NewObjectPath)
{
	if(OldObjectPath != NewObjectPath)
	{
		if (TObjectPtr<URigVMController>* Controller = Controllers.Find(OldObjectPath))
		{
			Controllers.Add(NewObjectPath, *Controller);
			Controllers.Remove(OldObjectPath);
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
	const FName SafeControllerName = GetUniqueName(*FString::Printf(TEXT("%s_Controller"), *InModel->GetName()));
	URigVMController* Controller = NewObject<URigVMController>(GetOuter(), SafeControllerName);
	Controllers.Add(InModel, Controller);
	Controller->SetGraph((URigVMGraph*)InModel);

	if (GetOuter()->Implements<URigVMClientHost>())
	{
		IRigVMClientHost* ClientHost = Cast<IRigVMClientHost>(GetOuter());
		ClientHost->HandleConfigureRigVMController(this, Controller);
	}
	
	Controller->RemoveStaleNodes();
	return Controller;
}

FName FRigVMClient::GetUniqueName(const FName& InDesiredName) const
{
	return GetUniqueName(GetOuter(), InDesiredName);
}

FName FRigVMClient::GetUniqueName(UObject* InOuter, const FName& InDesiredName)
{
	return URigVMController::GetUniqueName(*InDesiredName.ToString(), [InOuter](const FName& InName) -> bool
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
		InObject->MarkAsGarbage();
	}
}

