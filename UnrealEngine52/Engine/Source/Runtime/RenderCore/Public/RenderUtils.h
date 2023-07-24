// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RHIFwd.h"
#include "RHIShaderPlatform.h"
#include "RHIFeatureLevel.h"

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "RHI.h"
#include "RenderResource.h"
#include "RHIDefinitions.h"
#include "PackedNormal.h"
#include "RenderMath.h"
#include "GlobalRenderResources.h"
#include "ShaderPlatformCachedIniValue.h"
#endif

class ITargetPlatform;
struct IPooledRenderTarget;
enum class ERayTracingMode : uint8;

extern RENDERCORE_API void RenderUtilsInit();

#define NUM_DEBUG_UTIL_COLORS (32)
static const FColor DebugUtilColor[NUM_DEBUG_UTIL_COLORS] = 
{
	FColor(20,226,64),
	FColor(210,21,0),
	FColor(72,100,224),
	FColor(14,153,0),
	FColor(186,0,186),
	FColor(54,0,175),
	FColor(25,204,0),
	FColor(15,189,147),
	FColor(23,165,0),
	FColor(26,206,120),
	FColor(28,163,176),
	FColor(29,0,188),
	FColor(130,0,50),
	FColor(31,0,163),
	FColor(147,0,190),
	FColor(1,0,109),
	FColor(2,126,203),
	FColor(3,0,58),
	FColor(4,92,218),
	FColor(5,151,0),
	FColor(18,221,0),
	FColor(6,0,131),
	FColor(7,163,176),
	FColor(8,0,151),
	FColor(102,0,216),
	FColor(10,0,171),
	FColor(11,112,0),
	FColor(12,167,172),
	FColor(13,189,0),
	FColor(16,155,0),
	FColor(178,161,0),
	FColor(19,25,126)
};

#define NUM_CUBE_VERTICES 36

/** The indices for drawing a cube. */
extern RENDERCORE_API const uint16 GCubeIndices[36];

/**
 * Maps from an X,Y,Z cube vertex coordinate to the corresponding vertex index.
 */
inline uint16 GetCubeVertexIndex(uint32 X,uint32 Y,uint32 Z) { return (uint16)(X * 4 + Y * 2 + Z); }

/**
* A 3x1 of xyz(11:11:10) format.
*/
struct FPackedPosition
{
	union
	{
		struct
		{
#if PLATFORM_LITTLE_ENDIAN
			int32	X :	11;
			int32	Y : 11;
			int32	Z : 10;
#else
			int32	Z : 10;
			int32	Y : 11;
			int32	X : 11;
#endif
		} Vector;

		uint32		Packed;
	};

	// Constructors.
	FPackedPosition() : Packed(0) {}
	FPackedPosition(const FVector3f& Other) : Packed(0) 
	{
		Set(Other);
	}
	FPackedPosition(const FVector3d& Other) : Packed(0) 
	{
		Set(Other);
	}
	
	// Conversion operators.
	FPackedPosition& operator=( FVector3f Other )
	{
		Set( Other );
		return *this;
	}
	FPackedPosition& operator=( FVector3d Other )
	{
		Set( Other );
		return *this;
	}

	operator FVector3f() const;
	VectorRegister GetVectorRegister() const;

	// Set functions.
	void Set(const FVector3f& InVector);
	void Set(const FVector3d& InVector);

	// Serializer.
	friend FArchive& operator<<(FArchive& Ar,FPackedPosition& N);
};


/** Flags that control ConstructTexture2D */
enum EConstructTextureFlags
{
	/** Compress RGBA8 to DXT */
	CTF_Compress =				0x01,
	/** Don't actually compress until the pacakge is saved */
	CTF_DeferCompression =		0x02,
	/** Enable SRGB on the texture */
	CTF_SRGB =					0x04,
	/** Generate mipmaps for the texture */
	CTF_AllowMips =				0x08,
	/** Use DXT1a to get 1 bit alpha but only 4 bits per pixel (note: color of alpha'd out part will be black) */
	CTF_ForceOneBitAlpha =		0x10,
	/** When rendering a masked material, the depth is in the alpha, and anywhere not rendered will be full depth, which should actually be alpha of 0, and anything else is alpha of 255 */
	CTF_RemapAlphaAsMasked =	0x20,
	/** Ensure the alpha channel of the texture is opaque white (255). */
	CTF_ForceOpaque =			0x40,

	/** Default flags (maps to previous defaults to ConstructTexture2D) */
	CTF_Default = CTF_Compress | CTF_SRGB,
};

