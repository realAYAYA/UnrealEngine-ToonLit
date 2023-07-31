// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "RenderResource.h"
#include "ShaderParameterStruct.h"
#include "ShaderParameterMacros.h"
#include "SceneRenderTargetParameters.h"

BEGIN_SHADER_PARAMETER_STRUCT(FNiagaraSceneTextureParameters, )
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTextures)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FMobileSceneTextureUniformParameters, MobileSceneTextures)
	RDG_TEXTURE_ACCESS(Depth, ERHIAccess::SRVCompute)
	RDG_TEXTURE_ACCESS(Normal, ERHIAccess::SRVCompute)
	RDG_TEXTURE_ACCESS(Velocity, ERHIAccess::SRVCompute)
END_SHADER_PARAMETER_STRUCT()

class FNiagaraRenderViewDataManager : public FRenderResource
{
public:
	FNiagaraRenderViewDataManager();

	static void Init();
	static void Shutdown();

	NIAGARA_API void PostOpaqueRender(FPostOpaqueRenderParameters& Params);

	void ClearSceneTextureParameters();

	void GetSceneTextureParameters(FRDGBuilder& GraphBuilder, const FSceneTextures* SceneTextures, FNiagaraSceneTextureParameters& InParameters) const;

	virtual void InitDynamicRHI() override;

	virtual void ReleaseDynamicRHI() override;

private:
	FNiagaraSceneTextureParameters Parameters;
	FRHIUniformBuffer* ViewUniformBuffer = nullptr;

	FPostOpaqueRenderDelegate PostOpaqueDelegate;
	FDelegateHandle PostOpaqueDelegateHandle;
};

extern TGlobalResource<FNiagaraRenderViewDataManager> GNiagaraViewDataManager;

