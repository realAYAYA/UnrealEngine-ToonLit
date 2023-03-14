// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "RenderResource.h"
#include "UniformBuffer.h"
#include "VertexFactory.h"
#include "Containers/DynamicRHIResourceArray.h"
#include "SceneManagement.h"
#include "WaterInstanceDataBuffer.h"

/**
 * Uniform buffer to hold parameters specific to this vertex factory. Only set up once
 */
BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FWaterVertexFactoryParameters, )
	SHADER_PARAMETER(float, LODScale)
	SHADER_PARAMETER(int32, NumQuadsPerTileSide)
	SHADER_PARAMETER(int32, bRenderSelected)
	SHADER_PARAMETER(int32, bRenderUnselected)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

typedef TUniformBufferRef<FWaterVertexFactoryParameters> FWaterVertexFactoryBufferRef;

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FWaterVertexFactoryRaytracingParameters, )
	SHADER_PARAMETER_SRV(Buffer<float>, VertexBuffer)
	SHADER_PARAMETER(FVector4f, InstanceData0)
	SHADER_PARAMETER(FVector4f, InstanceData1)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

typedef TUniformBufferRef<FWaterVertexFactoryRaytracingParameters> FWaterVertexFactoryRaytracingParametersRef;

class FWaterMeshIndexBuffer : public FIndexBuffer
{
public:

	FWaterMeshIndexBuffer(int32 InNumQuadsPerSide) : NumQuadsPerSide(InNumQuadsPerSide) {}

	void InitRHI() override
	{
		// This is an optimized index buffer path for water tiles containing less than uint16 max vertices
		if (NumQuadsPerSide < 256)
		{
			IndexBufferRHI = CreateIndexBuffer<uint16>();
		}
		else
		{
			IndexBufferRHI = CreateIndexBuffer<uint32>();
		}
	}

	int32 GetIndexCount() const { return NumIndices; };

private:
	template <typename IndexType>
	FBufferRHIRef CreateIndexBuffer()
	{
		TResourceArray<IndexType, INDEXBUFFER_ALIGNMENT> Indices;

		// Allocate room for indices
		Indices.Reserve(NumQuadsPerSide * NumQuadsPerSide * 6);

		// Build index buffer in morton order for better vertex reuse. This amounts to roughly 75% reuse rate vs 66% of naive scanline approach
		for (int32 Morton = 0; Morton < NumQuadsPerSide * NumQuadsPerSide; Morton++)
		{
			int32 SquareX = FMath::ReverseMortonCode2(Morton);
			int32 SquareY = FMath::ReverseMortonCode2(Morton >> 1);

			bool ForwardDiagonal = false;

			if (SquareX % 2)
			{
				ForwardDiagonal = !ForwardDiagonal;
			}
			if (SquareY % 2)
			{
				ForwardDiagonal = !ForwardDiagonal;
			}

			int32 Index0 = SquareX + SquareY * (NumQuadsPerSide + 1);
			int32 Index1 = Index0 + 1;
			int32 Index2 = Index0 + (NumQuadsPerSide + 1);
			int32 Index3 = Index2 + 1;

			Indices.Add(Index3);
			Indices.Add(Index1);
			Indices.Add(ForwardDiagonal ? Index2 : Index0);
			Indices.Add(Index0);
			Indices.Add(Index2);
			Indices.Add(ForwardDiagonal ? Index1 : Index3);
		}

		NumIndices = Indices.Num();
		const uint32 Size = Indices.GetResourceDataSize();
		const uint32 Stride = sizeof(IndexType);

		// Create index buffer. Fill buffer with initial data upon creation
		FRHIResourceCreateInfo CreateInfo(TEXT("FWaterMeshIndexBuffer"), &Indices);
		return RHICreateIndexBuffer(Stride, Size, BUF_Static, CreateInfo);
	}

	int32 NumIndices = 0;
	const int32 NumQuadsPerSide = 0;
};


class FWaterMeshVertexBuffer : public FVertexBuffer
{
public:

	FWaterMeshVertexBuffer(int32 InNumQuadsPerSide) : NumQuadsPerSide(InNumQuadsPerSide) {}

	virtual void InitRHI() override
	{
		ensureAlways(NumQuadsPerSide > 0);
		const uint32 NumVertsPerSide = NumQuadsPerSide + 1;

		NumVerts = NumVertsPerSide * NumVertsPerSide;

		FRHIResourceCreateInfo CreateInfo(TEXT("FWaterMeshVertexBuffer"));
		VertexBufferRHI = RHICreateBuffer(sizeof(FVector4f) * NumVerts, BUF_Static | BUF_VertexBuffer | BUF_ShaderResource, 0, ERHIAccess::VertexOrIndexBuffer | ERHIAccess::SRVMask, CreateInfo);
		FVector4f* DummyContents = (FVector4f*)RHILockBuffer(VertexBufferRHI, 0, sizeof(FVector4f) * NumVerts, RLM_WriteOnly);

		for (uint32 VertY = 0; VertY < NumVertsPerSide; VertY++)
		{
			FVector4f VertPos;
			VertPos.Y = (float)VertY / NumQuadsPerSide - 0.5f;

			for (uint32 VertX = 0; VertX < NumVertsPerSide; VertX++)
			{
				VertPos.X = (float)VertX / NumQuadsPerSide - 0.5f;

				DummyContents[NumVertsPerSide * VertY + VertX] = VertPos;
			}
		}

		RHIUnlockBuffer(VertexBufferRHI);

		SRV = RHICreateShaderResourceView(VertexBufferRHI, sizeof(float), PF_R32_FLOAT);
	}

