// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraCompilationTasks.h"

#include "Algo/RemoveIf.h"
#include "Misc/PathViews.h"
#include "NiagaraCompilationTypes.h"
#include "NiagaraDigestDatabase.h"
#include "NiagaraEmitter.h"
#include "NiagaraGraphDigest.h"
#include "NiagaraCompilationBridge.h"
#include "NiagaraScriptSource.h"
#include "NiagaraShader.h"
#include "NiagaraSimulationStageBase.h"
#include "NiagaraSystem.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

// needed for AsType...pretty ugly
#include "NiagaraNodeOutput.h"
#include "NiagaraNodeFunctionCall.h"

namespace NiagaraCompilationCopyImpl
{
	void GetUsagesToDuplicate(ENiagaraScriptUsage TargetUsage, TArray<ENiagaraScriptUsage>& DuplicateUsages)
	{
		// For now we need to include both spawn and update for each target usage, otherwise attribute lists in the precompiled histories aren't generated correctly.
		switch (TargetUsage)
		{
		case ENiagaraScriptUsage::SystemSpawnScript:
		case ENiagaraScriptUsage::SystemUpdateScript:
			DuplicateUsages.Add(ENiagaraScriptUsage::SystemSpawnScript);
			DuplicateUsages.Add(ENiagaraScriptUsage::SystemUpdateScript);
			DuplicateUsages.Add(ENiagaraScriptUsage::EmitterSpawnScript);
			DuplicateUsages.Add(ENiagaraScriptUsage::EmitterUpdateScript);
			break;
		case ENiagaraScriptUsage::ParticleSpawnScript:
		case ENiagaraScriptUsage::ParticleSpawnScriptInterpolated:
		case ENiagaraScriptUsage::ParticleUpdateScript:
		case ENiagaraScriptUsage::ParticleEventScript:
		case ENiagaraScriptUsage::ParticleSimulationStageScript:
		case ENiagaraScriptUsage::ParticleGPUComputeScript:
			DuplicateUsages.Add(ENiagaraScriptUsage::ParticleSpawnScript);
			DuplicateUsages.Add(ENiagaraScriptUsage::ParticleSpawnScriptInterpolated);
			DuplicateUsages.Add(ENiagaraScriptUsage::ParticleUpdateScript);
			DuplicateUsages.Add(ENiagaraScriptUsage::ParticleEventScript);
			DuplicateUsages.Add(ENiagaraScriptUsage::ParticleSimulationStageScript);
			DuplicateUsages.Add(ENiagaraScriptUsage::ParticleGPUComputeScript);
			break;
		}
	}

	TArray<UNiagaraScript*> FindDependentScripts(UNiagaraSystem* System, FVersionedNiagaraEmitterData* EmitterData, UNiagaraScript* Script)
	{
		TArray<UNiagaraScript*> DependentScripts;

		const ENiagaraScriptUsage ScriptUsage = Script->GetUsage();

		if (System)
		{
			if (UNiagaraScript::IsEquivalentUsage(ScriptUsage, ENiagaraScriptUsage::EmitterSpawnScript))
			{
				DependentScripts.Add(System->GetSystemSpawnScript());
			}

			if (UNiagaraScript::IsEquivalentUsage(ScriptUsage, ENiagaraScriptUsage::EmitterUpdateScript))
			{
				DependentScripts.Add(System->GetSystemUpdateScript());
			}
		}

		if (EmitterData)
		{
			const bool bIsGpuEmitter = EmitterData->SimTarget == ENiagaraSimTarget::GPUComputeSim;

			if (UNiagaraScript::IsEquivalentUsage(ScriptUsage, ENiagaraScriptUsage::ParticleSpawnScript))
			{
				if (bIsGpuEmitter)
				{
					DependentScripts.Add(EmitterData->GetGPUComputeScript());
				}
			}

			if (UNiagaraScript::IsEquivalentUsage(ScriptUsage, ENiagaraScriptUsage::ParticleUpdateScript))
			{
				if (bIsGpuEmitter)
				{
					DependentScripts.Add(EmitterData->GetGPUComputeScript());
				}
				else if (EmitterData->bInterpolatedSpawning)
				{
					DependentScripts.Add(EmitterData->SpawnScriptProps.Script);
				}
			}
		}

		return DependentScripts;
	}

	static UE::DerivedData::FCacheKey BuildNiagaraDDCCacheKey(const FNiagaraVMExecutableDataId& CompileId, const FString& ScriptPath)
	{
		enum { UE_NIAGARASCRIPT_DERIVEDDATA_VER = 2 };

		FString KeyString;
		KeyString.Reserve(1024);

		KeyString.Appendf(TEXT("%i_%i"),
			(int32)UE_NIAGARASCRIPT_DERIVEDDATA_VER, GNiagaraSkipVectorVMBackendOptimizations);

		KeyString.AppendChar(TCHAR('_'));
		KeyString.Append(ScriptPath);
		KeyString.AppendChar(TCHAR('_'));

		CompileId.AppendKeyString(KeyString);

		static UE::DerivedData::FCacheBucket Bucket("NiagaraScript");
		return { Bucket, FIoHash::HashBuffer(MakeMemoryView(FTCHARToUTF8(KeyString))) };
	}

	static bool ValidateExecData(const UNiagaraScript* Script, const FNiagaraVMExecutableData& ExecData, FString& ErrorString)
	{
		bool IsValid = true;

		for (const auto& Attribute : ExecData.Attributes)
		{
			if (!Attribute.IsValid())
			{
				ErrorString.Appendf(TEXT("Failure - %s - Attribute [%s] is invalid!\n"), Script ? *Script->GetFullName() : TEXT("<unknown>"), *Attribute.GetName().ToString());
				IsValid = false;
			}
		}

		for (const auto& Parameter : ExecData.Parameters.Parameters)
		{
			if (!Parameter.IsValid())
			{
				ErrorString.Appendf(TEXT("Failure - %s - Parameter [%s] is invalid!\n"), Script ? *Script->GetFullName() : TEXT("<unknown>"), *Parameter.GetName().ToString());
				IsValid = false;
			}
		}

		return IsValid;
	}

	static bool BinaryToExecData(const UNiagaraScript* Script, FSharedBuffer SharedBuffer, FNiagaraVMExecutableData& OutExecData)
	{
		if (!UNiagaraScript::ShouldUseDDC())
		{
			return false;
		}

		if (SharedBuffer.GetSize() == 0)
		{
			return false;
		}

		FMemoryReaderView Ar(SharedBuffer.GetView(), true);
		FObjectAndNameAsStringProxyArchive SafeAr(Ar, false);
		OutExecData.SerializeData(SafeAr, true);

		FString ValidationErrors;
		if (!ValidateExecData(Script, OutExecData, ValidationErrors))
		{
			UE_LOG(LogNiagaraEditor, Display, TEXT("Failed to validate FNiagaraVMExecutableData received from DDC, rejecting!  Reasons:\n%s"), *ValidationErrors);
			return false;
		}

		return !SafeAr.IsError();
	}

	static FSharedBuffer ExecToBinaryData(const UNiagaraScript* Script, FNiagaraVMExecutableData& InExecData)
	{
		if (!UNiagaraScript::ShouldUseDDC())
		{
			return FSharedBuffer();
		}

		FString ValidationErrors;
		if (!ValidateExecData(Script, InExecData, ValidationErrors))
		{
			UE_LOG(LogNiagaraEditor, Error, TEXT("Failed to validate FNiagaraVMExecutableData being pushed to DDC, rejecting!  Errors:\n%s"), *ValidationErrors);
			return FSharedBuffer();
		}

		TArray<uint8> BinaryData;
		FMemoryWriter Ar(BinaryData, true);
		FObjectAndNameAsStringProxyArchive SafeAr(Ar, false);
		InExecData.SerializeData(SafeAr, true);

		if (!BinaryData.IsEmpty() && !SafeAr.IsError())
		{
			return MakeSharedBufferFromArray(MoveTemp(BinaryData));
		}

		return FSharedBuffer();
	}

	static TArray<FNiagaraVariable> ExtractInputVariablesFromHistory(const TNiagaraParameterMapHistory<FNiagaraCompilationDigestBridge>& History, const FNiagaraCompilationGraph* FunctionGraph)
	{
		TArray<FNiagaraVariable> InputVariables;

		const int32 ModuleNamespaceLength = FNiagaraConstants::ModuleNamespaceString.Len();

		for (int32 i = 0; i < History.Variables.Num(); i++)
		{
			const FNiagaraVariable& Variable = History.Variables[i];
			const TArray<TNiagaraParameterMapHistory<FNiagaraCompilationDigestBridge>::FReadHistory>& ReadHistory = History.PerVariableReadHistory[i];

			// A read is only really exposed if it's the first read and it has no corresponding write.
			if (ReadHistory.Num() > 0 && ReadHistory[0].PreviousWritePin.Pin == nullptr)
			{
				const FNiagaraCompilationPin* InputPin = ReadHistory[0].ReadPin.Pin;

				if (FNiagaraStackGraphUtilities::IsRapidIterationType(InputPin->Variable.GetType()))
				{
					// Make sure that the module input is from the called graph, and not a nested graph.
					const FNiagaraCompilationGraph* NodeGraph = InputPin->OwningNode->OwningGraph;
					if (NodeGraph == FunctionGraph)
					{
						FNameBuilder InputPinName(InputPin->PinName);
						FStringView InputPinView(InputPinName);
						if (InputPinView.StartsWith(FNiagaraConstants::ModuleNamespaceString)
							&& InputPinView.Len() > ModuleNamespaceLength
							&& InputPinView[ModuleNamespaceLength] == TCHAR('.'))
						{
							InputVariables.Add(InputPin->Variable);
						}
					}
				}
			}
		}

		return InputVariables;
	}

	void AddRapidIterationParameter(
		const FNiagaraParameterHandle& ParameterHandle,
		const FNiagaraVariable& InputVariable,
		const FNiagaraVariable& Parameter,
		const FNiagaraCompilationNodeFunctionCall* FunctionCallNode,
		FNiagaraParameterStore& ParameterStore)
	{
		const int32 ExistingParameterIndex = ParameterStore.IndexOf(Parameter);
		if (ExistingParameterIndex == INDEX_NONE)
		{
			// we only try to add the variable if we've already determined that it's got a reasonable
			// default value and there's no override pin for the function input
			if (Parameter.IsDataAllocated() && !FunctionCallNode->HasOverridePin(ParameterHandle))
			{
				constexpr bool bAddParameterIfMissing = true;
				ParameterStore.SetParameterData(Parameter.GetData(), Parameter, bAddParameterIfMissing);
			}
		}
	}

