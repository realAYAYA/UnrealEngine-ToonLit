// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BPTerminal.h"
#include "Containers/Array.h"
#include "Containers/IndirectArray.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphCompilerUtilities.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "HAL/PlatformMath.h"
#include "K2Node_Event.h"
#include "Kismet2/CompilerResultsLog.h"
#include "KismetCompiledFunctionContext.h"
#include "KismetCompilerMisc.h"
#include "Logging/LogMacros.h"
#include "Math/Color.h"
#include "Misc/EnumClassFlags.h"
#include "Templates/Casts.h"
#include "Templates/Function.h"
#include "Templates/SharedPointer.h"
#include "Templates/SubclassOf.h"
#include "Templates/TypeHash.h"
#include "UObject/NameTypes.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UnrealNames.h"

class FField;
class FKismetCompilerContext;
class FKismetCompilerVMBackend;
class FLinkerLoad;
class FProperty;
class UBlueprintGeneratedClass;
class UClass;
class UEdGraph;
class UFunction;
class UK2Node_CreateDelegate;
class UK2Node_Event;
class UK2Node_FunctionEntry;
class UK2Node_TemporaryVariable;
class UK2Node_Timeline;
class UK2Node_Tunnel;
class UScriptStruct;
class UStruct;
struct FBPTerminal;
struct FKismetFunctionContext;
struct FUserPinInfo;

KISMETCOMPILER_API DECLARE_LOG_CATEGORY_EXTERN(LogK2Compiler, Log, All);

//////////////////////////////////////////////////////////////////////////
// FKismetCompilerContext

enum class EInternalCompilerFlags
{
	None = 0x0,

	PostponeLocalsGenerationUntilPhaseTwo = 0x1,
	PostponeDefaultObjectAssignmentUntilReinstancing = 0x2,
	SkipRefreshExternalBlueprintDependencyNodes = 0x4,
};
ENUM_CLASS_FLAGS(EInternalCompilerFlags)

typedef TFunction<TSharedPtr<FKismetCompilerContext>(UBlueprint*, FCompilerResultsLog&, const FKismetCompilerOptions&)> CompilerContextFactoryFunction;

class KISMETCOMPILER_API FKismetCompilerContext : public FGraphCompilerContext
{
public:

	DECLARE_EVENT_OneParam(FKismetCompilerContext, FOnFunctionListCompiled, FKismetCompilerContext*);

protected:
	typedef FGraphCompilerContext Super;

	// Schema for the graph being compiled 
	UEdGraphSchema_K2* Schema;

	// Map from node class to a handler functor
	TMap< TSubclassOf<class UEdGraphNode>, FNodeHandlingFunctor*> NodeHandlers;

	// Map of properties created for timelines; to aid in debug data generation
	TMap<class UTimelineTemplate*, class FProperty*> TimelineToMemberVariableMap;

	// Map from UProperties to default object values, to be fixed up after compilation is complete
	TMap<FName, FString> DefaultPropertyValueMap;

	// Names of functions created
	TSet<FString> CreatedFunctionNames;

	// List of functions currently allocated
	TIndirectArray<FKismetFunctionContext> FunctionList;

	/** Set of function graphs generated for the class layout at compile time  */
	TArray<UEdGraph*> GeneratedFunctionGraphs;

	/** Event that is broadcast immediately after the function list for this context has been compiled. */
	FOnFunctionListCompiled FunctionListCompiledEvent;

	// This struct holds the various compilation options, such as which passes to perform, whether to save intermediate results, etc
	FKismetCompilerOptions CompileOptions;

	// Maximum height encountered in this row; used to position the next row appropriately
	int32 MacroRowMaxHeight;

	// Maximum bounds of the spawning area
	int32 MinimumSpawnX;
	int32 MaximumSpawnX;

	// Average node size for nodes with no size
	int32 AverageNodeWidth;
	int32 AverageNodeHeight;

	// Padding
	int32 HorizontalSectionPadding;
	int32 VerticalSectionPadding;
	int32 HorizontalNodePadding;
	
	// Used to space expanded macro nodes when saving intermediate results
	int32 MacroSpawnX;
	int32 MacroSpawnY;

