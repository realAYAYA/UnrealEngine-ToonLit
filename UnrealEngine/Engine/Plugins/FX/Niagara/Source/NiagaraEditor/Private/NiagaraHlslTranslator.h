// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet2/CompilerResultsLog.h"
#include "NiagaraScript.h"
#include "NiagaraParameterMapHistory.h"
#include "NiagaraShaderCompilationManager.h"
#include "NiagaraDataInterface.h"
#include "NiagaraScriptSource.h"
#include "NiagaraSimulationStageCompileData.h"

class Error;
class UNiagaraGraph;
class UNiagaraNode;
class UNiagaraNodeFunctionCall;
class UNiagaraNodeCustomHlsl;
class UNiagaraNodeOutput;
class UNiagaraScriptSource;
class UNiagaraNodeEmitter;
class UNiagaraScriptVariable;

UENUM()
enum class ENiagaraDataSetAccessMode : uint8
{
	/** Data set reads and writes use shared counters to add and remove the end of available data. Writes are conditional and read */
	AppendConsume,
	/** Data set is accessed directly at a specific index. */
	Direct,

	Num UMETA(Hidden),
};

struct FNiagaraCustomHlslInclude
{
	bool bIsVirtual = false;
	FString FilePath;

	bool operator==(const FNiagaraCustomHlslInclude& Other) const 
	{ 
		return bIsVirtual == Other.bIsVirtual && FilePath == Other.FilePath; 
	}
};


/** Defines information about the results of a Niagara script compile. */
struct FNiagaraTranslateResults
{
	/** Whether or not HLSL generation was successful */
	bool bHLSLGenSucceeded = false;;

	/** A results log with messages, warnings, and errors which occurred during the compile. */
	TArray<FNiagaraCompileEvent> CompileEvents;
	uint32 NumErrors;
	uint32 NumWarnings;

	TArray<FNiagaraCompileDependency> CompileDependencies;

	TArray<FNiagaraCompilerTag > CompileTags;

	/** A string representation of the compilation output. */
	FString OutputHLSL;

	FNiagaraTranslateResults() : NumErrors(0), NumWarnings(0)
	{
	}

	static ENiagaraScriptCompileStatus TranslateResultsToSummary(const FNiagaraTranslateResults* CompileResults);
};

class FNiagaraCompileRequestData : public FNiagaraCompileRequestDataBase
{
public:
	FNiagaraCompileRequestData()
	{

	}

	virtual void GatherPreCompiledVariables(const FString& InNamespaceFilter, TArray<FNiagaraVariable>& OutVars) const override;
	virtual FName ResolveEmitterAlias(FName VariableName) const override;

	const FString& GetUniqueEmitterName() const { return EmitterUniqueName; }
	void FinishPrecompile(const TArray<FNiagaraVariable>& EncounterableVariables, const TArray<FNiagaraVariable>& InStaticVariables, FCompileConstantResolver ConstantResolver, const TArray<ENiagaraScriptUsage>& UsagesToProcess, const TArray<class UNiagaraSimulationStageBase*>* SimStages, const TArray<FString> EmitterNames);
	virtual int32 GetDependentRequestCount() const override {
		return EmitterData.Num();
	};
	virtual TSharedPtr<FNiagaraCompileRequestDataBase, ESPMode::ThreadSafe> GetDependentRequest(int32 Index) override {
		return EmitterData[Index];
	}
	virtual const FNiagaraCompileRequestDataBase* GetDependentRequest(int32 Index) const override
	{
		return EmitterData[Index].Get();
	}
	void AddRapidIterationParameters(const FNiagaraParameterStore& InParamStore, FCompileConstantResolver InResolver);
	virtual bool GetUseRapidIterationParams() const override { return bUseRapidIterationParams; }

	virtual bool GetDisableDebugSwitches() const override { return bDisableDebugSwitches; }

	// Simulation Stage Variables. Sim stage of 0 is always Spawn/Update
	TArray<FNiagaraSimulationStageCompilationData> CompileSimStageData;

	struct FCompileDataInterfaceData
	{
		FString EmitterName;
		ENiagaraScriptUsage Usage;
		FGuid UsageId;
		FNiagaraVariable Variable;
		TArray<FString> ReadsEmitterParticleData;
	};
	TSharedPtr<TArray<FCompileDataInterfaceData>> SharedCompileDataInterfaceData;

	TArray<FNiagaraVariable> EncounteredVariables;
	FString EmitterUniqueName;
	TArray<TSharedPtr<FNiagaraCompileRequestData, ESPMode::ThreadSafe>> EmitterData;
	TWeakObjectPtr<UNiagaraScriptSource> Source;
	FString SourceName;
	bool bUseRapidIterationParams = true;
	bool bDisableDebugSwitches = false;

	UEnum* ENiagaraScriptCompileStatusEnum = nullptr;
	UEnum* ENiagaraScriptUsageEnum = nullptr;

	TArray<FNiagaraVariable> RapidIterationParams;

	TMap<FGraphTraversalHandle, FString> PinToConstantValues;
	TArray<FNiagaraVariable> StaticVariables;
	TArray<FNiagaraVariable> StaticVariablesWithMultipleWrites;

	void CompareAgainst(FNiagaraGraphCachedBuiltHistory* InCachedDataBase);

	static void SortOutputNodesByDependencies(TArray<class UNiagaraNodeOutput*>& NodesToSort, const TArray<class UNiagaraSimulationStageBase*>* SimStages);
};

class FNiagaraCompileRequestDuplicateData : public FNiagaraCompileRequestDuplicateDataBase
{
public:
	FNiagaraCompileRequestDuplicateData()
	{
	}

	virtual bool IsDuplicateDataFor(UNiagaraSystem* InSystem, UNiagaraEmitter* InEmitter, UNiagaraScript* InScript) const override;
	virtual void GetDuplicatedObjects(TArray<UObject*>& Objects) override;
	virtual const TMap<FName, UNiagaraDataInterface*>& GetObjectNameMap() override;
	virtual const UNiagaraScriptSourceBase* GetScriptSource() const override;
	UNiagaraDataInterface* GetDuplicatedDataInterfaceCDOForClass(UClass* Class) const;

