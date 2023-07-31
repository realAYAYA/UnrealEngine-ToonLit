// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataInterfaceUtilities.h"
#include "NiagaraComponent.h"
#include "NiagaraComputeExecutionContext.h"
#include "NiagaraConstants.h"
#include "NiagaraDataInterface.h"
#include "NiagaraSystem.h"

namespace FNiagaraDataInterfaceUtilities
{

void ForEachVMFunctionEqualsImpl(class UNiagaraDataInterface* DataInterface, class UNiagaraSystem* NiagaraSystem, const FNiagaraParameterStore& ParameterStore, TFunction<bool(const FVMExternalFunctionBindingInfo&)> Action)
{
	if ( DataInterface == nullptr || NiagaraSystem == nullptr )
	{
		return;
	}

	// Find all override parameter names
	TArray<FName, TInlineAllocator<8>> OverrideParameterNames;
	{
		for (const UNiagaraDataInterface* OverrideDI : ParameterStore.GetDataInterfaces())
		{
			if ((OverrideDI != nullptr) && ((OverrideDI == DataInterface) || OverrideDI->Equals(DataInterface)))
			{
				if (const FNiagaraVariableBase* Variable = ParameterStore.FindVariable(OverrideDI))
				{
					OverrideParameterNames.AddUnique(Variable->GetName());
				}
			}
		}
	}

	// Loop through all scripts
	bool bContinueSearching = true;
	NiagaraSystem->ForEachScript(
		[&](UNiagaraScript* Script)
		{
			if (bContinueSearching == false)
			{
				return;
			}

			const FNiagaraVMExecutableData& ScriptExecutableData = Script->GetVMExecutableData();
			if (!ScriptExecutableData.IsValid())
			{
				return;
			}

			const TArray<FNiagaraScriptDataInterfaceInfo>& CachedDefaultDIs = Script->GetCachedDefaultDataInterfaces();
			const TArray<FNiagaraScriptDataInterfaceCompileInfo>& DataInterfaceInfos = ScriptExecutableData.DataInterfaceInfo;
			for ( const FVMExternalFunctionBindingInfo& FunctionBinding : ScriptExecutableData.CalledVMExternalFunctions )
			{
				const int NumDataInterface = FMath::Min(DataInterfaceInfos.Num(), CachedDefaultDIs.Num());	// Note: Should always be equal but lets be safe
				for ( int iDataInterface=0; iDataInterface < NumDataInterface; ++iDataInterface )
				{
					const FNiagaraScriptDataInterfaceCompileInfo& DataInterfaceInfo = DataInterfaceInfos[iDataInterface];
					if ( DataInterfaceInfo.Name != FunctionBinding.OwnerName )
					{
						continue;
					}

					const FNiagaraScriptDataInterfaceInfo& CachedDefaultDI = CachedDefaultDIs[iDataInterface];
					if (CachedDefaultDI.DataInterface == nullptr || !DataInterfaceInfo.MatchesClass(DataInterface->GetClass()))
					{
						// Would be odd not to match here, but we are being safe
						break;
					}

					// Do we have a match?
					if (CachedDefaultDI.DataInterface->Equals(DataInterface) || OverrideParameterNames.Contains(CachedDefaultDI.Name))
					{
						if ( Action(FunctionBinding) == false )
						{
							bContinueSearching = false;
							return;
						}
					}
					break;
				}
			}
		}
	);
}

void ForEachGpuFunctionEqualsImpl(class UNiagaraDataInterface* DataInterface, class UNiagaraSystem* NiagaraSystem, const FNiagaraParameterStore& ParameterStore, TFunction<bool(const FNiagaraDataInterfaceGeneratedFunction&)> Action)
{
	if (DataInterface == nullptr || NiagaraSystem == nullptr)
	{
		return;
	}

	// Find all override parameter names
	TArray<FName, TInlineAllocator<8>> OverrideParameterNames;
	{
		for (const UNiagaraDataInterface* OverrideDI : ParameterStore.GetDataInterfaces())
		{
			if ((OverrideDI != nullptr) && ((OverrideDI == DataInterface) || OverrideDI->Equals(DataInterface)))
			{
				if (const FNiagaraVariableBase* Variable = ParameterStore.FindVariable(OverrideDI))
				{
					OverrideParameterNames.AddUnique(Variable->GetName());
				}
			}
		}
	}

	// Loop through all scripts
	bool bContinueSearching = true;
	NiagaraSystem->ForEachScript(
		[&](UNiagaraScript* Script)
		{
			if (bContinueSearching == false)
			{
				return;
			}

			if (FNiagaraShaderScript* ShaderScript = Script->GetRenderThreadScript())
			{
				const TArray<FNiagaraScriptDataInterfaceInfo>& CachedDefaultDIs = Script->GetCachedDefaultDataInterfaces();
				const TArray<FNiagaraDataInterfaceGPUParamInfo>& DataInterfaceParamInfos = ShaderScript->GetScriptParametersMetadata()->DataInterfaceParamInfo;

				const int NumDataInterfaces = FMath::Min(CachedDefaultDIs.Num(), DataInterfaceParamInfos.Num());	// Note: Should always be equal but lets be safe
				for (int iDataInterface = 0; iDataInterface < NumDataInterfaces; ++iDataInterface)
				{
					const FNiagaraScriptDataInterfaceInfo& CachedDefaultDI = CachedDefaultDIs[iDataInterface];
					if (CachedDefaultDI.DataInterface == nullptr)
					{
						continue;
					}
					if (CachedDefaultDI.DataInterface->Equals(DataInterface) || OverrideParameterNames.Contains(CachedDefaultDI.Name))
					{
						for (const FNiagaraDataInterfaceGeneratedFunction& GeneratedFunction : DataInterfaceParamInfos[iDataInterface].GeneratedFunctions)
						{
							if (Action(GeneratedFunction) == false)
							{
								bContinueSearching = false;
								return;
							}
						}
					}
				}
			}
		}
	);
}

void ForEachDataInterface(const FNiagaraParameterStore& ParameterStore, TFunction<bool(const FNiagaraVariableBase Variable, UNiagaraDataInterface* DataInterface)> Action)
{
	const TArray<UNiagaraDataInterface*>& DataInterfaces = ParameterStore.GetDataInterfaces();
	for (const FNiagaraVariableWithOffset& Variable : ParameterStore.ReadParameterVariables())
	{
		if (Variable.IsDataInterface() == false)
		{
			continue;
		}

		if (Variable.IsInNameSpace(FNiagaraConstants::InternalNamespaceString))
		{
			continue;
		}

		if ( UNiagaraDataInterface* DataInterface = DataInterfaces[Variable.Offset] )
		{
			Action(Variable, DataInterface);
		}
	}
}

void ForEachDataInterface(FDataInterfaceUsageContext& UsageContext, const FNiagaraParameterStore& ParameterStore, TFunction<bool(const FDataInterfaceUsageContext&)> Action)
{
	const TArray<UNiagaraDataInterface*>& DataInterfaces = ParameterStore.GetDataInterfaces();
	for (const FNiagaraVariableWithOffset& Variable : ParameterStore.ReadParameterVariables())
	{
		if (Variable.IsDataInterface() == false)
		{
			continue;
		}

		if (Variable.IsInNameSpace(FNiagaraConstants::InternalNamespaceString))
		{
			continue;
		}

		if (UNiagaraDataInterface* DataInterface = DataInterfaces[Variable.Offset])
		{
			UsageContext.Variable = Variable;
			UsageContext.DataInterface = DataInterface;
			Action(UsageContext);
		}
	}
}

} //namespace FNiagaraDataInterfaceUtilities

