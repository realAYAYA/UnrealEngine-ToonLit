// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigVMBlueprintGeneratedClass.h"
#include "Engine/Blueprint.h"
#include "RigVMCore/RigVM.h"
#include "RigVMHost.h"
#include "RigVMModel/RigVMClient.h"
#include "RigVMCompiler/RigVMCompiler.h"
#include "RigVMCore/RigVMGraphFunctionDefinition.h"
#include "EdGraph/RigVMEdGraph.h"
#include "EdGraph/RigVMEdGraphSchema.h"
#include "RigVMSettings.h"
#if WITH_EDITOR
#include "HAL/CriticalSection.h"
#endif

#include "RigVMBlueprint.generated.h"

class URigVMBlueprintGeneratedClass;

#if WITH_EDITOR
class IRigVMEditorModule;
#endif
struct FEndLoadPackageContext;
struct FRigVMMemoryStorageStruct;

DECLARE_EVENT_ThreeParams(URigVMBlueprint, FOnRigVMCompiledEvent, UObject*, URigVM*, FRigVMExtendedExecuteContext&);
DECLARE_EVENT_OneParam(URigVMBlueprint, FOnRigVMRefreshEditorEvent, URigVMBlueprint*);
DECLARE_EVENT_FourParams(URigVMBlueprint, FOnRigVMVariableDroppedEvent, UObject*, FProperty*, const FVector2D&, const FVector2D&);
DECLARE_EVENT_OneParam(URigVMBlueprint, FOnRigVMExternalVariablesChanged, const TArray<FRigVMExternalVariable>&);
DECLARE_EVENT_TwoParams(URigVMBlueprint, FOnRigVMNodeDoubleClicked, URigVMBlueprint*, URigVMNode*);
DECLARE_EVENT_OneParam(URigVMBlueprint, FOnRigVMGraphImported, UEdGraph*);
DECLARE_EVENT_OneParam(URigVMBlueprint, FOnRigVMPostEditChangeChainProperty, FPropertyChangedChainEvent&);
DECLARE_EVENT_ThreeParams(URigVMBlueprint, FOnRigVMLocalizeFunctionDialogRequested, FRigVMGraphFunctionIdentifier&, URigVMBlueprint*, bool);
DECLARE_EVENT_ThreeParams(URigVMBlueprint, FOnRigVMReportCompilerMessage, EMessageSeverity::Type, UObject*, const FString&);
DECLARE_DELEGATE_RetVal_FourParams(FRigVMController_BulkEditResult, FRigVMOnBulkEditDialogRequestedDelegate, URigVMBlueprint*, URigVMController*, URigVMLibraryNode*, ERigVMControllerBulkEditType);
DECLARE_DELEGATE_RetVal_OneParam(bool, FRigVMOnBreakLinksDialogRequestedDelegate, TArray<URigVMLink*>);
DECLARE_DELEGATE_RetVal_OneParam(TRigVMTypeIndex, FRigVMOnPinTypeSelectionRequestedDelegate, const TArray<TRigVMTypeIndex>&);
DECLARE_EVENT(URigVMBlueprint, FOnRigVMBreakpointAdded);
DECLARE_EVENT_OneParam(URigVMBlueprint, FOnRigVMRequestInspectObject, const TArray<UObject*>& );
DECLARE_EVENT_OneParam(URigVMBlueprint, FOnRigVMRequestInspectMemoryStorage, const TArray<FRigVMMemoryStorageStruct*>&);
//DECLARE_DELEGATE_RetVal(URigVMGraph*, FRigVMBlueprintGetFocusedGraph);

USTRUCT()
struct RIGVMDEVELOPER_API FRigVMPythonSettings
{
	GENERATED_BODY();

	FRigVMPythonSettings()
	{
	}
};

USTRUCT()
struct RIGVMDEVELOPER_API FRigVMEdGraphDisplaySettings
{
	GENERATED_BODY();

	FRigVMEdGraphDisplaySettings()
		: bShowNodeInstructionIndex(false)
		, bShowNodeRunCounts(false)
		, NodeRunLowerBound(1)
		, NodeRunLimit(256)
		, MinMicroSeconds(0.0)
		, MaxMicroSeconds(1.0)
		, TotalMicroSeconds(0.0)
		, AverageFrames(64)
		, bAutoDetermineRange(true)
		, LastMinMicroSeconds(0.0)
		, LastMaxMicroSeconds(1.0)
		, MinDurationColor(FLinearColor::Green)
		, MaxDurationColor(FLinearColor::Red)
	{
	}

	// When enabled shows the first node instruction index
	// matching the execution stack window.
	UPROPERTY(EditAnywhere, Category = "Graph Display Settings")
	bool bShowNodeInstructionIndex;

