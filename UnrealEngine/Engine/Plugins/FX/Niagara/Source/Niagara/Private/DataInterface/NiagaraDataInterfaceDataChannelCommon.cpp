// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataInterface/NiagaraDataInterfaceDataChannelCommon.h"
#include "Misc/LazySingleton.h"
#include "NiagaraDataInterfaceUtilities.h"
#include "NiagaraScript.h"

#define LOCTEXT_NAMESPACE "NiagaraDataInterfaceDataChannelCommon"


FAutoConsoleCommandWithWorldAndArgs ResetDataChannelLayouts(
	TEXT("fx.Niagara.DataChannels.ResetLayoutInfo"),
	TEXT("Resets all data channel layout info used by data interfaces to access data channels."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
		[](const TArray<FString>& Args, UWorld* World)
		{
			FNDIDataChannelLayoutManager::Get().Reset();
		}
	)
);


FNDIDataChannel_FunctionToDataSetBinding::FNDIDataChannel_FunctionToDataSetBinding(const FNDIDataChannelFunctionInfo& FunctionInfo, const FNiagaraDataSetCompiledData& DataSetLayout, TArray<FNiagaraVariableBase>& OutMissingParams)
{
#if DEBUG_NDI_DATACHANNEL
	DebugFunctionInfo = FunctionInfo;
	DebugCompiledData = DataSetLayout;
#endif

	FunctionLayoutHash = GetTypeHash(FunctionInfo);
	DataSetLayoutHash = DataSetLayout.GetLayoutHash();

	VMRegisterBindings.Reset();
	auto DoGenVMBindings = [&](const TArray<FNiagaraVariableBase>& Parameters)
	{
		for (int32 ParamIdx = 0; ParamIdx < Parameters.Num(); ++ParamIdx)
		{
			const FNiagaraVariableBase& Param = Parameters[ParamIdx];
			if (const FNiagaraVariableLayoutInfo* DataSetVariableLayout = DataSetLayout.FindVariableLayoutInfo(Param))
			{
				uint32 DataSetFloatRegister = DataSetVariableLayout->GetFloatComponentStart();
				uint32 DataSetIntRegister = DataSetVariableLayout->GetInt32ComponentStart();
				uint32 DataSetHalfRegister = DataSetVariableLayout->GetHalfComponentStart();

				GenVMBindings(Param, Param.GetType().GetStruct(), NumFloatComponents, NumInt32Components, NumHalfComponents, DataSetFloatRegister, DataSetIntRegister, DataSetHalfRegister);
			}
			else
			{
				DataSetLayoutHash = 0;

				#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
				OutMissingParams.Emplace(Param);
				#else
				return;
				#endif
			}
		}
	};

	DoGenVMBindings(FunctionInfo.Inputs);
	DoGenVMBindings(FunctionInfo.Outputs);
}

