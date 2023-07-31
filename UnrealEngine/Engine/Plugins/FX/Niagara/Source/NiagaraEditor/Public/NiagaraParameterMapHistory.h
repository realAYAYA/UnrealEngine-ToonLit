// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NiagaraCommon.h"
#include "NiagaraEmitter.h"
#include "Templates/Tuple.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#include "NiagaraScript.h"

class UNiagaraNodeOutput;
class UEdGraphPin;
class UEdGraphNode;
class UNiagaraParameterCollection;
class FHlslNiagaraTranslator;

class FCompileConstantResolver
{
public:
	FCompileConstantResolver() : Emitter(FVersionedNiagaraEmitter()), System(nullptr), Translator(nullptr), Usage(ENiagaraScriptUsage::Function), DebugState(ENiagaraFunctionDebugState::NoDebug) {}
	FCompileConstantResolver(const FVersionedNiagaraEmitter& Emitter, ENiagaraScriptUsage Usage, ENiagaraFunctionDebugState DebugState = ENiagaraFunctionDebugState::NoDebug) : Emitter(Emitter), System(nullptr), Translator(nullptr), Usage(Usage), DebugState(DebugState) {}
	FCompileConstantResolver(const UNiagaraSystem* System, ENiagaraScriptUsage Usage, ENiagaraFunctionDebugState DebugState = ENiagaraFunctionDebugState::NoDebug) : Emitter(FVersionedNiagaraEmitter()), System(System), Translator(nullptr), Usage(Usage), DebugState(DebugState) {}
	FCompileConstantResolver(const FHlslNiagaraTranslator* Translator, ENiagaraFunctionDebugState DebugState = ENiagaraFunctionDebugState::NoDebug) : Emitter(FVersionedNiagaraEmitter()), System(nullptr), Translator(Translator), Usage(ENiagaraScriptUsage::Function), DebugState(DebugState) {}

	const UNiagaraEmitter* GetEmitter() const { return Emitter.Emitter; }
	const UNiagaraSystem* GetSystem() const { return System; }
	const FHlslNiagaraTranslator* GetTranslator() const { return Translator; }
	ENiagaraScriptUsage GetUsage() const { return Usage; }
	ENiagaraFunctionDebugState GetDebugState() const { return DebugState; }

	bool ResolveConstant(FNiagaraVariable& OutConstant) const;

	ENiagaraFunctionDebugState CalculateDebugState() const;
	FCompileConstantResolver WithDebugState(ENiagaraFunctionDebugState InDebugState) const;
	FCompileConstantResolver WithUsage(ENiagaraScriptUsage ScriptUsage) const;

	// returns a hash of the data that is used when resolving constants using this resolver.  The specific
	// Emitter/System/Translator shouldn't be included but the values that it might use from those objects
	// should be
	uint32 BuildTypeHash() const;

private:
	FVersionedNiagaraEmitter Emitter;
	const UNiagaraSystem* System;
	const FHlslNiagaraTranslator* Translator;
	ENiagaraScriptUsage Usage;
	ENiagaraFunctionDebugState DebugState;
};

struct FModuleScopedPin
{
	FModuleScopedPin() = default;
	FModuleScopedPin(const UEdGraphPin* InPin, FName InName = NAME_None)
	: Pin(InPin)
	, ModuleName(InName)
	{}

	bool operator==(const FModuleScopedPin& Rhs) const
	{
		return Pin == Rhs.Pin && ModuleName == Rhs.ModuleName;
	}

	const UEdGraphPin* Pin = nullptr;
	FName ModuleName = NAME_None;
};

struct FGraphTraversalHandle
{
public:
	FGraphTraversalHandle() {}
	FGraphTraversalHandle(const TArray<FGuid>& InputPath, TArray<FString> InFriendlyPath) : Path(InputPath), FriendlyPath(InFriendlyPath) {}

	void Append(const FGuid& Guid, const FString& FriendlyName)
	{
		FriendlyPath.Add(FriendlyName);
		Path.Add(Guid);
	}

	void Push(const UEdGraphNode* Node);
	void Push(const UEdGraphPin* Pin);
	void Pop();

	bool operator==(const FGraphTraversalHandle& Rhs) const
	{
		return Path == Rhs.Path;
	}

