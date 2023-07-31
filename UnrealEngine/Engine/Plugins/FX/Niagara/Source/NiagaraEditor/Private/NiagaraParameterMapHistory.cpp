// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraParameterMapHistory.h"

#include "NiagaraEditorCommon.h"
#include "NiagaraHlslTranslator.h"
#include "NiagaraSystem.h"
#include "NiagaraGraph.h"
#include "EdGraphSchema_Niagara.h"
#include "NiagaraNode.h"
#include "NiagaraCompiler.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraNodeEmitter.h"
#include "NiagaraNodeParameterMapGet.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#include "NiagaraParameterCollection.h"
#include "NiagaraConstants.h"
#include "NiagaraNodeStaticSwitch.h"
#include "NiagaraEditorModule.h"
#include "INiagaraEditorTypeUtilities.h"
#include "NiagaraEditorModule.h"
#include "NiagaraScriptVariable.h"
#include "NiagaraNodeParameterMapSet.h"
#include "UObject/UObjectThreadContext.h"
#include "Modules/ModuleManager.h"
#include "NiagaraEditorModule.h"

#define LOCTEXT_NAMESPACE "NiagaraEditor"

static int32 GNiagaraLogNamespaceFixup = 0;
static FAutoConsoleVariableRef CVarNiagaraLogNamespaceFixup(
	TEXT("fx.NiagaraLogNamespaceFixup"),
	GNiagaraLogNamespaceFixup,
	TEXT("Log matched variables and pin name changes in precompile. \n"),
	ECVF_Default
);

static int32 GNiagaraForcePrecompilerCullDataset = 0;
static FAutoConsoleVariableRef CVarNiagaraForcePrecompilerCullDataset(
	TEXT("fx.NiagaraEnablePrecompilerNamespaceDatasetCulling"),
	GNiagaraForcePrecompilerCullDataset,
	TEXT("Force the namespace fixup precompiler process to cull unused Dataset parameters. Only enabled if fx.NiagaraEnablePrecompilerNamespaceFixup is also enabled. \n"),
	ECVF_Default
);

//#define NIAGARA_SHOULD_DO_PER_VARIABLE_SUBSTRING_MATCH_LOGGING 1
#if defined(NIAGARA_SHOULD_DO_PER_VARIABLE_SUBSTRING_MATCH_LOGGING)
static TArray<FString> CheckTheseNameSubstrings;// = { TEXT("XXXX") };

bool NiagaraDebugShouldLogEntriesByNameSubstring(const FString& InNameStr)
{
	for (const FString& Str : CheckTheseNameSubstrings)
	{
		if (InNameStr.EndsWith(Str))
		{
			return true;
		}
	}

	return false;
}
bool NiagaraDebugShouldLogEntriesByNameSubstring(const FName& InName)
{
	if (CheckTheseNameSubstrings.Num() == 0)
	{
		return false;
	}
	
	FString NameStr = InName.ToString();
	return NiagaraDebugShouldLogEntriesByNameSubstring(NameStr);
}
#else

#define NiagaraDebugShouldLogEntriesByNameSubstring(expr) (false) // Because the code may do specific allocations, we want to bypass the expression

#endif

void FGraphTraversalHandle::Push(const UEdGraphNode* Node)
{
	ensure(Node->NodeGuid.IsValid());
	Path.Push(Node->NodeGuid);
	FriendlyPath.Push(Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
}

void FGraphTraversalHandle::Push(const UEdGraphPin* Pin)
{
	if (Pin->PersistentGuid.IsValid())
	{
		Path.Push(Pin->PersistentGuid);
	}
	else if (Pin->GetOwningNode())
	{
		FGuid NodeGuid = Pin->GetOwningNode()->NodeGuid;
		int32 Index = Pin->GetOwningNode()->GetPinIndex((UEdGraphPin*)Pin);
		ensure(NodeGuid.IsValid());
		NodeGuid.A += Index; // This is a bit hacky, but the chance of collisions within the same graph is really low and we want a unique value for this graph.
		Path.Push(NodeGuid);
	}
	
	FriendlyPath.Push(Pin->GetOwningNode()->GetNodeTitle(ENodeTitleType::FullTitle).ToString() + TEXT("->") + Pin->PinName.ToString());
}

void FGraphTraversalHandle::Pop()
{
	Path.Pop();
	FriendlyPath.Pop();
}

FNiagaraParameterMapHistory::FNiagaraParameterMapHistory() 
{
	OriginatingScriptUsage = ENiagaraScriptUsage::Function;
}

void FNiagaraParameterMapHistory::GetValidNamespacesForReading(const UNiagaraScript* InScript, TArray<FString>& OutputNamespaces)
{
	GetValidNamespacesForReading(InScript->GetUsage(), 0, OutputNamespaces);
}

void FNiagaraParameterMapHistory::GetValidNamespacesForReading(ENiagaraScriptUsage InScriptUsage, int32 InUsageBitmask, TArray<FString>& OutputNamespaces)
{
	TArray<ENiagaraScriptUsage> SupportedContexts;
	SupportedContexts.Add(InScriptUsage);
	if (UNiagaraScript::IsStandaloneScript(InScriptUsage))
	{
		SupportedContexts.Append(UNiagaraScript::GetSupportedUsageContextsForBitmask(InUsageBitmask));
	}

	OutputNamespaces.Add(PARAM_MAP_MODULE_STR);
	OutputNamespaces.Add(PARAM_MAP_ENGINE_STR);
	OutputNamespaces.Add(PARAM_MAP_NPC_STR);
	OutputNamespaces.Add(PARAM_MAP_USER_STR);
	OutputNamespaces.Add(PARAM_MAP_SYSTEM_STR);
	OutputNamespaces.Add(PARAM_MAP_EMITTER_STR);
	OutputNamespaces.Add(PARAM_MAP_INDICES_STR);

	for (ENiagaraScriptUsage Usage : SupportedContexts)
	{
		if (UNiagaraScript::IsParticleScript(Usage))
		{
			OutputNamespaces.Add(PARAM_MAP_ATTRIBUTE_STR);
			break;
		}
	}
}

FString FNiagaraParameterMapHistory::GetNamespace(const FNiagaraVariable& InVar, bool bIncludeDelimiter)
{
	TArray<FString> SplitName;
	InVar.GetName().ToString().ParseIntoArray(SplitName, TEXT("."));

	check(SplitName.Num() > 0);

	if (bIncludeDelimiter)
	{
		return SplitName[0] + TEXT(".");
	}
	else
	{
		return SplitName[0];
	}
}

bool FNiagaraParameterMapHistory::IsValidNamespaceForReading(ENiagaraScriptUsage InScriptUsage, int32 InUsageBitmask, FString Namespace)
{
	TArray<FString> OutputNamespaces;
	GetValidNamespacesForReading(InScriptUsage, InUsageBitmask, OutputNamespaces);

	TArray<FString> ConcernedNamespaces;
	ConcernedNamespaces.Add(PARAM_MAP_MODULE_STR);
	ConcernedNamespaces.Add(PARAM_MAP_ENGINE_STR);
	ConcernedNamespaces.Add(PARAM_MAP_NPC_STR);
	ConcernedNamespaces.Add(PARAM_MAP_USER_STR);
	ConcernedNamespaces.Add(PARAM_MAP_SYSTEM_STR);
	ConcernedNamespaces.Add(PARAM_MAP_EMITTER_STR);
	ConcernedNamespaces.Add(PARAM_MAP_ATTRIBUTE_STR);
	ConcernedNamespaces.Add(PARAM_MAP_INDICES_STR);

	
	if (!Namespace.EndsWith(TEXT(".")))
	{
		Namespace.Append(TEXT("."));
	}

	// Pass if we are in the allowed list
	for (const FString& ValidNamespace : OutputNamespaces)
	{
		if (Namespace.StartsWith(ValidNamespace))
		{
			return true;
		}
	}

	// Only fail if we're using a namespace that we know is one of the reserved ones.
	for (const FString& ConcernedNamespace : ConcernedNamespaces)
	{
		if (Namespace.StartsWith(ConcernedNamespace))
		{
			return false;
		}
	}

	// This means that we are using a namespace that isn't one of the primary engine namespaces, so we don't care and let it go.
	return true;
}


int32 FNiagaraParameterMapHistory::RegisterParameterMapPin(const UEdGraphPin* Pin)
{
	int32 RetIdx =  MapPinHistory.Add(Pin);
	return RetIdx;
}

void FNiagaraParameterMapHistory::RegisterConstantPin(const FGraphTraversalHandle& InTraversalPath, const UEdGraphPin* InPin, const FString& InValue)
{
	if (UEdGraphSchema_Niagara::IsStaticPin(InPin))
	{
		FGraphTraversalHandle Handle = InTraversalPath;
		Handle.Push(InPin);
		
		FString* FoundValue = PinToConstantValues.Find(Handle);
		if (FoundValue != nullptr)
		{
			ensure(*FoundValue == InValue);
		}
		PinToConstantValues.Add(Handle, InValue);

		if (UNiagaraScript::LogCompileStaticVars > 0 || NiagaraDebugShouldLogEntriesByNameSubstring(Handle.ToString(true)))
		{
			UE_LOG(LogNiagaraEditor, Log, TEXT("RegisterConstantPin \"%s\" Pin: \"%s\""), *InValue, *Handle.ToString(true));
		}
	}
}


void FNiagaraParameterMapHistory::RegisterConstantVariableWrite(const FString& InValue, int32 VarIdx, bool bIsSettingDefault, bool bLinkNotValue)
{
	
	FNiagaraEditorModule& NiagaraEditorModule = FModuleManager::GetModuleChecked<FNiagaraEditorModule>("NiagaraEditor");

	if (bIsSettingDefault == false && PerVariableConstantValue.Num() > VarIdx && VarIdx >= 0)
	{
		PerVariableConstantValue[VarIdx].AddUnique(InValue);

		if (UNiagaraScript::LogCompileStaticVars > 0 || NiagaraDebugShouldLogEntriesByNameSubstring(Variables[VarIdx].GetName().ToString()))
		{
			UE_LOG(LogNiagaraEditor, Log, TEXT("RegisterConstantVariableWrite Value \"%s\" Var: \"%s\""), *InValue, *Variables[VarIdx].GetName().ToString());
		}
	}
	else if (bIsSettingDefault == true && PerVariableConstantDefaultValue.Num() > VarIdx && VarIdx >= 0)
	{
		PerVariableConstantDefaultValue[VarIdx].AddUnique(InValue);

		if (UNiagaraScript::LogCompileStaticVars > 0 || NiagaraDebugShouldLogEntriesByNameSubstring(Variables[VarIdx].GetName().ToString()))
		{
			UE_LOG(LogNiagaraEditor, Log, TEXT("RegisterConstantVariableWrite Default \"%s\" Var: \"%s\""), *InValue, *Variables[VarIdx].GetName().ToString());
		}
	}

	
}

uint32 FNiagaraParameterMapHistory::BeginNodeVisitation(const UNiagaraNode* Node)
{
	uint32 AddedIndex = MapNodeVisitations.Add(Node);
	MapNodeVariableMetaData.Add(TTuple<uint32, uint32>(Variables.Num(), 0));
	check(MapNodeVisitations.Num() == MapNodeVariableMetaData.Num());
	return AddedIndex;
}

void FNiagaraParameterMapHistory::EndNodeVisitation(uint32 IndexFromBeginNode)
{
	check(IndexFromBeginNode < (uint32)MapNodeVisitations.Num());
	check(MapNodeVisitations.Num() == MapNodeVariableMetaData.Num());
	MapNodeVariableMetaData[IndexFromBeginNode].Value = Variables.Num();
}


int32 FNiagaraParameterMapHistory::FindVariableByName(const FName& VariableName, bool bAllowPartialMatch) const
{
	if (!bAllowPartialMatch)
	{
		int32 FoundIdx = Variables.IndexOfByPredicate([&](const FNiagaraVariable& InObj) -> bool
		{
			return (InObj.GetName() == VariableName);
		});

		return FoundIdx;
	}
	else
	{
		return FNiagaraVariable::SearchArrayForPartialNameMatch(Variables, VariableName);
	}
}


int32 FNiagaraParameterMapHistory::FindVariable(const FName& VariableName, const FNiagaraTypeDefinition& Type) const
{
	int32 FoundIdx = Variables.IndexOfByPredicate([&](const FNiagaraVariable& InObj) -> bool
	{
		return (InObj.GetName() == VariableName && InObj.GetType() == Type);
	});

	return FoundIdx;
}

int32 FNiagaraParameterMapHistory::AddVariable(
	  const FNiagaraVariable& InVar
	, const FNiagaraVariable& InAliasedVar
	, FName ModuleName
	, const UEdGraphPin* InPin
	, TOptional<FNiagaraVariableMetaData> InMetaData /*= TOptional<FNiagaraVariableMetaData>()*/)
{
	FNiagaraVariable Var = InVar;

	int32 FoundIdx = FindVariable(Var.GetName(), Var.GetType());
	if (FoundIdx == -1)
	{
		FoundIdx = Variables.Add(Var);
		VariablesWithOriginalAliasesIntact.Add(InAliasedVar);
		PerVariableWarnings.AddDefaulted(1);
		PerVariableWriteHistory.AddDefaulted(1);
		PerVariableReadHistory.AddDefaulted(1);
		PerVariableConstantValue.AddDefaulted(1);
		PerVariableConstantDefaultValue.AddDefaulted(1);

		if (InMetaData.IsSet())
		{
			VariableMetaData.Add(InMetaData.GetValue());
			check(Variables.Num() == VariableMetaData.Num());
		}
	}
	else if (Variables[FoundIdx].GetType() != Var.GetType())
	{
		PerVariableWarnings[FoundIdx].Append(FString::Printf(TEXT("Type mismatch %s instead of %s in map!"), *Var.GetType().GetName(), *Variables[FoundIdx].GetType().GetName()));
	}

	if (InPin != nullptr)
	{
		PerVariableWriteHistory[FoundIdx].Emplace(InPin, ModuleName);
	}

	check(Variables.Num() == PerVariableWarnings.Num());
	check(Variables.Num() == PerVariableReadHistory.Num());
	check(Variables.Num() == PerVariableWriteHistory.Num());
	check(Variables.Num() == PerVariableConstantValue.Num());
	check(Variables.Num() == PerVariableConstantDefaultValue.Num());

	return FoundIdx;
}

int32 FNiagaraParameterMapHistory::AddExternalVariable(const FNiagaraVariable& Var)
{
	return AddVariable(Var, Var, NAME_None, nullptr);
}

const UEdGraphPin* FNiagaraParameterMapHistory::GetFinalPin() const
{
	if (MapPinHistory.Num() > 0)
	{
		return MapPinHistory[MapPinHistory.Num() - 1];
	}
	return nullptr;
}

const UEdGraphPin* FNiagaraParameterMapHistory::GetOriginalPin() const
{
	if (MapPinHistory.Num() > 0)
	{
		return MapPinHistory[0];
	}
	return nullptr;
}


FName FNiagaraParameterMapHistory::ResolveEmitterAlias(const FName& InName, const FString& InAlias)
{
	// If the alias is empty than the name can't be resolved.
	if (InAlias.IsEmpty())
	{
		return InName;
	}

	FNiagaraVariable Var(FNiagaraTypeDefinition::GetFloatDef(), InName);
	FNiagaraAliasContext ResolveAliasesContext(FNiagaraAliasContext::ERapidIterationParameterMode::EmitterOrParticleScript);
	ResolveAliasesContext.ChangeEmitterToEmitterName(InAlias);
	Var = FNiagaraUtilities::ResolveAliases(Var, ResolveAliasesContext);
	return Var.GetName();
}



FString FNiagaraParameterMapHistory::MakeSafeNamespaceString(const FString& InStr)
{
	FString  Sanitized = FHlslNiagaraTranslator::GetSanitizedSymbolName(InStr);
	return Sanitized;
}


FNiagaraVariable FNiagaraParameterMapHistory::ResolveAsBasicAttribute(const FNiagaraVariable& InVar, bool bSanitizeString)
{
	if (IsAttribute(InVar))
	{
		FString ParamName = InVar.GetName().ToString();
		ParamName.RemoveAt(0, FString(PARAM_MAP_ATTRIBUTE_STR).Len());

		if (bSanitizeString)
		{
			ParamName = MakeSafeNamespaceString(ParamName);
		}
		FNiagaraVariable RetVar = InVar;
		RetVar.SetName(*ParamName);
		return RetVar;
	}
	else
	{
		return InVar;
	}
}

FNiagaraVariable FNiagaraParameterMapHistory::BasicAttributeToNamespacedAttribute(const FNiagaraVariable& InVar, bool bSanitizeString)
{
	FString ParamName = InVar.GetName().ToString();
	ParamName.InsertAt(0, FString(PARAM_MAP_ATTRIBUTE_STR));

	if (bSanitizeString)
	{
		ParamName = MakeSafeNamespaceString(ParamName);
	}

	FNiagaraVariable RetVar = InVar;
	RetVar.SetName(*ParamName);
	return RetVar;
}

FNiagaraVariable FNiagaraParameterMapHistory::VariableToNamespacedVariable(const FNiagaraVariable& InVar, FString Namespace)
{
	FString ParamName = Namespace;
	if (Namespace.EndsWith(TEXT(".")))
	{
		ParamName += InVar.GetName().ToString();
	}
	else
	{
		ParamName += TEXT(".") + InVar.GetName().ToString();
	}
	

	FNiagaraVariable RetVar = InVar;
	RetVar.SetName(*ParamName);
	return RetVar;
}

bool FNiagaraParameterMapHistory::IsInNamespace(const FNiagaraVariableBase& InVar, const FString& Namespace)
{
	const bool FastTest = InVar.IsInNameSpace(Namespace);

	// Leaving old code commented out for now just as a reference to what was before.
	//if (Namespace.EndsWith(TEXT(".")))
	//{
	//	bool SlowTest =  InVar.GetName().ToString().StartsWith(Namespace);
	//	check(FastTest == SlowTest);
	//	return FastTest;
	//}
	//else
	//{
	//	bool SlowTest = InVar.GetName().ToString().StartsWith(Namespace + TEXT("."));
	//	check(FastTest == SlowTest);
	//	return FastTest;
	//}

	return FastTest;
}

bool FNiagaraParameterMapHistory::IsAliasedModuleParameter(const FNiagaraVariable& InVar)
{
	return IsInNamespace(InVar, PARAM_MAP_MODULE_STR);
}

bool FNiagaraParameterMapHistory::IsAliasedEmitterParameter(const FNiagaraVariable& InVar)
{
	return IsInNamespace(InVar, PARAM_MAP_EMITTER_STR);
}

bool FNiagaraParameterMapHistory::IsAliasedEmitterParameter(const FString& InVarName)
{
	return IsAliasedEmitterParameter(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), *InVarName));
}


