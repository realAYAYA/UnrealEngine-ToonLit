// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	FXSystemSet.cpp: Internal redirector to several fx systems.
=============================================================================*/

#include "FXSystemSet.h"
#include "FXRenderingUtils.h"
#include "GPUSortManager.h"
#include "Containers/StridedView.h"

FFXSystemSet::FFXSystemSet(FGPUSortManager* InGPUSortManager)
	: GPUSortManager(InGPUSortManager)
{
}

FFXSystemInterface* FFXSystemSet::GetInterface(const FName& InName)
{
	for (FFXSystemInterface* FXSystem : FXSystems)
	{
		check(FXSystem);
		FXSystem = FXSystem->GetInterface(InName);
		if (FXSystem)
		{
			return FXSystem;
		}
	}
	return nullptr;
}

void FFXSystemSet::Tick(UWorld* World, float DeltaSeconds)
{
	for (FFXSystemInterface* FXSystem : FXSystems)
	{
		check(FXSystem);
		FXSystem->Tick(World, DeltaSeconds);
	}
}

#if WITH_EDITOR

void FFXSystemSet::Suspend()
{
	for (FFXSystemInterface* FXSystem : FXSystems)
	{
		check(FXSystem);
		FXSystem->Suspend();
	}
}

void FFXSystemSet::Resume()
{
	for (FFXSystemInterface* FXSystem : FXSystems)
	{
		check(FXSystem);
		FXSystem->Resume();
	}
}

#endif // #if WITH_EDITOR

void FFXSystemSet::DrawDebug(FCanvas* Canvas)
{
	for (FFXSystemInterface* FXSystem : FXSystems)
	{
		check(FXSystem);
		FXSystem->DrawDebug(Canvas);
	}
}

bool FFXSystemSet::ShouldDebugDraw_RenderThread() const
{
	for (const FFXSystemInterface* FXSystem : FXSystems)
	{
		check(FXSystem);
		if (FXSystem->ShouldDebugDraw_RenderThread())
		{
			return true;
		}
	}
	return false;
}

void FFXSystemSet::DrawDebug_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const struct FScreenPassRenderTarget& Output)
{
	for (FFXSystemInterface* FXSystem : FXSystems)
	{
		check(FXSystem);
		FXSystem->DrawDebug_RenderThread(GraphBuilder, View, Output);
	}
}

void FFXSystemSet::DrawSceneDebug_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, FRDGTextureRef SceneColor, FRDGTextureRef SceneDepth)
{
	for (FFXSystemInterface* FXSystem : FXSystems)
	{
		check(FXSystem);
		FXSystem->DrawSceneDebug_RenderThread(GraphBuilder, View, SceneColor, SceneDepth);
	}
}

void FFXSystemSet::AddVectorField(UVectorFieldComponent* VectorFieldComponent)
{
	for (FFXSystemInterface* FXSystem : FXSystems)
	{
		check(FXSystem);
		FXSystem->AddVectorField(VectorFieldComponent);
	}
}

void FFXSystemSet::RemoveVectorField(UVectorFieldComponent* VectorFieldComponent)
{
	for (FFXSystemInterface* FXSystem : FXSystems)
	{
		check(FXSystem);
		FXSystem->RemoveVectorField(VectorFieldComponent);
	}
}

void FFXSystemSet::UpdateVectorField(UVectorFieldComponent* VectorFieldComponent)
{
	for (FFXSystemInterface* FXSystem : FXSystems)
	{
		check(FXSystem);
		FXSystem->UpdateVectorField(VectorFieldComponent);
	}
}

void FFXSystemSet::PreInitViews(FRDGBuilder& GraphBuilder, bool bAllowGPUParticleUpdate, const TArrayView<const FSceneViewFamily*>& ViewFamilies, const FSceneViewFamily* CurrentFamily)
{
	for (FFXSystemInterface* FXSystem : FXSystems)
	{
		check(FXSystem);
		FXSystem->PreInitViews(GraphBuilder, bAllowGPUParticleUpdate, ViewFamilies, CurrentFamily);
	}
}

