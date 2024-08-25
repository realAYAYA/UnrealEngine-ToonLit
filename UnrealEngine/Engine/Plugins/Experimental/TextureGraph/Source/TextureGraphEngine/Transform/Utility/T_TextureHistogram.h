// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "ShaderCompilerCore.h"
#include "Job/Job.h"
#include "FxMat/FxMaterial.h"
#include "Model/Mix/MixUpdateCycle.h"
#include <DataDrivenShaderPlatformInfo.h>

class CSH_Histogram : public CmpSH_Base<16, 16, 1>
{
public:
	DECLARE_GLOBAL_SHADER(CSH_Histogram);
	SHADER_USE_PARAMETER_STRUCT(CSH_Histogram, CmpSH_Base);
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_TEXTURE(Texture2D, SourceTiles)
		SHADER_PARAMETER_UAV(RWTexture2D<float4>, Result)
		SHADER_PARAMETER_UAV(RWStructuredBuffer<uint4>, Histogram)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsPCPlatform(Parameters.Platform) && EnumHasAllFlags(Parameters.Flags, EShaderPermutationFlags::HasEditorOnlyData)
			&& IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

template <typename CmpSH_Type>
class FxMaterial_Histogram : public FxMaterial_Compute<CmpSH_Type>
{
	FRWBufferStructured Buffer;
public:
	// type Name for the permutation domain
	using CmpSHPermutationDomain = typename CmpSH_Type::FPermutationDomain;

	 FxMaterial_Histogram(FString outputId, const CmpSHPermutationDomain* permDomain = nullptr,
		int numThreadsX = FxMaterial_Compute<CmpSH_Type>::GDefaultNumThreadsXY, int numThreadsY = FxMaterial_Compute<CmpSH_Type>::GDefaultNumThreadsXY, int numThreadsZ = 1,
		FUnorderedAccessViewRHIRef unorderedAccessView = nullptr)
		: FxMaterial_Compute<CmpSH_Type>(outputId, permDomain, numThreadsX, numThreadsY, numThreadsZ, unorderedAccessView)
	{

	}

	virtual void Blit(FRHICommandListImmediate& RHI, FRHITexture2D* Target, const RenderMesh* MeshObj, int32 TargetId, FGraphicsPipelineStateInitializer* PSO = nullptr) override
	{
		SetBuffer(RHI);

		FxMaterial_Compute<CmpSH_Type>::Blit(RHI, Target, MeshObj, TargetId, PSO);
	}

	void SetBuffer(FRHICommandListImmediate& RHI)
	{
		Buffer.Initialize(RHI, TEXT("HistogramRBuffer"), sizeof(uint32) * 4, 256, BUF_UnorderedAccess | BUF_ShaderResource);
		RHI.ClearUAVUint(Buffer.UAV, FUintVector4(0, 0, 0, 0));
		FxMaterial_Compute<CmpSH_Type>::Params.Histogram = Buffer.UAV;
	}
};

/**
 * T_TextureHistogram Transform
 */
class TEXTUREGRAPHENGINE_API T_TextureHistogram
{
public:
	T_TextureHistogram();
	~T_TextureHistogram();

	static TiledBlobPtr	Create(MixUpdateCyclePtr Cycle, TiledBlobPtr SourceTex, int32 TargetId);

	static TiledBlobPtr	CreateOnService(UMixInterface* InMix, TiledBlobPtr SourceTex, int32 TargetId);

private:
	static RenderMaterial_FXPtr CreateMaterial_Histogram(FString Name, FString OutputId, const CSH_Histogram::FPermutationDomain& cmpshPermutationDomain,int NumThreadsX, int NumThreadsY, int NumThreadsZ = 1);
	static TiledBlobPtr			CreateJobAndResult(JobUPtr& OutJob, MixUpdateCyclePtr Cycle, TiledBlobPtr Histogram, int32 TargetId);
};