	virtual void ReleaseRHI() override
	{
		SRV.SafeRelease();
		FVertexBuffer::ReleaseRHI();
	}

	int32 GetVertexCount() const { return NumVerts; }
	FRHIShaderResourceView* GetSRV() { return SRV; }

private:
	int32 NumVerts = 0;
	const int32 NumQuadsPerSide = 0;

	FShaderResourceViewRHIRef SRV;
};

enum class EWaterMeshRenderGroupType : uint8
{
	RG_RenderWaterTiles = 0,				// Render all water bodies

#if WITH_WATER_SELECTION_SUPPORT
	RG_RenderSelectedWaterTilesOnly,		// Render only selected water bodies
	RG_RenderUnselectedWaterTilesOnly,		// Render only unselected water bodies
#endif // WITH_WATER_SELECTION_SUPPORT
};

template <bool bWithWaterSelectionSupport>
class TWaterVertexFactory : public FVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(TWaterVertexFactory<bWithWaterSelectionSupport>);

public:
	using Super = FVertexFactory;

	static constexpr int32 NumRenderGroups = bWithWaterSelectionSupport ? 3 : 1; // Must match EWaterMeshRenderGroupType
	static constexpr int32 NumAdditionalVertexStreams = TWaterInstanceDataBuffers<bWithWaterSelectionSupport>::NumBuffers;

	TWaterVertexFactory(ERHIFeatureLevel::Type InFeatureLevel, int32 InNumQuadsPerSide,	float InLODScale);
	~TWaterVertexFactory();

	/**
	* Constructs render resources for this vertex factory.
	*/
	virtual void InitRHI() override;

	/**
	* Release render resources for this vertex factory.
	*/
	virtual void ReleaseRHI() override;

	/**
	 * Should we cache the material's shader type on this platform with this vertex factory?
	 */
	static bool ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);

	static void ValidateCompiledResult(const FVertexFactoryType* Type, EShaderPlatform Platform, const FShaderParameterMap& ParameterMap, TArray<FString>& OutErrors);

	static void GetPSOPrecacheVertexFetchElements(EVertexInputStreamType VertexInputStreamType, FVertexDeclarationElementList& Elements);

	inline const FUniformBufferRHIRef GetWaterVertexFactoryUniformBuffer(EWaterMeshRenderGroupType InRenderGroupType) const { return UniformBuffers[(int32)InRenderGroupType]; }

private:
	void SetupUniformDataForGroup(EWaterMeshRenderGroupType InRenderGroupType);

public:
	FWaterMeshVertexBuffer* VertexBuffer = nullptr;
	FWaterMeshIndexBuffer* IndexBuffer = nullptr;

private:
	TStaticArray<FWaterVertexFactoryBufferRef, NumRenderGroups> UniformBuffers;

	const int32 NumQuadsPerSide = 0;
	const float LODScale = 0.0f;
};

extern const FVertexFactoryType* GetWaterVertexFactoryType(bool bWithWaterSelectionSupport);


/**
 * Water user data provided to FMeshBatchElement(s)
 */
template <bool bWithWaterSelectionSupport>
struct TWaterMeshUserData
{
	TWaterMeshUserData() = default;

	TWaterMeshUserData(EWaterMeshRenderGroupType InRenderGroupType, const TWaterInstanceDataBuffers<bWithWaterSelectionSupport>* InInstanceDataBuffers)
		: RenderGroupType(InRenderGroupType)
		, InstanceDataBuffers(InInstanceDataBuffers)
	{
	}

	EWaterMeshRenderGroupType RenderGroupType = EWaterMeshRenderGroupType::RG_RenderWaterTiles;
	const TWaterInstanceDataBuffers<bWithWaterSelectionSupport>* InstanceDataBuffers = nullptr;

#if RHI_RAYTRACING	
	FUniformBufferRHIRef WaterVertexFactoryRaytracingVFUniformBuffer = nullptr;
#endif
};

/**
 * List of per-"water render group" user data buffers : 
 */
template <bool bWithWaterSelectionSupport>
struct TWaterMeshUserDataBuffers
{
	using WaterMeshUserDataType = TWaterMeshUserData<bWithWaterSelectionSupport>;

	TWaterMeshUserDataBuffers(const TWaterInstanceDataBuffers<bWithWaterSelectionSupport>* InInstanceDataBuffers)
	{
		int32 Index = 0;
		UserData[Index++] = MakeUnique<WaterMeshUserDataType>(EWaterMeshRenderGroupType::RG_RenderWaterTiles, InInstanceDataBuffers);

#if WITH_WATER_SELECTION_SUPPORT
		if (bWithWaterSelectionSupport)
		{
			UserData[Index++] = MakeUnique<WaterMeshUserDataType>(EWaterMeshRenderGroupType::RG_RenderSelectedWaterTilesOnly, InInstanceDataBuffers);
			UserData[Index++] = MakeUnique<WaterMeshUserDataType>(EWaterMeshRenderGroupType::RG_RenderUnselectedWaterTilesOnly, InInstanceDataBuffers);
		}
#endif // WITH_WATER_SELECTION_SUPPORT
	}

	const WaterMeshUserDataType* GetUserData(EWaterMeshRenderGroupType InRenderGroupType)
	{
		return UserData[(int32)InRenderGroupType].Get();
	}

	TStaticArray<TUniquePtr<WaterMeshUserDataType>, TWaterVertexFactory<bWithWaterSelectionSupport>::NumRenderGroups> UserData;
};

#include "WaterVertexFactory.inl"