/**
 * Calculates the amount of memory used for a single mip-map of a texture 3D.
 *
 * Use GPixelFormats[Format].Get3DTextureMipSizeInBytes() instead.
 * 
 * @param TextureSizeX		Number of horizontal texels (for the base mip-level)
 * @param TextureSizeY		Number of vertical texels (for the base mip-level)
 * @param TextureSizeZ		Number of slices (for the base mip-level)
 * @param Format	Texture format
 * @param MipIndex	The index of the mip-map to compute the size of.
 */
RENDERCORE_API SIZE_T CalcTextureMipMapSize3D( uint32 TextureSizeX, uint32 TextureSizeY, uint32 TextureSizeZ, EPixelFormat Format, uint32 MipIndex);

/**
 * Calculates the extent of a mip.
 *
 * Incorrectly forces min mip size to be block dimensions: UE-159189
 * 
 * @param TextureSizeX		Number of horizontal texels (for the base mip-level)
 * @param TextureSizeY		Number of vertical texels (for the base mip-level)
 * @param TextureSizeZ		Number of depth texels (for the base mip-level)
 * @param Format			Texture format
 * @param MipIndex			The index of the mip-map to compute the size of.
 * @param OutXExtent		The extent X of the mip
 * @param OutYExtent		The extent Y of the mip
 * @param OutZExtent		The extent Z of the mip
 */
RENDERCORE_API void CalcMipMapExtent3D( uint32 TextureSizeX, uint32 TextureSizeY, uint32 TextureSizeZ, EPixelFormat Format, uint32 MipIndex, uint32& OutXExtent, uint32& OutYExtent, uint32& OutZExtent );

/**
 * Calculates the extent of a mip.
 *
 * Incorrectly forces min mip size to be block dimensions: UE-159189
 * 
 * @param TextureSizeX		Number of horizontal texels (for the base mip-level)
 * @param TextureSizeY		Number of vertical texels (for the base mip-level)
 * @param Format	Texture format
 * @param MipIndex	The index of the mip-map to compute the size of.
 */
RENDERCORE_API FIntPoint CalcMipMapExtent( uint32 TextureSizeX, uint32 TextureSizeY, EPixelFormat Format, uint32 MipIndex );

/**
 * Calculates the width of a mip, in blocks.
 *
 * @param TextureSizeX		Number of horizontal texels (for the base mip-level)
 * @param Format			Texture format
 * @param MipIndex			The index of the mip-map to compute the size of.
 */
UE_DEPRECATED(5.1, "See GPixelFormats in PixelFormat.h for analogous functions")
RENDERCORE_API SIZE_T CalcTextureMipWidthInBlocks(uint32 TextureSizeX, EPixelFormat Format, uint32 MipIndex);

/**
 * Calculates the height of a mip, in blocks.
 *
 * @param TextureSizeY		Number of vertical texels (for the base mip-level)
 * @param Format			Texture format
 * @param MipIndex			The index of the mip-map to compute the size of.
 */
UE_DEPRECATED(5.1, "See GPixelFormats in PixelFormat.h for analogous functions")
RENDERCORE_API SIZE_T CalcTextureMipHeightInBlocks(uint32 TextureSizeY, EPixelFormat Format, uint32 MipIndex);

/**
 * Calculates the amount of memory used for a single mip-map of a texture.
 * 
 * Use GPixelFormats[Format].Get2DTextureMipSizeInBytes() instead.
 *
 * @param TextureSizeX		Number of horizontal texels (for the base mip-level)
 * @param TextureSizeY		Number of vertical texels (for the base mip-level)
 * @param Format	Texture format
 * @param MipIndex	The index of the mip-map to compute the size of.
 */
RENDERCORE_API SIZE_T CalcTextureMipMapSize( uint32 TextureSizeX, uint32 TextureSizeY, EPixelFormat Format, uint32 MipIndex );

/**
 * Calculates the amount of memory used for a texture.
 *
 * Use GPixelFormats[Format].Get2DTextureSizeInBytes() instead.
 * 
 * @param SizeX		Number of horizontal texels (for the base mip-level)
 * @param SizeY		Number of vertical texels (for the base mip-level)
 * @param Format	Texture format
 * @param MipCount	Number of mip-levels (including the base mip-level)
 */
RENDERCORE_API SIZE_T CalcTextureSize( uint32 SizeX, uint32 SizeY, EPixelFormat Format, uint32 MipCount );

/**
 * Calculates the amount of memory used for a texture.
 *
 * Use GPixelFormats[Format].Get3DTextureSizeInBytes() instead.
 * 
 * @param SizeX		Number of horizontal texels (for the base mip-level)
 * @param SizeY		Number of vertical texels (for the base mip-level)
 * @param SizeY		Number of depth texels (for the base mip-level)
 * @param Format	Texture format
 * @param MipCount	Number of mip-levels (including the base mip-level)
 */