	UScriptStruct* VectorStruct;
	UScriptStruct* RotatorStruct;
	UScriptStruct* TransformStruct;
	UScriptStruct* LinearColorStruct;

public:
	UBlueprint* Blueprint;
	UBlueprintGeneratedClass* NewClass;
	UBlueprintGeneratedClass* OldClass;

	// The ubergraph; valid from roughly the start of CreateAndProcessEventGraph
	UEdGraph* ConsolidatedEventGraph;

	// The ubergraph context; valid from the end of CreateAndProcessEventGraph
	FKismetFunctionContext* UbergraphContext;

	TMap<UEdGraphNode*, UEdGraphNode*> CallsIntoUbergraph;
	int32 bIsFullCompile:1;

	// Source pin to latent/delegate entry point
	TMap<UEdGraphNode*, UK2Node_Event*> SourceNodeToExpansionEvent;
	TMap<UEdGraphPin*, UK2Node_Event*> SourcePinToExpansionEvent;

	// Map that can be used to find the macro node that spawned a provided node, 
	// if any. Macro instances can have more macros inside of them, so entries 
	// in this map may chain (i.e. values may also need to be used as keys to find
	// the full chain). Used to generate deterministic, unique identifiers for 
	// properties generated by nodes.
	TMap<UEdGraphNode*, UEdGraphNode*> MacroGeneratedNodes;

	// Map from a name to the number of times it's been 'created' (identical nodes create the same variable names, so they need something appended)
	FNetNameMapping ClassScopeNetNameMap;

	// Data that persists across CompileClassLayout/CompileFunctions calls:
	UObject* OldCDO;
	int32 OldGenLinkerIdx;
	FLinkerLoad* OldLinker;
	UBlueprintGeneratedClass* TargetClass;

	// Flag to trigger FMulticastDelegateProperty::SignatureFunction resolution in 
	// CreateClassVariablesFromBlueprint:
	bool bAssignDelegateSignatureFunction;

	struct FDelegateInfo
	{
		FName ProxyFunctionName;
		FName CapturedVariableName;
	};

	TMap<UK2Node_CreateDelegate*, FDelegateInfo> ConvertibleDelegates;

	static FSimpleMulticastDelegate OnPreCompile;
	static FSimpleMulticastDelegate OnPostCompile;

	/** Broadcasts a notification immediately after the function list for this context has been compiled. */
	FOnFunctionListCompiled& OnFunctionListCompiled() { return FunctionListCompiledEvent; }

	FKismetCompilerContext(UBlueprint* SourceSketch, FCompilerResultsLog& InMessageLog, const FKismetCompilerOptions& InCompilerOptions);
	virtual ~FKismetCompilerContext();
	
	/** Compile the class layout of the blueprint */
	void CompileClassLayout(EInternalCompilerFlags InternalFlags);
	
	/** Compile the functions of the blueprint - must be done after compiling the class layout: */
	void CompileFunctions(EInternalCompilerFlags InternalFlags);

	/** Called after the CDO has been generated, allows assignment of cached/derived data: */
	void PostCDOCompiled(const UObject::FPostCDOCompiledContext& Context);

	/** Compile a blueprint into a class and a set of functions */
	void Compile();

	/** Function used to assign the new class that will be used by the compiler */
	void SetNewClass(UBlueprintGeneratedClass* ClassToUse);

	const UEdGraphSchema_K2* GetSchema() const { return Schema; }

	/** Spawn an intermediate function graph for this compilation using the specified desired name (and optional signature),
		which may be modified to make it unique. */
	UEdGraph* SpawnIntermediateFunctionGraph(const FString& InDesiredFunctionName, const UFunction* InSignature = nullptr, bool bUseUniqueName = true);

	// Spawns an intermediate node associated with the source node (for error purposes)
	template <typename NodeType>
	NodeType* SpawnIntermediateNode(UEdGraphNode* SourceNode, UEdGraph* ParentGraph = NULL)
	{
		if (ParentGraph == NULL)
		{
			ParentGraph = SourceNode->GetGraph();
		}

		NodeType* Result = ParentGraph->CreateIntermediateNode<NodeType>();
		//check (Cast<UK2Node_Event>(Result) == nullptr); -- Removed to avoid any fallout, will replace with care later
		MessageLog.NotifyIntermediateObjectCreation(Result, SourceNode); // this might be useful to track back function entry nodes to events.
		Result->CreateNewGuid();

		AutoAssignNodePosition(Result);

		return Result;
	}

