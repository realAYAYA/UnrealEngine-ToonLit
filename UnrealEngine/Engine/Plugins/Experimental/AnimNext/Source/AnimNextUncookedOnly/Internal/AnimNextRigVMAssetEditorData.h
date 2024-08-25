// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigVMModel/RigVMGraph.h"
#include "RigVMCore/RigVMGraphFunctionHost.h"
#include "RigVMBlueprint.h"
#include "UncookedOnlyUtils.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AnimNextRigVMAssetEditorData.generated.h"

enum class ERigVMGraphNotifType : uint8;
class UAnimNextRigVMAssetEntry;
class UAnimNextRigVMAssetEditorData;
class UAnimNextParameterBlock_EdGraph;
class UAnimNextGraph_EdGraph;

namespace UE::AnimNext::UncookedOnly
{
	struct FUtils;
	struct FUtilsPrivate;
}

namespace UE::AnimNext::Editor
{
	struct FUtils;
	class FParameterBlockEditor;
	class SRigVMAssetView;
	class SParameterPicker;
	class FParameterBlockTabSummoner;
	class SRigVMAssetViewRow;
	class FParameterBlockParameterCustomization;
	class FModule;
	class FWorkspaceEditor;
}

namespace UE::AnimNext::Tests
{
	class FEditor_Graph;
}

namespace UE::AnimNext::UncookedOnly
{
	// A delegate for subscribing / reacting to editor data modifications.
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnEditorDataModified, UAnimNextRigVMAssetEditorData* /* InEditorData */);
}

// Script-callable editor API hoisted onto UAnimNextRigVMAsset
UCLASS()
class UAnimNextRigVMAssetLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	UFUNCTION(BlueprintCallable, Category = "AnimNext|RigVM Asset", meta=(ScriptMethod))
	static UAnimNextRigVMAssetEntry* FindEntry(UAnimNextRigVMAsset* InAsset, FName InName);

	UFUNCTION(BlueprintCallable, Category = "AnimNext|RigVM Asset", meta=(ScriptMethod))
	static bool RemoveEntry(UAnimNextRigVMAsset* InAsset, UAnimNextRigVMAssetEntry* InEntry, bool bSetupUndoRedo = true, bool bPrintPythonCommand = true);

	UFUNCTION(BlueprintCallable, Category = "AnimNext|RigVM Asset", meta=(ScriptMethod))
	static bool RemoveEntries(UAnimNextRigVMAsset* InAsset, const TArray<UAnimNextRigVMAssetEntry*>& InEntries, bool bSetupUndoRedo = true, bool bPrintPythonCommand = true);
};

/* Base class for all AnimNext editor data objects that use RigVM */
UCLASS(MinimalAPI, Abstract)
class UAnimNextRigVMAssetEditorData : public UObject, public IRigVMClientHost, public IRigVMGraphFunctionHost
{
	GENERATED_BODY()

protected:
	friend class UE::AnimNext::Editor::SRigVMAssetView;
	friend class UE::AnimNext::Editor::SRigVMAssetViewRow;
	friend struct UE::AnimNext::Editor::FUtils;
	friend struct UE::AnimNext::UncookedOnly::FUtils;
	friend class UE::AnimNext::Editor::FModule;
	friend class UE::AnimNext::Editor::FWorkspaceEditor;
	friend class UAnimNextRigVMAssetEntry;
	friend class UAnimNextRigVMAssetLibrary;
	friend class UAnimNextGraph_EdGraph;
	friend class UAnimNextParameterBlock_EdGraph;
	friend class UE::AnimNext::Tests::FEditor_Graph;

	// UObject interface
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
	virtual void PostTransacted(const FTransactionObjectEvent& TransactionEvent) override;
	virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;
	virtual void GetAssetRegistryTags(FAssetRegistryTagsContext Context) const override;
	virtual bool IsEditorOnly() const override { return true; }

	void HandlePackageDone(const FEndLoadPackageContext& Context);
	void HandlePackageDone();