RENDERCORE_API SIZE_T CalcTextureSize3D( uint32 SizeX, uint32 SizeY, uint32 SizeZ, EPixelFormat Format, uint32 MipCount );

/**
 * Copies the data for a 2D texture between two buffers with potentially different strides.
 * @param Source       - The source buffer
 * @param Dest         - The destination buffer.
 * @param SizeY        - The height of the texture data to copy in pixels.
 * @param Format       - The format of the texture being copied.
 * @param SourceStride - The stride of the source buffer.
 * @param DestStride   - The stride of the destination buffer.
 */
RENDERCORE_API void CopyTextureData2D(const void* Source,void* Dest,uint32 SizeY,EPixelFormat Format,uint32 SourceStride,uint32 DestStride);

/**
 *  Returns the valid channels for this pixel format
 * 
 * @return e.g. EPixelFormatChannelFlags::G for PF_G8
 */
RENDERCORE_API EPixelFormatChannelFlags GetPixelFormatValidChannels(EPixelFormat InPixelFormat);


/**
 * Convert from ECubeFace to text string
 * @param Face - ECubeFace type to convert
 * @return text string for cube face enum value
 */
RENDERCORE_API const TCHAR* GetCubeFaceName(ECubeFace Face);

/**
 * Convert from text string to ECubeFace 
 * @param Name e.g. RandomNamePosX
 * @return CubeFace_MAX if not recognized
 */
RENDERCORE_API ECubeFace GetCubeFaceFromName(const FString& Name);

RENDERCORE_API FVertexDeclarationRHIRef& GetVertexDeclarationFVector4();

RENDERCORE_API FVertexDeclarationRHIRef& GetVertexDeclarationFVector3();

RENDERCORE_API FVertexDeclarationRHIRef& GetVertexDeclarationFVector2();

RENDERCORE_API bool MobileSupportsGPUScene();

RENDERCORE_API bool IsMobileDeferredShadingEnabled(const FStaticShaderPlatform Platform);

RENDERCORE_API bool MobileRequiresSceneDepthAux(const FStaticShaderPlatform Platform);

RENDERCORE_API bool SupportsTextureCubeArray(ERHIFeatureLevel::Type FeatureLevel);

RENDERCORE_API bool MaskedInEarlyPass(const FStaticShaderPlatform Platform);

RENDERCORE_API bool AllowPixelDepthOffset(const FStaticShaderPlatform Platform);

RENDERCORE_API bool AllowPerPixelShadingModels(const FStaticShaderPlatform Platform);

RENDERCORE_API uint32 GetPlatformShadingModelsMask(const FStaticShaderPlatform Platform);

RENDERCORE_API bool IsMobileAmbientOcclusionEnabled(const FStaticShaderPlatform Platform);

RENDERCORE_API bool IsMobileDistanceFieldEnabled(const FStaticShaderPlatform Platform);

RENDERCORE_API bool IsMobileMovableSpotlightShadowsEnabled(const FStaticShaderPlatform Platform);

RENDERCORE_API bool MobileForwardEnableLocalLights(const FStaticShaderPlatform Platform);

RENDERCORE_API bool MobileForwardEnableClusteredReflections(const FStaticShaderPlatform Platform);

RENDERCORE_API bool MobileUsesShadowMaskTexture(const FStaticShaderPlatform Platform);

RENDERCORE_API bool MobileUsesExtenedGBuffer(const FStaticShaderPlatform Platform);

RENDERCORE_API bool MobileUsesGBufferCustomData(const FStaticShaderPlatform Platform);

RENDERCORE_API bool MobileBasePassAlwaysUsesCSM(const FStaticShaderPlatform Platform);

RENDERCORE_API bool SupportsGen4TAA(const FStaticShaderPlatform Platform);

RENDERCORE_API bool SupportsTSR(const FStaticShaderPlatform Platform);

RENDERCORE_API bool PlatformSupportsVelocityRendering(const FStaticShaderPlatform Platform);

RENDERCORE_API bool IsUsingDBuffers(const FStaticShaderPlatform Platform);

/** Returns if ForwardShading is enabled. Only valid for the current platform (otherwise call ITargetPlatform::UsesForwardShading()). */
RENDERCORE_API bool IsForwardShadingEnabled(const FStaticShaderPlatform Platform);