bool FNiagaraParameterMapHistory::IsSystemParameter(const FNiagaraVariable& InVar)
{
	return IsInNamespace(InVar, PARAM_MAP_SYSTEM_STR);
}

bool FNiagaraParameterMapHistory::IsEngineParameter(const FNiagaraVariable& InVar)
{
	return IsInNamespace(InVar, PARAM_MAP_ENGINE_STR);
}

bool FNiagaraParameterMapHistory::IsPerInstanceEngineParameter(const FNiagaraVariable& InVar, const FString& EmitterAlias)
{
	FString EmitterEngineNamespaceAlias = TEXT("Engine.") + EmitterAlias + TEXT(".");
	return IsInNamespace(InVar, PARAM_MAP_ENGINE_OWNER_STR) || IsInNamespace(InVar, PARAM_MAP_ENGINE_SYSTEM_STR) || IsInNamespace(InVar, PARAM_MAP_ENGINE_EMITTER_STR) || 
		IsInNamespace(InVar, EmitterEngineNamespaceAlias);
}

bool FNiagaraParameterMapHistory::IsUserParameter(const FNiagaraVariable& InVar)
{
	return IsInNamespace(InVar, PARAM_MAP_USER_STR);
}

bool FNiagaraParameterMapHistory::IsRapidIterationParameter(const FNiagaraVariable& InVar)
{
	return IsInNamespace(InVar, PARAM_MAP_RAPID_ITERATION_STR);
}

bool FNiagaraParameterMapHistory::SplitRapidIterationParameterName(const FNiagaraVariable& InVar, ENiagaraScriptUsage InUsage, FString& EmitterName, FString& FunctionCallName, FString& InputName)
{
	TArray<FString> SplitName;
	InVar.GetName().ToString().ParseIntoArray(SplitName, TEXT("."));
	int32 MinimumSplitCount = InUsage == ENiagaraScriptUsage::SystemSpawnScript || InUsage == ENiagaraScriptUsage::SystemUpdateScript ? 3 : 4;
	if (SplitName.Num() >= MinimumSplitCount && (SplitName[0] + ".") == PARAM_MAP_RAPID_ITERATION_STR)
	{
		int32 CurrentIndex = 1;
		if (InUsage == ENiagaraScriptUsage::SystemSpawnScript || InUsage == ENiagaraScriptUsage::SystemUpdateScript)
		{
			EmitterName = FString();
		}
		else
		{
			EmitterName = SplitName[CurrentIndex];
			CurrentIndex++;
		}

		FunctionCallName = SplitName[CurrentIndex];
		CurrentIndex++;

		// Join any remaining name parts with a .
		InputName = FString::Join(TArrayView<FString>(SplitName).Slice(CurrentIndex, SplitName.Num() - CurrentIndex), TEXT("."));
		return true;
	}
	return false;
}

bool FNiagaraParameterMapHistory::IsAttribute(const FNiagaraVariableBase& InVar)
{
	return IsInNamespace(InVar, PARAM_MAP_ATTRIBUTE_STR);
}

const UEdGraphPin* FNiagaraParameterMapHistory::GetDefaultValuePin(int32 VarIdx) const
{
	if (PerVariableWriteHistory[VarIdx].Num() > 0)
	{
		const FModuleScopedPin& ScopedPin = PerVariableWriteHistory[VarIdx][0];
		if (ScopedPin.Pin != nullptr && ScopedPin.Pin->Direction == EEdGraphPinDirection::EGPD_Input && Cast<UNiagaraNodeParameterMapGet>(ScopedPin.Pin->GetOwningNode()) != nullptr)
		{
			return ScopedPin.Pin;
		}
	}
	return nullptr;
}