	// IRigVMClientHost interface
	virtual FRigVMClient* GetRigVMClient() override;
	virtual const FRigVMClient* GetRigVMClient() const override;
	virtual IRigVMGraphFunctionHost* GetRigVMGraphFunctionHost() override;
	virtual const IRigVMGraphFunctionHost* GetRigVMGraphFunctionHost() const override;
	virtual void HandleRigVMGraphAdded(const FRigVMClient* InClient, const FString& InNodePath) override;
	virtual void HandleRigVMGraphRemoved(const FRigVMClient* InClient, const FString& InNodePath) override;
	virtual void HandleRigVMGraphRenamed(const FRigVMClient* InClient, const FString& InOldNodePath, const FString& InNewNodePath) override {}
	virtual void HandleConfigureRigVMController(const FRigVMClient* InClient, URigVMController* InControllerToConfigure) override;
	virtual UObject* GetEditorObjectForRigVMGraph(URigVMGraph* InVMGraph) const override;
	virtual URigVMGraph* GetRigVMGraphForEditorObject(UObject* InObject) const override;
	virtual void RecompileVM() override PURE_VIRTUAL(UAnimNextRigVMAssetEditorData::RecompileVM, )
	virtual void RecompileVMIfRequired() override;
	virtual void RequestAutoVMRecompilation() override;
	virtual void SetAutoVMRecompile(bool bAutoRecompile) override;
	virtual bool GetAutoVMRecompile() const override;
	virtual void IncrementVMRecompileBracket() override;
	virtual void DecrementVMRecompileBracket() override;
	virtual void RefreshAllModels(ERigVMLoadType InLoadType) override;
	virtual void OnRigVMRegistryChanged() override;
	virtual void RequestRigVMInit() override;
	virtual URigVMGraph* GetModel(const UEdGraph* InEdGraph = nullptr) const override;
	virtual URigVMGraph* GetModel(const FString& InNodePath) const override;
	virtual URigVMGraph* GetDefaultModel() const override;
	virtual TArray<URigVMGraph*> GetAllModels() const override;
	virtual URigVMFunctionLibrary* GetLocalFunctionLibrary() const override;
	virtual URigVMGraph* AddModel(FString InName = TEXT("Rig Graph"), bool bSetupUndoRedo = true, bool bPrintPythonCommand = true) override;
	virtual bool RemoveModel(FString InName = TEXT("Rig Graph"), bool bSetupUndoRedo = true, bool bPrintPythonCommand = true) override;
	virtual FRigVMGetFocusedGraph& OnGetFocusedGraph() override;
	virtual const FRigVMGetFocusedGraph& OnGetFocusedGraph() const override;
	virtual URigVMGraph* GetFocusedModel() const override;
	virtual URigVMController* GetController(const URigVMGraph* InGraph = nullptr) const override;
	virtual URigVMController* GetControllerByName(const FString InGraphName = TEXT("")) const override;
	virtual URigVMController* GetOrCreateController(URigVMGraph* InGraph = nullptr) override;
	virtual URigVMController* GetController(const UEdGraph* InEdGraph) const override;
	virtual URigVMController* GetOrCreateController(const UEdGraph* InGraph) override;
	virtual TArray<FString> GeneratePythonCommands(const FString InNewBlueprintName) override;

	// IRigVMGraphFunctionHost interface
	virtual FRigVMGraphFunctionStore* GetRigVMGraphFunctionStore() override;
	virtual const FRigVMGraphFunctionStore* GetRigVMGraphFunctionStore() const override;

	// Override called during initialization to determine what RigVM controller class is used
	virtual TSubclassOf<URigVMController> GetControllerClass() const { return URigVMController::StaticClass(); }

	// Override called during initialization to determine what RigVM schema class is used
	virtual TSubclassOf<URigVMSchema> GetRigVMSchemaClass() const PURE_VIRTUAL(UAnimNextRigVMAssetEditorData::GetRigVMSchemaClass, return nullptr;)

	// Override called during initialization to determine what RigVM execute struct is used
	virtual UScriptStruct* GetExecuteContextStruct() const PURE_VIRTUAL(UAnimNextRigVMAssetEditorData::GetExecuteContextStruct, return nullptr;)

	// Create and store a UEdGraph that corresponds to a URigVMGraph
	virtual UEdGraph* CreateEdGraph(URigVMGraph* InRigVMGraph, bool bForce) PURE_VIRTUAL(UAnimNextRigVMAssetEditorData::CreateEdGraph, return nullptr;)

	// Create and store a UEdGraph that corresponds to a URigVMCollapseNode
	virtual void CreateEdGraphForCollapseNode(URigVMCollapseNode* InNode) PURE_VIRTUAL(UAnimNextRigVMAssetEditorData::CreateEdGraphForCollapseNode, )

	// Remove the UEdGraph that corresponds to a URigVMGraph
	virtual bool RemoveEdGraph(URigVMGraph* InModel) PURE_VIRTUAL(UAnimNextRigVMAssetEditorData::RemoveEdGraph, return false;)

	// Initialize the asset for use
	virtual void Initialize(bool bRecompileVM);

	// Handle RigVM modification events
	virtual void HandleModifiedEvent(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject);

	// Get all the kinds of entry for this asset
	virtual TConstArrayView<TSubclassOf<UAnimNextRigVMAssetEntry>> GetEntryClasses() const PURE_VIRTUAL(UAnimNextRigVMAssetEditorData::GetEntryClasses, return {};)

	// Helper for creating new sub-entries. Sets package flags and outers appropriately 
	static UObject* CreateNewSubEntry(UAnimNextRigVMAssetEditorData* InEditorData, TSubclassOf<UObject> InClass);