	friend uint32 GetTypeHash(const FGraphTraversalHandle& Ref)
	{
		uint32 Value = 0;
		for (int32 i = 0; i < Ref.Path.Num(); i++)
			Value = HashCombine(Value, GetTypeHash(Ref.Path[i]));
		return Value;
	}

	FString ToString(bool bVerbose = false) const
	{
		FString Output;
		for (const FString& Str : FriendlyPath)
			Output += TEXT("/") + Str;

		if (bVerbose)
		{
			Output += " GUID: ";
			for (const FGuid& Guid : Path)
				Output += TEXT("/") + Guid.ToString();
		}
		return Output;
	}
private:
	TArray<FGuid > Path;
	TArray<FString> FriendlyPath;
};


/** Traverses a Niagara node graph to identify the variables that have been written and read from a parameter map. 
* 	This class is meant to aid in UI and compilation of the graph. There are several main script types and each one interacts
*	slightly differently with the history depending on context.
*/
struct FNiagaraParameterMapHistory
{
public:
	FNiagaraParameterMapHistory();

	ENiagaraScriptUsage OriginatingScriptUsage;
	FGuid UsageGuid;
	FName UsageName;

	/** The variables that have been identified during the traversal. */
	TArray<FNiagaraVariable> Variables;

	/** The metadata associated with each variable identified during the traversal. Only gathered by FNiagaraParameterMapHistoryWithMetaDataBuilder. */
	TArray<FNiagaraVariableMetaData> VariableMetaData;

	TArray<TArray<FString> > PerVariableConstantValue;
	TArray<TArray<FString> > PerVariableConstantDefaultValue;

	TArray<FNiagaraVariable> VariablesWithOriginalAliasesIntact;


	/** Used parameter collections identified during the traversal. TODO: Need to ensure these cannot be GCd if the asset is deleted while it's being used in an in flight compilation. */
	TArray<UNiagaraParameterCollection*> ParameterCollections;
	/** Cached off contents of used parameter collections, in case they change during threaded compilation. */
	TArray<TArray<FNiagaraVariable>> ParameterCollectionVariables;
	/** Cached off contents of used parameter collections, in case they change during threaded compilation. */
	TArray<FString> ParameterCollectionNamespaces;
	
	/** Are there any warnings that were encountered during the traversal of the graph for a given variable? */
	TArray<FString> PerVariableWarnings;

	/** For each variable that was found, identify the pins that wrote to them in order from first to last write.*/
	TArray<TArray<FModuleScopedPin> > PerVariableWriteHistory;

	/** For each variable that was found, identify the pins that read them from the map in order from first to last read. First of the pair has the read pin, second of the pair has the last set that wrote to the pin.*/
	struct FReadHistory
	{
		FModuleScopedPin ReadPin;
		FModuleScopedPin PreviousWritePin;
	};

	TArray<TArray<FReadHistory>> PerVariableReadHistory; 

	/** List of pins that manipulated the parameter map from input to output. */
	TArray<const UEdGraphPin*> MapPinHistory;

	/** List of nodes that manipulated the parameter map from input to output.*/
	TArray<const class UNiagaraNode*> MapNodeVisitations;

	/** For each node in MapNodeVisitations, record the start index and end index of variables added within the body of the node, i.e. a Get node will record just the values it pulls out directly. A function call, however, will record all sub-graph traversals.*/
	TArray<TTuple<uint32, uint32> > MapNodeVariableMetaData;

	/** List of emitter namespaces encountered as this parameter map was built.*/
	TArray<FString> EmitterNamespacesEncountered;

	/** List of all the custom iteration source override namespaces encountered */
	TArray<FName> IterationNamespaceOverridesEncountered;

	/** List of additional DataSets to be written that were encountered during traversal. */
	TArray<FNiagaraDataSetID> AdditionalDataSetWrites;

	TMap<FGraphTraversalHandle, FString> PinToConstantValues;
	bool bParameterCollectionsSkipped = false;
	void RegisterConstantPin(const FGraphTraversalHandle& InTraversalPath, const UEdGraphPin* InPin, const FString& InValue);

	bool IsVariableFromCustomIterationNamespaceOverride(const FNiagaraVariable& InVar) const;
	
	/**
	* Called in a depth-first traversal to identify a given Niagara Parameter Map pin that was touched during traversal.
	*/
	int32 RegisterParameterMapPin(const UEdGraphPin* Pin);