	void FixupRenamedParameter(
		const FNiagaraVariable& InputVariable,
		const FNiagaraVariable& Parameter,
		const FNiagaraCompilationNodeFunctionCall* FunctionCallNode,
		const TMultiMap<FGuid, FNiagaraVariable>& GuidMapping,
		FNiagaraParameterStore& ParameterStore)
	{
		TOptional<FNiagaraVariableMetaData> VariableMetaData = FunctionCallNode->CalledGraph->GetMetaData(InputVariable);
		if (!VariableMetaData.IsSet())
		{
			return;
		}

		TArray<FNiagaraVariable> ExistingParameters;
		GuidMapping.MultiFind(VariableMetaData->GetVariableGuid(), ExistingParameters);

		// if we didn't match anything then there's nothing left for us to do
		if (ExistingParameters.IsEmpty())
		{
			return;
		}

		for (const FNiagaraVariable& ExistingParameter : ExistingParameters)
		{
			// if we've found a match then there's nothing to rename
			if (ExistingParameter.GetName() == Parameter.GetName())
			{
				return;
			}
		}

		// next go through to see if we may need to actually do a rename.  We'll only do this if we have something
		// that matches the GUID and is in the same namespace
		FNameBuilder NameBuilder(Parameter.GetName());
		int32 NamespaceStrCount;
		if (NameBuilder.ToView().FindLastChar(TEXT('.'), NamespaceStrCount))
		{
			FStringView ParameterNamespace = NameBuilder.ToView().Left(NamespaceStrCount);

			for (const FNiagaraVariable& ExistingParameter : ExistingParameters)
			{
				if (ExistingParameter.IsInNameSpace(ParameterNamespace))
				{
					ParameterStore.RenameParameter(ExistingParameter, Parameter.GetName());
					return;
				}
			}
		}
	}

	template<typename T, typename InAllocatorType, typename OutAllocatorType>
	static void AppendUnique(const TArray<T, InAllocatorType>& Input, TArray<T, OutAllocatorType>& Output)
	{
		Output.Reserve(Input.Num() + Output.Num());
		for (const T& InputValue : Input)
		{
			Output.AddUnique(InputValue);
		}
	}

	const UNiagaraGraph* GetGraphFromScriptSource(const UNiagaraScriptSourceBase* ScriptSourceBase)
	{
		if (const UNiagaraScriptSource* ScriptSource = CastChecked<const UNiagaraScriptSource>(ScriptSourceBase))
		{
			return ScriptSource->NodeGraph;
		}
		return nullptr;
	}

	const UNiagaraGraph* GetGraphFromScript(const UNiagaraScript* Script)
	{
		if (Script)
		{
			return GetGraphFromScriptSource(Script->GetLatestSource());
		}
		return nullptr;
	}
} // NiagaraCompilationCopyImpl

FNiagaraSystemCompilationTask::FNiagaraSystemCompilationTask(FNiagaraCompilationTaskHandle InTaskHandle, UNiagaraSystem* InSystem)
	: TaskHandle(InTaskHandle)
	, DDCRequestOwner(UE::DerivedData::EPriority::Normal)
	, System_GT(InSystem)
{
}

void FNiagaraSystemCompilationTask::Abort()
{
	bAborting = true;
}

FNiagaraSystemCompilationTask::FCompileGroupInfo::FCompileGroupInfo(int32 InEmitterIndex)
: EmitterIndex(InEmitterIndex)
{}

bool FNiagaraSystemCompilationTask::FCompileGroupInfo::HasOutstandingCompileTasks(const FNiagaraSystemCompilationTask& ParentTask) const
{
	for (int32 CompileTaskIndex : CompileTaskIndices)
	{
		if (ParentTask.CompileTasks[CompileTaskIndex].IsOutstanding())
		{
			return true;
		}
	}

	return false;
}

void FNiagaraSystemCompilationTask::FCompileGroupInfo::InstantiateCompileGraph(const FNiagaraSystemCompilationTask& ParentTask)
{
	CompilationCopy = MakeShared<FNiagaraCompilationCopyData, ESPMode::ThreadSafe>();
	FNiagaraCompilationCopyData& BasePtr = *CompilationCopy.Get();

	TArray<TSharedPtr<FNiagaraCompilationCopyData, ESPMode::ThreadSafe>> DependentRequests;
	FCompileConstantResolver EmptyResolver;

	TArray<UClass*> DataInterfaceClasses;

	auto CollectDataInterfaceClasses = [&](TConstArrayView<FNiagaraVariable> Variables)
	{
		// Collect classes for external encounterable variables
		for (const FNiagaraVariable& EncounterableVariable : Variables)
		{
			if (EncounterableVariable.IsDataInterface())
			{
				DataInterfaceClasses.AddUnique(EncounterableVariable.GetType().GetClass());
			}
		}
	};

	BasePtr.ValidUsages = ValidUsages;

	// First deep copy all the emitter graphs referenced by the system so that we can later hook up emitter handles in the system traversal.
	const int32 EmitterCount = ParentTask.SystemInfo.EmitterInfo.Num();
	BasePtr.EmitterData.Reserve(EmitterCount);

	for (int32 EmitterIt = 0; EmitterIt < EmitterCount; ++EmitterIt)
	{
		const FEmitterInfo& EmitterInfo = ParentTask.SystemInfo.EmitterInfo[EmitterIt];

		TSharedPtr<FNiagaraCompilationCopyData, ESPMode::ThreadSafe> EmitterPtr = MakeShared<FNiagaraCompilationCopyData, ESPMode::ThreadSafe>();
		EmitterPtr->EmitterUniqueName = EmitterInfo.UniqueEmitterName;
		EmitterPtr->ValidUsages = BasePtr.ValidUsages;

		// Don't need to copy the graph if we aren't going to use it.
		if (EmitterInfo.Enabled && ((EmitterIndex == INDEX_NONE) || (EmitterIt == EmitterIndex)))
		{
			const FNiagaraPrecompileData* EmitterRequestData = static_cast<const FNiagaraPrecompileData*>(ParentTask.SystemPrecompileData->GetDependentRequest(EmitterIt).Get());

			if (const FNiagaraCompilationGraph* SourceGraph = EmitterInfo.SourceGraph.Get())
			{
				EmitterPtr->InstantiateCompilationCopy(*SourceGraph, EmitterRequestData, ENiagaraScriptUsage::EmitterSpawnScript, EmitterInfo.ConstantResolver);
			}
		}
		EmitterPtr->ValidUsages = BasePtr.ValidUsages;
		BasePtr.EmitterData.Add(EmitterPtr);
	}

	// Now deep copy the system graphs, skipping traversal into any emitter references.
	{
		TArray<FNiagaraVariable> EncounterableSystemVariables;
		ParentTask.SystemPrecompileData->GatherPreCompiledVariables(FString(), EncounterableSystemVariables);

		// skip the deep copy if we're not compiling the system scripts
		if (BasePtr.ValidUsages.Contains(ENiagaraScriptUsage::SystemSpawnScript))
		{
			if (const FNiagaraCompilationGraph* SourceGraph = ParentTask.SystemInfo.SystemSourceGraph.Get())
			{
				BasePtr.InstantiateCompilationCopy(*SourceGraph, ParentTask.SystemPrecompileData.Get(), ENiagaraScriptUsage::SystemSpawnScript, ParentTask.SystemInfo.ConstantResolver);
			}

			BasePtr.CreateParameterMapHistory(ParentTask, EncounterableSystemVariables, ParentTask.SystemInfo.StaticVariableResults, ParentTask.SystemInfo.ConstantResolver, {});

			// bubble up the referenced DI classes to the system
			for (const FNiagaraCompilationCopyData::FSharedCompilationCopy& EmitterData : BasePtr.EmitterData)
			{
				BasePtr.AggregatedDataInterfaceCDODuplicates.Append(EmitterData->AggregatedDataInterfaceCDODuplicates);
			}
		}

		CollectDataInterfaceClasses(EncounterableSystemVariables);
	}

	// Now we can finish off the emitters.
	for (int32 EmitterIt = 0; EmitterIt < EmitterCount; ++EmitterIt)
	{
		const FEmitterInfo& EmitterInfo = ParentTask.SystemInfo.EmitterInfo[EmitterIt];

		TArray<FNiagaraVariable> EncounterableEmitterVariables;
		ParentTask.SystemPrecompileData->GetDependentRequest(EmitterIt)->GatherPreCompiledVariables(FString(), EncounterableEmitterVariables);

		if (EmitterInfo.Enabled && ((EmitterIndex == INDEX_NONE) || (EmitterIt == EmitterIndex)))
		{
			TArray<FNiagaraVariable> StaticVariablesFromEmitter = ParentTask.SystemInfo.StaticVariableResults;
			StaticVariablesFromEmitter.Append(EmitterInfo.StaticVariableResults);

			BasePtr.EmitterData[EmitterIt]->CreateParameterMapHistory(ParentTask, EncounterableEmitterVariables, StaticVariablesFromEmitter, EmitterInfo.ConstantResolver, EmitterInfo.SimStages);
		}
	}
}

bool FNiagaraSystemCompilationTask::FCompileTaskInfo::IsOutstanding() const
{
	return !ExeData.IsValid();
}


FNiagaraSystemCompilationTask::FCompileGroupInfo* FNiagaraSystemCompilationTask::GetCompileGroupInfo(int32 EmitterIndex)
{
	return CompileGroups.FindByPredicate([EmitterIndex](const FCompileGroupInfo& GroupInfo) -> bool
	{
		return GroupInfo.EmitterIndex == EmitterIndex;
	});
}

const FNiagaraSystemCompilationTask::FScriptInfo* FNiagaraSystemCompilationTask::GetScriptInfo(const TObjectKey<UNiagaraScript>& ScriptKey) const
{
	return DigestedScriptInfo.Find(ScriptKey);
}