	// Helper for creating new sub-entries. Sets package flags and outers appropriately
	template<typename EntryClassType>
	static EntryClassType* CreateNewSubEntry(UAnimNextRigVMAssetEditorData* InEditorData)
	{
		return CastChecked<EntryClassType>(CreateNewSubEntry(InEditorData, EntryClassType::StaticClass()));
	}

	// Get all the entries for this asset
	TConstArrayView<TObjectPtr<UAnimNextRigVMAssetEntry>> GetAllEntries() const { return Entries; } 

	// Access all the UEdGraphs in this asset
	TArray<UEdGraph*> GetAllEdGraphs() const;

	// Iterate over all entries of the specified type
	// If predicate returns false, iteration is stopped
	template<typename EntryType, typename PredicateType>
	void ForEachEntryOfType(PredicateType InPredicate) const
	{
		for(UAnimNextRigVMAssetEntry* Entry : Entries)
		{
			if(EntryType* TypedEntry = Cast<EntryType>(Entry))
			{
				if(!InPredicate(TypedEntry))
				{
					return;
				}
			}
		}
	}

	// Returns all nodes in all graphs of the specified class
	template<class T>
	void GetAllNodesOfClass(TArray<T*>& OutNodes)
	{
		ForEachEntryOfType<IAnimNextRigVMGraphInterface>([&OutNodes](IAnimNextRigVMGraphInterface* InGraphInterface)
		{
			TArray<T*> GraphNodes;
			InGraphInterface->GetEdGraph()->GetNodesOfClass<T>(GraphNodes);
			OutNodes.Append(GraphNodes);
			return true;
		});
	}
	
	// Find an entry by name
	ANIMNEXTUNCOOKEDONLY_API UAnimNextRigVMAssetEntry* FindEntry(FName InName) const;

	// Remove an entry from the asset
	// @return true if the item was removed
	ANIMNEXTUNCOOKEDONLY_API bool RemoveEntry(UAnimNextRigVMAssetEntry* InEntry, bool bSetupUndoRedo = true, bool bPrintPythonCommand = true);

	// Remove a number of entries from the asset
	// @return true if any items were removed
	ANIMNEXTUNCOOKEDONLY_API bool RemoveEntries(const TArray<UAnimNextRigVMAssetEntry*>& InEntries, bool bSetupUndoRedo = true, bool bPrintPythonCommand = true);

	ANIMNEXTUNCOOKEDONLY_API void BroadcastModified();

	void ReportError(const TCHAR* InMessage) const;

	void ReconstructAllNodes();

	// Called from PostLoad to load external packages
	void PostLoadExternalPackages();

	// Find an entry that corresponds to the specified RigVMGraph. This uses the name of the graph to match the entry 
	UAnimNextRigVMAssetEntry* FindEntryForRigVMGraph(URigVMGraph* InRigVMGraph) const;

	/** All entries in this asset - not saved, either serialized or discovered at load time */
	UPROPERTY(transient)
	TArray<TObjectPtr<UAnimNextRigVMAssetEntry>> Entries;

	UPROPERTY()
	FRigVMClient RigVMClient;

	UPROPERTY()
	FRigVMGraphFunctionStore GraphFunctionStore;

	UPROPERTY(EditAnywhere, Category = "User Interface")
	FRigVMEdGraphDisplaySettings RigGraphDisplaySettings;

	UPROPERTY(EditAnywhere, Category = "VM")
	FRigVMRuntimeSettings VMRuntimeSettings;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VM", meta = (AllowPrivateAccess = "true"))
	FRigVMCompileSettings VMCompileSettings;

	UPROPERTY(transient, DuplicateTransient)
	TMap<FString, FRigVMOperand> PinToOperandMap;

	UPROPERTY()
	TArray<FEditedDocumentInfo> LastEditedDocuments;

	UPROPERTY(transient, DuplicateTransient)
	int32 VMRecompilationBracket = 0;

	UPROPERTY(transient, DuplicateTransient)
	bool bVMRecompilationRequired = false;

	UPROPERTY(transient, DuplicateTransient)
	bool bIsCompiling = false;

	FCompilerResultsLog CompileLog;

	FOnRigVMCompiledEvent RigVMCompiledEvent;

	FRigVMGraphModifiedEvent RigVMGraphModifiedEvent;

	// Delegate to subscribe to modifications to this editor data
	UE::AnimNext::UncookedOnly::FOnEditorDataModified ModifiedDelegate;

	// Cached exports, generated on compilation
	FAnimNextParameterProviderAssetRegistryExports CachedExports;
	
	bool bAutoRecompileVM = true;
	bool bErrorsDuringCompilation = false;
	bool bSuspendModelNotificationsForSelf = false;
	bool bSuspendModelNotificationsForOthers = false;
	bool bSuspendAllNotifications = false;
	bool bCompileInDebugMode = false;
	bool bSuspendPythonMessagesForRigVMClient = true;
	bool bSuspendEditorDataNotifications = false;
};
