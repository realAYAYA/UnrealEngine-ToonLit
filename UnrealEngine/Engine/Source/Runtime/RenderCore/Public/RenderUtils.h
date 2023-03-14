// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "PackedNormal.h"
#include "RenderResource.h"
#include "RHIDefinitions.h"

class FBufferWithRDG;
class FRDGBuffer;
class FRDGPooledBuffer;
class FRDGTexture;
class FRDGBuilder;
struct IPooledRenderTarget;

class RENDERCORE_API FBufferWithRDG : public FRenderResource
{
public:
	FBufferWithRDG();
	FBufferWithRDG(const FBufferWithRDG& Other);
	FBufferWithRDG& operator=(const FBufferWithRDG& Other);
	~FBufferWithRDG() override;

	void ReleaseRHI() override;

	TRefCountPtr<FRDGPooledBuffer> Buffer;
};

extern RENDERCORE_API void RenderUtilsInit();

/**
* Constructs a basis matrix for the axis vectors and returns the sign of the determinant
*
* @param XAxis - x axis (tangent)
* @param YAxis - y axis (binormal)
* @param ZAxis - z axis (normal)
* @return sign of determinant either -1 or +1 
*/
FORCEINLINE float GetBasisDeterminantSign( const FVector& XAxis, const FVector& YAxis, const FVector& ZAxis )
{
	FMatrix Basis(
		FPlane(XAxis,0),
		FPlane(YAxis,0),
		FPlane(ZAxis,0),
		FPlane(0,0,0,1)
		);
	return (Basis.Determinant() < 0) ? -1.0f : +1.0f;
}

/**
* Constructs a basis matrix for the axis vectors and returns the sign of the determinant
*
* @param XAxis - x axis (tangent)
* @param YAxis - y axis (binormal)
* @param ZAxis - z axis (normal)
* @return sign of determinant either -127 (-1) or +1 (127)
*/
FORCEINLINE int8 GetBasisDeterminantSignByte( const FPackedNormal& XAxis, const FPackedNormal& YAxis, const FPackedNormal& ZAxis )
{
	return GetBasisDeterminantSign(XAxis.ToFVector(),YAxis.ToFVector(),ZAxis.ToFVector()) < 0 ? -127 : 127;
}

/**
 * Given 2 axes of a basis stored as a packed type, regenerates the y-axis tangent vector and scales by z.W
 * @param XAxis - x axis (tangent)
 * @param ZAxis - z axis (normal), the sign of the determinant is stored in ZAxis.W
 * @return y axis (binormal)
 */
template<typename VectorType>
FORCEINLINE FVector GenerateYAxis(const VectorType& XAxis, const VectorType& ZAxis)
{
	static_assert(	ARE_TYPES_EQUAL(VectorType, FPackedNormal) ||
					ARE_TYPES_EQUAL(VectorType, FPackedRGBA16N), "ERROR: Must be FPackedNormal or FPackedRGBA16N");
	FVector  x = XAxis.ToFVector();
	FVector4 z = ZAxis.ToFVector4();
	return (FVector(z) ^ x) * z.W;
}

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

/** A global white texture. */
extern RENDERCORE_API class FTexture* GWhiteTexture;
extern RENDERCORE_API class FTextureWithSRV* GWhiteTextureWithSRV;

/** A global black texture. */
extern RENDERCORE_API class FTexture* GBlackTexture;
extern RENDERCORE_API class FTextureWithSRV* GBlackTextureWithSRV;

extern RENDERCORE_API class FTexture* GTransparentBlackTexture;
extern RENDERCORE_API class FTextureWithSRV* GTransparentBlackTextureWithSRV;

extern RENDERCORE_API class FVertexBufferWithSRV* GEmptyVertexBufferWithUAV;

extern RENDERCORE_API class FVertexBufferWithSRV* GWhiteVertexBufferWithSRV;

extern RENDERCORE_API class FBufferWithRDG* GWhiteVertexBufferWithRDG;

/** A global black array texture. */
extern RENDERCORE_API class FTexture* GBlackArrayTexture;

