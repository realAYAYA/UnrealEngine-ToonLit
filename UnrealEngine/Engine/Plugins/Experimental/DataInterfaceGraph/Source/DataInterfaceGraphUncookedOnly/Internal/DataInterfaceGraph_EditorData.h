// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigVMModel/RigVMGraph.h"
#include "DataInterfaceGraph_EdGraph.h"
#include "DataInterfaceGraph_EditorData.generated.h"

class UDataInterfaceGraph;
enum class ERigVMGraphNotifType : uint8;
namespace UE::DataInterfaceGraphUncookedOnly
{
	struct FUtils;
}

namespace UE::DataInterfaceGraphEditor
{
	class FGraphEditor;
}

UCLASS(MinimalAPI)
class UDataInterfaceGraph_EditorData : public UObject, public IRigVMClientHost
{
	GENERATED_BODY()

	UDataInterfaceGraph_EditorData(const FObjectInitializer& ObjectInitializer);
	
	friend class UDataInterfaceGraphFactory;
	friend class UDataInterfaceGraph_EdGraph;
	friend struct UE::DataInterfaceGraphUncookedOnly::FUtils;
	friend class UE::DataInterfaceGraphEditor::FGraphEditor;
	friend struct FDataInterfaceGraphSchemaAction_RigUnit;

	// UObject interface
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
	virtual bool IsEditorOnly() const override { return true; }
	
	// IRigVMClientHost interface
	virtual FRigVMClient* GetRigVMClient() override;
	virtual const FRigVMClient* GetRigVMClient() const override;
	virtual void HandleRigVMGraphAdded(const FRigVMClient* InClient, const FString& InNodePath) override;
	virtual void HandleRigVMGraphRemoved(const FRigVMClient* InClient, const FString& InNodePath) override {}
	virtual void HandleRigVMGraphRenamed(const FRigVMClient* InClient, const FString& InOldNodePath, const FString& InNewNodePath) override {}
	virtual void HandleConfigureRigVMController(const FRigVMClient* InClient, URigVMController* InControllerToConfigure) override;
	virtual UObject* GetEditorObjectForRigVMGraph(URigVMGraph* InVMGraph) const override;

	DATAINTERFACEGRAPHUNCOOKEDONLY_API void Initialize(bool bRecompileVM);

	void RecompileVM();
	
	void RecompileVMIfRequired();

	void RequestAutoVMRecompilation();
	
	void HandleModifiedEvent(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject);

	DATAINTERFACEGRAPHUNCOOKEDONLY_API URigVMGraph* GetVMGraphForEdGraph(const UEdGraph* InGraph) const;

	void CreateEdGraphForCollapseNode(URigVMCollapseNode* InNode);
	
	UPROPERTY()
	TObjectPtr<UDataInterfaceGraph_EdGraph> RootGraph;

	UPROPERTY()
	TObjectPtr<UDataInterfaceGraph_EdGraph> EntryPointGraph;
	
	UPROPERTY()
	TObjectPtr<UDataInterfaceGraph_EdGraph> FunctionLibraryEdGraph;

	UPROPERTY()
	FRigVMClient RigVMClient;

	UPROPERTY()
	TObjectPtr<URigVMGraph> RigVMGraph_DEPRECATED;
	
	UPROPERTY()
	TObjectPtr<URigVMFunctionLibrary> RigVMFunctionLibrary_DEPRECATED;

	UPROPERTY()
	TObjectPtr<URigVMLibraryNode> EntryPoint;
	
	UPROPERTY(transient)
	TMap<TObjectPtr<URigVMGraph>, TObjectPtr<URigVMController>> Controllers;

	UPROPERTY(EditAnywhere, Category = "User Interface")
	FRigGraphDisplaySettings RigGraphDisplaySettings;

	UPROPERTY(EditAnywhere, Category = "VM")
	FRigVMRuntimeSettings VMRuntimeSettings;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VM", meta = (AllowPrivateAccess = "true"))
	FRigVMCompileSettings VMCompileSettings;

	UPROPERTY(EditAnywhere, Category = "Python Log Settings")
	FControlRigPythonSettings PythonLogSettings;

	UPROPERTY(transient, DuplicateTransient)
	TMap<FString, FRigVMOperand> PinToOperandMap;

	UPROPERTY(transient, DuplicateTransient)
	bool bVMRecompilationRequired = false;

	UPROPERTY(transient, DuplicateTransient)
	bool bIsCompiling = false;
	
	FCompilerResultsLog CompileLog;

	FOnVMCompiledEvent VMCompiledEvent;
	FRigVMGraphModifiedEvent ModifiedEvent;

	bool bAutoRecompileVM = true;
	bool bErrorsDuringCompilation = false;
	bool bSuspendModelNotificationsForSelf = false;
	bool bSuspendModelNotificationsForOthers = false;
	bool bSuspendAllNotifications = false;
	bool bCompileInDebugMode = false;
};