	// Spawns an intermediate event node associated with the source node (for error purposes)
	template <typename NodeType>
	NodeType* SpawnIntermediateEventNode(UEdGraphNode* SourceNode, UEdGraphPin* SourcePin = nullptr, UEdGraph* ParentGraph = nullptr)
	{
		if (ParentGraph == nullptr && SourceNode != nullptr)
		{
			ParentGraph = SourceNode->GetGraph();
		}

		NodeType* Result = ParentGraph->CreateIntermediateNode<NodeType>();
		//check (Cast<UK2Node_Event>(Result) != nullptr); -- Removed to avoid any fallout, will replace with care later
		MessageLog.NotifyIntermediateObjectCreation(Result, SourceNode); // this might be useful to track back function entry nodes to events.
		Result->CreateNewGuid();
		// Track all expansion step generated events.
		if (bIsFullCompile)
		{
			if (SourcePin)
			{
				UEdGraphNode* TrueSourceNode = Cast<UEdGraphNode>(MessageLog.FindSourceObject(SourcePin->GetOwningNode()));
				UEdGraphPin* TrueSourcePin = TrueSourceNode->FindPin(SourcePin->GetFName());
				SourcePinToExpansionEvent.Add(TrueSourcePin) = Cast<UK2Node_Event>(Result);
			}
			else if (SourceNode)
			{
				UEdGraphNode* TrueSourceNode = Cast<UEdGraphNode>(MessageLog.FindSourceObject(SourceNode));
				SourceNodeToExpansionEvent.Add(TrueSourceNode) = Cast<UK2Node_Event>(Result);
			}
		}
		AutoAssignNodePosition(Result);

		return Result;
	}

	/**
	 * Moves pin links over from the source-pin to the specified intermediate, 
	 * and validates the result (additionally logs a redirect from the 
	 * intermediate-pin back to the source so we can back trace for debugging, etc.)
	 * 
	 * @param  SourcePin		The pin you want disconnected.
	 * @param  IntermediatePin	The pin you want the SourcePin's links moved to.
	 * @return The result from calling the schema's MovePinLinks().
	 */
	FPinConnectionResponse MovePinLinksToIntermediate(UEdGraphPin& SourcePin, UEdGraphPin& IntermediatePin);

	/**
	 * Copies pin links over from the source-pin to the specified intermediate, 
	 * and validates the result (additionally logs a redirect from the 
	 * intermediate-pin back to the source so we can back trace for debugging, etc.)
	 * 
	 * @param  SourcePin		The pin whose links you want copied.
	 * @param  IntermediatePin	The pin you want the SourcePin's links copied to.
	 * @return The result from calling the schema's CopyPinLinks().
	 */
	FPinConnectionResponse CopyPinLinksToIntermediate(UEdGraphPin& SourcePin, UEdGraphPin& IntermediatePin);

	struct FNameParameterHelper
	{
		FNameParameterHelper(const FName InNameParameter) : NameParameter(InNameParameter) { }
		FNameParameterHelper(const FString& InNameParameter) : NameParameter(*InNameParameter) { }
		FNameParameterHelper(const TCHAR* InNameParameter) : NameParameter(InNameParameter) { }

		FName operator*() const { return NameParameter; }

	private:
		FName NameParameter;
	};

	UK2Node_TemporaryVariable* SpawnInternalVariable(UEdGraphNode* SourceNode, FName Category, FName SubCategory = NAME_None, UObject* SubcategoryObject = nullptr, EPinContainerType PinContainerType = EPinContainerType::None, const FEdGraphTerminalType& ValueTerminalType = FEdGraphTerminalType());

	bool UsePersistentUberGraphFrame() const;

	FString GetGuid(const UEdGraphNode* Node) const;

	static TSharedPtr<FKismetCompilerContext> GetCompilerForBP(UBlueprint* BP, FCompilerResultsLog& InMessageLog, const FKismetCompilerOptions& InCompileOptions);
	static void RegisterCompilerForBP(UClass* BPClass, CompilerContextFactoryFunction FactoryFunction );