	TArray<FNiagaraParameterMapHistory>& GetPrecomputedHistories() { return PrecompiledHistories; }
	const TArray<FNiagaraParameterMapHistory>& GetPrecomputedHistories() const { return PrecompiledHistories; }

	void DuplicateReferencedGraphs(UNiagaraGraph* InSrcGraph, UNiagaraGraph* InDupeGraph, ENiagaraScriptUsage InUsage, FCompileConstantResolver ConstantResolver, TMap<UNiagaraNodeFunctionCall*, ENiagaraScriptUsage> FunctionsWithUsage = TMap<UNiagaraNodeFunctionCall*, ENiagaraScriptUsage>());
	void DeepCopyGraphs(UNiagaraScriptSource* ScriptSource, ENiagaraScriptUsage InUsage, FCompileConstantResolver ConstantResolver);
	void DeepCopyGraphs(const FVersionedNiagaraEmitter& Emitter);
	
	void FinishPrecompileDuplicate(const TArray<FNiagaraVariable>& EncounterableVariables, const TArray<FNiagaraVariable>& InStaticVariables, FCompileConstantResolver ConstantResolver, const TArray<class UNiagaraSimulationStageBase*>* SimStages, const TArray<FNiagaraVariable>& InParamStore);
	void CreateDataInterfaceCDO(TArrayView<UClass*> VariableDataInterfaces);

	virtual int32 GetDependentRequestCount() const override	{ return EmitterData.Num();	}
	virtual TSharedPtr<FNiagaraCompileRequestDuplicateDataBase, ESPMode::ThreadSafe> GetDependentRequest(int32 Index) override { return EmitterData[Index];	}

	virtual void ReleaseCompilationCopies() override;

	TWeakObjectPtr<UNiagaraSystem> OwningSystem;
	TWeakObjectPtr<UNiagaraEmitter> OwningEmitter;
	TArray<ENiagaraScriptUsage> ValidUsages;

	TWeakObjectPtr<UNiagaraScriptSource> SourceDeepCopy;
	TWeakObjectPtr<UNiagaraGraph> NodeGraphDeepCopy;

	TArray<FNiagaraParameterMapHistory> PrecompiledHistories;

	TArray<FNiagaraVariable> ChangedFromNumericVars;
	FString EmitterUniqueName;
	TArray<TSharedPtr<FNiagaraCompileRequestDuplicateData, ESPMode::ThreadSafe>> EmitterData;

	struct FDuplicatedGraphData
	{
		UNiagaraScript* ClonedScript;
		UNiagaraGraph* ClonedGraph;
		TArray<UEdGraphPin*> CallInputs;
		TArray<UEdGraphPin*> CallOutputs;
		ENiagaraScriptUsage Usage;
		bool bHasNumericParameters;
	};

	TSharedPtr<TMap<const UNiagaraGraph*, TArray<FDuplicatedGraphData>>> SharedSourceGraphToDuplicatedGraphsMap;
	TSharedPtr<TMap<FName, UNiagaraDataInterface*>> SharedNameToDuplicatedDataInterfaceMap;
	TSharedPtr<TMap<UClass*, UNiagaraDataInterface*>> SharedDataInterfaceClassToDuplicatedCDOMap;

protected:
	void DuplicateReferencedGraphsRecursive(UNiagaraGraph* InGraph, const FCompileConstantResolver& ConstantResolver, TMap<UNiagaraNodeFunctionCall*, ENiagaraScriptUsage> FunctionsWithUsage);
private:
	TArray<TWeakObjectPtr<UNiagaraScriptSource>> TrackedScriptSourceCopies;
};


/** Data which is generated from the hlsl by the VectorVMBackend and fed back into the */
struct FNiagaraTranslatorOutput
{
	FNiagaraTranslatorOutput() {}

	FNiagaraVMExecutableData ScriptData;

	/** Ordered table of functions actually called by the VM script. */
	struct FCalledVMFunction
	{
		FString Name;
		TArray<bool> InputParamLocations;
		int32 NumOutputs;
		FCalledVMFunction() :NumOutputs(0) {}
	};
	TArray<FCalledVMFunction> CalledVMFunctionTable;

	FString Errors;

};

struct FCompiledPin
{
	int32 CompilationIndex;
	UEdGraphPin* Pin;

	FCompiledPin(int32 CompilationIndex, UEdGraphPin* Pin) : CompilationIndex(CompilationIndex), Pin(Pin)
	{}
};

enum class ENiagaraCodeChunkMode : uint8
{
	GlobalConstant,
	SystemConstant,
	OwnerConstant,
	EmitterConstant,
	Uniform,
	Source,
	Body,
	SpawnBody,
	UpdateBody,
	InitializerBody,
	SimulationStageBody,
	SimulationStageBodyMax = SimulationStageBody + 100,
	Num,
};

FORCEINLINE uint32 GetTypeHash(const FNiagaraFunctionSignature& Sig)
{
	uint32 Hash = GetTypeHash(Sig.Name);
	for (const FNiagaraVariable& Var : Sig.Inputs)
	{
		Hash = HashCombine(Hash, GetTypeHash(Var));
	}
	for (const FNiagaraVariable& Var : Sig.Outputs)
	{
		Hash = HashCombine(Hash, GetTypeHash(Var));
	}
	Hash = HashCombine(Hash, GetTypeHash(Sig.OwnerName));
	Hash = HashCombine(Hash, GetTypeHash(Sig.ContextStageIndex));
	return Hash;
}

struct FNiagaraCodeChunk
{
	/** Symbol name for the chunk. Cam be empty for some types of chunk. */
	FString SymbolName;
	/** Format definition for incorporating SourceChunks into the final code for this chunk. */
	FString Definition;
	/** Original constant data*/
	FNiagaraVariable Original;
	/** The returned data type of this chunk. */
	FNiagaraTypeDefinition Type;
	/** If this chunk should declare it's symbol name. */
	bool bDecl;
	/** If the chunk is unterminated (no semicolon, because it's a scope or similar */
	bool bIsTerminated;
	/** Chunks used as input for this chunk. */
	TArray<int32> SourceChunks;
	/** Component mask for access to padded uniforms; will be empty except for float2 and float3 uniforms */
	FString ComponentMask;