	// When enabled shows the node counts both in the graph view as
	// we as in the execution stack window.
	// The number on each node represents how often the node has been run.
	// Keep in mind when looking at nodes in a function the count
	// represents the sum of all counts for each node based on all
	// references of the function currently running.
	UPROPERTY(EditAnywhere, Category = "Graph Display Settings")
	bool bShowNodeRunCounts;

	// A lower limit for counts for nodes used for debugging.
	// Any node lower than this count won't show the run count.
	UPROPERTY(EditAnywhere, Category = "Graph Display Settings")
	int32 NodeRunLowerBound;

	// A upper limit for counts for nodes used for debugging.
	// If a node reaches this count a warning will be issued for the
	// node and displayed both in the execution stack as well as in the
	// graph. Setting this to <= 1 disables the warning.
	// Note: The count limit doesn't apply to functions / collapse nodes.
	UPROPERTY(EditAnywhere, Category = "Graph Display Settings")
	int32 NodeRunLimit;

	// The duration in microseconds of the fastest instruction / node
	UPROPERTY(EditAnywhere, Category = "Graph Display Settings", transient, meta = (EditCondition = "!bAutoDetermineRange"))
	double MinMicroSeconds;

	// The duration in microseconds of the slowest instruction / node
	UPROPERTY(EditAnywhere, Category = "Graph Display Settings", transient, meta = (EditCondition = "!bAutoDetermineRange"))
	double MaxMicroSeconds;

	// The total duration of the last execution of the rig
	UPROPERTY(VisibleAnywhere, Category = "Graph Display Settings", transient)
	double TotalMicroSeconds;

	// If you set this to more than 1 the results will be averaged across multiple frames
	UPROPERTY(EditAnywhere, Category = "Graph Display Settings", meta = (UIMin=1, UIMax=256))
	int32 AverageFrames;

	TArray<double> MinMicroSecondsFrames;
	TArray<double> MaxMicroSecondsFrames;
	TArray<double> TotalMicroSecondsFrames;

	UPROPERTY(EditAnywhere, Category = "Graph Display Settings")
	bool bAutoDetermineRange;

	UPROPERTY(transient)
	double LastMinMicroSeconds;

	UPROPERTY(transient)
	double LastMaxMicroSeconds;

	// The color of the fastest instruction / node
	UPROPERTY(EditAnywhere, Category = "Graph Display Settings")
	FLinearColor MinDurationColor;

	// The color of the slowest instruction / node
	UPROPERTY(EditAnywhere, Category = "Graph Display Settings")
	FLinearColor MaxDurationColor;

	void SetTotalMicroSeconds(double InTotalMicroSeconds);
	void SetLastMinMicroSeconds(double InMinMicroSeconds);
	void SetLastMaxMicroSeconds(double InMaxMicroSeconds);
	double AggregateAverage(TArray<double>& InFrames, double InPrevious, double InNext) const;
};

enum class UE_DEPRECATED(5.4, "Pease, use ERigVMLoadType") ERigVMBlueprintLoadType : uint8
{
	PostLoad,
	CheckUserDefinedStructs
};

USTRUCT(meta = (Deprecated = "5.2"))
struct RIGVMDEVELOPER_API FRigVMOldPublicFunctionArg
{
	GENERATED_BODY();
	
	FRigVMOldPublicFunctionArg()
	: Name(NAME_None)
	, CPPType(NAME_None)
	, CPPTypeObjectPath(NAME_None)
	, bIsArray(false)
	, Direction(ERigVMPinDirection::Input)
	{}

	UPROPERTY()
	FName Name;

	UPROPERTY()
	FName CPPType;

	UPROPERTY()
	FName CPPTypeObjectPath;

	UPROPERTY()
	bool bIsArray;

	UPROPERTY()
	ERigVMPinDirection Direction;

	FEdGraphPinType GetPinType() const;
};

USTRUCT(meta = (Deprecated = "5.2"))
struct RIGVMDEVELOPER_API FRigVMOldPublicFunctionData
{
	GENERATED_BODY();

	FRigVMOldPublicFunctionData()
		:Name(NAME_None)
	{}

	UPROPERTY()
	FName Name;

	UPROPERTY()
	FString DisplayName;

	UPROPERTY()
	FString Category;

	UPROPERTY()
	FString Keywords;

	UPROPERTY()
	FRigVMOldPublicFunctionArg ReturnValue;

	UPROPERTY()
	TArray<FRigVMOldPublicFunctionArg> Arguments;

	bool IsMutable() const;
};


UCLASS(BlueprintType, meta=(IgnoreClassThumbnail))
class RIGVMDEVELOPER_API URigVMBlueprint : public UBlueprint, public IRigVMClientHost
{
	GENERATED_UCLASS_BODY()

public:
	URigVMBlueprint();

	void CommonInitialization(const FObjectInitializer& ObjectInitializer);
	
	void InitializeModelIfRequired(bool bRecompileVM = true);