	/** Ensures that all variables have valid names for compilation/replication */
	void ValidateVariableNames();

	/** Ensures that all component class overrides are legal overrides of the parent class */
	void ValidateComponentClassOverrides();

	/**
	* Ensures that all class reference Properties are legal overrides of the parent class
	* by checking the default value set on any PC_Class variable types. Requires a 
	* valid CDO in order to do this validation. Called in Stage V: Validate 
	*/
	void ValidateClassPropertyDefaults();

	/** Creates a class variable for each entry in the Blueprint NewVars array */
	virtual void CreateClassVariablesFromBlueprint();

	/**
	 * Picks the name to use for an autogenerated event stub
	 */ 
	FName GetEventStubFunctionName(UK2Node_Event* SrcEventNode);

	/**
	 * Searches the function graphs and ubergraph pages for any delegate proxies,
	 * which are then registered with the compiler context.
	 * If a "captured" variable is needed, then a new property will be added to the current class.
	 * In this context, a captured variable is any target actor that the delegate will be called on.
	 */
	void RegisterClassDelegateProxiesFromBlueprint();

protected:
	virtual UEdGraphSchema_K2* CreateSchema();
	virtual void PostCreateSchema();
	virtual void SpawnNewClass(const FString& NewClassName);
	virtual void OnNewClassSet(UBlueprintGeneratedClass* ClassToUse) {}
	virtual void OnPostCDOCompiled(const UObject::FPostCDOCompiledContext& Context) {}

	/**
	 * Backwards Compatability:  Ensures that the passed in TargetClass is of the proper type (e.g. BlueprintGeneratedClass, AnimBlueprintGeneratedClass), and NULLs the reference if it is not 
	 */
	virtual void EnsureProperGeneratedClass(UClass*& TargetClass);

	/**
	 * Removes the properties and functions from a class, so that new ones can be created in its place
	 * 
	 * @param ClassToClean		The UClass to scrub
	 * @param OldCDO			Reference to the old CDO of the class, so we can copy the properties from it to the new class's CDO
	 */
	virtual void CleanAndSanitizeClass(UBlueprintGeneratedClass* ClassToClean, UObject*& OldCDO);

	struct KISMETCOMPILER_API FSubobjectCollection
	{
	private:
		TSet<const UObject*> Collection;

	public:
		void AddObject(const UObject* const InObject);

		template<typename TOBJ>
		void AddObjects(const TArray<TOBJ>& InObjects)
		{
			for ( const auto& ObjPtr : InObjects )
			{
				AddObject(ObjPtr);
			}
		}

		bool operator()(const UObject* const RemovalCandidate) const;
	};

	/**
	 * Saves any SubObjects on the blueprint that need to survive the clean 
	 */
	virtual void SaveSubObjectsFromCleanAndSanitizeClass(FSubobjectCollection& SubObjectsToSave, UBlueprintGeneratedClass* ClassToClean);

	/** 
	 * Checks a connection response, and errors if it didn't succeed (not public, 
	 * users should be using MovePinLinksToIntermediate/CopyPinLinksToIntermediate 
	 * instead of wrapping their own with this).
	 */
	void CheckConnectionResponse(const FPinConnectionResponse &Response, const UEdGraphNode *Node);

	/** Prune isolated nodes given the specified graph */
	void PruneIsolatedNodes(UEdGraph* InGraph, bool bInIncludeNodesThatCouldBeExpandedToRootSet);

	// FGraphCompilerContext interface
	virtual void ValidateLink(const UEdGraphPin* PinA, const UEdGraphPin* PinB) const override;
	virtual void ValidatePin(const UEdGraphPin* Pin) const override;
	virtual void ValidateNode(const UEdGraphNode* Node) const override;
	virtual bool CanIgnoreNode(const UEdGraphNode* Node) const override;
	virtual bool ShouldForceKeepNode(const UEdGraphNode* Node) const override;
	virtual void PruneIsolatedNodes(const TArray<UEdGraphNode*>& RootSet, TArray<UEdGraphNode*>& GraphNodes) override;
	virtual bool PinIsImportantForDependancies(const UEdGraphPin* Pin) const override
	{
		// The execution wires do not form data dependencies, they are only important for final scheduling and that is handled thru gotos
		return Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec;
	}