	void RegisterConstantVariableWrite(const FString& InValue, int32 InVarIdx, bool bIsSettingDefault, bool bIsLinkNotValue);

	uint32 BeginNodeVisitation(const UNiagaraNode* Node);
	void EndNodeVisitation(uint32 IndexFromBeginNode);

	/**
	* Find a variable by name with no concern for type.
	*/
	int32 FindVariableByName(const FName& VariableName, bool bAllowPartialMatch = false) const;

	static int32 SearchArrayForPartialNameMatch(const TArray<FNiagaraVariable>& Variables, const FName& VariableName);
	
	/**
	* Find a variable by both name and type. 
	*/
	int32 FindVariable(const FName& VariableName, const FNiagaraTypeDefinition& Type) const;


	/**
	* Add a variable outside the normal get/set node paradigm.
	*/
	int32 AddExternalVariable(const FNiagaraVariable& InVar);

	/**
	* Get the pin that added the parameter map to the graph.
	*/
	const UEdGraphPin* GetOriginalPin() const;

	/**
	* Get the output pin that we traced to build this history object.
	*/
	const UEdGraphPin* GetFinalPin() const;



	/** Get the first namespace entry for this variable. Optionally includes the trailing period.*/
	static FString GetNamespace(const FNiagaraVariable& InVar, bool bIncludeDelimiter = true);


	static FName ResolveEmitterAlias(const FName& InName, const FString& InAlias);

	
	/**
	* Remove the Particles namespace if it exists.
	*/
	static FNiagaraVariable ResolveAsBasicAttribute(const FNiagaraVariable& InVar, bool bSanitizeInput = true);


	/**
	* Reverses ResolveAsBasicAttribute.
	*/
	static FNiagaraVariable BasicAttributeToNamespacedAttribute(const FNiagaraVariable& InVar, bool bSanitizeInput = true);

	
	/** Prepends the namespace string to the variable name.*/
	static FNiagaraVariable VariableToNamespacedVariable(const FNiagaraVariable& InVar, FString Namespace);
		
	/**
	* Does this parameter start with the "Module" namespace? Note that the emitter namespace is an alias
	* for all non-funcion/module script types and will be specialized to the function call node's name using the module.
	*/
	static bool IsAliasedModuleParameter(const FNiagaraVariable& InVar);

	/**
	* Does this parameter start with the "Emitter" namespace? Note that the emitter namespace is an alias
	* for Emitter and System script types and will be specialized to the name of that specific emitter.
	*/
	static bool IsAliasedEmitterParameter(const FNiagaraVariable& InVar);
	static bool IsAliasedEmitterParameter(const FString& InVarName);
	/** Is this parameter in the special "System" namespace?*/
	static bool IsSystemParameter(const FNiagaraVariable& InVar);
	/** Is this parameter in the special "Engine" namespace?*/
	static bool IsEngineParameter(const FNiagaraVariable& InVar);
	/*** Is per instance engine parameter. */
	static bool IsPerInstanceEngineParameter(const FNiagaraVariable& InVar, const FString& EmitterAlias);
	static bool IsUserParameter(const FNiagaraVariable& InVar);
	static bool IsRapidIterationParameter(const FNiagaraVariable& InVar);
	static bool SplitRapidIterationParameterName(const FNiagaraVariable& InVar, ENiagaraScriptUsage InUsage, FString& EmitterName, FString& FunctionCallName, FString& InputName);
	
	/** Take an input string and make it hlsl safe.*/
	static FString MakeSafeNamespaceString(const FString& InStr);

	/** Does the variable start with this namespace?*/
	static bool IsInNamespace(const FNiagaraVariableBase& InVar, const FString& Namespace);

	static void GetValidNamespacesForReading(const UNiagaraScript* InScript, TArray<FString>& OutputNamespaces);
	static void GetValidNamespacesForReading(ENiagaraScriptUsage InScriptUsage, int32 InUsageBitmask, TArray<FString>& OutputNamespaces);
	static bool IsValidNamespaceForReading(ENiagaraScriptUsage InScriptUsage, int32 InUsageBitmask, FString Namespace);