/** A global black volume texture. */
extern RENDERCORE_API class FTexture* GBlackVolumeTexture;

/** A global black volume texture, with alpha=1. */
extern RENDERCORE_API class FTexture* GBlackAlpha1VolumeTexture;

/** A global black texture<uint> */
extern RENDERCORE_API class FTexture* GBlackUintTexture;

/** A global black volume texture<uint>  */
extern RENDERCORE_API class FTexture* GBlackUintVolumeTexture;

/** A global white cube texture. */
extern RENDERCORE_API class FTexture* GWhiteTextureCube;

/** A global black cube texture. */
extern RENDERCORE_API class FTexture* GBlackTextureCube;

/** A global black cube depth texture. */
extern RENDERCORE_API class FTexture* GBlackTextureDepthCube;

/** A global black cube array texture. */
extern RENDERCORE_API class FTexture* GBlackCubeArrayTexture;

/** A global texture that has a different solid color in each mip-level. */
extern RENDERCORE_API class FTexture* GMipColorTexture;

/** Number of mip-levels in 'GMipColorTexture' */
extern RENDERCORE_API int32 GMipColorTextureMipLevels;

// 4: 8x8 cubemap resolution, shader needs to use the same value as preprocessing
extern RENDERCORE_API const uint32 GDiffuseConvolveMipLevel;

#define NUM_CUBE_VERTICES 36
/** The indices for drawing a cube. */
extern RENDERCORE_API const uint16 GCubeIndices[36];

class FCubeIndexBuffer : public FIndexBuffer
{
public:
	/**
	* Initialize the RHI for this rendering resource
	*/
	virtual void InitRHI() override
	{
		// create a static vertex buffer
		FRHIResourceCreateInfo CreateInfo(TEXT("FCubeIndexBuffer"));
		IndexBufferRHI = RHICreateIndexBuffer(sizeof(uint16), sizeof(uint16) * NUM_CUBE_VERTICES, BUF_Static, CreateInfo);
		void* VoidPtr = RHILockBuffer(IndexBufferRHI, 0, sizeof(uint16) * NUM_CUBE_VERTICES, RLM_WriteOnly);
		FMemory::Memcpy(VoidPtr, GCubeIndices, NUM_CUBE_VERTICES * sizeof(uint16));
		RHIUnlockBuffer(IndexBufferRHI);
	}
};
extern RENDERCORE_API TGlobalResource<FCubeIndexBuffer> GCubeIndexBuffer;

class FTwoTrianglesIndexBuffer : public FIndexBuffer
{
public:
	/**
	* Initialize the RHI for this rendering resource
	*/
	virtual void InitRHI() override
	{
		// create a static vertex buffer
		FRHIResourceCreateInfo CreateInfo(TEXT("FTwoTrianglesIndexBuffer"));
		IndexBufferRHI = RHICreateIndexBuffer(sizeof(uint16), sizeof(uint16) * 6, BUF_Static, CreateInfo);
		void* VoidPtr = RHILockBuffer(IndexBufferRHI, 0, sizeof(uint16) * 6, RLM_WriteOnly);
		static const uint16 Indices[] = { 0, 1, 3, 0, 3, 2 };
		FMemory::Memcpy(VoidPtr, Indices, 6 * sizeof(uint16));
		RHIUnlockBuffer(IndexBufferRHI);
	}
};
extern RENDERCORE_API TGlobalResource<FTwoTrianglesIndexBuffer> GTwoTrianglesIndexBuffer;