	// Expands out nodes that need it
	void ExpansionStep(UEdGraph* Graph, bool bAllowUbergraphExpansions);

	// Advances the macro position tracking
	void AdvanceMacroPlacement(int32 Width, int32 Height);
	void AutoAssignNodePosition(UEdGraphNode* Node);
	void CreateCommentBlockAroundNodes(const TArray<UEdGraphNode*>& Nodes, UObject* SourceObject, UEdGraph* TargetGraph, FString CommentText, FLinearColor CommentColor, int32& Out_OffsetX, int32& Out_OffsetY);

	/** Creates a class variable */
	FProperty* CreateVariable(const FName Name, const FEdGraphPinType& Type);

	// Gives derived classes a chance to emit debug data
	virtual void PostCompileDiagnostics() {}

	// Gives derived classes a chance to hook up any custom logic
	virtual void PreCompile() { OnPreCompile.Broadcast(); }
	virtual void PostCompile() { OnPostCompile.Broadcast(); }

	// Gives derived classes a chance to process post-node expansion
	virtual void PostExpansionStep(const UEdGraph* Graph) {}

	/** Determines if a node is pure */
	virtual bool IsNodePure(const UEdGraphNode* Node) const;

	/** Creates a property with flags including PropertyFlags in the Scope structure for each entry in the Terms array */
	void CreatePropertiesFromList(UStruct* Scope, FField**& PropertyStorageLocation, TIndirectArray<FBPTerminal>& Terms, EPropertyFlags PropertyFlags, bool bPropertiesAreLocal, bool bPropertiesAreParameters = false);

	/** Create the properties on a function for input/output parameters */
	void CreateParametersForFunction(FKismetFunctionContext& Context, UFunction* ParameterSignature, FField**& FunctionPropertyStorageLocation);

	/** Creates the properties on a function that store the local and event graph (if applicable) variables */
	void CreateLocalVariablesForFunction(FKismetFunctionContext& Context, FField**& FunctionPropertyStorageLocation);

	/** Creates user defined local variables for function */
	void CreateUserDefinedLocalVariablesForFunction(FKismetFunctionContext& Context, FField**& FunctionPropertyStorageLocation);

	/** Helper function for CreateUserDefinedLocalVariablesForFunction and compilation manager's FastGenerateSkeletonClass: */
	static FProperty* CreateUserDefinedLocalVariableForFunction(const FBPVariableDescription& Variable, UFunction* Function, UBlueprintGeneratedClass* OwningClass, FField**& FunctionPropertyStorageLocation, const UEdGraphSchema_K2* Schema, FCompilerResultsLog& MessageLog);

	/** Adds a default value entry into the DefaultPropertyValueMap for the property specified */
	void SetPropertyDefaultValue(const FProperty* PropertyToSet, FString& Value);

	/** Copies default values cached for the terms in the DefaultPropertyValueMap to the final CDO */
	virtual void CopyTermDefaultsToDefaultObject(UObject* DefaultObject);

	/** Non virtual wrapper to encapsulate functions that occur when the CDO is ready for values: */
	void PropagateValuesToCDO(UObject* NewCDO, UObject* OldCDO);

	/** 
	 * Function works only if subclass of AActor or UActorComponent.
	 * If ReceiveTick event is defined, force CanEverTick.
	 */
	void SetCanEverTick() const;

	/** Scan FunctionList and return Entry point, for matching one  */
	const UK2Node_FunctionEntry* FindLocalEntryPoint(const UFunction* Function) const;

	//@TODO: Debug printing
	void PrintVerboseInfoStruct(UStruct* Struct) const;
	void PrintVerboseInformation(UClass* Class) const;
	//@ENDTODO

	/**
	 * Performs transformations on specific nodes that require it according to the schema
	 */
	virtual void TransformNodes(FKismetFunctionContext& Context);

	/**
	 * Merges in any all ubergraph pages into the gathering ubergraph
	 */
	virtual void MergeUbergraphPagesIn(UEdGraph* Ubergraph);