void FNiagaraSystemCompilationTask::FCompileTaskInfo::CollectNamedDataInterfaces(FNiagaraSystemCompilationTask* SystemCompileTask, const FCompileGroupInfo& GroupInfo)
{
	const FNiagaraCompilationCopyData* CompilationCopyData = GroupInfo.EmitterIndex == INDEX_NONE
		? GroupInfo.CompilationCopy.Get()
		: static_cast<const FNiagaraCompilationCopyData*>(GroupInfo.CompilationCopy->GetDependentRequest(GroupInfo.EmitterIndex).Get());
	const FScriptInfo& ScriptInfo = SystemCompileTask->DigestedScriptInfo.FindChecked(ScriptKey);
	FString UniqueEmitterName;

	if (SystemCompileTask->SystemInfo.EmitterInfo.IsValidIndex(GroupInfo.EmitterIndex))
	{
		UniqueEmitterName = SystemCompileTask->SystemInfo.EmitterInfo[GroupInfo.EmitterIndex].UniqueEmitterName;
	}

	auto AccumulateInputDataInterfaces = [this, &UniqueEmitterName](const FNiagaraCompilationNode& Node) -> bool
	{
		if (const FNiagaraCompilationNodeInput* InputNode = Node.AsType<FNiagaraCompilationNodeInput>())
		{
			if (InputNode->DataInterfaceName != NAME_None && InputNode->InstancedDataInterface)
			{
				constexpr bool bIsParameterMapDataInterface = false;
				FName DIName = FNiagaraHlslTranslator::GetDataInterfaceName(InputNode->DataInterfaceName, UniqueEmitterName, bIsParameterMapDataInterface);

				NamedDataInterfaces.Add(DIName, InputNode->InstancedDataInterface);
			}
		}

		return true;
	};

	auto DefaultNodeFinder = [&ScriptInfo](const FNiagaraCompilationNodeOutput& OutputNode) -> bool
	{
		if (UNiagaraScript::IsEquivalentUsage(OutputNode.Usage, ScriptInfo.Usage) && OutputNode.UsageId == ScriptInfo.UsageId)
		{
			return true;
		}

		if (ScriptInfo.Usage == ENiagaraScriptUsage::ParticleGPUComputeScript)
		{
			return UNiagaraScript::IsParticleScript(OutputNode.Usage);
		}
		else if (ScriptInfo.Usage == ENiagaraScriptUsage::ParticleSpawnScriptInterpolated)
		{
			return UNiagaraScript::IsEquivalentUsage(OutputNode.Usage, ENiagaraScriptUsage::ParticleUpdateScript);
		}

		return OutputNode.Usage == ScriptInfo.Usage && OutputNode.UsageId == ScriptInfo.UsageId;
	};

	CompilationCopyData->InstantiatedGraph->NodeTraversal(true, false, DefaultNodeFinder, AccumulateInputDataInterfaces);

	if (UNiagaraScript::IsSystemScript(ScriptInfo.Usage))
	{
		const ENiagaraScriptUsage EmitterUsage = ScriptInfo.Usage == ENiagaraScriptUsage::SystemSpawnScript
			? ENiagaraScriptUsage::EmitterSpawnScript
			: ENiagaraScriptUsage::EmitterUpdateScript;
		const FGuid EmitterUsageId = ScriptInfo.UsageId;
		auto EmitterNodeFinder = [EmitterUsage, EmitterUsageId](const FNiagaraCompilationNodeOutput& OutputNode) -> bool
		{
			return UNiagaraScript::IsEquivalentUsage(OutputNode.Usage, EmitterUsage) && OutputNode.UsageId == EmitterUsageId;
		};

		check(GroupInfo.EmitterIndex == INDEX_NONE);
		for (const FNiagaraCompilationCopyData::FSharedCompilationCopy& EmitterCompilationCopy : CompilationCopyData->EmitterData)
		{
			if (const FNiagaraCompilationGraph* EmitterGraph = EmitterCompilationCopy->InstantiatedGraph.Get())
			{
				UniqueEmitterName = EmitterCompilationCopy->EmitterUniqueName;
				EmitterGraph->NodeTraversal(true, false, EmitterNodeFinder, AccumulateInputDataInterfaces);
			}
		}
	}
}

void FNiagaraSystemCompilationTask::FCompileTaskInfo::TranslateAndIssueCompile(FNiagaraSystemCompilationTask* SystemCompileTask, const FCompileGroupInfo& GroupInfo)
{
	const FNiagaraPrecompileData* PrecompileData = GroupInfo.EmitterIndex == INDEX_NONE
		? SystemCompileTask->SystemPrecompileData.Get()
		: static_cast<const FNiagaraPrecompileData*>(SystemCompileTask->SystemPrecompileData->GetDependentRequest(GroupInfo.EmitterIndex).Get());

	const FNiagaraCompilationCopyData* CompilationCopyData = GroupInfo.EmitterIndex == INDEX_NONE
		? GroupInfo.CompilationCopy.Get()
		: static_cast<const FNiagaraCompilationCopyData*>(GroupInfo.CompilationCopy->GetDependentRequest(GroupInfo.EmitterIndex).Get());

	const FScriptInfo& ScriptInfo = SystemCompileTask->DigestedScriptInfo.FindChecked(ScriptKey);

	// TRANSLATION
	{
		double TranslationStartTime = FPlatformTime::Seconds();

		BakedRapidIterationParameters = PrecompileData->BakedRapidIterationParameters;

		FHlslNiagaraTranslatorOptions TranslateOptions;

		if (CompileOptions.TargetUsage == ENiagaraScriptUsage::ParticleGPUComputeScript)
		{
			TranslateOptions.SimTarget = ENiagaraSimTarget::GPUComputeSim;
		}
		else
		{
			TranslateOptions.SimTarget = ENiagaraSimTarget::CPUSim;
		}
		TranslateOptions.OverrideModuleConstants = BakedRapidIterationParameters;
		TranslateOptions.bParameterRapidIteration = PrecompileData->GetUseRapidIterationParams();
		TranslateOptions.bDisableDebugSwitches = PrecompileData->GetDisableDebugSwitches();

		FString PathName = SourceScript->GetPathName();

		TUniquePtr<INiagaraHlslTranslator> Translator = INiagaraHlslTranslator::CreateTranslator(PrecompileData, CompilationCopyData);
		TranslateResults = Translator->Translate(CompileOptions, TranslateOptions);
		TranslateOutput = Translator->GetTranslateOutput();
		TranslatedHlsl = Translator->GetTranslatedHLSL();

		TranslationTime = (float) (FPlatformTime::Seconds() - TranslationStartTime);
	}

	// COMPILATION
	{
		Compiler = MakeShared<FHlslNiagaraCompiler>();

		TStringBuilder<512> DebugGroupName;
		FPathViews::Append(DebugGroupName, PrecompileData->SourceName);
		FPathViews::Append(DebugGroupName, PrecompileData->EmitterUniqueName);
		FPathViews::Append(DebugGroupName, PrecompileData->ENiagaraScriptUsageEnum->GetNameStringByValue((int64)CompileOptions.TargetUsage));

		if (CompileOptions.TargetUsageId.IsValid())
		{
			DebugGroupName << TEXT("_");
			CompileOptions.TargetUsageId.AppendString(DebugGroupName, EGuidFormats::Digits);
		}

		CompilationJobId = Compiler->CompileScript(DebugGroupName, CompileOptions, TranslateResults, TranslateOutput, TranslatedHlsl);
	}
}

/** Returns true if the task has valid results */
bool FNiagaraSystemCompilationTask::FCompileTaskInfo::RetrieveCompilationResult(bool bWait)
{
	check(CompileResultsReadyEvent.IsValid() && CompileResultsReadyEvent->IsValid());

	if (CompileResultsReadyEvent->IsCompleted())
	{
		check(ExeData.IsValid());
		return true;
	}

	check(!ExeData.IsValid());

	TOptional<FNiagaraCompileResults> CompileResult;
	CompileResult = Compiler->GetCompileResult(CompilationJobId, bWait);
	if (!CompileResult)
	{
		return false;
	}

	FNiagaraCompileResults& Results = CompileResult.GetValue();

	FString OutGraphLevelErrorMessages;
	for (const FNiagaraCompileEvent& Message : Results.CompileEvents)
	{
#if defined(NIAGARA_SCRIPT_COMPILE_LOGGING_MEDIUM)
		UE_LOG(LogNiagaraEditor, Log, TEXT("%s"), *Message.Message);
#endif
		if (Message.Severity == FNiagaraCompileEventSeverity::Error)
		{
			// Write the error messages to the string as well so that they can be echoed up the chain.
			if (OutGraphLevelErrorMessages.Len() > 0)
			{
				OutGraphLevelErrorMessages += "\n";
			}
			OutGraphLevelErrorMessages += Message.Message;
		}
	}
	Results.Data->ErrorMsg = OutGraphLevelErrorMessages;

	Results.Data->LastCompileStatus = (FNiagaraCompileResults::CompileResultsToSummary(&Results));
	if (Results.Data->LastCompileStatus != ENiagaraScriptCompileStatus::NCS_Error)
	{
		// When there are no errors the compile events get emptied, so add them back here.
		Results.Data->LastCompileEvents.Append(Results.CompileEvents);
	}

	ExeData = CompileResult->Data;

	// we also need to include the information about the rapid iteration parameters that were used
	// to generate the ExeData
	ExeData->BakedRapidIterationParameters = BakedRapidIterationParameters;

	CompileResultsReadyEvent->Trigger();

	CompilerWallTime = CompileResult->CompilerWallTime;
	CompilerPreprocessTime = CompileResult->CompilerPreprocessTime;
	CompilerWorkerTime = CompileResult->CompilerWorkerTime;

	return true;
}

void FNiagaraSystemCompilationTask::FCompileTaskInfo::ProcessCompilation(FNiagaraSystemCompilationTask* SystemCompileTask, const FCompileGroupInfo& GroupInfo)
{
	ON_SCOPE_EXIT
	{
		CompileResultsProcessedEvent->Trigger();
	};

#if VECTORVM_SUPPORTS_LEGACY
	const bool bExperimentalVMDisabled = CompileOptions.AdditionalDefines.Contains(FNiagaraCompileOptions::ExperimentalVMDisabled);

	if (bExperimentalVMDisabled)
	{
		return;
	}
#endif

#if VECTORVM_SUPPORTS_EXPERIMENTAL
	if (const uint8* ByteCode = ExeData->ByteCode.GetDataPtr())
	{
		const double OptimizeStartTime = FPlatformTime::Seconds();

		//this is just necessary because VectorVM doesn't know about FVMExternalFunctionBindingInfo
		const int32 NumExtFns = ExeData->CalledVMExternalFunctions.Num();

		TArray<FVectorVMExtFunctionData> ExtFnTable;
		ExtFnTable.SetNumZeroed(NumExtFns);
		for (int i = 0; i < NumExtFns; ++i)
		{
			ExtFnTable[i].NumInputs = ExeData->CalledVMExternalFunctions[i].GetNumInputs();
			ExtFnTable[i].NumOutputs = ExeData->CalledVMExternalFunctions[i].GetNumOutputs();
		}

		FVectorVMOptimizeContext OptimizeContext;
		FMemory::Memzero(&OptimizeContext, sizeof(OptimizeContext));
		OptimizeVectorVMScript(ByteCode, ExeData->ByteCode.GetLength(), ExtFnTable.GetData(), ExtFnTable.Num(), &OptimizeContext, VVMFlag_OptOmitStats);

		// freeze the OptimizeContext
		FreezeVectorVMOptimizeContext(OptimizeContext, ExeData->ExperimentalContextData);

		FreeVectorVMOptimizeContext(&OptimizeContext);

		ByteCodeOptimizeTime = (float) (OptimizeStartTime - FPlatformTime::Seconds());
	}
#endif // VECTORVM_SUPPORTS_EXPERIMENTAL

}