void FNiagaraDataInterfaceUtilities::ForEachVMFunctionEquals(UNiagaraDataInterface* DataInterface, UNiagaraSystem* NiagaraSystem, TFunction<bool(const FVMExternalFunctionBindingInfo&)> Action)
{
	ForEachVMFunctionEqualsImpl(DataInterface, NiagaraSystem, NiagaraSystem->GetExposedParameters(), Action);
}

void FNiagaraDataInterfaceUtilities::ForEachVMFunctionEquals(UNiagaraDataInterface* DataInterface, UNiagaraSystem* NiagaraSystem, UNiagaraComponent* Component, TFunction<bool(const FVMExternalFunctionBindingInfo&)> Action)
{
	const FNiagaraParameterStore& Parameters = Component ? Component->GetOverrideParameters() : NiagaraSystem->GetExposedParameters();
	ForEachVMFunctionEqualsImpl(DataInterface, NiagaraSystem, Parameters, Action);
}

void FNiagaraDataInterfaceUtilities::ForEachVMFunctionEquals(UNiagaraDataInterface* DataInterface, UNiagaraSystem* NiagaraSystem, FNiagaraSystemInstance* SystemInstance, TFunction<bool(const FVMExternalFunctionBindingInfo&)> Action)
{
	const FNiagaraParameterStore& Parameters = (SystemInstance && SystemInstance->GetOverrideParameters()) ? *SystemInstance->GetOverrideParameters() : NiagaraSystem->GetExposedParameters();
	ForEachVMFunctionEqualsImpl(DataInterface, NiagaraSystem, Parameters, Action);
}

