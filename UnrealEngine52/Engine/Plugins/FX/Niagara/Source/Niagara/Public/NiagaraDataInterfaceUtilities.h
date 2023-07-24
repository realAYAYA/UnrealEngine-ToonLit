// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "NiagaraCommon.h"
#include "NiagaraShared.h"

class UNiagaraComponent;
class UNiagaraDataInterface;
class UNiagaraSystem;
class FNiagaraSystemInstance;

namespace FNiagaraDataInterfaceUtilities
{
	struct FDataInterfaceUsageContext
	{
		UObject*				OwnerObject = nullptr;
		FNiagaraVariableBase	Variable;
		UNiagaraDataInterface*	DataInterface = nullptr;
	};

	struct FDataInterfaceSearchOptions
	{
		bool bIncludeInternal = false;
	};

	// Finds all VM function calls made using the data interface that equals this one (i.e. A->Equals(B))
	// The same function call made be made multiple times across different scripts so you may see the same function multiple times
	// The action should return True to continue iteration or False to stop
	NIAGARA_API void ForEachVMFunctionEquals(UNiagaraDataInterface* DataInterface, UNiagaraSystem* NiagaraSystem, TFunction<bool(const UNiagaraScript*, const FVMExternalFunctionBindingInfo&)> Action);
	NIAGARA_API void ForEachVMFunctionEquals(UNiagaraDataInterface* DataInterface, UNiagaraSystem* NiagaraSystem, UNiagaraComponent* Component, TFunction<bool(const UNiagaraScript*, const FVMExternalFunctionBindingInfo&)> Action);
	NIAGARA_API void ForEachVMFunctionEquals(UNiagaraDataInterface* DataInterface, UNiagaraSystem* NiagaraSystem, FNiagaraSystemInstance* SystemInstance, TFunction<bool(const UNiagaraScript*, const FVMExternalFunctionBindingInfo&)> Action);

	// Finds all Gpu function calls made using the data interface that equals this one (i.e. A->Equals(B))
	// The same function call made be made multiple times across different scripts so you may see the same function multiple times
	// The action should return True to continue iteration or False to stop
	NIAGARA_API void ForEachGpuFunctionEquals(UNiagaraDataInterface* DataInterface, UNiagaraSystem* NiagaraSystem, TFunction<bool(const UNiagaraScript*, const FNiagaraDataInterfaceGeneratedFunction&)> Action);
	NIAGARA_API void ForEachGpuFunctionEquals(UNiagaraDataInterface* DataInterface, UNiagaraSystem* NiagaraSystem, UNiagaraComponent* Component, TFunction<bool(const UNiagaraScript*, const FNiagaraDataInterfaceGeneratedFunction&)> Action);
	NIAGARA_API void ForEachGpuFunctionEquals(UNiagaraDataInterface* DataInterface, UNiagaraSystem* NiagaraSystem, FNiagaraSystemInstance* SystemInstance, TFunction<bool(const UNiagaraScript*, const FNiagaraDataInterfaceGeneratedFunction&)> Action);

	// Finds all VM function calls made using the data interface (i.e. pointer comparison A == B)
	// The same function call made be made multiple times across different scripts so you may see the same function multiple times
	// The action should return True to continue iteration or False to stop
	NIAGARA_API void ForEachVMFunction(UNiagaraDataInterface* DataInterface, FNiagaraSystemInstance* SystemInstance, TFunction<bool(const UNiagaraScript*, const FVMExternalFunctionBindingInfo&)> Action);
	// Finds all Gpu function calls made using the data interface (i.e. pointer comparison A == B)
	// The same function call made be made multiple times across different scripts so you may see the same function multiple times
	// The action should return True to continue iteration or False to stop
	NIAGARA_API void ForEachGpuFunction(UNiagaraDataInterface* DataInterface, FNiagaraSystemInstance* SystemInstance, TFunction<bool(const FNiagaraDataInterfaceGeneratedFunction&)> Action);

	// Loops over all data interfaces inside the SystemInstance
	// The action should return True to continue iteration or False to stop
	NIAGARA_API void ForEachDataInterface(FNiagaraSystemInstance* SystemInstance, TFunction<bool(const FNiagaraVariableBase Variable, UNiagaraDataInterface* DataInterface)> Action, FDataInterfaceSearchOptions SearchOptions = FDataInterfaceSearchOptions());
	// Loops over all data interfaces inside the SystemInstance
	// The action should return True to continue iteration or False to stop
	NIAGARA_API void ForEachDataInterface(FNiagaraSystemInstance* SystemInstance, TFunction<bool(const FDataInterfaceUsageContext&)> Action, FDataInterfaceSearchOptions SearchOptions = FDataInterfaceSearchOptions());
	// Loops over all data interfaces inside the NiagaraSystem
	// The action should return True to continue iteration or False to stop
	NIAGARA_API void ForEachDataInterface(UNiagaraSystem* NiagaraSystem, TFunction<bool(const FDataInterfaceUsageContext&)> Action, FDataInterfaceSearchOptions SearchOptions = FDataInterfaceSearchOptions());
}

#if WITH_EDITOR

/** Helper context object helping to facilitate data interfaces building their hlsl shader code for GPU simulations. */
struct FNiagaraDataInterfaceHlslGenerationContext
{
	DECLARE_DELEGATE_RetVal_OneParam(FString, FGetStructHlslTypeName, const FNiagaraTypeDefinition& /*Type*/)
	DECLARE_DELEGATE_RetVal_OneParam(FString, FGetPropertyHlslTypeName, const FProperty* /*Property*/)
	DECLARE_DELEGATE_RetVal_TwoParams(FString, FGetSanitizedSymbolName, FStringView /*SymbolName*/, bool /*bCollapsNamespaces*/)

	FNiagaraDataInterfaceHlslGenerationContext(const FNiagaraDataInterfaceGPUParamInfo& InParameterInfo, TArrayView<const FNiagaraFunctionSignature> InSignatures)
		: ParameterInfo(InParameterInfo), Signatures(InSignatures)
	{
	}

	const FNiagaraDataInterfaceGPUParamInfo& ParameterInfo;
	TArrayView<const FNiagaraFunctionSignature> Signatures;
	int32 FunctionInstanceIndex = INDEX_NONE;

	FGetStructHlslTypeName GetStructHlslTypeNameDelegate;
	FGetPropertyHlslTypeName GetPropertyHlslTypeNameDelegate;
	FGetSanitizedSymbolName GetSanitizedSymbolNameDelegate;

	const FNiagaraDataInterfaceGeneratedFunction& GetFunctionInfo() const { return ParameterInfo.GeneratedFunctions[FunctionInstanceIndex]; }

	FString GetStructHlslTypeName(const FNiagaraTypeDefinition& Type) { return GetStructHlslTypeNameDelegate.Execute(Type); }
	FString GetPropertyHlslTypeName(const FProperty* Property) { return GetPropertyHlslTypeNameDelegate.Execute(Property); }
	FString GetSanitizedSymbolName(FStringView SymbolName, bool bCollapsNamespaces = false) { return GetSanitizedSymbolNameDelegate.Execute(SymbolName, bCollapsNamespaces); }
};

#endif