	/** Get the (full) generated class for this rigvm blueprint */
	URigVMBlueprintGeneratedClass* GetRigVMBlueprintGeneratedClass() const;

	/** Get the (skeleton) generated class for this rigvm blueprint */
	URigVMBlueprintGeneratedClass* GetRigVMBlueprintSkeletonClass() const;

	/** Returns the class used as the super class for all generated classes */
	virtual UClass* GetRigVMBlueprintGeneratedClassPrototype() const { return URigVMBlueprintGeneratedClass::StaticClass(); }

	/** Returns the expected schema class to use for this blueprint */
	virtual UClass* GetRigVMSchemaClass() const { return URigVMSchema::StaticClass(); }

	/** Returns the expected execute context struct to use for this blueprint */
	virtual UScriptStruct* GetRigVMExecuteContextStruct() const { return FRigVMExecuteContext::StaticStruct(); }

	/** Returns the expected ed graph class to use for this blueprint */
	virtual UClass* GetRigVMEdGraphClass() const { return URigVMEdGraph::StaticClass(); }

	/** Returns the expected ed graph node class to use for this blueprint */
	virtual UClass* GetRigVMEdGraphNodeClass() const { return URigVMEdGraphNode::StaticClass(); }

	/** Returns the expected ed graph schema class to use for this blueprint */
	virtual UClass* GetRigVMEdGraphSchemaClass() const { return URigVMEdGraphSchema::StaticClass(); }

	/** Returns the class of the settings to use */
	virtual UClass* GetRigVMEditorSettingsClass() const { return URigVMEditorSettings::StaticClass(); }

	/** Returns the settings defaults for this blueprint */
	URigVMEditorSettings* GetRigVMEditorSettings() const;

#if WITH_EDITOR
	/** Returns true if a given panel node factory is compatible this blueprint */
	virtual const FName& GetPanelNodeFactoryName() const;

	/** Returns true if a given panel pin factory is compatible this blueprint */
	virtual const FName& GetPanelPinFactoryName() const;

	static const FName RigVMPanelNodeFactoryName;
	static const FName RigVMPanelPinFactoryName;

	/** Returns the editor module to be used for this blueprint */
	virtual IRigVMEditorModule* GetEditorModule() const;
#endif

	virtual void Serialize(FArchive& Ar) override;

#if WITH_EDITOR

	// UBlueprint interface
	virtual UClass* GetBlueprintClass() const override;
	virtual UClass* RegenerateClass(UClass* ClassToRegenerate, UObject* PreviousCDO) override;
	virtual bool SupportedByDefaultBlueprintFactory() const override { return false; }
	virtual bool IsValidForBytecodeOnlyRecompile() const override { return false; }
	virtual void LoadModulesRequiredForCompilation() override;
	virtual void GetTypeActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual void GetInstanceActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual void SetObjectBeingDebugged(UObject* NewObject) override;
	PRAGMA_DISABLE_DEPRECATION_WARNINGS // Suppress compiler warning on override of deprecated function
	UE_DEPRECATED(5.0, "Use version that takes FObjectPreSaveContext instead.")
	virtual void PreSave(const class ITargetPlatform* TargetPlatform) override;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	virtual void PreSave(FObjectPreSaveContext ObjectSaveContext) override;
	virtual void PostSaveRoot(FObjectPostSaveRootContext ObjectSaveContext) override;
	virtual void PostLoad() override;
#if WITH_EDITORONLY_DATA
	static void DeclareConstructClasses(TArray<FTopLevelAssetPath>& OutConstructClasses, const UClass* SpecificSubclass);
#endif
	virtual bool IsPostLoadThreadSafe() const override { return false; }
	virtual void PostTransacted(const FTransactionObjectEvent& TransactionEvent) override;
	virtual void ReplaceDeprecatedNodes() override;
	virtual void PreDuplicate(FObjectDuplicationParameters& DupParams) override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	virtual void GetAssetRegistryTags(FAssetRegistryTagsContext Context) const override;
	UE_DEPRECATED(5.4, "Implement the version that takes FAssetRegistryTagsContext instead.")
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;

	virtual bool SupportsGlobalVariables() const override { return true; }
	virtual bool SupportsLocalVariables() const override { return true; }
	virtual bool SupportsFunctions() const override { return true; }
	virtual bool SupportsMacros() const override { return false; }
	virtual bool SupportsDelegates() const override { return false; }
	virtual bool SupportsEventGraphs() const override { return true; }
	virtual bool SupportsAnimLayers() const override { return false; }
	virtual bool ExportGraphToText(UEdGraph* InEdGraph, FString& OutText) override;
	virtual bool TryImportGraphFromText(const FString& InClipboardText, UEdGraph** OutGraphPtr = nullptr) override;
	virtual bool CanImportGraphFromText(const FString& InClipboardText) override;
	virtual bool RequiresForceLoadMembers(UObject* InObject) const override;