	/** Called to determine if a given variable should be output from a script. It is not static as it requires the overall context to include emitter namespaces visited for system scripts.*/
	bool IsPrimaryDataSetOutput(const FNiagaraVariable& InVar, const UNiagaraScript* InScript,   bool bAllowDataInterfaces = false,  bool bAllowStatics = false) const;
	bool IsPrimaryDataSetOutput(const FNiagaraVariable& InVar, ENiagaraScriptUsage InUsage, bool bAllowDataInterfaces = false, bool bAllowStatics = false) const;
	static bool IsWrittenToScriptUsage(const FNiagaraVariable& InVar, ENiagaraScriptUsage InUsage, bool bAllowDataInterfaces);

	/** Are we required to export this variable as an external constant?*/
	bool IsExportableExternalConstant(const FNiagaraVariable& InVar, const UNiagaraScript* InScript);

	/** Does this variable belong in a namespace that needs to come in as an external constant to this script?*/
	static bool IsExternalConstantNamespace(const FNiagaraVariable& InVar, const UNiagaraScript* InScript, const FGuid& VersionGuid);
	static bool IsExternalConstantNamespace(const FNiagaraVariable& InVar, ENiagaraScriptUsage InUsage, int32 InUsageBitmask);
	
	/** Take a non-namespaced variable and move it to an appropriate external constant namespace for this script type.*/
	static FNiagaraVariable MoveToExternalConstantNamespaceVariable(const FNiagaraVariable& InVar, const UNiagaraScript* InScript);
	static FNiagaraVariable MoveToExternalConstantNamespaceVariable(const FNiagaraVariable& InVar, ENiagaraScriptUsage InUsage);	
	/**
	* Does this parameter start with the "Particles" namespace?
	*/
	static bool IsAttribute(const FNiagaraVariableBase& InVar);

	/** Does this parameter contain the "Initial" namespace as one of its intermediate namespaces?*/
	static bool IsInitialValue(const FNiagaraVariableBase& InVar);

	/** Does this variable name contain the "Initial" namespace as one of its intermediate namespaces? */
	static bool IsInitialName(const FName& InVariableName);

	/** Does this parameter contain the "Previous" namespace as one of its intermediate namespaces?*/
	static bool IsPreviousValue(const FNiagaraVariableBase& InVar);

	/** Does this variable name contain the "Previous" namespace as one of its intermediate namespaces? */
	static bool IsPreviousName(const FName& InVariableName);

	/** Get the output node associated with this graph.*/
	const UNiagaraNodeOutput* GetFinalOutputNode() const;

	/** Does this parameter contain the "Initial" namespace as one of its intermediate namespaces? If so, remove the "Initial" namespace and return the original value.*/
	static FNiagaraVariable GetSourceForInitialValue(const FNiagaraVariable& InVar);

	/** Does this parameter contain the "Initial" namespace as one of its intermediate namespaces? If so, remove the "Initial" namespace and return the original value.*/
	static FName GetSourceForInitialValue(const FName& InVariableName);

	/** Does this parameter contain the "Previous" namespace as one of its intermediate namespaces? If so, remove the "Previous" namespace and return the original value.*/
	static FNiagaraVariable GetSourceForPreviousValue(const FNiagaraVariable& InVar);

	/** Does this parameter contain the "Previous" namespace as one of its intermediate namespaces? If so, remove the "Previous" namespace and return the original value.*/
	static FName GetSourceForPreviousValue(const FName& InVariableName);

	/**
	* Helper to add a variable to the known list for a parameter map.
	*/
	int32 AddVariable(const FNiagaraVariable& InVar, const FNiagaraVariable& InAliasedVar, FName ModuleName, const UEdGraphPin* InPin, TOptional<FNiagaraVariableMetaData> InMetaData = TOptional<FNiagaraVariableMetaData>());

	/** Get the default value for this variable.*/
	const UEdGraphPin* GetDefaultValuePin(int32 VarIdx) const;

	static FNiagaraVariable ConvertVariableToRapidIterationConstantName(FNiagaraVariable InVar, const TCHAR* InEmitterName, ENiagaraScriptUsage InUsage);

	/**
	If this is variable is a parameter in one of our tracked collections, return it.
	@param InVar	Variable to test.
	@param bMissingParameter	bool set to mark if this parameter was a collection parameter but is now missing from it's collection.
	*/
	UNiagaraParameterCollection* IsParameterCollectionParameter(FNiagaraVariable& InVar, bool& bMissingParameter);

	bool ShouldIgnoreVariableDefault(const FNiagaraVariable& Var)const;
};

class FNiagaraParameterMapHistoryBuilder
{
public:
	/** Collection of the build histories from the graph traversal.*/
	TArray<FNiagaraParameterMapHistory> Histories;