	ENiagaraCodeChunkMode Mode;

	FNiagaraCodeChunk()
		: bDecl(true)
		, bIsTerminated(true)
		, ComponentMask("")
		, Mode(ENiagaraCodeChunkMode::Num)
	{
		Type = FNiagaraTypeDefinition::GetFloatDef();
	}

	void AddSourceChunk(int32 ChunkIdx)
	{
		SourceChunks.Add(ChunkIdx);
	}

	int32 GetSourceChunk(int32 i)
	{
		return SourceChunks[i];
	}

	void ReplaceSourceIndex(int32 SourceIdx, int32 NewIdx)
	{
		SourceChunks[SourceIdx] = NewIdx;
	}

	bool operator==(const FNiagaraCodeChunk& Other)
	{
		return SymbolName == Other.SymbolName &&
			Definition == Other.Definition &&
			Mode == Other.Mode &&
			Type == Other.Type &&
			bDecl == Other.bDecl &&
			Original == Other.Original &&
			SourceChunks == Other.SourceChunks;
	}
};

class NIAGARAEDITOR_API FHlslNiagaraTranslatorOptions
{
public:
	FHlslNiagaraTranslatorOptions()
		: SimTarget(ENiagaraSimTarget::CPUSim)
		, bParameterRapidIteration(true)
		, bDisableDebugSwitches(false)
	{

	}

	FHlslNiagaraTranslatorOptions(const FHlslNiagaraTranslatorOptions& InOptions)
		: SimTarget(InOptions.SimTarget)
		, InstanceParameterNamespaces(InOptions.InstanceParameterNamespaces)
		, bParameterRapidIteration(InOptions.bParameterRapidIteration)
		, bDisableDebugSwitches(InOptions.bDisableDebugSwitches)
		, OverrideModuleConstants(InOptions.OverrideModuleConstants)
	{
	}

	ENiagaraSimTarget SimTarget;

	/** Any parameters in these namespaces will be pulled from an "InstanceParameters" dataset rather than from the uniform table. */
	TArray<FString> InstanceParameterNamespaces;

	/** Whether or not to treat top-level module variables as external values for rapid iteration without need for compilation.*/
	bool bParameterRapidIteration;

	/** Should we disable debug switches during translation. */
	bool bDisableDebugSwitches;

	/** Whether or not to override top-level module variables with values from the constant override table. This is only used for variables that were candidates for rapid iteration.*/
	TArray<FNiagaraVariable> OverrideModuleConstants;


};

class NIAGARAEDITOR_API FHlslNiagaraTranslationStage
{
public:
	FHlslNiagaraTranslationStage(ENiagaraScriptUsage InScriptUsage, FGuid InUsageId)
		: ScriptUsage(InScriptUsage)
		, UsageId(InUsageId)
	{
	}
		
	ENiagaraScriptUsage ScriptUsage;
	FGuid UsageId;
	UNiagaraNodeOutput* OutputNode = nullptr;
	FString PassNamespace;
	bool bInterpolatePreviousParams = false;
	bool bCopyPreviousParams = true;
	ENiagaraCodeChunkMode ChunkModeIndex = (ENiagaraCodeChunkMode)-1;
	FName IterationSource;
	int32 SimulationStageIndex = -1;
	FName EnabledBinding;
	FName ElementCountXBinding;
	FName ElementCountYBinding;
	FName ElementCountZBinding;
	int32 NumIterations = 1;
	FName NumIterationsBinding;
	ENiagaraSimStageExecuteBehavior ExecuteBehavior = ENiagaraSimStageExecuteBehavior::Always;
	bool bParticleIterationStateEnabled = false;
	FName ParticleIterationStateBinding;
	FIntPoint ParticleIterationStateRange = FIntPoint::ZeroValue;
	bool bUsesAlive = false;
	bool bWritesAlive = false;
	bool bWritesParticles = false;
	bool bPartialParticleUpdate = false;
	bool bGpuDispatchForceLinear = false;
	bool bOverrideGpuDispatchType = false;
	ENiagaraGpuDispatchType OverrideGpuDispatchType = ENiagaraGpuDispatchType::OneD;
	bool bOverrideGpuDispatchNumThreads = false;
	bool bShouldUpdateInitialAttributeValues = false;
	FIntVector OverrideGpuDispatchNumThreads = FIntVector(1, 1, 1);
	TArray<FNiagaraVariable> SetParticleAttributes;
	FString CustomReadFunction;
	FString CustomWriteFunction;
	int32 ParamMapHistoryIndex = INDEX_NONE;

	bool ShouldDoSpawnOnlyLogic() const;
	bool IsRelevantToSpawnForStage(const FNiagaraParameterMapHistory& InHistory, const FNiagaraVariable& InAliasedVar, const FNiagaraVariable& InVar) const;

	bool IsExternalConstantNamespace(const FNiagaraVariable& InVar, ENiagaraScriptUsage InTargetUsage, uint32 InTargetBitmask);
	int32 CurrentCallID = 0;
	bool bCallIDInitialized = false;
};

struct FunctionNodeStackEntry
{
	TSet<FName> UnusedInputs;
	TSet<FString> CulledFunctionNames;
};

class NIAGARAEDITOR_API FHlslNiagaraTranslator
{
public:

	struct FDataSetAccessInfo
	{
		//Variables accessed.
		TArray<FNiagaraVariable> Variables;
		/** Code chunks relating to this access. */
		TArray<int32> CodeChunks;
	};

	struct FNiagaraConstantBuffer
	{
		TArray<FNiagaraVariable> Variables;
		FString StructName;
	};

protected:
	const FNiagaraCompileRequestData* CompileData;
	const FNiagaraCompileRequestDuplicateData* CompileDuplicateData;
	FNiagaraCompileOptions CompileOptions;

	FHlslNiagaraTranslatorOptions TranslationOptions;

	const class UEdGraphSchema_Niagara* Schema;

	/** The set of all generated code chunks for this script. */
	TArray<FNiagaraCodeChunk> CodeChunks;

