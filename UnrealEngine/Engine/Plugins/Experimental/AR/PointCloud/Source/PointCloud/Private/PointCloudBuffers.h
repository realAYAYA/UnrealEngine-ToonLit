// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ResourceArray.h"
#include "RenderResource.h"
#include "RHI.h"
#include "RHICommandList.h"

class FPointCloudVertexResourceArray :
	public FResourceArrayInterface
{
public:
	FPointCloudVertexResourceArray(const void* InData, uint32 InSize)
		: Data(InData)
		, Size(InSize)
	{
	}

	virtual const void* GetResourceData() const override { return Data; }
	virtual uint32 GetResourceDataSize() const override { return Size; }
	virtual void Discard() override { }
	virtual bool IsStatic() const override { return false; }
	virtual bool GetAllowCPUAccess() const override { return false; }
	virtual void SetAllowCPUAccess(bool bInNeedsCPUAccess) override { }

private:
	const void* Data;
	uint32 Size;
};

/** Point cloud vertex buffer that can hold an arbitrary single data type (color or position) */
class FPointCloudVertexBufferBase :
	public FVertexBuffer
{
public:
	inline uint32 GetNumVerts() const
	{
		return NumVerts;
	}

	inline FShaderResourceViewRHIRef GetBufferSRV() const
	{
		return BufferSRV;
	}

protected:
	void InitWith(FRHICommandListBase& RHICmdList, const void* InVertexData, uint32 InNumVerts, uint32 InSizeInBytes)
	{
		InitResource(RHICmdList);

		NumVerts = InNumVerts;

		FPointCloudVertexResourceArray ResourceArray(InVertexData, InSizeInBytes);
		FRHIResourceCreateInfo CreateInfo(TEXT("FPointCloudVertexBufferBase"), &ResourceArray);
		VertexBufferRHI = RHICmdList.CreateVertexBuffer(InSizeInBytes, BUF_Static | BUF_ShaderResource, CreateInfo);
	}

	uint32 NumVerts = 0;
	FShaderResourceViewRHIRef BufferSRV;
};

/** Point cloud color buffer that sets the SRV too */
class FPointCloudColorVertexBuffer :
	public FPointCloudVertexBufferBase
{
public:
	void InitRHIWith(FRHICommandListBase& RHICmdList, const TArray<FColor>& RawColorData)
	{
		InitWith(RHICmdList, RawColorData.GetData(), (uint32)RawColorData.Num(), RawColorData.Num() * sizeof(FColor));
		BufferSRV = RHICmdList.CreateShaderResourceView(VertexBufferRHI, sizeof(FColor), PF_R8G8B8A8);
		// We have a full stream so we can index safely
		ColorMask = ~0;
	}

	void InitRHIWith(FRHICommandListBase& RHICmdList, const FColor& RawColor)
	{
		InitWith(RHICmdList, (const void*)&RawColor, 1, sizeof(FColor));
		BufferSRV = RHICmdList.CreateShaderResourceView(VertexBufferRHI, sizeof(FColor), PF_R8G8B8A8);
	}

	inline uint32 GetColorMask() const
	{
		return ColorMask;
	}

private:
	uint32 ColorMask = 0;
};

/** Point cloud location buffer that sets the SRV too */
class FPointCloudLocationVertexBuffer :
	public FPointCloudVertexBufferBase
{
public:
	void InitRHIWith(FRHICommandListBase& RHICmdList, const TArray<FVector>& RawLocationData)
	{
		InitWith(RHICmdList, RawLocationData.GetData(), RawLocationData.Num(), RawLocationData.Num() * sizeof(FVector));
		BufferSRV = RHICmdList.CreateShaderResourceView(VertexBufferRHI, sizeof(float), PF_R32_FLOAT);
	}
};

/**
 * We generate a index buffer for N points in the point cloud section. Each point
 * generates the vert positions using vertex id and point id (vertex id / 4)
 * by fetching the points and colors from the buffers
 */
