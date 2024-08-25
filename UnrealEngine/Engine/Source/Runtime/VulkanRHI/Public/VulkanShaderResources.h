// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanResources.h: Vulkan resource RHI definitions.
=============================================================================*/

#pragma once

#include "BoundShaderStateCache.h"
#include "CrossCompilerCommon.h"
#include "VulkanCommon.h"
#include "VulkanThirdParty.h"

static inline VkDescriptorType BindingToDescriptorType(EVulkanBindingType::EType Type)
{
	// Make sure these do NOT alias EPackedTypeName*
	switch (Type)
	{
	case EVulkanBindingType::PackedUniformBuffer:	return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	case EVulkanBindingType::UniformBuffer:			return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	case EVulkanBindingType::CombinedImageSampler:	return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	case EVulkanBindingType::Sampler:				return VK_DESCRIPTOR_TYPE_SAMPLER;
	case EVulkanBindingType::Image:					return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	case EVulkanBindingType::UniformTexelBuffer:	return VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
	case EVulkanBindingType::StorageImage:			return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	case EVulkanBindingType::StorageTexelBuffer:	return VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
	case EVulkanBindingType::StorageBuffer:			return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	case EVulkanBindingType::InputAttachment:		return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
#if RHI_RAYTRACING
	case EVulkanBindingType::AccelerationStructure:	return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
#endif
	default:
		check(0);
		break;
	}

	return VK_DESCRIPTOR_TYPE_MAX_ENUM;
}

static inline EVulkanBindingType::EType DescriptorTypeToBinding(VkDescriptorType Type, bool bUsePacked = false)
{
	switch (Type)
	{
	case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:				return bUsePacked ? EVulkanBindingType::PackedUniformBuffer : EVulkanBindingType::UniformBuffer;
	case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:		return EVulkanBindingType::CombinedImageSampler;
	case VK_DESCRIPTOR_TYPE_SAMPLER:					return EVulkanBindingType::Sampler;
	case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:				return EVulkanBindingType::Image;
	case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:		return EVulkanBindingType::UniformTexelBuffer;
	case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:				return EVulkanBindingType::StorageImage;
	case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:		return EVulkanBindingType::StorageTexelBuffer;
	case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:				return EVulkanBindingType::StorageBuffer;
	case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:			return EVulkanBindingType::InputAttachment;
#if RHI_RAYTRACING
	case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR: return EVulkanBindingType::AccelerationStructure;
#endif
	default:
		check(0);
		break;
	}

	return EVulkanBindingType::Count;
}


// Vulkan ParameterMap:
// Buffer Index = EBufferIndex
// Base Offset = Index into the subtype
// Size = Ignored for non-globals
struct FVulkanShaderHeader
{
	enum EType
	{
		PackedGlobal,
		Global,
		UniformBuffer,

		Count,
	};

	struct FSpirvInfo
	{
		FSpirvInfo() = default;
		FSpirvInfo(uint32 InDescriptorSetOffset, uint32 InBindingIndexOffset)
			: DescriptorSetOffset(InDescriptorSetOffset)
			, BindingIndexOffset(InBindingIndexOffset)
		{
		}

		uint32	DescriptorSetOffset = UINT32_MAX;
		uint32	BindingIndexOffset = UINT32_MAX;
	};

	struct FUBResourceInfo
	{
		uint16									SourceUBResourceIndex;
		uint16									OriginalBindingIndex;
		// Index into the Global Array
		uint16									GlobalIndex;
		TEnumAsByte<EUniformBufferBaseType>		UBBaseType;
		uint8									Pad0 = 0;
#if VULKAN_ENABLE_BINDING_DEBUG_NAMES
		FString									DebugName;
#endif
	};

	struct FUniformBufferInfo
	{
		uint32					LayoutHash;
		uint16					ConstantDataOriginalBindingIndex;
		uint8					bOnlyHasResources;
		uint8					Pad0 = 0;
		//uint32					ConstantDataSizeInBytes;
		TArray<FUBResourceInfo>	ResourceEntries;
#if VULKAN_ENABLE_BINDING_DEBUG_NAMES
		FString					DebugName;
#endif
	};
	TArray<FUniformBufferInfo>	UniformBuffers;