void FNiagaraDataInterfaceUtilities::ForEachGpuFunctionEquals(class UNiagaraDataInterface* DataInterface, class UNiagaraSystem* NiagaraSystem, TFunction<bool(const FNiagaraDataInterfaceGeneratedFunction&)> Action)
{
	ForEachGpuFunctionEqualsImpl(DataInterface, NiagaraSystem, NiagaraSystem->GetExposedParameters(), Action);
}

void FNiagaraDataInterfaceUtilities::ForEachGpuFunctionEquals(class UNiagaraDataInterface* DataInterface, class UNiagaraSystem* NiagaraSystem, UNiagaraComponent* Component, TFunction<bool(const FNiagaraDataInterfaceGeneratedFunction&)> Action)
{
	const FNiagaraParameterStore& Parameters = Component ? Component->GetOverrideParameters() : NiagaraSystem->GetExposedParameters();
	ForEachGpuFunctionEqualsImpl(DataInterface, NiagaraSystem, Parameters, Action);
}

void FNiagaraDataInterfaceUtilities::ForEachGpuFunctionEquals(class UNiagaraDataInterface* DataInterface, class UNiagaraSystem* NiagaraSystem, FNiagaraSystemInstance* SystemInstance, TFunction<bool(const FNiagaraDataInterfaceGeneratedFunction&)> Action)
{
	const FNiagaraParameterStore& Parameters = (SystemInstance && SystemInstance->GetOverrideParameters()) ? *SystemInstance->GetOverrideParameters() : NiagaraSystem->GetExposedParameters();
	ForEachGpuFunctionEqualsImpl(DataInterface, NiagaraSystem, Parameters, Action);
}

