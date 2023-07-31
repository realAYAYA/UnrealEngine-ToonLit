// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMGraph.h"
#include "RigVMFunctionLibrary.h"
#include "RigVMController.h"
#include "RigVMClient.generated.h"

struct FRigVMClient;

UINTERFACE()
class RIGVMDEVELOPER_API URigVMClientHost : public UInterface
{
	GENERATED_BODY()
};

// Interface that allows an object to host a rig VM client. Used by graph edting code to interact with the controller.
class RIGVMDEVELOPER_API IRigVMClientHost
{
	GENERATED_BODY()

public:
	
	// Returns the rigvm client for this host
	virtual FRigVMClient* GetRigVMClient() = 0;

	// Returns the rigvm client for this host
	virtual const FRigVMClient* GetRigVMClient() const = 0;

	// Returns the editor object corresponding with the supplied editor object
	virtual UObject* GetEditorObjectForRigVMGraph(URigVMGraph* InVMGraph) const = 0;

	// Reacts to adding a graph
	virtual void HandleRigVMGraphAdded(const FRigVMClient* InClient, const FString& InNodePathOrName) = 0;

	// Reacts to removing a graph
	virtual void HandleRigVMGraphRemoved(const FRigVMClient* InClient, const FString& InNodePathOrName) = 0;

	// Reacts to renaming a graph
	virtual void HandleRigVMGraphRenamed(const FRigVMClient* InClient, const FString& InOldNodePath, const FString& InNewNodePath) = 0;

	// Reacts to a request to configure a controller
	virtual void HandleConfigureRigVMController(const FRigVMClient* InClient, URigVMController* InControllerToConfigure) = 0;
};

UINTERFACE()
class RIGVMDEVELOPER_API URigVMEditorSideObject : public UInterface
{
	GENERATED_BODY()
};

// Interface that allows a UI graph to identify itself against a model graph
class RIGVMDEVELOPER_API IRigVMEditorSideObject
{
	GENERATED_BODY()

public:

	// Returns the corresponding VM graph
	virtual FRigVMClient* GetRigVMClient() const = 0;

	// Returns the nodepath for this UI graph
	virtual FString GetRigVMNodePath() const = 0;

	// Reacts to renaming the model
	virtual void HandleRigVMGraphRenamed(const FString& InOldNodePath, const FString& InNewNodePath) = 0;
};

// A management struct containing graphs and controllers.
USTRUCT()
struct RIGVMDEVELOPER_API FRigVMClient
{
public:

	GENERATED_BODY()

	FRigVMClient()
		: FunctionLibrary(nullptr)
		, bSuspendNotifications(false)
		, OuterClientHost(nullptr)
		, OuterClientPropertyName(NAME_None)
	{
	}

	void SetOuterClientHost(UObject* InOuterClientHost, const FName& InOuterClientHostPropertyName);
	void SetFromDeprecatedData(URigVMGraph* InDefaultGraph, URigVMFunctionLibrary* InFunctionLibrary);

	void Reset();
	FORCEINLINE int32 Num() const { return Models.Num(); }
	URigVMGraph* GetDefaultModel() const;
	FORCEINLINE URigVMGraph* GetModel(int32 InIndex) const { return Models[InIndex]; }
	URigVMGraph* GetModel(const FString& InNodePathOrName) const;
	URigVMGraph* GetModel(const UObject* InEditorSideObject) const;
	TArray<URigVMGraph*> GetAllModels(bool bIncludeFunctionLibrary, bool bRecursive) const;
	URigVMController* GetController(int32 InIndex) const;
	URigVMController* GetController(const FString& InNodePathOrName) const;
	URigVMController* GetController(const URigVMGraph* InModel) const;
	URigVMController* GetController(const UObject* InEditorSideObject) const;
	URigVMController* GetOrCreateController(int32 InIndex);
	URigVMController* GetOrCreateController(const FString& InNodePathOrName);
	URigVMController* GetOrCreateController(const URigVMGraph* InModel);
	URigVMController* GetOrCreateController(const UObject* InEditorSideObject);
	URigVMFunctionLibrary* GetFunctionLibrary() const { return FunctionLibrary; }
	URigVMFunctionLibrary* GetOrCreateFunctionLibrary(bool bSetupUndoRedo, const FObjectInitializer* ObjectInitializer = nullptr, bool bCreateController = true);
	TArray<FName> GetEntryNames() const;

	URigVMGraph* AddModel(const FName& InName, bool bSetupUndoRedo, const FObjectInitializer* ObjectInitializer = nullptr, bool bCreateController = true);
	void AddModel(URigVMGraph* InModel, bool bCreateController);
	bool RemoveModel(const FString& InNodePathOrName, bool bSetupUndoRedo);
	FName RenameModel(const FString& InNodePathOrName, const FName& InNewName, bool bSetupUndoRedo);
	void PostTransacted(const FTransactionObjectEvent& TransactionEvent);
	void OnSubGraphRenamed(const FSoftObjectPath& OldObjectPath, const FSoftObjectPath& NewObjectPath);

	URigVMNode* FindNode(const FString& InNodePathOrName) const;
	URigVMPin* FindPin(const FString& InPinPath) const;
	
	FORCEINLINE TArray<TObjectPtr<URigVMGraph>>::RangedForIteratorType      begin() { return Models.begin(); }
	FORCEINLINE TArray<TObjectPtr<URigVMGraph>>::RangedForConstIteratorType begin() const { return Models.begin(); }
	FORCEINLINE TArray<TObjectPtr<URigVMGraph>>::RangedForIteratorType      end() { return Models.end(); }
	FORCEINLINE TArray<TObjectPtr<URigVMGraph>>::RangedForConstIteratorType end() const { return Models.end(); }

	UObject* GetOuter() const;
	FProperty* GetOuterClientProperty() const;
	void NotifyOuterOfPropertyChange(EPropertyChangeType::Type ChangeType = EPropertyChangeType::Interactive) const;
	FName GetUniqueName(const FName& InDesiredName) const;
	static FName GetUniqueName(UObject* InOuter, const FName& InDesiredName);
	static void DestroyObject(UObject* InObject);

private:

	enum ERigVMClientAction
	{
		ERigVMClientAction_AddModel,
		ERigVMClientAction_RemoveModel,
		ERigVMClientAction_RenameModel
	};

	struct FRigVMClientAction
	{
		ERigVMClientAction Type;
		FString NodePath;
		FString OtherNodePath;
	};

	URigVMController* CreateController(const URigVMGraph* InModel);

	UPROPERTY()
	TArray<TObjectPtr<URigVMGraph>> Models;

	UPROPERTY()
	TObjectPtr<URigVMFunctionLibrary> FunctionLibrary;

	UPROPERTY(transient)
	TMap<FSoftObjectPath, TObjectPtr<URigVMController>> Controllers;

	UPROPERTY(transient)
	int32 UndoRedoIndex = 0;
	
	TArray<FRigVMClientAction> UndoStack;
	TArray<FRigVMClientAction> RedoStack;

public:
	bool bSuspendNotifications;
private:
	TWeakObjectPtr<UObject> OuterClientHost;
	FName OuterClientPropertyName;
};