	struct FGlobalInfo
	{
		uint16							OriginalBindingIndex;
		// If this is UINT16_MAX, it's a regular parameter, otherwise this is the SamplerState portion for a CombinedImageSampler
		// and this is the index into Global for the Texture portion
		uint16							CombinedSamplerStateAliasIndex;
		uint16							TypeIndex;
		// 1 if this is an immutable sampler
		uint8							bImmutableSampler = 0;
		uint8							Pad0 = 0;
#if VULKAN_ENABLE_BINDING_DEBUG_NAMES
		FString							DebugName;
#endif
	};
	TArray<FGlobalInfo>						Globals;
	TArray<TEnumAsByte<EVulkanBindingType::EType>>	GlobalDescriptorTypes;

	struct FPackedGlobalInfo
	{
		uint16							ConstantDataSizeInFloats;
		CrossCompiler::EPackedTypeIndex	PackedTypeIndex;
		uint8							PackedUBIndex;
#if VULKAN_ENABLE_BINDING_DEBUG_NAMES
		FString							DebugName;
#endif
	};
	TArray<FPackedGlobalInfo>	PackedGlobals;

	struct FPackedUBInfo
	{
		uint32							SizeInBytes;
		uint16							OriginalBindingIndex;
		CrossCompiler::EPackedTypeIndex	PackedTypeIndex;
		uint8							Pad0 = 0;
		uint32							SPIRVDescriptorSetOffset;
		uint32							SPIRVBindingIndexOffset;
	};
	TArray<FPackedUBInfo>					PackedUBs;

	enum class EAttachmentType : uint8
	{
		Color0,
		Color1,
		Color2,
		Color3,
		Color4,
		Color5,
		Color6,
		Color7,
		Depth,

		Count,
	};
	struct FInputAttachment
	{
		uint16			GlobalIndex;
		EAttachmentType	Type;
		uint8			Pad = 0;
	};
	TArray<FInputAttachment>				InputAttachments;

	// Number of copies per emulated buffer source index (to skip searching among UniformBuffersCopyInfo). Upper uint16 is the index, Lower uint16 is the count
	TArray<uint32>									EmulatedUBCopyRanges;
	TArray<CrossCompiler::FUniformBufferCopyInfo>	EmulatedUBsCopyInfo;

	// Mostly relevant for Vertex Shaders
	uint32									InOutMask;

	// Relevant for Ray Tracing Shaders
	uint32                                  RayTracingPayloadType = 0;
	uint32                                  RayTracingPayloadSize = 0;

	FSHAHash								SourceHash;
	uint32									SpirvCRC = 0;
	uint8									WaveSize = 0;

	// For RayHitGroup shaders
	enum class ERayHitGroupEntrypoint : uint8
	{
		NotPresent = 0,

		// Hit group types are all stored in a single spirv blob
		// and each have different entry point names
		// NOTE: Not used yet because of compiler issues
		CommonBlob,

		// Hit group types are each stored in a different spirv blob
		// to circumvent DXC compilation issues
		SeparateBlob
	};
	ERayHitGroupEntrypoint RayGroupAnyHit = ERayHitGroupEntrypoint::NotPresent;
	ERayHitGroupEntrypoint RayGroupIntersection = ERayHitGroupEntrypoint::NotPresent;

	TArray<FSpirvInfo>						UniformBufferSpirvInfos;
	TArray<FSpirvInfo>						GlobalSpirvInfos;

	FString									DebugName;

	FVulkanShaderHeader() = default;
	enum EInit
	{
		EZero
	};
	FVulkanShaderHeader(EInit)
		: InOutMask(0)
	{
	}
};

inline FArchive& operator<<(FArchive& Ar, FVulkanShaderHeader::FSpirvInfo& Info)
{
	Ar << Info.DescriptorSetOffset;
	Ar << Info.BindingIndexOffset;
	return Ar;
}