	// UObject interface
	virtual void PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent) override;
	virtual void PostRename(UObject* OldOuter, const FName OldName) override;
	/** Called during cooking. Must return all objects that will be Preload()ed when this is serialized at load time. */
	virtual void GetPreloadDependencies(TArray<UObject*>& OutDeps) override;

	//  --- IRigVMClientHost interface Start---
	virtual FRigVMClient* GetRigVMClient() override;
	virtual const FRigVMClient* GetRigVMClient() const override;
	virtual IRigVMGraphFunctionHost* GetRigVMGraphFunctionHost() override;
	virtual const IRigVMGraphFunctionHost* GetRigVMGraphFunctionHost() const override;
	virtual UObject* GetEditorObjectForRigVMGraph(URigVMGraph* InVMGraph) const override;
	virtual URigVMGraph* GetRigVMGraphForEditorObject(UObject* InObject) const override;
	virtual void HandleRigVMGraphAdded(const FRigVMClient* InClient, const FString& InNodePath) override;
	virtual void HandleRigVMGraphRemoved(const FRigVMClient* InClient, const FString& InNodePath) override;
	virtual void HandleRigVMGraphRenamed(const FRigVMClient* InClient, const FString& InOldNodePath, const FString& InNewNodePath) override;
	virtual void HandleConfigureRigVMController(const FRigVMClient* InClient, URigVMController* InControllerToConfigure) override;
	virtual UObject* ResolveUserDefinedTypeById(const FString& InTypeName) const override;

	UFUNCTION(BlueprintCallable, Category = "RigVM Blueprint")
	virtual void RecompileVM() override;
	UFUNCTION(BlueprintCallable, Category = "RigVM Blueprint")
	virtual void RecompileVMIfRequired() override;
	UFUNCTION(BlueprintCallable, Category = "RigVM Blueprint")
	virtual void RequestAutoVMRecompilation() override;
	UFUNCTION(BlueprintCallable, Category = "RigVM Blueprint")
	virtual void SetAutoVMRecompile(bool bAutoRecompile) override { bAutoRecompileVM = bAutoRecompile; }
	UFUNCTION(BlueprintPure, Category = "RigVM Blueprint")
	virtual bool GetAutoVMRecompile() const override { return bAutoRecompileVM; }

	virtual void IncrementVMRecompileBracket() override;
	virtual void DecrementVMRecompileBracket() override;

	// this is needed since even after load
	// model data can change while the RigVM BP is not opened
	// for example, if a user defined struct changed after BP load,
	// any pin that references the struct needs to be regenerated
	virtual void RefreshAllModels(ERigVMLoadType InLoadType = ERigVMLoadType::PostLoad) override;

	// RigVMRegistry changes can be triggered when new user defined types(structs/enums) are added/removed
	// in which case we have to refresh the model
	virtual void OnRigVMRegistryChanged() override;

	UFUNCTION(BlueprintCallable, Category = "RigVM Blueprint")
	virtual void RequestRigVMInit() override;

	UFUNCTION(BlueprintCallable, Category = "RigVM Blueprint")
	virtual URigVMGraph* GetModel(const UEdGraph* InEdGraph = nullptr) const override;
	virtual URigVMGraph* GetModel(const FString& InNodePath) const override;

	UFUNCTION(BlueprintCallable, Category = "RigVM Blueprint")
	virtual URigVMGraph* GetDefaultModel() const override;

	UFUNCTION(BlueprintCallable, Category = "RigVM Blueprint")
	virtual TArray<URigVMGraph*> GetAllModels() const override;

	UFUNCTION(BlueprintCallable, Category = "RigVM Blueprint")
	virtual URigVMFunctionLibrary* GetLocalFunctionLibrary() const override;

	UFUNCTION(BlueprintCallable, Category = "RigVM Blueprint")
	virtual URigVMGraph* AddModel(FString InName = TEXT("Rig Graph"), bool bSetupUndoRedo = true, bool bPrintPythonCommand = true) override;

	UFUNCTION(BlueprintCallable, Category = "RigVM Blueprint")
	virtual bool RemoveModel(FString InName = TEXT("Rig Graph"), bool bSetupUndoRedo = true, bool bPrintPythonCommand = true) override;

	virtual FRigVMGetFocusedGraph& OnGetFocusedGraph() override;
	virtual const FRigVMGetFocusedGraph& OnGetFocusedGraph() const override;

	UFUNCTION(BlueprintCallable, Category = "RigVM Blueprint")
	virtual URigVMGraph* GetFocusedModel() const override;

	UFUNCTION(BlueprintCallable, Category = "RigVM Blueprint")
	virtual URigVMController* GetController(const URigVMGraph* InGraph = nullptr) const override;

	UFUNCTION(BlueprintCallable, Category = "RigVM Blueprint")
	virtual URigVMController* GetControllerByName(const FString InGraphName = TEXT("")) const override;

	UFUNCTION(BlueprintCallable, Category = "RigVM Blueprint")
	virtual URigVMController* GetOrCreateController(URigVMGraph* InGraph = nullptr) override;

	virtual URigVMController* GetController(const UEdGraph* InEdGraph) const override;
	virtual URigVMController* GetOrCreateController(const UEdGraph* InGraph) override;

	UFUNCTION(BlueprintCallable, Category = "RigVM Blueprint")
	virtual TArray<FString> GeneratePythonCommands(const FString InNewBlueprintName) override;

	//  --- IRigVMClientHost interface End ---


	FOnRigVMRequestInspectObject& OnRequestInspectObject() { return OnRequestInspectObjectEvent; }
	void RequestInspectObject(const TArray<UObject*>& InObjects) { OnRequestInspectObjectEvent.Broadcast(InObjects); }

	FOnRigVMRequestInspectMemoryStorage& OnRequestInspectMemoryStorage() { return OnRequestInspectMemoryStorageEvent; }
	void RequestInspectMemoryStorage(const TArray<FRigVMMemoryStorageStruct*>& InMemoryStorageStructs) { OnRequestInspectMemoryStorageEvent.Broadcast(InMemoryStorageStructs); }

