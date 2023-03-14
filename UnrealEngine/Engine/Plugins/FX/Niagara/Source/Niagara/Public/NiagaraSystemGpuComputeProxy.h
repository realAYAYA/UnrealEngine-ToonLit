// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraCommon.h"

class FNiagaraSystemInstance;
class FNiagaraGpuComputeDispatchInterface;
struct FNiagaraComputeExecutionContext;
class FNiagaraGPUInstanceCountManager;
class FNiagaraGPUSystemTick;

class FNiagaraSystemGpuComputeProxy
{
	friend class FNiagaraGpuComputeDispatch;

public:
	FNiagaraSystemGpuComputeProxy(FNiagaraSystemInstance* OwnerInstance);
	~FNiagaraSystemGpuComputeProxy();

	void AddToRenderThread(FNiagaraGpuComputeDispatchInterface* ComputeDispatchInterface);
	void RemoveFromRenderThread(FNiagaraGpuComputeDispatchInterface* ComputeDispatchInterface, bool bDeleteProxy);
	void ClearTicksFromRenderThread(FNiagaraGpuComputeDispatchInterface* ComputeDispatchInterface);

	FNiagaraSystemInstanceID GetSystemInstanceID() const { return SystemInstanceID; }
	ENiagaraGpuComputeTickStage::Type GetComputeTickStage() const { return ComputeTickStage; }
	void QueueTick(const FNiagaraGPUSystemTick& Tick);
	void ReleaseTicks(FNiagaraGPUInstanceCountManager& GPUInstanceCountManager, int32 NumTicksToRelease);

	bool RequiresDistanceFieldData() const { return bRequiresDistanceFieldData; }
	bool RequiresDepthBuffer() const { return bRequiresDepthBuffer; }
	bool RequiresEarlyViewData() const { return bRequiresEarlyViewData; }
	bool RequiresViewUniformBuffer() const { return bRequiresViewUniformBuffer; }
	bool RequiresRayTracingScene() const { return bRequiresRayTracingScene; }
	FVector3f GetSystemLWCTile() const { return SystemLWCTile; }

private:
	FNiagaraSystemInstance*						DebugOwnerInstance = nullptr;
	FNiagaraGpuComputeDispatchInterface*		DebugOwnerComputeDispatchInterface = nullptr;
	int32										ComputeDispatchIndex = INDEX_NONE;
	FVector3f									SystemLWCTile;

	FNiagaraSystemInstanceID					SystemInstanceID = FNiagaraSystemInstanceID();
	ENiagaraGpuComputeTickStage::Type			ComputeTickStage = ENiagaraGpuComputeTickStage::PostOpaqueRender;
	uint32										bRequiresDistanceFieldData : 1;
	uint32										bRequiresDepthBuffer : 1;
	uint32										bRequiresEarlyViewData : 1;
	uint32										bRequiresViewUniformBuffer : 1;
	uint32										bRequiresRayTracingScene : 1;

	FShaderResourceViewRHIRef					StaticFloatBuffer;

	TArray<FNiagaraComputeExecutionContext*>	ComputeContexts;
	TArray<FNiagaraGPUSystemTick>				PendingTicks;
};
