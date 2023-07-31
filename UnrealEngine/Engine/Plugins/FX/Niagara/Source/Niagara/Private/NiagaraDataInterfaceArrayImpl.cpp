// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataInterfaceArrayImpl.h"

const TCHAR* FNiagaraDataInterfaceArrayImplHelper::HLSLReadTemplateFile = TEXT("/Plugin/FX/Niagara/Private/NiagaraDataInterfaceArrayTemplate.ush");
const TCHAR* FNiagaraDataInterfaceArrayImplHelper::HLSLReadWriteTemplateFile = TEXT("/Plugin/FX/Niagara/Private/NiagaraDataInterfaceArrayRWTemplate.ush");

const FName FNiagaraDataInterfaceArrayImplHelper::Function_LengthName(TEXT("Length"));
const FName FNiagaraDataInterfaceArrayImplHelper::Function_IsValidIndexName(TEXT("IsValidIndex"));
const FName FNiagaraDataInterfaceArrayImplHelper::Function_LastIndexName(TEXT("LastIndex"));
const FName FNiagaraDataInterfaceArrayImplHelper::Function_GetName(TEXT("Get"));

const FName FNiagaraDataInterfaceArrayImplHelper::Function_ClearName(TEXT("Clear"));
const FName FNiagaraDataInterfaceArrayImplHelper::Function_ResizeName(TEXT("Resize"));
const FName FNiagaraDataInterfaceArrayImplHelper::Function_SetArrayElemName(TEXT("SetArrayElem"));
const FName FNiagaraDataInterfaceArrayImplHelper::Function_AddName(TEXT("Add"));
const FName FNiagaraDataInterfaceArrayImplHelper::Function_RemoveLastElemName(TEXT("RemoveLastElem"));

const FName FNiagaraDataInterfaceArrayImplHelper::Function_AtomicAddName("AtomicAdd");
const FName FNiagaraDataInterfaceArrayImplHelper::Function_AtomicMinName("AtomicMin");
const FName FNiagaraDataInterfaceArrayImplHelper::Function_AtomicMaxName("AtomicMax");

#if WITH_EDITORONLY_DATA
bool FNiagaraDataInterfaceArrayImplHelper::UpgradeFunctionCall(FNiagaraFunctionSignature& FunctionSignature)
{
	// Early out, nothing to do here
	if ( FunctionSignature.FunctionVersion == FFunctionVersion::LatestVersion )
	{
		return false;
	}

	if ( FunctionSignature.FunctionVersion < FFunctionVersion::AddOptionalExecuteToSet )
	{
		static const TPair<FName, FName> NodeRenames[] =
		{
			MakeTuple(FName("GetNum"),			FNiagaraDataInterfaceArrayImplHelper::Function_LengthName),
			MakeTuple(FName("GetValue"),		FNiagaraDataInterfaceArrayImplHelper::Function_GetName),
			MakeTuple(FName("Reset"),			FNiagaraDataInterfaceArrayImplHelper::Function_ClearName),
			MakeTuple(FName("SetNum"),			FNiagaraDataInterfaceArrayImplHelper::Function_ResizeName),
			MakeTuple(FName("SetValue"),		FNiagaraDataInterfaceArrayImplHelper::Function_SetArrayElemName),
			MakeTuple(FName("PushValue"),		FNiagaraDataInterfaceArrayImplHelper::Function_AddName),
			MakeTuple(FName("PopValue"),		FNiagaraDataInterfaceArrayImplHelper::Function_RemoveLastElemName),
		};

		for (const auto& Pair : NodeRenames)
		{
			if (Pair.Key == FunctionSignature.Name)
			{
				FunctionSignature.Name = Pair.Value;
				break;
			}
		}

		FunctionSignature.bExperimental = false;

		if (FunctionSignature.Name == FNiagaraDataInterfaceArrayImplHelper::Function_SetArrayElemName)
		{
			FunctionSignature.Inputs.EmplaceAt(1, FNiagaraTypeDefinition::GetBoolDef(), TEXT("SkipSet"));
		}
	}

	FunctionSignature.FunctionVersion = FFunctionVersion::LatestVersion;

	return true;
}
#endif

bool FNiagaraDataInterfaceArrayImplHelper::IsRWFunction(const FName FunctionName)
{
	static const TSet<FName> RWFunctions =
	{
		Function_ClearName,
		Function_ResizeName,
		Function_SetArrayElemName,
		Function_AddName,
		Function_RemoveLastElemName,

		Function_AtomicAddName,
		Function_AtomicMinName,
		Function_AtomicMaxName,
	};
	return RWFunctions.Contains(FunctionName);
}
