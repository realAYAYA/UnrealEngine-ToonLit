// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraGpuComputeDataManager.h"
#include "NiagaraDataSet.h"
#include "NiagaraRendererProperties.h"

#include "RenderGraphFwd.h"

class FSceneViewFamily;
class FNiagaraDataBuffer;
class FNiagaraGpuComputeDispatchInterface;
namespace NiagaraStateless
{
	class FEmitterInstance_RT;
}

class FNiagaraStatelessComputeManager final : public FNiagaraGpuComputeDataManager
{
	struct FStatelessDataCache
	{
		uint32										DataSetLayoutHash = 0;
		FNiagaraDataSet								DataSet;
		FNiagaraDataBufferRef						DataBuffer;

		const NiagaraStateless::FEmitterInstance_RT* EmitterInstance = nullptr;
		uint32										ActiveParticles = 0;
	};

public:
	explicit FNiagaraStatelessComputeManager(FNiagaraGpuComputeDispatchInterface* InOwnerInterface);
	virtual ~FNiagaraStatelessComputeManager();

	static FName GetManagerName()
	{
		static FName ManagerName("FNiagaraStatelessComputeManager");
		return ManagerName;
	}

	FNiagaraDataBuffer* GetDataBuffer(uintptr_t EmitterKey, const NiagaraStateless::FEmitterInstance_RT* EmitterInstance);

private:
	void OnPostPreRender(FRDGBuilder& GraphBuilder);
	void OnPostPostRender(FRDGBuilder& GraphBuilder);

private:
	FNiagaraRendererLayout								SpriteStatelessLayout;

	TMap<uintptr_t, TUniquePtr<FStatelessDataCache>>	UsedData;
	TArray<TUniquePtr<FStatelessDataCache>>				FreeData;
	TArray<uint32>										CountsToRelease;
};