class FScreenSpaceVertexBuffer : public FVertexBuffer
{
public:
	/**
	* Initialize the RHI for this rendering resource
	*/
	virtual void InitRHI() override
	{
		// create a static vertex buffer
		FRHIResourceCreateInfo CreateInfo(TEXT("FScreenSpaceVertexBuffer"));
		VertexBufferRHI = RHICreateVertexBuffer(sizeof(FVector2f) * 4, BUF_Static, CreateInfo);
		void* VoidPtr = RHILockBuffer(VertexBufferRHI, 0, sizeof(FVector2f) * 4, RLM_WriteOnly);
		static const FVector2f Vertices[4] =
		{
			FVector2f(-1,-1),
			FVector2f(-1,+1),
			FVector2f(+1,-1),
			FVector2f(+1,+1),
		};
		FMemory::Memcpy(VoidPtr, Vertices, sizeof(FVector2f) * 4);
		RHIUnlockBuffer(VertexBufferRHI);
	}
};

extern RENDERCORE_API TGlobalResource<FScreenSpaceVertexBuffer> GScreenSpaceVertexBuffer;

class FTileVertexDeclaration : public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;

	/** Destructor. */
	virtual ~FTileVertexDeclaration() {}

	virtual void InitRHI()
	{
		FVertexDeclarationElementList Elements;
		uint16 Stride = sizeof(FVector2f);
		Elements.Add(FVertexElement(0, 0, VET_Float2, 0, Stride, false));
		VertexDeclarationRHI = RHICreateVertexDeclaration(Elements);
	}

	virtual void ReleaseRHI()
	{
		VertexDeclarationRHI.SafeRelease();
	}
};

extern RENDERCORE_API TGlobalResource<FTileVertexDeclaration> GTileVertexDeclaration;

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

template<typename Type>
struct RENDERCORE_API FShaderPlatformCachedIniValue
{
	FShaderPlatformCachedIniValue(const TCHAR* InCVarName)
		: CVarName(InCVarName)
		, CVar(nullptr)
	{
	}

	FShaderPlatformCachedIniValue(IConsoleVariable* InCVar)
		: CVar(InCVar)
	{
	}

	Type Get(EShaderPlatform ShaderPlatform)
	{
		EShaderPlatform ActualShaderPlatform = ShaderPlatform;
		Type Value{};

		if (GIsEditor)
		{
			if (FDataDrivenShaderPlatformInfo::GetIsPreviewPlatform(ActualShaderPlatform))
			{
				ActualShaderPlatform = FDataDrivenShaderPlatformInfo::GetPreviewShaderPlatformParent(ActualShaderPlatform);
			}
		}

		FName IniPlatformName = ShaderPlatformToPlatformName(ActualShaderPlatform);
		// find the cvar if needed
		if (CVar == nullptr)
		{
			CVar = IConsoleManager::Get().FindConsoleVariable(*CVarName);
		}

		// if we are looking up our own platform, just use the current value, however
		// ShaderPlatformToPlatformName can return the wrong platform than expected - for instance, Linux Vulkan will return Windows
		// so instead of hitting an asser below, we detect that the request SP is the current SP, and use the CVar value that is set currently
		if (IniPlatformName == FPlatformProperties::IniPlatformName() || ActualShaderPlatform == GMaxRHIShaderPlatform)
		{
			checkf(CVar != nullptr, TEXT("Failed to find CVar %s when getting current value for FShaderPlatformCachedIniValue"));

			CVar->GetValue(Value);
			return Value;
		}

#if ALLOW_OTHER_PLATFORM_CONFIG
		// create a dummy cvar if needed
		if (CVar == nullptr)
		{
			// this could be a cvar that only exists on the target platform so create a dummy one
			CVar = IConsoleManager::Get().RegisterConsoleVariable(*CVarName, Type(), TEXT(""), ECVF_ReadOnly);
		}

		// now get the value from the platform that makes sense for this shader platform
		TSharedPtr<IConsoleVariable> OtherPlatformVar = CVar->GetPlatformValueVariable(IniPlatformName);
		ensureMsgf(OtherPlatformVar.IsValid(), TEXT("Failed to get another platform's version of a cvar (possible name: '%s'). It is probably an esoteric subclass that needs to implement GetPlatformValueVariable."), *CVarName);
		if (OtherPlatformVar.IsValid())
		{
			OtherPlatformVar->GetValue(Value);
		}
		else
		{
			// get this platform's value, even tho it could be wrong
			CVar->GetValue(Value);
		}
#else
		checkf(IniPlatformName == FName(FPlatformProperties::IniPlatformName()), TEXT("FShaderPlatformCachedIniValue can only look up the current platform when ALLOW_OTHER_PLATFORM_CONFIG is false"));
#endif
		return Value;
	}

private:
	FString CVarName;
	IConsoleVariable* CVar;
};

