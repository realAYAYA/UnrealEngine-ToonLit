// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RenderResource.h"
#include "RHI.h"

template< uint32 NumSprites >
class FSpriteIndexBuffer : public FIndexBuffer
{
public:
	virtual void InitRHI() override
	{
		const uint32 Size = sizeof(uint16) * 6 * NumSprites;
		const uint32 Stride = sizeof(uint16);
		FRHIResourceCreateInfo CreateInfo(TEXT("FSpriteIndexBuffer"));
		IndexBufferRHI = RHICreateBuffer( Size, BUF_Static | BUF_IndexBuffer, Stride, ERHIAccess::VertexOrIndexBuffer, CreateInfo );
		uint16* Indices = (uint16*)RHILockBuffer( IndexBufferRHI, 0, Size, RLM_WriteOnly );
		for (uint32 SpriteIndex = 0; SpriteIndex < NumSprites; ++SpriteIndex)
		{
			Indices[SpriteIndex*6 + 0] = SpriteIndex*4 + 0;
			Indices[SpriteIndex*6 + 1] = SpriteIndex*4 + 3;
			Indices[SpriteIndex*6 + 2] = SpriteIndex*4 + 2;
			Indices[SpriteIndex*6 + 3] = SpriteIndex*4 + 0;
			Indices[SpriteIndex*6 + 4] = SpriteIndex*4 + 1;
			Indices[SpriteIndex*6 + 5] = SpriteIndex*4 + 3;
		}
		RHIUnlockBuffer( IndexBufferRHI );
	}
};