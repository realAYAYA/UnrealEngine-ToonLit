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

	// Finds all VM function calls made using the data interface that equals this one (i.e. A->Equals(B))
	// The same function call made be made multiple times across different scripts so you may see the same function multiple times
	// The action should return True to continue iteration or False to stop
	NIAGARA_API void ForEachVMFunctionEquals(UNiagaraDataInterface* DataInterface, UNiagaraSystem* NiagaraSystem, TFunction<bool(const FVMExternalFunctionBindingInfo&)> Action);
	NIAGARA_API void ForEachVMFunctionEquals(UNiagaraDataInterface* DataInterface, UNiagaraSystem* NiagaraSystem, UNiagaraComponent* Component, TFunction<bool(const FVMExternalFunctionBindingInfo&)> Action);
	NIAGARA_API void ForEachVMFunctionEquals(UNiagaraDataInterface* DataInterface, UNiagaraSystem* NiagaraSystem, FNiagaraSystemInstance* SystemInstance, TFunction<bool(const FVMExternalFunctionBindingInfo&)> Action);

	// Finds all Gpu function calls made using the data interface that equals this one (i.e. A->Equals(B))
	// The same function call made be made multiple times across different scripts so you may see the same function multiple times
	// The action should return True to continue iteration or False to stop
	NIAGARA_API void ForEachGpuFunctionEquals(UNiagaraDataInterface* DataInterface, UNiagaraSystem* NiagaraSystem, TFunction<bool(const FNiagaraDataInterfaceGeneratedFunction&)> Action);
	NIAGARA_API void ForEachGpuFunctionEquals(UNiagaraDataInterface* DataInterface, UNiagaraSystem* NiagaraSystem, UNiagaraComponent* Component, TFunction<bool(const FNiagaraDataInterfaceGeneratedFunction&)> Action);
	NIAGARA_API void ForEachGpuFunctionEquals(UNiagaraDataInterface* DataInterface, UNiagaraSystem* NiagaraSystem, FNiagaraSystemInstance* SystemInstance, TFunction<bool(const FNiagaraDataInterfaceGeneratedFunction&)> Action);

	// Finds all VM function calls made using the data interface (i.e. pointer comparison A == B)
	// The same function call made be made multiple times across different scripts so you may see the same function multiple times
	// The action should return True to continue iteration or False to stop
	NIAGARA_API void ForEachVMFunction(UNiagaraDataInterface* DataInterface, FNiagaraSystemInstance* SystemInstance, TFunction<bool(const FVMExternalFunctionBindingInfo&)> Action);
	// Finds all Gpu function calls made using the data interface (i.e. pointer comparison A == B)
	// The same function call made be made multiple times across different scripts so you may see the same function multiple times
	// The action should return True to continue iteration or False to stop
	NIAGARA_API void ForEachGpuFunction(UNiagaraDataInterface* DataInterface, FNiagaraSystemInstance* SystemInstance, TFunction<bool(const FNiagaraDataInterfaceGeneratedFunction&)> Action);

	// Loops over all data interfaces inside the SystemInstance
	// The action should return True to continue iteration or False to stop
	void ForEachDataInterface(FNiagaraSystemInstance* SystemInstance, TFunction<bool(const FNiagaraVariableBase Variable, UNiagaraDataInterface* DataInterface)> Action);
	// Loops over all data interfaces inside the SystemInstance
	// The action should return True to continue iteration or False to stop
	void ForEachDataInterface(FNiagaraSystemInstance* SystemInstance, TFunction<bool(const FDataInterfaceUsageContext&)> Action);
}
