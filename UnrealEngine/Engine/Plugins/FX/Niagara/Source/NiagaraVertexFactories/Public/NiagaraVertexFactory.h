// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ParticleVertexFactory.h: Particle vertex factory definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RenderResource.h"
#include "UniformBuffer.h"
#include "VertexFactory.h"
#include "SceneView.h"

class FMaterial;


class FNiagaraNullSortedIndicesVertexBuffer : public FVertexBuffer
{
public:
	/**
	* Initialize the RHI for this rendering resource
	*/
	virtual void InitRHI() override
	{
		// create a static vertex buffer
		FRHIResourceCreateInfo CreateInfo(TEXT("FNiagaraNullSortedIndicesVertexBuffer"));
		VertexBufferRHI = RHICreateBuffer(sizeof(uint32), BUF_Static | BUF_VertexBuffer | BUF_ShaderResource, 0, ERHIAccess::VertexOrIndexBuffer | ERHIAccess::SRVMask, CreateInfo);
		void* BufferData = RHILockBuffer(VertexBufferRHI, 0, sizeof(uint32), RLM_WriteOnly);
		FMemory::Memzero(BufferData, sizeof(uint32));
		RHIUnlockBuffer(VertexBufferRHI);

		VertexBufferSRV = RHICreateShaderResourceView(VertexBufferRHI, sizeof(uint32), PF_R32_UINT);
	}

	virtual void ReleaseRHI() override
	{
		VertexBufferSRV.SafeRelease();
		FVertexBuffer::ReleaseRHI();
	}

	FShaderResourceViewRHIRef VertexBufferSRV;
};
extern NIAGARAVERTEXFACTORIES_API TGlobalResource<FNiagaraNullSortedIndicesVertexBuffer> GFNiagaraNullSortedIndicesVertexBuffer;

/**
* Enum identifying the type of a particle vertex factory.
*/
enum ENiagaraVertexFactoryType
{
	NVFT_Sprite,
	NVFT_Ribbon,
	NVFT_Mesh,
	NVFT_MAX
};

/**
* Base class for particle vertex factories.
*/
class FNiagaraVertexFactoryBase : public FVertexFactory
{
public:

	/** Default constructor. */
	explicit FNiagaraVertexFactoryBase(ENiagaraVertexFactoryType Type, ERHIFeatureLevel::Type InFeatureLevel)
		: FVertexFactory(InFeatureLevel)
		, LastFrameSetup(MAX_uint32)
		, LastViewFamily(nullptr)
		, LastView(nullptr)
		, LastFrameRealTime(-1.0f)
		, ParticleFactoryType(Type)
		, bInUse(false)
	{
		bNeedsDeclaration = false;
	}
	
	static void ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FVertexFactory::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("NIAGARA_PARTICLE_FACTORY"), TEXT("1"));
	}

	/** Return the vertex factory type */
	FORCEINLINE ENiagaraVertexFactoryType GetParticleFactoryType() const
	{
		return ParticleFactoryType;
	}

	inline void SetParticleFactoryType(ENiagaraVertexFactoryType InType)
	{
		ParticleFactoryType = InType;
	}

	/** Specify whether the factory is in use or not. */
	FORCEINLINE void SetInUse(bool bInInUse)
	{
		bInUse = bInInUse;
	}

	/** Return the vertex factory type */
	FORCEINLINE bool GetInUse() const
	{
		return bInUse;
	}

	ERHIFeatureLevel::Type GetFeatureLevel() const { check(HasValidFeatureLevel());  return FRenderResource::GetFeatureLevel(); }

	bool CheckAndUpdateLastFrame(const FSceneViewFamily& ViewFamily, const FSceneView *View = nullptr) const
	{
		if (LastFrameSetup != MAX_uint32 && (&ViewFamily == LastViewFamily) && (View == LastView) && ViewFamily.FrameNumber == LastFrameSetup && LastFrameRealTime == ViewFamily.Time.GetRealTimeSeconds())
		{
			return false;
		}
		LastFrameSetup = ViewFamily.FrameNumber;
		LastFrameRealTime = ViewFamily.Time.GetRealTimeSeconds();
		LastViewFamily = &ViewFamily;
		LastView = View;
		return true;
	}

private:
	/** Last state where we set this. We only need to setup these once per frame, so detemine same frame by number, time, and view family. */
	mutable uint32 LastFrameSetup;
	mutable const FSceneViewFamily *LastViewFamily;
	mutable const FSceneView *LastView;
	mutable float LastFrameRealTime;

	/** The type of the vertex factory. */
	ENiagaraVertexFactoryType ParticleFactoryType;

	/** Whether the vertex factory is in use. */
	bool bInUse;
};

/**
* Base class for Niagara vertex factory shader parameters.
*/
class FNiagaraVertexFactoryShaderParametersBase : public FVertexFactoryShaderParameters
{
	DECLARE_TYPE_LAYOUT(FNiagaraVertexFactoryShaderParametersBase, NonVirtual);

public:
	void Bind(const FShaderParameterMap& ParameterMap);
	void GetElementShaderBindings(
		const FSceneInterface* Scene,
		const FSceneView* View,
		const FMeshMaterialShader* Shader,
		const EVertexInputStreamType VertexStreamType,
		ERHIFeatureLevel::Type FeatureLevel,
		const FVertexFactory* VertexFactory,
		const FMeshBatchElement& BatchElement,
		class FMeshDrawSingleShaderBindings& ShaderBindings,
		FVertexInputStreamArray& VertexStreams) const;
};