static bool DoesVariableIncludeNamespace(const FName& InVariableName, const TCHAR* Namespace)
{
	TArray<FString> SplitName;
	InVariableName.ToString().ParseIntoArray(SplitName, TEXT("."));

	for (int32 i = 1; i < SplitName.Num() - 1; i++)
	{
		if (SplitName[i].Equals(Namespace, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}
	return false;
}

static FName ReplaceVariableNamespace(const FName& InVariableName, const TCHAR* Namespace)
{
	TArray<FString> SplitName;
	InVariableName.ToString().ParseIntoArray(SplitName, TEXT("."));

	TArray<FString> JoinString;
	bool bFound = false;
	for (int32 i = 0; i < SplitName.Num(); i++)
	{
		if (!bFound && SplitName[i].Equals(Namespace, ESearchCase::IgnoreCase))
		{
			bFound = true;
			continue;
		}
		else
		{
			JoinString.Add(SplitName[i]);
		}
	}

	return *FString::Join(JoinString, TEXT("."));
}

bool FNiagaraParameterMapHistory::IsInitialName(const FName& InVariableName)
{
	return DoesVariableIncludeNamespace(InVariableName, PARAM_MAP_INITIAL_BASE_STR);
}

bool FNiagaraParameterMapHistory::IsInitialValue(const FNiagaraVariableBase& InVar)
{
	return IsInitialName(InVar.GetName());
}

bool FNiagaraParameterMapHistory::IsPreviousName(const FName& InVariableName)
{
	return DoesVariableIncludeNamespace(InVariableName, PARAM_MAP_PREVIOUS_BASE_STR);
}

bool FNiagaraParameterMapHistory::IsPreviousValue(const FNiagaraVariableBase& InVar)
{
	return IsPreviousName(InVar.GetName());
}

FNiagaraVariable FNiagaraParameterMapHistory::GetSourceForInitialValue(const FNiagaraVariable& InVar)
{
	FNiagaraVariable Var = InVar;
	Var.SetName(GetSourceForInitialValue(InVar.GetName()));
	return Var;
}

FName FNiagaraParameterMapHistory::GetSourceForInitialValue(const FName& InVariableName)
{
	return ReplaceVariableNamespace(InVariableName, PARAM_MAP_INITIAL_BASE_STR);
}

FNiagaraVariable FNiagaraParameterMapHistory::GetSourceForPreviousValue(const FNiagaraVariable& InVar)
{
	FNiagaraVariable Var = InVar;
	Var.SetName(GetSourceForPreviousValue(InVar.GetName()));
	return Var;
}

FName FNiagaraParameterMapHistory::GetSourceForPreviousValue(const FName& InVariableName)
{
	return ReplaceVariableNamespace(InVariableName, PARAM_MAP_PREVIOUS_BASE_STR);
}

bool FNiagaraParameterMapHistory::IsPrimaryDataSetOutput(const FNiagaraVariable& InVar, const UNiagaraScript* InScript,  bool bAllowDataInterfaces, bool bAllowStatics) const
{
	return IsPrimaryDataSetOutput(InVar, InScript->GetUsage(),  bAllowDataInterfaces);
}

bool FNiagaraParameterMapHistory::IsPrimaryDataSetOutput(const FNiagaraVariable& InVar, ENiagaraScriptUsage Usage, bool bAllowDataInterfaces, bool bAllowStatics) const
{
	if (bAllowDataInterfaces == false && InVar.GetType().GetClass() != nullptr)
	{
		return false;
	}

	if (bAllowStatics == false && InVar.GetType().IsStatic())
	{
		return false;
	}

	if (Usage == ENiagaraScriptUsage::EmitterSpawnScript || Usage == ENiagaraScriptUsage::EmitterUpdateScript || 
		Usage == ENiagaraScriptUsage::SystemSpawnScript || Usage == ENiagaraScriptUsage::SystemUpdateScript)
	{
		// In the case of system/emitter scripts we must include the variables in the overall system namespace as well as any of 
		// the child emitters that were encountered.
		for (FString EmitterEncounteredNamespace : EmitterNamespacesEncountered)
		{
			if (IsInNamespace(InVar, EmitterEncounteredNamespace))
			{
				return true;
			}
		}
		return IsInNamespace(InVar, PARAM_MAP_SYSTEM_STR) || IsInNamespace(InVar, PARAM_MAP_EMITTER_STR);
	}
	else if (Usage == ENiagaraScriptUsage::Module || Usage == ENiagaraScriptUsage::Function)
	{
		return IsInNamespace(InVar, PARAM_MAP_MODULE_STR);
	}
	return IsInNamespace(InVar, PARAM_MAP_ATTRIBUTE_STR);
}

bool FNiagaraParameterMapHistory::IsWrittenToScriptUsage(const FNiagaraVariable& InVar, ENiagaraScriptUsage Usage, bool bAllowDataInterfaces)
{
	if (bAllowDataInterfaces == false && InVar.GetType().GetClass() != nullptr)
	{
		return false;
	}

	if (Usage == ENiagaraScriptUsage::EmitterSpawnScript || Usage == ENiagaraScriptUsage::EmitterUpdateScript ||
		Usage == ENiagaraScriptUsage::SystemSpawnScript || Usage == ENiagaraScriptUsage::SystemUpdateScript)
	{
		return IsInNamespace(InVar, PARAM_MAP_SYSTEM_STR) || IsInNamespace(InVar, PARAM_MAP_EMITTER_STR);
	}
	else if (Usage == ENiagaraScriptUsage::Module || Usage == ENiagaraScriptUsage::Function)
	{
		return IsInNamespace(InVar, PARAM_MAP_MODULE_STR);
	}
	return IsInNamespace(InVar, PARAM_MAP_ATTRIBUTE_STR);
}

FNiagaraVariable FNiagaraParameterMapHistory::MoveToExternalConstantNamespaceVariable(const FNiagaraVariable& InVar, ENiagaraScriptUsage InUsage)
{
	if (UNiagaraScript::IsParticleScript(InUsage))
	{
		return VariableToNamespacedVariable(InVar, PARAM_MAP_EMITTER_STR);
	}
	else if (UNiagaraScript::IsStandaloneScript(InUsage))
	{
		return VariableToNamespacedVariable(InVar, PARAM_MAP_MODULE_STR);
	}
	else if (UNiagaraScript::IsEmitterSpawnScript(InUsage) || UNiagaraScript::IsEmitterUpdateScript(InUsage) || UNiagaraScript::IsSystemSpawnScript(InUsage) || UNiagaraScript::IsSystemUpdateScript(InUsage))
	{
		return VariableToNamespacedVariable(InVar, PARAM_MAP_USER_STR);
	}
	return InVar;
}

FNiagaraVariable FNiagaraParameterMapHistory::MoveToExternalConstantNamespaceVariable(const FNiagaraVariable& InVar, const UNiagaraScript* InScript)
{
	return MoveToExternalConstantNamespaceVariable(InVar, InScript->GetUsage());
}

bool FNiagaraParameterMapHistory::IsExportableExternalConstant(const FNiagaraVariable& InVar, const UNiagaraScript* InScript)
{
	if (InScript->IsEquivalentUsage(ENiagaraScriptUsage::SystemSpawnScript))
	{
		return IsExternalConstantNamespace(InVar, InScript, FGuid());
	}
	else
	{
		return false;
	}
}

bool FNiagaraParameterMapHistory::IsExternalConstantNamespace(const FNiagaraVariable& InVar, ENiagaraScriptUsage InUsage, int32 InUsageBitmask)
{
	// Parameter collections are always constants
	if (IsInNamespace(InVar, PARAM_MAP_NPC_STR))
	{
		return true;
	}

	// Engine parameters are never writable.
	if (IsInNamespace(InVar, PARAM_MAP_ENGINE_STR))
	{
		return true;
	}

	if (IsInNamespace(InVar, PARAM_MAP_USER_STR))
	{
		return true;
	}

	if (IsInNamespace(InVar, PARAM_MAP_INDICES_STR))
	{
		return true;
	}
	

	// Modules and functions need to act as if they are within the script types that they 
	// say that they support rather than using their exact script type.
	if (UNiagaraScript::IsStandaloneScript(InUsage))
	{
		TArray<ENiagaraScriptUsage> SupportedContexts = UNiagaraScript::GetSupportedUsageContextsForBitmask(InUsageBitmask);
		if (((!SupportedContexts.Contains(ENiagaraScriptUsage::EmitterSpawnScript) && !SupportedContexts.Contains(ENiagaraScriptUsage::EmitterUpdateScript)) && IsInNamespace(InVar, PARAM_MAP_EMITTER_STR))
			|| ((!SupportedContexts.Contains(ENiagaraScriptUsage::SystemSpawnScript) && !SupportedContexts.Contains(ENiagaraScriptUsage::SystemUpdateScript)) && IsInNamespace(InVar, PARAM_MAP_SYSTEM_STR)))
		{
			return true;
		}
	}

	// Particle scripts cannot write to the emitter or system namespace.
	if (UNiagaraScript::IsParticleScript(InUsage))
	{
		if (IsInNamespace(InVar, PARAM_MAP_EMITTER_STR) || IsInNamespace(InVar, PARAM_MAP_SYSTEM_STR))
		{
			return true;
		}
	}

	return false;
}

bool FNiagaraParameterMapHistory::IsExternalConstantNamespace(const FNiagaraVariable& InVar, const UNiagaraScript* InScript, const FGuid& VersionGuid)
{
	return IsExternalConstantNamespace(InVar, InScript->GetUsage(), InScript->GetScriptData(VersionGuid)->ModuleUsageBitmask);
}

const UNiagaraNodeOutput* FNiagaraParameterMapHistory::GetFinalOutputNode() const
{
	const UEdGraphPin* Pin = GetFinalPin();
	if (Pin != nullptr)
	{
		const UNiagaraNodeOutput* Output = Cast<const UNiagaraNodeOutput>(Pin->GetOwningNode());
		if (Output != nullptr)
		{
			return Output;
		}
	}

	return nullptr;
}

FNiagaraVariable FNiagaraParameterMapHistory::ConvertVariableToRapidIterationConstantName(FNiagaraVariable InVar, const TCHAR* InEmitterName, ENiagaraScriptUsage InUsage)
{
	return FNiagaraUtilities::ConvertVariableToRapidIterationConstantName(InVar, InEmitterName, InUsage);
}

UNiagaraParameterCollection* FNiagaraParameterMapHistory::IsParameterCollectionParameter(FNiagaraVariable& InVar, bool& bMissingParameter)
{
	if (bParameterCollectionsSkipped)
	{
		UE_LOG(LogNiagaraEditor, Error, TEXT("NiagaraParameterCollection was skipped during history building.  History which require NPC data can not be generated during PostLoad()."));
		return nullptr;
	}

	bMissingParameter = false;
	FString VarName = InVar.GetName().ToString();
	for (int32 i = 0; i < ParameterCollections.Num(); ++i)
	{
		if (VarName.StartsWith(ParameterCollectionNamespaces[i]))
		{
			bMissingParameter = !ParameterCollectionVariables[i].Contains(InVar);
			return ParameterCollections[i];
		}
	}
	return nullptr;
}

bool FNiagaraParameterMapHistory::ShouldIgnoreVariableDefault(const FNiagaraVariable& Var)const
{
	// NOTE(mv): Used for variables that are explicitly assigned to (on spawn) and should not be default initialized
	//           These are explicitly written to in NiagaraHlslTranslator::DefineMain
	bool bShouldBeIgnored = false;
	bShouldBeIgnored |= (Var == FNiagaraVariable(FNiagaraTypeDefinition::GetIDDef(), TEXT("Particles.ID")));
	bShouldBeIgnored |= (Var == FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Particles.UniqueID")));
	return bShouldBeIgnored;
}

bool FNiagaraParameterMapHistory::IsVariableFromCustomIterationNamespaceOverride(const FNiagaraVariable& InVar) const
{
	for (const FName& Name : IterationNamespaceOverridesEncountered)
	{
		if (Name.IsValid())
		{
			if (InVar.IsInNameSpace(Name))
				return true;
		}
	}
	return false;
}


FNiagaraParameterMapHistoryBuilder::FNiagaraParameterMapHistoryBuilder()
{
	ContextuallyVisitedNodes.AddDefaulted(1);
	PinToParameterMapIndices.AddDefaulted(1);
	PinToConstantIndices.AddDefaulted(1);
	VariableToConstantIndices.AddDefaulted(1);
	bFilterByScriptAllowList = false;
	bIgnoreDisabled = true;
	FilterScriptType = ENiagaraScriptUsage::Function;

	// depending on when this builder is created, we could be doing a PostLoad.  In this scenario
	// we need to avoid triggering any loads for dependent NiagaraParameterCollection.  Mark the builder
	// to skip those steps if necessary and we'll ensure that the generated histories don't require the
	// collection information
	bIncludeParameterCollectionInfo = !FUObjectThreadContext::Get().IsRoutingPostLoad;
}

void FNiagaraParameterMapHistoryBuilder::BuildParameterMaps(const UNiagaraNodeOutput* OutputNode, bool bRecursive)
{
	if (UNiagaraScript::LogCompileStaticVars > 0)
	{
		UE_LOG(LogNiagaraEditor, Log, TEXT("************** BuildParameterMaps %d  **************"), OutputNode->GetUsage());
	}
	TOptional<FName> StackContextAlias = OutputNode->GetStackContextOverride();
	BeginUsage(OutputNode->GetUsage(), StackContextAlias.Get(ScriptUsageContextNameStack.Num() > 0 ? ScriptUsageContextNameStack.Top() : NAME_None));

	OutputNode->BuildParameterMapHistory(*this, bRecursive);

	EndUsage();
}

void FNiagaraParameterMapHistoryBuilder::EnableScriptAllowList(bool bInEnable, ENiagaraScriptUsage InScriptType)
{
	bFilterByScriptAllowList = bInEnable;
	FilterScriptType = InScriptType;
}

bool FNiagaraParameterMapHistoryBuilder::HasCurrentUsageContext() const
{
	return RelevantScriptUsageContext.Num() > 0;
}


bool FNiagaraParameterMapHistoryBuilder::ContextContains(ENiagaraScriptUsage InUsage) const
{
	if (RelevantScriptUsageContext.Num() == 0)
	{
		return false;
	}
	else
	{
		return RelevantScriptUsageContext.Contains(InUsage);
	}
}


ENiagaraScriptUsage FNiagaraParameterMapHistoryBuilder::GetCurrentUsageContext()const
{
	if (RelevantScriptUsageContext.Num() == 0)
		return ENiagaraScriptUsage::Function;
	return RelevantScriptUsageContext.Last();
}

ENiagaraScriptUsage FNiagaraParameterMapHistoryBuilder::GetBaseUsageContext()const
{
	if (RelevantScriptUsageContext.Num() == 0)
		return ENiagaraScriptUsage::Function;
	return RelevantScriptUsageContext[0];
}

int32 FNiagaraParameterMapHistoryBuilder::CreateParameterMap()
{
	int32 RetValue = Histories.AddDefaulted(1);
	FNiagaraParameterMapHistory& History = Histories[RetValue];

	FName StackName = ScriptUsageContextNameStack.Num() > 0 ? ScriptUsageContextNameStack.Top() : NAME_None;

	if (StackName.IsValid())
	{
		History.IterationNamespaceOverridesEncountered.AddUnique(StackName);
	}

	History.bParameterCollectionsSkipped = !bIncludeParameterCollectionInfo;

	return RetValue;
}

uint32 FNiagaraParameterMapHistoryBuilder::BeginNodeVisitation(int32 WhichParameterMap, const class UNiagaraNode* Node)
{
	if (WhichParameterMap != INDEX_NONE)
	{
		return Histories[WhichParameterMap].BeginNodeVisitation(Node);
	}
	else
	{
		return INDEX_NONE;
	}
}

void FNiagaraParameterMapHistoryBuilder::EndNodeVisitation(int32 WhichParameterMap, uint32 IndexFromBeginNode)
{
	if (WhichParameterMap != INDEX_NONE)
	{
		return Histories[WhichParameterMap].EndNodeVisitation(IndexFromBeginNode);
	}
}

void FNiagaraParameterMapHistoryBuilder::RegisterDataSetWrite(int32 WhichParameterMap, const FNiagaraDataSetID& DataSet)
{
	if (Histories.IsValidIndex(WhichParameterMap))
	{
		Histories[WhichParameterMap].AdditionalDataSetWrites.AddUnique(DataSet);
	}
}

int32 FNiagaraParameterMapHistoryBuilder::RegisterParameterMapPin(int32 WhichParameterMap, const UEdGraphPin* Pin)
{
	if (Pin && UNiagaraScript::LogCompileStaticVars > 0)
	{
		UE_LOG(LogNiagaraEditor, Log, TEXT("FNiagaraParameterMapHistoryBuilder::RegisterParameterMapPin %d > %s .... %p"), WhichParameterMap, *Pin->GetOwningNode()->GetNodeTitle(ENodeTitleType::FullTitle).ToString(), Pin);
	}
	if (Pin && WhichParameterMap != INDEX_NONE)
	{
		if (Pin->Direction == EEdGraphPinDirection::EGPD_Output)
		{
			PinToParameterMapIndices.Last().Add(Pin, WhichParameterMap);
			if (UNiagaraScript::LogCompileStaticVars > 0)
			{
				UE_LOG(LogNiagaraEditor, Log, TEXT("FNiagaraParameterMapHistoryBuilder::RegisterParameterMapPin Added %d"), WhichParameterMap);
			}
		}
		else if (UNiagaraScript::LogCompileStaticVars > 0)
		{
			UE_LOG(LogNiagaraEditor, Log, TEXT("FNiagaraParameterMapHistoryBuilder::RegisterParameterMapPin Not Output %p"), Pin);
		}

		return Histories[WhichParameterMap].RegisterParameterMapPin(Pin);
	}
	else
	{
		return INDEX_NONE;
	}
}

int32 FNiagaraParameterMapHistoryBuilder::RegisterConstantPin(int32 WhichConstant, const UEdGraphPin* Pin)
{
	if (WhichConstant != INDEX_NONE)
	{
		PinToConstantIndices.Last().Add(Pin, WhichConstant);

		if (Histories.Num() > 0)
			Histories.Last().RegisterConstantPin(ActivePath, Pin, Constants[WhichConstant]);

		return WhichConstant;
	}
	else
	{
		return INDEX_NONE;
	}
}

bool FNiagaraParameterMapHistoryBuilder::IsStaticVariableExportableToOuterScopeBasedOnCurrentContext(const FNiagaraVariable& Var) const
{
	if ((IsInEncounteredEmitterNamespace(Var) || FNiagaraParameterMapHistory::IsAliasedEmitterParameter(Var)) && (ContextContains(ENiagaraScriptUsage::EmitterSpawnScript) || ContextContains(ENiagaraScriptUsage::EmitterUpdateScript)))
	{
		return true;
	}
	else if (FNiagaraParameterMapHistory::IsAttribute(Var) && (UNiagaraScript::IsParticleScript(GetBaseUsageContext())))
	{
		return true;
	}
	else if (FNiagaraParameterMapHistory::IsSystemParameter(Var) && (UNiagaraScript::IsSystemScript(GetBaseUsageContext()) || UNiagaraScript::IsEmitterScript(GetBaseUsageContext())))
	{
		return true;
	}
	return false;
}

int32 FNiagaraParameterMapHistoryBuilder::RegisterConstantVariableWrite(int32 WhichParamMapIdx, int32 WhichConstant,  int32 WhichVarIdx, bool bIsSettingDefault, bool bIsLinkNotValue)
{

	FNiagaraEditorModule& NiagaraEditorModule = FModuleManager::GetModuleChecked<FNiagaraEditorModule>("NiagaraEditor");
	if (WhichConstant != INDEX_NONE)
	{
		const FNiagaraVariableBase& Var = Histories[WhichParamMapIdx].Variables[WhichVarIdx];
		if (Var.IsValid())
		{
			VariableToConstantIndices.Last().Add(Var, WhichConstant);

			if (WhichParamMapIdx != INDEX_NONE)
			{
				Histories[WhichParamMapIdx].RegisterConstantVariableWrite(Constants[WhichConstant], WhichVarIdx, bIsSettingDefault, bIsLinkNotValue);
			}

			if (bIsLinkNotValue == false && Constants[WhichConstant].Len() != 0 && Var.GetType().IsStatic())
			{
				TSharedPtr<INiagaraEditorTypeUtilities, ESPMode::ThreadSafe> TypeEditorUtilities = NiagaraEditorModule.GetTypeUtilities(Var.GetType());
				if (TypeEditorUtilities.IsValid() && TypeEditorUtilities->CanHandlePinDefaults())
				{
					FNiagaraVariable VarWithValue = Var;
					VarWithValue.AllocateData();
					if (TypeEditorUtilities->SetValueFromPinDefaultString(Constants[WhichConstant], VarWithValue))
					{
						// Index of uses == operator, which only checks name and type. This will allow us to detect instances of the duplicate
						// data down the line.
						int32 LastStaticVarIdx = StaticVariables.Find(Var);
						int32 AddedIdx = INDEX_NONE;
						if (LastStaticVarIdx == INDEX_NONE)
						{
							AddedIdx = StaticVariables.Add(VarWithValue); // Didn't find it, so add it.
						}
						else if (false == VarWithValue.HoldsSameData(StaticVariables[LastStaticVarIdx]))
						{
							AddedIdx = StaticVariables.Add(VarWithValue); // Add as a duplicate here. We will filter out later
						}

						if (AddedIdx != INDEX_NONE)
						{
							bool bExportable = IsStaticVariableExportableToOuterScopeBasedOnCurrentContext(Var);
							int32 AddedBoolIdx = StaticVariableExportable.Emplace(bExportable);
							ensure(AddedBoolIdx == AddedIdx);
						}
					}
				}
			}
		}

		return WhichConstant;
	}
	else
	{
		return INDEX_NONE;
	}
}


FString FNiagaraParameterMapHistoryBuilder::GetConstant(int32 InIndex)
{
	if (Constants.IsValidIndex(InIndex))
		return Constants[InIndex];
	return FString();
}

int32 FNiagaraParameterMapHistoryBuilder::AddOrGetConstantFromValue(const FString& Value)
{
	return Constants.AddUnique(Value);
}

int32 FNiagaraParameterMapHistoryBuilder::RegisterConstantFromInputPin(const UEdGraphPin* InputPin) 
{
	int32 ConstantIdx = INDEX_NONE;
	if (InputPin && InputPin->Direction == EEdGraphPinDirection::EGPD_Input)
	{
		FString Value;
		if (InputPin && InputPin->LinkedTo.Num() > 0 && InputPin->LinkedTo[0] != nullptr)
		{
			UEdGraphPin* ConnectedOutputPin = InputPin->LinkedTo[0];
			ConstantIdx = GetConstantFromOutputPin(ConnectedOutputPin);
			RegisterConstantPin(ConstantIdx, InputPin);
		}
		else if (InputPin && InputPin->Direction == EEdGraphPinDirection::EGPD_Input && InputPin->LinkedTo.Num() == 0 && InputPin->DefaultValue.Len() != 0 && !InputPin->PinName.IsNone())
		{
			Value = InputPin->DefaultValue;
			ConstantIdx = AddOrGetConstantFromValue(Value);
			RegisterConstantPin(ConstantIdx, InputPin);
		}
	}

	if (UNiagaraScript::LogCompileStaticVars > 0 || NiagaraDebugShouldLogEntriesByNameSubstring(InputPin->PinName.ToString()))
	{
		FGraphTraversalHandle TestPath = ActivePath;
		TestPath.Push(InputPin);
		UE_LOG(LogNiagaraEditor, Log, TEXT("RegisterConstantFromInputPin Value \"%s\" Var: \"%s\""), ConstantIdx != INDEX_NONE ? *Constants[ConstantIdx] : TEXT("INDEX_NONE"), *TestPath.ToString());
	}

	return ConstantIdx;
}

int32 FNiagaraParameterMapHistoryBuilder::GetConstantFromInputPin(const UEdGraphPin* InputPin) const
{
	if (InputPin && InputPin->Direction == EEdGraphPinDirection::EGPD_Input)
	{
		if (InputPin)
		{
			const int32* IdxPtr = PinToConstantIndices.Last().Find(InputPin);
			if (IdxPtr != nullptr)
			{
				if (UNiagaraScript::LogCompileStaticVars > 0 || NiagaraDebugShouldLogEntriesByNameSubstring(InputPin->PinName.ToString()))
				{
					FGraphTraversalHandle TestPath = ActivePath;
					TestPath.Push(InputPin);
					UE_LOG(LogNiagaraEditor, Log, TEXT("GetConstantFromInputPin Value \"%s\" Var: \"%s\""), *Constants[*IdxPtr], *TestPath.ToString());
				}
				return *IdxPtr;
			}
		}
	}
	return INDEX_NONE;
}

int32 FNiagaraParameterMapHistoryBuilder::GetConstantFromOutputPin(const UEdGraphPin* OutputPin) const
{
	if (OutputPin && OutputPin->Direction == EEdGraphPinDirection::EGPD_Output)
	{
		if (OutputPin)
		{
			const int32* IdxPtr = PinToConstantIndices.Last().Find(OutputPin);
			if (IdxPtr != nullptr)
			{
				if (UNiagaraScript::LogCompileStaticVars > 0 || NiagaraDebugShouldLogEntriesByNameSubstring(OutputPin->GetName()))
				{
					FGraphTraversalHandle TestPath = ActivePath;
					TestPath.Push(OutputPin);
					UE_LOG(LogNiagaraEditor, Log, TEXT("GetConstantFromOutputPin Value \"%s\" Var: \"%s\""), *Constants[*IdxPtr], *TestPath.ToString());
				}
				return *IdxPtr;
			}
		}
	}
	return INDEX_NONE;
}


int32 FNiagaraParameterMapHistoryBuilder::GetConstantFromVariableRead(const FNiagaraVariableBase& InVar)
{
	if (InVar.IsValid())
	{
		for (int32 i = VariableToConstantIndices.Num() - 1; i >= 0; i--)
		{
			const int32* IdxPtr = VariableToConstantIndices[i].Find(InVar);
			if (IdxPtr != nullptr)
			{
				if (UNiagaraScript::LogCompileStaticVars > 0 || NiagaraDebugShouldLogEntriesByNameSubstring(ActivePath.ToString()))
				{
					UE_LOG(LogNiagaraEditor, Log, TEXT("GetConstantFromVariableRead Value \"%s\" Var: \"%s\" Path: \"%s\""), *Constants[*IdxPtr], *InVar.GetName().ToString(),  *ActivePath.ToString());
				}
				return *IdxPtr;
			}
		}
		
	}
	return INDEX_NONE;
}

int32 FNiagaraParameterMapHistoryBuilder::TraceParameterMapOutputPin(const UEdGraphPin* OutputPin)
{
	if (OutputPin && OutputPin->Direction == EEdGraphPinDirection::EGPD_Output)
	{
		if (OutputPin)
		{
			const int32* IdxPtr = PinToParameterMapIndices.Last().Find(OutputPin);
			if (IdxPtr != nullptr)
			{
				return *IdxPtr;
			}
		}
	}
	return INDEX_NONE;
}

bool FNiagaraParameterMapHistoryBuilder::GetPinPreviouslyVisited(const UEdGraphPin* InPin) const
{
	if (InPin != nullptr)
		return GetNodePreviouslyVisited(CastChecked<UNiagaraNode>(InPin->GetOwningNode()));
	else
		return true;
}


bool FNiagaraParameterMapHistoryBuilder::GetNodePreviouslyVisited(const class UNiagaraNode* Node) const
{
	return ContextuallyVisitedNodes.Last().Contains(Node);
}

int32 FNiagaraParameterMapHistoryBuilder::FindMatchingParameterMapFromContextInputs(const FNiagaraVariable& InVar) const
{
	if (CallingContext.Num() == 0)
	{
		return INDEX_NONE;
	}
	const UNiagaraNode* Node = CallingContext.Last();
	FPinCollectorArray Inputs;
	Node->GetInputPins(Inputs);
	const UEdGraphSchema_Niagara* Schema = CastChecked<UEdGraphSchema_Niagara>(Node->GetSchema());

	for (int32 i = 0; i < Inputs.Num(); i++)
	{
		FNiagaraVariable CallInputVar = Schema->PinToNiagaraVariable(Inputs[i]);
		if (CallInputVar.IsEquivalent(InVar) && CallInputVar.GetType() == FNiagaraTypeDefinition::GetParameterMapDef())
		{
			if (Inputs[i]->LinkedTo.Num() != 0 && PinToParameterMapIndices.Num() >= 2)
			{
				const UEdGraphPin* OutputPin = Inputs[i]->LinkedTo[0];
				const int32* ParamMapIdxPtr = PinToParameterMapIndices[PinToParameterMapIndices.Num() - 2].Find(OutputPin);
				if (ParamMapIdxPtr != nullptr)
				{
					return *ParamMapIdxPtr;
				}
				else
				{
					FString ScriptUsageDisplayName;
					const UNiagaraNodeOutput* ContextOutputNode = FNiagaraStackGraphUtilities::GetEmitterOutputNodeForStackNode(*Node);
					if (ContextOutputNode != nullptr)
					{
						UEnum* NiagaraScriptUsageEnum = FindObjectChecked<UEnum>(nullptr, TEXT("/Script/Niagara.ENiagaraScriptUsage"), true);
						ScriptUsageDisplayName = NiagaraScriptUsageEnum->GetDisplayNameTextByValue((uint64)ContextOutputNode->GetUsage()).ToString();
					}
					else
					{
						ScriptUsageDisplayName = "Unknown";
					}
					FString FunctionDisplayName;
					if (FunctionNameContextStack.Num() > 0)
					{
						FunctionDisplayName = FunctionNameContextStack.Last().ToString();
					}
					else
					{
						FunctionDisplayName = Node->GetName();
					}

					
					/* 
					UE_LOG(LogNiagaraEditor, Error, TEXT("Unable to find matching parameter map for variable.  Name:%s  Function:%s  Usage:%s  Package:%s"),
						*InVar.GetName().ToString(), *FunctionDisplayName, *ScriptUsageDisplayName, *Node->GetOutermost()->GetName());
					*/
					
				}
			}
		}
	}
	return INDEX_NONE;
}

int32 FNiagaraParameterMapHistoryBuilder::FindMatchingStaticFromContextInputs(const FNiagaraVariable& InVar) const
{
	if (CallingContext.Num() == 0)
	{
		return INDEX_NONE;
	}
	const UNiagaraNode* Node = CallingContext.Last();
	FPinCollectorArray Inputs;
	Node->GetInputPins(Inputs);
	const UEdGraphSchema_Niagara* Schema = CastChecked<UEdGraphSchema_Niagara>(Node->GetSchema());

	for (int32 i = 0; i < Inputs.Num(); i++)
	{
		FNiagaraVariable CallInputVar = Schema->PinToNiagaraVariable(Inputs[i]);
		if (CallInputVar.IsEquivalent(InVar))
		{
			// If linked to something, then the output we are linked to will carry the constant.
			if (Inputs[i]->LinkedTo.Num() != 0 && PinToConstantIndices.Num() >= 2)
			{
				const UEdGraphPin* OutputPin = Inputs[i]->LinkedTo[0];
				//int32 ConstantIdx = GetConstantFromOutputPin(OutputPin);
				const int32* IdxPtr = PinToConstantIndices[PinToConstantIndices.Num() - 2].Find(OutputPin);
				if (IdxPtr != nullptr)
					return *IdxPtr;
			}
			// If just using the default, the input pin will carry the constant.
			else if (Inputs[i]->LinkedTo.Num() == 0 && PinToConstantIndices.Num() >= 2)
			{				
				const int32* IdxPtr = PinToConstantIndices[PinToConstantIndices.Num() - 2].Find(Inputs[i]);
				if (IdxPtr != nullptr)
					return *IdxPtr;
			}
		}
	}
	return INDEX_NONE;
}

void FNiagaraParameterMapHistoryBuilder::BeginTranslation(const UNiagaraScript* Script)
{
	//For now this will just tell particle scripts what emitter they're being compiled as part of but maybe we want to do more here.
	//This is mainly so that parameter names match up between System/Emitter scripts and the parameters they drive within particle scripts.
	//I dislike this coupling of the translator to emitters but for now it'll have to do.
	//Will refactor in the future.
	UNiagaraEmitter* Emitter = Script->GetTypedOuter<UNiagaraEmitter>();
	BeginTranslation(Emitter);
}

void FNiagaraParameterMapHistoryBuilder::EndTranslation(const UNiagaraScript* Script)
{
	EmitterNameContextStack.Reset();
}

void FNiagaraParameterMapHistoryBuilder::BeginTranslation(const UNiagaraEmitter* Emitter)
{
	//For now this will just tell particle scripts what emitter they're being compiled as part of but maybe we want to do more here.
	//This is mainly so that parameter names match up between System/Emitter scripts and the parameters they drive within particle scripts.
	//I dislike this coupling of the translator to emitters but for now it'll have to do.
	//Will refactor in the future.
	if (Emitter)
	{
		FString EmitterUniqueName = Emitter->GetUniqueEmitterName();
		EmitterNameContextStack.Add(*EmitterUniqueName);
	}
	BuildCurrentAliases();
}

void FNiagaraParameterMapHistoryBuilder::EndTranslation(const UNiagaraEmitter* Emitter)
{
	EmitterNameContextStack.Reset();
}

void FNiagaraParameterMapHistoryBuilder::BeginTranslation(const FString& EmitterUniqueName)
{
	//For now this will just tell particle scripts what emitter they're being compiled as part of but maybe we want to do more here.
	//This is mainly so that parameter names match up between System/Emitter scripts and the parameters they drive within particle scripts.
	//I dislike this coupling of the translator to emitters but for now it'll have to do.
	//Will refactor in the future.
	if (EmitterUniqueName.IsEmpty() == false)
	{
		EmitterNameContextStack.Add(*EmitterUniqueName);
	}
	BuildCurrentAliases();
}
void FNiagaraParameterMapHistoryBuilder::EndTranslation(const FString& EmitterUniqueName)
{
	EmitterNameContextStack.Reset();
}

void FNiagaraParameterMapHistoryBuilder::BeginUsage(ENiagaraScriptUsage InUsage, FName InStageName)
{
	RelevantScriptUsageContext.Push(InUsage);
	ScriptUsageContextNameStack.Push(InStageName);
		
	BuildCurrentAliases();
}

void FNiagaraParameterMapHistoryBuilder::EndUsage()
{
	RelevantScriptUsageContext.Pop();
	ScriptUsageContextNameStack.Pop();
}

const UNiagaraNode* FNiagaraParameterMapHistoryBuilder::GetCallingContext() const
{
	if (CallingContext.Num() == 0)
	{
		return nullptr;
	}
	else
	{
		return CallingContext.Last();
	}
}

bool FNiagaraParameterMapHistoryBuilder::InTopLevelFunctionCall(ENiagaraScriptUsage InFilterScriptType) const
{
	if (InFilterScriptType == ENiagaraScriptUsage::EmitterSpawnScript || InFilterScriptType == ENiagaraScriptUsage::EmitterUpdateScript || InFilterScriptType == ENiagaraScriptUsage::SystemSpawnScript || InFilterScriptType == ENiagaraScriptUsage::SystemUpdateScript)
	{
		if (CallingContext.Num() <= 1) // Handles top-level system graph and any function calls off of it.
		{
			return true;
		}
		else if (CallingContext.Num() <= 2 && Cast<UNiagaraNodeEmitter>(CallingContext[0]) != nullptr) // Handle a function call off of an emitter
		{
			return true;
		}

	}
	else if (UNiagaraScript::IsParticleScript(InFilterScriptType))
	{
		if (CallingContext.Num() <= 1) // Handle a function call
		{
			return true;
		}
	}

	return false;
}


void FNiagaraParameterMapHistoryBuilder::EnterFunction(const FString& InNodeName, const UNiagaraScript* InScript, const UNiagaraGraph* InGraph, const UNiagaraNode* Node)
{
	if (InScript != nullptr )
	{
		RegisterNodeVisitation(Node);
		CallingContext.Push(Node);
		ActivePath.Push(Node);
		CallingGraphContext.Push(InGraph);
		PinToParameterMapIndices.Emplace();
		PinToConstantIndices.Emplace();
		FunctionNameContextStack.Emplace(*InNodeName);
		BuildCurrentAliases();
		if (EncounteredFunctionNames.Num() != 0)
		{
			EncounteredFunctionNames.Last().AddUnique(InNodeName);
		}
		ContextuallyVisitedNodes.Emplace();
	}
}

void FNiagaraParameterMapHistoryBuilder::ExitFunction(const FString& InNodeName, const UNiagaraScript* InScript, const UNiagaraNode* Node)
{
	if (InScript != nullptr)
	{
		CallingContext.Pop();
		CallingGraphContext.Pop();
		ActivePath.Pop();
		PinToParameterMapIndices.Pop();
		FunctionNameContextStack.Pop();
		PinToConstantIndices.Pop();
		BuildCurrentAliases();
		ContextuallyVisitedNodes.Pop();
	}
}

void FNiagaraParameterMapHistoryBuilder::EnterEmitter(const FString& InEmitterName, const UNiagaraGraph* InGraph, const UNiagaraNode* Node)
{
	RegisterNodeVisitation(Node);
	CallingContext.Push(Node);
	ActivePath.Push(Node);
	CallingGraphContext.Push(InGraph);
	EmitterNameContextStack.Emplace(*InEmitterName);
	BuildCurrentAliases();

	// Emitters must record their namespaces to their histories as well as
	// make sure to record their current usage type is so that we can filter variables
	// for relevance downstream.
	const UNiagaraNodeEmitter* EmitterNode = Cast<UNiagaraNodeEmitter>(Node);
	if (EmitterNode != nullptr)
	{
		RelevantScriptUsageContext.Emplace(EmitterNode->GetUsage());
	}
	else
	{
		RelevantScriptUsageContext.Emplace(ENiagaraScriptUsage::EmitterSpawnScript);
	}

	for (FNiagaraParameterMapHistory& History : Histories)
	{
		History.EmitterNamespacesEncountered.AddUnique(InEmitterName);
	}
	EncounteredEmitterNames.AddUnique(InEmitterName);
	TArray<FString> EmptyFuncs;
	EncounteredFunctionNames.Push(EmptyFuncs);
	ContextuallyVisitedNodes.Emplace();
}

void FNiagaraParameterMapHistoryBuilder::ExitEmitter(const FString& InEmitterName, const UNiagaraNode* Node)
{
	CallingContext.Pop();
	CallingGraphContext.Pop();
	EmitterNameContextStack.Pop();
	ActivePath.Pop();
	BuildCurrentAliases();
	ContextuallyVisitedNodes.Pop();
	EncounteredFunctionNames.Pop();
}


bool FNiagaraParameterMapHistoryBuilder::IsInEncounteredFunctionNamespace(const FNiagaraVariable& InVar) const
{
	if (EncounteredFunctionNames.Num() != 0)
	{
		for (FString EncounteredNamespace : EncounteredFunctionNames.Last())
		{
			if (FNiagaraParameterMapHistory::IsInNamespace(InVar, EncounteredNamespace))
			{
				return true;
			}
		}
	}
	return false;
}

bool FNiagaraParameterMapHistoryBuilder::IsInEncounteredEmitterNamespace(const FNiagaraVariable& InVar) const
{
	for (FString EmitterEncounteredNamespace : EncounteredEmitterNames)
	{
		if (FNiagaraParameterMapHistory::IsInNamespace(InVar, EmitterEncounteredNamespace))
		{
			return true;
		}
	}
	return false;
}

/**
* Use the current alias map to resolve any aliases in this input variable name.
*/
FNiagaraVariable FNiagaraParameterMapHistoryBuilder::ResolveAliases(const FNiagaraVariable& InVar) const
{
	FNiagaraVariable Var = FNiagaraUtilities::ResolveAliases(InVar, ResolveAliasContext);
	//ensure(!Var.IsInNameSpace(FNiagaraConstants::StackContextNamespace));
	return Var;
}

void FNiagaraParameterMapHistoryBuilder::RegisterNodeVisitation(const UEdGraphNode* Node)
{
	ContextuallyVisitedNodes.Last().AddUnique(CastChecked<UNiagaraNode>(Node));
}


const FString* FNiagaraParameterMapHistoryBuilder::GetModuleAlias() const
{
	return ResolveAliasContext.GetModuleName().IsSet()
		? &ResolveAliasContext.GetModuleName().GetValue()
		: nullptr;
}

const FString* FNiagaraParameterMapHistoryBuilder::GetEmitterAlias() const
{
	return ResolveAliasContext.GetEmitterName().IsSet()
		? &ResolveAliasContext.GetEmitterName().GetValue()
		: nullptr;
}

void FNiagaraParameterMapHistoryBuilder::VisitInputPin(const UEdGraphPin* Pin, const class UNiagaraNode* InNode, bool bFilterForCompilation)
{
	const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();

	if (Pin != nullptr && Pin->Direction == EEdGraphPinDirection::EGPD_Input)
	{
		for (int32 j = 0; j < Pin->LinkedTo.Num(); j++)
		{
			UEdGraphPin* LinkedPin = Pin->LinkedTo[j];	

			if (LinkedPin)
			{
				UNiagaraNode* Node = CastChecked<UNiagaraNode>(LinkedPin->GetOwningNode());

				if (!GetNodePreviouslyVisited(Node))
				{
				
					if (UNiagaraScript::LogCompileStaticVars > 0)
					{
						UE_LOG(LogNiagaraEditor, Log, TEXT("Build Parameter Map History: %s %s"), *Node->GetClass()->GetName(), *Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
					}
					Node->BuildParameterMapHistory(*this, true, bFilterForCompilation);
					RegisterNodeVisitation(Node);
				}

				if (Schema->PinToTypeDefinition(Pin) == FNiagaraTypeDefinition::GetParameterMapDef())
				{
					int32 ParamMapIdx = TraceParameterMapOutputPin(LinkedPin);
					
					if (UNiagaraScript::LogCompileStaticVars > 0)
					{
						if (ParamMapIdx == INDEX_NONE)
						{
							UE_LOG(LogNiagaraEditor, Log, TEXT("Found bad pin!"));
						}

						UE_LOG(LogNiagaraEditor, Log, TEXT("Build Parameter Map History: %s %s PMapIdx: %d"), *Node->GetClass()->GetName(), *Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString(), ParamMapIdx);
					}

					RegisterParameterMapPin(ParamMapIdx, Pin);
				}
			}
		} 
	}
}

void FNiagaraParameterMapHistoryBuilder::VisitInputPins(const class UNiagaraNode* InNode, bool bFilterForCompilation)
{
	FPinCollectorArray InputPins;
	InNode->GetInputPins(InputPins);

	for (int32 i = 0; i < InputPins.Num(); i++)
	{
		VisitInputPin(InputPins[i], InNode, bFilterForCompilation);
	}
}

bool FNiagaraParameterMapHistoryBuilder::IsNamespacedVariableRelevantToScriptType(const FNiagaraVariable& InVar, ENiagaraScriptUsage InFilterScriptType)
{
	return true;
}

bool FNiagaraParameterMapHistoryBuilder::ShouldTrackVariable(const FNiagaraVariable& InVar)
{
	if (!bFilterByScriptAllowList)
	{
		return true;
	}
	if (IsNamespacedVariableRelevantToScriptType(InVar, FilterScriptType))
	{
		return true;
	}
	return false;
}

int32 FNiagaraParameterMapHistoryBuilder::HandleVariableWrite(int32 ParamMapIdx, const UEdGraphPin* InPin)
{
	const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();
	FNiagaraVariable Var = Schema->PinToNiagaraVariable(InPin);

	if (!ShouldTrackVariable(Var))
	{
		return INDEX_NONE;
	}
	FNiagaraVariable AliasedVar = Var;
	Var = ResolveAliases(Var);

	int32 FoundIdx = AddVariableToHistory(Histories[ParamMapIdx], Var, AliasedVar, InPin);
	if (InPin && Var.GetType().IsStatic() && InPin->LinkedTo.Num() > 0)
	{
		int32 ConstantIdx = GetConstantFromOutputPin(InPin->LinkedTo[0]);
		RegisterConstantPin(ConstantIdx, InPin);

		if (FoundIdx != INDEX_NONE && ParamMapIdx != INDEX_NONE && ConstantIdx != INDEX_NONE)
		{
			RegisterConstantVariableWrite(ParamMapIdx, ConstantIdx, FoundIdx, false, false);
		}
	}
	else if (InPin && Var.GetType().IsStatic() && InPin->LinkedTo.Num() == 0)
	{
		int32 ConstantIdx = AddOrGetConstantFromValue(InPin->DefaultValue);
		RegisterConstantPin(ConstantIdx, InPin);

		if (FoundIdx != INDEX_NONE && ParamMapIdx != INDEX_NONE && ConstantIdx != INDEX_NONE)
		{
			RegisterConstantVariableWrite(ParamMapIdx, ConstantIdx, FoundIdx, false, false);
		}
	}

	return FoundIdx;
}

int32 FNiagaraParameterMapHistoryBuilder::HandleVariableWrite(int32 ParameterMapIndex, const FNiagaraVariable& Var)
{
	const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();

	if (!ShouldTrackVariable(Var))
	{
		return INDEX_NONE;
	}
	FNiagaraVariable ResolvedVar = ResolveAliases(Var);

	int32 FoundIdx = AddVariableToHistory(Histories[ParameterMapIndex], ResolvedVar, Var, nullptr);
	return FoundIdx;
}


int32 FNiagaraParameterMapHistoryBuilder::HandleVariableRead(int32 ParamMapIdx, const UEdGraphPin* InPin, bool RegisterReadsAsVariables, const UEdGraphPin* InDefaultPin, bool bFilterForCompilation, bool& OutUsedDefault)
{
	FString DefaultValue;
	OutUsedDefault = false;
	const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();
	FNiagaraVariable Var = Schema->PinToNiagaraVariable(InPin);

	FNiagaraParameterMapHistory& History = Histories[ParamMapIdx];

	if (!ShouldTrackVariable(Var))
	{
		return INDEX_NONE;
	}
	FNiagaraVariable AliasedVar = Var;
	Var = ResolveAliases(Var);

	//Track any parameter collections we're referencing.
	if (bIncludeParameterCollectionInfo)
	{
		if (UNiagaraParameterCollection* Collection = Schema->VariableIsFromParameterCollection(Var))
		{
			int32 Index = History.ParameterCollections.AddUnique(Collection);
			History.ParameterCollectionNamespaces.SetNum(History.ParameterCollections.Num());
			History.ParameterCollectionVariables.SetNum(History.ParameterCollections.Num());
			History.ParameterCollectionNamespaces[Index] = Collection->GetFullNamespace();
			History.ParameterCollectionVariables[Index] = Collection->GetParameters();
		}
	}

	const FString* ModuleAlias = GetModuleAlias();
	FName ModuleName = ModuleAlias ? FName(**ModuleAlias) : NAME_None;

	bool AddWriteHistory = true;
	bool bUseRapidIterationParam = true;

	int32 FoundIdx = History.FindVariable(Var.GetName(), Var.GetType());
	if (FoundIdx == -1)
	{
		if (RegisterReadsAsVariables)
		{
			OutUsedDefault = true;

			if (InDefaultPin)
			{
				VisitInputPin(InDefaultPin, Cast<UNiagaraNode>(InDefaultPin->GetOwningNode()), bFilterForCompilation);
				DefaultValue = InDefaultPin->DefaultValue;
			}

			FoundIdx = AddVariableToHistory(History, Var, AliasedVar, InDefaultPin);
			AddWriteHistory = false;

			// Add the default binding as well to the parameter history, if used.
			if (UNiagaraGraph* Graph = Cast<UNiagaraGraph>(InPin->GetOwningNode()->GetGraph()))
			{
				FNiagaraScriptVariableBinding VariableBinding;
				TOptional<ENiagaraDefaultMode> DefaultMode = Graph->GetDefaultMode(AliasedVar, &VariableBinding);
				if (DefaultMode.IsSet() && *DefaultMode == ENiagaraDefaultMode::Binding && VariableBinding.IsValid())
				{
					FNiagaraVariable TempVar = FNiagaraVariable(Var.GetType(), VariableBinding.GetName());
					if (FNiagaraConstants::GetOldPositionTypeVariables().Contains(TempVar))
					{
						// Old assets often have vector inputs that default bind to what is now a position type. If we detect that, we change the type to prevent a compiler error.
						TempVar.SetType(FNiagaraTypeDefinition::GetPositionDef());
					}
					int32 FoundIdxBinding = AddVariableToHistory(History, TempVar, TempVar, nullptr);

					DefaultValue = History.Variables[FoundIdxBinding].GetName().ToString();
					bUseRapidIterationParam = false;

					History.PerVariableReadHistory[FoundIdxBinding].AddDefaulted_GetRef().ReadPin = FModuleScopedPin(InDefaultPin, ModuleName);
				}
			}
		}
	}
	else
	{
		if (History.Variables[FoundIdx].GetType() != Var.GetType())
		{
			History.PerVariableWarnings[FoundIdx].Append(FString::Printf(TEXT("Type mismatch %s instead of %s in map!"), *Var.GetType().GetName(), *History.Variables[FoundIdx].GetType().GetName()));
		}
	}

	FNiagaraParameterMapHistory::FReadHistory& ReadEntry = History.PerVariableReadHistory[FoundIdx].AddDefaulted_GetRef();
	ReadEntry.ReadPin = FModuleScopedPin(InPin, ModuleName);
	if (AddWriteHistory && History.PerVariableWriteHistory[FoundIdx].Num())
	{
		ReadEntry.PreviousWritePin = History.PerVariableWriteHistory[FoundIdx].Last();
	}

	check(History.Variables.Num() == History.PerVariableWarnings.Num());
	check(History.Variables.Num() == History.PerVariableWriteHistory.Num());
	check(History.Variables.Num() == History.PerVariableReadHistory.Num());
	check(History.Variables.Num() == History.PerVariableConstantValue.Num());
	check(History.Variables.Num() == History.PerVariableConstantDefaultValue.Num());

	if (OutUsedDefault && InDefaultPin && UEdGraphSchema_Niagara::IsStaticPin(InDefaultPin))
	{
		int32 ConstantIdx = INDEX_NONE;
		ENiagaraScriptUsage Usage = GetCurrentUsageContext();

		FString EmitterNamespace;
		if (!UNiagaraScript::IsSystemScript(Usage) && Histories[ParamMapIdx].EmitterNamespacesEncountered.Num() > 0)
		{
			EmitterNamespace = Histories[ParamMapIdx].EmitterNamespacesEncountered.Last();
		}
		else if (!UNiagaraScript::IsSystemScript(Usage) )
		{
			if (EmitterNamespace.IsEmpty() && EmitterNameContextStack.Num() > 0)
				EmitterNamespace = EmitterNameContextStack.Last().ToString();
			else
				EmitterNamespace = TEXT("Emitter");
		}

		bool bWroteVarNameToDefaultValue = false;

		if (bUseRapidIterationParam && /*InTopLevelFunctionCall(Usage) && */FNiagaraParameterMapHistory::IsAliasedModuleParameter(Histories[ParamMapIdx].VariablesWithOriginalAliasesIntact[FoundIdx]))
		{					
			FNiagaraVariable VarRapid = FNiagaraParameterMapHistory::ConvertVariableToRapidIterationConstantName(Histories[ParamMapIdx].Variables[FoundIdx], EmitterNamespace.Len() == 0 ? nullptr : *EmitterNamespace, Usage);
			
			int32 StaticIdx = StaticVariables.IndexOfByPredicate([&](const FNiagaraVariable& InObj) -> bool
				{
					return (InObj.GetName() == VarRapid.GetName());
				});;

			if (StaticIdx == INDEX_NONE)
			{
				StaticIdx = StaticVariables.IndexOfByPredicate([&](const FNiagaraVariable& InObj) -> bool
					{
						return (InObj.GetName() == Var.GetName());
					});;
			}

			if (ConstantResolver.ResolveConstant(VarRapid))
			{
				DefaultValue = VarRapid.GetType().ToString(VarRapid.GetData());
			}
			else if (StaticIdx != INDEX_NONE)
			{
				DefaultValue = StaticVariables[StaticIdx].GetType().ToString(StaticVariables[StaticIdx].GetData());	
			}
			else if (DefaultValue.Len() == 0)
			{
				//DefaultValue = VarRapid.GetName().ToString();
				FNiagaraVariable VarDefault = Var;
				FNiagaraEditorUtilities::ResetVariableToDefaultValue(VarDefault);// Do this to handle non-zero defaults
				DefaultValue = VarDefault.GetType().ToString(VarDefault.GetData());
			}
			
			if (UNiagaraScript::LogCompileStaticVars > 0)
			{
				FGraphTraversalHandle TestPath = ActivePath;
				TestPath.Push(InPin);
				UE_LOG(LogNiagaraEditor, Log, TEXT("HandleVariableRead RapidIt Value \"%s\" Var: \"%s\" Path: \"%s\""), *DefaultValue, *VarRapid.GetName().ToString(),  *TestPath.ToString());
			}
			

		}
		else if (FNiagaraParameterMapHistory::IsExternalConstantNamespace(Var, Usage, 0) || Var.IsInNameSpace(EmitterNamespace) || Var.IsInNameSpace(FNiagaraConstants::SystemNamespace) || Var.IsInNameSpace(FNiagaraConstants::ParticleAttributeNamespace))
		{
			const FNiagaraVariable& VarRapid = Histories[ParamMapIdx].Variables[FoundIdx];
			int32 StaticIdx = StaticVariables.IndexOfByPredicate([&](const FNiagaraVariable& InObj) -> bool
				{
					return (InObj.GetName() == VarRapid.GetName());
				});;

			if (ConstantResolver.ResolveConstant(Var))
			{
				DefaultValue = Var.GetType().ToString(Var.GetData());
			}
			else if (StaticIdx != INDEX_NONE)
			{
				DefaultValue = StaticVariables[StaticIdx].GetType().ToString(StaticVariables[StaticIdx].GetData());
			}
			else
			{
				DefaultValue = Var.GetName().ToString();
				bWroteVarNameToDefaultValue = true;
			}
			
			if (UNiagaraScript::LogCompileStaticVars > 0)
			{
				FGraphTraversalHandle TestPath = ActivePath;
				TestPath.Push(InPin);
				UE_LOG(LogNiagaraEditor, Log, TEXT("HandleVariableRead Ext Value \"%s\" Var: \"%s\" Path: \"%s\""), *DefaultValue, *VarRapid.GetName().ToString(),  *TestPath.ToString());
			}
		}

		ConstantIdx = AddOrGetConstantFromValue(DefaultValue);

		RegisterConstantPin(ConstantIdx, InPin);
		if (FoundIdx != INDEX_NONE && ParamMapIdx != INDEX_NONE && ConstantIdx != INDEX_NONE)
		{
			RegisterConstantVariableWrite(ParamMapIdx, ConstantIdx, FoundIdx, true, bWroteVarNameToDefaultValue);
		}
	}
	else if (FoundIdx != INDEX_NONE && ParamMapIdx != INDEX_NONE && UEdGraphSchema_Niagara::IsStaticPin(InPin))
	{
		int32 ConstantIdx = GetConstantFromVariableRead(Histories[ParamMapIdx].Variables[FoundIdx]);
		RegisterConstantPin(ConstantIdx, InPin);
	}

	return FoundIdx;
}

void FNiagaraParameterMapHistoryBuilder::RegisterEncounterableVariables(TConstArrayView<FNiagaraVariable> Variables)
{
	EncounterableExternalVariables.Append(Variables.GetData(), Variables.Num());
}

void FNiagaraParameterMapHistoryBuilder::RegisterExternalStaticVariables(TConstArrayView<FNiagaraVariable> Variables)
{
	int32 LastIdx = StaticVariables.Num();
	StaticVariables.Append(Variables.GetData(), Variables.Num());

	// Register all new entries as exportable true
	for (int32 i = LastIdx; i < StaticVariables.Num(); i++)
	{
		StaticVariableExportable.Emplace(true);
	}

	if (UNiagaraScript::LogCompileStaticVars > 0)
	{
		UE_LOG(LogNiagaraEditor, Log, TEXT("FNiagaraParameterMapHistoryBuilder::RegisterExternalStaticVariables ==================================="));
		for (int32 i = 0; i < StaticVariables.Num(); i++)
		{
			UE_LOG(LogNiagaraEditor, Log, TEXT("[%d] %s"), i, *StaticVariables[i].ToString());
		}
		UE_LOG(LogNiagaraEditor, Log, TEXT("  ==================================="));
	}
}

void UpdateAliasedVariable(FNiagaraVariable& AliasedVar, const FNiagaraVariable& OriginalUnaliasedVar, const FNiagaraVariable& UpdatedUnaliasedVar)
{
	AliasedVar.SetType(UpdatedUnaliasedVar.GetType());

	TArray<FString> AliasedSplitName;
	AliasedVar.GetName().ToString().ParseIntoArray(AliasedSplitName, TEXT("."));

	TArray<FString> OriginalUnaliasedSplitName;
	OriginalUnaliasedVar.GetName().ToString().ParseIntoArray(OriginalUnaliasedSplitName, TEXT("."));

	TArray<FString> UpdatedUnaliasedSplitName;
	UpdatedUnaliasedVar.GetName().ToString().ParseIntoArray(UpdatedUnaliasedSplitName, TEXT("."));

	TArray<FString> JoinName;
	for (int32 i = 0; i < AliasedSplitName.Num(); i++)
	{
		if (i >= OriginalUnaliasedSplitName.Num() || i >= UpdatedUnaliasedSplitName.Num())
		{
			continue;
		}

		//if (UpdatedUnaliasedSplitName[i] == OriginalUnaliasedSplitName[i])
		{
			JoinName.Add(AliasedSplitName[i]);
		}
		//else
		//{
		//	JoinName.Add(AliasedSplitName[i]);
		//}
	}

	FString OutVarStrName = FString::Join(JoinName, TEXT("."));
	AliasedVar.SetName(*OutVarStrName);
}

int32 FNiagaraParameterMapHistoryBuilder::HandleExternalVariableRead(int32 ParamMapIdx, const FName& Name)
{
	const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();

	FNiagaraParameterMapHistory& History = Histories[ParamMapIdx];

	FNiagaraVariable Var(FNiagaraTypeDefinition(), Name);

	if (!ShouldTrackVariable(Var))
	{
		return INDEX_NONE;
	}
	FNiagaraVariable AliasedVar = Var;
	Var = ResolveAliases(Var);
	FNiagaraVariable OriginalUnaliasedVar = Var;

	//Track any parameter collections we're referencing.
	if (bIncludeParameterCollectionInfo)
	{
		FNiagaraVariable FoundTempVar;
		if (UNiagaraParameterCollection* Collection = Schema->VariableIsFromParameterCollection(Name.ToString(), true, FoundTempVar))
		{
			int32 Index = History.ParameterCollections.AddUnique(Collection);
			History.ParameterCollectionNamespaces.SetNum(History.ParameterCollections.Num());
			History.ParameterCollectionVariables.SetNum(History.ParameterCollections.Num());
			History.ParameterCollectionNamespaces[Index] = Collection->GetFullNamespace();
			History.ParameterCollectionVariables[Index] = Collection->GetParameters();
			Var = FoundTempVar;
			UpdateAliasedVariable(AliasedVar, OriginalUnaliasedVar, Var);
		}
	}

	int32 FoundIdx = Histories[ParamMapIdx].FindVariableByName(Name, true);
	
	if (FoundIdx == -1)
	{
		const FNiagaraVariable* TempKnownConstant = FNiagaraConstants::GetKnownConstant(Name, true);

		if (!Var.IsValid() && TempKnownConstant != nullptr)
		{
			Var = *TempKnownConstant;
			UpdateAliasedVariable(AliasedVar, OriginalUnaliasedVar, Var);
		}

		if (!Var.IsValid())
		{
			int32 EncounterableFoundIdx = FNiagaraVariable::SearchArrayForPartialNameMatch(EncounterableExternalVariables, Name);

			if (EncounterableFoundIdx != INDEX_NONE)
			{
				Var = EncounterableExternalVariables[EncounterableFoundIdx];
				UpdateAliasedVariable(AliasedVar, OriginalUnaliasedVar, Var);
			}
		}

		if (Var.IsValid())
		{
			FoundIdx = AddVariableToHistory(Histories[ParamMapIdx], Var, AliasedVar, nullptr);
		}
		else
		{
			// This is overly spammy and doesn't provide useful info. Disabling for now.
			//UE_LOG(LogNiagaraEditor, Log, TEXT("Could not resolve variable: %s"), *Name.ToString());
		}
		
	}
	else
	{
		// Do nothing here
	}

	return FoundIdx;
}


void FNiagaraParameterMapHistoryBuilder::SetConstantByStaticVariable(int32& OutValue, const UEdGraphPin* InDefaultPin)
{
	const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();

	FNiagaraEditorModule& NiagaraEditorModule = FModuleManager::GetModuleChecked<FNiagaraEditorModule>("NiagaraEditor");
	OutValue = 0;
	FNiagaraVariable Var = Schema->PinToNiagaraVariable(InDefaultPin, true);
	FNiagaraVariable VarDefault = Var;
	FNiagaraEditorUtilities::ResetVariableToDefaultValue(VarDefault);// Do this to handle non-zero defaults
	if (VarDefault.GetType().IsStatic())
	{
		FNiagaraVariable VarWithValue = FNiagaraVariable(Var.GetType(), Var.GetName());
		FString Value;
		if (InDefaultPin && InDefaultPin->Direction == EEdGraphPinDirection::EGPD_Input && InDefaultPin->LinkedTo.Num() > 0 && InDefaultPin->LinkedTo[0] != nullptr)
		{
			UEdGraphPin* ConnectedOutputPin = InDefaultPin->LinkedTo[0];
			int32 InputConstant = GetConstantFromOutputPin(ConnectedOutputPin);
			if (InputConstant != INDEX_NONE)
			{
				Value = Constants[InputConstant];
			}
		}
		else if (InDefaultPin && InDefaultPin->Direction == EEdGraphPinDirection::EGPD_Input && InDefaultPin->LinkedTo.Num() == 0 && InDefaultPin->DefaultValue.Len() != 0)
		{
			Value = InDefaultPin->DefaultValue;
		}

		// If we found a string, we should try and map to the actual value of that variable..
		if (Value.Len() != 0)
		{
			TSharedPtr<INiagaraEditorTypeUtilities, ESPMode::ThreadSafe> TypeEditorUtilities = NiagaraEditorModule.GetTypeUtilities(Var.GetType());
			if (TypeEditorUtilities.IsValid() && TypeEditorUtilities->CanHandlePinDefaults() )
			{
				TypeEditorUtilities->SetValueFromPinDefaultString(Value, VarWithValue);
			}
		}

		if (VarWithValue.IsDataAllocated())
		{
			if (VarWithValue.GetType().IsSameBaseDefinition(FNiagaraTypeDefinition::GetBoolDef()))
			{
				OutValue = VarWithValue.GetValue<bool>();
			}
			else if (VarWithValue.GetType().IsSameBaseDefinition(FNiagaraTypeDefinition::GetIntDef()) || VarWithValue.GetType().IsEnum())
			{
				OutValue = VarWithValue.GetValue<int32>();
			}
		}
	}
}

int32 FNiagaraParameterMapHistoryBuilder::FindStaticVariable(const FNiagaraVariable& Var) const
{
	FString Value = Var.GetName().ToString();
	int32 FoundOverrideIdx = StaticVariables.IndexOfByPredicate([&](const FNiagaraVariable& InObj) -> bool
		{
			return (InObj.GetName() == *Value);
		});;

	return FoundOverrideIdx;
}

void FNiagaraParameterMapHistoryBuilder::SetConstantByStaticVariable(int32& OutValue, const FNiagaraVariable& Var)
{
	
	const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();

	OutValue = 0;
	FNiagaraVariable VarDefault = Var;
	FNiagaraEditorUtilities::ResetVariableToDefaultValue(VarDefault);// Do this to handle non-zero defaults
	if (VarDefault.GetType().IsStatic())
	{
		FNiagaraVariable VarWithValue = FNiagaraVariable(Var.GetType(), Var.GetName());
		FString Value = Var.GetName().ToString();
		int32 Input = INDEX_NONE;

		// If we found a string, we should try and map to the actual value of that variable..
		if (Value.Len() != 0)
		{
			if (!VarWithValue.IsDataAllocated())
			{
				int32 FoundOverrideIdx = StaticVariables.IndexOfByPredicate([&](const FNiagaraVariable& InObj) -> bool
					{
						return (InObj.GetName() == *Value);
					});;

				if (FoundOverrideIdx != INDEX_NONE)
				{
					VarWithValue.SetData(StaticVariables[FoundOverrideIdx].GetData());
				}
			}
		}

		if (VarWithValue.IsDataAllocated())
		{
			if (VarWithValue.GetType().IsSameBaseDefinition(FNiagaraTypeDefinition::GetBoolDef()))
			{
				OutValue = VarWithValue.GetValue<bool>();
			}
			else if (VarWithValue.GetType().IsSameBaseDefinition(FNiagaraTypeDefinition::GetIntDef()) || VarWithValue.GetType().IsEnum())
			{
				OutValue = VarWithValue.GetValue<int32>();
			}
		}
	}
	
}


int32 FNiagaraParameterMapHistoryBuilder::AddVariableToHistory(FNiagaraParameterMapHistory& History, const FNiagaraVariable& InVar, const FNiagaraVariable& InAliasedVar, const UEdGraphPin* InPin)
{
	const FString* ModuleAlias = GetModuleAlias();
	const FName ModuleName = ModuleAlias ? FName(**ModuleAlias) : NAME_None;
	
	return History.AddVariable(InVar, InAliasedVar, ModuleName, InPin);
}

void FNiagaraParameterMapHistoryBuilder::BuildCurrentAliases()
{
	if(RelevantScriptUsageContext.Num() > 0)
	{
		ResolveAliasContext = FNiagaraAliasContext(RelevantScriptUsageContext.Top());
	}
	else
	{
		ResolveAliasContext = FNiagaraAliasContext();
	}

	{
		TStringBuilder<1024> Callstack;
		for (int32 i = 0; i < FunctionNameContextStack.Num(); i++)
		{
			if (i != 0)
			{
				Callstack << TEXT(".");
			}
			FunctionNameContextStack[i].AppendString(Callstack);
		}

		if (Callstack.Len() > 0)
		{
			ResolveAliasContext.ChangeModuleToModuleName(Callstack.ToString());
		}

		Callstack.Reset();
		for (int32 i = 0; i < EmitterNameContextStack.Num(); i++)
		{
			if (i != 0)
			{
				Callstack << TEXT(".");
			}
			EmitterNameContextStack[i].AppendString(Callstack);
		}

		if (Callstack.Len() > 0)
		{
			ResolveAliasContext.ChangeEmitterToEmitterName(Callstack.ToString());
		}
	}

	{
		FString Callstack;
		for (int32 i = 0; i < RelevantScriptUsageContext.Num(); i++)
		{
			switch (RelevantScriptUsageContext[i])
			{
				/** The script defines a function for use in modules. */
			case ENiagaraScriptUsage::Function:
			case ENiagaraScriptUsage::Module:
			case ENiagaraScriptUsage::DynamicInput:
				break;
			case ENiagaraScriptUsage::ParticleSpawnScript:
			case ENiagaraScriptUsage::ParticleSpawnScriptInterpolated:
			case ENiagaraScriptUsage::ParticleUpdateScript:
			case ENiagaraScriptUsage::ParticleEventScript:
				Callstack = TEXT("Particles");
				break;
			case ENiagaraScriptUsage::ParticleSimulationStageScript:
			{
				if (ScriptUsageContextNameStack.Num() == 0 || ScriptUsageContextNameStack.Last() == NAME_None)
					Callstack = TEXT("Particles");
				else
					Callstack = ScriptUsageContextNameStack.Last().ToString();
			}
			break;
			case ENiagaraScriptUsage::ParticleGPUComputeScript:
				Callstack = TEXT("Particles");
				break;
			case ENiagaraScriptUsage::EmitterSpawnScript:
			case ENiagaraScriptUsage::EmitterUpdateScript:
				Callstack = TEXT("Emitter");
				{
					if (ResolveAliasContext.GetEmitterName().IsSet())
					{
						Callstack = *ResolveAliasContext.GetEmitterName().GetValue();
					}
				}
				break;
			case ENiagaraScriptUsage::SystemSpawnScript:
			case ENiagaraScriptUsage::SystemUpdateScript:
				Callstack = TEXT("System");
				break;
			}
		}

		if (!Callstack.IsEmpty())
		{
			ResolveAliasContext.ChangeStackContext(Callstack);
		}
	}
}

bool FCompileConstantResolver::ResolveConstant(FNiagaraVariable& OutConstant) const
{
	if (OutConstant == FNiagaraVariable(FNiagaraTypeDefinition::GetFunctionDebugStateEnum(), TEXT("Function.DebugState")))
	{
		FNiagaraInt32 EnumValue;
		EnumValue.Value = (uint8)CalculateDebugState();
		OutConstant.SetValue(EnumValue);
		return true;
	}

	// handle translator case
	if (Translator)
	{
		return Translator->GetLiteralConstantVariable(OutConstant);
	}

	// handle emitter case
	FVersionedNiagaraEmitterData* EmitterData = Emitter.GetEmitterData();
	if (EmitterData && OutConstant == FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("Emitter.Localspace")))
	{
		OutConstant.SetValue(EmitterData->bLocalSpace ? FNiagaraBool(true) : FNiagaraBool(false));
		return true;
	}
	if (EmitterData && OutConstant == FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("Emitter.Determinism")))
	{
		OutConstant.SetValue(EmitterData->bDeterminism ? FNiagaraBool(true) : FNiagaraBool(false));
		return true;
	}
	if (EmitterData && OutConstant == FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("Emitter.InterpolatedSpawn")))
	{
		OutConstant.SetValue(EmitterData->bInterpolatedSpawning ? FNiagaraBool(true) : FNiagaraBool(false));
		return true;
	}
	if (EmitterData && OutConstant == FNiagaraVariable(FNiagaraTypeDefinition::GetSimulationTargetEnum(), TEXT("Emitter.SimulationTarget")))
	{
		FNiagaraInt32 EnumValue;
		EnumValue.Value = (uint8)EmitterData->SimTarget;
		OutConstant.SetValue(EnumValue);
		return true;
	}
	if (EmitterData && OutConstant == FNiagaraVariable(FNiagaraTypeDefinition::GetScriptUsageEnum(), TEXT("Script.Usage")))
	{
		FNiagaraInt32 EnumValue;
		EnumValue.Value = (uint8)FNiagaraUtilities::ConvertScriptUsageToStaticSwitchUsage(Usage);
		OutConstant.SetValue(EnumValue);
		return true;
	}
	if (EmitterData && OutConstant == FNiagaraVariable(FNiagaraTypeDefinition::GetScriptContextEnum(), TEXT("Script.Context")))
	{
		FNiagaraInt32 EnumValue;
		EnumValue.Value = (uint8)FNiagaraUtilities::ConvertScriptUsageToStaticSwitchContext(Usage);
		OutConstant.SetValue(EnumValue);
		return true;
	}

	// handle system case
	if (System && OutConstant == FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("Emitter.Localspace")))
	{
		OutConstant.SetValue(FNiagaraBool(false));
		return true;
	}
	if (System && OutConstant == FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("Emitter.Determinism")))
	{
		OutConstant.SetValue(FNiagaraBool(true));
		return true;
	}
	if (System && OutConstant == FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("Emitter.InterpolatedSpawn")))
	{
		OutConstant.SetValue(FNiagaraBool(false));
		return true;
	}
	if (System && OutConstant == FNiagaraVariable(FNiagaraTypeDefinition::GetScriptUsageEnum(), TEXT("Script.Usage")))
	{
		FNiagaraInt32 EnumValue;
		EnumValue.Value = (uint8)FNiagaraUtilities::ConvertScriptUsageToStaticSwitchUsage(Usage);
		OutConstant.SetValue(EnumValue);
		return true;
	}
	if (System && OutConstant == FNiagaraVariable(FNiagaraTypeDefinition::GetScriptContextEnum(), TEXT("Script.Context")))
	{
		FNiagaraInt32 EnumValue;
		EnumValue.Value = (uint8)FNiagaraUtilities::ConvertScriptUsageToStaticSwitchContext(Usage);
		OutConstant.SetValue(EnumValue);
		return true;
	}

	return false;
}