/** Returns if the GBuffer is used. Only valid for the current platform. */
RENDERCORE_API bool IsUsingGBuffers(const FStaticShaderPlatform Platform);

/** Returns whether the base pass should output to the velocity buffer is enabled for a given shader platform */
RENDERCORE_API bool IsUsingBasePassVelocity(const FStaticShaderPlatform Platform);

/** Returns whether the base pass should use selective outputs for a given shader platform */
RENDERCORE_API bool IsUsingSelectiveBasePassOutputs(const FStaticShaderPlatform Platform);

/** Returns whether distance fields are enabled for a given shader platform */
RENDERCORE_API bool IsUsingDistanceFields(const FStaticShaderPlatform Platform);

/** Returns if water should render distance field shadow a second time for the water surface. This is for a platofrm so can be used at cook time. */
RENDERCORE_API bool IsWaterDistanceFieldShadowEnabled(const FStaticShaderPlatform Platform);

RENDERCORE_API bool UseGPUScene(const FStaticShaderPlatform Platform, const FStaticFeatureLevel FeatureLevel);

RENDERCORE_API bool UseGPUScene(const FStaticShaderPlatform Platform);

RENDERCORE_API bool ForceSimpleSkyDiffuse(const FStaticShaderPlatform Platform);

RENDERCORE_API bool VelocityEncodeDepth(const FStaticShaderPlatform Platform);

/** Unit cube vertex buffer (VertexDeclarationFVector4) */
RENDERCORE_API FBufferRHIRef& GetUnitCubeVertexBuffer();

/** Unit cube index buffer */
RENDERCORE_API FBufferRHIRef& GetUnitCubeIndexBuffer();

/** Unit cube AABB vertex buffer (useful to create procedural raytracing geometry) */
RENDERCORE_API FBufferRHIRef& GetUnitCubeAABBVertexBuffer();

/**
* Takes the requested buffer size and quantizes it to an appropriate size for the rest of the
* rendering pipeline. Currently ensures that sizes are multiples of 4 so that they can safely
* be halved in size several times.
*/
RENDERCORE_API void QuantizeSceneBufferSize(const FIntPoint& InBufferSize, FIntPoint& OutBufferSize);

/**
* Checks if virtual texturing enabled and supported
* todo: Deprecate the version of the function that takes FStaticFeatureLevel
*/
RENDERCORE_API bool UseVirtualTexturing(const EShaderPlatform InShaderPlatform, const ITargetPlatform* TargetPlatform = nullptr);
RENDERCORE_API bool UseVirtualTexturing(const FStaticFeatureLevel InFeatureLevel, const ITargetPlatform* TargetPlatform = nullptr);

RENDERCORE_API bool DoesPlatformSupportNanite(EShaderPlatform Platform, bool bCheckForProjectSetting = true);

RENDERCORE_API bool NaniteAtomicsSupported();

RENDERCORE_API bool NaniteComputeMaterialsSupported();

RENDERCORE_API bool DoesRuntimeSupportNanite(EShaderPlatform ShaderPlatform, bool bCheckForAtomicSupport, bool bCheckForProjectSetting);

/**
 * Returns true if Nanite rendering should be used for the given shader platform.
 */
RENDERCORE_API bool UseNanite(EShaderPlatform ShaderPlatform, bool bCheckForAtomicSupport = true, bool bCheckForProjectSetting = true);

/**
 * Returns true if Virtual Shadow Maps should be used for the given shader platform.
 * Note: Virtual Shadow Maps require Nanite support.
 */
RENDERCORE_API bool UseVirtualShadowMaps(EShaderPlatform ShaderPlatform, const FStaticFeatureLevel FeatureLevel);

/**
* Returns true if Virtual Shadow Mapsare supported for the given shader platform.
* Note: Virtual Shadow Maps require Nanite platform support.
*/
RENDERCORE_API bool DoesPlatformSupportVirtualShadowMaps(EShaderPlatform Platform);

/**
* Returns true if non-Nanite virtual shadow maps are enabled by CVar r.Shadow.Virtual.NonNaniteVSM
* and the runtime supports Nanite/virtual shadow maps.
*/
RENDERCORE_API bool DoesPlatformSupportNonNaniteVirtualShadowMaps(EShaderPlatform ShaderPlatform);

/**
* Similar to DoesPlatformSupportNonNaniteVirtualShadowMaps, but checks if nanite and virtual shadow maps are enabled (at runtime).
*/
RENDERCORE_API bool UseNonNaniteVirtualShadowMaps(EShaderPlatform ShaderPlatform, FStaticFeatureLevel FeatureLevel);