	/** Array of code chunks of each different type. */
	TArray<int32> ChunksByMode[(int32)ENiagaraCodeChunkMode::Num];

	/**
	Map of Pins to compiled code chunks. Allows easy reuse of previously compiled pins.
	A stack so that we can track pin reuse within function calls but not have cached pins cross talk with subsequent calls to the same function.
	*/
	TArray<TMap<const UEdGraphPin*, int32>> PinToCodeChunks;

	/** The combined output of the compilation of this script. This is temporary and will be reworked soon. */
	FNiagaraTranslatorOutput CompilationOutput;

	/** Message log. Automatically handles marking the NodeGraph with errors. */
	FCompilerResultsLog MessageLog;

	/** Captures information about a script compile. */
	FNiagaraTranslateResults TranslateResults;

	TMap<FName, uint32> GeneratedSymbolCounts;

	FDataSetAccessInfo InstanceRead;
	FDataSetAccessInfo InstanceWrite;

	TMap<FNiagaraDataSetID, TMap<int32, FDataSetAccessInfo>> DataSetReadInfo[(int32)ENiagaraDataSetAccessMode::Num];
	TMap<FNiagaraDataSetID, TMap<int32, FDataSetAccessInfo>> DataSetWriteInfo[(int32)ENiagaraDataSetAccessMode::Num];
	TMap<FNiagaraDataSetID, int32> DataSetWriteConditionalInfo[(int32)ENiagaraDataSetAccessMode::Num];

	FString GetDataSetAccessSymbol(FNiagaraDataSetID DataSet, int32 IndexChunk, bool bRead);
	FORCEINLINE FNiagaraDataSetID GetInstanceDataSetID()const { return FNiagaraDataSetID(TEXT("DataInstance"), ENiagaraDataSetType::ParticleData); }
	FORCEINLINE FNiagaraDataSetID GetSystemEngineDataSetID()const { return FNiagaraDataSetID(TEXT("Engine"), ENiagaraDataSetType::ParticleData); }
	FORCEINLINE FNiagaraDataSetID GetSystemUserDataSetID()const { return FNiagaraDataSetID(TEXT("User"), ENiagaraDataSetType::ParticleData); }
	FORCEINLINE FNiagaraDataSetID GetSystemConstantDataSetID()const { return FNiagaraDataSetID(TEXT("Constant"), ENiagaraDataSetType::ParticleData); }

	/** All functions called in the script. */
	struct FNiagaraFunctionBody
	{
		FString Body;
		TArray<int32> StageIndices;
	};

	TMap<FNiagaraFunctionSignature, FNiagaraFunctionBody> Functions;
	TArray<FNiagaraCustomHlslInclude> FunctionIncludeFilePaths;
	TMap<FNiagaraFunctionSignature, TArray<FName>> FunctionStageWriteTargets;
	TArray<TArray<FName>> ActiveStageWriteTargets;

	TMap<FNiagaraFunctionSignature, TArray<FName>> FunctionStageReadTargets;
	TArray<TArray<FName>> ActiveStageReadTargets;

	void RegisterFunctionCall(ENiagaraScriptUsage ScriptUsage, const FString& InName, const FString& InFullName, const FGuid& CallNodeId, const FString& InFunctionNameSuffix, UNiagaraScriptSource* Source, FNiagaraFunctionSignature& InSignature, bool bIsCustomHlsl, const FString& InCustomHlsl, const TArray<FNiagaraCustomHlslInclude>& InCustomHlslIncludeFilePaths, TArray<int32>& Inputs, TArrayView<UEdGraphPin* const> CallInputs, TArrayView<UEdGraphPin* const> CallOutputs,
		FNiagaraFunctionSignature& OutSignature);
	void GenerateFunctionCall(ENiagaraScriptUsage ScriptUsage, FNiagaraFunctionSignature& FunctionSignature, TArrayView<const int32> Inputs, TArray<int32>& Outputs);
	FString GetFunctionIncludeStatement(const FNiagaraCustomHlslInclude& Include) const;
	FString GetFunctionSignature(const FNiagaraFunctionSignature& Sig);

	/** Compiles an output Pin on a graph node. Caches the result for any future inputs connected to it. */
	int32 CompileOutputPin(const UEdGraphPin* Pin);

	void WriteDataSetContextVars(TMap<FNiagaraDataSetID, TMap<int32, FDataSetAccessInfo>>& DataSetAccessInfo, bool bRead, FString &OutHLSLOutput);
	void WriteDataSetStructDeclarations(TMap<FNiagaraDataSetID, TMap<int32, FDataSetAccessInfo>>& DataSetAccessInfo, bool bRead, FString &OutHLSLOutput);
	void DecomposeVariableAccess(UStruct* Struct, bool bRead, FString IndexSymbol, FString HLSLString);

	FString GetUniqueSymbolName(FName BaseName);

	/** Stack of all function params. */
	struct FFunctionContext
	{
		FString Name;
		FNiagaraFunctionSignature& Signature;
		TArrayView<const int32> Inputs;
		FGuid Id;
		FFunctionContext(const FString& InName, FNiagaraFunctionSignature& InSig, TArrayView<const int32> InInputs, const FGuid& InId)
			: Name(InName)
			, Signature(InSig)
			, Inputs(InInputs)
			, Id(InId)
		{}
	};
	TArray<FFunctionContext> FunctionContextStack;
	const FFunctionContext* FunctionCtx()const { return FunctionContextStack.Num() > 0 ? &FunctionContextStack.Last() : nullptr; }
	void EnterFunction(const FString& Name, FNiagaraFunctionSignature& Signature, TArrayView<const int32> Inputs, const FGuid& InGuid);
	void ExitFunction();
	FString GetCallstack();
	TArray<FGuid> GetCallstackGuids();

	void EnterStatsScope(FNiagaraStatScope StatScope);
	void ExitStatsScope();
	void EnterStatsScope(FNiagaraStatScope StatScope, FString& OutHlsl);
	void ExitStatsScope(FString& OutHlsl);

	FString GeneratedConstantString(float Constant);
	FString GeneratedConstantString(FVector4 Constant);

	/* Add a chunk that is not written to the source, only used as a source chunk for others. */
	int32 AddSourceChunk(FString SymbolName, const FNiagaraTypeDefinition& Type, bool bSanitize = true);