void FNDIDataChannel_FunctionToDataSetBinding::GenVMBindings(const FNiagaraVariableBase& Var, const UStruct* Struct, uint32& FuncFloatRegister, uint32& FuncIntRegister, uint32& FuncHalfRegister, uint32& DataSetFloatRegister, uint32& DataSetIntRegister, uint32& DataSetHalfRegister)
{
	for (TFieldIterator<FProperty> PropertyIt(Struct, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
	{
		FProperty* Property = *PropertyIt;
		if (Property->IsA(FFloatProperty::StaticClass()))
		{
			VMRegisterBindings.Emplace(FuncFloatRegister++, DataSetFloatRegister == INDEX_NONE ? INDEX_NONE : DataSetFloatRegister++, ENiagaraBaseTypes::Float);
		}
		else if (Property->IsA(FUInt16Property::StaticClass()))
		{
			VMRegisterBindings.Emplace(FuncHalfRegister++, DataSetHalfRegister == INDEX_NONE ? INDEX_NONE : DataSetIntRegister++, ENiagaraBaseTypes::Half);
		}
		else if (Property->IsA(FIntProperty::StaticClass()))
		{
			VMRegisterBindings.Emplace(FuncIntRegister++, DataSetIntRegister == INDEX_NONE ? INDEX_NONE : DataSetIntRegister++, ENiagaraBaseTypes::Int32);
		}
		else if (Property->IsA(FBoolProperty::StaticClass()))
		{
			VMRegisterBindings.Emplace(FuncIntRegister++, DataSetIntRegister == INDEX_NONE ? INDEX_NONE : DataSetIntRegister++, ENiagaraBaseTypes::Bool);
		}
		//Should be able to support double easily enough
		else if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
		{
			GenVMBindings(Var, FNiagaraTypeHelper::FindNiagaraFriendlyTopLevelStruct(StructProp->Struct, ENiagaraStructConversion::Simulation), FuncFloatRegister, FuncIntRegister, FuncHalfRegister, DataSetFloatRegister, DataSetIntRegister, DataSetHalfRegister);
		}
		else
		{
			checkf(false, TEXT("Property(%s) Class(%s) is not a supported type"), *Property->GetName(), *Property->GetClass()->GetName());
			DataSetLayoutHash = 0;
			return;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
// FNDIDataChannelFunctionInfo

uint32 GetTypeHash(const FNDIDataChannelFunctionInfo& FuncInfo)
{
	uint32 Ret = GetTypeHash(FuncInfo.FunctionName);
	for (const FNiagaraVariableBase& Param : FuncInfo.Inputs)
	{
		Ret = HashCombine(Ret, GetTypeHash(Param));
	}
	for (const FNiagaraVariableBase& Param : FuncInfo.Outputs)
	{
		Ret = HashCombine(Ret, GetTypeHash(Param));
	}
	return Ret;
}

bool FNDIDataChannelFunctionInfo::operator==(const FNDIDataChannelFunctionInfo& Other)const
{
	return FunctionName == Other.FunctionName && Inputs == Other.Inputs && Outputs == Other.Outputs;
}

bool FNDIDataChannelFunctionInfo::CheckHashConflict(const FNDIDataChannelFunctionInfo& Other)const
{
	//If we have the same hash ensure we have the same data.
	if (GetTypeHash(*this) == GetTypeHash(Other))
	{
		return *this != Other;
	}
	return false;
}

//FNDIDataChannelFunctionInfo End
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//FNDIDataChannelLayoutManager

FNDIDataChannelLayoutManager& FNDIDataChannelLayoutManager::Get()
{
	return TLazySingleton<FNDIDataChannelLayoutManager>::Get();
}

void FNDIDataChannelLayoutManager::TearDown()
{
	TLazySingleton<FNDIDataChannelLayoutManager>::TearDown();
}

void FNDIDataChannelLayoutManager::Reset()
{
	FRWScopeLock WriteLock(FunctionToDataSetMapLock, SLT_Write);
	FunctionToDataSetLayoutMap.Reset();
}

FNDIDataChannel_FuncToDataSetBindingPtr FNDIDataChannelLayoutManager::GetLayoutInfo(const FNDIDataChannelFunctionInfo& FunctionInfo, const FNiagaraDataSetCompiledData& DataSetLayout, TArray<FNiagaraVariableBase>& OutMissingParams)
{
	uint32 Key = GetLayoutKey(FunctionInfo, DataSetLayout);
	
	//Attempt to find a valid existing layout.
	{
		FRWScopeLock ReadLock(FunctionToDataSetMapLock, SLT_ReadOnly);
		FNDIDataChannel_FuncToDataSetBindingPtr* FuncLayout = FunctionToDataSetLayoutMap.Find(Key);
		if (FuncLayout && FuncLayout->IsValid())
		{
			return *FuncLayout;
		}
	}

	//No valid existing layout so generate a new one.
	FRWScopeLock WriteLock(FunctionToDataSetMapLock, SLT_Write);
	FNDIDataChannel_FuncToDataSetBindingPtr FuncLayout = MakeShared<FNDIDataChannel_FunctionToDataSetBinding, ESPMode::ThreadSafe>(FunctionInfo, DataSetLayout, OutMissingParams);
	if (FuncLayout.IsValid() == false)
	{
		FunctionToDataSetLayoutMap.Add(Key) = FuncLayout;
	}
#if DEBUG_NDI_DATACHANNEL
	else
	{
		checkf(FuncLayout->DebugFunctionInfo.CheckHashConflict(FunctionInfo) == false, TEXT("Key conflict. Function Information does not match that already placed at this key."));
		checkf(FuncLayout->DebugCompiledData.CheckHashConflict(DataSetLayout) == false, TEXT("Key conflict. DataSet Compiled Information does not match that already placed at this key."));
	}
#endif
	return FuncLayout;
}

//FNDIDataChannelLayoutManager END
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//FNDIDataChannelCompiledData

void FNDIDataChannelCompiledData::GatherAccessInfo(UNiagaraSystem* System, UNiagaraDataInterface* Owner)
{
	//We search all VM functions called on this DI to generate an appropriate FNDIDataChannelFunctionInfo that can later be used in binding to actual dataset Data.
	auto HandleVMFunc = [&](const UNiagaraScript* Script, const FVMExternalFunctionBindingInfo& BindingInfo)
	{
		if (BindingInfo.VariadicInputs.Num() > 0 || BindingInfo.VariadicOutputs.Num() > 0)
		{
			//Ensure we have a binding with valid input and outputs.		
			int32 FuncInfoIdx = FindFunctionInfoIndex(BindingInfo.Name, BindingInfo.VariadicInputs, BindingInfo.VariadicOutputs);
			if (FuncInfoIdx == INDEX_NONE)
			{
				FNDIDataChannelFunctionInfo& FuncInfo = FunctionInfo.AddDefaulted_GetRef();
				FuncInfo.FunctionName = BindingInfo.Name;
				FuncInfo.Inputs = BindingInfo.VariadicInputs;
				FuncInfo.Outputs = BindingInfo.VariadicOutputs;
			}
		}
		bUsedByCPU = true;

		if(BindingInfo.Name == NDIDataChannelUtilities::GetNDCSpawnDataName)
		{
			bNeedsSpawnDataTable = true;
		}

		return true;
	};
	FNiagaraDataInterfaceUtilities::ForEachVMFunction(Owner, System, HandleVMFunc);


	//For every GPU script we iterate over the functions it calls and add each of them to the mapping.
	//This will then be placed in a buffer for the RT to pass to the GPU so that each script can look up the correct function layout info.
	GPUScriptParameterInfos.Empty();
	TotalParams = 0;
	auto HandleGpuFunc = [&](const UNiagaraScript* Script, const FNiagaraDataInterfaceGeneratedFunction& BindingInfo)
	{
		if (BindingInfo.VariadicInputs.Num() > 0 || BindingInfo.VariadicOutputs.Num() > 0)
		{
			const FNiagaraCompileHash ScriptCompileHash = Script->GetComputedVMCompilationId().BaseScriptCompileHash;
			FNDIDataChannel_GPUScriptParameterAccessInfo& ScriptParamAccessInfo = GPUScriptParameterInfos.FindOrAdd(ScriptCompileHash);

			for (const auto& Var : BindingInfo.VariadicInputs)
			{
				ScriptParamAccessInfo.SortedParameters.AddUnique(Var);
			}
			for (const auto& Var : BindingInfo.VariadicOutputs)
			{
				ScriptParamAccessInfo.SortedParameters.AddUnique(Var);
			}
		}
		bUsedByGPU = true;

		if (BindingInfo.DefinitionName == TEXT("GetNDCSpawnData"))
		{
			bNeedsSpawnDataTable = true;
		}
		return true;
	};
	FNiagaraDataInterfaceUtilities::ForEachGpuFunction(Owner, System, HandleGpuFunc);

	//Now we've generated the complete set of parameters accessed by each GPU script, we sort them to ensure identical access between the hlsl and the table we generate.
	for (auto& Pair : GPUScriptParameterInfos)
	{
		FNDIDataChannel_GPUScriptParameterAccessInfo& ScriptParamAccessInfo = Pair.Value;
		NDIDataChannelUtilities::SortParameters(ScriptParamAccessInfo.SortedParameters);
		TotalParams += ScriptParamAccessInfo.SortedParameters.Num();
	}
}

bool FNDIDataChannelCompiledData::Init(UNiagaraSystem* System, UNiagaraDataInterface* OwnerDI)
{
	FunctionInfo.Empty();

	check(System);
	check(OwnerDI);

	GatherAccessInfo(System, OwnerDI);
	return true;
}

int32 FNDIDataChannelCompiledData::FindFunctionInfoIndex(FName Name, const TArray<FNiagaraVariableBase>& VariadicInputs, const TArray<FNiagaraVariableBase>& VariadicOutputs)const
{
	for (int32 FuncIndex = 0; FuncIndex < FunctionInfo.Num(); ++FuncIndex)
	{
		const FNDIDataChannelFunctionInfo& FuncInfo = FunctionInfo[FuncIndex];
		if (FuncInfo.FunctionName == Name && VariadicInputs == FuncInfo.Inputs && VariadicOutputs == FuncInfo.Outputs)
		{
			return FuncIndex;
		}
	}
	return INDEX_NONE;
}

//FNDIDataChannelCompiledData END
//////////////////////////////////////////////////////////////////////////



namespace NDIDataChannelUtilities
{
	const FName GetNDCSpawnDataName(TEXT("GetNDCSpawnData"));

	void SortParameters(TArray<FNiagaraVariableBase>& Parameters)
	{
		Parameters.Sort([](const FNiagaraVariableBase& Lhs, const FNiagaraVariableBase& Rhs)
			{
				int32 ComparisonDiff = Lhs.GetName().Compare(Rhs.GetName());
				if (ComparisonDiff == 0)
				{
					ComparisonDiff = Lhs.GetType().GetFName().Compare(Rhs.GetType().GetFName());
				}
				return ComparisonDiff < 0;
			});
	}
}

#undef LOCTEXT_NAMESPACE