void FNiagaraSystemCompilationTask::FDispatchAndProcessDataCacheGetRequests::Launch(FNiagaraSystemCompilationTask* SystemCompileTask)
{
	// todo - incorporate something here where we skip if we try to update the compileid (post ri collection)
	// and there's no actual change to the compileid...in that case we shouldn't worry about trying to grab
	// stuff from the ddc again


	using namespace UE::DerivedData;

	TArray<FCacheGetValueRequest> GetRequests;
	const int32 CompileTaskCount = SystemCompileTask->CompileTasks.Num();

	GetRequests.Reserve(CompileTaskCount);

	for (int32 CompileTaskIt = 0; CompileTaskIt < CompileTaskCount; ++CompileTaskIt)
	{
		FNiagaraSystemCompilationTask::FCompileTaskInfo& CompileTask = SystemCompileTask->CompileTasks[CompileTaskIt];
		if (CompileTask.IsOutstanding())
		{
			FCacheGetValueRequest& GetValueRequest = GetRequests.AddDefaulted_GetRef();
			GetValueRequest.Name = CompileTask.AssetPath;
			GetValueRequest.Key = NiagaraCompilationCopyImpl::BuildNiagaraDDCCacheKey(CompileTask.CompileId, CompileTask.AssetPath);
			GetValueRequest.UserData = (uint64)CompileTaskIt;

			CompileTask.DataCacheGetKey = GetValueRequest.Key;
		}
	}

	PendingGetRequestCount = GetRequests.Num();

	if (PendingGetRequestCount > 0)
	{
		FRequestBarrier AsyncBarrier(SystemCompileTask->DDCRequestOwner);
		GetCache().GetValue(GetRequests, SystemCompileTask->DDCRequestOwner, [this, SystemCompileTask](FCacheGetValueResponse&& Response)
		{
			if (ensure(SystemCompileTask->CompileTasks.IsValidIndex(Response.UserData)))
			{
				FNiagaraSystemCompilationTask::FCompileTaskInfo& CompileTask = SystemCompileTask->CompileTasks[Response.UserData];
				if (Response.Status == EStatus::Ok)
				{
					FSharedBuffer DDCData = Response.Value.GetData().Decompress();
					FNiagaraVMExecutableData ExeData;
					if (NiagaraCompilationCopyImpl::BinaryToExecData(CompileTask.SourceScript.Get(), DDCData, ExeData))
					{
						CompileTask.ExeData = MakeShared<FNiagaraVMExecutableData>(MoveTemp(ExeData));
					}
				}

				if (!CompileTask.ExeData.IsValid())
				{
					CompileTask.DataCachePutKeys.Add(CompileTask.DataCacheGetKey);
				}
				else
				{
					CompileTask.bFromDerivedDataCache = true;
				}
			}

			if (PendingGetRequestCount.fetch_sub(1, std::memory_order_relaxed) == 1)
			{
				CompletionEvent.Trigger();
			}
		});
	}
	else
	{
		CompletionEvent.Trigger();
	}
}

void FNiagaraSystemCompilationTask::FDispatchDataCachePutRequests::Launch(FNiagaraSystemCompilationTask* SystemCompileTask)
{
	using namespace UE::DerivedData;

	TArray<FCachePutValueRequest> PutRequests;
	PutRequests.Reserve(SystemCompileTask->CompileTasks.Num());

	for (const FNiagaraSystemCompilationTask::FCompileTaskInfo& CompileTask : SystemCompileTask->CompileTasks)
	{
		if (CompileTask.ExeData.IsValid() && !CompileTask.DataCachePutKeys.IsEmpty())
		{
			if (FSharedBuffer SharedBuffer = NiagaraCompilationCopyImpl::ExecToBinaryData(CompileTask.SourceScript.Get(), *CompileTask.ExeData))
			{
				FValue PutValue = FValue::Compress(SharedBuffer);
				for (const FCacheKey& CachePutKey : CompileTask.DataCachePutKeys)
				{
					FCachePutValueRequest& PutRequest = PutRequests.AddDefaulted_GetRef();
					PutRequest.Name = CompileTask.AssetPath;
					PutRequest.Key = CachePutKey;
					PutRequest.Value = PutValue;
				}
			}
		}
	}

	PendingPutRequestCount = PutRequests.Num();

	if (!PutRequests.IsEmpty())
	{
		FRequestBarrier AsyncBarrier(SystemCompileTask->DDCRequestOwner);
		GetCache().PutValue(PutRequests, SystemCompileTask->DDCRequestOwner, [this](FCachePutValueResponse&& Response)
		{
			if (PendingPutRequestCount.fetch_sub(1, std::memory_order_relaxed) == 1)
			{
				CompletionEvent.Trigger();
			}
		});
	}
	else
	{
		CompletionEvent.Trigger();
	}
}

void FNiagaraSystemCompilationTask::DigestSystemInfo()
{
	using namespace NiagaraCompilationCopyImpl;

	FNiagaraDigestDatabase& DigestDatabase = FNiagaraDigestDatabase::Get();
	const UNiagaraScript* SystemSpawnScript = System_GT->GetSystemSpawnScript();
	const UNiagaraScript* SystemUpdateScript = System_GT->GetSystemUpdateScript();

	const UNiagaraGraph* SystemGraph = GetGraphFromScript(SystemSpawnScript);
	check(SystemGraph == GetGraphFromScript(SystemUpdateScript));

	FNiagaraGraphChangeIdBuilder ChangeIdBuilder;
	ChangeIdBuilder.ParseReferencedGraphs(SystemGraph);

	SystemInfo.SystemName = System_GT->GetName();
	SystemInfo.SystemPackageName = System_GT->GetOutermost()->GetFName();
	SystemInfo.bUseRapidIterationParams = System_GT->ShouldUseRapidIterationParameters();
	SystemInfo.bDisableDebugSwitches = System_GT->ShouldDisableDebugSwitches();
	SystemInfo.ConstantResolver = FNiagaraFixedConstantResolver(FCompileConstantResolver(System_GT.Get(), ENiagaraScriptUsage::SystemSpawnScript));
	SystemInfo.OwnedScriptKeys = { SystemSpawnScript, SystemUpdateScript };
	SystemInfo.SystemSourceGraph = DigestDatabase.CreateGraphDigest(SystemGraph, ChangeIdBuilder);
	
	{
		TArray<FNiagaraVariable> StaticVariablesFromEmitters;

		System_GT->GatherStaticVariables(SystemInfo.InitialStaticVariables, StaticVariablesFromEmitters);
		for (const FNiagaraVariable& EmitterVariable : StaticVariablesFromEmitters)
		{
			SystemInfo.InitialStaticVariables.AddUnique(EmitterVariable);
		}
	}

	const TArray<FNiagaraEmitterHandle>& EmitterHandles = System_GT->GetEmitterHandles();
	const int32 EmitterCount = EmitterHandles.Num();

	SystemInfo.EmitterInfo.Reserve(EmitterCount);
	for (const FNiagaraEmitterHandle& Handle : EmitterHandles)
	{
		const int32 EmitterIndex = SystemInfo.EmitterInfo.Num();

		FEmitterInfo& EmitterInfo = SystemInfo.EmitterInfo.AddDefaulted_GetRef();
		const FVersionedNiagaraEmitter& HandleInstance = Handle.GetInstance();

		const UNiagaraGraph* EmitterGraph = GetGraphFromScriptSource(Handle.GetEmitterData()->GraphSource);
		ChangeIdBuilder.ParseReferencedGraphs(EmitterGraph);

		EmitterInfo.UniqueEmitterName = HandleInstance.Emitter->GetUniqueEmitterName();
		EmitterInfo.UniqueInstanceName = Handle.GetUniqueInstanceName();
		EmitterInfo.Enabled = Handle.GetIsEnabled();
		EmitterInfo.ConstantResolver = FNiagaraFixedConstantResolver(FCompileConstantResolver(HandleInstance, ENiagaraScriptUsage::EmitterSpawnScript));
		EmitterInfo.EmitterIndex = EmitterIndex;
		EmitterInfo.SourceGraph = DigestDatabase.CreateGraphDigest(EmitterGraph, ChangeIdBuilder);

		{
			constexpr bool bCompilableOnly = false;
			constexpr bool bEnabledOnly = false;
			TArray<UNiagaraScript*> EmitterScripts;
			Handle.GetEmitterData()->GetScripts(EmitterScripts, bCompilableOnly, bEnabledOnly);
			EmitterInfo.OwnedScriptKeys.Reserve(EmitterScripts.Num());
			for (const UNiagaraScript* EmitterScript : EmitterScripts)
			{
				EmitterInfo.OwnedScriptKeys.Add(EmitterScript);
			}
		}

		if (FVersionedNiagaraEmitterData* EmitterData = Handle.GetEmitterData())
		{
			const TArray<UNiagaraSimulationStageBase*> SimStages = EmitterData->GetSimulationStages();
			EmitterInfo.SimStages.Reserve(SimStages.Num());
			for (const UNiagaraSimulationStageBase* SourceSimStage : SimStages)
			{
				if (SourceSimStage)
				{
					FNiagaraSimulationStageInfo& SimStageInfo = EmitterInfo.SimStages.AddDefaulted_GetRef();
					SimStageInfo.bEnabled = SourceSimStage->bEnabled;
					SimStageInfo.StageId = SourceSimStage->Script->GetUsageId();
					
					if (const UNiagaraSimulationStageGeneric* SourceGenericSimStage = Cast<const UNiagaraSimulationStageGeneric>(SourceSimStage))
					{
						SimStageInfo.bGenericStage = true;
						SimStageInfo.IterationSource = SourceGenericSimStage->IterationSource;
						if (SimStageInfo.IterationSource == ENiagaraIterationSource::DataInterface)
						{
							SimStageInfo.DataInterfaceBindingName = SourceGenericSimStage->DataInterface.BoundVariable.GetName();
						}
					}

					{
						TArray<FNiagaraSimulationStageCompilationData> SimStageCompilationData;

						SimStageInfo.bHasCompilationData = SourceSimStage->FillCompilationData(SimStageCompilationData);
						if (SimStageInfo.bHasCompilationData)
						{
							SimStageInfo.CompilationData = SimStageCompilationData[0];
						}
					}
				}
			}

			EmitterData->GatherStaticVariables(EmitterInfo.InitialStaticVariables);
		}
	}

	System_GT->GetExposedParameters().GetParameters(SystemInfo.OriginalExposedParams);
}