	/** Add a chunk defining a uniform value. */
	int32 AddUniformChunk(FString SymbolName, const FNiagaraVariable& InVariable, ENiagaraCodeChunkMode ChunkMode, bool AddPadding);

	/* Add a chunk that is written to the body of the shader code. */
	int32 AddBodyChunk(FString SymbolName, FString Definition, const FNiagaraTypeDefinition& Type, TArray<int32>& SourceChunks, bool bDecl = true, bool bIsTerminated = true);
	int32 AddBodyChunk(FString SymbolName, FString Definition, const FNiagaraTypeDefinition& Type, int32 SourceChunk, bool bDecl = true, bool bIsTerminated = true);
	int32 AddBodyChunk(FString SymbolName, FString Definition, const FNiagaraTypeDefinition& Type, bool bDecl = true, bool bIsTerminated = true);
	int32 AddBodyComment(const FString& Comment);
	int32 AddBodyChunk(const FString& Definition);


	FString GetFunctionDefinitions();

	FString GetUniqueEmitterName() const;

	void HandleDataInterfaceCall(FNiagaraScriptDataInterfaceCompileInfo& Info, const FNiagaraFunctionSignature& InMatchingSignature);
	void ConvertCompileInfoToParamInfo(const FNiagaraScriptDataInterfaceCompileInfo& InCompileInfo, FNiagaraDataInterfaceGPUParamInfo& OutGPUParamInfo);
public:

	FHlslNiagaraTranslator();
	virtual ~FHlslNiagaraTranslator() {}

	static void Init();
	
	virtual const FNiagaraTranslateResults &Translate(const FNiagaraCompileRequestData* InCompileData, const FNiagaraCompileRequestDuplicateData* InCompileDuplicateData, const FNiagaraCompileOptions& InCompileOptions, FHlslNiagaraTranslatorOptions Options);
	FNiagaraTranslatorOutput &GetTranslateOutput() { return CompilationOutput; }

	virtual int32 CompilePin(const UEdGraphPin* Pin);

	virtual int32 RegisterDataInterface(FNiagaraVariable& Var, UNiagaraDataInterface* DataInterface, bool bPlaceholder, bool bAddParameterMapRead);

	virtual void Operation(class UNiagaraNodeOp* Operation, TArray<int32>& Inputs, TArray<int32>& Outputs);
	virtual void Output(UNiagaraNodeOutput* OutputNode, const TArray<int32>& ComputedInputs);

	virtual int32 GetParameter(const FNiagaraVariable& Parameter);
	virtual int32 GetRapidIterationParameter(const FNiagaraVariable& Parameter);

	bool DisableDebugSwitches() const { return TranslationOptions.bDisableDebugSwitches; }

	bool IsCompileOptionDefined(const TCHAR* InDefineStr);

	virtual int32 GetAttribute(const FNiagaraVariable& Attribute);

	virtual int32 GetConstant(const FNiagaraVariable& Constant, FString* DebugOutputValue = nullptr);
	
	virtual void ReadDataSet(const FNiagaraDataSetID DataSet, const TArray<FNiagaraVariable>& Variable, ENiagaraDataSetAccessMode AccessMode, int32 InputChunk, TArray<int32>& Outputs);
	virtual void WriteDataSet(const FNiagaraDataSetID DataSet, const TArray<FNiagaraVariable>& Variable, ENiagaraDataSetAccessMode AccessMode, const TArray<int32>& Inputs, TArray<int32>& Outputs);
	virtual void ParameterMapSet(class UNiagaraNodeParameterMapSet* SetNode, TArrayView<const FCompiledPin> Inputs, TArray<int32>& Outputs);
	virtual void ParameterMapGet(class UNiagaraNodeParameterMapGet* GetNode, TArrayView<const int32> Inputs, TArray<int32>& Outputs);
	virtual void Emitter(class UNiagaraNodeEmitter* GetNode, TArray<int32>& Inputs, TArray<int32>& Outputs);

	virtual void ParameterMapForBegin(class UNiagaraNodeParameterMapFor* ForNode, int32 IterationCount);
	virtual void ParameterMapForContinue(class UNiagaraNodeParameterMapFor* ForNode, int32 IterationEnabled);
	virtual void ParameterMapForEnd(class UNiagaraNodeParameterMapFor* ForNode);
	virtual int32 ParameterMapForInnerIndex() const;

	void DefineInterpolatedParametersFunction(FString &HlslOutput);
	void DefinePreviousParametersFunction(FString& HlslOutput, TArray<TArray<FNiagaraVariable>>& DataSetVariables, TMap<FNiagaraDataSetID, int32>& DataSetReads, TMap<FNiagaraDataSetID, int32>& DataSetWrites);

	void DefineDataSetReadFunction(FString &HlslOutput, TArray<FNiagaraDataSetID> &ReadDataSets);
	void DefineDataSetWriteFunction(FString &HlslOutput, TArray<FNiagaraDataSetProperties> &WriteDataSets, TArray<int32>& WriteConditionVarIndices);
	void DefineMain(FString &HLSLOutput, const TArray<TArray<FNiagaraVariable>>& DataSetVariables, const TMap<FNiagaraDataSetID, int32>& DataSetReads, const TMap<FNiagaraDataSetID, int32>& DataSetWrites);
	void DefineMainGPUFunctions(const TArray<TArray<FNiagaraVariable>>& DataSetVariables, const TMap<FNiagaraDataSetID, int32>& DataSetReads, const TMap<FNiagaraDataSetID, int32>& DataSetWrites);

	void DefineDataSetVariableReads(FString &HLSLOutput, const FNiagaraDataSetID& Id, int32 DataSetIndex, const TArray<FNiagaraVariable>& ReadVars);
	void DefineDataSetVariableWrites(FString &HlslOutput, const FNiagaraDataSetID& Id, int32 DataSetIndex, const TArray<FNiagaraVariable>& WriteVars);
	void DefineDataInterfaceHLSL(FString &HlslOutput);
	void DefineExternalFunctionsHLSL(FString &HlslOutput);