/** Returns if ForwardShading is enabled. Only valid for the current platform (otherwise call ITargetPlatform::UsesForwardShading()). */
inline bool IsForwardShadingEnabled(const FStaticShaderPlatform Platform)
{
	extern RENDERCORE_API ShaderPlatformMaskType GForwardShadingPlatformMask;
	return (GForwardShadingPlatformMask[(int)Platform])
		// Culling uses compute shader
		&& GetMaxSupportedFeatureLevel(Platform) >= ERHIFeatureLevel::SM5;
}

/** Returns if the GBuffer is used. Only valid for the current platform. */
inline bool IsUsingGBuffers(const FStaticShaderPlatform Platform)
{
	if (IsMobilePlatform(Platform))
	{
		return IsMobileDeferredShadingEnabled(Platform);
	}
	else
	{
		return !IsForwardShadingEnabled(Platform);
	}
}

/** Returns whether the base pass should output to the velocity buffer is enabled for a given shader platform */
RENDERCORE_API bool IsUsingBasePassVelocity(const FStaticShaderPlatform Platform);

/** Returns whether the base pass should use selective outputs for a given shader platform */
inline bool IsUsingSelectiveBasePassOutputs(const FStaticShaderPlatform Platform)
{
	extern RENDERCORE_API ShaderPlatformMaskType GSelectiveBasePassOutputsPlatformMask;
	return (GSelectiveBasePassOutputsPlatformMask[(int)Platform]);
}

/** Returns whether distance fields are enabled for a given shader platform */
inline bool IsUsingDistanceFields(const FStaticShaderPlatform Platform)
{
	extern RENDERCORE_API ShaderPlatformMaskType GDistanceFieldsPlatformMask;
	return (GDistanceFieldsPlatformMask[(int)Platform]);
}

/** Returns if water should render distance field shadow a second time for the water surface. This is for a platofrm so can be used at cook time. */
inline bool IsWaterDistanceFieldShadowEnabled(const FStaticShaderPlatform Platform)
{
	// Only deferred support such a feature. It is not possible to do that for water without a water depth pre-pass.
	static const auto CVarWaterSingleLayerShaderSupportDistanceFieldShadow = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Water.SingleLayer.ShadersSupportDistanceFieldShadow"));
	const bool bWaterSingleLayerShaderSupportDistanceFieldShadow = CVarWaterSingleLayerShaderSupportDistanceFieldShadow && (CVarWaterSingleLayerShaderSupportDistanceFieldShadow->GetInt() > 0);
	return !IsForwardShadingEnabled(Platform) && IsUsingDistanceFields(Platform) && bWaterSingleLayerShaderSupportDistanceFieldShadow;
}

inline bool UseGPUScene(const FStaticShaderPlatform Platform, const FStaticFeatureLevel FeatureLevel)
{
	if (FeatureLevel == ERHIFeatureLevel::ES3_1)
	{
		return MobileSupportsGPUScene();
	}
	
	// GPU Scene management uses compute shaders
	return FeatureLevel >= ERHIFeatureLevel::SM5 
		//@todo - support GPU Scene management compute shaders on these platforms to get dynamic instancing speedups on the Rendering Thread and RHI Thread
		&& !IsOpenGLPlatform(Platform)
		&& !IsVulkanMobileSM5Platform(Platform)
        && !IsMetalMobileSM5Platform(Platform)
		// we only check DDSPI for platforms that have been read in - IsValid() can go away once ALL platforms are converted over to this system
		&& (!FDataDrivenShaderPlatformInfo::IsValid(Platform) || FDataDrivenShaderPlatformInfo::GetSupportsGPUScene(Platform));
}