void FFXSystemSet::PostInitViews(FRDGBuilder& GraphBuilder, TConstStridedView<FSceneView> Views, bool bAllowGPUParticleUpdate)
{
	for (FFXSystemInterface* FXSystem : FXSystems)
	{
		check(FXSystem);
		FXSystem->PostInitViews(GraphBuilder, Views, bAllowGPUParticleUpdate);
	}
}

bool FFXSystemSet::UsesGlobalDistanceField() const
{
	for (FFXSystemInterface* FXSystem : FXSystems)
	{
		check(FXSystem);
		if (FXSystem->UsesGlobalDistanceField())
		{
			return true;
		}
	}
	return false;
}

bool FFXSystemSet::UsesDepthBuffer() const
{
	for (FFXSystemInterface* FXSystem : FXSystems)
	{
		check(FXSystem);
		if (FXSystem->UsesDepthBuffer())
		{
			return true;
		}
	}
	return false;
}

bool FFXSystemSet::RequiresEarlyViewUniformBuffer() const
{
	for (FFXSystemInterface* FXSystem : FXSystems)
	{
		check(FXSystem);
		if (FXSystem->RequiresEarlyViewUniformBuffer())
		{
			return true;
		}
	}
	return false;
}

bool FFXSystemSet::RequiresRayTracingScene() const
{
	for (FFXSystemInterface* FXSystem : FXSystems)
	{
		check(FXSystem);
		if (FXSystem->RequiresRayTracingScene())
		{
			return true;
		}
	}
	return false;
}

void FFXSystemSet::PreRender(FRDGBuilder& GraphBuilder, TConstStridedView<FSceneView> Views, FSceneUniformBuffer &SceneUniformBuffer, bool bAllowGPUParticleSceneUpdate)
{
	for (FFXSystemInterface* FXSystem : FXSystems)
	{
		check(FXSystem);
		FXSystem->PreRender(GraphBuilder, Views, SceneUniformBuffer, bAllowGPUParticleSceneUpdate);
	}
}

void FFXSystemSet::SetSceneTexturesUniformBuffer(const TUniformBufferRef<FSceneTextureUniformParameters>& InSceneTexturesUniformParams)
{
	for (FFXSystemInterface* FXSystem : FXSystems)
	{
		check(FXSystem);
		FXSystem->SetSceneTexturesUniformBuffer(InSceneTexturesUniformParams);
	}
}

void FFXSystemSet::PostRenderOpaque(FRDGBuilder& GraphBuilder, TConstStridedView<FSceneView> Views, FSceneUniformBuffer &SceneUniformBuffer, bool bAllowGPUParticleSceneUpdate)
{
	for (FFXSystemInterface* FXSystem : FXSystems)
	{
		check(FXSystem);
		FXSystem->PostRenderOpaque(GraphBuilder, Views, SceneUniformBuffer, bAllowGPUParticleSceneUpdate);
	}
}

void FFXSystemSet::OnDestroy()
{
	for (FFXSystemInterface*& FXSystem : FXSystems)
	{
		check(FXSystem);
		FXSystem->OnDestroy();
	}

	FFXSystemInterface::OnDestroy();
}


void FFXSystemSet::DestroyGPUSimulation()
{
	for (FFXSystemInterface*& FXSystem : FXSystems)
	{
		check(FXSystem);
		FXSystem->DestroyGPUSimulation();
	}
}

FFXSystemSet::~FFXSystemSet()
{
	for (FFXSystemInterface* FXSystem : FXSystems)
	{
		check(FXSystem);
		delete FXSystem;
	}
}

FGPUSortManager* FFXSystemSet::GetGPUSortManager() const
{
	return GPUSortManager;
}