class FPointCloudIndexBuffer :
	public FIndexBuffer
{
public:
	FPointCloudIndexBuffer()
		: NumPoints(0)
		, NumPrimitives(0)
		, MaxIndex(0)
		, bIsQuadList(false)
	{
	}

	FPointCloudIndexBuffer(uint32 InNumPoints)
		: NumPoints(InNumPoints)
		, NumPrimitives(0)
		, MaxIndex(4 * InNumPoints)
		, bIsQuadList(false)
	{
	}

	/** Generates a quad list when available on the platform */
	template <typename INDEX_TYPE>
	void CreateQuadList(FRHICommandListBase& RHICmdList)
	{
		bIsQuadList = true;
		NumPrimitives = NumPoints;
		const uint32 Size = sizeof(INDEX_TYPE) * 4 * NumPoints;
		const uint32 Stride = sizeof(INDEX_TYPE);
		FRHIResourceCreateInfo CreateInfo(TEXT("PointCloudQuadList"));
		IndexBufferRHI = RHICmdList.CreateBuffer( Size, BUF_Static | BUF_IndexBuffer, Stride, ERHIAccess::VertexOrIndexBuffer, CreateInfo );
		INDEX_TYPE* Indices = (INDEX_TYPE*)RHICmdList.LockBuffer( IndexBufferRHI, 0, Size, RLM_WriteOnly );
		for (uint32 SpriteIndex = 0; SpriteIndex < NumPoints; ++SpriteIndex)
		{
			Indices[SpriteIndex * 4 + 0] = SpriteIndex * 4 + 0;
			Indices[SpriteIndex * 4 + 1] = SpriteIndex * 4 + 1;
			Indices[SpriteIndex * 4 + 2] = SpriteIndex * 4 + 3;
			Indices[SpriteIndex * 4 + 3] = SpriteIndex * 4 + 2;
		}
		RHICmdList.UnlockBuffer( IndexBufferRHI );
	}

	/** Generates a tri list when quad lists are not available on the platform */
	template <typename INDEX_TYPE>
	void CreateTriList(FRHICommandListBase& RHICmdList)
	{
		NumPrimitives = 2 * NumPoints;
		const uint32 Size = sizeof(INDEX_TYPE) * 6 * NumPoints;
		const uint32 Stride = sizeof(INDEX_TYPE);
		FRHIResourceCreateInfo CreateInfo(TEXT("PointCloudTriList"));
		IndexBufferRHI = RHICmdList.CreateBuffer( Size, BUF_Static | BUF_IndexBuffer, Stride, ERHIAccess::VertexOrIndexBuffer, CreateInfo );
		INDEX_TYPE* Indices = (INDEX_TYPE*)RHICmdList.LockBuffer( IndexBufferRHI, 0, Size, RLM_WriteOnly );
		for (uint32 SpriteIndex = 0; SpriteIndex < NumPoints; ++SpriteIndex)
		{
			Indices[SpriteIndex * 6 + 0] = SpriteIndex * 4 + 0;
			Indices[SpriteIndex * 6 + 1] = SpriteIndex * 4 + 3;
			Indices[SpriteIndex * 6 + 2] = SpriteIndex * 4 + 2;
			Indices[SpriteIndex * 6 + 3] = SpriteIndex * 4 + 0;
			Indices[SpriteIndex * 6 + 4] = SpriteIndex * 4 + 1;
			Indices[SpriteIndex * 6 + 5] = SpriteIndex * 4 + 3;
		}
		RHICmdList.UnlockBuffer( IndexBufferRHI );
	}

	virtual void InitRHI(FRHICommandListBase& RHICmdList) override
	{
		check(NumPoints > 0 && MaxIndex > 0);
//@todo joeg - cvar quad vs tri in addition to platform support
		const bool bShouldUseQuadList = GRHISupportsQuadTopology;
		// Use 32 bit indices if needed
		if (NumPoints * 4 > 65535)
		{
			if (bShouldUseQuadList)
			{
				CreateQuadList<uint32>(RHICmdList);
			}
			else
			{
				CreateTriList<uint32>(RHICmdList);
			}
		}
		else
		{
			if (bShouldUseQuadList)
			{
				CreateQuadList<uint16>(RHICmdList);
			}
			else
			{
				CreateTriList<uint16>(RHICmdList);
			}
		}
	}

	void InitRHIWithSize(FRHICommandListBase& RHICmdList, uint32 InNumPoints)
	{
		NumPoints = InNumPoints;
		MaxIndex = (4 * InNumPoints) - 1;
		InitResource(RHICmdList);
	}

	inline bool IsQuadList() const
	{
		return bIsQuadList;
	}

	inline bool IsTriList() const
	{
		return !IsQuadList();
	}

	inline uint32 GetNumPrimitives() const
	{
		return NumPrimitives;
	}

	inline uint32 GetMaxIndex() const
	{
		return MaxIndex;
	}

private:
	uint32 NumPoints;
	uint32 NumPrimitives;
	uint32 MaxIndex;
	bool bIsQuadList;
};