inline FArchive& operator<<(FArchive& Ar, FVulkanShaderHeader::FUBResourceInfo& Entry)
{
	Ar << Entry.SourceUBResourceIndex;
	Ar << Entry.OriginalBindingIndex;
	Ar << Entry.GlobalIndex;
	Ar << Entry.UBBaseType;
#if VULKAN_ENABLE_BINDING_DEBUG_NAMES
	Ar << Entry.DebugName;
#endif
	return Ar;
}

inline FArchive& operator<<(FArchive& Ar, FVulkanShaderHeader::FUniformBufferInfo& UBInfo)
{
	Ar << UBInfo.LayoutHash;
	Ar << UBInfo.ConstantDataOriginalBindingIndex;
	Ar << UBInfo.bOnlyHasResources;
	//Ar << UBInfo.ConstantDataSizeInBytes;
	Ar << UBInfo.ResourceEntries;
#if VULKAN_ENABLE_BINDING_DEBUG_NAMES
	Ar << UBInfo.DebugName;
#endif
	return Ar;
}

inline FArchive& operator<<(FArchive& Ar, FVulkanShaderHeader::FPackedGlobalInfo& PackedGlobalInfo)
{
	Ar << PackedGlobalInfo.ConstantDataSizeInFloats;
	Ar << PackedGlobalInfo.PackedTypeIndex;
	Ar << PackedGlobalInfo.PackedUBIndex;
#if VULKAN_ENABLE_BINDING_DEBUG_NAMES
	Ar << PackedGlobalInfo.DebugName;
#endif
	return Ar;
}

inline FArchive& operator<<(FArchive& Ar, FVulkanShaderHeader::FPackedUBInfo& PackedUBInfo)
{
	Ar << PackedUBInfo.SizeInBytes;
	Ar << PackedUBInfo.OriginalBindingIndex;
	Ar << PackedUBInfo.PackedTypeIndex;
	Ar << PackedUBInfo.SPIRVDescriptorSetOffset;
	Ar << PackedUBInfo.SPIRVBindingIndexOffset;
	return Ar;
}

inline FArchive& operator<<(FArchive& Ar, FVulkanShaderHeader::FGlobalInfo& GlobalInfo)
{
	Ar << GlobalInfo.OriginalBindingIndex;
	Ar << GlobalInfo.CombinedSamplerStateAliasIndex;
	Ar << GlobalInfo.TypeIndex;
	Ar << GlobalInfo.bImmutableSampler;
#if VULKAN_ENABLE_BINDING_DEBUG_NAMES
	Ar << GlobalInfo.DebugName;
#endif
	return Ar;
}

inline FArchive& operator<<(FArchive& Ar, FVulkanShaderHeader::FInputAttachment& AttachmentInfo)
{
	Ar << AttachmentInfo.GlobalIndex;
	Ar << AttachmentInfo.Type;
	return Ar;
}

inline FArchive& operator<<(FArchive& Ar, FVulkanShaderHeader& Header)
{
	Ar << Header.UniformBuffers;
	Ar << Header.Globals;
	Ar << Header.GlobalDescriptorTypes;
	Ar << Header.PackedGlobals;
	Ar << Header.PackedUBs;
	Ar << Header.InputAttachments;
	Ar << Header.EmulatedUBCopyRanges;
	Ar << Header.EmulatedUBsCopyInfo;
	Ar << Header.InOutMask;
	Ar << Header.RayTracingPayloadType;
	Ar << Header.RayTracingPayloadSize;
	Ar << Header.SourceHash;
	Ar << Header.SpirvCRC;
	Ar << Header.WaveSize;
	Ar << Header.RayGroupAnyHit;
	Ar << Header.RayGroupIntersection;
	Ar << Header.UniformBufferSpirvInfos;
	Ar << Header.GlobalSpirvInfos;
	Ar << Header.DebugName;
	return Ar;
}