/** Returns if water should evaluate virtual shadow maps a second time for the water surface. This is for a platform so can be used at cook time. */
RENDERCORE_API bool IsWaterVirtualShadowMapFilteringEnabled(const FStaticShaderPlatform Platform);

/**
*	(Non-runtime) Checks if the depth prepass for single layer water is enabled. This also depends on virtual shadow maps to be supported on the platform.
*/
RENDERCORE_API bool IsSingleLayerWaterDepthPrepassEnabled(const FStaticShaderPlatform Platform, FStaticFeatureLevel FeatureLevel);

/**
*	Checks if virtual texturing lightmap enabled and supported
*/
RENDERCORE_API bool UseVirtualTextureLightmap(const FStaticFeatureLevel InFeatureLevel, const ITargetPlatform* TargetPlatform = nullptr);

/**
*	Checks if platform uses a Nanite landscape mesh
*/
RENDERCORE_API bool UseNaniteLandscapeMesh(EShaderPlatform ShaderPlatform);

/**
 *  Checks if the non-pipeline shaders will not be compild and ones from FShaderPipeline used instead.
 */
RENDERCORE_API bool ExcludeNonPipelinedShaderTypes(EShaderPlatform ShaderPlatform);

/**
 *   Checks if skin cache shaders are enabled for the platform (via r.SkinCache.CompileShaders)
 */
RENDERCORE_API bool AreSkinCacheShadersEnabled(EShaderPlatform Platform);

/*
 * Detect (at runtime) if the runtime supports rendering one-pass point light shadows (i.e., cube maps)
 */
RENDERCORE_API bool DoesRuntimeSupportOnePassPointLightShadows(EShaderPlatform Platform);

/**
 * Read-only switch to check if translucency per object shadows are enabled.
 */
RENDERCORE_API bool AllowTranslucencyPerObjectShadows(const FStaticShaderPlatform Platform);


/**Note, this should only be used when a platform requires special shader compilation for 32 bit pixel format render targets.
Does not replace pixel format associations across the board**/

RENDERCORE_API bool PlatformRequires128bitRT(EPixelFormat PixelFormat);

RENDERCORE_API bool IsRayTracingEnabledForProject(EShaderPlatform ShaderPlatform);
RENDERCORE_API bool ShouldCompileRayTracingShadersForProject(EShaderPlatform ShaderPlatform);
RENDERCORE_API bool ShouldCompileRayTracingCallableShadersForProject(EShaderPlatform ShaderPlatform);

// Returns `true` when running on RT-capable machine, RT support is enabled for the project and by game graphics options and RT is enabled with r.Raytracing.Enable
// This function may only be called at runtime, never during cooking.
extern RENDERCORE_API bool IsRayTracingEnabled();

// Returns 'true' when running on RT-capable machine, RT support is enabled for the project and by game graphics options and ShaderPlatform supports RT and RT is enabled with r.Raytracing.Enable
// This function may only be called at runtime, never during cooking.
RENDERCORE_API bool IsRayTracingEnabled(EShaderPlatform ShaderPlatform);

// Returns 'true' when running on RT-capable machine, RT support is enabled for the project and by game graphics options
// This function may only be called at runtime, never during cooking.
extern RENDERCORE_API bool IsRayTracingAllowed();

// Returns the ray tracing mode if ray tracing is allowed.
// This function may only be called at runtime, never during cooking.
extern RENDERCORE_API ERayTracingMode GetRayTracingMode();

namespace Strata
{
	RENDERCORE_API bool IsStrataEnabled();
	RENDERCORE_API bool IsRoughDiffuseEnabled();
	RENDERCORE_API bool IsBackCompatibilityEnabled();
	RENDERCORE_API bool IsDBufferPassEnabled(EShaderPlatform InPlatform);
	RENDERCORE_API bool IsOpaqueRoughRefractionEnabled();
	RENDERCORE_API bool IsAdvancedVisualizationEnabled();
	RENDERCORE_API bool Is8bitTileCoordEnabled();
	RENDERCORE_API bool IsAccurateSRGBEnabled();

	RENDERCORE_API uint32 GetRayTracingMaterialPayloadSizeInBytes();
	RENDERCORE_API uint32 GetRayTracingMaterialPayloadSizeInBytes(EShaderPlatform InPlatform);

	RENDERCORE_API uint32 GetBytePerPixel();
	RENDERCORE_API uint32 GetBytePerPixel(EShaderPlatform InPlatform);

	RENDERCORE_API uint32 GetNormalQuality();

	RENDERCORE_API uint32 GetShadingQuality();
	RENDERCORE_API uint32 GetShadingQuality(EShaderPlatform InPlatform);
}