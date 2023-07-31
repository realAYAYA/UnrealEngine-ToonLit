// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "GlobalDistanceFieldParameters.h"
#include "NiagaraAsyncGpuTraceProvider.h"

class FNiagaraAsyncGpuTraceProviderGsdf : public FNiagaraAsyncGpuTraceProvider
{
public:
	static const EProviderType Type;

	FNiagaraAsyncGpuTraceProviderGsdf(EShaderPlatform InShaderPlatform, FNiagaraGpuComputeDispatchInterface* Dispatcher);

	static bool IsSupported();
	virtual void PostRenderOpaque(FRHICommandList& RHICmdList, TConstArrayView<FViewInfo> Views, FCollisionGroupHashMap* CollisionGroupHash) override;
	virtual bool IsAvailable() const override;
	virtual void IssueTraces(FRHICommandList& RHICmdList, const FDispatchRequest& Request, FCollisionGroupHashMap* CollisionGroupHash) override;
	virtual void Reset() override;
	virtual EProviderType GetType() const override { return Type; }

private:
	FGlobalDistanceFieldParameterData m_DistanceFieldData;
	TUniformBufferRef<FViewUniformShaderParameters> m_ViewUniformBuffer;
};