inline bool UseGPUScene(const FStaticShaderPlatform Platform)
{
	return UseGPUScene(Platform, GetMaxSupportedFeatureLevel(Platform));
}

inline bool ForceSimpleSkyDiffuse(const FStaticShaderPlatform Platform)
{
	extern RENDERCORE_API ShaderPlatformMaskType GSimpleSkyDiffusePlatformMask;
	return (GSimpleSkyDiffusePlatformMask[(int)Platform]);
}

inline bool VelocityEncodeDepth(const FStaticShaderPlatform Platform)
{
	extern RENDERCORE_API ShaderPlatformMaskType GVelocityEncodeDepthPlatformMask;
	return (GVelocityEncodeDepthPlatformMask[(int)Platform]);
}

/** Unit cube vertex buffer (VertexDeclarationFVector4) */
RENDERCORE_API FBufferRHIRef& GetUnitCubeVertexBuffer();

/** Unit cube index buffer */
RENDERCORE_API FBufferRHIRef& GetUnitCubeIndexBuffer();

#if RHI_RAYTRACING
/** Unit cube AABB vertex buffer (useful to create procedural raytracing geometry) */
RENDERCORE_API FBufferRHIRef& GetUnitCubeAABBVertexBuffer();
#endif

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
RENDERCORE_API bool UseVirtualTexturing(const EShaderPlatform InShaderPlatform, const class ITargetPlatform* TargetPlatform = nullptr);
RENDERCORE_API bool UseVirtualTexturing(const FStaticFeatureLevel InFeatureLevel, const class ITargetPlatform* TargetPlatform = nullptr);

RENDERCORE_API bool DoesPlatformSupportNanite(EShaderPlatform Platform, bool bCheckForProjectSetting = true);

inline bool NaniteAtomicsSupported()
{
	// Are 64bit image atomics supported by the GPU/Driver/OS/API?
	bool bAtomicsSupported = GRHISupportsAtomicUInt64;

#if PLATFORM_WINDOWS
	const ERHIInterfaceType RHIInterface = RHIGetInterfaceType();
	const bool bIsDx11 = RHIInterface == ERHIInterfaceType::D3D11;
	const bool bIsDx12 = RHIInterface == ERHIInterfaceType::D3D12;

	static const auto NaniteRequireDX12CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Nanite.RequireDX12"));
	static const uint32 NaniteRequireDX12 = (NaniteRequireDX12CVar != nullptr) ? NaniteRequireDX12CVar->GetInt() : 1;
	
	if (bAtomicsSupported && NaniteRequireDX12 != 0)
	{
		// Only allow Vulkan or D3D12
		bAtomicsSupported = !bIsDx11;

		// Disable DX12 vendor extensions unless DX12 SM6.6 is supported
		if (NaniteRequireDX12 == 1 && bIsDx12 && !GRHISupportsDX12AtomicUInt64)
		{
			// Vendor extensions currently support atomic64, but SM 6.6 and the DX12 Agility SDK are reporting that atomics are not supported.
			// Likely due to a pre-1909 Windows 10 version, or outdated drivers without SM 6.6 support.
			// See: https://devblogs.microsoft.com/directx/gettingstarted-dx12agility/
			bAtomicsSupported = false;
		}
	}
#endif

	return bAtomicsSupported;
}

inline bool DoesRuntimeSupportNanite(EShaderPlatform ShaderPlatform, bool bCheckForAtomicSupport, bool bCheckForProjectSetting)
{
	// Does the platform support Nanite?
	const bool bSupportedPlatform = DoesPlatformSupportNanite(ShaderPlatform, bCheckForProjectSetting);

	// Nanite is not supported with forward shading at this time.
	const bool bForwardShadingEnabled = IsForwardShadingEnabled(ShaderPlatform);

	return bSupportedPlatform && (!bCheckForAtomicSupport || NaniteAtomicsSupported()) && !bForwardShadingEnabled;
}

/**
 * Returns true if Nanite rendering should be used for the given shader platform.
 */
