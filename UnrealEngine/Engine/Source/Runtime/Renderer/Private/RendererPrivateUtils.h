// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

class FTileTexCoordVertexBuffer : public FVertexBuffer
{
public:
	FTileTexCoordVertexBuffer(int32 InNumTileQuadsInBuffer)
		: NumTileQuadsInBuffer(InNumTileQuadsInBuffer)
	{
	}

	virtual void InitRHI() override
	{
		const uint32 Size = sizeof(FVector2f) * 4 * NumTileQuadsInBuffer;
		FRHIResourceCreateInfo CreateInfo(TEXT("FTileTexCoordVertexBuffer"));
		VertexBufferRHI = RHICreateBuffer(Size, BUF_Static | BUF_VertexBuffer, 0, ERHIAccess::VertexOrIndexBuffer, CreateInfo);
		FVector2f* Vertices = (FVector2f*)RHILockBuffer(VertexBufferRHI, 0, Size, RLM_WriteOnly);
		for (uint32 SpriteIndex = 0; SpriteIndex < NumTileQuadsInBuffer; ++SpriteIndex)
		{
			Vertices[SpriteIndex * 4 + 0] = FVector2f(0.0f, 0.0f);
			Vertices[SpriteIndex * 4 + 1] = FVector2f(0.0f, 1.0f);
			Vertices[SpriteIndex * 4 + 2] = FVector2f(1.0f, 1.0f);
			Vertices[SpriteIndex * 4 + 3] = FVector2f(1.0f, 0.0f);
		}
		RHIUnlockBuffer(VertexBufferRHI);
	}

	const uint32 NumTileQuadsInBuffer;
};

class FTileIndexBuffer : public FIndexBuffer
{
public:
	FTileIndexBuffer(int32 InNumTileQuadsInBuffer)
		: NumTileQuadsInBuffer(InNumTileQuadsInBuffer)
	{
	}

	/** Initialize the RHI for this rendering resource */
	void InitRHI() override
	{
		const uint32 Size = sizeof(uint16) * 6 * NumTileQuadsInBuffer;
		const uint32 Stride = sizeof(uint16);
		FRHIResourceCreateInfo CreateInfo(TEXT("FTileIndexBuffer"));
		IndexBufferRHI = RHICreateBuffer(Size, BUF_Static | BUF_IndexBuffer, Stride, ERHIAccess::VertexOrIndexBuffer, CreateInfo);
		uint16* Indices = (uint16*)RHILockBuffer(IndexBufferRHI, 0, Size, RLM_WriteOnly);
		for (uint32 SpriteIndex = 0; SpriteIndex < NumTileQuadsInBuffer; ++SpriteIndex)
		{
			Indices[SpriteIndex * 6 + 0] = SpriteIndex * 4 + 0;
			Indices[SpriteIndex * 6 + 1] = SpriteIndex * 4 + 1;
			Indices[SpriteIndex * 6 + 2] = SpriteIndex * 4 + 2;
			Indices[SpriteIndex * 6 + 3] = SpriteIndex * 4 + 0;
			Indices[SpriteIndex * 6 + 4] = SpriteIndex * 4 + 2;
			Indices[SpriteIndex * 6 + 5] = SpriteIndex * 4 + 3;
		}
		RHIUnlockBuffer(IndexBufferRHI);
	}

	const uint32 NumTileQuadsInBuffer;
};