void FNiagaraSystemCompilationTask::DigestParameterCollections(TConstArrayView<TWeakObjectPtr<UNiagaraParameterCollection>> Collections)
{
	DigestedParameterCollections.Reserve(Collections.Num());
	for (TWeakObjectPtr<UNiagaraParameterCollection> Collection : Collections)
	{
		if (const UNiagaraParameterCollection* CollectionPtr = Collection.Get())
		{
			DigestedParameterCollections.Add(CollectionPtr, FNiagaraDigestDatabase::Get().CreateCompilationCopy(CollectionPtr));
		}
	}
}

void FNiagaraSystemCompilationTask::AddScript(int32 EmitterIndex, UNiagaraScript* Script, bool bRequiresCompilation)
{
	if (!Script)
	{
		return;
	}

	if (bRequiresCompilation)
	{
		FNiagaraVMExecutableDataId CompileId;
		Script->ComputeVMCompilationId(CompileId, FGuid());
		FVersionedNiagaraScriptData* ScriptData = Script->GetScriptData(CompileId.ScriptVersionID);
		if (ensure(ScriptData))
		{
			FCompileGroupInfo* GroupInfo = GetCompileGroupInfo(EmitterIndex);
			if (!GroupInfo)
			{
				GroupInfo = &CompileGroups.Emplace_GetRef(EmitterIndex);
			}

			TArray<ENiagaraScriptUsage> DuplicateUsages;
			NiagaraCompilationCopyImpl::GetUsagesToDuplicate(Script->GetUsage(), DuplicateUsages);
			for (ENiagaraScriptUsage DuplicateUsage : DuplicateUsages)
			{
				GroupInfo->ValidUsages.AddUnique(DuplicateUsage);
			}

			const int32 CompileTaskIndex = CompileTasks.AddDefaulted();
			GroupInfo->CompileTaskIndices.Add(CompileTaskIndex);

			FCompileTaskInfo& TaskInfo = CompileTasks[CompileTaskIndex];
			TaskInfo.CompileOptions = FNiagaraCompileOptions(
				Script->GetUsage(),
				Script->GetUsageId(),
				ScriptData->ModuleUsageBitmask,
				Script->GetPathName(),
				Script->GetFullName(),
				Script->GetName());

			TaskInfo.CompileOptions.AdditionalDefines = CompileId.AdditionalDefines;
			TaskInfo.CompileOptions.AdditionalVariables = CompileId.AdditionalVariables;
			TaskInfo.SourceScript = Script;
			TaskInfo.ScriptKey = Script;
			TaskInfo.AssetPath = Script->GetPathName();
			TaskInfo.CompileId = MoveTemp(CompileId);
			TaskInfo.TaskStartTime = FPlatformTime::Seconds();
		}
	}

	FVersionedNiagaraEmitterData* EmitterData = nullptr;

	if (EmitterIndex != INDEX_NONE)
	{
		EmitterData = System_GT->GetEmitterHandle(EmitterIndex).GetEmitterData();
	}

	FScriptInfo& Info = DigestedScriptInfo.Add(Script);
	Script->RapidIterationParameters.CopyParametersTo(Info.RapidIterationParameters, false, FNiagaraParameterStore::EDataInterfaceCopyMethod::None);
	Info.RapidIterationParameters.ParameterGuidMapping = Script->RapidIterationParameters.ParameterGuidMapping;

	{
		const FNiagaraTypeDefinition& PositionTypeDef = FNiagaraTypeDefinition::GetPositionDef();
		const FNiagaraTypeDefinition& Vec3TypeDef = FNiagaraTypeDefinition::GetVec3Def();

		// sanitize the guid mapping to account for parameters that might have been auto upgraded from Vec3 to Position
		// for LWC.  The ParameterStore will be updated, but the guid mapping may not have been
		for (const FNiagaraVariableWithOffset& ParamStoreVariable : Info.RapidIterationParameters.ReadParameterVariables())
		{
			if (ParamStoreVariable.GetType() == PositionTypeDef)
			{
				if (!Info.RapidIterationParameters.ParameterGuidMapping.Contains(ParamStoreVariable))
				{
					FNiagaraVariable VariableAsVector(Vec3TypeDef, ParamStoreVariable.GetName());

					FGuid ExistingGuid;
					if (Info.RapidIterationParameters.ParameterGuidMapping.RemoveAndCopyValue(ParamStoreVariable, ExistingGuid))
					{
						Info.RapidIterationParameters.ParameterGuidMapping.Add(ParamStoreVariable, ExistingGuid);
					}
				}
			}
		}
	}

	Info.Usage = Script->GetUsage();
	Info.UsageId = Script->GetUsageId();
	Info.EmitterIndex = EmitterIndex;
	for (UNiagaraScript* DependentScript : NiagaraCompilationCopyImpl::FindDependentScripts(System_GT.Get(), EmitterData, Script))
	{
		Info.DependentScripts.AddUnique(DependentScript);
	}
}

void FNiagaraSystemCompilationTask::Tick()
{
	switch (CompilationState)
	{
		case EState::WaitingForProcessing:
		{
			// check to see if the task's compiler has completed
			for (FCompileTaskInfo& CompileTask : CompileTasks)
			{
				const bool HasPendingCompilationTask = CompileTask.CompileResultsReadyEvent.IsValid()
					&& CompileTask.CompileResultsReadyEvent->IsValid()
					&& !CompileTask.CompileResultsReadyEvent->IsCompleted();

				if (HasPendingCompilationTask)
				{
					CompileTask.RetrieveCompilationResult(false);
				}
			}
		} break;

		case EState::Completed:
		{
			if (bAborting)
			{
				CompilationState = EState::Aborted;
			}
		} break;
	}
}

bool FNiagaraSystemCompilationTask::Poll(FNiagaraSystemAsyncCompileResults& Results) const
{
	// ignore actually collecting the data if we're in the middle of aborting and just let the caller know
	// that there's no tasks waiting for them
	if (bAborting)
	{
		return true;
	}

	switch (CompilationState)
	{
		case EState::ResultsProcessed:
		case EState::Completed:
		{
			for (const FCompileTaskInfo& TaskInfo : CompileTasks)
			{
				if (UNiagaraScript* SourceScript = TaskInfo.SourceScript.Get())
				{
					FNiagaraScriptAsyncCompileData& CompileData = Results.CompileResultMap.Add(SourceScript);

					CompileData.bFromDerivedDataCache = TaskInfo.bFromDerivedDataCache;
					CompileData.CompileId = TaskInfo.CompileId;
					CompileData.ExeData = TaskInfo.ExeData;
					CompileData.NamedDataInterfaces.Reserve(TaskInfo.NamedDataInterfaces.Num());
					Algo::Transform(TaskInfo.NamedDataInterfaces, CompileData.NamedDataInterfaces, [](const TTuple<FName, UNiagaraDataInterface*>& InElement)
					{
						return MakeTuple(InElement.Key, InElement.Value);
					});

					CompileData.CompileMetrics.TaskWallTime = (float) (FPlatformTime::Seconds() - TaskInfo.TaskStartTime);
					CompileData.CompileMetrics.DDCFetchTime = 0.0f;
					CompileData.CompileMetrics.CompilerWallTime = TaskInfo.CompilerWallTime;
					CompileData.CompileMetrics.CompilerWorkerTime = TaskInfo.CompilerWorkerTime;
					CompileData.CompileMetrics.CompilerPreprocessTime = TaskInfo.CompilerPreprocessTime;
					CompileData.CompileMetrics.TranslateTime = TaskInfo.TranslationTime;
					CompileData.CompileMetrics.ByteCodeOptimizeTime = TaskInfo.ByteCodeOptimizeTime;

					if (const FScriptInfo* ScriptInfo = DigestedScriptInfo.Find(SourceScript))
					{
						if (SystemInfo.EmitterInfo.IsValidIndex(ScriptInfo->EmitterIndex))
						{
							CompileData.UniqueEmitterName = SystemInfo.EmitterInfo[ScriptInfo->EmitterIndex].UniqueEmitterName;
						}

						// we also need to incorporate the rapid iteration parameters that we encountered
						TConstArrayView<FNiagaraVariableWithOffset> RapidIterationParameters = ScriptInfo->RapidIterationParameters.ReadParameterVariables();

						CompileData.RapidIterationParameters.Reserve(RapidIterationParameters.Num());
						for (const FNiagaraVariableWithOffset& ParamWithOffset : RapidIterationParameters)
						{
							FNiagaraVariable& Parameter = CompileData.RapidIterationParameters.Add_GetRef(ParamWithOffset);
							Parameter.SetData(ScriptInfo->RapidIterationParameters.GetParameterData(ParamWithOffset.Offset));
						}
					}
				}
			}

			Results.bForced = bForced;
			Results.ExposedVariables = SystemExposedVariables.Array();

			return true;
		} break;
	}

	return false;
}

bool FNiagaraSystemCompilationTask::CanRemove() const
{
	return (CompilationState == EState::Completed && ResultsRetrieved)
		|| (CompilationState == EState::Aborted);
}

void FNiagaraSystemCompilationTask::WaitTillCompileCompletion()
{
	const FTimespan WaitTimeout = FTimespan::FromMilliseconds(50.0);
	while (!CompileCompletionEvent.IsCompleted())
	{
		// if the busy wait doesn't complete the task then we need to make sure to poke the
		// compilation manager since there we may be waiting on it
		if (!CompileCompletionEvent.BusyWait(WaitTimeout))
		{
			FAssetCompilingManager::Get().ProcessAsyncTasks();
		}
	}
}

void FNiagaraSystemCompilationTask::WaitTillCachePutCompletion()
{
	if (PutRequestHelper.IsValid())
	{
		PutRequestHelper->CompletionEvent.Wait();
	}
}