	// Format string should have up to 5 entries, {{0} = Computed Variable Suffix, {1} = Float or Int, {2} = Data Set Index, {3} = Register Index, {4} Default value for that type.
	void GatherVariableForDataSetAccess(const FNiagaraVariable& Variable, FString Format, int32& RegisterIdxInt, int32& RegisterIdxFloat, int32& RegisterIdxHalf, int32 DataSetIndex, FString InstanceIdxSymbol, FString &HlslOutput, bool bWriteHLSL = true);
	void GatherComponentsForDataSetAccess(UScriptStruct* Struct, FString VariableSymbol, bool bMatrixRoot, TArray<FString>& Components, TArray<ENiagaraBaseTypes>& Types);

	virtual void FunctionCall(UNiagaraNodeFunctionCall* FunctionNode, TArray<int32>& Inputs, TArray<int32>& Outputs);
	void EnterFunctionCallNode(const TSet<FName>& UnusedInputs);
	void ExitFunctionCallNode();
	bool IsFunctionVariableCulledFromCompilation(const FName& InputName) const;
	void CullMapSetInputPin(UEdGraphPin* InputPin);

	virtual void Convert(class UNiagaraNodeConvert* Convert, TArrayView<const int32> Inputs, TArray<int32>& Outputs);
	virtual void If(class UNiagaraNodeIf* IfNode, TArray<FNiagaraVariable>& Vars, int32 Condition, TArray<int32>& PathA, TArray<int32>& PathB, TArray<int32>& Outputs);
	/** Options is a map from selector values to compiled pin code chunk indices */
	virtual void Select(class UNiagaraNodeSelect* SelectNode, int32 Selector, const TArray<FNiagaraVariable>& OutputVariables, TMap<int32, TArray<int32>>& Options, TArray<int32>& Outputs);
	
	void WriteCompilerTag(int32 InputCompileResult, const UEdGraphPin* Pin, bool bEmitMessageOnFailure, FNiagaraCompileEventSeverity FailureSeverity, const FString& Prefix = FString());

	void Message(FNiagaraCompileEventSeverity Severity, FText MessageText, const UNiagaraNode* Node, const UEdGraphPin* Pin, FString ShortDescription = FString(), bool bDismissable = false);
	virtual void Error(FText ErrorText, const UNiagaraNode* Node, const UEdGraphPin* Pin, FString ShortDescription = FString(), bool bDismissable = false);
	virtual void Warning(FText WarningText, const UNiagaraNode* Node, const UEdGraphPin* Pin, FString ShortDescription = FString(), bool bDismissable = false);
	void RegisterCompileDependency(const FNiagaraVariableBase& InVar, FText ErrorText, const UNiagaraNode* Node, const UEdGraphPin* Pin, bool bEmitAsLinker, int32 ParamMapHistoryIdx);
	FString NodePinToMessage(FText MessageText, const UNiagaraNode* Node, const UEdGraphPin* Pin);

	virtual bool GetFunctionParameter(const FNiagaraVariable& Parameter, int32& OutParam)const;
	int32 GetUniqueCallerID();

	virtual bool CanReadAttributes()const;
	virtual ENiagaraScriptUsage GetTargetUsage() const;
	FGuid GetTargetUsageId() const;
	virtual ENiagaraScriptUsage GetCurrentUsage() const;
	virtual ENiagaraSimTarget GetSimulationTarget() const
	{
		return CompilationTarget;
	}

	static bool IsBuiltInHlslType(const FNiagaraTypeDefinition& Type);
	static FString GetStructHlslTypeName(const FNiagaraTypeDefinition& Type);
	static FString GetPropertyHlslTypeName(const FProperty* Property);
	static FString BuildHLSLStructDecl(const FNiagaraTypeDefinition& Type, FText& OutErrorMessage, bool bGpuScript);
	static FString GetHlslDefaultForType(const FNiagaraTypeDefinition& Type);
	static bool IsHlslBuiltinVector(const FNiagaraTypeDefinition& Type);
	static TArray<FName> ConditionPropertyPath(const FNiagaraTypeDefinition& Type, const TArray<FName>& InPath);


	static FString GetSanitizedSymbolName(FStringView SymbolName, bool bCollapseNamespaces=false);
	static FString GetSanitizedDIFunctionName(const FString& FunctionName);
	static FString GetSanitizedFunctionNameSuffix(FString Name);

	/** Replaces all non-ascii characters with a "ASCXXX" string, where XXX is their int value */
	static FString ConvertToAsciiString(FString Name);

	bool AddStructToDefinitionSet(const FNiagaraTypeDefinition& TypeDef);

	FString &GetTranslatedHLSL()
	{
		return HlslOutput;
	}

	static FString GetFunctionSignatureSymbol(const FNiagaraFunctionSignature& Sig);

	static FName GetDataInterfaceName(FName BaseName, const FString& UniqueEmitterName, bool bIsParameterMapDataInterface);

	/** If OutVar can be replaced by a literal constant, it's data is initialized with the correct value and we return true. Returns false otherwise. */
	bool GetLiteralConstantVariable(FNiagaraVariable& OutVar) const;

	/** If Var can be replaced by a another constant variable, or is a constant itself, add the appropriate body chunk and return true. */
	bool HandleBoundConstantVariableToDataSetRead(FNiagaraVariable InVariable, UNiagaraNode* InNode, int32 InParamMapHistoryIdx, int32& Output, const UEdGraphPin* InDefaultPin);

	static FString GenerateFunctionHlslPrototype(FStringView InVariableName, const FNiagaraFunctionSignature& FunctionSignature);
	void FillVariableWithDefaultValue(FNiagaraVariable& InVar, const UEdGraphPin* InDefaultPin);
	void FillVariableWithDefaultValue(int32& OutValue, const UEdGraphPin* InDefaultPin);

	void SetConstantByStaticVariable(int32& OutValue, const UEdGraphPin* InDefaultPin, FString* DebugString = nullptr);
	void SetConstantByStaticVariable(int32& OutValue, const FNiagaraVariable& Var, FString* DebugString = nullptr);
	void SetConstantByStaticVariable(FNiagaraVariable& OutValue, const FNiagaraVariable& Var, FString* DebugString = nullptr);
	void SetConstantByStaticVariable(FNiagaraVariable& OutValue, const UEdGraphPin* InDefaultPin, FString* DebugString = nullptr);