void FNiagaraDataInterfaceUtilities::ForEachVMFunction(class UNiagaraDataInterface* DataInterface, class FNiagaraSystemInstance* SystemInstance, TFunction<bool(const FVMExternalFunctionBindingInfo&)> Action)
{
	if ( !DataInterface || !SystemInstance )
	{
		return;
	}

	auto SearchVMContext =
		[](const FNiagaraScriptExecutionContextBase* ExecContext, UNiagaraDataInterface* DataInterface, const TFunction<bool(const FVMExternalFunctionBindingInfo&)>& Action) -> bool
		{
			if (ExecContext == nullptr || ExecContext->Script == nullptr)
			{
				return true;
			}

			const FNiagaraVMExecutableData& ScriptExecutableData = ExecContext->Script->GetVMExecutableData();
			if (!ScriptExecutableData.IsValid())
			{
				return true;
			}

			const TArray<UNiagaraDataInterface*>& DataInterfaces = ExecContext->GetDataInterfaces();
			const int32 NumDataInterfaces = FMath::Min(ScriptExecutableData.DataInterfaceInfo.Num(), DataInterfaces.Num());		// Should be equal, but be safe
			for (const FVMExternalFunctionBindingInfo& FunctionBinding : ScriptExecutableData.CalledVMExternalFunctions)
			{
				for (int32 iDataInterface = 0; iDataInterface < NumDataInterfaces; ++iDataInterface)
				{
					const FNiagaraScriptDataInterfaceCompileInfo& DataInterfaceInfo = ScriptExecutableData.DataInterfaceInfo[iDataInterface];
					if (FunctionBinding.OwnerName != DataInterfaceInfo.Name)
					{
						continue;
					}
					if (DataInterfaces[iDataInterface] != DataInterface)
					{
						continue;
					}

					if (Action(FunctionBinding) == false)
					{
						return false;
					}
				}
			}
			return true;
		};


	// Search system scripts (always VM)
	TSharedPtr<FNiagaraSystemSimulation, ESPMode::ThreadSafe> SystemSimulation = SystemInstance->GetSystemSimulation();
	if ( SystemSimulation.IsValid() )
	{
		if (SearchVMContext(SystemSimulation->GetSpawnExecutionContext(), DataInterface, Action) == false)
		{
			return;
		}
		if (SearchVMContext(SystemSimulation->GetUpdateExecutionContext(), DataInterface, Action) == false)
		{
			return;
		}
	}

	// Search emitter scripts
	for (const TSharedRef<FNiagaraEmitterInstance, ESPMode::ThreadSafe>& EmitterInstance : SystemInstance->GetEmitters())
	{
		if (EmitterInstance->IsDisabled() || EmitterInstance->GetCachedEmitter().Emitter == nullptr || EmitterInstance->GetGPUContext() != nullptr)
		{
			continue;
		}

		if ( SearchVMContext(&EmitterInstance->GetSpawnExecutionContext(), DataInterface, Action) == false )
		{
			return;
		}

		if (SearchVMContext(&EmitterInstance->GetUpdateExecutionContext(), DataInterface, Action) == false)
		{
			return;
		}

		for ( const FNiagaraScriptExecutionContext& EventExecContext : EmitterInstance->GetEventExecutionContexts() )
		{
			if (SearchVMContext(&EventExecContext, DataInterface, Action) == false)
			{
				return;
			}
		}
	}
}

void FNiagaraDataInterfaceUtilities::ForEachGpuFunction(class UNiagaraDataInterface* DataInterface, class FNiagaraSystemInstance* SystemInstance, TFunction<bool(const FNiagaraDataInterfaceGeneratedFunction&)> Action)
{
	if ( !DataInterface || !SystemInstance )
	{
		return;
	}

	for (const TSharedRef<FNiagaraEmitterInstance, ESPMode::ThreadSafe>& EmitterInstance : SystemInstance->GetEmitters())
	{
		if (EmitterInstance->IsDisabled() || EmitterInstance->GetCachedEmitter().Emitter == nullptr || EmitterInstance->GetGPUContext() == nullptr)
		{
			continue;
		}

		const FNiagaraScriptInstanceParameterStore& ParameterStore = EmitterInstance->GetGPUContext()->CombinedParamStore;
		const TArray<UNiagaraDataInterface*>& DataInterfaces = ParameterStore.GetDataInterfaces();
		const TArray<FNiagaraDataInterfaceGPUParamInfo>& DataInterfaceParamInfos = EmitterInstance->GetGPUContext()->GPUScript_RT->GetScriptParametersMetadata()->DataInterfaceParamInfo;
		const int32 NumDataInterface = FMath::Min(DataInterfaces.Num(), DataInterfaceParamInfos.Num());		// Should be equal, but be safe
		for ( int32 iDataInterface=0; iDataInterface < NumDataInterface; ++iDataInterface)
		{
			if ( DataInterfaces[iDataInterface] != DataInterface )
			{
				continue;
			}

			for (const FNiagaraDataInterfaceGeneratedFunction& GeneratedFunction : DataInterfaceParamInfos[iDataInterface].GeneratedFunctions)
			{
				if (Action(GeneratedFunction) == false)
				{
					return;
				}
			}
		}
	}
}

