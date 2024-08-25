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
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override
	{
		// create a static vertex buffer
		FRHIResourceCreateInfo CreateInfo(TEXT("FNiagaraNullSortedIndicesVertexBuffer"));
		VertexBufferRHI = RHICmdList.CreateBuffer(sizeof(uint32), BUF_Static | BUF_VertexBuffer | BUF_ShaderResource, 0, ERHIAccess::VertexOrIndexBuffer | ERHIAccess::SRVMask, CreateInfo);
		void* BufferData = RHICmdList.LockBuffer(VertexBufferRHI, 0, sizeof(uint32), RLM_WriteOnly);
		FMemory::Memzero(BufferData, sizeof(uint32));
		RHICmdList.UnlockBuffer(VertexBufferRHI);

		VertexBufferSRV = RHICmdList.CreateShaderResourceView(VertexBufferRHI, sizeof(uint32), PF_R32_UINT);
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
	{
		bNeedsDeclaration = false;
	}
	
	static void ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FVertexFactory::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("NIAGARA_PARTICLE_FACTORY"), TEXT("1"));
	}

	ERHIFeatureLevel::Type GetFeatureLevel() const { check(HasValidFeatureLevel());  return FRenderResource::GetFeatureLevel(); }
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
