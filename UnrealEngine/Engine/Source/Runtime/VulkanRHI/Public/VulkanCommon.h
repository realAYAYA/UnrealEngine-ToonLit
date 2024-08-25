// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanCommon.h: Common definitions used for both runtime and compiling shaders.
=============================================================================*/

#pragma once

#include "RHIDefinitions.h"
#include "Logging/LogMacros.h"

#ifndef VULKAN_SUPPORTS_GEOMETRY_SHADERS
	#define VULKAN_SUPPORTS_GEOMETRY_SHADERS					PLATFORM_SUPPORTS_GEOMETRY_SHADERS
#endif

// This defines controls shader generation (so will cause a format rebuild)
// be careful wrt cooker/target platform not matching define-wise!!!
// ONLY used for debugging binding table/descriptor set bugs/mismatches.
#define VULKAN_ENABLE_BINDING_DEBUG_NAMES						0

namespace ShaderStage
{
	enum EStage
	{
		// Adjusting these requires a full shader rebuild (ie modify the guid on VulkanCommon.usf)
		// Keep the values in sync with EShaderFrequency
		Vertex = 0,
		Pixel = 1,

#if VULKAN_SUPPORTS_GEOMETRY_SHADERS
		Geometry = 2,
#endif

#if RHI_RAYTRACING
		RayGen = 3,
		RayMiss = 4,
		RayHitGroup = 5,
		RayCallable = 6,
#endif

#if VULKAN_SUPPORTS_GEOMETRY_SHADERS
		NumGeometryStages = 1,
#else
		NumGeometryStages = 0,
#endif

#if RHI_RAYTRACING
		NumRayTracingStages = 4,
#else
		NumRayTracingStages = 0,
#endif

		NumStages = (2 + NumGeometryStages + NumRayTracingStages),

		// Compute is its own pipeline, so it can all live as set 0
		Compute = 0,

#if VULKAN_SUPPORTS_GEOMETRY_SHADERS || RHI_RAYTRACING
		MaxNumSets = 8,
#else
		MaxNumSets = 4,
#endif

		Invalid = -1,
	};

	inline EStage GetStageForFrequency(EShaderFrequency Stage)
	{
		switch (Stage)
		{
		case SF_Vertex:		return Vertex;
		case SF_Pixel:		return Pixel;
#if VULKAN_SUPPORTS_GEOMETRY_SHADERS
		case SF_Geometry:	return Geometry;
#endif
#if RHI_RAYTRACING
		case SF_RayGen:			return RayGen;
		case SF_RayMiss:		return RayMiss;
		case SF_RayHitGroup:	return RayHitGroup;
		case SF_RayCallable:	return RayCallable;
#endif // RHI_RAYTRACING
		case SF_Compute:	return Compute;
		default:
			checkf(0, TEXT("Invalid shader Stage %d"), (int32)Stage);
			break;
		}

		return Invalid;
	}

	inline EShaderFrequency GetFrequencyForGfxStage(EStage Stage)
	{
		switch (Stage)
		{
		case EStage::Vertex:	return SF_Vertex;
		case EStage::Pixel:		return SF_Pixel;
#if VULKAN_SUPPORTS_GEOMETRY_SHADERS
		case EStage::Geometry:	return SF_Geometry;
#endif
#if RHI_RAYTRACING
		case EStage::RayGen:		return SF_RayGen;
		case EStage::RayMiss:		return SF_RayMiss;
		case EStage::RayHitGroup:	return SF_RayHitGroup;
		case EStage::RayCallable:	return SF_RayCallable;
#endif //	RHI_RAYTRACING
		default:
			checkf(0, TEXT("Invalid shader Stage %d"), (int32)Stage);
			break;
		}

		return SF_NumFrequencies;
	}
};

namespace VulkanBindless
{
	static constexpr uint32 MaxUniformBuffersPerStage = 16;

	enum EDescriptorSets
	{
		BindlessSamplerSet = 0,

		BindlessStorageBufferSet,
		BindlessUniformBufferSet,

		BindlessStorageImageSet,
		BindlessSampledImageSet,

		BindlessStorageTexelBufferSet,
		BindlessUniformTexelBufferSet,

		BindlessAccelerationStructureSet,

		BindlessSingleUseUniformBufferSet,  // Keep last
		NumBindlessSets,
		MaxNumSets = NumBindlessSets
	};
};

namespace EVulkanBindingType
{
	enum EType : uint8
	{
		PackedUniformBuffer,	//VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
		UniformBuffer,			//VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER

		CombinedImageSampler,	//VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER	*not used*
		Sampler,				//VK_DESCRIPTOR_TYPE_SAMPLER				(HLSL: SamplerState/SamplerComparisonState)
		Image,					//VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE			(HLSL: Texture2D/3D/Cube)

		UniformTexelBuffer,		//VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER	(HLSL: Buffer)

		// A storage image is a descriptor type that is used for load, store, and atomic operations on image memory from within shaders bound to pipelines.
		StorageImage,			//VK_DESCRIPTOR_TYPE_STORAGE_IMAGE			(HLSL: RWTexture2D/3D/Cube)

		//A storage texel buffer represents a tightly packed array of homogeneous formatted data that is stored in a buffer and is made accessible to shaders. Storage texel buffers differ from uniform texel buffers in that they support stores and atomic operations in shaders, may support a different maximum length, and may have different performance characteristics.
		StorageTexelBuffer,		//VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER	(HLSL: RWBuffer)

		// A storage buffer is a region of structured storage that supports both read and write access for shaders. In addition to general read and write operations, some members of storage buffers can be used as the target of atomic operations. In general, atomic operations are only supported on members that have unsigned integer formats.
		StorageBuffer,			//VK_DESCRIPTOR_TYPE_STORAGE_BUFFER			(HLSL: StructuredBuffer/RWStructureBuffer/ByteAddressBuffer/RWByteAddressBuffer)

		InputAttachment,

		AccelerationStructure,	//VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR

		Count,
	};

	static inline char GetBindingTypeChar(EType Type)
	{
		// Make sure these do NOT alias EPackedTypeName*
		switch (Type)
		{
		case UniformBuffer:			return 'b';
		case CombinedImageSampler:	return 'c';
		case Sampler:				return 'p';
		case Image:					return 'w';
		case UniformTexelBuffer:	return 'x';
		case StorageImage:			return 'y';
		case StorageTexelBuffer:	return 'z';
		case StorageBuffer:			return 'v';
		case InputAttachment:		return 'a';
		case AccelerationStructure:	return 'r';
		default:
			check(0);
			break;
		}

		return 0;
	}
}

DECLARE_LOG_CATEGORY_EXTERN(LogVulkan, Display, All);

template< class T >
static FORCEINLINE void ZeroVulkanStruct(T& Struct, int32 VkStructureType)
{
	static_assert(!TIsPointer<T>::Value, "Don't use a pointer!");
	static_assert(STRUCT_OFFSET(T, sType) == 0, "Assumes sType is the first member in the Vulkan type!");
	static_assert(sizeof(T::sType) == sizeof(int32), "Assumed sType is compatible with int32!");
	// Horrible way to coerce the compiler to not have to know what T::sType is so we can have this header not have to include vulkan.h
	(int32&)Struct.sType = VkStructureType;
	FMemory::Memzero(((uint8*)&Struct) + sizeof(VkStructureType), sizeof(T) - sizeof(VkStructureType));
}