void FNiagaraDataInterfaceUtilities::ForEachDataInterface(FNiagaraSystemInstance* SystemInstance, TFunction<bool(const FNiagaraVariableBase Variable, UNiagaraDataInterface* DataInterface)> Action)
{
	if ( SystemInstance == nullptr )
	{
		return;
	}

	FNiagaraSystemSimulationPtr SystemSimulation = SystemInstance->GetSystemSimulation();
	if (SystemSimulation == nullptr || !SystemSimulation->IsValid())
	{
		return;
	}

	ForEachDataInterface(*SystemInstance->GetOverrideParameters(), Action);

	ForEachDataInterface(SystemSimulation->GetSpawnExecutionContext()->Parameters, Action);
	ForEachDataInterface(SystemSimulation->GetUpdateExecutionContext()->Parameters, Action);

	for (const TSharedRef<FNiagaraEmitterInstance, ESPMode::ThreadSafe>& Emitter : SystemInstance->GetEmitters())
	{
		if ( Emitter->IsDisabled() )
		{
			continue;
		}

		ForEachDataInterface(Emitter->GetSpawnExecutionContext().Parameters, Action);
		ForEachDataInterface(Emitter->GetUpdateExecutionContext().Parameters, Action);
		for (int32 i=0; i < Emitter->GetEventExecutionContexts().Num(); i++)
		{
			ForEachDataInterface(Emitter->GetEventExecutionContexts()[i].Parameters, Action);
		}

		FVersionedNiagaraEmitterData* EmitterData = Emitter->GetCachedEmitterData();
		if (EmitterData && EmitterData->SimTarget == ENiagaraSimTarget::GPUComputeSim && Emitter->GetGPUContext())
		{
			ForEachDataInterface(Emitter->GetGPUContext()->CombinedParamStore, Action);
		}
	}
}

void FNiagaraDataInterfaceUtilities::ForEachDataInterface(FNiagaraSystemInstance* SystemInstance, TFunction<bool(const FDataInterfaceUsageContext&)> Action)
{
	if (SystemInstance == nullptr)
	{
		return;
	}

	FNiagaraSystemSimulationPtr SystemSimulation = SystemInstance->GetSystemSimulation();
	if (SystemSimulation == nullptr || !SystemSimulation->IsValid())
	{
		return;
	}

	FDataInterfaceUsageContext UsageContext;
	UsageContext.OwnerObject = SystemSimulation->GetSystem();
	ForEachDataInterface(UsageContext, *SystemInstance->GetOverrideParameters(), Action);

	ForEachDataInterface(UsageContext, SystemSimulation->GetSpawnExecutionContext()->Parameters, Action);
	ForEachDataInterface(UsageContext, SystemSimulation->GetUpdateExecutionContext()->Parameters, Action);

	for (const TSharedRef<FNiagaraEmitterInstance, ESPMode::ThreadSafe>& Emitter : SystemInstance->GetEmitters())
	{
		if (Emitter->IsDisabled())
		{
			continue;
		}
		UsageContext.OwnerObject = Emitter->GetCachedEmitter().Emitter;

		ForEachDataInterface(UsageContext, Emitter->GetSpawnExecutionContext().Parameters, Action);
		ForEachDataInterface(UsageContext, Emitter->GetUpdateExecutionContext().Parameters, Action);
		for (int32 i = 0; i < Emitter->GetEventExecutionContexts().Num(); i++)
		{
			ForEachDataInterface(UsageContext, Emitter->GetEventExecutionContexts()[i].Parameters, Action);
		}

		FVersionedNiagaraEmitterData* EmitterData = Emitter->GetCachedEmitterData();
		if (EmitterData && EmitterData->SimTarget == ENiagaraSimTarget::GPUComputeSim && Emitter->GetGPUContext())
		{
			ForEachDataInterface(UsageContext, Emitter->GetGPUContext()->CombinedParamStore, Action);
		}
	}
}