	/**
	 * Creates a list of functions to compile
	 */
	virtual void CreateFunctionList();

	/** Creates a new function context and adds it to the function list to be processed. */
	FKismetFunctionContext* CreateFunctionContext();

	/**
	 * Merges macros/subgraphs into the graph and validates it, creating a function list entry if it's reasonable.
	 */
	virtual void ProcessOneFunctionGraph(UEdGraph* SourceGraph, bool bInternalFunction = false);

	/**
	 * Gets the unique name for this context's ExecuteUbergraph function
	 */
	FName GetUbergraphCallName() const
	{
		const FString UbergraphCallString = UEdGraphSchema_K2::FN_ExecuteUbergraphBase.ToString() + TEXT("_") + Blueprint->GetName();
		return FName(*UbergraphCallString);
	}

	/**
	 * Expands any macro instances and collapses any tunnels in the nodes of SourceGraph
	 */
	void ExpandTunnelsAndMacros(UEdGraph* SourceGraph);

	/**
	 * Maps the nodes in an intermediate tunnel expansion path back to the owning tunnel instance node.
	 */
	void MapExpansionPathToTunnelInstance(const UEdGraphNode* InnerExpansionNode, const UEdGraphNode* OuterTunnelInstance);

	/**
	* Processes an intermediate tunnel expansion boundary.
	*
	* We define a tunnel boundary as the input and output sides of an intermediate tunnel instance node expansion. Each boundary
	* consists of a pair of tunnel nodes (input/output), with one side being the tunnel "instance" node that owns the expansion.
	* After expansion, tunnel nodes are cropped and removed from the function graph, so they do not result in any actual bytecode.
	*
	* This function maps the nodes in the execution path through the expansion and back to the outer tunnel instance node. If
	* Blueprint debugging is enabled, this function also spawns one or more intermediate "boundary" NOPs around the tunnel I/O
	* pair. The boundary nodes are intended to serve as debug sites, allowing breakpoints to be hit on both sides of the tunnel.
	*
	* @param	TunnelInput		Tunnel input node. This will either be a tunnel instance node (OutputSource) or a tunnel exit node.
	* @param	TunnelOutput	Tunnel output node. This will either be a tunnel entry node or a tunnel instance node (InputSink).
	*/
	void ProcessIntermediateTunnelBoundary(UK2Node_Tunnel* TunnelInput, UK2Node_Tunnel* TunnelOutput);

	/**
	 * Merges pages and creates function stubs, etc...
	 */
	void CreateAndProcessUbergraph();

	/** Create a stub function graph for the event node, and have it invoke the correct point in the ubergraph */
	void CreateFunctionStubForEvent(UK2Node_Event* Event, UObject* OwnerOfTemporaries);

	/** Expand timeline nodes into necessary nodes */
	void ExpandTimelineNodes(UEdGraph* SourceGraph);

	/**
	 * First phase of compiling a function graph
	 *   - Performs initial validation that the graph is at least well formed enough to be processed further
	 *   - Creates a copy of the graph to allow further transformations to occur
	 *   - Prunes the 'graph' to only included the connected portion that contains the function entry point 
	 *   - Schedules execution of each node based on data/execution dependencies
	 *   - Creates a UFunction object containing parameters and local variables (but no script code yet)
	 */
	virtual void PrecompileFunction(FKismetFunctionContext& Context, EInternalCompilerFlags InternalFlags);

	/**
	 * Used for performing custom patching during stage IX of the compilation during load.
	 */
	virtual void PreCompileUpdateBlueprintOnLoad(UBlueprint* BP);

	/**
	 * Second phase of compiling a function graph
	 *   - Generates an executable statement list
	 */
	virtual void CompileFunction(FKismetFunctionContext& Context);

	/**
	 * Final phase of compiling a function graph; called after all functions have had CompileFunction called
	 *   - Patches up cross-references, etc..., and performs final validation
	 */
	virtual void PostcompileFunction(FKismetFunctionContext& Context);

	/**
	 * Handles final post-compilation setup, flags, creates cached values that would normally be set during deserialization, etc...
	 */
	void FinishCompilingFunction(FKismetFunctionContext& Context);