	int32 MakeStaticVariableDirect(const UEdGraphPin* InDefaultPin);

	bool IsEventSpawnScript()const;

private:
	void InitializeParameterMapDefaults(int32 ParamMapHistoryIdx);
	void HandleParameterRead(int32 ParamMapHistoryIdx, const FNiagaraVariable& Var, const UEdGraphPin* DefaultPin, UNiagaraNode* ErrorNode, int32& OutputChunkId, TOptional<ENiagaraDefaultMode> DefaultMode = TOptional<ENiagaraDefaultMode>(), TOptional<FNiagaraScriptVariableBinding> DefaultBinding = TOptional<FNiagaraScriptVariableBinding>(), bool bTreatAsUnknownParameterMap = false, bool bIgnoreDefaultSetFirst = false);
	bool ShouldConsiderTargetParameterMap(ENiagaraScriptUsage InUsage) const;
	FString BuildParameterMapHlslDefinitions(TArray<FNiagaraVariable>& PrimaryDataSetOutputEntries);
	void BuildMissingDefaults();
	void FinalResolveNamespacedTokens(const FString& ParameterMapInstanceNamespace, TArray<FString>& Tokens, TArray<FString>& ValidChildNamespaces, FNiagaraParameterMapHistoryBuilder& Builder, TArray<FNiagaraVariable>& UniqueParameterMapEntriesAliasesIntact, TArray<FNiagaraVariable>& UniqueParameterMapEntries, int32 ParamMapHistoryIdx, UNiagaraNode* InNodeForErrorReporting);

	void HandleNamespacedExternalVariablesToDataSetRead(TArray<FNiagaraVariable>& InDataSetVars, FString InNamespaceStr);

	void FindConstantValue(int32 InputCompileResult, const FNiagaraTypeDefinition& TypeDef, FString& Value, FNiagaraVariable& Variable);

	// For GPU simulations we have to special case some variables and pass them view shader parameters rather than the uniform buffer as they vary from CPU simulations
	bool IsVariableInUniformBuffer(const FNiagaraVariable& Variable) const;

	FString ComputeMatrixColumnAccess(const FString& Name);
	FString ComputeMatrixRowAccess(const FString& Name);

	bool ParseDIFunctionSpecifiers(UNiagaraNode* NodeForErrorReporting, FNiagaraFunctionSignature& Sig, TArray<FString>& Tokens, int32& TokenIdx);
	void HandleCustomHlslNode(UNiagaraNodeCustomHlsl* CustomFunctionHlsl, ENiagaraScriptUsage& OutScriptUsage, FString& OutName, FString& OutFullName, bool& bOutCustomHlsl, FString& OutCustomHlsl, TArray<FNiagaraCustomHlslInclude>& OutCustomHlslIncludeFilePaths,
		FNiagaraFunctionSignature& OutSignature, TArray<int32>& Inputs);
	void ProcessCustomHlsl(const FString& InCustomHlsl, ENiagaraScriptUsage InUsage, const FNiagaraFunctionSignature& InSignature, const TArray<int32>& Inputs, UNiagaraNode* NodeForErrorReporting, FString& OutCustomHlsl, FNiagaraFunctionSignature& OutSignature);
	void HandleSimStageSetupAndTeardown(int32 InWhichStage, FString& OutHlsl);
	
	// Add a raw float constant chunk
	int32 GetConstantDirect(float InValue);
	int32 GetConstantDirect(bool InValue);
	int32 GetConstantDirect(int InValue);

	FNiagaraTypeDefinition GetChildType(const FNiagaraTypeDefinition& BaseType, const FName& PropertyName);
	FString NamePathToString(const FString& Prefix, const FNiagaraTypeDefinition& RootType, const TArray<FName>& NamePath);
	FString GenerateAssignment(const FNiagaraTypeDefinition& SrcType, const TArray<FName>& SrcPath, const FNiagaraTypeDefinition& DestType, const TArray<FName>& DestPath);

	//Generates the code for the passed chunk.
	FString GetCode(FNiagaraCodeChunk& Chunk);
	FString GetCode(int32 ChunkIdx);
	//Retreives the code for this chunk being used as a source for another chunk
	FString GetCodeAsSource(int32 ChunkIdx);

	// Generate a structure initializer string
	// Returns true if we generated the structure successfully or false if we encounter something we could not handle
	bool GenerateStructInitializer(TStringBuilder<128>& InitializerString, UStruct* UserDefinedStruct, const void* StructData, int32 ByteOffset = 0);
	// Convert a variable with actual data into a constant string
	FString GenerateConstantString(const FNiagaraVariable& Constant);

	// Takes the current script state (interpolated or not) and determines the correct context variable.
	FString GetParameterMapInstanceName(int32 ParamMapHistoryIdx);
	
	// Register a System/Engine/read-only variable in its namespaced form
	bool ParameterMapRegisterExternalConstantNamespaceVariable(FNiagaraVariable InVariable, UNiagaraNode* InNode, int32 InParamMapHistoryIdx, int32& Output, const UEdGraphPin* InDefaultPin);

	// Register an attribute in its non namespaced form
	bool ParameterMapRegisterNamespaceAttributeVariable(const FNiagaraVariable& InVariable, UNiagaraNode* InNode, int32 InParamMapHistoryIdx, int32& Output);
	// Register an attribute in its namespaced form
	bool ParameterMapRegisterUniformAttributeVariable(const FNiagaraVariable& InVariable, UNiagaraNode* InNode, int32 InParamMapHistoryIdx, int32& Output);

    // Checks that the Partices.ID parameter is only used if persistent IDs are active
    void ValidateParticleIDUsage();
	bool ValidateTypePins(const UNiagaraNode* NodeToValidate);
	void GenerateFunctionSignature(ENiagaraScriptUsage ScriptUsage, FString InName, const FString& InFullName, const FString& InFunctionNameSuffix, UNiagaraGraph* FuncGraph, TArray<int32>& Inputs, 
		bool bHadNumericInputs, bool bHasParameterMapParameters, TArray<UEdGraphPin*> StaticSwitchValues, FNiagaraFunctionSignature& OutSig) const;