#endif	// #if WITH_EDITOR

	

	virtual bool ShouldBeMarkedDirtyUponTransaction() const override { return false; }

	URigVMGraph* GetTemplateModel(bool bIsFunctionLibrary = false);
	URigVMController* GetTemplateController(bool bIsFunctionLibrary = false);

#if WITH_EDITOR
	UEdGraph* GetEdGraph(URigVMGraph* InModel) const;
	UEdGraph* GetEdGraph(const FString& InNodePath) const;
#endif

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<URigVMEdGraph> FunctionLibraryEdGraph;
#endif

	bool IsFunctionPublic(const FName& InFunctionName) const;
	void MarkFunctionPublic(const FName& InFunctionName, bool bIsPublic = true);

	// Returns a list of dependencies of this blueprint.
	// Dependencies are blueprints that contain functions used in this blueprint
	TArray<URigVMBlueprint*> GetDependencies(bool bRecursive = false) const;

	// Returns a list of dependents as unresolved soft object pointers.
	// A dependent is a blueprint which uses a function defined in this blueprint.
	// This function is not recursive, since it avoids opening the asset.
	// Use GetDependentBlueprints as an alternative.
	TArray<FAssetData> GetDependentAssets() const;

	// Returns a list of dependents as resolved blueprints.
	// A dependent is a blueprint which uses a function defined in this blueprint.
	// If bOnlyLoaded is false, this function loads the dependent assets and can introduce a large cost
	// depending on the size / count of assets in the project.
	TArray<URigVMBlueprint*> GetDependentBlueprints(bool bRecursive = false, bool bOnlyLoaded = false) const;

	UPROPERTY(EditAnywhere, Category = "User Interface")
	FRigVMEdGraphDisplaySettings RigGraphDisplaySettings;

	UPROPERTY(EditAnywhere, Category = "VM")
	FRigVMRuntimeSettings VMRuntimeSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VM")
	FRigVMCompileSettings VMCompileSettings;

	UPROPERTY(EditAnywhere, Category = "Python Log Settings")
	FRigVMPythonSettings PythonLogSettings;

	UPROPERTY()
	TMap<FString, FSoftObjectPath> UserDefinedStructGuidToPathName;

	UPROPERTY(transient)
	TSet<TObjectPtr<UObject>> UserDefinedTypesInUse;

protected:

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<URigVMGraph> Model_DEPRECATED;

	UPROPERTY()
	TObjectPtr<URigVMFunctionLibrary> FunctionLibrary_DEPRECATED;
#endif

	UPROPERTY()
	FRigVMClient RigVMClient;

#if WITH_EDITORONLY_DATA

	UPROPERTY()
	bool ReferencedObjectPathsStored;

	UPROPERTY()
	TArray<FSoftObjectPath> ReferencedObjectPaths;

#endif

	/** Asset searchable information about exposed public functions on this rig */
	UPROPERTY(AssetRegistrySearchable)
	TArray<FRigVMGraphFunctionHeader> PublicGraphFunctions;

	/** Asset searchable information function references in this rig */
	UPROPERTY(AssetRegistrySearchable)
	TArray<FRigVMReferenceNodeData> FunctionReferenceNodeData;

#if WITH_EDITORONLY_DATA

	UPROPERTY(transient, DuplicateTransient)
	TObjectPtr<URigVMGraph> TemplateModel;

	UPROPERTY(transient, DuplicateTransient)
	TObjectPtr<URigVMController> TemplateController;

	mutable TArray<FAssetRegistryTag> CachedAssetTags;

#endif

