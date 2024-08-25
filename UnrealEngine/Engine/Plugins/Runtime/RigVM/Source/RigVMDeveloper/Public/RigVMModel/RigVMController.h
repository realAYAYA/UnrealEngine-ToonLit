// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMGraph.h"
#include "RigVMFunctionLibrary.h"
#include "RigVMSchema.h"
#include "RigVMModel/Nodes/RigVMUnitNode.h"
#include "RigVMModel/Nodes/RigVMVariableNode.h"
#include "RigVMModel/Nodes/RigVMParameterNode.h"
#include "RigVMModel/Nodes/RigVMCommentNode.h"
#include "RigVMModel/Nodes/RigVMRerouteNode.h"
#include "RigVMModel/Nodes/RigVMIfNode.h"
#include "RigVMModel/Nodes/RigVMSelectNode.h"
#include "RigVMModel/Nodes/RigVMTemplateNode.h"
#include "RigVMModel/Nodes/RigVMEnumNode.h"
#include "RigVMModel/Nodes/RigVMCollapseNode.h"
#include "RigVMModel/Nodes/RigVMFunctionReferenceNode.h"
#include "RigVMModel/Nodes/RigVMInvokeEntryNode.h"
#include "RigVMModel/RigVMBuildData.h"
#include "RigVMCore/RigVMUserWorkflow.h"
#include "UObject/Interface.h"
#include "Algo/Sort.h"
#include "RigVMController.generated.h"

class URigVMActionStack;
struct FRigVMGraphFunctionArgument;
struct FRigVMGraphFunctionHeader;

UENUM()
enum class ERigVMControllerBulkEditType : uint8
{
	AddExposedPin,
    RemoveExposedPin,
    RenameExposedPin,
    ChangeExposedPinType,
    AddVariable,
    RemoveVariable,
	RenameVariable,
    ChangeVariableType,
	RemoveFunction,
    Max UMETA(Hidden),
};

UENUM()
enum class ERigVMControllerBulkEditProgress : uint8
{
	BeginLoad,
    FinishedLoad,
	BeginEdit,
    FinishedEdit,
    Max UMETA(Hidden),
};

struct FRigVMController_BulkEditResult
{
	bool bCanceled;
	bool bSetupUndoRedo;

	FRigVMController_BulkEditResult()
		: bCanceled(false)
		, bSetupUndoRedo(true)
	{}
};

class RIGVMDEVELOPER_API FRigVMControllerCompileBracketScope
{
public:
   
	FRigVMControllerCompileBracketScope(URigVMController *InController);

	~FRigVMControllerCompileBracketScope();

private:

	URigVMGraph* Graph;
	bool bSuspendNotifications;
};

DECLARE_DELEGATE_RetVal_OneParam(bool, FRigVMController_ShouldStructUnfoldDelegate, const UStruct*)
DECLARE_DELEGATE_RetVal_OneParam(TArray<FRigVMExternalVariable>, FRigVMController_GetExternalVariablesDelegate, URigVMGraph*)
DECLARE_DELEGATE_RetVal(const FRigVMByteCode*, FRigVMController_GetByteCodeDelegate)
DECLARE_DELEGATE_RetVal_OneParam(bool, FRigVMController_RequestLocalizeFunctionDelegate, FRigVMGraphFunctionIdentifier&)
DECLARE_DELEGATE_RetVal_ThreeParams(FName, FRigVMController_RequestNewExternalVariableDelegate, FRigVMGraphVariableDescription, bool, bool);
DECLARE_DELEGATE_RetVal_TwoParams(bool, FRigVMController_IsDependencyCyclicDelegate, const FRigVMGraphFunctionHeader& Dependent, const FRigVMGraphFunctionHeader& Dependency)
DECLARE_DELEGATE_RetVal_TwoParams(FRigVMController_BulkEditResult, FRigVMController_RequestBulkEditDialogDelegate, URigVMLibraryNode*, ERigVMControllerBulkEditType)
DECLARE_DELEGATE_RetVal_OneParam(bool, FRigVMController_RequestBreakLinksDialogDelegate, TArray<URigVMLink*>)
DECLARE_DELEGATE_RetVal_OneParam(TRigVMTypeIndex, FRigVMController_RequestPinTypeSelectionDelegate, const TArray<TRigVMTypeIndex>& Types)
DECLARE_DELEGATE_FiveParams(FRigVMController_OnBulkEditProgressDelegate, TSoftObjectPtr<URigVMFunctionReferenceNode>, ERigVMControllerBulkEditType, ERigVMControllerBulkEditProgress, int32, int32)
DECLARE_DELEGATE_RetVal_TwoParams(FString, FRigVMController_PinPathRemapDelegate, const FString& /* InPinPath */, bool /* bIsInput */);
DECLARE_DELEGATE_OneParam(FRigVMController_RequestJumpToHyperlinkDelegate, const UObject* InSubject);
DECLARE_DELEGATE_OneParam(FRigVMController_ConfigureWorkflowOptionsDelegate, URigVMUserWorkflowOptions* InOutOptions);
DECLARE_DELEGATE_RetVal_TwoParams(bool, FRigVMController_CheckPinCompatibilityDelegate, URigVMPin*, URigVMPin*);

USTRUCT(BlueprintType)
struct RIGVMDEVELOPER_API FRigStructScope
{
	GENERATED_BODY()

public:
	
	FRigStructScope()
		: ScriptStruct(nullptr)
		, Memory(nullptr)
	{}

	template <
		typename T,
		typename TEnableIf<TModels_V<CRigVMUStruct, T>>::Type * = nullptr
	>
	FRigStructScope(const T& InInstance)
		: ScriptStruct(T::StaticStruct())
		, Memory((const uint8*)&InInstance)
	{}

	FRigStructScope(const FStructOnScope& InScope)
		: ScriptStruct(Cast<UScriptStruct>(InScope.GetStruct()))
		, Memory(InScope.GetStructMemory()) 
	{}

	const UScriptStruct* GetScriptStruct() const { return ScriptStruct; }
	const uint8* GetMemory() const { return Memory; }
	bool IsValid() const { return ScriptStruct != nullptr && Memory != nullptr; }

protected:

	const UScriptStruct* ScriptStruct;
	const uint8* Memory;
};

// A struct describing the result of a backwards compatibility patch
USTRUCT()
struct RIGVMDEVELOPER_API FRigVMClientPatchResult
{
public:

	GENERATED_BODY()
	
	FRigVMClientPatchResult()
		: bSucceeded(true)
		, bChangedContent(false)
		, bRequiresToMarkPackageDirty(false)
	{}

	bool Succeeded() const { return bSucceeded; }
	bool ChangedContent() const { return bChangedContent; }
	bool RequiresToMarkPackageDirty() const { return bRequiresToMarkPackageDirty; }

	const TArray<FString>& GetErrorMessages() const { return ErrorMessages; }
	const TArray<FString>& GetRemovedNodes() const { return RemovedNodes; }
	const TArray<TWeakObjectPtr<const URigVMNode>>& GetAddedNodes() const { return AddedNodes; }

private:

	void Merge(const FRigVMClientPatchResult& InOther);
	
	bool bSucceeded;
	bool bChangedContent;
	bool bRequiresToMarkPackageDirty;

	TArray<FString> ErrorMessages;
	TArray<FString> RemovedNodes;
	TArray<TWeakObjectPtr<const URigVMNode>> AddedNodes;

	friend struct FRigVMClient;
	friend class URigVMController;
};

struct RIGVMDEVELOPER_API FRigVMPinInfo
{
	FRigVMPinInfo();
	FRigVMPinInfo(const URigVMPin* InPin, int32 InParentIndex, ERigVMPinDirection InDirection = ERigVMPinDirection::Invalid);
	FRigVMPinInfo(FProperty* InProperty, ERigVMPinDirection InDirection, int32 InParentIndex, const uint8* InDefaultValueMemory);

	void CorrectExecuteTypeIndex();

	int32 ParentIndex;
	FName Name;
	ERigVMPinDirection Direction;
	TRigVMTypeIndex TypeIndex;
	bool bIsArray;
	FProperty* Property;
	FString PinPath;
	FString DefaultValue;
	FString DisplayName;
	FString CustomWidgetName;
	bool bIsExpanded;
	bool bIsConstant;
	bool bIsDynamicArray;
	bool bIsDecorator;
	bool bIsLazy;
	TArray<int32> SubPins;

	friend uint32 GetTypeHash(const FRigVMPinInfo& InPin);
};

struct RIGVMDEVELOPER_API FRigVMPinInfoArray
{
	FRigVMPinInfoArray() {}
	explicit FRigVMPinInfoArray(const URigVMNode* InNode);
	FRigVMPinInfoArray(const URigVMNode* InNode, URigVMController* InController, const FRigVMPinInfoArray* InPreviousPinInfos = nullptr);
	FRigVMPinInfoArray(const FRigVMGraphFunctionHeader& FunctionHeader, URigVMController* InController, const FRigVMPinInfoArray* InPreviousPinInfos = nullptr);

	int32 Num() const { return Pins.Num(); }
	const FRigVMPinInfo& operator[](int32 InIndex) const { return Pins[InIndex]; }
	FRigVMPinInfo& operator[](int32 InIndex) { return Pins[InIndex]; }
	TArray<FRigVMPinInfo>::RangedForIteratorType begin() const { return Pins.begin(); }
	TArray<FRigVMPinInfo>::RangedForIteratorType end() const { return Pins.end(); }

	int32 AddPin(const URigVMPin* InPin, int32 InParentIndex, ERigVMPinDirection InDirection = ERigVMPinDirection::Invalid);
	int32 AddPin(FProperty* InProperty, URigVMController* InController, ERigVMPinDirection InDirection, int32 InParentIndex, const uint8* InDefaultValueMemory, bool bAddSubPins);
	int32 AddPin(URigVMController* InController, int32 InParentIndex, const FName& InName, ERigVMPinDirection InDirection, TRigVMTypeIndex InTypeIndex, const FString& InDefaultValue, const uint8* InDefaultValueMemory, const FRigVMPinInfoArray* InPreviousPinInfos, bool bAddSubPins);
	void AddPins(UScriptStruct* InScriptStruct, URigVMController* InController, ERigVMPinDirection InDirection, int32 InParentIndex, const uint8* InDefaultValueMemory, bool bAddSubPins);