	/** Constructor*/
	FNiagaraParameterMapHistoryBuilder();

	virtual ~FNiagaraParameterMapHistoryBuilder() {};

	/** Add a new parameter map to the array.*/
	int32 CreateParameterMap();

	/** Called in order to set up the correct initial context for an Output node and invokes the UNiagaraNodeOutput's BuildParameterMapHistory method.*/
	void BuildParameterMaps(const UNiagaraNodeOutput* OutputNode, bool bRecursive = true);

	/**
	* Called first during a node's visitation during traversal to identify that a node has been visited.
	*/
	void RegisterNodeVisitation(const UEdGraphNode* Node);

	/** Important. Must be called for each routing of the parameter map. This feeds the list used by TraceParameterMapOutputPin.*/
	int32 RegisterParameterMapPin(int32 WhichParameterMap, const UEdGraphPin* Pin);

	int32 RegisterConstantPin(int32 WhichConstant, const UEdGraphPin* Pin);
	int32 RegisterConstantVariableWrite(int32 WhichParamMapIdx, int32 WhichConstant, int32 WhichVarIdx, bool bIsSettingDefault, bool bLinkNotValue);

	int32  RegisterConstantFromInputPin(const UEdGraphPin* InputPin);

	int32 GetConstantFromInputPin(const UEdGraphPin* InputPin) const;
	int32 GetConstantFromOutputPin(const UEdGraphPin* OutputPin) const;
	int32 GetConstantFromVariableRead(const FNiagaraVariableBase& InVar);
	int32 AddOrGetConstantFromValue(const FString& Value);
	FString GetConstant(int32 InIndex);

	/** Records a write to a DataSet in the appropriate parameter map history */
	void RegisterDataSetWrite(int32 WhichParameterMap, const FNiagaraDataSetID& DataSet);

	uint32 BeginNodeVisitation(int32 WhichParameterMap, const class UNiagaraNode* Node);
	void EndNodeVisitation(int32 WhichParameterMap, uint32 IndexFromBeginNode);

	/** Trace back a pin to whom it was connected to to find the current parameter map to use.*/
	int32 TraceParameterMapOutputPin(const UEdGraphPin* OutputPin);

	void BeginTranslation(const UNiagaraScript* Script);

	void EndTranslation(const UNiagaraScript* Script);

	void BeginTranslation(const FString& EmitterUniqueName);
	void EndTranslation(const FString& EmitterUniqueName);

	void BeginTranslation(const UNiagaraEmitter* Emitter);

	void EndTranslation(const UNiagaraEmitter* Emitter);

	void BeginUsage(ENiagaraScriptUsage InUsage, FName InStageName = FName());
	void EndUsage();


	FGraphTraversalHandle ActivePath;

	/**
	* Record that we have entered a new function scope.
	*/
	void EnterFunction(const FString& InNodeName, const class UNiagaraScript* InScript, const class UNiagaraGraph* InGraph, const class UNiagaraNode* Node);

	/**
	* Record that we have exited a function scope.
	*/
	void ExitFunction(const FString& InNodeName, const class UNiagaraScript* InScript, const class UNiagaraNode* Node);

	/**
	* Record that we have entered an emitter scope.
	*/
	void EnterEmitter(const FString& InEmitterName, const class UNiagaraGraph* InGraph, const class UNiagaraNode* Node);

	/**
	* Record that we have exited an emitter scope.
	*/
	void ExitEmitter(const FString& InEmitterName, const class UNiagaraNode* Node);

	/**
	* Use the current alias map to resolve any aliases in this input variable name.
	*/
	FNiagaraVariable ResolveAliases(const FNiagaraVariable& InVar) const;
	
	/**
	* Has RegisterNodeVisitation been called yet on the owning node of this pin?
	*/
	bool GetPinPreviouslyVisited(const UEdGraphPin* InPin) const;

	/**
	* Has RegisterNodeVisitation been called on the input node yet?
	*/
	bool GetNodePreviouslyVisited(const class UNiagaraNode* Node) const;

	/** If we haven't already visited the owning nodes, do so.*/
	void VisitInputPins(const class UNiagaraNode*, bool bFilterForCompilation);
	
	/** If we haven't already visited the owning node, do so.*/
	void VisitInputPin(const UEdGraphPin* Pin, const class UNiagaraNode*, bool bFilterForCompilation);

