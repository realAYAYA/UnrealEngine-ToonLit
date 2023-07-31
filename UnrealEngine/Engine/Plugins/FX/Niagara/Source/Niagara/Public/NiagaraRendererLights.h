// Copyright Epic Games, Inc. All Rights Reserved.

/*==============================================================================
NiagaraRendererLights.h: Renderer for rendering Niagara particles as Lights.
==============================================================================*/
#pragma once

#include "NiagaraRenderer.h"

/**
* NiagaraRendererLights renders an FNiagaraEmitterInstance as simple lights
*/
class NIAGARA_API FNiagaraRendererLights : public FNiagaraRenderer
{
public:
	struct SimpleLightData
	{
		FSimpleLightEntry LightEntry;
		FSimpleLightPerViewEntry PerViewEntry;
	};

	FNiagaraRendererLights(ERHIFeatureLevel::Type FeatureLevel, const UNiagaraRendererProperties *InProps, const FNiagaraEmitterInstance* Emitter);

	//FNiagaraRenderer interface
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View, const FNiagaraSceneProxy *SceneProxy)const override;
	virtual FNiagaraDynamicDataBase *GenerateDynamicData(const FNiagaraSceneProxy* Proxy, const UNiagaraRendererProperties* InProperties, const FNiagaraEmitterInstance* Emitter) const override;
	virtual void GatherSimpleLights(FSimpleLightArray& OutParticleLights)const override;
	//FNiagaraRenderer interface END
};