ENiagaraFunctionDebugState FCompileConstantResolver::CalculateDebugState() const
{
	// System or Emitter
	const UNiagaraSystem* OwnerSystem = System ? System : (Emitter.Emitter ? Cast<const UNiagaraSystem>(Emitter.Emitter->GetOuter()) : nullptr);
	if ( OwnerSystem != nullptr )
	{
		return OwnerSystem->ShouldDisableDebugSwitches() ? ENiagaraFunctionDebugState::NoDebug : DebugState;
	}

	// Translator
	if ( Translator != nullptr )
	{
		return Translator->DisableDebugSwitches() ? ENiagaraFunctionDebugState::NoDebug : DebugState;
	}

	return DebugState;
}


FCompileConstantResolver FCompileConstantResolver::WithDebugState(ENiagaraFunctionDebugState InDebugState) const
{
	FCompileConstantResolver Copy = *this;
	Copy.DebugState = InDebugState;
	return Copy;
}

FCompileConstantResolver FCompileConstantResolver::WithUsage(ENiagaraScriptUsage ScriptUsage) const
{
	FCompileConstantResolver Copy = *this;
	Copy.Usage = ScriptUsage;
	return Copy;
}

uint32 FCompileConstantResolver::BuildTypeHash() const
{
	union
	{
		struct  
		{
			uint8 Usage;
			uint8 DebugState;
			uint8 EmitterSimTarget;
			uint8 bHasSystem : 1;
			uint8 bHasEmitter : 1;
			uint8 bEmitterLocalSpace : 1;
			uint8 bEmitterDeterminism : 1;
			uint8 bEmitterInterpolatedSpawn : 1;
		};
		uint32 Hash;
	} HashUnion;

	HashUnion.Hash = 0;

	if (Emitter.Emitter)
	{
		HashUnion.bHasEmitter = true;
		if (FVersionedNiagaraEmitterData* EmitterData = Emitter.GetEmitterData())
		{
			HashUnion.EmitterSimTarget = (uint8)EmitterData->SimTarget;
			HashUnion.bEmitterLocalSpace = EmitterData->bLocalSpace;
			HashUnion.bEmitterDeterminism = EmitterData->bDeterminism;
			HashUnion.bEmitterInterpolatedSpawn = EmitterData->bInterpolatedSpawning;
		}
	}

	if (System)
	{
		HashUnion.bHasSystem = true;
	}

	HashUnion.Usage = (uint8)Usage;
	HashUnion.DebugState = (uint8)DebugState;

	return HashUnion.Hash;
}

int32 FNiagaraParameterMapHistoryWithMetaDataBuilder::AddVariableToHistory(FNiagaraParameterMapHistory& History, const FNiagaraVariable& InVar, const FNiagaraVariable& InAliasedVar, const UEdGraphPin* InPin)
{
	const FString* ModuleAlias = GetModuleAlias();
	const FName ModuleName = ModuleAlias ? FName(**ModuleAlias) : NAME_None;

	TOptional<FNiagaraVariableMetaData> MetaData = CallingGraphContext.Last()->GetMetaData(InAliasedVar);
	if (MetaData.IsSet())
	{
		return History.AddVariable(InVar, InAliasedVar, ModuleName, InPin, MetaData);
	}
	TOptional<FNiagaraVariableMetaData> BlankMetaData = FNiagaraVariableMetaData();
	return History.AddVariable(InVar, InAliasedVar, ModuleName, InPin, BlankMetaData);
}

#undef LOCTEXT_NAMESPACE