public:

	UPROPERTY(transient, DuplicateTransient)
	TMap<FString, FRigVMOperand> PinToOperandMap;

	bool bSuspendModelNotificationsForSelf;
	bool bSuspendModelNotificationsForOthers;
	bool bSuspendAllNotifications;

	virtual void SetupPinRedirectorsForBackwardsCompatibility() {};
	void RebuildGraphFromModel();

	FRigVMGraphModifiedEvent& OnModified();
	FOnRigVMCompiledEvent& OnVMCompiled();

	UFUNCTION(BlueprintCallable, Category = "VM")
	UClass* GetRigVMHostClass() const;

	UFUNCTION(BlueprintCallable, Category = "VM")
	URigVMHost* CreateRigVMHost();

	UFUNCTION(BlueprintCallable, Category = "VM")
	URigVMHost* GetDebuggedRigVMHost() { return Cast<URigVMHost>(GetObjectBeingDebugged()); }

	UFUNCTION(BlueprintCallable, Category = "VM")
	virtual TArray<UStruct*> GetAvailableRigVMStructs() const;

#if WITH_EDITOR
	UFUNCTION(BlueprintCallable, Category = "Variables")
	TArray<FRigVMGraphVariableDescription> GetMemberVariables() const;
	
	UFUNCTION(BlueprintCallable, Category = "Variables")
	FName AddMemberVariable(const FName& InName, const FString& InCPPType, bool bIsPublic = false, bool bIsReadOnly = false, FString InDefaultValue = TEXT(""));

	UFUNCTION(BlueprintCallable, Category = "Variables")
	bool RemoveMemberVariable(const FName& InName);

	UFUNCTION(BlueprintCallable, Category = "Variables")
	bool RenameMemberVariable(const FName& InOldName, const FName& InNewName);

	UFUNCTION(BlueprintCallable, Category = "Variables")
	bool ChangeMemberVariableType(const FName& InName, const FString& InCPPType, bool bIsPublic = false, bool bIsReadOnly = false, FString InDefaultValue = TEXT(""));
#endif

private:

	/** The event names this rigvm blueprint contains */
	UPROPERTY(AssetRegistrySearchable)
	TArray<FName> SupportedEventNames;

	UPROPERTY(transient, DuplicateTransient)
	bool bAutoRecompileVM;

	UPROPERTY(transient, DuplicateTransient)
	bool bVMRecompilationRequired;

	UPROPERTY(transient, DuplicateTransient)
	bool bIsCompiling;

	UPROPERTY(transient, DuplicateTransient)
	int32 VMRecompilationBracket;

	FRigVMGraphModifiedEvent ModifiedEvent;
	void Notify(ERigVMGraphNotifType InNotifType, UObject* InSubject);
	void HandleModifiedEvent(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject);

#if WITH_EDITOR

	UFUNCTION(BlueprintCallable, Category = "RigVM Blueprint")
	void SuspendNotifications(bool bSuspendNotifs);

	FOnRigVMRefreshEditorEvent RefreshEditorEvent;
	FOnRigVMVariableDroppedEvent VariableDroppedEvent;
	FOnRigVMBreakpointAdded BreakpointAddedEvent;

public:

	void BroadcastRefreshEditor() { return RefreshEditorEvent.Broadcast(this); }
	FOnRigVMRefreshEditorEvent& OnRefreshEditor() { return RefreshEditorEvent; }
	FOnRigVMVariableDroppedEvent& OnVariableDropped() { return VariableDroppedEvent; }
	FOnRigVMBreakpointAdded& OnBreakpointAdded() { return BreakpointAddedEvent; }

private:

#endif

	FOnRigVMCompiledEvent VMCompiledEvent;

	virtual void PatchFunctionReferencesOnLoad();
	virtual void PathDomainSpecificContentOnLoad() {}
	virtual void PatchBoundVariables();
	virtual void PatchVariableNodesWithIncorrectType();
	virtual void PatchParameterNodesOnLoad() {}
	virtual void PatchLinksWithCast();
	virtual void PatchFunctionsOnLoad();

protected:

#if WITH_EDITOR
	static FName FindHostMemberVariableUniqueName(TSharedPtr<FKismetNameValidator> InNameValidator, const FString& InBaseName);
	static int32 AddHostMemberVariable(URigVMBlueprint* InBlueprint, const FName& InVarName, FEdGraphPinType InVarType, bool bIsPublic, bool bIsReadOnly, FString InDefaultValue);
	FName AddHostMemberVariableFromExternal(FRigVMExternalVariable InVariableToCreate, FString InDefaultValue = FString());
#endif
	
	virtual void CreateMemberVariablesOnLoad();
	virtual void PatchVariableNodesOnLoad();
	TMap<FName, int32> AddedMemberVariableMap;
	TArray<FBPVariableDescription> LastNewVariables;