	/** Adds metadata for a particular compiled function based on its characteristics */
	virtual void SetCalculatedMetaDataAndFlags(UFunction* Function, UK2Node_FunctionEntry* EntryNode, const UEdGraphSchema_K2* Schema );

	/** Reflects each pin's user set, default value into the function's metadata (so it can be queried for later by CallFunction nodes, etc.) */
	static void SetDefaultInputValueMetaData(UFunction* Function, const TArray< TSharedPtr<FUserPinInfo> >& InputData);

	/**
	 * Handles adding the implemented interface information to the class
	 */
	virtual void AddInterfacesFromBlueprint(UClass* Class);

	/**
	 * Handles final post-compilation setup, flags, creates cached values that would normally be set during deserialization, etc...
	 */
	virtual void FinishCompilingClass(UClass* Class);

	/** Build the dynamic bindings objects used to tie events to delegates at runtime */
	void BuildDynamicBindingObjects(UBlueprintGeneratedClass* Class);

	/**
	 *  If a function in the graph cannot be placed as event make sure that it is not.
	 */
	void VerifyValidOverrideEvent(const UEdGraph* Graph);

	/**
	 *  If a function in the graph cannot be overridden make sure that it is not.
	 */
	void VerifyValidOverrideFunction(const UEdGraph* Graph);

	/**
	 * Checks if self pins are connected.
	 */
	void ValidateSelfPinsInGraph(FKismetFunctionContext& Context);

	/**
	* Checks if pin types are unresolved (e.g. still wildcards).
	*/
	void ValidateNoWildcardPinsInGraph(const UEdGraph* SourceGraph);

	/** Ensures that all timelines have valid names for compilation/replication */
	void ValidateTimelineNames();

	/** Ensures that all function graphs have valid names for compilation/replication */
	void ValidateFunctionGraphNames();

	/** Validates the generated class */
	virtual bool ValidateGeneratedClass(UBlueprintGeneratedClass* Class);

	/** Discovers exec pin links for the sourcenode */
	void DetermineNodeExecLinks(UEdGraphNode* SourceNode, TMap<UEdGraphPin*, UEdGraphPin*>& SourceNodeLinks) const;

private:
	void CreateLocalsAndRegisterNets(FKismetFunctionContext& Context, FField**& FunctionPropertyStorageLocation);

	/**
	 * Handles creating a new event node for a given output on a timeline node utilizing the named function
	 */
	void CreatePinEventNodeForTimelineFunction(UK2Node_Timeline* TimelineNode, UEdGraph* SourceGraph, FName FunctionName, const FName PinName, FName ExecFuncName);

	/** Util for creating a node to call a function on a timeline and move connections to it */
	class UK2Node_CallFunction* CreateCallTimelineFunction(UK2Node_Timeline* TimelineNode, UEdGraph* SourceGraph, FName FunctionName, UEdGraphPin* TimelineVarPin, UEdGraphPin* TimelineFunctionPin);

	/**
	 * Function to reset graph node's error flag before compiling
	 *
	 * @param: Reference to graph instance
	 */
	void ResetErrorFlags(UEdGraph* Graph) const;
	
	/**
	 * Registers any nodes that are bound to a "convertible" function signature.
	 * Specifically, these are function signatures that differ only by float/double parameters.
	 * 
	 * @param: Reference to graph instance
	 */
	void RegisterConvertibleDelegates(UEdGraph* Graph);

	/**
	 * Modifies the graph to use a proxy delegate function if it uses a convertible delegate signature.
	 * This involves several steps:
	 * 1. Creates a new function graph that uses the exact function signature of the delegate.
	 * 2. Adds and links the original delegate function call, which implicity casts the input parameters.
	 * 3. If applicable, adds a node to the original graph that sets the variable of the target actor (ie: the captured variable)
	 * 
	 * @param: Reference to graph instance
	 */
	void ReplaceConvertibleDelegates(UEdGraph* Graph);

	/**
	 * The compilation manager is a new client of the compiler - it reuses several virtual functions
	 * that are not usefully public in any other context and so rather than expand the public interface
	 * we have decided to make the compilation manager a friend:
	 */
	friend struct FBlueprintCompilationManagerImpl;
};

//////////////////////////////////////////////////////////////////////////