	const FString& GetPinPath(const int32 InIndex) const;
	int32 GetIndexFromPinPath(const FString& InPinPath) const;
	const FRigVMPinInfo* GetPinFromPinPath(const FString& InPinPath) const;
	int32 GetRootIndex(const int32 InIndex) const;

	friend RIGVMDEVELOPER_API uint32 GetTypeHash(const FRigVMPinInfoArray& InPins);

	mutable TArray<FRigVMPinInfo> Pins;
	mutable TMap<FString, int32> PinPathLookup;;
};

RIGVMDEVELOPER_API uint32 GetTypeHash(const FRigVMPinInfoArray& InPins);

/**
 * The Controller is the sole authority to perform changes
 * on the Graph. The Controller itself is stateless.
 * The Controller offers a Modified event to subscribe to 
 * for user interface views - so they can be informed about
 * any change that's happening within the Graph.
 * The Controller routes all changes through the Graph itself,
 * so you can have N Controllers performing edits on 1 Graph,
 * and N Views subscribing to 1 Controller.
 * In Python you can also subscribe to this event to be 
 * able to react to topological changes of the Graph there.
 */
UCLASS(BlueprintType)
class RIGVMDEVELOPER_API URigVMController : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	// Default constructor
	URigVMController();

	// Default destructor
	~URigVMController();

#if WITH_EDITORONLY_DATA
	static void DeclareConstructClasses(TArray<FTopLevelAssetPath>& OutConstructClasses, const UClass* SpecificSubclass);
#endif

	// Returns the currently edited Graph of this controller.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	URigVMGraph* GetGraph() const;

	// Sets the currently edited Graph of this controller.
	// This causes a GraphChanged modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController, meta=(DeprecatedFunction, DeprecationMessage="Function has been deprecated, please rely on GetControllerForGraph instead."))
	void SetGraph(URigVMGraph* InGraph);

	// Returns the schema used by this controller
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	URigVMSchema* GetSchema() const { return SchemaPtr; }

	// Sets the schema on the controller
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	void SetSchema(URigVMSchema* InSchema);

	// Pushes a new graph to the stack
	// This causes a GraphChanged modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController, meta=(DeprecatedFunction, DeprecationMessage="Function has been deprecated, please rely on GetControllerForGraph instead."))
	bool PushGraph(URigVMGraph* InGraph, bool bSetupUndoRedo = true);

	// Pops the last graph off the stack
	// This causes a GraphChanged modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController, meta=(DeprecatedFunction, DeprecationMessage="Function has been deprecated, please rely on GetControllerForGraph instead."))
	URigVMGraph* PopGraph(bool bSetupUndoRedo = true);
	
	// Returns the top level graph
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	URigVMGraph* GetTopLevelGraph() const;

	// Returns another controller for a given graph
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	URigVMController* GetControllerForGraph(const URigVMGraph* InGraph) const;

	// The Modified event used to subscribe to changes
	// happening within the Graph. This is broadcasted to 
	// for any change happening - not only the changes 
	// performed by this Controller - so it can be used
	// for UI Views to react accordingly.
	FRigVMGraphModifiedEvent& OnModified();

	// Submits an event to the graph for broadcasting.
	void Notify(ERigVMGraphNotifType InNotifType, UObject* InSubject) const;

	// Resends all notifications
	void ResendAllNotifications();

	// Enables or disables the error reporting of this Controller.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	void EnableReporting(bool bEnabled = true) { bReportWarningsAndErrors = bEnabled; }

	// Returns true if reporting is enabled
	UFUNCTION(BlueprintPure, Category = RigVMController)
	bool IsReportingEnabled() const { return bReportWarningsAndErrors; }

	// Returns true if the controller is currently transacting
	UFUNCTION(BlueprintPure, Category = RigVMController)
	bool IsTransacting() const { return bIsTransacting; }

	UFUNCTION(BlueprintCallable, Category = RigVMController)
	TArray<FString> GeneratePythonCommands();

	TArray<FString> GetAddNodePythonCommands(URigVMNode* Node) const;
	TArray<FString> GetAddDecoratorPythonCommands(URigVMNode* Node, const FName& DecoratorName) const;