	/**
	* Record that a pin writes to the parameter map. The pin name is expected to be the namespaced parameter map version of the name. If any aliases are in place, they are removed.
	*/
	int32 HandleVariableWrite(int32 ParameterMapIndex, const UEdGraphPin* InPin);
	
	/**
	* Record that a variable write to the parameter map. The var name is expected to be the namespaced parameter map version of the name. If any aliases are in place, they are removed.
	*/
	int32 HandleVariableWrite(int32 ParameterMapIndex, const FNiagaraVariable& Var);

	/**
	* Record that a pin reads from the parameter map. The pin name is expected to be the namespaced parameter map version of the name. If any aliases are in place, they are removed.
	*/
	int32 HandleVariableRead(int32 ParameterMapIndex, const UEdGraphPin* InPin, bool RegisterReadsAsVariables, const UEdGraphPin* InDefaultPin, bool bFilterForCompilation, bool& OutUsedDefault);

	int32 HandleExternalVariableRead(int32 ParamMapIdx, const FName& InVarName);

	/**
	* Virtual wrapper to call correct AddVariable method on the target ParameterMapHistory.
	*/
	virtual int32 AddVariableToHistory(FNiagaraParameterMapHistory& History, const FNiagaraVariable& InVar, const FNiagaraVariable& InAliasedVar, const UEdGraphPin* InPin);
	
	/**
	* Get the string that the "Module" namespace maps to currently (if it exists)
	*/
	const FString* GetModuleAlias() const;

	/**
	* Get the string that the "Emitter" namespace maps to currently (if it exists)
	*/
	const FString* GetEmitterAlias() const;

	/** Get the node calling this sub-graph.*/
	const UNiagaraNode* GetCallingContext() const;

	/** Are we currently in a top-level function call context (ie a node in the main graph or an argument into a function for the main graph.)*/
	bool InTopLevelFunctionCall(ENiagaraScriptUsage InFilterScriptType) const;

	/** Helper method to identify any matching input nodes from the calling context node to the input variable.*/
	int32 FindMatchingParameterMapFromContextInputs(const FNiagaraVariable& InVar) const;

	int32 FindMatchingStaticFromContextInputs(const FNiagaraVariable& InVar) const;

	/** In some cases, we don't want all the variables encountered in a traversal. In this case, you can filter 
	*  the map history to only include variables that are relevant to the specific script type. For instance, a System 
	*  script doesn't really care about the Particles namespace.
	*/
	void EnableScriptAllowList(bool bInEnable, ENiagaraScriptUsage InScriptType);

	bool HasCurrentUsageContext() const;
	ENiagaraScriptUsage GetCurrentUsageContext()const;
	ENiagaraScriptUsage GetBaseUsageContext()const;
	bool ContextContains(ENiagaraScriptUsage InUsage) const;

	bool GetIgnoreDisabled() const { return bIgnoreDisabled; }
	void SetIgnoreDisabled(bool bInIgnore) { bIgnoreDisabled = bInIgnore; }

	bool IsInEncounteredFunctionNamespace(const FNiagaraVariable& InVar) const;
	bool IsInEncounteredEmitterNamespace(const FNiagaraVariable& InVar) const;

	/** Register any user or other external variables that could possibly be encountered but may not be declared explicitly. */
	void RegisterEncounterableVariables(TConstArrayView<FNiagaraVariable> Variables);
	const TArray<FNiagaraVariable>& GetEncounterableVariables() const {	return EncounterableExternalVariables;}

	void RegisterExternalStaticVariables(TConstArrayView<FNiagaraVariable> Variables);

	FCompileConstantResolver ConstantResolver;

	bool bShouldBuildSubHistories = true;

	int32 MaxGraphDepthTraversal = INDEX_NONE;
	int32 CurrentGraphDepth = 0;

	void SetConstantByStaticVariable(int32& OutValue, const UEdGraphPin* InDefaultPin);
	void SetConstantByStaticVariable(int32& OutValue, const FNiagaraVariable& Var);
	int32 FindStaticVariable(const FNiagaraVariable& Var) const;

	TArray<FNiagaraVariable> StaticVariables;
	TArray<bool> StaticVariableExportable; // Should we export out these static variables to calling context?