void FNiagaraSystemCompilationTask::Precompile()
{
	SystemPrecompileData = MakeShared<FNiagaraPrecompileData, ESPMode::ThreadSafe>();

	SystemPrecompileData->SharedCompileDataInterfaceData = MakeShared<TArray<FNiagaraPrecompileData::FCompileDataInterfaceData>>();
	TArray<TSharedPtr<FNiagaraPrecompileData, ESPMode::ThreadSafe>> DependentRequests;
	FCompileConstantResolver EmptyResolver;

	SystemPrecompileData->SourceName = SystemInfo.SystemName;

	SystemPrecompileData->DigestedSourceGraph = SystemInfo.SystemSourceGraph;
	SystemPrecompileData->bUseRapidIterationParams = SystemInfo.bUseRapidIterationParams;
	SystemPrecompileData->bDisableDebugSwitches = SystemInfo.bDisableDebugSwitches;

	TArray<FString> EmitterNames;

	// Create an array of variables that we might encounter when traversing the graphs (include the originally exposed vars above)
	TArray<FNiagaraVariable> EncounterableVars = SystemInfo.OriginalExposedParams;

	const int32 EmitterCount = SystemInfo.EmitterInfo.Num();
	SystemPrecompileData->EmitterData.Empty(EmitterCount);

	for (const FEmitterInfo& EmitterInfo : SystemInfo.EmitterInfo)
	{
		TSharedPtr<FNiagaraPrecompileData, ESPMode::ThreadSafe> EmitterPtr = MakeShared<FNiagaraPrecompileData, ESPMode::ThreadSafe>();
		EmitterPtr->EmitterUniqueName = EmitterInfo.UniqueEmitterName;
		EmitterPtr->SourceName = SystemPrecompileData->SourceName;
		EmitterPtr->DigestedSourceGraph = EmitterInfo.SourceGraph;
		EmitterPtr->bUseRapidIterationParams = SystemPrecompileData->bUseRapidIterationParams;
		EmitterPtr->bDisableDebugSwitches = SystemPrecompileData->bDisableDebugSwitches;
		EmitterPtr->SharedCompileDataInterfaceData = SystemPrecompileData->SharedCompileDataInterfaceData;
		SystemPrecompileData->EmitterData.Add(EmitterPtr);
		EmitterNames.Add(EmitterInfo.UniqueInstanceName);
	}

	// Now deep copy the system graphs, skipping traversal into any emitter references.
	{
		SystemPrecompileData->FinishPrecompile(
			*this,
			EncounterableVars,
			SystemInfo.StaticVariableResults,
			SystemInfo.ConstantResolver,
			{ ENiagaraScriptUsage::SystemSpawnScript, ENiagaraScriptUsage::SystemUpdateScript },
			{},
			EmitterNames);

		SystemPrecompileData->CollectBakedRapidIterationParameters(*this, SystemInfo.OwnedScriptKeys);
	}

	// Add the User and System variables that we did encounter to the list that emitters might also encounter.
	TArray<FNiagaraVariable> SystemEncounteredUserVariables;
	SystemPrecompileData->GatherPreCompiledVariables(FNiagaraConstants::UserNamespaceString, SystemEncounteredUserVariables);
	NiagaraCompilationCopyImpl::AppendUnique(SystemEncounteredUserVariables, EncounterableVars);
	SystemExposedVariables.Append(SystemEncounteredUserVariables);

	SystemPrecompileData->GatherPreCompiledVariables(FNiagaraConstants::SystemNamespaceString, EncounterableVars);

	// Now we can finish off the emitters.
	for (int32 EmitterIt = 0; EmitterIt < EmitterCount; ++EmitterIt)
	{
		const FEmitterInfo& EmitterInfo = SystemInfo.EmitterInfo[EmitterIt];

		if (EmitterInfo.Enabled) // Don't pull in the emitter if it isn't going to be used.
		{
			TArray<FNiagaraVariable> StaticVariablesFromEmitter = SystemInfo.StaticVariableResults;
			StaticVariablesFromEmitter.Append(EmitterInfo.StaticVariableResults);

			FNiagaraPrecompileData* EmitterPrecompileData = SystemPrecompileData->EmitterData[EmitterIt].Get();

			EmitterPrecompileData->FinishPrecompile(
				*this,
				EncounterableVars,
				StaticVariablesFromEmitter,
				EmitterInfo.ConstantResolver,
				{ ENiagaraScriptUsage::EmitterSpawnScript, ENiagaraScriptUsage::EmitterUpdateScript },
				{},
				EmitterNames);

			// Then finish the precompile for the particle scripts once we've gathered the emitter vars which might be referenced.
			TArray<FNiagaraVariable> ParticleEncounterableVars = EncounterableVars;
			EmitterPrecompileData->GatherPreCompiledVariables(FNiagaraConstants::EmitterNamespaceString, ParticleEncounterableVars);

			TArray<FNiagaraVariable> EmitterEncounteredUserVariables;
			EmitterPrecompileData->GatherPreCompiledVariables(FNiagaraConstants::UserNamespaceString, EmitterEncounteredUserVariables);
			SystemExposedVariables.Append(EmitterEncounteredUserVariables);

			TArray<FNiagaraVariable> OldStaticVars = EmitterPrecompileData->StaticVariables;
			EmitterPrecompileData->FinishPrecompile(
				*this,
				ParticleEncounterableVars,
				OldStaticVars,
				EmitterInfo.ConstantResolver,
				{
					ENiagaraScriptUsage::ParticleSpawnScript,
					ENiagaraScriptUsage::ParticleSpawnScriptInterpolated,
					ENiagaraScriptUsage::ParticleUpdateScript,
					ENiagaraScriptUsage::ParticleEventScript,
					ENiagaraScriptUsage::ParticleGPUComputeScript,
					ENiagaraScriptUsage::ParticleSimulationStageScript 
				},
				EmitterInfo.SimStages,
				EmitterNames);

			EmitterPrecompileData->CollectBakedRapidIterationParameters(*this, EmitterInfo.OwnedScriptKeys);
		}
	}
}

void FNiagaraSystemCompilationTask::GetAvailableCollections(TArray<FNiagaraCompilationNPCHandle>& OutCollections) const
{
	DigestedParameterCollections.GenerateValueArray(OutCollections);
}

bool FNiagaraSystemCompilationTask::GetStageName(int32 EmitterIndex, const FNiagaraCompilationNodeOutput* OutputNode, FName& OutStageName) const
{
	OutStageName = NAME_None;

	if (EmitterIndex == INDEX_NONE)
	{
		return true;
	}

	bool bStageEnabled = true;
	if (OutputNode->Usage == ENiagaraScriptUsage::ParticleSimulationStageScript)
	{
		const FNiagaraSimulationStageInfo* SimStageInfo = SystemInfo.EmitterInfo[EmitterIndex].SimStages.FindByPredicate([OutputNode](const FNiagaraSimulationStageInfo& SimStageInfo)
		{
			return SimStageInfo.StageId == OutputNode->UsageId;
		});

		bStageEnabled = SimStageInfo && SimStageInfo->bEnabled;
		if (bStageEnabled && SimStageInfo->bGenericStage)
		{
			OutStageName = SimStageInfo->DataInterfaceBindingName;
		}
	}

	return bStageEnabled;		
}

struct FNiagaraSystemCompilationTask::FCollectStaticVariablesTaskBuilder
{
	FCollectStaticVariablesTaskBuilder(FNiagaraSystemCompilationTask& CompilationTask)
	: FoundStaticVariables(CompilationTask.SystemInfo.StaticVariableResults)
	{
		if (FNiagaraCompilationGraph* SystemGraph = CompilationTask.SystemInfo.SystemSourceGraph.Get())
		{
			FStaticVariableBuilderTaskHandle BuilderTask = MakeShared<FStaticVariableBuilderTask, ESPMode::ThreadSafe>();
			BuilderTask->CompilationTask = &CompilationTask;
			BuilderTask->ConstantResolver = CompilationTask.SystemInfo.ConstantResolver;
			BuilderTask->GraphContext = SystemGraph;
			BuilderTask->InitialStaticVariables = CompilationTask.SystemInfo.InitialStaticVariables;
			SystemGraph->FindOutputNodes(BuilderTask->OutputNodes);
			BuilderTask->StageNames.Init(NAME_None, BuilderTask->OutputNodes.Num());

			StaticVariableTasksToProcess.Add(BuilderTask);
		}
	}

	FCollectStaticVariablesTaskBuilder(const FNiagaraSystemCompilationTask& CompilationTask, FEmitterInfo& EmitterInfo)
	: FoundStaticVariables(EmitterInfo.StaticVariableResults)
	{
		if (FNiagaraCompilationGraph* EmitterGraph = EmitterInfo.SourceGraph.Get())
		{
			TArray<const FNiagaraCompilationNodeOutput*> OutputNodes;
			EmitterGraph->FindOutputNodes(OutputNodes);

			// collect sim stage info for each of the output nodes (if they are even sim stages)
			TArray<TTuple<bool, FName>> OutputNodeStageInfo;
			OutputNodeStageInfo.Reserve(OutputNodes.Num());
			int32 EnabledOutputNodeCount = 0;
			Algo::Transform(OutputNodes, OutputNodeStageInfo, [&EmitterInfo, &EnabledOutputNodeCount](const FNiagaraCompilationNodeOutput* OutputNode) -> TTuple<bool, FName>
			{
				bool bEnabled = true;
				FName StageName = NAME_None;

				if (OutputNode->Usage == ENiagaraScriptUsage::ParticleSimulationStageScript)
				{
					const FNiagaraSimulationStageInfo* SimStage = EmitterInfo.SimStages.FindByPredicate([OutputNode](const FNiagaraSimulationStageInfo& StageInfo) -> bool
					{
						return StageInfo.StageId == OutputNode->UsageId;
					});

					if (SimStage)
					{
						bEnabled = SimStage->bEnabled;
						StageName = SimStage->DataInterfaceBindingName;
					}
				}

				if (bEnabled)
				{
					++EnabledOutputNodeCount;
				}

				return MakeTuple(bEnabled, StageName);
			});

			if (EnabledOutputNodeCount)
			{
				FStaticVariableBuilderTaskHandle BuilderTask = MakeShared<FStaticVariableBuilderTask, ESPMode::ThreadSafe>();
				BuilderTask->CompilationTask = &CompilationTask;
				BuilderTask->ConstantResolver = EmitterInfo.ConstantResolver;
				BuilderTask->GraphContext = EmitterGraph;

				// the initial static variables needs to come from gathering data from the Emitter
				// but also what has been calculated in the SystemInfo
				BuilderTask->InitialStaticVariables = CompilationTask.SystemInfo.StaticVariableResults;
				for (const FNiagaraVariable& EmitterGatheredVariable : EmitterInfo.InitialStaticVariables)
				{
					BuilderTask->InitialStaticVariables.AddUnique(EmitterGatheredVariable);
				}

				// Only use the static variables that match up with our expectations for this script. IE for emitters, filter things out for resolution.
				{ // FNiagaraParameterUtilities::FilterToRelevantStaticVariables
					FNiagaraAliasContext RenameContext(ENiagaraScriptUsage::ParticleSpawnScript);
					RenameContext.ChangeEmitterName(EmitterInfo.UniqueEmitterName, FNiagaraConstants::EmitterNamespaceString);

					for (FNiagaraVariable& StaticVariable : BuilderTask->InitialStaticVariables)
					{
						const FNiagaraVariable ResolvedVariable = FNiagaraUtilities::ResolveAliases(StaticVariable, RenameContext);
						StaticVariable.SetName(ResolvedVariable.GetName());
					}
				}

				//BuilderTask->InitialStaticVariables = EmitterInfo.InitialStaticVariables;
				BuilderTask->OutputNodes.Reserve(EnabledOutputNodeCount);
				BuilderTask->StageNames.Reserve(EnabledOutputNodeCount);

				for (int32 OutputNodeIt = 0; OutputNodeIt < OutputNodes.Num(); ++OutputNodeIt)
				{
					if (OutputNodeStageInfo[OutputNodeIt].Key)
					{
						BuilderTask->OutputNodes.Add(OutputNodes[OutputNodeIt]);
						BuilderTask->StageNames.Add(OutputNodeStageInfo[OutputNodeIt].Value);
					}
				}

				StaticVariableTasksToProcess.Add(BuilderTask);
			}
		}
	}