public:
	void PropagateRuntimeSettingsFromBPToInstances();
	void InitializeArchetypeInstances();

private:

#if WITH_EDITOR
	void HandlePackageDone(const FEndLoadPackageContext& Context);

protected:
	
	virtual void HandlePackageDone();

	/** Our currently running rig vm instance */
	UPROPERTY(transient)
	TObjectPtr<URigVMHost> EditorHost = nullptr;

private:
	
	// RigVMBP, once end-loaded, will inform other RigVM-Dependent systems that Host instances are ready.
	void BroadcastRigVMPackageDone();

	// Previously some memory classes were parented to the asset object
	// however it is no longer supported since classes are now identified 
	// with only package name + class name, see FTopLevelAssetPath
	// this function removes those deprecated class.
	// new classes should be created by RecompileVM and parented to the Package
	// during PostLoad
	void RemoveDeprecatedVMMemoryClass();

#if WITH_EDITORONLY_DATA
	// During load, we do not want the GC to destroy the generator classes until all URigVMMemoryStorage objects
	// are loaded, so we need to keep a pointer to the classes. These pointers will be removed on PreSave so that the
	// GC can do its work.
	UPROPERTY(Transient)
	TArray<TObjectPtr<URigVMMemoryStorageGeneratorClass>> OldMemoryStorageGeneratorClasses;

#endif

#endif
#if WITH_EDITOR

public:

	FOnRigVMExternalVariablesChanged& OnExternalVariablesChanged() { return ExternalVariablesChangedEvent; }

	virtual void OnPreVariableChange(UObject* InObject);
	virtual void OnPostVariableChange(UBlueprint* InBlueprint);
	virtual void OnVariableAdded(const FName& InVarName);
	virtual void OnVariableRemoved(const FName& InVarName);
	virtual void OnVariableRenamed(const FName& InOldVarName, const FName& InNewVarName);
	virtual void OnVariableTypeChanged(const FName& InVarName, FEdGraphPinType InOldPinType, FEdGraphPinType InNewPinType);

	FOnRigVMNodeDoubleClicked& OnNodeDoubleClicked() { return NodeDoubleClickedEvent; }
	void BroadcastNodeDoubleClicked(URigVMNode* InNode);

	FOnRigVMGraphImported& OnGraphImported() { return GraphImportedEvent; }
	void BroadcastGraphImported(UEdGraph* InGraph);

	FOnRigVMPostEditChangeChainProperty& OnPostEditChangeChainProperty() { return PostEditChangeChainPropertyEvent; }
	void BroadcastPostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedChainEvent);

	FOnRigVMLocalizeFunctionDialogRequested& OnRequestLocalizeFunctionDialog() { return RequestLocalizeFunctionDialog; }
	void BroadcastRequestLocalizeFunctionDialog(FRigVMGraphFunctionIdentifier InFunction, bool bForce = false);

	FRigVMOnBulkEditDialogRequestedDelegate& OnRequestBulkEditDialog() { return RequestBulkEditDialog; }

	FRigVMOnBreakLinksDialogRequestedDelegate& OnRequestBreakLinksDialog() { return RequestBreakLinksDialog; }

	FRigVMOnPinTypeSelectionRequestedDelegate& OnRequestPinTypeSelectionDialog() { return RequestPinTypeSelectionDialog; }

	FRigVMController_RequestJumpToHyperlinkDelegate& OnRequestJumpToHyperlink() { return RequestJumpToHyperlink; };

	FOnRigVMReportCompilerMessage& OnReportCompilerMessage() { return ReportCompilerMessageEvent; }
	void BroadCastReportCompilerMessage(EMessageSeverity::Type InSeverity, UObject* InSubject, const FString& InMessage);

	const FCompilerResultsLog& GetCompileLog() const { return CompileLog; }
	FCompilerResultsLog& GetCompileLog() { return CompileLog; }

private:

	FOnRigVMExternalVariablesChanged ExternalVariablesChangedEvent;
	bool bUpdatingExternalVariables;
	void BroadcastExternalVariablesChangedEvent();
	FCompilerResultsLog CompileLog;

	FOnRigVMNodeDoubleClicked NodeDoubleClickedEvent;
	FOnRigVMGraphImported GraphImportedEvent;
	FOnRigVMPostEditChangeChainProperty PostEditChangeChainPropertyEvent;
	FOnRigVMLocalizeFunctionDialogRequested RequestLocalizeFunctionDialog;
	FOnRigVMReportCompilerMessage ReportCompilerMessageEvent;
	FRigVMOnBulkEditDialogRequestedDelegate RequestBulkEditDialog;
	FRigVMOnBreakLinksDialogRequestedDelegate RequestBreakLinksDialog;
	FRigVMOnPinTypeSelectionRequestedDelegate RequestPinTypeSelectionDialog;
	FRigVMController_RequestJumpToHyperlinkDelegate RequestJumpToHyperlink;

