// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraRenderViewDataManager.h"

#include "EngineModule.h"
#include "SceneInterface.h"
#include "SceneRenderTargetParameters.h"

TGlobalResource<FNiagaraRenderViewDataManager> GNiagaraViewDataManager;

void FNiagaraRenderViewDataManager::PostOpaqueRender(FPostOpaqueRenderParameters& Params)
{
	ViewUniformBuffer = Params.ViewUniformBuffer;
	Parameters.SceneTextures = Params.SceneTexturesUniformParams;
	Parameters.MobileSceneTextures = Params.MobileSceneTexturesUniformParams;
	Parameters.Depth = Params.DepthTexture;
	Parameters.Normal = Params.NormalTexture;
	Parameters.Velocity = Params.VelocityTexture;
}

void FNiagaraRenderViewDataManager::GetSceneTextureParameters(FRDGBuilder& GraphBuilder, const FSceneTextures* SceneTextures, FNiagaraSceneTextureParameters& OutParameters) const
{
	OutParameters = Parameters;

	ERHIFeatureLevel::Type LocalFeatureLevel = GetFeatureLevel();
	if (FSceneInterface::GetShadingPath(LocalFeatureLevel) == EShadingPath::Deferred)
	{
		if ( !Parameters.SceneTextures )
		{
			OutParameters.SceneTextures = CreateSceneTextureUniformBuffer(GraphBuilder, SceneTextures, LocalFeatureLevel, ESceneTextureSetupMode::SceneVelocity);
		}
		OutParameters.MobileSceneTextures = nullptr;
		OutParameters.Depth = Parameters.Depth;
		OutParameters.Normal = Parameters.Normal;
		OutParameters.Velocity = Parameters.Velocity;
	}
	else if (FSceneInterface::GetShadingPath(LocalFeatureLevel) == EShadingPath::Mobile)
	{
		if ( !Parameters.MobileSceneTextures )
		{
			OutParameters.MobileSceneTextures = CreateMobileSceneTextureUniformBuffer(GraphBuilder, SceneTextures, EMobileSceneTextureSetupMode::None);
		}
		OutParameters.SceneTextures = nullptr;
		// These will not work correctly, all scene textures should be accessed through SceneTextures UB
		OutParameters.Depth = nullptr;
		OutParameters.Normal = nullptr;
		OutParameters.Velocity = nullptr;
	}
}

void FNiagaraRenderViewDataManager::ClearSceneTextureParameters()
{
	Parameters = {};
}

FNiagaraRenderViewDataManager::FNiagaraRenderViewDataManager()
	: FRenderResource()
{

}

void FNiagaraRenderViewDataManager::Init()
{
	IRendererModule& RendererModule = GetRendererModule();

	GNiagaraViewDataManager.PostOpaqueDelegate.BindRaw(&GNiagaraViewDataManager, &FNiagaraRenderViewDataManager::PostOpaqueRender);
	GNiagaraViewDataManager.PostOpaqueDelegateHandle = RendererModule.RegisterPostOpaqueRenderDelegate(GNiagaraViewDataManager.PostOpaqueDelegate);
}

void FNiagaraRenderViewDataManager::Shutdown()
{
	IRendererModule& RendererModule = GetRendererModule();

	RendererModule.RemovePostOpaqueRenderDelegate(GNiagaraViewDataManager.PostOpaqueDelegateHandle);
	GNiagaraViewDataManager.ReleaseDynamicRHI();
}

void FNiagaraRenderViewDataManager::InitDynamicRHI()
{

}

void FNiagaraRenderViewDataManager::ReleaseDynamicRHI()
{
}