	void ValidateFailIfPreviouslyNotSet(const FNiagaraVariable& InVar, bool& bFailIfNotSet);

	bool ShouldInterpolateParameter(const FNiagaraVariable& Parameter);

	void UpdateStaticSwitchConstants(UEdGraphNode* Node);

	bool IsBulkSystemScript() const;
	bool IsSpawnScript() const;
	bool RequiresInterpolation() const;

	template<typename T>
	void BuildConstantBuffer(ENiagaraCodeChunkMode ChunkMode);

	void TrimAttributes(const FNiagaraCompileOptions& InCompileOptions, TArray<FNiagaraVariable>& Attributes);

	// Specific method to reconcile whether the default value has been implicitly or explicitly set for a namespaced var added to the param map.
	void RecordParamMapDefinedAttributeToNamespaceVar(const FNiagaraVariable& VarToRecord, const UEdGraphPin* VarAssociatedDefaultPin);
	
	/** Map of symbol names to count of times it's been used. Used for generating unique symbol names. */
	TMap<FName, uint32> SymbolCounts;

	//Set of non-builtin structs we have to define in hlsl.
	TArray<FNiagaraTypeDefinition> StructsToDefine;

	// Keep track of all the paths that the parameter maps can take through the graph.
	TArray<FNiagaraParameterMapHistory> ParamMapHistories;

	// Keep track of which parameter map history this came from.
	TArray<int32> ParamMapHistoriesSourceInOtherHistories;

	// Keep track of the other output nodes in the graph's histories so that we can make sure to 
	// create any variables that are needed downstream.
	TArray<FNiagaraParameterMapHistory> OtherOutputParamMapHistories;

	// All of the variables arrays in the other histories converted to sanitized HLSL format. Used in parsing custom hlsl nodes.
	TArray< TArray<FNiagaraVariable> > OtherOutputParamMapHistoriesSanitizedVariables;

	// Make sure that the function call names match up on the second traversal.
	FNiagaraParameterMapHistoryBuilder ActiveHistoryForFunctionCalls;

	// Synced to the ParamMapHistories.
	TArray<TArray<int32>> ParamMapSetVariablesToChunks;

	// Used to keep track of contextual information about the currently compiled function node
	TArray<FunctionNodeStackEntry> FunctionNodeStack;

	// Synced to the System uniforms encountered for parameter maps thus far.
	struct UniformVariableInfo
	{
		FNiagaraVariable Variable;
		int32 ChunkIndex;
		int32 ChunkMode;
	};

	struct FVarAndDefaultSource
	{
		FNiagaraVariable Variable;
		bool bDefaultExplicit = false; //Whether or not the default value of the variable is explicit, e.g. there is an explicit value on a pin, explicit binding, or explicit custom initialization.
	};

	TMap<FName, UniformVariableInfo> ParamMapDefinedSystemVars; // Map from the defined constants to the uniform chunk expressing them (i.e. have we encountered before in this graph?)

	// Synced to the EmitterParameter uniforms encountered for parameter maps thus far.
	TMap<FName, int32> ParamMapDefinedEmitterParameterVarsToUniformChunks; // Map from the variable name exposed by the emitter as a parameter to the uniform chunk expressing it (i.e. have we encountered before in this graph?)
	TMap<FName, FNiagaraVariable> ParamMapDefinedEmitterParameterToNamespaceVars; // Map from defined parameter to the Namespaced variable expressing it.

	// Synced to the Attributes encountered for parameter maps thus far.
	TMap<FName, int32> ParamMapDefinedAttributesToUniformChunks; // Map from the variable name exposed as a attribute to the uniform chunk expressing it (i.e. have we encountered before in this graph?)
	TMap<FName, FVarAndDefaultSource> ParamMapDefinedAttributesToNamespaceVars; // Map from defined parameter to the Namespaced variable expressing it.

	// Synced to the external variables used when bulk compiling system scripts.
	TArray<FNiagaraVariable> ExternalVariablesForBulkUsage;

	// List of primary output variables encountered that need to be properly handled in spawn scripts.
	TArray<FNiagaraVariable> UniqueVars;

	// List of variables for interpolated spawning
	TArray<FNiagaraVariable> InterpSpawnVariables;

	// Map of primary ouput variable description to its default value pin
	TMap<FNiagaraVariable, const UEdGraphPin*> UniqueVarToDefaultPin;
	
	// Map of primary output variable description to whether or not it came from this script's parameter map
	TMap<FNiagaraVariable, bool> UniqueVarToWriteToParamMap;
	
	// Map ofthe primary output variable description to the actual chunk id that wrote to it.
	TMap<FNiagaraVariable, int32> UniqueVarToChunk;

	TArray<int32> ParameterMapForIndexStack;

	// Strings to be inserted within the main function
	TArray<TArray<FString>> PerStageMainPreSimulateChunks;

	// read and write data set indices
	int32 ReadIdx;
	int32 WriteIdx;

	// Parameter data per data interface.
	FNiagaraShaderScriptParametersMetadata ShaderScriptParametersMetadata;
	
	/** Stack of currently tracked stats scopes. */
	TArray<int32> StatScopeStack;

	FString HlslOutput;

	ENiagaraSimTarget CompilationTarget;

	// Used to keep track of which output node we are working back from. This allows us 
	// to find the right parameter map.
	TArray<int32> CurrentParamMapIndices;

	ENiagaraCodeChunkMode CurrentBodyChunkMode;

	TArray<FHlslNiagaraTranslationStage> TranslationStages;
	int32 ActiveStageIdx;
	bool bInitializedDefaults;
	bool bEnforceStrictTypesValidations;

	TArray<const UEdGraphPin*> CurrentDefaultPinTraversal;
	// Variables that need to be initialized based on some other variable's value at the end of spawn.
	TArray<FNiagaraVariable> InitialNamespaceVariablesMissingDefault;
	// Variables that need to be initialized in the body or at the end of spawn.
	TArray<FNiagaraVariable> DeferredVariablesMissingDefault;
};
