// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMGraph.h"
#include "RigVMFunctionLibrary.h"
#include "RigVMModel/Nodes/RigVMUnitNode.h"
#include "RigVMModel/Nodes/RigVMVariableNode.h"
#include "RigVMModel/Nodes/RigVMParameterNode.h"
#include "RigVMModel/Nodes/RigVMCommentNode.h"
#include "RigVMModel/Nodes/RigVMRerouteNode.h"
#include "RigVMModel/Nodes/RigVMBranchNode.h"
#include "RigVMModel/Nodes/RigVMIfNode.h"
#include "RigVMModel/Nodes/RigVMSelectNode.h"
#include "RigVMModel/Nodes/RigVMTemplateNode.h"
#include "RigVMModel/Nodes/RigVMEnumNode.h"
#include "RigVMModel/Nodes/RigVMCollapseNode.h"
#include "RigVMModel/Nodes/RigVMFunctionReferenceNode.h"
#include "RigVMModel/Nodes/RigVMArrayNode.h"
#include "RigVMModel/Nodes/RigVMInvokeEntryNode.h"
#include "RigVMModel/RigVMBuildData.h"
#include "RigVMCore/RigVMUserWorkflow.h"
#include "UObject/Interface.h"
#include "RigVMController.generated.h"

#ifndef UE_RIGVM_ENABLE_TEMPLATE_NODES
#define UE_RIGVM_ENABLE_TEMPLATE_NODES 1
#endif