	void GetContextuallyVisitedNodes(TArray<const class UNiagaraNode*>& OutVistedNodes)
	{
		if (ContextuallyVisitedNodes.Num() > 0)
			OutVistedNodes.Append(ContextuallyVisitedNodes.Last());
	}
protected:
	/** Internal helper function to decide whether or not we should use this static variable when merging with other
		graph traversal static variables, like Emitter graphs merging with System graphs. */
	bool IsStaticVariableExportableToOuterScopeBasedOnCurrentContext(const FNiagaraVariable& Var) const;

	/**
	* Generate the internal alias map from the current traversal state.
	*/
	void BuildCurrentAliases();

	/** Helper function called when variables are added to enable the filtering specified in EnableScriptAllowList.*/
	bool ShouldTrackVariable(const FNiagaraVariable& InVar);

	/** Helper method used to take in input script type and determine if the passed in namespaced variable is worth tracking.*/
	bool IsNamespacedVariableRelevantToScriptType(const FNiagaraVariable& InVar, ENiagaraScriptUsage ScriptType);

	/** Contains the hierarchy of nodes leading to the current graph being processed. Usually made up of FunctionCall and Emitter nodes.*/
	TArray<const UNiagaraNode*> CallingContext;

	/** Contains the hierarchy of graphs leading to and including the current graph being processed. Is not in sync with CallingContext as there is an additional 0th entry for the NodeGraphDeepCopy. */
	TArray<const UNiagaraGraph*> CallingGraphContext;

	TArray< TMap<const UEdGraphPin*, int32> > PinToConstantIndices;
	TArray<FString> Constants;
	TArray< TMap<FNiagaraVariableBase, int32> > VariableToConstantIndices;

	/** Tracker for each context level of the parameter map index associated with a given pin. Used to trace parameter maps through the graph.*/
	TArray<TMap<const UEdGraphPin*, int32> > PinToParameterMapIndices;
	/** List of previously visited nodes per context. Note that the same node may be visited multiple times across all graph traversals, but only one time per context level.*/
	TArray<TArray<const class UNiagaraNode*> > ContextuallyVisitedNodes;
	/** Contains the hierarchy of emitter node names leading to the current graph being processed.*/
	TArray<FName> EmitterNameContextStack;
	/** Contains the hierarchy of function call node names leading to the current graph being processed.*/
	TArray<FName> FunctionNameContextStack;
	/** Keeps track of the script usage at the current context level. This allows us to make some decisions about relevence.*/
	TArray<ENiagaraScriptUsage> RelevantScriptUsageContext;
	/** Resolved alias map for the current context level. Rebuilt by BuildCurrentAliases.*/
	FNiagaraAliasContext ResolveAliasContext;
	TArray<FName> ScriptUsageContextNameStack;

	TArray<TArray<FString> > EncounteredFunctionNames;
	TArray<FString> EncounteredEmitterNames;

	
	/** Whether or not the script allow list is active.*/
	bool bFilterByScriptAllowList;
	/** What the script type is that we should be filtering to if the allow list is enabled.*/
	ENiagaraScriptUsage FilterScriptType;

	/** Whether or not to ignore disabled nodes.*/
	bool bIgnoreDisabled;

	/** Whether we want to include ParameterCollection information */
	bool bIncludeParameterCollectionInfo;

	TArray<FNiagaraVariable> EncounterableExternalVariables;
};

class FNiagaraParameterMapHistoryWithMetaDataBuilder : public FNiagaraParameterMapHistoryBuilder
{
public:
	//** Add a graph to the calling graph context stack. Generally used to prime the context stack with the node graph deep copy during precompilation. */
	void AddGraphToCallingGraphContextStack(const UNiagaraGraph* InGraph) { CallingGraphContext.Add(InGraph); };

	/**
	* Virtual wrapper to call correct AddVariable method on the target ParameterMapHistory.
	*/
	virtual int32 AddVariableToHistory(FNiagaraParameterMapHistory& History, const FNiagaraVariable& InVar, const FNiagaraVariable& InAliasedVar, const UEdGraphPin* InPin);
};

struct FNiagaraGraphCachedBuiltHistory : public FNiagaraGraphCachedDataBase
{
	virtual ~FNiagaraGraphCachedBuiltHistory() {};
	virtual void GetStaticVariables(TArray<FNiagaraVariable>& OutVars) { OutVars = StaticVariables; }

	TArray<FNiagaraVariable> StaticVariables;
};