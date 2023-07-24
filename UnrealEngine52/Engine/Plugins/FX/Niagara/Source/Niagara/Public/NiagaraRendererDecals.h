// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraRenderer.h"

class FDeferredDecalProxy;
class USceneComponent;

class NIAGARA_API FNiagaraRendererDecals : public FNiagaraRenderer
{
public:
	explicit FNiagaraRendererDecals(ERHIFeatureLevel::Type FeatureLevel, const UNiagaraRendererProperties *InProps, const FNiagaraEmitterInstance* Emitter);
	~FNiagaraRendererDecals();

	void ReleaseAllDecals() const;

	//FNiagaraRenderer interface
	virtual void DestroyRenderState_Concurrent() override;
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View, const FNiagaraSceneProxy *SceneProxy)const override;
	virtual FNiagaraDynamicDataBase *GenerateDynamicData(const FNiagaraSceneProxy* Proxy, const UNiagaraRendererProperties* InProperties, const FNiagaraEmitterInstance* Emitter) const override;
	//FNiagaraRenderer interface END

	mutable TWeakObjectPtr<USceneComponent>		WeakOwnerComponent;
	mutable TWeakObjectPtr<UMaterialInterface>	WeakMaterial;
	mutable TArray<FDeferredDecalProxy*>		ActiveDecalProxies;
};