inline bool UseNanite(EShaderPlatform ShaderPlatform, bool bCheckForAtomicSupport = true, bool bCheckForProjectSetting = true)
{
	static const auto EnableNaniteCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Nanite"));
	const bool bNaniteEnabled   = (EnableNaniteCVar != nullptr) ? (EnableNaniteCVar->GetInt() != 0) : true;
	return bNaniteEnabled && DoesRuntimeSupportNanite(ShaderPlatform, bCheckForAtomicSupport, bCheckForProjectSetting);
}

/**
 * Returns true if Virtual Shadow Maps should be used for the given shader platform.
 * Note: Virtual Shadow Maps require Nanite support.
 */
inline bool UseVirtualShadowMaps(EShaderPlatform ShaderPlatform, const FStaticFeatureLevel FeatureLevel)
{
	static const auto EnableVirtualSMCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shadow.Virtual.Enable"));
	const bool bVirtualShadowMapsEnabled = EnableVirtualSMCVar ? (EnableVirtualSMCVar->GetInt() != 0) : false;
	return bVirtualShadowMapsEnabled && DoesRuntimeSupportNanite(ShaderPlatform, true /* check for atomics */, false /* check project setting */);
}

/**
* Returns true if Virtual Shadow Mapsare supported for the given shader platform.
* Note: Virtual Shadow Maps require Nanite platform support.
*/
inline bool DoesPlatformSupportVirtualShadowMaps(EShaderPlatform Platform)
{
	return DoesPlatformSupportNanite(Platform, false /* check project setting */);
}

/**
* Returns true if non-Nanite virtual shadow maps are enabled by CVar r.Shadow.Virtual.NonNaniteVSM
* and the runtime supports Nanite/virtual shadow maps.
*/
inline bool DoesPlatformSupportNonNaniteVirtualShadowMaps(EShaderPlatform ShaderPlatform)
{
	static const auto EnableCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shadow.Virtual.NonNaniteVSM"));	
	return EnableCVar->GetInt() != 0 && DoesPlatformSupportNanite(ShaderPlatform, false /* check project setting */);
}

/**
* Similar to DoesPlatformSupportNonNaniteVirtualShadowMaps, but checks if nanite and virtual shadow maps are enabled (at runtime).
*/
inline bool UseNonNaniteVirtualShadowMaps(EShaderPlatform ShaderPlatform, const FStaticFeatureLevel FeatureLevel)
{
	static const auto EnableCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shadow.Virtual.NonNaniteVSM"));
	return EnableCVar->GetInt() != 0 && UseVirtualShadowMaps(ShaderPlatform, FeatureLevel);
}

/**
*	(Non-runtime) Checks if the depth prepass for single layer water is enabled. This also depends on virtual shadow maps to be supported on the platform.
*/
inline bool IsSingleLayerWaterDepthPrepassEnabled(const FStaticShaderPlatform& Platform, const FStaticFeatureLevel& FeatureLevel)
{
	static const auto CVarWaterSingleLayerDepthPrepass = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Water.SingleLayer.DepthPrepass"));
	const bool bPrepassEnabled = CVarWaterSingleLayerDepthPrepass && CVarWaterSingleLayerDepthPrepass->GetInt() > 0;
	// Currently VSM is the only feature dependent on the depth prepass which is why we only enable it if VSM could also be enabled.
	// VSM can be toggled at runtime, but we need a compile time value here, so we fall back to DoesRuntimeSupportNanite() to check if
	// VSM *could* be enabled.
	const bool bVSMSupported = DoesPlatformSupportNanite(Platform, false /* check project setting */);

	return bPrepassEnabled && bVSMSupported;
}

/**
*	Checks if virtual texturing lightmap enabled and supported
*/
RENDERCORE_API bool UseVirtualTextureLightmap(const FStaticFeatureLevel InFeatureLevel, const class ITargetPlatform* TargetPlatform = nullptr);

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
RENDERCORE_API bool AllowTranslucencyPerObjectShadows(const FStaticShaderPlatform &Platform);