	struct FStaticVariableBuilderTask
	{
		const FNiagaraSystemCompilationTask* CompilationTask = nullptr;
		FNiagaraFixedConstantResolver ConstantResolver;
		const FNiagaraCompilationGraph* GraphContext = nullptr;
		TArray<const FNiagaraCompilationNodeOutput*> OutputNodes;
		TArray<FNiagaraVariable> InitialStaticVariables;
		TArray<FName> StageNames;

		TArray<FNiagaraVariable> FoundStaticVariables;
	};

	using FStaticVariableBuilderTaskHandle = TSharedPtr<FStaticVariableBuilderTask, ESPMode::ThreadSafe>;

	UE::Tasks::FTask LaunchCollectedTasks()
	{
		using namespace UE::Tasks;

		if (!StaticVariableTasksToProcess.IsEmpty())
		{
			// fire off the tasks related to collecting static variables
			const int32 TaskCount = StaticVariableTasksToProcess.Num();
			TArray<FTask> PendingTasks;
			PendingTasks.Reserve(TaskCount);

			for (FStaticVariableBuilderTaskHandle& BuilderTask : StaticVariableTasksToProcess)
			{
				PendingTasks.Add(Launch(UE_SOURCE_LOCATION, [BuilderTask]
				{
					TArray<FNiagaraVariable> ActiveStaticVariables = BuilderTask->InitialStaticVariables;

					const int32 OutputNodeCount = BuilderTask->OutputNodes.Num();
					for (int32 OutputNodeIt = 0; OutputNodeIt < OutputNodeCount; ++OutputNodeIt)
					{
						const FNiagaraCompilationNodeOutput* OutputNode = BuilderTask->OutputNodes[OutputNodeIt];

						TNiagaraParameterMapHistoryWithMetaDataBuilder<FNiagaraCompilationDigestBridge> Builder;
						*Builder.ConstantResolver = BuilderTask->ConstantResolver;
						Builder.AddGraphToCallingGraphContextStack(BuilderTask->GraphContext);
						Builder.RegisterExternalStaticVariables(ActiveStaticVariables);
						BuilderTask->CompilationTask->GetAvailableCollections(Builder.AvailableCollections->EditCollections());
						Builder.BeginTranslation(FNiagaraConstants::EmitterNamespaceString);
						Builder.EnableScriptAllowList(true, OutputNode->Usage);
						Builder.IncludeStaticVariablesOnly();
						Builder.BeginUsage(OutputNode->Usage, BuilderTask->StageNames[OutputNodeIt]);
						Builder.BuildParameterMaps(OutputNode, true);
						Builder.EndUsage();

						for (const FNiagaraVariable& FoundVariable : Builder.StaticVariables)
						{
							ActiveStaticVariables.AddUnique(FoundVariable);
						}
					}

					BuilderTask->FoundStaticVariables = MoveTemp(ActiveStaticVariables);
				}));
			}

			TArray<FStaticVariableBuilderTaskHandle> MergeTaskInput = StaticVariableTasksToProcess;
			TArray<FNiagaraVariable>& MergeTaskOutput = FoundStaticVariables;
			return Launch(UE_SOURCE_LOCATION, [MergeTaskInput, &MergeTaskOutput]
			{
				if (MergeTaskInput.Num() == 1)
				{
					MergeTaskOutput = MergeTaskInput[0]->FoundStaticVariables;
				}
				else
				{
					for (const FStaticVariableBuilderTaskHandle& BuilderTask : MergeTaskInput)
					{
						for (const FNiagaraVariable& BuilderTaskVariable : BuilderTask->FoundStaticVariables)
						{
							MergeTaskOutput.AddUnique(BuilderTaskVariable);
						}
					}
				}
			}, PendingTasks);
		}

		return FTask();
	}

	TArray<FStaticVariableBuilderTaskHandle> StaticVariableTasksToProcess;
	TArray<FNiagaraVariable>& FoundStaticVariables;
};

struct FNiagaraSystemCompilationTask::FBuildRapidIterationTaskBuilder
{
	FBuildRapidIterationTaskBuilder(FNiagaraSystemCompilationTask& CompilationTask, FScriptInfo& ScriptInfo)
		: ParameterStore(ScriptInfo.RapidIterationParameters)
	{
		const FNiagaraCompilationGraph* Graph;
		FString UniqueEmitterName;
		FNiagaraFixedConstantResolver ConstantResolver;
	
		if (ScriptInfo.EmitterIndex == INDEX_NONE)
		{
			Graph = CompilationTask.SystemInfo.SystemSourceGraph.Get();
			FoundStaticVariables = &CompilationTask.SystemInfo.StaticVariableResults;
			ConstantResolver = CompilationTask.SystemInfo.ConstantResolver;
		}
		else
		{
			FEmitterInfo& EmitterInfo = CompilationTask.SystemInfo.EmitterInfo[ScriptInfo.EmitterIndex];

			Graph = EmitterInfo.SourceGraph.Get();
			FoundStaticVariables = &EmitterInfo.StaticVariableResults;
			UniqueEmitterName = EmitterInfo.UniqueEmitterName;
			ConstantResolver = EmitterInfo.ConstantResolver;
		}

		if (Graph)
		{
			TArray<const FNiagaraCompilationNodeOutput*> OutputNodes;
			if (ScriptInfo.Usage == ENiagaraScriptUsage::ParticleGPUComputeScript)
			{
				Graph->FindOutputNodes(OutputNodes);
				OutputNodes.SetNum(Algo::RemoveIf(OutputNodes, [](const FNiagaraCompilationNodeOutput* OutputNode) -> bool
				{
					return !UNiagaraScript::IsParticleScript(OutputNode->Usage);
				}));
			}
			else
			{
				if (const FNiagaraCompilationNodeOutput* OutputNode = Graph->FindEquivalentOutputNode(ScriptInfo.Usage, ScriptInfo.UsageId))
				{
					OutputNodes.Add(OutputNode);
				}
			}

			for (const FNiagaraCompilationNodeOutput* OutputNode : OutputNodes)
			{
				TArray<const FNiagaraCompilationNode*> TraversalNodes;
				Graph->BuildTraversal(OutputNode, TraversalNodes);
				for (const FNiagaraCompilationNode* TraversalNode : TraversalNodes)
				{
					if (const FNiagaraCompilationNodeFunctionCall* FunctionCallNode = TraversalNode->AsType<FNiagaraCompilationNodeFunctionCall>())
					{
						if (FunctionCallNode->CalledGraph.IsValid())
						{
							FRapidIterationBuilderTaskHandle BuilderTask = MakeShared<FRapidIterationBuilderTask, ESPMode::ThreadSafe>();
							BuilderTask->CompilationTask = &CompilationTask;
							BuilderTask->ConstantResolver = ConstantResolver;
							BuilderTask->FunctionCallNode = FunctionCallNode;
							BuilderTask->UniqueEmitterName = UniqueEmitterName;
							BuilderTask->ScriptUsage = ScriptInfo.Usage;

							RapidIterationTasksToProcess.Add(BuilderTask);
						}
					}
				}
			}
		}
	}

	struct FRapidIterationBuilderTask
	{
		const FNiagaraSystemCompilationTask* CompilationTask = nullptr;
		FNiagaraFixedConstantResolver ConstantResolver;
		const FNiagaraCompilationNodeFunctionCall* FunctionCallNode = nullptr;
		TArray<FNiagaraVariable> ModuleInputVariables;
		FString UniqueEmitterName;
		ENiagaraScriptUsage ScriptUsage;
	};

	using FRapidIterationBuilderTaskHandle = TSharedPtr<FRapidIterationBuilderTask, ESPMode::ThreadSafe>;

