// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/PlatformAtomics.h"
#include "RHIGlobals.h"
#include "RHIResources.h"
#include "RHIStats.h"

namespace UE::RHICore
{
	inline void UpdateGlobalTextureStats(ETextureCreateFlags TextureFlags, ETextureDimension Dimension, uint64 TextureSizeInBytes, bool bOnlyStreamableTexturesInTexturePool, bool bAllocating)
	{
		constexpr ETextureCreateFlags AllRenderTargetFlags = ETextureCreateFlags::RenderTargetable | ETextureCreateFlags::ResolveTargetable | ETextureCreateFlags::DepthStencilTargetable;
#if STATS
		const int64 TextureSizeDeltaInBytes = bAllocating ? static_cast<int64>(TextureSizeInBytes) : -static_cast<int64>(TextureSizeInBytes);
		if (EnumHasAnyFlags(TextureFlags, AllRenderTargetFlags))
		{
			switch (Dimension)
			{
			default:
				checkNoEntry();
				[[fallthrough]];
			case ETextureDimension::Texture2D:
				[[fallthrough]];
			case ETextureDimension::Texture2DArray:
				INC_MEMORY_STAT_BY(STAT_RenderTargetMemory2D, TextureSizeDeltaInBytes);
				break;
			case ETextureDimension::Texture3D:
				INC_MEMORY_STAT_BY(STAT_RenderTargetMemory3D, TextureSizeDeltaInBytes);
				break;
			case ETextureDimension::TextureCube:
				[[fallthrough]];
			case ETextureDimension::TextureCubeArray:
				INC_MEMORY_STAT_BY(STAT_RenderTargetMemoryCube, TextureSizeDeltaInBytes);
				break;
			};
		}
		else if (EnumHasAnyFlags(TextureFlags, ETextureCreateFlags::UAV))
		{
			INC_MEMORY_STAT_BY(STAT_UAVTextureMemory, TextureSizeDeltaInBytes);
		}
		else
		{
			switch (Dimension)
			{
			default:
				checkNoEntry();
				[[fallthrough]];
			case ETextureDimension::Texture2D:
				[[fallthrough]];
			case ETextureDimension::Texture2DArray:
				INC_MEMORY_STAT_BY(STAT_TextureMemory2D, TextureSizeDeltaInBytes);
				break;
			case ETextureDimension::Texture3D:
				INC_MEMORY_STAT_BY(STAT_TextureMemory3D, TextureSizeDeltaInBytes);
				break;
			case ETextureDimension::TextureCube:
				[[fallthrough]];
			case ETextureDimension::TextureCubeArray:
				INC_MEMORY_STAT_BY(STAT_TextureMemoryCube, TextureSizeDeltaInBytes);
				break;
			};
		}
#endif
		const int64 AlignedTextureSizeInKB = static_cast<int64>(Align(TextureSizeInBytes, 1024) / 1024);
		const int64 TextureSizeDeltaInKB = bAllocating ? AlignedTextureSizeInKB : -AlignedTextureSizeInKB;

		const bool bAlwaysExcludedFromStreamingSize = EnumHasAnyFlags(TextureFlags,
			AllRenderTargetFlags
			| ETextureCreateFlags::UAV
			| ETextureCreateFlags::ForceIntoNonStreamingMemoryTracking
		);

		const bool bStreamable = EnumHasAnyFlags(TextureFlags, ETextureCreateFlags::Streamable);

		if (!bAlwaysExcludedFromStreamingSize && (!bOnlyStreamableTexturesInTexturePool || bStreamable))
		{
			FPlatformAtomics::InterlockedAdd((volatile int64*)&GRHIGlobals.StreamingTextureMemorySizeInKB, TextureSizeDeltaInKB);
		}
		else
		{
			FPlatformAtomics::InterlockedAdd((volatile int64*)&GRHIGlobals.NonStreamingTextureMemorySizeInKB, TextureSizeDeltaInKB);
		}
	}

	inline void UpdateGlobalTextureStats(const FRHITextureDesc& TextureDesc, uint64 TextureSizeInBytes, bool bOnlyStreamableTexturesInTexturePool, bool bAllocating)
	{
		return UpdateGlobalTextureStats(TextureDesc.Flags, TextureDesc.Dimension, TextureSizeInBytes, bOnlyStreamableTexturesInTexturePool, bAllocating);
	}

	inline void FillBaselineTextureMemoryStats(FTextureMemoryStats& OutStats)
	{
		OutStats.StreamingMemorySize    = GRHIGlobals.StreamingTextureMemorySizeInKB * 1024;
		OutStats.NonStreamingMemorySize = GRHIGlobals.NonStreamingTextureMemorySizeInKB * 1024;
		OutStats.TexturePoolSize        = GRHIGlobals.TexturePoolSize;

		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		OutStats.AllocatedMemorySize = OutStats.StreamingMemorySize;
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	inline void UpdateGlobalBufferStats(const FRHIBufferDesc& BufferDesc, int64 BufferSizeDelta)
	{
#if STATS
		if (EnumHasAnyFlags(BufferDesc.Usage, EBufferUsageFlags::VertexBuffer))
		{
			INC_MEMORY_STAT_BY(STAT_VertexBufferMemory, BufferSizeDelta);
		}
		else if (EnumHasAnyFlags(BufferDesc.Usage, EBufferUsageFlags::IndexBuffer))
		{
			INC_MEMORY_STAT_BY(STAT_IndexBufferMemory, BufferSizeDelta);
		}
		else if (EnumHasAnyFlags(BufferDesc.Usage, EBufferUsageFlags::AccelerationStructure))
		{
			INC_MEMORY_STAT_BY(STAT_RTAccelerationStructureMemory, BufferSizeDelta);
		}
		else if (EnumHasAnyFlags(BufferDesc.Usage, EBufferUsageFlags::ByteAddressBuffer))
		{
			INC_MEMORY_STAT_BY(STAT_ByteAddressBufferMemory, BufferSizeDelta);
		}
		else if (EnumHasAnyFlags(BufferDesc.Usage, EBufferUsageFlags::DrawIndirect))
		{
			INC_MEMORY_STAT_BY(STAT_DrawIndirectBufferMemory, BufferSizeDelta);
		}
		else if (EnumHasAnyFlags(BufferDesc.Usage, EBufferUsageFlags::StructuredBuffer))
		{
			INC_MEMORY_STAT_BY(STAT_StructuredBufferMemory, BufferSizeDelta);
		}
		else
		{
			INC_MEMORY_STAT_BY(STAT_MiscBufferMemory, BufferSizeDelta);
		}
#endif

		FPlatformAtomics::InterlockedAdd((volatile int64*)&GRHIGlobals.BufferMemorySize, BufferSizeDelta);
	}

	inline void UpdateGlobalBufferStats(const FRHIBufferDesc& BufferDesc, uint64 BufferSize, bool bAllocating)
	{
		const int64 BufferSizeDelta = bAllocating ? static_cast<int64>(BufferSize) : -static_cast<int64>(BufferSize);
		UpdateGlobalBufferStats(BufferDesc, BufferSizeDelta);
	}

	inline void UpdateGlobalUniformBufferStats(int64 BufferSize, bool bAllocating)
	{
		const int64 BufferSizeDelta = bAllocating ? static_cast<int64>(BufferSize) : -static_cast<int64>(BufferSize);

		INC_MEMORY_STAT_BY(STAT_UniformBufferMemory, BufferSizeDelta);
		FPlatformAtomics::InterlockedAdd((volatile int64*)&GRHIGlobals.UniformBufferMemorySize, BufferSizeDelta);
	}
}
