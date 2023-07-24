// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraCommon.h"

class FNiagaraGpuComputeDispatchInterface;

// Abstract class for managing GPU data for Niagara
// Once the manager is created it will last for the lifetime of the owner dispatch interface
// The manager will be destroyed when the dispatch interface is also destroyed
class NIAGARA_API FNiagaraGpuComputeDataManager
{
	UE_NONCOPYABLE(FNiagaraGpuComputeDataManager);

public:
	FNiagaraGpuComputeDataManager(FNiagaraGpuComputeDispatchInterface* InOwnerInterface)
		: InternalOwnerInterface(InOwnerInterface)
	{
	}
	virtual ~FNiagaraGpuComputeDataManager() {}

	FNiagaraGpuComputeDispatchInterface* GetOwnerInterface() const { return InternalOwnerInterface; }

	//static FName GetManagerName()
	//{
	//	static FName ManagerName("FNiagaraGpuComputeDataManager");
	//	return ManagerName;
	//}

private:
	FNiagaraGpuComputeDispatchInterface* InternalOwnerInterface = nullptr;
};