#endif

	UEdGraph* CreateEdGraph(URigVMGraph* InModel, bool bForce = false);
	bool RemoveEdGraph(URigVMGraph* InModel);
	void DestroyObject(UObject* InObject);
	void RenameGraph(const FString& InNodePath, const FName& InNewName);
	void CreateEdGraphForCollapseNodeIfNeeded(URigVMCollapseNode* InNode, bool bForce = false);
	bool RemoveEdGraphForCollapseNode(URigVMCollapseNode* InNode, bool bNotify = false);
	void HandleReportFromCompiler(EMessageSeverity::Type InSeverity, UObject* InSubject, const FString& InMessage);

protected:
	
	TArray<IRigVMGraphFunctionHost*> GetReferencedFunctionHosts(bool bForceLoad);
	
#if WITH_EDITOR
private:
	bool bCompileInDebugMode;
	
	TArray<URigVMNode*> RigVMBreakpointNodes;

	FOnRigVMRequestInspectObject OnRequestInspectObjectEvent;
	FOnRigVMRequestInspectMemoryStorage OnRequestInspectMemoryStorageEvent;
		
public:

	/** Sets the execution mode. In Release mode the rig will ignore all breakpoints. */
	void SetDebugMode(const bool bValue);

	/** Returns the execution mode */
	bool IsInDebugMode() const { return bCompileInDebugMode; }

	/** Removes all the breakpoints from the blueprint and the VM */
	void ClearBreakpoints();

	/** Adds a breakpoint to all loaded blueprints which use the node indicated by InBreakpointNodePath 
	  * If the node is inside a public function, it will add a breakpoint to all blueprints calling this function. */
	bool AddBreakpoint(const FString& InBreakpointNodePath);

	/** Adds a breakpoint to all loaded blueprints which use the InBreakpointNode. 
	  * If LibraryNode is not null, it indicates that the library uses the InBreapointNode, and the function will add
	  * breakpoints to any other loaded blueprint that references this library. */
	bool AddBreakpoint(URigVMNode* InBreakpointNode, URigVMLibraryNode* LibraryNode = nullptr);

	/** Adds a breakpoint to the first instruction of each callpath related to the InBreakpointNode */
	bool AddBreakpointToHost(URigVMNode* InBreakpointNode);

	/** Removes the given breakpoint from all the loaded blueprints that use this node, and recomputes all breakpoints
	  * in the VM  */
	bool RemoveBreakpoint(const FString& InBreakpointNodePath);
	bool RemoveBreakpoint(URigVMNode* InBreakpointNode);

	/** Recomputes the instruction breakpoints given the node breakpoints in the blueprint */
	void RefreshBreakpoints();

	/** Shape libraries to load during package load completed */ 
	TArray<FString> ShapeLibrariesToLoadOnPackageLoaded;

	TArray<FRigVMReferenceNodeData> GetReferenceNodeData() const;

#endif

protected:

	static FSoftObjectPath PreDuplicateAssetPath;
	static FSoftObjectPath PreDuplicateHostPath;
	static TArray<URigVMBlueprint*> sCurrentlyOpenedRigVMBlueprints;

	void MarkDirtyDuringLoad() { bDirtyDuringLoad = true; }
	
	virtual void SetupDefaultObjectDuringCompilation(URigVMHost* InCDO);

public:
	
	bool IsMarkedDirtyDuringLoad() const { return bDirtyDuringLoad; }

private:
	bool bDirtyDuringLoad;
	bool bErrorsDuringCompilation;
	bool bSuspendPythonMessagesForRigVMClient;
	bool bMarkBlueprintAsStructurallyModifiedPending;

#if WITH_EDITOR

public:

	static void QueueCompilerMessageDelegate(const FOnRigVMReportCompilerMessage::FDelegate& InDelegate);
	static void ClearQueuedCompilerMessageDelegates();
	
private:

	static FCriticalSection QueuedCompilerMessageDelegatesMutex;
	static TArray<FOnRigVMReportCompilerMessage::FDelegate> QueuedCompilerMessageDelegates;

#endif

	friend class FRigVMBlueprintCompilerContext;
	friend class FRigVMEditor;
	friend class FRigVMEditorModule;
	friend class URigVMEdGraphSchema;
	friend struct FRigVMEdGraphSchemaAction_PromoteToVariable;
};

class RIGVMDEVELOPER_API FRigVMBlueprintCompileScope
{
public:
   
	FRigVMBlueprintCompileScope(URigVMBlueprint *InBlueprint)
	: Blueprint(InBlueprint)
	{
		check(Blueprint);
		Blueprint->IncrementVMRecompileBracket();
	}

	~FRigVMBlueprintCompileScope()
	{
		Blueprint->DecrementVMRecompileBracket();
	}
   
private:

	URigVMBlueprint* Blueprint;
};