#if WITH_EDITOR
	// Note: The functions below are scoped with WITH_EDITOR since we are considering
	// to move this code into the runtime in the future. Right now there's a dependency
	// on the metadata of the USTRUCT - which is only available in the editor.

	// Adds a Function / Struct Node to the edited Graph.
	// UnitNode represent a RIGVM_METHOD declaration on a USTRUCT.
	// This causes a NodeAdded modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	URigVMUnitNode* AddUnitNode(UScriptStruct* InScriptStruct, const FName& InMethodName = TEXT("Execute"), const FVector2D& InPosition = FVector2D::ZeroVector, const FString& InNodeName = TEXT(""), bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Adds a Function / Struct Node to the edited Graph given its struct object path name.
	// UnitNode represent a RIGVM_METHOD declaration on a USTRUCT.
	// This causes a NodeAdded modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	URigVMUnitNode* AddUnitNodeFromStructPath(const FString& InScriptStructPath, const FName& InMethodName = TEXT("Execute"), const FVector2D& InPosition = FVector2D::ZeroVector, const FString& InNodeName = TEXT(""), bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Adds a unit node using a template
	template <
		typename T,
		typename TEnableIf<TModels_V<CRigVMUStruct, T>>::Type * = nullptr
	>
	URigVMUnitNode* AddUnitNode(const T& InDefaults, const FName& InMethodName = TEXT("Execute"), const FVector2D& InPosition = FVector2D::ZeroVector, const FString& InNodeName = TEXT(""), bool bSetupUndoRedo = true, bool bPrintPythonCommand = false)
	{
		return AddUnitNodeWithDefaults(T::StaticStruct(), InDefaults, InMethodName, InPosition, InNodeName, bSetupUndoRedo, bPrintPythonCommand); 
	}

	// Adds a Function / Struct Node to the edited Graph.
	// UnitNode represent a RIGVM_METHOD declaration on a USTRUCT.
	// This causes a NodeAdded modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	URigVMUnitNode* AddUnitNodeWithDefaults(UScriptStruct* InScriptStruct, const FString& InDefaults, const FName& InMethodName = TEXT("Execute"), const FVector2D& InPosition = FVector2D::ZeroVector, const FString& InNodeName = TEXT(""), bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Adds a Function / Struct Node to the edited Graph.
	// UnitNode represent a RIGVM_METHOD declaration on a USTRUCT.
	// This causes a NodeAdded modified event.
	URigVMUnitNode* AddUnitNodeWithDefaults(UScriptStruct* InScriptStruct, const FRigStructScope& InDefaults, const FName& InMethodName = TEXT("Execute"), const FVector2D& InPosition = FVector2D::ZeroVector, const FString& InNodeName = TEXT(""), bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Adds a Function / Struct Node to the edited Graph.
	// UnitNode represent a RIGVM_METHOD declaration on a USTRUCT.
	// This causes a NodeAdded modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool SetUnitNodeDefaults(URigVMUnitNode* InNode, const FString& InDefaults, bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);
	bool SetUnitNodeDefaults(URigVMUnitNode* InNode, const FRigStructScope& InDefaults, bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Adds a Variable Node to the edited Graph.
	// Variables represent local work state for the function and
	// can be read from and written to. 
	// This causes a NodeAdded modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	URigVMVariableNode* AddVariableNode(const FName& InVariableName, const FString& InCPPType, UObject* InCPPTypeObject, bool bIsGetter, const FString& InDefaultValue, const FVector2D& InPosition = FVector2D::ZeroVector, const FString& InNodeName = TEXT(""), bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Adds a Variable Node to the edited Graph given a struct object path name.
	// Variables represent local work state for the function and
	// can be read from (bIsGetter == true) or written to (bIsGetter == false). 
	// This causes a NodeAdded modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	URigVMVariableNode* AddVariableNodeFromObjectPath(const FName& InVariableName, const FString& InCPPType, const FString& InCPPTypeObjectPath, bool bIsGetter, const FString& InDefaultValue, const FVector2D& InPosition = FVector2D::ZeroVector, const FString& InNodeName = TEXT(""), bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Refreshes the variable node with the new data
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	void RefreshVariableNode(const FName& InNodeName, const FName& InVariableName, const FString& InCPPType, UObject* InCPPTypeObject, bool bSetupUndoRedo, bool bSetupOrphanPins = true);

	// Removes all nodes related to a given variable
	void OnExternalVariableRemoved(const FName& InVarName, bool bSetupUndoRedo);

	// Renames the variable name in all relevant nodes
	bool OnExternalVariableRenamed(const FName& InOldVarName, const FName& InNewVarName, bool bSetupUndoRedo);

	// Changes the data type of all nodes matching a given variable name
	void OnExternalVariableTypeChanged(const FName& InVarName, const FString& InCPPType, UObject* InCPPTypeObject, bool bSetupUndoRedo);
	void OnExternalVariableTypeChangedFromObjectPath(const FName& InVarName, const FString& InCPPType, const FString& InCPPTypeObjectPath, bool bSetupUndoRedo);

	// Refreshes the variable node with the new data
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	URigVMVariableNode* ReplaceParameterNodeWithVariable(const FName& InNodeName, const FName& InVariableName, const FString& InCPPType, UObject* InCPPTypeObject, bool bSetupUndoRedo);

	// Turns a resolved templated node(s) back into its template.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool UnresolveTemplateNodes(const TArray<FName>& InNodeNames, bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);
	bool UnresolveTemplateNodes(const TArray<URigVMNode*>& InNodes, bool bSetupUndoRedo);

	// Upgrades a set of nodes with each corresponding next known version
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	TArray<URigVMNode*> UpgradeNodes(const TArray<FName>& InNodeNames, bool bRecursive = true, bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Adds a Parameter Node to the edited Graph.
	// Parameters represent input or output arguments to the Graph / Function.
	// Input Parameters are constant values / literals.
	// This causes a NodeAdded modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController, meta=(DeprecatedFunction))
	URigVMParameterNode* AddParameterNode(const FName& InParameterName, const FString& InCPPType, UObject* InCPPTypeObject, bool bIsInput, const FString& InDefaultValue, const FVector2D& InPosition = FVector2D::ZeroVector, const FString& InNodeName = TEXT(""), bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Adds a Parameter Node to the edited Graph given a struct object path name.
	// Parameters represent input or output arguments to the Graph / Function.
	// Input Parameters are constant values / literals.
	// This causes a NodeAdded modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController, meta=(DeprecatedFunction))
	URigVMParameterNode* AddParameterNodeFromObjectPath(const FName& InParameterName, const FString& InCPPType, const FString& InCPPTypeObjectPath, bool bIsInput, const FString& InDefaultValue, const FVector2D& InPosition = FVector2D::ZeroVector, const FString& InNodeName = TEXT(""), bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Adds a Comment Node to the edited Graph.
	// Comments can be used to annotate the Graph.
	// This causes a NodeAdded modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	URigVMCommentNode* AddCommentNode(const FString& InCommentText, const FVector2D& InPosition = FVector2D::ZeroVector, const FVector2D& InSize = FVector2D(400.f, 300.f), const FLinearColor& InColor = FLinearColor::Black, const FString& InNodeName = TEXT(""), bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Adds a Reroute Node on an existing Link to the edited Graph.
	// Reroute Nodes can be used to visually improve the data flow,
	// they don't require any additional memory though and are purely
	// cosmetic. This causes a NodeAdded modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	URigVMRerouteNode* AddRerouteNodeOnLink(URigVMLink* InLink, const FVector2D& InPosition = FVector2D::ZeroVector, const FString& InNodeName = TEXT(""), bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Adds a Reroute Node on an existing Link to the edited Graph given the Link's string representation.
	// Reroute Nodes can be used to visually improve the data flow,
	// they don't require any additional memory though and are purely
	// cosmetic. This causes a NodeAdded modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	URigVMRerouteNode* AddRerouteNodeOnLinkPath(const FString& InLinkPinPathRepresentation, const FVector2D& InPosition = FVector2D::ZeroVector, const FString& InNodeName = TEXT(""), bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Adds a Reroute Node on an existing Pin to the editor Graph.
	// Reroute Nodes can be used to visually improve the data flow,
	// they don't require any additional memory though and are purely
	// cosmetic. This causes a NodeAdded modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	URigVMRerouteNode* AddRerouteNodeOnPin(const FString& InPinPath, bool bAsInput, const FVector2D& InPosition = FVector2D::ZeroVector, const FString& InNodeName = TEXT(""), bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Adds a free Reroute Node
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	URigVMRerouteNode* AddFreeRerouteNode(const FString& InCPPType, const FName& InCPPTypeObjectPath, bool bIsConstant, const FName& InCustomWidgetName, const FString& InDefaultValue, const FVector2D& InPosition = FVector2D::ZeroVector, const FString& InNodeName = TEXT(""), bool bSetupUndoRedo = true);

	// helper function to set up a constant node
	URigVMTemplateNode* AddConstantNode(const FString& InCPPType, const FName& InCPPTypeObjectPath, const FString& InDefaultValue, const FVector2D& InPosition = FVector2D::ZeroVector, const FString& InNodeName = TEXT(""), bool bSetupUndoRedo = true);

	// helper function to set up a make struct node
	URigVMTemplateNode* AddMakeStructNode(const FString& InCPPType, const FName& InCPPTypeObjectPath, const FString& InDefaultValue, const FVector2D& InPosition = FVector2D::ZeroVector, const FString& InNodeName = TEXT(""), bool bSetupUndoRedo = true);

	// helper function to set up a break struct node
	URigVMTemplateNode* AddBreakStructNode(const FString& InCPPType, const FName& InCPPTypeObjectPath, const FString& InDefaultValue, const FVector2D& InPosition = FVector2D::ZeroVector, const FString& InNodeName = TEXT(""), bool bSetupUndoRedo = true);

	// helper function to set up a constant node on a pin
	URigVMTemplateNode* AddConstantNodeOnPin(const FString& InPinPath, const FVector2D& InPosition = FVector2D::ZeroVector, const FString& InNodeName = TEXT(""), bool bSetupUndoRedo = true);

	// helper function to set up a make struct node on a pin
	URigVMTemplateNode* AddMakeStructNodeOnPin(const FString& InPinPath, const FVector2D& InPosition = FVector2D::ZeroVector, const FString& InNodeName = TEXT(""), bool bSetupUndoRedo = true);

	// helper function to set up a break struct node on a pin
	URigVMTemplateNode* AddBreakStructNodeOnPin(const FString& InPinPath, const FVector2D& InPosition = FVector2D::ZeroVector, const FString& InNodeName = TEXT(""), bool bSetupUndoRedo = true);

	// Adds a branch node to the graph.
	// Branch nodes can be used to split the execution of into multiple branches,
	// allowing to drive behavior by logic.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	URigVMNode* AddBranchNode(const FVector2D& InPosition = FVector2D::ZeroVector, const FString& InNodeName = TEXT(""), bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Adds an if node to the graph.
	// If nodes can be used to pick between two values based on a condition.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	URigVMNode* AddIfNode(const FString& InCPPType, const FName& InCPPTypeObjectPath, const FVector2D& InPosition = FVector2D::ZeroVector, const FString& InNodeName = TEXT(""), bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	URigVMNode* AddIfNodeFromStruct(UScriptStruct* InScriptStruct, const FVector2D& InPosition = FVector2D::ZeroVector, const FString& InNodeName = TEXT(""), bool bSetupUndoRedo = true);

	// Adds a select node to the graph.
	// Select nodes can be used to pick between multiple values based on an index.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	URigVMNode* AddSelectNode(const FString& InCPPType, const FName& InCPPTypeObjectPath, const FVector2D& InPosition = FVector2D::ZeroVector, const FString& InNodeName = TEXT(""), bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	URigVMNode* AddSelectNodeFromStruct(UScriptStruct* InScriptStruct, const FVector2D& InPosition = FVector2D::ZeroVector, const FString& InNodeName = TEXT(""), bool bSetupUndoRedo = true);

	// Adds a template node to the graph.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	URigVMTemplateNode* AddTemplateNode(const FName& InNotation, const FVector2D& InPosition = FVector2D::ZeroVector, const FString& InNodeName = TEXT(""), bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Returns all registered unit structs
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	static TArray<UScriptStruct*> GetRegisteredUnitStructs();

	// Returns all registered template notations
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	static TArray<FString> GetRegisteredTemplates();

	// Returns all supported unit structs for a given template notation
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	static TArray<UScriptStruct*> GetUnitStructsForTemplate(const FName& InNotation);

	// Returns the template for a given function (or an empty string)
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	static FString GetTemplateForUnitStruct(UScriptStruct* InFunction, const FString& InMethodName = TEXT("Execute"));

	// Resolves a wildcard pin on any node
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool ResolveWildCardPin(const FString& InPinPath, const FString& InCPPType, const FName& InCPPTypeObjectPath, bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);
	bool ResolveWildCardPin(URigVMPin* InPin, const FRigVMTemplateArgumentType& InType, bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);
	bool ResolveWildCardPin(const FString& InPinPath, TRigVMTypeIndex InTypeIndex, bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);
	bool ResolveWildCardPin(URigVMPin* InPin, TRigVMTypeIndex InTypeIndex, bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Adds a Function / Struct Node to the edited Graph as an injected node
	// UnitNode represent a RIGVM_METHOD declaration on a USTRUCT.
	// This causes a NodeAdded modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	URigVMInjectionInfo* AddInjectedNode(const FString& InPinPath, bool bAsInput, UScriptStruct* InScriptStruct, const FName& InMethodName, const FName& InInputPinName, const FName& InOutputPinName, const FString& InNodeName = TEXT(""), bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Adds a Function / Struct Node to the edited Graph as an injected node
	// UnitNode represent a RIGVM_METHOD declaration on a USTRUCT.
	// This causes a NodeAdded modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	URigVMInjectionInfo* AddInjectedNodeFromStructPath(const FString& InPinPath, bool bAsInput, const FString& InScriptStructPath, const FName& InMethodName, const FName& InInputPinName, const FName& InOutputPinName, const FString& InNodeName = TEXT(""), bool bSetupUndoRedo = true);

	// Removes an injected node
	// This causes a NodeRemoved modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool RemoveInjectedNode(const FString& InPinPath, bool bAsInput, bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Ejects the last injected node on a pin
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	URigVMNode* EjectNodeFromPin(const FString& InPinPath, bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Adds an enum node to the graph
	// Enum nodes can be used to represent constant enum values within the graph
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	URigVMEnumNode* AddEnumNode(const FName& InCPPTypeObjectPath, const FVector2D& InPosition = FVector2D::ZeroVector, const FString& InNodeName = TEXT(""), bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Adds a Array Node to the edited Graph.
	// This causes a NodeAdded modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	URigVMNode* AddArrayNode(ERigVMOpCode InOpCode, const FString& InCPPType, UObject* InCPPTypeObject, const FVector2D& InPosition = FVector2D::ZeroVector, const FString& InNodeName = TEXT(""), bool bSetupUndoRedo = true, bool bPrintPythonCommand = false, bool bIsPatching = false);

	// Adds a Array Node to the edited Graph given a struct object path name.
	// This causes a NodeAdded modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	URigVMNode* AddArrayNodeFromObjectPath(ERigVMOpCode InOpCode, const FString& InCPPType, const FString& InCPPTypeObjectPath, const FVector2D& InPosition = FVector2D::ZeroVector, const FString& InNodeName = TEXT(""), bool bSetupUndoRedo = true, bool bPrintPythonCommand = false, bool bIsPatching = false);

	// Adds an entry invocation node
	// This causes a NodeAdded modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	URigVMInvokeEntryNode* AddInvokeEntryNode(const FName& InEntryName, const FVector2D& InPosition = FVector2D::ZeroVector, const FString& InNodeName = TEXT(""), bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Adds a decorator to a node
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	FName AddDecorator(const FName& InNodeName, const FName& InDecoratorTypeObjectPath, const FName& InDecoratorName = NAME_None, const FString& InDefaultValue = TEXT(""), int32 InPinIndex = -1, bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Removes a decorator from a node
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool RemoveDecorator(const FName& InNodeName, const FName& InDecoratorName, bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Un-does the last action on the stack.
	// Note: This should really only be used for unit tests,
	// use the GEditor's main Undo method instead.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool Undo();

	// Re-does the last action on the stack.
	// Note: This should really only be used for unit tests,
	// use the GEditor's main Undo method instead.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool Redo();

	// Opens an undo bracket / scoped transaction for
	// a series of actions to be performed as one step on the 
	// Undo stack. This is primarily useful for Python.
	// This causes a UndoBracketOpened modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool OpenUndoBracket(const FString& InTitle);

	// Closes an undo bracket / scoped transaction.
	// This is primarily useful for Python.
	// This causes a UndoBracketClosed modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool CloseUndoBracket();

	// Cancels an undo bracket / scoped transaction.
	// This is primarily useful for Python.
	// This causes a UndoBracketCanceled modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool CancelUndoBracket();

	// Exports the given nodes as text
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	FString ExportNodesToText(const TArray<FName>& InNodeNames, bool bIncludeExteriorLinks = false);

	// Exports the selected nodes as text
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	FString ExportSelectedNodesToText(bool bIncludeExteriorLinks = false);

	// Exports the given nodes as text
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool CanImportNodesFromText(const FString& InText);

	// Exports the given nodes as text
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	TArray<FName> ImportNodesFromText(const FString& InText, bool bSetupUndoRedo = true, bool bPrintPythonCommands = false);

	// Copies a function declaration into this graph's local function library
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	URigVMLibraryNode* LocalizeFunctionFromPath(const FString& InHostPath, const FName& InFunctionName, bool bLocalizeDependentPrivateFunctions = true, bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	UFUNCTION(BlueprintCallable, Category = RigVMController)
    URigVMLibraryNode* LocalizeFunction(
		const FRigVMGraphFunctionIdentifier& InFunctionDefinition,
		bool bLocalizeDependentPrivateFunctions = true,
		bool bSetupUndoRedo = true,
		bool bPrintPythonCommand = false);

	// Copies a series of function declaratioms into this graph's local function library
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	TMap<FRigVMGraphFunctionIdentifier, URigVMLibraryNode*> LocalizeFunctions(
		TArray<FRigVMGraphFunctionIdentifier> InFunctionDefinitions,
		bool bLocalizeDependentPrivateFunctions = true,
		bool bSetupUndoRedo = true,
		bool bPrintPythonCommand = false);


	// Turns a series of nodes into a Collapse node
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	URigVMCollapseNode* CollapseNodes(const TArray<FName>& InNodeNames, const FString& InCollapseNodeName = TEXT(""), bool bSetupUndoRedo = true, bool bPrintPythonCommand = false, bool bIsAggregate = false);

	// Turns a library node into its contained nodes
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	TArray<URigVMNode*> ExpandLibraryNode(const FName& InNodeName, bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Turns a collapse node into a function node
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	FName PromoteCollapseNodeToFunctionReferenceNode(const FName& InNodeName, bool bSetupUndoRedo = true, bool bPrintPythonCommand = false, const FString& InExistingFunctionDefinitionPath = TEXT(""));

	// Turns a collapse node into a function node
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	FName PromoteFunctionReferenceNodeToCollapseNode(const FName& InNodeName, bool bSetupUndoRedo = true, bool bPrintPythonCommand = false, bool bRemoveFunctionDefinition = false);

#endif

	// Removes a node from the graph
	// This causes a NodeRemoved modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool RemoveNode(URigVMNode* InNode, bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Removes a node from the graph given the node's name.
	// This causes a NodeRemoved modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool RemoveNodeByName(const FName& InNodeName, bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Removes a list of nodes from the graph
	// This causes a NodeRemoved modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool RemoveNodes(TArray<URigVMNode*> InNodes, bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Removes a list of nodes from the graph given the names
	// This causes a NodeRemoved modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool RemoveNodesByName(const TArray<FName>& InNodeNames, bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Renames a node in the graph
	// This causes a NodeRenamed modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool RenameNode(URigVMNode* InNode, const FName& InNewName, bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Selects a single node in the graph.
	// This causes a NodeSelected / NodeDeselected modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool SelectNode(URigVMNode* InNode, bool bSelect = true, bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Selects a single node in the graph by name.
	// This causes a NodeSelected / NodeDeselected modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool SelectNodeByName(const FName& InNodeName, bool bSelect = true, bool bSetupUndoRedo = true);

	// Deselects all currently selected nodes in the graph.
	// This might cause several NodeDeselected modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool ClearNodeSelection(bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Selects the nodes given the selection
	// This might cause several NodeDeselected modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool SetNodeSelection(const TArray<FName>& InNodeNames, bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Sets the position of a node in the graph.
	// This causes a NodePositionChanged modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool SetNodePosition(URigVMNode* InNode, const FVector2D& InPosition, bool bSetupUndoRedo = true, bool bMergeUndoAction = false, bool bPrintPythonCommand = false);

	// Sets the position of a node in the graph by name.
	// This causes a NodePositionChanged modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool SetNodePositionByName(const FName& InNodeName, const FVector2D& InPosition, bool bSetupUndoRedo = true, bool bMergeUndoAction = false, bool bPrintPythonCommand = false);

	// Sets the size of a node in the graph.
	// This causes a NodeSizeChanged modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool SetNodeSize(URigVMNode* InNode, const FVector2D& InSize, bool bSetupUndoRedo = true, bool bMergeUndoAction = false, bool bPrintPythonCommand = false);

	// Sets the size of a node in the graph by name.
	// This causes a NodeSizeChanged modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool SetNodeSizeByName(const FName& InNodeName, const FVector2D& InSize, bool bSetupUndoRedo = true, bool bMergeUndoAction = false, bool bPrintPythonCommand = false);

	// Sets the color of a node in the graph.
	// This causes a NodeColorChanged modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool SetNodeColor(URigVMNode* InNode, const FLinearColor& InColor, bool bSetupUndoRedo = true, bool bMergeUndoAction = false, bool bPrintPythonCommand = false);

	// Sets the color of a node in the graph by name.
	// This causes a NodeColorChanged modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool SetNodeColorByName(const FName& InNodeName, const FLinearColor& InColor, bool bSetupUndoRedo = true, bool bMergeUndoAction = false);

	// Sets the category of a node in the graph.
	// This causes a NodeCategoryChanged modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool SetNodeCategory(URigVMCollapseNode* InNode, const FString& InCategory, bool bSetupUndoRedo = true, bool bMergeUndoAction = false, bool bPrintPythonCommand = false);

	// Sets the category of a node in the graph.
	// This causes a NodeCategoryChanged modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool SetNodeCategoryByName(const FName& InNodeName, const FString& InCategory, bool bSetupUndoRedo = true, bool bMergeUndoAction = false);

	// Sets the keywords of a node in the graph.
	// This causes a NodeKeywordsChanged modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool SetNodeKeywords(URigVMCollapseNode* InNode, const FString& InKeywords, bool bSetupUndoRedo = true, bool bMergeUndoAction = false, bool bPrintPythonCommand = false);

	// Sets the keywords of a node in the graph.
	// This causes a NodeKeywordsChanged modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool SetNodeKeywordsByName(const FName& InNodeName, const FString& InKeywords, bool bSetupUndoRedo = true, bool bMergeUndoAction = false);

	// Sets the function description of a node in the graph.
	// This causes a NodeDescriptionChanged modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool SetNodeDescription(URigVMCollapseNode* InNode, const FString& InDescription, bool bSetupUndoRedo = true, bool bMergeUndoAction = false, bool bPrintPythonCommand = false);

	// Sets the keywords of a node in the graph.
	// This causes a NodeDescriptionChanged modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool SetNodeDescriptionByName(const FName& InNodeName, const FString& InDescription, bool bSetupUndoRedo = true, bool bMergeUndoAction = false);

	// Sets the comment text and properties of a comment node in the graph.
	// This causes a CommentTextChanged modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool SetCommentText(URigVMNode* InNode, const FString& InCommentText, const int32& InCommentFontSize, const bool& bInCommentBubbleVisible, const bool& bInCommentColorBubble, bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Sets the comment text and properties of a comment node in the graph by name.
	// This causes a CommentTextChanged modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool SetCommentTextByName(const FName& InNodeName, const FString& InCommentText, const int32& InCommentFontSize, const bool& bInCommentBubbleVisible, const bool& bInCommentColorBubble, bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Renames a variable in the graph.
	// This causes a VariableRenamed modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController, meta=(DeprecatedFunction))
	bool RenameVariable(const FName& InOldName, const FName& InNewName, bool bSetupUndoRedo = true);

	// Renames a parameter in the graph.
	// This causes a ParameterRenamed modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController, meta=(DeprecatedFunction))
	bool RenameParameter(const FName& InOldName, const FName& InNewName, bool bSetupUndoRedo = true);\

	// Upgrades a set of nodes with each corresponding next known version
	TArray<URigVMNode*> UpgradeNodes(const TArray<URigVMNode*>& InNodes, bool bRecursive = true, bool bSetupUndoRedo = true);

	// Upgrades a single node with its next known version
	URigVMNode* UpgradeNode(URigVMNode* InNode, bool bSetupUndoRedo = true, FRigVMController_PinPathRemapDelegate* OutRemapPinDelegate = nullptr);

	// Sets the pin to be expanded or not
	// This causes a PinExpansionChanged modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool SetPinExpansion(const FString& InPinPath, bool bIsExpanded, bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Sets the pin to be watched (or not)
	// This causes a PinWatchedChanged modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool SetPinIsWatched(const FString& InPinPath, bool bIsWatched, bool bSetupUndoRedo = true);

	// Returns the default value of a pin given its pinpath.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	FString GetPinDefaultValue(const FString& InPinPath);

	// Sets the default value of a pin given its pinpath.
	// This causes a PinDefaultValueChanged modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool SetPinDefaultValue(const FString& InPinPath, const FString& InDefaultValue, bool bResizeArrays = true, bool bSetupUndoRedo = true, bool bMergeUndoAction = false, bool bPrintPythonCommand = false, bool bSetValueOnLinkedPins = true);

	// Resets the default value of a pin given its pinpath.
	// This causes a PinDefaultValueChanged modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool ResetPinDefaultValue(const FString& InPinPath, bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	UFUNCTION(BlueprintCallable, Category = RigVMController)
	FString AddAggregatePin(const FString& InNodeName, const FString& InPinName, const FString& InDefaultValue = TEXT(""), bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool RemoveAggregatePin(const FString& InPinPath, bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

#if UE_RIGVM_AGGREGATE_NODES_ENABLED
	FString AddAggregatePin(URigVMNode* InNode, const FString& InPinName, const FString& InDefaultValue = TEXT(""), bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);
	bool RemoveAggregatePin(URigVMPin* InPin, bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);
#endif

	// Adds an array element pin to the end of an array pin.
	// This causes a PinArraySizeChanged modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	FString AddArrayPin(const FString& InArrayPinPath, const FString& InDefaultValue = TEXT(""), bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Duplicates an array element pin.
	// This causes a PinArraySizeChanged modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	FString DuplicateArrayPin(const FString& InArrayElementPinPath, bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Inserts an array element pin into an array pin.
	// This causes a PinArraySizeChanged modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	FString InsertArrayPin(const FString& InArrayPinPath, int32 InIndex = -1, const FString& InDefaultValue = TEXT(""), bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Removes an array element pin from an array pin.
	// This causes a PinArraySizeChanged modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool RemoveArrayPin(const FString& InArrayElementPinPath, bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Removes all (but one) array element pin from an array pin.
	// This causes a PinArraySizeChanged modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool ClearArrayPin(const FString& InArrayPinPath, bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Sets the size of the array pin
	// This causes a PinArraySizeChanged modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool SetArrayPinSize(const FString& InArrayPinPath, int32 InSize, const FString& InDefaultValue = TEXT(""), bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Binds a pin to a variable (or removes the binding given NAME_None)
	// This causes a PinBoundVariableChanged modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool BindPinToVariable(const FString& InPinPath, const FString& InNewBoundVariablePath, bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Removes the binging of a pin to a variable
	// This causes a PinBoundVariableChanged modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool UnbindPinFromVariable(const FString& InPinPath, bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Turns a variable node into one or more bindings
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool MakeBindingsFromVariableNode(const FName& InNodeName, bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Turns a binding to a variable node
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool MakeVariableNodeFromBinding(const FString& InPinPath, const FVector2D& InNodePosition = FVector2D::ZeroVector, bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Promotes a pin to a variable
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool PromotePinToVariable(const FString& InPinPath, bool bCreateVariableNode, const FVector2D& InNodePosition = FVector2D::ZeroVector, bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Adds a link to the graph.
	// This causes a LinkAdded modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool AddLink(const FString& InOutputPinPath, const FString& InInputPinPath, bool bSetupUndoRedo = true, bool bPrintPythonCommand = false, ERigVMPinDirection InUserDirection = ERigVMPinDirection::Output, bool bCreateCastNode = false);

	// Removes a link from the graph.
	// This causes a LinkRemoved modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool BreakLink(const FString& InOutputPinPath, const FString& InInputPinPath, bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Removes all links on a given pin from the graph.
	// This might cause multiple LinkRemoved modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool BreakAllLinks(const FString& InPinPath, bool bAsInput = true, bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Adds an exposed pin to the graph controlled by this
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	FName AddExposedPin(const FName& InPinName, ERigVMPinDirection InDirection, const FString& InCPPType, const FName& InCPPTypeObjectPath, const FString& InDefaultValue, bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Removes an exposed pin from the graph controlled by this
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool RemoveExposedPin(const FName& InPinName, bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Renames an exposed pin in the graph controlled by this
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool RenameExposedPin(const FName& InOldPinName, const FName& InNewPinName, bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Changes the type of an exposed pin in the graph controlled by this
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool ChangeExposedPinType(const FName& InPinName, const FString& InCPPType, const FName& InCPPTypeObjectPath, UPARAM(ref) bool& bSetupUndoRedo, bool bSetupOrphanPins = true, bool bPrintPythonCommand = false);

	// Sets the index for an exposed pin. This can be used to move the pin up and down on the node.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool SetExposedPinIndex(const FName& InPinName, int32 InNewIndex, bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Adds a function reference / invocation to the graph
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	URigVMFunctionReferenceNode* AddFunctionReferenceNodeFromDescription(const FRigVMGraphFunctionHeader& InFunctionDefinition, const FVector2D& InNodePosition = FVector2D::ZeroVector, const
	                                                                     FString& InNodeName = TEXT(""), bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	URigVMFunctionReferenceNode* AddExternalFunctionReferenceNode(const FString& InHostPath, const FName& InFunctionName, const FVector2D& InNodePosition = FVector2D::ZeroVector, const FString& InNodeName = TEXT(""), bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);
	
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	URigVMFunctionReferenceNode* AddFunctionReferenceNode(URigVMLibraryNode* InFunctionDefinition, const FVector2D& InNodePosition = FVector2D::ZeroVector, const FString& InNodeName = TEXT(""), bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Sets the remapped variable on a function reference node
	UFUNCTION(BlueprintCallable, Category = RigVMController)
    bool SetRemappedVariable(URigVMFunctionReferenceNode* InFunctionRefNode, const FName& InInnerVariableName, const FName& InOuterVariableName, bool bSetupUndoRedo = true);

	// Adds a function definition to a function library graph
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	URigVMLibraryNode* AddFunctionToLibrary(const FName& InFunctionName, bool bMutable, const FVector2D& InNodePosition = FVector2D::ZeroVector, bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Removes a function from a function library graph
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool RemoveFunctionFromLibrary(const FName& InFunctionName, bool bSetupUndoRedo = true);

	// Renames a function in the function library
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool RenameFunction(const FName& InOldFunctionName, const FName& InNewFunctionName, bool bSetupUndoRedo = true);

	// Mark a function as public/private in the function library
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool MarkFunctionAsPublic(const FName& InFunctionName, bool bInIsPublic, bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Returns true if a function is marked as public in the function library
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool IsFunctionPublic(const FName& InFunctionName);

	// Add a local variable to the graph
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	FRigVMGraphVariableDescription AddLocalVariable(const FName& InVariableName, const FString& InCPPType, UObject* InCPPTypeObject, const FString& InDefaultValue, bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Add a local variable to the graph given a struct object path name.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	FRigVMGraphVariableDescription AddLocalVariableFromObjectPath(const FName& InVariableName, const FString& InCPPType, const FString& InCPPTypeObjectPath, const FString& InDefaultValue, bool bSetupUndoRedo = true);

	// Remove a local variable from the graph
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool RemoveLocalVariable(const FName& InVariableName, bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Rename a local variable from the graph
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool RenameLocalVariable(const FName& InVariableName, const FName& InNewVariableName, bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Sets the type of the local variable
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool SetLocalVariableType(const FName& InVariableName, const FString& InCPPType, UObject* InCPPTypeObject, bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool SetLocalVariableTypeFromObjectPath(const FName& InVariableName, const FString& InCPPType, const FString& InCPPTypeObjectPath, bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool SetLocalVariableDefaultValue(const FName& InVariableName, const FString& InDefaultValue, bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// creates the options struct for a given workflow
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	URigVMUserWorkflowOptions* MakeOptionsForWorkflow(UObject* InSubject, const FRigVMUserWorkflow& InWorkflow);

	// performs all actions representing the workflow
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool PerformUserWorkflow(const FRigVMUserWorkflow& InWorkflow, const URigVMUserWorkflowOptions* InOptions, bool bSetupUndoRedo = true);

	// Determine affected function references for a potential bulk edit on a library node
	TArray<TSoftObjectPtr<URigVMFunctionReferenceNode>> GetAffectedReferences(ERigVMControllerBulkEditType InEditType, bool bForceLoad = false);

	// Determine affected assets for a potential bulk edit on a library node
	TArray<FAssetData> GetAffectedAssets(ERigVMControllerBulkEditType InEditType, bool bForceLoad = false);

	// A delegate to retrieve the list of external variables
	FRigVMController_GetExternalVariablesDelegate GetExternalVariablesDelegate;

	// A delegate to retrieve the current bytecode of the graph
	FRigVMController_GetByteCodeDelegate GetCurrentByteCodeDelegate;

	// A delegate to localize a function on demand
	FRigVMController_RequestLocalizeFunctionDelegate RequestLocalizeFunctionDelegate;

	// A delegate to create a new blueprint member variable
	FRigVMController_RequestNewExternalVariableDelegate RequestNewExternalVariableDelegate;

	// A delegate to ask the host / client for a dialog to confirm a bulk edit
	FRigVMController_RequestBulkEditDialogDelegate RequestBulkEditDialogDelegate;

	// A delegate to ask the host / client for a dialog to confirm a bulk edit
	FRigVMController_RequestBreakLinksDialogDelegate RequestBreakLinksDialogDelegate;

	// A delegate to ask the host / client for a dialog to select a pin type
	FRigVMController_RequestPinTypeSelectionDelegate RequestPinTypeSelectionDelegate;

	// A delegate to inform the host / client about the progress during a bulk edit
	FRigVMController_OnBulkEditProgressDelegate OnBulkEditProgressDelegate;

	// A delegate to request the client to follow a hyper link
	FRigVMController_RequestJumpToHyperlinkDelegate RequestJumpToHyperlinkDelegate;

	// A delegate to request to configure an options instance for a node workflow
	FRigVMController_ConfigureWorkflowOptionsDelegate ConfigureWorkflowOptionsDelegate; 

	void AddPinRedirector(bool bInput, bool bOutput, const FString& OldPinPath, const FString& NewPinPath);
	void ClearPinRedirectors();

	// Removes nodes which went stale.
	void RemoveStaleNodes();

#if WITH_EDITOR
	bool ShouldRedirectPin(UScriptStruct* InOwningStruct, const FString& InOldRelativePinPath, FString& InOutNewRelativePinPath) const;
	bool ShouldRedirectPin(const FString& InOldPinPath, FString& InOutNewPinPath) const;


	struct FRepopulatePinsNodeData
	{
		URigVMNode* Node = nullptr;
		uint32 PreviousPinHash = 0;
		FRigVMPinInfoArray PreviousPinInfos;
		FRigVMPinInfoArray NewPinInfos;
		TArray<int32> NewPinsToAdd;
		TArray<int32> PreviousPinsToRemove;
		TArray<int32> PreviousPinsToOrphan;
		TArray<int32> PreviousPinsToUpdate;
		bool bSetupOrphanPinsForThisNode = false;
		bool bFollowCoreRedirectors = false;
		bool bRequirePinStates = false;
		bool bRecreateLinks = false;
		bool bRequireRecreateLinks = false;
	};

	void GenerateRepopulatePinsNodeData(TArray<FRepopulatePinsNodeData>& NodesPinData, URigVMNode* InNode, bool bInFollowCoreRedirectors = true, bool bInSetupOrphanedPins = false, bool bInRecreateLinks = false);
	void OrphanPins(const TArray<FRepopulatePinsNodeData>& NodesPinData);
	void RepopulatePins(const TArray<FRepopulatePinsNodeData>& NodesPinData);
	bool CorrectExecutePinsOnNode(URigVMNode* InOutNode);

	void RepopulatePinsOnNode(URigVMNode* InNode, bool bFollowCoreRedirectors = true, bool bSetupOrphanedPins = false, bool bRecreateLinks = false);
	bool GenerateNewPinInfos(const FRigVMRegistry& Registry, URigVMNode* InNode, const FRigVMPinInfoArray& PreviousPinInfos, FRigVMPinInfoArray& NewPinInfos, const bool bSetupOrphanPinsForThisNode);
	void GenerateRepopulatePinLists(const FRigVMRegistry& Registry, FRepopulatePinsNodeData& NodeData);
	void RepopulatePinsOnNode(const FRigVMRegistry& Registry, const FRepopulatePinsNodeData& NodeData);
	void RemovePinsDuringRepopulate(URigVMNode* InNode, TArray<URigVMPin*>& InPins, bool bSetupOrphanedPins);

	// removes any orphan pins that no longer holds a link
	// @param bRelayLinks If true we'll try to relay the links back to their original pins 
	bool RemoveUnusedOrphanedPins(URigVMNode* InNode, bool bRelayLinks = false);

	// Update filtered permutations, and propagate both ways of the link before adding this link
	bool PrepareToLink(URigVMPin* FirstToResolve, URigVMPin* SecondToResolve, bool bSetupUndoRedo);

#endif

	bool FullyResolveTemplateNode(URigVMTemplateNode* InNode, int32 InPermutationIndex, bool bSetupUndoRedo);

	FRigVMUnitNodeCreatedContext& GetUnitNodeCreatedContext() { return UnitNodeCreatedContext; }

	// Wires the unit node delegates to the default controller delegates.
	// this is used only within the RigVM Editor currently.
	void SetupDefaultUnitNodeDelegates(TDelegate<FName(FRigVMExternalVariable, FString)> InCreateExternalVariableDelegate);
	void ResetUnitNodeDelegates();

	// A flag that can be used to turn off pin default value validation if necessary
	bool bValidatePinDefaults;

	const FRigVMByteCode* GetCurrentByteCode() const;

	void ReportInfo(const FString& InMessage) const;
	void ReportWarning(const FString& InMessage) const;
	void ReportError(const FString& InMessage) const;
	void ReportAndNotifyInfo(const FString& InMessage) const;
	void ReportAndNotifyWarning(const FString& InMessage) const;
	void ReportAndNotifyError(const FString& InMessage) const;
	void ReportPinTypeChange(URigVMPin* InPin, const FString& InNewCPPType);
	void SendUserFacingNotification(const FString& InMessage, float InDuration = 0.f, const UObject* InSubject = nullptr, const FName& InBrushName = TEXT("MessageLog.Warning")) const;

	template <typename FmtType, typename... Types>
	void ReportInfof(const FmtType& Fmt, Types... Args) const
	{
		ReportInfo(FString::Printf(Fmt, Args...));
	}

	template <typename FmtType, typename... Types>
	void ReportWarningf(const FmtType& Fmt, Types... Args) const
	{
		ReportWarning(FString::Printf(Fmt, Args...));
	}

	template <typename FmtType, typename... Types>
	void ReportErrorf(const FmtType& Fmt, Types... Args) const
	{
		ReportError(FString::Printf(Fmt, Args...));
	}

	template <typename FmtType, typename... Types>
	void ReportAndNotifyInfof(const FmtType& Fmt, Types... Args) const
	{
		ReportAndNotifyInfo(FString::Printf(Fmt, Args...));
	}

	template <typename FmtType, typename... Types>
	void ReportAndNotifyWarningf(const FmtType& Fmt, Types... Args) const
	{
		ReportAndNotifyWarning(FString::Printf(Fmt, Args...));
	}

	template <typename FmtType, typename... Types>
	void ReportAndNotifyErrorf(const FmtType& Fmt, Types... Args) const
	{
		ReportAndNotifyError(FString::Printf(Fmt, Args...));
	}

	/**
	 * Helper function to disable a series of checks that can be ignored during a unit test
	 */
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	void SetIsRunningUnitTest(bool bIsRunning);

private:

	UPROPERTY(BlueprintReadOnly, Category = RigVMController, meta = (ScriptName = "ModifiedEvent", AllowPrivateAccess = "true"))
	FRigVMGraphModifiedDynamicEvent ModifiedEventDynamic;

	FRigVMGraphModifiedEvent ModifiedEventStatic;
	void HandleModifiedEvent(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject);

	bool IsValidGraph() const;
	bool IsValidSchema() const;
	bool IsGraphEditable() const;
	bool IsValidNodeForGraph(const URigVMNode* InNode);
	bool IsValidPinForGraph(const URigVMPin* InPin);
	bool IsValidLinkForGraph(const URigVMLink* InLink);
	void AddPinsForStruct(UStruct* InStruct, URigVMNode* InNode, URigVMPin* InParentPin, ERigVMPinDirection InPinDirection, const FString& InDefaultValue, bool bAutoExpandArrays, const FRigVMPinInfoArray* PreviousPins = nullptr);
	void AddPinsForArray(FArrayProperty* InArrayProperty, URigVMNode* InNode, URigVMPin* InParentPin, ERigVMPinDirection InPinDirection, const TArray<FString>& InDefaultValues, bool bAutoExpandArrays);
	void AddPinsForTemplate(const FRigVMTemplate* InTemplate, const FRigVMTemplateTypeMap& InPinTypeMap, URigVMNode* InNode);
	void ConfigurePinFromProperty(FProperty* InProperty, URigVMPin* InOutPin, ERigVMPinDirection InPinDirection = ERigVMPinDirection::Invalid) const;
	void ConfigurePinFromPin(URigVMPin* InOutPin, URigVMPin* InPin, bool bCopyDisplayName = false);
	void ConfigurePinFromArgument(URigVMPin* InOutPin, const FRigVMGraphFunctionArgument& InArgument, bool bCopyDisplayName = false);
	bool SetPinDefaultValue(URigVMPin* InPin, const FString& InDefaultValue, bool bResizeArrays, bool bSetupUndoRedo, bool bMergeUndoAction, bool bSetValueOnLinkedPins = true);
	bool ResetPinDefaultValue(URigVMPin* InPin, bool bSetupUndoRedo);
	static FString GetPinInitialDefaultValue(const URigVMPin* InPin);
	static FString GetPinInitialDefaultValueFromStruct(UScriptStruct* ScriptStruct, const URigVMPin* InPin, uint32 InOffset);
	URigVMPin* InsertArrayPin(URigVMPin* ArrayPin, int32 InIndex, const FString& InDefaultValue, bool bSetupUndoRedo);
	bool RemovePin(URigVMPin* InPinToRemove, bool bSetupUndoRedo, bool bForceBreakLinks = false);
	FProperty* FindPropertyForPin(const FString& InPinPath);
	bool BindPinToVariable(URigVMPin* InPin, const FString& InNewBoundVariablePath, bool bSetupUndoRedo, const FString& InVariableNodeName = FString());
	bool UnbindPinFromVariable(URigVMPin* InPin, bool bSetupUndoRedo);
	bool MakeBindingsFromVariableNode(URigVMVariableNode* InNode, bool bSetupUndoRedo);
	bool PromotePinToVariable(URigVMPin* InPin, bool bCreateVariableNode, const FVector2D& InNodePosition, bool bSetupUndoRedo);
	URigVMInjectionInfo* InjectNodeIntoPin(const FString& InPinPath, bool bAsInput, const FName& InInputPinName, const FName& InOutputPinName, bool bSetupUndoRedo = true);
	URigVMInjectionInfo* InjectNodeIntoPin(URigVMPin* InPin, bool bAsInput, const FName& InInputPinName, const FName& InOutputPinName, bool bSetupUndoRedo = true);
	URigVMNode* EjectNodeFromPin(URigVMPin* InPin, bool bSetupUndoRedo = true, bool bPrintPythonCommands = false);
	bool EjectAllInjectedNodes(URigVMNode* InNode, bool bSetupUndoRedo = true, bool bPrintPythonCommands = false);
	FName AddDecorator(URigVMNode* InNode, UScriptStruct* InDecoratorScriptStruct, const FName& InDecoratorName, const FString& InDefaultValue, int32 InPinIndex = -1, bool bSetupUndoRedo = true);
	bool RemoveDecorator(URigVMNode* InNode, const FName& InDecoratorName, bool bSetupUndoRedo = true);

protected:

#if WITH_EDITOR
	URigVMUnitNode* AddUnitNode(UScriptStruct* InScriptStruct, TSubclassOf<URigVMUnitNode> InUnitNodeClass, const FName& InMethodName, const FVector2D& InPosition, const FString& InNodeName, bool bSetupUndoRedo, bool bPrintPythonCommand);
#endif

public:

	// try to reconnect source and target pins after a node deletion
	void RelinkSourceAndTargetPins(URigVMNode* RigNode, bool bSetupUndoRedo = true);

	bool AddLink(URigVMPin* OutputPin, URigVMPin* InputPin, bool bSetupUndoRedo = true, ERigVMPinDirection InUserDirection = ERigVMPinDirection::Invalid, bool bCreateCastNode = false, bool bIsRestoringLinks = false, FString* OutFailureReason = nullptr);
	bool BreakLink(URigVMPin* OutputPin, URigVMPin* InputPin, bool bSetupUndoRedo = true);
	bool BreakAllLinks(URigVMPin* Pin, bool bAsInput, bool bSetupUndoRedo = true);
	void EnableTypeCasting(bool bEnabled = true) { bEnableTypeCasting = bEnabled; }

private:
	bool BreakAllLinksRecursive(URigVMPin* Pin, bool bAsInput, bool bTowardsParent, bool bSetupUndoRedo);
	bool SetPinExpansion(URigVMPin* InPin, bool bIsExpanded, bool bSetupUndoRedo = true);
	void ExpandPinRecursively(URigVMPin* InPin, bool bSetupUndoRedo);
	bool SetPinIsWatched(URigVMPin* InPin, bool bIsWatched, bool bSetupUndoRedo);
	bool SetVariableName(URigVMVariableNode* InVariableNode, const FName& InVariableName, bool bSetupUndoRedo);
	static void ForEveryPinRecursively(URigVMPin* InPin, TFunction<void(URigVMPin*)> OnEachPinFunction);
	static void ForEveryPinRecursively(URigVMNode* InNode, TFunction<void(URigVMPin*)> OnEachPinFunction);
	URigVMCollapseNode* CollapseNodes(const TArray<URigVMNode*>& InNodes, const FString& InCollapseNodeName, bool bSetupUndoRedo, bool bIsAggregate);
	TArray<URigVMNode*> ExpandLibraryNode(URigVMLibraryNode* InNode, bool bSetupUndoRedo);
	URigVMFunctionReferenceNode* PromoteCollapseNodeToFunctionReferenceNode(URigVMCollapseNode* InCollapseNode, bool bSetupUndoRedo, const FString& InExistingFunctionDefinitionPath);
	URigVMCollapseNode* PromoteFunctionReferenceNodeToCollapseNode(URigVMFunctionReferenceNode* InFunctionRefNode, bool bSetupUndoRedo, bool bRemoveFunctionDefinition);
	void SetReferencedFunction(URigVMFunctionReferenceNode* InFunctionRefNode, URigVMLibraryNode* InNewReferencedNode, bool bSetupUndoRedo);

	void RefreshFunctionPins(URigVMNode* InNode, bool bSetupUndoRedo = false);

	void ReportRemovedLink(const FString& InSourcePinPath, const FString& InTargetPinPath, const FString& Reason = FString());

	struct FPinState
	{
		ERigVMPinDirection Direction;
		FString CPPType;
		UObject* CPPTypeObject;
		FString DefaultValue;
		bool bIsExpanded;
		TArray<URigVMInjectionInfo*> InjectionInfos;
		TArray<URigVMInjectionInfo::FWeakInfo> WeakInjectionInfos;
	};

	TMap<FString, FString> GetRedirectedPinPaths(URigVMNode* InNode) const;
	FPinState GetPinState(URigVMPin* InPin, bool bStoreWeakInjectionInfos = false) const;
	TMap<FString, FPinState> GetPinStates(URigVMNode* InNode, bool bStoreWeakInjectionInfos = false) const;
	void ApplyPinState(URigVMPin* InPin, const FPinState& InPinState, bool bSetupUndoRedo = false);
	void ApplyPinStates(URigVMNode* InNode, const TMap<FString, FPinState>& InPinStates, const TMap<FString, FString>& InRedirectedPinPaths = TMap<FString, FString>(), bool bSetupUndoRedo = false);


	static FLinearColor GetColorFromMetadata(const FString& InMetadata);
	static void CreateDefaultValueForStructIfRequired(UScriptStruct* InStruct, FString& InOutDefaultValue);
	static void PostProcessDefaultValue(URigVMPin* Pin, FString& OutDefaultValue);

	void ResolveTemplateNodeMetaData(URigVMTemplateNode* InNode, bool bSetupUndoRedo);

	// Changes Pin types if filtered types of a pin are unique
	bool UpdateTemplateNodePinTypes(URigVMTemplateNode* InNode, bool bSetupUndoRedo, bool bInitializeDefaultValue = true, TMap<URigVMPin*, TArray<TRigVMTypeIndex>> ProposedTypes = TMap<URigVMPin*, TArray<TRigVMTypeIndex>>());

	bool ChangePinType(const FString& InPinPath, const FString& InCPPType, const FName& InCPPTypeObjectPath, bool bSetupUndoRedo, bool bSetupOrphanPins = true, bool bBreakLinks = true, bool bRemoveSubPins = true, bool bInitializeDefaultValue = true);
	bool ChangePinType(URigVMPin* InPin, const FString& InCPPType, const FName& InCPPTypeObjectPath, bool bSetupUndoRedo, bool bSetupOrphanPins = true, bool bBreakLinks = true, bool bRemoveSubPins = true, bool bInitializeDefaultValue = true);
	bool ChangePinType(URigVMPin* InPin, const FString& InCPPType,UObject* InCPPTypeObject, bool bSetupUndoRedo, bool bSetupOrphanPins = true, bool bBreakLinks = true, bool bRemoveSubPins = true, bool bInitializeDefaultValue = true);
	bool ChangePinType(URigVMPin* InPin, TRigVMTypeIndex InTypeIndex, bool bSetupUndoRedo, bool bSetupOrphanPins = true, bool bBreakLinks = true, bool bRemoveSubPins = true, bool bInitializeDefaultValue = true);

#if WITH_EDITOR
	void RewireLinks(URigVMPin* OldPin, URigVMPin* NewPin, bool bAsInput, bool bSetupUndoRedo, TArray<URigVMLink*> InLinks = TArray<URigVMLink*>());
#endif


	bool RenameObject(UObject* InObjectToRename, const TCHAR* InNewName, UObject* InNewOuter = nullptr) const;
	void DestroyObject(UObject* InObjectToDestroy) const ;
	static URigVMPin* MakeExecutePin(URigVMNode* InNode, const FName& InName);
	static bool MakeExecutePin(URigVMPin* InOutPin);
	bool AddGraphNode(URigVMNode* InNode, bool bNotify);
	static void AddNodePin(URigVMNode* InNode, URigVMPin* InPin);
	static void AddSubPin(URigVMPin* InParentPin, URigVMPin* InPin);
	static bool EnsurePinValidity(URigVMPin* InPin, bool bRecursive);
	static void ValidatePin(URigVMPin* InPin);

	// recreate the CPP type strings for variables that reference a type object
	// they can get out of sync when the variable references a user defined struct
	void EnsureLocalVariableValidity();
	
	FRigVMExternalVariable GetVariableByName(const FName& InExternalVariableName, const bool bIncludeInputArguments = false) const;
	TArray<FRigVMExternalVariable> GetAllVariables(const bool bIncludeInputArguments = false) const;

	void RefreshFunctionReferences(URigVMLibraryNode* InFunctionDefinition, bool bSetupUndoRedo, bool bLoadIfNecessary);

public:

	struct FLinkedPath
	{
		FLinkedPath()
			: bSourceNodeIsInjected(false)
			, bTargetNodeIsInjected(false)
		{}

		FLinkedPath(URigVMLink* InLink);

		URigVMGraph* GetGraph(URigVMGraph* InGraph = nullptr) const;
		FString GetPinPathRepresentation() const;
		URigVMPin* GetSourcePin(URigVMGraph* InGraph = nullptr) const;
		URigVMPin* GetTargetPin(URigVMGraph* InGraph = nullptr) const;

		friend uint32 GetTypeHash(const FLinkedPath& InPath);

		bool operator ==(const FLinkedPath& InOther) const { return GetTypeHash(*this) == GetTypeHash(InOther); }
		bool operator !=(const FLinkedPath& InOther) const { return !(*this == InOther); }
		
		TSoftObjectPtr<URigVMGraph> GraphPtr;
		FString SourcePinPath;
		FString TargetPinPath;
		bool bSourceNodeIsInjected;
		bool bTargetNodeIsInjected;
	};

	struct FRestoreLinkedPathSettings
	{
		FRestoreLinkedPathSettings()
			: bFollowCoreRedirectors(false)
			, bRelayToOrphanPins(false)
			, bIsImportingFromText(false)
			, UserDirection(ERigVMPinDirection::Invalid)
		{}

		bool bFollowCoreRedirectors;
		bool bRelayToOrphanPins;
		bool bIsImportingFromText;
		ERigVMPinDirection UserDirection;
		TMap<FString, FString> NodeNameMap;
		TMap<FString,FRigVMController_PinPathRemapDelegate> RemapDelegates;
		FRigVMController_CheckPinCompatibilityDelegate CompatibilityDelegate;
	};

	TArray<FLinkedPath> GetLinkedPaths() const;
	static TArray<FLinkedPath> GetLinkedPaths(const TArray<URigVMLink*>& InLinks);
	static TArray<FLinkedPath> GetLinkedPaths(URigVMNode* InNode, bool bIncludeInjectionNodes = false);
	static TArray<FLinkedPath> GetLinkedPaths(const TArray<URigVMNode*>& InNodes, bool bIncludeInjectionNodes = false);
	static TArray<FLinkedPath> GetLinkedPaths(const URigVMPin* InPin, bool bSourceLinksRecursive = false, bool bTargetLinksRecursive = false);
	bool BreakLinkedPaths(const TArray<FLinkedPath>& InLinkedPaths, bool bSetupUndoRedo = false, bool bRelyOnBreakLink = true);
	bool RestoreLinkedPaths(
		const TArray<FLinkedPath>& InLinkedPaths,
		const FRestoreLinkedPathSettings& InSettings = FRestoreLinkedPathSettings(),
		bool bSetupUndoRedo = false);
	TArray<FLinkedPath> RemapLinkedPaths(
		const TArray<FLinkedPath>& InLinkedPaths,
		const FRestoreLinkedPathSettings& InSettings = FRestoreLinkedPathSettings(),
		bool bSetupUndoRedo = false);
	bool FastBreakLinkedPaths(const TArray<FLinkedPath>& InLinkedPaths, bool bSetupUndoRedo = false);
	URigVMLink* FindLinkFromPinPathRepresentation(const FString& InPinPathRepresentation, bool bLookForDetachedLink) const;
	void ProcessDetachedLinks(const FRestoreLinkedPathSettings& InSettings = FRestoreLinkedPathSettings());

#if WITH_EDITOR
	// Registers this template node's use for later determining the commonly used types
	void RegisterUseOfTemplate(const URigVMTemplateNode* InNode);

	// Inquire on the commonly used types for a template node. This can be used to resolve a node without user input (as a default)
	FRigVMTemplate::FTypeMap GetCommonlyUsedTypesForTemplate(const URigVMTemplateNode* InNode) const;
#endif

	URigVMActionStack* GetActionStack() const;
	void SetActionStack(URigVMActionStack* InActionStack);

	URigVMNode* ConvertRerouteNodeToDispatch(URigVMRerouteNode* InRerouteNode, const FName& InTemplateNotation, bool bSetupUndoRedo, bool bPrintPythonCommand);

protected:

	URigVMPin* CreatePinFromPinInfo(const FRigVMRegistry& InRegistry, const FRigVMPinInfoArray& InPreviousPinInfos, const FRigVMPinInfo& InPinInfo, const FString& InPinPath, UObject* InOuter) const;

	// backwards compatibility code
	FRigVMClientPatchResult PatchRerouteNodesOnLoad();
	FRigVMClientPatchResult PatchUnitNodesOnLoad();
	FRigVMClientPatchResult PatchDispatchNodesOnLoad();
	FRigVMClientPatchResult PatchBranchNodesOnLoad();
	FRigVMClientPatchResult PatchIfSelectNodesOnLoad();
	FRigVMClientPatchResult PatchArrayNodesOnLoad();
	FRigVMClientPatchResult PatchReduceArrayFloatDoubleConvertsionsOnLoad();
	FRigVMClientPatchResult PatchInvalidLinksOnWildcards();
	FRigVMClientPatchResult PatchFunctionsWithInvalidReturnPaths();
	FRigVMClientPatchResult PatchExecutePins();
	FRigVMClientPatchResult PatchLazyPins();

	// work to do after a duplication of the host asset
	void PostDuplicateHost(const FString& InOldPathName, const FString& InNewPathName);

	template<typename T>
	static void SortGraphElementsByGraphDepth(TArray<T*>& InOutElements, bool bReverse = false)
	{
		if(InOutElements.IsEmpty())
		{
			return;
		}

		int32 MinDepth = InOutElements[0]->GetGraphDepth();
		int32 MaxDepth = MinDepth;
		int32 Increment = 1;
		TMap<int32, TArray<T*>> ElementsPerDepth;
		for(T* Element : InOutElements)
		{
			const int32 Depth = Element->GetGraphDepth(); 
			ElementsPerDepth.FindOrAdd(Depth).Add(Element);
			MinDepth = FMath::Min<int32>(MinDepth, Depth);
			MaxDepth = FMath::Max<int32>(MaxDepth, Depth);
		}

		if(bReverse)
		{
			Swap(MinDepth, MaxDepth);
			Increment = -1;
		}
		
		InOutElements.Reset();
		for(int32 Depth = MinDepth; /* no exit condition */; Depth += Increment)
		{
			if(const TArray<T*>* Elements = ElementsPerDepth.Find(Depth))
			{
				InOutElements.Append(*Elements);
			}

			if(Depth == MaxDepth)
			{
				break;
			}
		}
	}

	template<typename T>
	static void SortGraphElementsByImportOrder(
		TArray<TObjectPtr<T>> InOutElements,
		const TArray<T*>& InElementsInImportOrder,
		const TArray<T*>& InPreviousElementPriorToImport
	) {
		Algo::SortBy(InOutElements, [InElementsInImportOrder, InPreviousElementPriorToImport](T* Element) -> int32
		{
			const int32 ImportOrderIndex = InElementsInImportOrder.Find(Element);
			if(ImportOrderIndex != INDEX_NONE)
			{
				return ImportOrderIndex + InPreviousElementPriorToImport.Num();
			}
			return InPreviousElementPriorToImport.Find(Element);
		});
	}
	
private: 
	UPROPERTY(transient)
	TArray<TObjectPtr<URigVMGraph>> Graphs;

	UPROPERTY(transient, DuplicateTransient)
	TObjectPtr<URigVMSchema> SchemaPtr;

	mutable TWeakObjectPtr<URigVMActionStack> WeakActionStack;
	mutable FDelegateHandle ActionStackHandle;

	bool bSuspendNotifications;
	bool bReportWarningsAndErrors;
	bool bIgnoreRerouteCompactnessChanges;
	ERigVMPinDirection UserLinkDirection;
	bool bEnableTypeCasting;
	bool bAllowPrivateFunctions;

	// temporary maps used for pin redirection
	// only valid between Detach & ReattachLinksToPinObjects
	TMap<FString, FString> InputPinRedirectors;
	TMap<FString, FString> OutputPinRedirectors;

	struct FRigVMStructPinRedirectorKey
	{
		FRigVMStructPinRedirectorKey()
		{
		}

		FRigVMStructPinRedirectorKey(UScriptStruct* InScriptStruct, const FString& InPinPathInNode)
		: Struct(InScriptStruct)
		, PinPathInNode(InPinPathInNode)
		{
		}

		friend uint32 GetTypeHash(const FRigVMStructPinRedirectorKey& Cache)
		{
			return HashCombine(GetTypeHash(Cache.Struct), GetTypeHash(Cache.PinPathInNode));
		}

		bool operator ==(const FRigVMStructPinRedirectorKey& Other) const
		{
			return Struct == Other.Struct && PinPathInNode == Other.PinPathInNode;
		}

		bool operator !=(const FRigVMStructPinRedirectorKey& Other) const
		{
			return Struct != Other.Struct || PinPathInNode != Other.PinPathInNode;
		}

		UScriptStruct* Struct;
		FString PinPathInNode;
	};

	static TMap<FRigVMStructPinRedirectorKey, FString> PinPathCoreRedirectors;
	FCriticalSection PinPathCoreRedirectorsLock;

	FRigVMUnitNodeCreatedContext UnitNodeCreatedContext;

	bool bIsTransacting; // Performing undo/redo transaction
	bool bIsRunningUnitTest;
	bool bIsFullyResolvingTemplateNode;

public:
	
	bool bSuspendTemplateComputation;

private:

#if WITH_EDITOR
	bool bRegisterTemplateNodeUsage;
#endif

	bool bEnableSchemaRemoveNodeCheck;

	friend class URigVMGraph;
	friend class URigVMPin;
	friend class URigVMActionStack;
	friend struct FRigVMBaseAction;
	friend class URigVMCompiler;
	friend struct FRigVMControllerObjectFactory;
	friend struct FRigVMAddRerouteNodeAction;
	friend struct FRigVMChangePinTypeAction;
	friend struct FRigVMInjectNodeIntoPinAction;
	friend struct FRigVMEjectNodeFromPinAction;
	friend class FRigVMParserAST;
	friend class FRigVMControllerCompileBracketScope;
	friend struct FRigVMPinInfoArray;
	friend class FRigVMControllerNotifGuard;
	friend struct FRigVMClient;
	friend struct FRigVMActionWrapper;
	friend class URigVMSchema;
	friend class URigVMEdGraphFunctionRefNodeSpawner;
};

class FRigVMControllerNotifGuard
{
public:

	FRigVMControllerNotifGuard(URigVMController* InController, bool bInSuspendNotifications = true)
		: Controller(InController)
	{
		bPreviousSuspendNotifications = Controller->bSuspendNotifications;
		Controller->bSuspendNotifications = bInSuspendNotifications;
	}

	~FRigVMControllerNotifGuard()
	{
		Controller->bSuspendNotifications = bPreviousSuspendNotifications;
	}

private:

	URigVMController* Controller;
	bool bPreviousSuspendNotifications;
};

USTRUCT()
struct FRigVMController_CommonTypePerTemplate
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "RigVMController")
	TMap<FString,int32> Counts;
};

/**
 * Default settings for the RigVM Controller
 */
UCLASS(config = EditorSettings)
class  URigVMControllerSettings : public UObject
{
public:
	URigVMControllerSettings(const FObjectInitializer& Initializer);

	GENERATED_BODY()

	/**
	 * When adding a link to an execute pin on a template node,
	 * this functionality automatically resolves the template node to the
	 * most commonly used type.
	 */
	UPROPERTY(EditAnywhere, Category = "RigVMController")
	bool bAutoResolveTemplateNodesWhenLinkingExecute;

	/** The commonly used types for a template node */
	UPROPERTY()
	TMap<FName,FRigVMController_CommonTypePerTemplate> TemplateDefaultTypes;
};