class URigVMActionStack;

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
DECLARE_DELEGATE_RetVal_OneParam(bool, FRigVMController_IsFunctionAvailableDelegate, URigVMLibraryNode*)
DECLARE_DELEGATE_RetVal_OneParam(bool, FRigVMController_RequestLocalizeFunctionDelegate, URigVMLibraryNode*)
DECLARE_DELEGATE_RetVal_ThreeParams(FName, FRigVMController_RequestNewExternalVariableDelegate, FRigVMGraphVariableDescription, bool, bool);
DECLARE_DELEGATE_RetVal_TwoParams(bool, FRigVMController_IsDependencyCyclicDelegate, UObject*, UObject*)
DECLARE_DELEGATE_RetVal_TwoParams(FRigVMController_BulkEditResult, FRigVMController_RequestBulkEditDialogDelegate, URigVMLibraryNode*, ERigVMControllerBulkEditType)
DECLARE_DELEGATE_RetVal_OneParam(bool, FRigVMController_RequestBreakLinksDialogDelegate, TArray<URigVMLink*>)
DECLARE_DELEGATE_FiveParams(FRigVMController_OnBulkEditProgressDelegate, TSoftObjectPtr<URigVMFunctionReferenceNode>, ERigVMControllerBulkEditType, ERigVMControllerBulkEditProgress, int32, int32)
DECLARE_DELEGATE_RetVal_TwoParams(FString, FRigVMController_PinPathRemapDelegate, const FString& /* InPinPath */, bool /* bIsInput */);
DECLARE_DELEGATE_OneParam(FRigVMController_RequestJumpToHyperlinkDelegate, const UObject* InSubject);
DECLARE_DELEGATE_OneParam(FRigVMController_ConfigureWorkflowOptionsDelegate, URigVMUserWorkflowOptions* InOutOptions);
DECLARE_DELEGATE_RetVal_TwoParams(bool, FRigVMController_CheckPinComatibilityDelegate, URigVMPin*, URigVMPin*);

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
		typename TEnableIf<TModels<CRigVMUStruct, T>::Value>::Type * = nullptr
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
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	void SetGraph(URigVMGraph* InGraph);

	// Pushes a new graph to the stack
	// This causes a GraphChanged modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	void PushGraph(URigVMGraph* InGraph, bool bSetupUndoRedo = true);

	// Pops the last graph off the stack
	// This causes a GraphChanged modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	URigVMGraph* PopGraph(bool bSetupUndoRedo = true);
	
	// Returns the top level graph
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	URigVMGraph* GetTopLevelGraph() const;

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

	UFUNCTION(BlueprintCallable, Category = RigVMController)
	TArray<FString> GeneratePythonCommands();

	TArray<FString> GetAddNodePythonCommands(URigVMNode* Node) const;

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
		typename TEnableIf<TModels<CRigVMUStruct, T>::Value>::Type * = nullptr
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
	bool UnresolveTemplateNodes(const TArray<URigVMNode*>& InNodes, bool bSetupUndoRedo, bool bBreakLinks = true);

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
	URigVMRerouteNode* AddRerouteNodeOnLink(URigVMLink* InLink, bool bShowAsFullNode, const FVector2D& InPosition = FVector2D::ZeroVector, const FString& InNodeName = TEXT(""), bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Adds a Reroute Node on an existing Link to the edited Graph given the Link's string representation.
	// Reroute Nodes can be used to visually improve the data flow,
	// they don't require any additional memory though and are purely
	// cosmetic. This causes a NodeAdded modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	URigVMRerouteNode* AddRerouteNodeOnLinkPath(const FString& InLinkPinPathRepresentation, bool bShowAsFullNode, const FVector2D& InPosition = FVector2D::ZeroVector, const FString& InNodeName = TEXT(""), bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Adds a Reroute Node on an existing Pin to the editor Graph.
	// Reroute Nodes can be used to visually improve the data flow,
	// they don't require any additional memory though and are purely
	// cosmetic. This causes a NodeAdded modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	URigVMRerouteNode* AddRerouteNodeOnPin(const FString& InPinPath, bool bAsInput, bool bShowAsFullNode, const FVector2D& InPosition = FVector2D::ZeroVector, const FString& InNodeName = TEXT(""), bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Adds a free Reroute Node
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	URigVMRerouteNode* AddFreeRerouteNode(bool bShowAsFullNode, const FString& InCPPType, const FName& InCPPTypeObjectPath, bool bIsConstant, const FName& InCustomWidgetName, const FString& InDefaultValue, const FVector2D& InPosition = FVector2D::ZeroVector, const FString& InNodeName = TEXT(""), bool bSetupUndoRedo = true);

	// Adds a branch node to the graph.
	// Branch nodes can be used to split the execution of into multiple branches,
	// allowing to drive behavior by logic.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	URigVMBranchNode* AddBranchNode(const FVector2D& InPosition = FVector2D::ZeroVector, const FString& InNodeName = TEXT(""), bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Adds an if node to the graph.
	// If nodes can be used to pick between two values based on a condition.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	URigVMIfNode* AddIfNode(const FString& InCPPType, const FName& InCPPTypeObjectPath, const FVector2D& InPosition = FVector2D::ZeroVector, const FString& InNodeName = TEXT(""), bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	URigVMIfNode* AddIfNodeFromStruct(UScriptStruct* InScriptStruct, const FVector2D& InPosition = FVector2D::ZeroVector, const FString& InNodeName = TEXT(""), bool bSetupUndoRedo = true);

	// Adds a select node to the graph.
	// Select nodes can be used to pick between multiple values based on an index.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	URigVMSelectNode* AddSelectNode(const FString& InCPPType, const FName& InCPPTypeObjectPath, const FVector2D& InPosition = FVector2D::ZeroVector, const FString& InNodeName = TEXT(""), bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	URigVMSelectNode* AddSelectNodeFromStruct(UScriptStruct* InScriptStruct, const FVector2D& InPosition = FVector2D::ZeroVector, const FString& InNodeName = TEXT(""), bool bSetupUndoRedo = true);

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
	URigVMArrayNode* AddArrayNode(ERigVMOpCode InOpCode, const FString& InCPPType, UObject* InCPPTypeObject, const FVector2D& InPosition = FVector2D::ZeroVector, const FString& InNodeName = TEXT(""), bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Adds a Array Node to the edited Graph given a struct object path name.
	// This causes a NodeAdded modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	URigVMArrayNode* AddArrayNodeFromObjectPath(ERigVMOpCode InOpCode, const FString& InCPPType, const FString& InCPPTypeObjectPath, const FVector2D& InPosition = FVector2D::ZeroVector, const FString& InNodeName = TEXT(""), bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Adds an entry invocation node
	// This causes a NodeAdded modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	URigVMInvokeEntryNode* AddInvokeEntryNode(const FName& InEntryName, const FVector2D& InPosition = FVector2D::ZeroVector, const FString& InNodeName = TEXT(""), bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

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
	FString ExportNodesToText(const TArray<FName>& InNodeNames);

	// Exports the selected nodes as text
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	FString ExportSelectedNodesToText();

	// Exports the given nodes as text
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool CanImportNodesFromText(const FString& InText);

	// Exports the given nodes as text
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	TArray<FName> ImportNodesFromText(const FString& InText, bool bSetupUndoRedo = true, bool bPrintPythonCommands = false);

	// Copies a function declaration into this graph's local function library
	UFUNCTION(BlueprintCallable, Category = RigVMController)
    URigVMLibraryNode* LocalizeFunction(
    	URigVMLibraryNode* InFunctionDefinition,
		bool bLocalizeDependentPrivateFunctions = true,
    	bool bSetupUndoRedo = true,
    	bool bPrintPythonCommand = false);

	// Copies a series of function declaratioms into this graph's local function library
	UFUNCTION(BlueprintCallable, Category = RigVMController)
    TMap<URigVMLibraryNode*, URigVMLibraryNode*> LocalizeFunctions(
        TArray<URigVMLibraryNode*> InFunctionDefinitions,
        bool bLocalizeDependentPrivateFunctions = true,
        bool bSetupUndoRedo = true,
        bool bPrintPythonCommand = false);

	// Returns a unique name
	static FName GetUniqueName(const FName& InName, TFunction<bool(const FName&)> IsNameAvailableFunction, bool bAllowPeriod, bool bAllowSpace);

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
	bool RemoveNode(URigVMNode* InNode, bool bSetupUndoRedo = true, bool bRecursive = false, bool bPrintPythonCommand = false, bool bRelinkPins = false);

	// Removes a node from the graph given the node's name.
	// This causes a NodeRemoved modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool RemoveNodeByName(const FName& InNodeName, bool bSetupUndoRedo = true, bool bRecursive = false, bool bPrintPythonCommand = false, bool bRelinkPins = false);
	
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

	// Sets the compactness of a reroute node in the graph.
	// This causes a RerouteCompactnessChanged modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool SetRerouteCompactness(URigVMNode* InNode, bool bShowAsFullNode, bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Sets the compactness of a reroute node in the graph by name.
	// This causes a RerouteCompactnessChanged modified event.
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool SetRerouteCompactnessByName(const FName& InNodeName, bool bShowAsFullNode, bool bSetupUndoRedo = true);

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
	bool SetPinDefaultValue(const FString& InPinPath, const FString& InDefaultValue, bool bResizeArrays = true, bool bSetupUndoRedo = true, bool bMergeUndoAction = false, bool bPrintPythonCommand = false);

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
	bool AddLink(const FString& InOutputPinPath, const FString& InInputPinPath, bool bSetupUndoRedo = true, bool bPrintPythonCommand = false, ERigVMPinDirection InUserDirection = ERigVMPinDirection::Output);

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
	bool SetLocalVariableDefaultValue(const FName& InVariableName, const FString& InDefaultValue, bool bSetupUndoRedo = true, bool bPrintPythonCommand = false, bool bNotify = true);

	// creates the options struct for a given workflow
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	URigVMUserWorkflowOptions* MakeOptionsForWorkflow(UObject* InSubject, const FRigVMUserWorkflow& InWorkflow);

	// performs all actions representing the workflow
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	bool PerformUserWorkflow(const FRigVMUserWorkflow& InWorkflow, const URigVMUserWorkflowOptions* InOptions, bool bSetupUndoRedo = true);

	// Determine affected function references for a potential bulk edit on a library node
	TArray<TSoftObjectPtr<URigVMFunctionReferenceNode>> GetAffectedReferences(ERigVMControllerBulkEditType InEditType, bool bForceLoad = false, bool bNotify = true);

	// Determine affected assets for a potential bulk edit on a library node
	TArray<FAssetData> GetAffectedAssets(ERigVMControllerBulkEditType InEditType, bool bForceLoad = false, bool bNotify = true);

	// A delegate that can be set to change the struct unfolding behaviour
	FRigVMController_ShouldStructUnfoldDelegate UnfoldStructDelegate;

	// A delegate to retrieve the list of external variables
	FRigVMController_GetExternalVariablesDelegate GetExternalVariablesDelegate;

	// A delegate to retrieve the current bytecode of the graph
	FRigVMController_GetByteCodeDelegate GetCurrentByteCodeDelegate;

	// A delegate to determine if a function is public
	FRigVMController_IsFunctionAvailableDelegate IsFunctionAvailableDelegate;

	// A delegate to localize a function on demand
	FRigVMController_RequestLocalizeFunctionDelegate RequestLocalizeFunctionDelegate;

	// A delegate to create a new blueprint member variable
	FRigVMController_RequestNewExternalVariableDelegate RequestNewExternalVariableDelegate;
	
	// A delegate to validate if we are allowed to introduce a dependency between two objects
	FRigVMController_IsDependencyCyclicDelegate IsDependencyCyclicDelegate;

	// A delegate to ask the host / client for a dialog to confirm a bulk edit
	FRigVMController_RequestBulkEditDialogDelegate RequestBulkEditDialogDelegate;

	// A delegate to ask the host / client for a dialog to confirm a bulk edit
	FRigVMController_RequestBreakLinksDialogDelegate RequestBreakLinksDialogDelegate;

	// A delegate to inform the host / client about the progress during a bulk edit
	FRigVMController_OnBulkEditProgressDelegate OnBulkEditProgressDelegate;

	// A delegate to request the client to follow a hyper link
	FRigVMController_RequestJumpToHyperlinkDelegate RequestJumpToHyperlinkDelegate;

	// A delegate to request to configure an options instance for a node workflow
	FRigVMController_ConfigureWorkflowOptionsDelegate ConfigureWorkflowOptionsDelegate; 

	// Returns the build data of the host
	static URigVMBuildData* GetBuildData(bool bCreateIfNeeded = true);

	int32 DetachLinksFromPinObjects(const TArray<URigVMLink*>* InLinks = nullptr, bool bNotify = false);
	int32 ReattachLinksToPinObjects(bool bFollowCoreRedirectors = false, const TArray<URigVMLink*>* InLinks = nullptr, bool bNotify = false, bool bSetupOrphanedPins = false, bool bAllowNonArgumentLinks = false);
	void AddPinRedirector(bool bInput, bool bOutput, const FString& OldPinPath, const FString& NewPinPath);

	// Removes nodes which went stale.
	void RemoveStaleNodes();

#if WITH_EDITOR
	bool ShouldRedirectPin(UScriptStruct* InOwningStruct, const FString& InOldRelativePinPath, FString& InOutNewRelativePinPath) const;
	bool ShouldRedirectPin(const FString& InOldPinPath, FString& InOutNewPinPath) const;

	void RepopulatePinsOnNode(URigVMNode* InNode, bool bFollowCoreRedirectors = true, bool bNotify = false, bool bSetupOrphanedPins = false);
	void RemovePinsDuringRepopulate(URigVMNode* InNode, TArray<URigVMPin*>& InPins, bool bNotify, bool bSetupOrphanedPins);

	// removes any orphan pins that no longer holds a link
	bool RemoveUnusedOrphanedPins(URigVMNode* InNode, bool bNotify);

	// Initializes and recomputes the filtered permutations of all template nodes in the graph
	// Returns true if any pin has change it's type or link was broken
	bool RecomputeAllTemplateFilteredPermutations(bool bSetupUndoRedo);

	// Update the template of a subgraph with the filtered permutations of the interface nodes
	bool UpdateLibraryTemplate(URigVMLibraryNode* LibraryNode, bool bSetupUndoRedo);

	// Update filtered permutations, and propagate both ways of the link before adding this link
	bool PrepareToLink(URigVMPin* FirstToResolve, URigVMPin* SecondToResolve, bool bSetupUndoRedo);

	// Try to initialize the filterd permutations from the pin types
	void InitializeFilteredPermutationsFromTemplateTypes();

	// Initializes filtered permuations to be unresolved in all template nodes in graph
	void InitializeAllTemplateFiltersInGraph(bool bSetupUndoRedo, bool bChangePinTypes);
	
#endif

	bool FullyResolveTemplateNode(URigVMTemplateNode* InNode, int32 InPermutationIndex, bool bSetupUndoRedo);

	FRigVMUnitNodeCreatedContext& GetUnitNodeCreatedContext() { return UnitNodeCreatedContext; }

	// Wires the unit node delegates to the default controller delegates.
	// this is used only within the Control Rig Editor currently.
	void SetupDefaultUnitNodeDelegates(TDelegate<FName(FRigVMExternalVariable, FString)> InCreateExternalVariableDelegate);
	void ResetUnitNodeDelegates();

	// A flag that can be used to turn off pin default value validation if necessary
	bool bValidatePinDefaults;

	// A flag to suspend the recomputation of filtered permutations of outer graphs
	bool bSuspendRecomputingOuterTemplateFilters;

	const FRigVMByteCode* GetCurrentByteCode() const;

	void ReportInfo(const FString& InMessage) const;
	void ReportWarning(const FString& InMessage) const;
	void ReportError(const FString& InMessage) const;
	void ReportAndNotifyInfo(const FString& InMessage) const;
	void ReportAndNotifyWarning(const FString& InMessage) const;
	void ReportAndNotifyError(const FString& InMessage) const;
	void SendUserFacingNotification(const FString& InMessage, float InDuration = 0.f, const UObject* InSubject = nullptr, const FName& InBrushName = TEXT("MessageLog.Warning")) const;

	template <typename FmtType, typename... Types>
	void ReportInfof(const FmtType& Fmt, Types... Args)
	{
		ReportInfo(FString::Printf(Fmt, Args...));
	}

	template <typename FmtType, typename... Types>
	void ReportWarningf(const FmtType& Fmt, Types... Args)
	{
		ReportWarning(FString::Printf(Fmt, Args...));
	}

	template <typename FmtType, typename... Types>
	void ReportErrorf(const FmtType& Fmt, Types... Args)
	{
		ReportError(FString::Printf(Fmt, Args...));
	}

	template <typename FmtType, typename... Types>
	void ReportAndNotifyInfof(const FmtType& Fmt, Types... Args)
	{
		ReportAndNotifyInfo(FString::Printf(Fmt, Args...));
	}

	template <typename FmtType, typename... Types>
	void ReportAndNotifyWarningf(const FmtType& Fmt, Types... Args)
	{
		ReportAndNotifyWarning(FString::Printf(Fmt, Args...));
	}

	template <typename FmtType, typename... Types>
	void ReportAndNotifyErrorf(const FmtType& Fmt, Types... Args)
	{
		ReportAndNotifyError(FString::Printf(Fmt, Args...));
	}

	/**
	 * Function to override the notification behavior and temporarily
	 * disable all notifications. Client code is responsible for calling
	 * SuspendNotifications(true) once all changes have been done.
	 */
	void SuspendNotifications(bool bSuspend) { bSuspendNotifications = bSuspend; }

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

	FString GetValidNodeName(const FString& InPrefix);
	bool IsValidGraph() const;
	bool IsGraphEditable() const;
	bool IsValidNodeForGraph(URigVMNode* InNode);
	bool IsValidPinForGraph(URigVMPin* InPin);
	bool IsValidLinkForGraph(URigVMLink* InLink);
	bool CanAddNode(URigVMNode* InNode, bool bReportErrors, bool bIgnoreFunctionEntryReturnNodes = false);
	TObjectPtr<URigVMNode> FindEventNode(const UScriptStruct* InScriptStruct) const;
	bool CanAddEventNode(UScriptStruct* InScriptStruct, const bool bReportErrors) const;
	bool CanAddFunctionRefForDefinition(URigVMLibraryNode* InFunctionDefinition, bool bReportErrors);
	void AddPinsForStruct(UStruct* InStruct, URigVMNode* InNode, URigVMPin* InParentPin, ERigVMPinDirection InPinDirection, const FString& InDefaultValue, bool bAutoExpandArrays, bool bNotify = false);
	void AddPinsForArray(FArrayProperty* InArrayProperty, URigVMNode* InNode, URigVMPin* InParentPin, ERigVMPinDirection InPinDirection, const TArray<FString>& InDefaultValues, bool bAutoExpandArrays);
	void ConfigurePinFromProperty(FProperty* InProperty, URigVMPin* InOutPin, ERigVMPinDirection InPinDirection = ERigVMPinDirection::Invalid);
	void ConfigurePinFromPin(URigVMPin* InOutPin, URigVMPin* InPin, bool bCopyDisplayName = false);
	virtual bool ShouldStructBeUnfolded(const UStruct* InStruct);
	virtual bool ShouldPinBeUnfolded(URigVMPin* InPin);
	bool SetPinDefaultValue(URigVMPin* InPin, const FString& InDefaultValue, bool bResizeArrays, bool bSetupUndoRedo, bool bMergeUndoAction, bool bNotify = true);
	bool ResetPinDefaultValue(URigVMPin* InPin, bool bSetupUndoRedo);
	static FString GetPinInitialDefaultValue(const URigVMPin* InPin);
	static FString GetPinInitialDefaultValueFromStruct(UScriptStruct* ScriptStruct, const URigVMPin* InPin, uint32 InOffset);
	URigVMPin* InsertArrayPin(URigVMPin* ArrayPin, int32 InIndex, const FString& InDefaultValue, bool bSetupUndoRedo);
	bool RemovePin(URigVMPin* InPinToRemove, bool bSetupUndoRedo, bool bNotify);
	FProperty* FindPropertyForPin(const FString& InPinPath);
	bool BindPinToVariable(URigVMPin* InPin, const FString& InNewBoundVariablePath, bool bSetupUndoRedo, const FString& InVariableNodeName = FString());
	bool UnbindPinFromVariable(URigVMPin* InPin, bool bSetupUndoRedo);
	bool MakeBindingsFromVariableNode(URigVMVariableNode* InNode, bool bSetupUndoRedo);
	bool PromotePinToVariable(URigVMPin* InPin, bool bCreateVariableNode, const FVector2D& InNodePosition, bool bSetupUndoRedo);
	URigVMInjectionInfo* InjectNodeIntoPin(const FString& InPinPath, bool bAsInput, const FName& InInputPinName, const FName& InOutputPinName, bool bSetupUndoRedo = true);
	URigVMInjectionInfo* InjectNodeIntoPin(URigVMPin* InPin, bool bAsInput, const FName& InInputPinName, const FName& InOutputPinName, bool bSetupUndoRedo = true);
	URigVMNode* EjectNodeFromPin(URigVMPin* InPin, bool bSetupUndoRedo = true, bool bPrintPythonCommands = false);
	bool EjectAllInjectedNodes(URigVMNode* InNode, bool bSetupUndoRedo = true, bool bPrintPythonCommands = false);

	// try to reconnect source and target pins after a node deletion
	void RelinkSourceAndTargetPins(URigVMNode* RigNode, bool bSetupUndoRedo = true);

public:
	bool AddLink(URigVMPin* OutputPin, URigVMPin* InputPin, bool bSetupUndoRedo = true, ERigVMPinDirection InUserDirection = ERigVMPinDirection::Invalid);
	bool BreakLink(URigVMPin* OutputPin, URigVMPin* InputPin, bool bSetupUndoRedo = true);
	bool BreakAllLinks(URigVMPin* Pin, bool bAsInput, bool bSetupUndoRedo = true);

private:
	bool BreakAllLinksRecursive(URigVMPin* Pin, bool bAsInput, bool bTowardsParent, bool bSetupUndoRedo);
	void UpdateRerouteNodeAfterChangingLinks(URigVMPin* PinChanged, bool bSetupUndoRedo = true);
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

	void RefreshFunctionPins(URigVMNode* InNode, bool bNotify = true);

	void ReportRemovedLink(const FString& InSourcePinPath, const FString& InTargetPinPath);

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
	
	// Prepare the graph for the change this template node is about to make
	// If any of the types is supported (without breaking any links), then the filtered permutations will be updated and the change will
	// propagate to other nodes in the graph.
	// If it is not supported, we will attempt to find and break any links that do not support this change
	bool PrepareTemplatePinForType(URigVMPin* InPin, const TArray<TRigVMTypeIndex>& InTypeIndices, bool bSetupUndoRedo);

	// Get filtered types for a wildcard node. If template node, that means just returning its filtered wildcard types, but if it's another type of node (select, if, rereoute), iterate
	// its connections to figure out the filtered types
	TArray<TRigVMTypeIndex> GetFilteredTypes(URigVMPin* InPin);
	
	// Updates the permutations allowed without having to break any links
	bool UpdateFilteredPermutations(URigVMPin* InPin, URigVMPin* InLinkedPin, bool bSetupUndoRedo);
	bool UpdateFilteredPermutations(URigVMPin* InPin, const TArray<TRigVMTypeIndex>& InTypeIndices, bool bSetupUndoRedo);
	bool UpdateFilteredPermutations(URigVMTemplateNode* InNode, const TArray<int32>& InPermutations, bool bSetupUndoRedo);

	// Changes Pin types if filtered types of a pin are unique
	bool UpdateTemplateNodePinTypes(URigVMTemplateNode* InNode, bool bSetupUndoRedo, bool bInitializeDefaultValue = true);

	// Reduces the filtered permutations of all templates in the graph to comply with the types filtered by InNode
	// Returns false if a link had to be broken
	bool PropagateTemplateFilteredTypes(URigVMTemplateNode* InNode, bool bSetupUndoRedo);

	// Adds a preferred type for the pin
	// Returns false if the pin already has a different type
	bool AddPreferredType(URigVMTemplateNode* InNode, const FName& InPinName, const TRigVMTypeIndex& InPreferredTypeIndex, bool bSetupUndoRedo);

	// Removes preferred type
	// Returns true if the preferred type was found and removed
	bool RemovePreferredType(URigVMTemplateNode* InNode, const FName& InPinName, bool bSetupUndoRedo);

	// Returns true if the pin is connected, and the filtered types is reduced (not infinite like reroute, if, select or array nodes)
	bool ShouldPinOwnArgument(URigVMPin* InPin);

	// Given a Entry or Return pin and a potential pin to link it to, try to add the first pin as an argument of the library's template
	bool AddArgumentForPin(URigVMPin* InPin, URigVMPin* InToLinkPin, bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	bool ChangePinType(const FString& InPinPath, const FString& InCPPType, const FName& InCPPTypeObjectPath, bool bSetupUndoRedo, bool bSetupOrphanPins = true, bool bBreakLinks = true, bool bRemoveSubPins = true, bool bInitializeDefaultValue = true);
	bool ChangePinType(URigVMPin* InPin, const FString& InCPPType, const FName& InCPPTypeObjectPath, bool bSetupUndoRedo, bool bSetupOrphanPins = true, bool bBreakLinks = true, bool bRemoveSubPins = true, bool bInitializeDefaultValue = true);
	bool ChangePinType(URigVMPin* InPin, const FString& InCPPType,UObject* InCPPTypeObject, bool bSetupUndoRedo, bool bSetupOrphanPins = true, bool bBreakLinks = true, bool bRemoveSubPins = true, bool bInitializeDefaultValue = true);
	bool ChangePinType(URigVMPin* InPin, TRigVMTypeIndex InTypeIndex, bool bSetupUndoRedo, bool bSetupOrphanPins = true, bool bBreakLinks = true, bool bRemoveSubPins = true, bool bInitializeDefaultValue = true);

#if WITH_EDITOR
	void RewireLinks(URigVMPin* OldPin, URigVMPin* NewPin, bool bAsInput, bool bSetupUndoRedo, TArray<URigVMLink*> InLinks = TArray<URigVMLink*>());
#endif


	bool RenameObject(UObject* InObjectToRename, const TCHAR* InNewName, UObject* InNewOuter = nullptr);
	void DestroyObject(UObject* InObjectToDestroy);
	static URigVMPin* MakeExecutePin(URigVMNode* InNode, const FName& InName);
	static void MakeExecutePin(URigVMPin* InOutPin);
	static void AddNodePin(URigVMNode* InNode, URigVMPin* InPin);
	static void AddSubPin(URigVMPin* InParentPin, URigVMPin* InPin);
	static bool EnsurePinValidity(URigVMPin* InPin, bool bRecursive);
	static void ValidatePin(URigVMPin* InPin);

	// recreate the CPP type strings for variables that reference a type object
	// they can get out of sync when the variable references a user defined struct
	void EnsureLocalVariableValidity();
	
	FRigVMExternalVariable GetVariableByName(const FName& InExternalVariableName, const bool bIncludeInputArguments = false);
	TArray<FRigVMExternalVariable> GetAllVariables(const bool bIncludeInputArguments = false);

	void RefreshFunctionReferences(URigVMLibraryNode* InFunctionDefinition, bool bSetupUndoRedo);

	FString GetGraphOuterName() const;

public:
	static int32 GetMaxNameLength() { return 100; }
	static FString GetSanitizedName(const FString& InName, bool bAllowPeriod, bool bAllowSpace);
	static FString GetSanitizedGraphName(const FString& InName);
	static FString GetSanitizedNodeName(const FString& InName);
	static FString GetSanitizedVariableName(const FString& InName);
	static FString GetSanitizedPinName(const FString& InName);
	static FString GetSanitizedPinPath(const FString& InName);
	static void SanitizeName(FString& InOutName, bool bAllowPeriod, bool bAllowSpace);
	static TArray<TPair<FString, FString>> GetLinkedPinPaths(URigVMNode* InNode, bool bIncludeInjectionNodes = false);
	static TArray<TPair<FString, FString>> GetLinkedPinPaths(const TArray<URigVMNode*>& InNodes, bool bIncludeInjectionNodes = false);
	bool BreakLinkedPaths(const TArray<TPair<FString, FString>>& InLinkedPaths, bool bSetupUndoRedo);
	bool RestoreLinkedPaths(
		const TArray<TPair<FString, FString>>& InLinkedPaths,
		const TMap<FString, FString>& InNodeNameMap,
		const TMap<FString,FRigVMController_PinPathRemapDelegate>& InRemapDelegates,
		FRigVMController_CheckPinComatibilityDelegate InCompatibilityDelegate,
		bool bSetupUndoRedo,
		ERigVMPinDirection InUserDirection = ERigVMPinDirection::Invalid);
	bool RestoreLinkedPaths(
		const TArray<TPair<FString, FString>>& InLinkedPaths,
		const TMap<FString, FString>& InNodeNameMap,
		const TMap<FString,FRigVMController_PinPathRemapDelegate>& InRemapDelegates,
		bool bSetupUndoRedo,
		ERigVMPinDirection InUserDirection = ERigVMPinDirection::Invalid)
	{
		return RestoreLinkedPaths(InLinkedPaths, InNodeNameMap, InRemapDelegates, FRigVMController_CheckPinComatibilityDelegate(), bSetupUndoRedo, InUserDirection);
	}

#if WITH_EDITOR
	// Registers this template node's use for later determining the commonly used types
	void RegisterUseOfTemplate(const URigVMTemplateNode* InNode);

	// Inquire on the commonly used types for a template node. This can be used to resolve a node without user input (as a default)
	FRigVMTemplate::FTypeMap GetCommonlyUsedTypesForTemplate(const URigVMTemplateNode* InNode) const;
#endif
	
private: 
	UPROPERTY(transient)
	TArray<TObjectPtr<URigVMGraph>> Graphs;

	UPROPERTY(transient)
	TObjectPtr<URigVMActionStack> ActionStack;

	bool bSuspendNotifications;
	bool bReportWarningsAndErrors;
	bool bIgnoreRerouteCompactnessChanges;
	ERigVMPinDirection UserLinkDirection;

	// temporary maps used for pin redirection
	// only valid between Detach & ReattachLinksToPinObjects
	TMap<FString, FString> InputPinRedirectors;
	TMap<FString, FString> OutputPinRedirectors;

	struct FControlRigStructPinRedirectorKey
	{
		FControlRigStructPinRedirectorKey()
		{
		}

		FControlRigStructPinRedirectorKey(UScriptStruct* InScriptStruct, const FString& InPinPathInNode)
		: Struct(InScriptStruct)
		, PinPathInNode(InPinPathInNode)
		{
		}

		friend FORCEINLINE uint32 GetTypeHash(const FControlRigStructPinRedirectorKey& Cache)
		{
			return HashCombine(GetTypeHash(Cache.Struct), GetTypeHash(Cache.PinPathInNode));
		}

		FORCEINLINE bool operator ==(const FControlRigStructPinRedirectorKey& Other) const
		{
			return Struct == Other.Struct && PinPathInNode == Other.PinPathInNode;
		}

		FORCEINLINE bool operator !=(const FControlRigStructPinRedirectorKey& Other) const
		{
			return Struct != Other.Struct || PinPathInNode != Other.PinPathInNode;
		}

		UScriptStruct* Struct;
		FString PinPathInNode;
	};

	static TMap<FControlRigStructPinRedirectorKey, FString> PinPathCoreRedirectors;
	FCriticalSection PinPathCoreRedirectorsLock;

	FRigVMUnitNodeCreatedContext UnitNodeCreatedContext;

	bool bIsTransacting; // Performing undo/redo transaction
	bool bIsRunningUnitTest;
	bool bIsFullyResolvingTemplateNode;
	bool bSuspendRecomputingTemplateFilters;
#if WITH_EDITOR
	bool bRegisterTemplateNodeUsage;
#endif

	friend class URigVMGraph;
	friend class URigVMPin;
	friend class URigVMActionStack;
	friend struct FRigVMBaseAction;
	friend class URigVMCompiler;
	friend struct FRigVMControllerObjectFactory;
	friend struct FRigVMAddRerouteNodeAction;
	friend struct FRigVMChangePinTypeAction;
	friend struct FRigVMInjectNodeIntoPinAction;
	friend class FRigVMParserAST;
	friend class FRigVMControllerCompileBracketScope;
	friend class FRigVMControllerGraphGuard;
};

class FRigVMControllerGraphGuard
{
public:

	FRigVMControllerGraphGuard(URigVMController* InController, URigVMGraph* InGraph, bool bSetupUndoRedo = true)
		: Controller(InController)
		, bUndo(bSetupUndoRedo)
	{
		Controller->PushGraph(InGraph, bUndo);
		NumGraphs = Controller->Graphs.Num();
	}

	~FRigVMControllerGraphGuard()
	{
		// an action can be cancelled in the middle of a graph guard,
		// in that case CancelAction should have already popped the graph
		if (Controller->Graphs.Num() < NumGraphs)
		{
			return;
		}
		Controller->PopGraph(bUndo);
	}

private:

	URigVMController* Controller;
	bool bUndo;

	int32 NumGraphs;
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