	UE::Tasks::FTask LaunchCollectedTasks()
	{
		using namespace UE::Tasks;

		const int32 TaskCount = RapidIterationTasksToProcess.Num();
		TArray<FTask> PendingTasks;
		PendingTasks.Reserve(TaskCount);

		const TArray<FNiagaraVariable>* OwnerStaticVariables = FoundStaticVariables;

		for (FRapidIterationBuilderTaskHandle& BuilderTask : RapidIterationTasksToProcess)
		{
			PendingTasks.Add(Launch(UE_SOURCE_LOCATION, [BuilderTask, OwnerStaticVariables]
			{
				constexpr bool bIgnoreDisabled = false;
				constexpr bool bFilterForCompilation = false;

				TNiagaraParameterMapHistoryBuilder<FNiagaraCompilationDigestBridge> Builder;
				Builder.SetIgnoreDisabled(bIgnoreDisabled);
				*Builder.ConstantResolver = BuilderTask->ConstantResolver;
				Builder.RegisterExternalStaticVariables(*OwnerStaticVariables);
				BuilderTask->CompilationTask->GetAvailableCollections(Builder.AvailableCollections->EditCollections());

				/* The line below represents a compromise. It basically says don't look up the static variable from the
				static variables list set in RegisterExternalStaticVariables as a rapid iteration parameter. Why? Rapid Iteration Parameters
				may be stale and linger around from prior changes that have been abandoned. We keep them around so that when you toggle back
				in the UI, you haven't also lost your last set values for that version. So therefore, we can't officially remove them.

				The issue comes in when a static variable is set via some other means, linking to another variable for example. When generating
				the UI, it *should* show anything that would match up to the linked variable, but because this is a parameter map history
				traversal that is in isolation on the node and doesn't include upstream set nodes, it will fail and just look up the rapid iteration
				version if left to its' own devices.

				Trial 1 at a fix was to cache all the values at the root of the graph and look them up when we build the static variables list
				we use above. That isn't good because that version skips all disabled modules and we want the UI to be consistent regardless
				of disabled module status.

				Trial 2 was to back up and include the precursor gets/sets. This fails for the same reason, ultimately something will
				be bound to a variable that will assume that it needs to look up a bogus rapid iteration value and we end up back here.

				Trial 3 just says, we know the static variables list above includes basically all relevant intermediate static values. So if
				we just circumvent the logic in the parameter map history traversal that looks up by rapid iteration parameter, it will
				find the right "cached" value. So we just do that below.
					*/
				Builder.SetIgnoreStaticRapidIterationParameters(true);

				// if we are only dealing with the module input pins then we don't need to delve deep into the graph
				Builder.MaxGraphDepthTraversal = 1;

				BuilderTask->FunctionCallNode->BuildParameterMapHistory(Builder, false, bFilterForCompilation);

				if (Builder.Histories.Num() == 1)
				{
					BuilderTask->ModuleInputVariables = NiagaraCompilationCopyImpl::ExtractInputVariablesFromHistory(
						Builder.Histories[0], BuilderTask->FunctionCallNode->CalledGraph.Get());

					BuilderTask->FunctionCallNode->MultiFindParameterMapDefaultValues(BuilderTask->ScriptUsage, BuilderTask->ConstantResolver, BuilderTask->ModuleInputVariables);
				}
			}));
		}

		TArray<FRapidIterationBuilderTaskHandle> MergeTaskInput = RapidIterationTasksToProcess;
		FNiagaraParameterStore& MergeTaskOutput = ParameterStore;
		return Launch(UE_SOURCE_LOCATION, [MergeTaskInput, &MergeTaskOutput]
		{
			TSet<FName> ValidParameterNames;

			// we need to build a reverse mapping of the ParameterGuidMapping so that we can quickly find renamed parameters
			TMultiMap<FGuid, FNiagaraVariable> GuidMapping;
			for (const FNiagaraVariableBase& RapidIterationParameter : MergeTaskOutput.ReadParameterVariables())
			{
				if (FGuid* ParameterGuid = MergeTaskOutput.ParameterGuidMapping.Find(RapidIterationParameter))
				{
					GuidMapping.Add(*ParameterGuid, RapidIterationParameter);
				}
			}

			for (FRapidIterationBuilderTaskHandle InputTask : MergeTaskInput)
			{
				for (const FNiagaraVariable& ModuleInputVariable : InputTask->ModuleInputVariables)
				{
					if (FNiagaraStackGraphUtilities::IsRapidIterationType(ModuleInputVariable.GetType()))
					{
						const FName FunctionName = FName(InputTask->FunctionCallNode->FunctionName);

						FNiagaraParameterHandle AliasedFunctionInputHandle = FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(ModuleInputVariable.GetName(), FunctionName);
						const FName AliasedName = AliasedFunctionInputHandle.GetParameterHandleString();

						FNiagaraVariable RapidIterationParameter(ModuleInputVariable);
						const TCHAR* EmitterName = InputTask->UniqueEmitterName.IsEmpty() ? nullptr	: *InputTask->UniqueEmitterName;
						const FString RapidIterationConstantName = FNiagaraUtilities::CreateRapidIterationConstantName(AliasedName, EmitterName, InputTask->ScriptUsage);

						RapidIterationParameter.SetName(*RapidIterationConstantName);

						ValidParameterNames.Add(RapidIterationParameter.GetName());

						NiagaraCompilationCopyImpl::FixupRenamedParameter(
							ModuleInputVariable,
							RapidIterationParameter,
							InputTask->FunctionCallNode,
							GuidMapping,
							MergeTaskOutput);

						NiagaraCompilationCopyImpl::AddRapidIterationParameter(
							AliasedFunctionInputHandle,
							ModuleInputVariable,
							RapidIterationParameter,
							InputTask->FunctionCallNode,
							MergeTaskOutput);
					}
				}
			}

			// go through and remove any entries in the ParameterStore that weren't found
			TArray<FNiagaraVariableBase> ParametersToRemove;
			for (const FNiagaraVariableWithOffset& Parameter : MergeTaskOutput.ReadParameterVariables())
			{
				if (!ValidParameterNames.Contains(Parameter.GetName()))
				{
					ParametersToRemove.Add(Parameter);
				}
			}

			for (const FNiagaraVariableBase& ParameterToRemove : ParametersToRemove)
			{
				MergeTaskOutput.RemoveParameter(ParameterToRemove);
			}
		}, PendingTasks);
	}

	TArray<FRapidIterationBuilderTaskHandle> RapidIterationTasksToProcess;
	const TArray<FNiagaraVariable>* FoundStaticVariables = nullptr;
	FNiagaraParameterStore& ParameterStore;
};

UE::Tasks::FTask FNiagaraSystemCompilationTask::BuildRapidIterationParametersAsync()
{
	using namespace UE::Tasks;

	TMap<int32 /*EmitterIndex*/, FTask> CollectStaticVariableTasks;
	
	FCollectStaticVariablesTaskBuilder SystemTaskBuilder(*this);
	FTask SystemCollectStaticVariableTask = SystemTaskBuilder.LaunchCollectedTasks();
	CollectStaticVariableTasks.Add(INDEX_NONE, SystemCollectStaticVariableTask);

	if (SystemCollectStaticVariableTask.IsValid())
	{
		const int32 EmitterCount = SystemInfo.EmitterInfo.Num();
		for (int32 EmitterIndex = 0; EmitterIndex < EmitterCount; ++EmitterIndex)
		{
			FEmitterInfo& EmitterInfo = SystemInfo.EmitterInfo[EmitterIndex];
			if (!EmitterInfo.Enabled)
			{
				continue;
			}
			FTask EmitterCollectStaticVariableTask = Launch(UE_SOURCE_LOCATION, [this, &EmitterInfo]
			{
				FCollectStaticVariablesTaskBuilder EmitterTaskBuilder(*this, EmitterInfo);
				FTask EmitterStaticVariableTask = EmitterTaskBuilder.LaunchCollectedTasks();
			}, SystemCollectStaticVariableTask);

			CollectStaticVariableTasks.Add(EmitterIndex, EmitterCollectStaticVariableTask);
		}

		TArray<FTask> PendingTasks;
		for (TMap<TObjectKey<UNiagaraScript>, FScriptInfo>::ElementType& CurrentIt : DigestedScriptInfo)
		{
			FScriptInfo& ScriptInfo = CurrentIt.Value;

			PendingTasks.Add(Launch(UE_SOURCE_LOCATION, [this, &ScriptInfo]
			{
				FBuildRapidIterationTaskBuilder TaskBuilder(*this, ScriptInfo);
				AddNested(TaskBuilder.LaunchCollectedTasks());
			}, CollectStaticVariableTasks.FindRef(ScriptInfo.EmitterIndex)));
		}

		// lastly we need a single task to copy over parameters between dependent scripts
		TMap<TObjectKey<UNiagaraScript>, FScriptInfo>* ScriptInfoMapPtr = &DigestedScriptInfo;
		return Launch(UE_SOURCE_LOCATION, [ScriptInfoMapPtr]
		{
			for (TMap<TObjectKey<UNiagaraScript>, FScriptInfo>::ElementType& CurrentIt : *ScriptInfoMapPtr)
			{
				FScriptInfo& SrcScriptInfo = CurrentIt.Value;
				for (TObjectKey<UNiagaraScript>& DependentScriptKey : SrcScriptInfo.DependentScripts)
				{
					FScriptInfo& DstScriptInfo = ScriptInfoMapPtr->FindChecked(DependentScriptKey);
					SrcScriptInfo.RapidIterationParameters.CopyParametersTo(DstScriptInfo.RapidIterationParameters, false, FNiagaraParameterStore::EDataInterfaceCopyMethod::None);
				}
			}
		}, PendingTasks);
	}

	return FTask();
}

void FNiagaraSystemCompilationTask::IssueCompilationTasks()
{
	using namespace UE::Tasks;

	TArray<FTask> IssueCompilationTasks;
	TArray<FTaskEvent> CompilationTasks;

	if (HasOutstandingCompileTasks())
	{
		FTask PrecompileTask = Launch(UE_SOURCE_LOCATION, [this]
		{
			Precompile();
		});

		// async compilation copy tasks
		for (FCompileGroupInfo& GroupInfo : CompileGroups)
		{
			if (GroupInfo.HasOutstandingCompileTasks(*this))
			{
				FTask InstantiateGraphTask = Launch(UE_SOURCE_LOCATION, [this, &GroupInfo]
				{
					GroupInfo.InstantiateCompileGraph(*this);
				}, PrecompileTask);

				for (int32 CompileTaskIndex : GroupInfo.CompileTaskIndices)
				{
					FCompileTaskInfo& CompileTask = CompileTasks[CompileTaskIndex];
					if (CompileTask.IsOutstanding())
					{
						CompileTask.CompileResultsReadyEvent = MakeUnique<FTaskEvent>(UE_SOURCE_LOCATION);
						CompileTask.CompileResultsProcessedEvent = MakeUnique<FTaskEvent>(UE_SOURCE_LOCATION);

						IssueCompilationTasks.Add(Launch(UE_SOURCE_LOCATION, [this, &GroupInfo, &CompileTask]
						{
							CompileTask.CollectNamedDataInterfaces(this, GroupInfo);
							CompileTask.TranslateAndIssueCompile(this, GroupInfo);
						}, InstantiateGraphTask));

						Launch(UE_SOURCE_LOCATION, [this, &GroupInfo, &CompileTask]
						{
							CompileTask.ProcessCompilation(this, GroupInfo);
						}, *CompileTask.CompileResultsReadyEvent);

						CompilationTasks.Add(*CompileTask.CompileResultsProcessedEvent);
					}
				}
			}
		}
	}

	Launch(UE_SOURCE_LOCATION, [this]
	{
		CompilationState = EState::WaitingForProcessing;
	}, IssueCompilationTasks);

	Launch(UE_SOURCE_LOCATION, [this]
	{
		CompileCompletionEvent.Trigger();
		CompilationState = EState::ResultsProcessed;
		PutRequestHelper = MakeUnique<FDispatchDataCachePutRequests>();
		PutRequestHelper->Launch(this);

		Launch(UE_SOURCE_LOCATION, [this]
		{
			CompilationState = EState::Completed;
		}, PutRequestHelper->CompletionEvent);
	}, CompilationTasks);
}

bool FNiagaraSystemCompilationTask::HasOutstandingCompileTasks() const
{
	for (const FCompileTaskInfo& CompileTask : CompileTasks)
	{
		if (!CompileTask.ExeData.IsValid())
		{
			return true;
		}
	}

	return false;
}

UE::Tasks::FTask FNiagaraSystemCompilationTask::BeginTasks()
{
	using namespace UE::Tasks;

	InitialGetRequestHelper.Launch(this);

	FTask SystemTask = Launch(UE_SOURCE_LOCATION, [this]
	{
		if (HasOutstandingCompileTasks())
		{
			FTask BuildRIParamTask = BuildRapidIterationParametersAsync();

			PostRIParameterGetRequestHelper = MakeUnique<FDispatchAndProcessDataCacheGetRequests>();

			Launch(UE_SOURCE_LOCATION, [this]
			{
				PostRIParameterGetRequestHelper->Launch(this);
			}, BuildRIParamTask);

			FTask InnerTask = Launch(UE_SOURCE_LOCATION, [this]
			{
				IssueCompilationTasks();
			}, PostRIParameterGetRequestHelper->CompletionEvent);
		}
		else
		{
			CompilationState = EState::Completed;
			CompileCompletionEvent.Trigger();
		}
	}, InitialGetRequestHelper.CompletionEvent);

	return SystemTask;
}
