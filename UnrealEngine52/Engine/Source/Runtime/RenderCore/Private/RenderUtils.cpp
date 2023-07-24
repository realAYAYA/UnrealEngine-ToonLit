// Copyright Epic Games, Inc. All Rights Reserved.

#include "RenderUtils.h"

#include "Containers/DynamicRHIResourceArray.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "HAL/ConsoleManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/DataDrivenPlatformInfoRegistry.h"
#include "PackedNormal.h"
#include "PipelineStateCache.h"
#include "RenderResource.h"
#include "RHI.h"
#include "Shader.h"
#include "ShaderPlatformCachedIniValue.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "RenderCore.h"

#if WITH_EDITOR
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "RHIShaderFormatDefinitions.inl"
#endif

// This is a per-project main switch for Nanite, that influences the shader permutations compiled. Changing it will cause shaders to be recompiled.
int32 GNaniteProjectEnabled = 1;
FAutoConsoleVariableRef CVarAllowNanite(
	TEXT("r.Nanite.ProjectEnabled"),
	GNaniteProjectEnabled,
	TEXT("This setting allows you to disable Nanite on platforms that support it to reduce the number of shaders. It cannot be used to force Nanite on on unsupported platforms.\n"),
	ECVF_ReadOnly | ECVF_RenderThreadSafe
);

int32 GAllowTranslucencyShadowsInProject = 0;
FAutoConsoleVariableRef CVarAllowTranslucencyShadowsInProject(
	TEXT("r.Shadow.TranslucentPerObject.ProjectEnabled"),
	GAllowTranslucencyShadowsInProject,
	TEXT("Enable/Disable translucency shadows on a per-project basis. Turning off can significantly reduce the number of permutations if your project has many translucent materials.\n"),
	ECVF_ReadOnly | ECVF_RenderThreadSafe
);

int32 GRayTracingEnableInGame = 1;
FAutoConsoleVariableRef CVarRayTracingEnableInGame(
	TEXT("r.RayTracing.EnableInGame"),
	GRayTracingEnableInGame,
	TEXT("Controls the default state of ray tracing effects when running the game. This setting is overridden by its counterpart in GameUserSettings.ini (if it exists) to allow control through in-game UI. ")
	TEXT("(default = 1)"),
	ECVF_ReadOnly
);

int32 GRayTracingEnableInEditor = 1;
FAutoConsoleVariableRef CVarRayTracingEnableInEditor(
	TEXT("r.RayTracing.EnableInEditor"),
	GRayTracingEnableInEditor,
	TEXT("Controls whether ray tracing effects are available by default when running the editor. This can be useful to improve editor performance when only some people require ray tracing features. ")
	TEXT("(default = 1)"),
	ECVF_ReadOnly
);

static int32 GRayTracingEnableOnDemand = 0;
static FAutoConsoleVariableRef CVarRayTracingEnableOnDemand(
	TEXT("r.RayTracing.EnableOnDemand"),
	GRayTracingEnableOnDemand,
	TEXT("Controls whether ray tracing features can be toggled on demand at runtime without restarting the game (experimental).\n")
	TEXT("Requires r.RayTracing=1 and will override GameUserSettings in game. Requires r.RayTracing.EnableInEditor=1 in editor. Has a small performance and memory overhead.\n")
	TEXT(" 0: off (default)\n")
	TEXT(" 1: on"),
	ECVF_RenderThreadSafe | ECVF_ReadOnly);

static int32 GRayTracingRequireSM6 = 0;
static FAutoConsoleVariableRef CVarRayTracingRequireSM6(
	TEXT("r.RayTracing.RequireSM6"),
	GRayTracingRequireSM6,
	TEXT("Whether ray tracing shaders and features should only be available when targetting and running SM6. If disabled, ray tracing shaders will also be available when running in SM5 mode. (default = 0, allow SM5 and SM6)"),
	ECVF_RenderThreadSafe | ECVF_ReadOnly
);

const uint16 GCubeIndices[12*3] =
{
	0, 2, 3,
	0, 3, 1,
	4, 5, 7,
	4, 7, 6,
	0, 1, 5,
	0, 5, 4,
	2, 6, 7,
	2, 7, 3,
	0, 4, 6,
	0, 6, 2,
	1, 3, 7,
	1, 7, 5,
};

//
// FPackedNormal serializer
//
FArchive& operator<<(FArchive& Ar, FDeprecatedSerializedPackedNormal& N)
{
	Ar << N.Vector.Packed;
	return Ar;
}

FArchive& operator<<(FArchive& Ar, FPackedNormal& N)
{
	Ar << N.Vector.Packed;
	return Ar;
}

FArchive& operator<<(FArchive& Ar, FPackedRGBA16N& N)
{
	Ar << N.X;
	Ar << N.Y;
	Ar << N.Z;
	Ar << N.W;
	return Ar;
}

/*
	3 XYZ packed in 4 bytes. (11:11:10 for X:Y:Z)
*/

/**
*	operator FVector - unpacked to -1 to 1
*/
FPackedPosition::operator FVector3f() const
{

	return FVector3f(Vector.X/1023.f, Vector.Y/1023.f, Vector.Z/511.f);
}

/**
* operator VectorRegister
*/
VectorRegister FPackedPosition::GetVectorRegister() const
{
	FVector3f UnpackedVect = *this;

	VectorRegister VectorToUnpack = VectorLoadFloat3_W0(&UnpackedVect);

	return VectorToUnpack;
}

/**
* Pack this vector(-1 to 1 for XYZ) to 4 bytes XYZ(11:11:10)
*/
void FPackedPosition::Set( const FVector3f& InVector )
{
	check (FMath::Abs<float>(InVector.X) <= 1.f && FMath::Abs<float>(InVector.Y) <= 1.f &&  FMath::Abs<float>(InVector.Z) <= 1.f);
	
#if !WITH_EDITORONLY_DATA
	// This should not happen in Console - this should happen during Cooking in PC
	check (false);
#else
	// Too confusing to use .5f - wanted to use the last bit!
	// Change to int for easier read
	Vector.X = FMath::Clamp<int32>(FMath::TruncToInt(InVector.X * 1023.0f),-1023,1023);
	Vector.Y = FMath::Clamp<int32>(FMath::TruncToInt(InVector.Y * 1023.0f),-1023,1023);
	Vector.Z = FMath::Clamp<int32>(FMath::TruncToInt(InVector.Z * 511.0f),-511,511);
#endif
}

// LWC_TODO: Perf pessimization
void FPackedPosition::Set(const FVector3d& InVector)
{
	Set(FVector3f(InVector));
}

/**
* operator << serialize
*/
FArchive& operator<<(FArchive& Ar,FPackedPosition& N)
{
	// Save N.Packed
	return Ar << N.Packed;
}

void CalcMipMapExtent3D( uint32 TextureSizeX, uint32 TextureSizeY, uint32 TextureSizeZ, EPixelFormat Format, uint32 MipIndex, uint32& OutXExtent, uint32& OutYExtent, uint32& OutZExtent )
{
	// UE-159189 to explain/fix why this forces min size of block size
	OutXExtent = FMath::Max<uint32>(TextureSizeX >> MipIndex, GPixelFormats[Format].BlockSizeX);
	OutYExtent = FMath::Max<uint32>(TextureSizeY >> MipIndex, GPixelFormats[Format].BlockSizeY);
	OutZExtent = FMath::Max<uint32>(TextureSizeZ >> MipIndex, GPixelFormats[Format].BlockSizeZ);
}

SIZE_T CalcTextureMipMapSize3D( uint32 TextureSizeX, uint32 TextureSizeY, uint32 TextureSizeZ, EPixelFormat Format, uint32 MipIndex )
{
	return GPixelFormats[Format].Get3DTextureMipSizeInBytes(TextureSizeX, TextureSizeY, TextureSizeZ, MipIndex);
}

SIZE_T CalcTextureSize3D( uint32 SizeX, uint32 SizeY, uint32 SizeZ, EPixelFormat Format, uint32 MipCount )
{
	return GPixelFormats[Format].Get3DTextureSizeInBytes(SizeX, SizeY, SizeZ, MipCount);
}

FIntPoint CalcMipMapExtent( uint32 TextureSizeX, uint32 TextureSizeY, EPixelFormat Format, uint32 MipIndex )
{
	// UE-159189 to explain/fix why this forces min size of block size
	return FIntPoint(FMath::Max<uint32>(TextureSizeX >> MipIndex, GPixelFormats[Format].BlockSizeX), FMath::Max<uint32>(TextureSizeY >> MipIndex, GPixelFormats[Format].BlockSizeY));
}

SIZE_T CalcTextureMipWidthInBlocks(uint32 TextureSizeX, EPixelFormat Format, uint32 MipIndex)
{
	const uint32 WidthInTexels = FMath::Max<uint32>(TextureSizeX >> MipIndex, 1);
	return GPixelFormats[Format].GetBlockCountForWidth(WidthInTexels);
}

SIZE_T CalcTextureMipHeightInBlocks(uint32 TextureSizeY, EPixelFormat Format, uint32 MipIndex)
{
	const uint32 HeightInTexels = FMath::Max<uint32>(TextureSizeY >> MipIndex, 1);
	return GPixelFormats[Format].GetBlockCountForHeight(HeightInTexels);
}

SIZE_T CalcTextureMipMapSize( uint32 TextureSizeX, uint32 TextureSizeY, EPixelFormat Format, uint32 MipIndex )
{
	return GPixelFormats[Format].Get2DTextureMipSizeInBytes(TextureSizeX, TextureSizeY, MipIndex);
}

SIZE_T CalcTextureSize( uint32 SizeX, uint32 SizeY, EPixelFormat Format, uint32 MipCount )
{
	return GPixelFormats[Format].Get2DTextureSizeInBytes(SizeX, SizeY, MipCount);
}

void CopyTextureData2D(const void* Source,void* Dest,uint32 SizeY,EPixelFormat Format,uint32 SourceStride,uint32 DestStride)
{
	const uint32 BlockSizeY = GPixelFormats[Format].BlockSizeY;
	const uint32 NumBlocksY = (SizeY + BlockSizeY - 1) / BlockSizeY;

	// a DestStride of 0 means to use the SourceStride
	if(SourceStride == DestStride || DestStride == 0)
	{
		// If the source and destination have the same stride, copy the data in one block.
		if (ensure(Source))
		{
			FMemory::ParallelMemcpy(Dest,Source,NumBlocksY * SourceStride, EMemcpyCachePolicy::StoreUncached);
		}
		else
		{
			FMemory::Memzero(Dest,NumBlocksY * SourceStride);
		}
	}
	else
	{
		// If the source and destination have different strides, copy each row of blocks separately.
		const uint32 NumBytesPerRow = FMath::Min<uint32>(SourceStride, DestStride);
		for(uint32 BlockY = 0;BlockY < NumBlocksY;++BlockY)
		{
			if (ensure(Source))
			{
				FMemory::ParallelMemcpy(
					(uint8*)Dest   + DestStride   * BlockY,
					(uint8*)Source + SourceStride * BlockY,
					NumBytesPerRow,
					EMemcpyCachePolicy::StoreUncached
					);
			}
			else
			{
				FMemory::Memzero((uint8*)Dest + DestStride * BlockY, NumBytesPerRow);
			}
		}
	}
}

EPixelFormatChannelFlags GetPixelFormatValidChannels(EPixelFormat InPixelFormat)
{
	static constexpr EPixelFormatChannelFlags PixelFormatToChannelFlags[] =
	{
		EPixelFormatChannelFlags::None,		// PF_Unknown,
		EPixelFormatChannelFlags::RGBA,		// PF_A32B32G32R32F
		EPixelFormatChannelFlags::RGBA,		// PF_B8G8R8A8
		EPixelFormatChannelFlags::R,		// PF_G8
		EPixelFormatChannelFlags::R,		// PF_G16
		EPixelFormatChannelFlags::RGB,		// PF_DXT1
		EPixelFormatChannelFlags::RGBA,		// PF_DXT3
		EPixelFormatChannelFlags::RGBA,		// PF_DXT5
		EPixelFormatChannelFlags::RG,		// PF_UYVY
		EPixelFormatChannelFlags::RGB,		// PF_FloatRGB
		EPixelFormatChannelFlags::RGBA,		// PF_FloatRGBA
		EPixelFormatChannelFlags::None,		// PF_DepthStencil
		EPixelFormatChannelFlags::None,		// PF_ShadowDepth
		EPixelFormatChannelFlags::R,		// PF_R32_FLOAT
		EPixelFormatChannelFlags::RG,		// PF_G16R16
		EPixelFormatChannelFlags::RG,		// PF_G16R16F
		EPixelFormatChannelFlags::RG,		// PF_G16R16F_FILTER
		EPixelFormatChannelFlags::RG,		// PF_G32R32F
		EPixelFormatChannelFlags::RGBA,		// PF_A2B10G10R10
		EPixelFormatChannelFlags::RGBA,		// PF_A16B16G16R16
		EPixelFormatChannelFlags::None,		// PF_D24
		EPixelFormatChannelFlags::R,		// PF_R16F
		EPixelFormatChannelFlags::R,		// PF_R16F_FILTER
		EPixelFormatChannelFlags::RG,		// PF_BC5
		EPixelFormatChannelFlags::RG,		// PF_V8U8
		EPixelFormatChannelFlags::A,		// PF_A1
		EPixelFormatChannelFlags::RGB,		// PF_FloatR11G11B10
		EPixelFormatChannelFlags::A,		// PF_A8
		EPixelFormatChannelFlags::R,		// PF_R32_UINT
		EPixelFormatChannelFlags::RGBA,		// PF_R32_SINT
		EPixelFormatChannelFlags::RGBA,		// PF_PVRTC2
		EPixelFormatChannelFlags::RGBA,		// PF_PVRTC4
		EPixelFormatChannelFlags::R,		// PF_R16_UINT
		EPixelFormatChannelFlags::R,		// PF_R16_SINT
		EPixelFormatChannelFlags::RGBA,		// PF_R16G16B16A16_UINT
		EPixelFormatChannelFlags::RGBA,		// PF_R16G16B16A16_SINT
		EPixelFormatChannelFlags::RGB,		// PF_R5G6B5_UNORM
		EPixelFormatChannelFlags::RGBA,		// PF_R8G8B8A8
		EPixelFormatChannelFlags::RGBA,		// PF_A8R8G8B8
		EPixelFormatChannelFlags::R,		// PF_BC4
		EPixelFormatChannelFlags::RG,		// PF_R8G8
		EPixelFormatChannelFlags::RGB,		// PF_ATC_RGB
		EPixelFormatChannelFlags::RGBA,		// PF_ATC_RGBA_E
		EPixelFormatChannelFlags::RGBA,		// PF_ATC_RGBA_I
		EPixelFormatChannelFlags::G,		// PF_X24_G8		
		EPixelFormatChannelFlags::RGB,		// PF_ETC1
		EPixelFormatChannelFlags::RGB,		// PF_ETC2_RGB
		EPixelFormatChannelFlags::RGBA,		// PF_ETC2_RGBA
		EPixelFormatChannelFlags::RGBA,		// PF_R32G32B32A32_UINT
		EPixelFormatChannelFlags::RG,		// PF_R16G16_UINT
		EPixelFormatChannelFlags::RGB,		// PF_ASTC_4x4
		EPixelFormatChannelFlags::RGB,		// PF_ASTC_6x6
		EPixelFormatChannelFlags::RGB,		// PF_ASTC_8x8
		EPixelFormatChannelFlags::RGB,		// PF_ASTC_10x10
		EPixelFormatChannelFlags::RGB,		// PF_ASTC_12x12
		EPixelFormatChannelFlags::RGB,		// PF_BC6H
		EPixelFormatChannelFlags::RGBA,		// PF_BC7
		EPixelFormatChannelFlags::R,		// PF_R8_UINT
		EPixelFormatChannelFlags::None,		// PF_L8
		EPixelFormatChannelFlags::RGBA,		// PF_XGXR8
		EPixelFormatChannelFlags::RGBA,		// PF_R8G8B8A8_UINT
		EPixelFormatChannelFlags::RGBA,		// PF_R8G8B8A8_SNORM
		EPixelFormatChannelFlags::RGBA,		// PF_R16G16B16A16_UNORM
		EPixelFormatChannelFlags::RGBA,		// PF_R16G16B16A16_SNORM
		EPixelFormatChannelFlags::RGBA,		// PF_PLATFORM_HDR_0
		EPixelFormatChannelFlags::RGBA,		// PF_PLATFORM_HDR_1
		EPixelFormatChannelFlags::RGBA,		// PF_PLATFORM_HDR_2
		EPixelFormatChannelFlags::None,		// PF_NV12
		EPixelFormatChannelFlags::RG,		// PF_R32G32_UINT
		EPixelFormatChannelFlags::R,		// PF_ETC2_R11_EAC
		EPixelFormatChannelFlags::RG,		// PF_ETC2_RG11_EAC
		EPixelFormatChannelFlags::R,		// PF_R8
		EPixelFormatChannelFlags::RGBA,		// PF_B5G5R5A1_UNORM
		EPixelFormatChannelFlags::RGB,		// PF_ASTC_4x4_HDR
		EPixelFormatChannelFlags::RGB,		// PF_ASTC_6x6_HDR
		EPixelFormatChannelFlags::RGB,		// PF_ASTC_8x8_HDR
		EPixelFormatChannelFlags::RGB,		// PF_ASTC_10x10_HDR
		EPixelFormatChannelFlags::RGB,		// PF_ASTC_12x12_HDR
		EPixelFormatChannelFlags::RG,		// PF_G16R16_SNORM
		EPixelFormatChannelFlags::RG,		// PF_R8G8_UINT
		EPixelFormatChannelFlags::RGB,		// PF_R32G32B32_UINT
		EPixelFormatChannelFlags::RGB,		// PF_R32G32B32_SINT
		EPixelFormatChannelFlags::RGB,		// PF_R32G32B32F
		EPixelFormatChannelFlags::R,		// PF_R8_SINT
		EPixelFormatChannelFlags::R,		// PF_R64_UINT
		EPixelFormatChannelFlags::RGB,		// PF_R9G9B9EXP5
		EPixelFormatChannelFlags::None,		// PF_P010
	};
	static_assert(UE_ARRAY_COUNT(PixelFormatToChannelFlags) == (uint8)PF_MAX, "Missing pixel format");
	return (InPixelFormat < PF_MAX) ? PixelFormatToChannelFlags[(uint8)InPixelFormat] : EPixelFormatChannelFlags::None;
}

const TCHAR* GetCubeFaceName(ECubeFace Face)
{
	switch(Face)
	{
	case CubeFace_PosX:
		return TEXT("PosX");
	case CubeFace_NegX:
		return TEXT("NegX");
	case CubeFace_PosY:
		return TEXT("PosY");
	case CubeFace_NegY:
		return TEXT("NegY");
	case CubeFace_PosZ:
		return TEXT("PosZ");
	case CubeFace_NegZ:
		return TEXT("NegZ");
	default:
		return TEXT("");
	}
}

ECubeFace GetCubeFaceFromName(const FString& Name)
{
	// not fast but doesn't have to be
	if(Name.EndsWith(TEXT("PosX")))
	{
		return CubeFace_PosX;
	}
	else if(Name.EndsWith(TEXT("NegX")))
	{
		return CubeFace_NegX;
	}
	else if(Name.EndsWith(TEXT("PosY")))
	{
		return CubeFace_PosY;
	}
	else if(Name.EndsWith(TEXT("NegY")))
	{
		return CubeFace_NegY;
	}
	else if(Name.EndsWith(TEXT("PosZ")))
	{
		return CubeFace_PosZ;
	}
	else if(Name.EndsWith(TEXT("NegZ")))
	{
		return CubeFace_NegZ;
	}

	return CubeFace_MAX;
}

class FVector4VertexDeclaration : public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;
	virtual void InitRHI() override
	{
		FVertexDeclarationElementList Elements;
		Elements.Add(FVertexElement(0, 0, VET_Float4, 0, sizeof(FVector4f)));
		VertexDeclarationRHI = PipelineStateCache::GetOrCreateVertexDeclaration(Elements);
	}
	virtual void ReleaseRHI() override
	{
		VertexDeclarationRHI.SafeRelease();
	}
};

TGlobalResource<FVector4VertexDeclaration> FVector4VertexDeclaration;

RENDERCORE_API FVertexDeclarationRHIRef& GetVertexDeclarationFVector4()
{
	return FVector4VertexDeclaration.VertexDeclarationRHI;
}

class FVector3VertexDeclaration : public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;
	virtual void InitRHI() override
	{
		FVertexDeclarationElementList Elements;
		Elements.Add(FVertexElement(0, 0, VET_Float3, 0, sizeof(FVector3f)));
		VertexDeclarationRHI = PipelineStateCache::GetOrCreateVertexDeclaration(Elements);
	}
	virtual void ReleaseRHI() override
	{
		VertexDeclarationRHI.SafeRelease();
	}
};

TGlobalResource<FVector3VertexDeclaration> GVector3VertexDeclaration;

RENDERCORE_API FVertexDeclarationRHIRef& GetVertexDeclarationFVector3()
{
	return GVector3VertexDeclaration.VertexDeclarationRHI;
}

class FVector2VertexDeclaration : public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;
	virtual void InitRHI() override
	{
		FVertexDeclarationElementList Elements;
		Elements.Add(FVertexElement(0, 0, VET_Float2, 0, sizeof(FVector2f)));
		VertexDeclarationRHI = PipelineStateCache::GetOrCreateVertexDeclaration(Elements);
	}
	virtual void ReleaseRHI() override
	{
		VertexDeclarationRHI.SafeRelease();
	}
};

TGlobalResource<FVector2VertexDeclaration> GVector2VertexDeclaration;

RENDERCORE_API FVertexDeclarationRHIRef& GetVertexDeclarationFVector2()
{
	return GVector2VertexDeclaration.VertexDeclarationRHI;
}

RENDERCORE_API bool MobileSupportsGPUScene()
{
	// make it shader platform setting?
	static TConsoleVariableData<int32>* CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.SupportGPUScene"));
	return (CVar && CVar->GetValueOnAnyThread() != 0) ? true : false;
}

RENDERCORE_API bool IsMobileDeferredShadingEnabled(const FStaticShaderPlatform Platform)
{
	static FShaderPlatformCachedIniValue<bool> MobileShadingPathIniValue(TEXT("r.Mobile.ShadingPath"));
	return 
		MobileShadingPathIniValue.Get(Platform) == 1 && 
		// OpenGL requires DXC for deferred shading
		(!IsOpenGLPlatform(Platform) || IsDxcEnabledForPlatform(Platform));
}

RENDERCORE_API bool MobileRequiresSceneDepthAux(const FStaticShaderPlatform Platform)
{
	static const auto CVarMobileHDR = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MobileHDR"));
	const bool bMobileHDR = (CVarMobileHDR && CVarMobileHDR->GetValueOnAnyThread() != 0);

	// SceneDepthAux is used on tiled GPUs (iOS, Android) with forward shading and on iOS only with deferred
	if (IsMetalMobilePlatform(Platform))
	{
		return true;
	}
	else if (bMobileHDR && !IsMobileDeferredShadingEnabled(Platform))
	{
		return (IsAndroidOpenGLESPlatform(Platform) || IsVulkanMobilePlatform(Platform));
	}
	return false;
}

RENDERCORE_API bool SupportsTextureCubeArray(ERHIFeatureLevel::Type FeatureLevel)
{
	return FeatureLevel >= ERHIFeatureLevel::SM5 
		// requries ES3.2 feature set
		|| IsMobileDeferredShadingEnabled(GetFeatureLevelShaderPlatform(FeatureLevel))
		|| MobileForwardEnableClusteredReflections(GetFeatureLevelShaderPlatform(FeatureLevel));
}

RENDERCORE_API bool MaskedInEarlyPass(const FStaticShaderPlatform Platform)
{
	static IConsoleVariable* CVarMobileEarlyZPassOnlyMaterialMasking = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Mobile.EarlyZPassOnlyMaterialMasking"));
	static IConsoleVariable* CVarEarlyZPassOnlyMaterialMasking = IConsoleManager::Get().FindConsoleVariable(TEXT("r.EarlyZPassOnlyMaterialMasking"));
	if (IsMobilePlatform(Platform))
	{
		return (CVarMobileEarlyZPassOnlyMaterialMasking && CVarMobileEarlyZPassOnlyMaterialMasking->GetInt() != 0);
	}
	else
	{
		return (CVarEarlyZPassOnlyMaterialMasking && CVarEarlyZPassOnlyMaterialMasking->GetInt() != 0);
	}
}

RENDERCORE_API bool AllowPixelDepthOffset(const FStaticShaderPlatform Platform)
{
	if (IsMobilePlatform(Platform))
	{
		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.AllowPixelDepthOffset"));
		return CVar->GetValueOnAnyThread() != 0;
	}
	return true;
}

RENDERCORE_API bool AllowPerPixelShadingModels(const FStaticShaderPlatform Platform)
{
	if (IsMobilePlatform(Platform))
	{
		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.AllowPerPixelShadingModels"));
		return CVar->GetValueOnAnyThread() != 0;
	}
	return true;
}

RENDERCORE_API uint32 GetPlatformShadingModelsMask(const FStaticShaderPlatform Platform)
{
	if (IsMobilePlatform(Platform))
	{
		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.ShadingModelsMask"));
		return CVar->GetValueOnAnyThread();
	}
	return 0xFFFFFFFF;
}

typedef TBitArray<TInlineAllocator<EShaderPlatform::SP_NumPlatforms / 8>> ShaderPlatformMaskType;

RENDERCORE_API ShaderPlatformMaskType GMobileAmbientOcclusionPlatformMask;

RENDERCORE_API bool IsMobileAmbientOcclusionEnabled(const FStaticShaderPlatform Platform)
{
	return IsMobilePlatform(Platform) && GMobileAmbientOcclusionPlatformMask[(int)Platform];
}

RENDERCORE_API bool IsMobileDistanceFieldEnabled(const FStaticShaderPlatform Platform)
{
	return IsMobilePlatform(Platform) && (FDataDrivenShaderPlatformInfo::GetSupportsMobileDistanceField(Platform)/* || IsD3DPlatform(Platform)*/) && IsUsingDistanceFields(Platform);
}

RENDERCORE_API bool IsMobileMovableSpotlightShadowsEnabled(const FStaticShaderPlatform Platform)
{
	static FShaderPlatformCachedIniValue<bool> MobileMovableSpotlightShadowsEnabledIniValue(TEXT("r.Mobile.EnableMovableSpotlightsShadow"));
	return MobileMovableSpotlightShadowsEnabledIniValue.Get(Platform);
}

RENDERCORE_API bool MobileForwardEnableLocalLights(const FStaticShaderPlatform Platform)
{
	static FShaderPlatformCachedIniValue<bool> MobileForwardEnableLocalLightsIniValue(TEXT("r.Mobile.Forward.EnableLocalLights"));
	return MobileForwardEnableLocalLightsIniValue.Get(Platform);
}

RENDERCORE_API bool MobileForwardEnableClusteredReflections(const FStaticShaderPlatform Platform)
{
	static FShaderPlatformCachedIniValue<bool> MobileForwardEnableClusteredReflectionsIniValue(TEXT("r.Mobile.Forward.EnableClusteredReflections"));
	return MobileForwardEnableClusteredReflectionsIniValue.Get(Platform);
}

RENDERCORE_API bool MobileUsesShadowMaskTexture(const FStaticShaderPlatform Platform)
{
	// Only distance field shadow needs to render shadow mask texture on mobile deferred, normal shadows need to be rendered separately because of handling lighting channels.
	// Besides distance field shadow, with clustered lighting and shadow of local light enabled, shadows will render to shadow mask texture on mobile forward, lighting channels are handled in base pass shader.
	return IsMobileDistanceFieldEnabled(Platform) || (!IsMobileDeferredShadingEnabled(Platform) && IsMobileMovableSpotlightShadowsEnabled(Platform) && MobileForwardEnableLocalLights(Platform));
}

// Whether to support more than 4 color attachments for GBuffer 
RENDERCORE_API bool MobileUsesExtenedGBuffer(FStaticShaderPlatform ShaderPlatform)
{
	// Android GLES: uses PLS for deferred shading and limited to 128 bits
	// Vulkan requires:
		// maxDescriptorSetInputAttachments > 4
		// maxColorAttachments > 4
	// iOS: A8+
	return (ShaderPlatform != SP_OPENGL_ES3_1_ANDROID) && false;
}

// Required for shading models with a custom data
RENDERCORE_API bool MobileUsesGBufferCustomData(FStaticShaderPlatform ShaderPlatform)
{
	static const auto CVarAllowStaticLighting = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AllowStaticLighting"));
	static const bool bAllowStaticLighting = CVarAllowStaticLighting->GetValueOnAnyThread() != 0;
	// we can pack CustomData into static lighting related space
	return MobileUsesExtenedGBuffer(ShaderPlatform) || !bAllowStaticLighting;
}

RENDERCORE_API bool MobileBasePassAlwaysUsesCSM(const FStaticShaderPlatform Platform)
{
	static TConsoleVariableData<int32>* CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.Shadow.CSMShaderCullingMethod"));
	if (IsMobileDeferredShadingEnabled(Platform))
	{
		// deferred shading does not need CSM culling
		return true;
	}
	else
	{
		return CVar && (CVar->GetValueOnAnyThread() & 0xF) == 5 && IsMobileDistanceFieldEnabled(Platform);
	}
}

RENDERCORE_API bool SupportsGen4TAA(const FStaticShaderPlatform Platform)
{
	if (IsMobilePlatform(Platform))
	{
		static FShaderPlatformCachedIniValue<bool> MobileSupportsGen4TAAIniValue(TEXT("r.Mobile.SupportsGen4TAA"));
		return (MobileSupportsGen4TAAIniValue.Get(Platform) != 0);
	}

	return true;
}

RENDERCORE_API bool SupportsTSR(const FStaticShaderPlatform Platform)
{
	return FDataDrivenShaderPlatformInfo::GetSupportsGen5TemporalAA(Platform);
}

EShaderPlatform GetEditorShaderPlatform(EShaderPlatform ShaderPlatform)
{
	EShaderPlatform ActualShaderPlatform = ShaderPlatform;

	if (GIsEditor)
	{
		if (FDataDrivenShaderPlatformInfo::GetIsPreviewPlatform(ActualShaderPlatform))
		{
			ActualShaderPlatform = FDataDrivenShaderPlatformInfo::GetPreviewShaderPlatformParent(ActualShaderPlatform);
		}
	}

	return ActualShaderPlatform;
}

RENDERCORE_API int32 GUseForwardShading = 0;
static FAutoConsoleVariableRef CVarForwardShading(
	TEXT("r.ForwardShading"),
	GUseForwardShading,
	TEXT("Whether to use forward shading on desktop platforms - requires Shader Model 5 hardware.\n")
	TEXT("Forward shading has lower constant cost, but fewer features supported. 0:off, 1:on\n")
	TEXT("This rendering path is a work in progress with many unimplemented features, notably only a single reflection capture is applied per object and no translucency dynamic shadow receiving."),
	ECVF_RenderThreadSafe | ECVF_ReadOnly
	); 

static TAutoConsoleVariable<int32> CVarGBufferDiffuseSampleOcclusion(
	TEXT("r.GBufferDiffuseSampleOcclusion"), 0,
	TEXT("Whether the gbuffer contain occlusion information for individual diffuse samples."),
	ECVF_RenderThreadSafe | ECVF_ReadOnly
	); 

static TAutoConsoleVariable<int32> CVarDistanceFields(
	TEXT("r.DistanceFields"),
	1,
	TEXT("Enables distance fields rendering.\n") \
	TEXT(" 0: Disabled.\n") \
	TEXT(" 1: Enabled."),
	ECVF_RenderThreadSafe | ECVF_ReadOnly
	); 


RENDERCORE_API ShaderPlatformMaskType GForwardShadingPlatformMask;
RENDERCORE_API ShaderPlatformMaskType GDBufferPlatformMask;
RENDERCORE_API ShaderPlatformMaskType GVelocityEncodeDepthPlatformMask;
RENDERCORE_API ShaderPlatformMaskType GSelectiveBasePassOutputsPlatformMask;
RENDERCORE_API ShaderPlatformMaskType GDistanceFieldsPlatformMask;
RENDERCORE_API ShaderPlatformMaskType GSimpleSkyDiffusePlatformMask;

// Specifies whether ray tracing *can* be enabled on a particular platform.
// This takes into account whether RT is globally enabled for the project and specifically enabled on a target platform.
// Safe to use to make cook-time decisions, such as whether to compile ray tracing shaders.
RENDERCORE_API ShaderPlatformMaskType GRayTracingPlatformMask;

// Specifies whether ray tracing *is* enabled on the current running system (in current game or editor process).
// This takes into account additional factors, such as concrete current GPU/OS/Driver capability, user-set game graphics options, etc.
// Only safe to make run-time decisions, such as whether to build acceleration structures and render ray tracing effects.
// Value may be queried using IsRayTracingEnabled().
RENDERCORE_API ERayTracingMode GRayTracingMode = ERayTracingMode::Disabled;

void GetAllPossiblePreviewPlatformsForMainShaderPlatform(TArray<EShaderPlatform>& OutPreviewPlatforms, EShaderPlatform ParentShaderPlatform)
{
	for (int i = 0; i < EShaderPlatform::SP_NumPlatforms; ++i)
	{
		EShaderPlatform ShaderPlatform = EShaderPlatform(i);
		if (FDataDrivenShaderPlatformInfo::IsValid(ShaderPlatform))
		{
			bool bIsPreviewPlatform = FDataDrivenShaderPlatformInfo::GetIsPreviewPlatform(ShaderPlatform) && (FDataDrivenShaderPlatformInfo::GetPreviewShaderPlatformParent(ShaderPlatform) == ParentShaderPlatform);
			if (bIsPreviewPlatform)
			{
				OutPreviewPlatforms.Add(ShaderPlatform);
			}
		}
	}
}

RENDERCORE_API void RenderUtilsInit()
{
	checkf(GIsRHIInitialized, TEXT("RenderUtilsInit() may only be called once RHI is initialized."));

	GForwardShadingPlatformMask.Init(GUseForwardShading == 1, EShaderPlatform::SP_NumPlatforms);

	static IConsoleVariable* DBufferVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DBuffer"));
	GDBufferPlatformMask.Init(DBufferVar && DBufferVar->GetInt(), EShaderPlatform::SP_NumPlatforms);

	static IConsoleVariable* SelectiveBasePassOutputsCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.SelectiveBasePassOutputs"));
	GSelectiveBasePassOutputsPlatformMask.Init(SelectiveBasePassOutputsCVar && SelectiveBasePassOutputsCVar->GetInt(), EShaderPlatform::SP_NumPlatforms);

	static IConsoleVariable* DistanceFieldsCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DistanceFields")); 
	GDistanceFieldsPlatformMask.Init(DistanceFieldsCVar && DistanceFieldsCVar->GetInt(), EShaderPlatform::SP_NumPlatforms);

	static IConsoleVariable* RayTracingCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.RayTracing"));

	GSimpleSkyDiffusePlatformMask.Init(false, EShaderPlatform::SP_NumPlatforms);
	GVelocityEncodeDepthPlatformMask.Init(false, EShaderPlatform::SP_NumPlatforms);
	GRayTracingPlatformMask.Init(false, EShaderPlatform::SP_NumPlatforms);

	static IConsoleVariable* MobileAmbientOcclusionCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Mobile.AmbientOcclusion"));
	GMobileAmbientOcclusionPlatformMask.Init(MobileAmbientOcclusionCVar && MobileAmbientOcclusionCVar->GetInt(), EShaderPlatform::SP_NumPlatforms);

#if WITH_EDITOR
	ITargetPlatformManagerModule* TargetPlatformManager = GetTargetPlatformManager();
	if (TargetPlatformManager)
	{
		for (ITargetPlatform* TargetPlatform : TargetPlatformManager->GetTargetPlatforms())
		{
			TArray<FName> PlatformPossibleShaderFormats;
			TargetPlatform->GetAllPossibleShaderFormats(PlatformPossibleShaderFormats);

			for (FName Format : PlatformPossibleShaderFormats)
			{
				EShaderPlatform ShaderPlatform = ShaderFormatNameToShaderPlatform(Format);
				TArray<EShaderPlatform> PossiblePreviewPlatformsAndMainPlatform;
				GetAllPossiblePreviewPlatformsForMainShaderPlatform(PossiblePreviewPlatformsAndMainPlatform, ShaderPlatform);
				PossiblePreviewPlatformsAndMainPlatform.Add(ShaderPlatform);

				for (EShaderPlatform ShaderPlatformToEdit : PossiblePreviewPlatformsAndMainPlatform)
				{
					uint32 ShaderPlatformIndex = static_cast<uint32>(ShaderPlatformToEdit);

					uint64 Mask = 1ull << ShaderPlatformToEdit;

					if (!FDataDrivenShaderPlatformInfo::IsValid(ShaderPlatformToEdit))
					{
						continue;
					}

					GForwardShadingPlatformMask[ShaderPlatformIndex] = TargetPlatform->UsesForwardShading();

					GDBufferPlatformMask[ShaderPlatformIndex] = TargetPlatform->UsesDBuffer() && !IsMobilePlatform(ShaderPlatformToEdit);

					GSelectiveBasePassOutputsPlatformMask[ShaderPlatformIndex] = TargetPlatform->UsesSelectiveBasePassOutputs();

					GDistanceFieldsPlatformMask[ShaderPlatformIndex] = TargetPlatform->UsesDistanceFields();

					GSimpleSkyDiffusePlatformMask[ShaderPlatformIndex] = TargetPlatform->ForcesSimpleSkyDiffuse();

					GVelocityEncodeDepthPlatformMask[ShaderPlatformIndex] = TargetPlatform->VelocityEncodeDepth();

					GMobileAmbientOcclusionPlatformMask[ShaderPlatformIndex] = TargetPlatform->UsesMobileAmbientOcclusion();
				}
			}


			if (TargetPlatform->UsesRayTracing())
			{
				TArray<FName> PlatformRayTracingShaderFormats;
				TargetPlatform->GetRayTracingShaderFormats(PlatformRayTracingShaderFormats);

				for (FName FormatName : PlatformRayTracingShaderFormats)
				{
					EShaderPlatform MainShaderPlatform = ShaderFormatNameToShaderPlatform(FormatName);
					TArray<EShaderPlatform> PossiblePreviewPlatformsAndMainPlatform;
					GetAllPossiblePreviewPlatformsForMainShaderPlatform(PossiblePreviewPlatformsAndMainPlatform, MainShaderPlatform);

					PossiblePreviewPlatformsAndMainPlatform.Add(MainShaderPlatform);

					for (EShaderPlatform ShaderPlatform : PossiblePreviewPlatformsAndMainPlatform)
					{
						uint32 ShaderPlatformIndex = static_cast<uint32>(ShaderPlatform);
						GRayTracingPlatformMask[ShaderPlatformIndex] = true;
					}
				}
			}
		}
	}

#else // WITH_EDITOR

	if (IsMobilePlatform(GMaxRHIShaderPlatform))
	{
		GDBufferPlatformMask.Init(false, EShaderPlatform::SP_NumPlatforms);
	}

	if (RayTracingCVar && RayTracingCVar->GetInt() && GRHISupportsRayTracing)
	{
		GRayTracingPlatformMask.Init(true, EShaderPlatform::SP_NumPlatforms);
	}

	// Load runtime values from and *.ini file used by a current platform
	// Should be code shared between cook and game, but unfortunately can't be done before we untangle non data driven platforms
	const FString PlatformName(FPlatformProperties::IniPlatformName());
	const FDataDrivenPlatformInfo& PlatformInfo = FDataDrivenPlatformInfoRegistry::GetPlatformInfo(PlatformName);

	const FString CategoryName = PlatformInfo.TargetSettingsIniSectionName;
	if (!CategoryName.IsEmpty())
	{
		FConfigFile PlatformIniFile;
		if (FConfigCacheIni::LoadLocalIniFile(PlatformIniFile, TEXT("Engine"), /*bIsBaseIniName*/ true, *PlatformName))
		{
			bool bDistanceFields = false;
			if (PlatformIniFile.GetBool(*CategoryName, TEXT("bEnableDistanceFields"), bDistanceFields) && !bDistanceFields)
			{
				GDistanceFieldsPlatformMask.Init(false, EShaderPlatform::SP_NumPlatforms);
			}

			bool bRayTracing = false;
			if (PlatformIniFile.GetBool(*CategoryName, TEXT("bEnableRayTracing"), bRayTracing) && !bRayTracing)
			{
				GRayTracingPlatformMask.Init(false, EShaderPlatform::SP_NumPlatforms);
			}
		}
	}
#endif // WITH_EDITOR

	// Run-time ray tracing support depends on the following factors:
	// - Ray tracing must be enabled for the project
	// - Skin cache must be enabled for the project
	// - Current GPU, OS and driver must support ray tracing
	// - User is running the Editor and r.RayTracing.EnableInEditor=1 
	//   *OR* running the game with ray tracing enabled in graphics options

	// When ray tracing is enabled, we must load additional shaders and build acceleration structures for meshes.
	// For this reason it is only possible to enable RT at startup and changing the state requires restart.
	// This is also the reason why IsRayTracingEnabled() lives in RenderCore module, as it controls creation of 
	// RT pipelines in ShaderPipelineCache.cpp.

	// There are 2 modes in which ray tracing can be enabled:
	// 1 - Everything necessary is built and available if platform supports ray tracing
	// 2 - Same as above but ray tracing can be toggled on/off at runtime

	auto GetRayTracingModeName = [](ERayTracingMode Mode)
	{
		switch (Mode)
		{
		case ERayTracingMode::Enabled:
			return TEXT("enabled");
		case ERayTracingMode::Dynamic:
			return TEXT("enabled (dynamic)");
		}
		return TEXT("disabled");
	};

	if (RayTracingCVar && RayTracingCVar->GetInt())
	{
		const int32 RayTracingInt = RayTracingCVar->GetInt();

		ERayTracingMode DesiredRayTracingMode = ERayTracingMode::Disabled;
		if (RayTracingInt != 0)
		{
			if (GRayTracingEnableOnDemand == 1)
			{
				DesiredRayTracingMode = ERayTracingMode::Dynamic;
			}
			else
			{
				DesiredRayTracingMode = ERayTracingMode::Enabled;
			}
		}

		const bool bRayTracingAllowedOnCurrentPlatform = (GRayTracingPlatformMask[(int)GMaxRHIShaderPlatform]);
		if (GRHISupportsRayTracing && bRayTracingAllowedOnCurrentPlatform)
		{
			if (GIsEditor)
			{
				// Ray tracing is enabled for the project and we are running on RT-capable machine,
				// therefore the core ray tracing features are also enabled, so that required shaders
				// are loaded, acceleration structures are built, etc.
				GRayTracingMode = (GRayTracingEnableInEditor != 0) ? DesiredRayTracingMode : ERayTracingMode::Disabled;

				UE_LOG(LogRendererCore, Log, TEXT("Ray tracing is %s for the editor. Reason: r.RayTracing=%d and r.RayTracing.EnableInEditor=%d."),
					GetRayTracingModeName(GRayTracingMode),
					RayTracingInt,
					GRayTracingEnableInEditor);
			}
			else
			{
				// If user preference exists in game settings file, the bRayTracingEnabled will be set based on its value.
				// Otherwise the current value is preserved.
				bool bUseRayTracing = false;
				if (GRayTracingEnableOnDemand == 1)
				{
					GRayTracingMode = DesiredRayTracingMode;

					UE_LOG(LogRendererCore, Log, TEXT("Ray tracing is %s for the game. Reason: r.RayTracing=%d and r.RayTracing.EnableOnDemand=1."),
						GetRayTracingModeName(GRayTracingMode),
						RayTracingInt);
				}
				else if (GConfig->GetBool(TEXT("RayTracing"), TEXT("r.RayTracing.EnableInGame"), bUseRayTracing, GGameUserSettingsIni))
				{
					GRayTracingMode = bUseRayTracing ? DesiredRayTracingMode : ERayTracingMode::Disabled;

					UE_LOG(LogRendererCore, Log, TEXT("Ray tracing is %s for the game. Reason: game user setting r.RayTracing.EnableInGame=%d."),
						GetRayTracingModeName(GRayTracingMode),
						(int)bUseRayTracing);
				}
				else
				{
					GRayTracingMode = GRayTracingEnableInGame != 0 ? DesiredRayTracingMode : ERayTracingMode::Disabled;

					UE_LOG(LogRendererCore, Log, TEXT("Ray tracing is %s for the game. Reason: CVar r.RayTracing=%d, and r.RayTracing.EnableInGame game user setting does not exist (using default from CVar: %d)."),
						GetRayTracingModeName(GRayTracingMode),
						RayTracingInt,
						GRayTracingEnableInGame);
				}
			}

			// Sanity check: skin cache is *required* for ray tracing.
			// It can be dynamically enabled only when its shaders have been compiled.
			IConsoleVariable* SkinCacheCompileShadersCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.SkinCache.CompileShaders"));
			const bool bUseRayTracing = (int32)GRayTracingMode >= (int32)ERayTracingMode::Enabled;
			if (bUseRayTracing && SkinCacheCompileShadersCVar->GetInt() <= 0)
			{
				GRayTracingMode = ERayTracingMode::Disabled;

				UE_LOG(LogRendererCore, Fatal, TEXT("Ray tracing requires skin cache to be enabled. Set r.SkinCache.CompileShaders=1."));
			}

		}
		else
		{
			if (!GRHISupportsRayTracing)
			{
				UE_LOG(LogRendererCore, Log, TEXT("Ray tracing is disabled. Reason: not supported by current RHI."));
			}
			else
			{
				UE_LOG(LogRendererCore, Log, TEXT("Ray tracing is disabled. Reason: disabled on current platform."));
			}
		}
	}
	else
	{
		UE_LOG(LogRendererCore, Log, TEXT("Ray tracing is disabled. Reason: disabled through project setting (r.RayTracing=0)."));
	}
}

class FUnitCubeVertexBuffer : public FVertexBuffer
{
public:
	/**
	* Initialize the RHI for this rendering resource
	*/
	void InitRHI() override
	{
		const int32 NumVerts = 8;
		TResourceArray<FVector4f, VERTEXBUFFER_ALIGNMENT> Verts;
		Verts.SetNumUninitialized(NumVerts);

		for (uint32 Z = 0; Z < 2; Z++)
		{
			for (uint32 Y = 0; Y < 2; Y++)
			{
				for (uint32 X = 0; X < 2; X++)
				{
					const FVector4f Vertex = FVector4f(
					  (X ? -1.f : 1.f),
					  (Y ? -1.f : 1.f),
					  (Z ? -1.f : 1.f),
					  1.f
					);

					Verts[GetCubeVertexIndex(X, Y, Z)] = Vertex;
				}
			}
		}

		uint32 Size = Verts.GetResourceDataSize();

		// Create vertex buffer. Fill buffer with initial data upon creation
		FRHIResourceCreateInfo CreateInfo(TEXT("FUnitCubeVertexBuffer"), &Verts);
		VertexBufferRHI = RHICreateVertexBuffer(Size, BUF_Static, CreateInfo);
	}
};

class FUnitCubeIndexBuffer : public FIndexBuffer
{
public:
	/**
	* Initialize the RHI for this rendering resource
	*/
	void InitRHI() override
	{
		TResourceArray<uint16, INDEXBUFFER_ALIGNMENT> Indices;
		
		int32 NumIndices = UE_ARRAY_COUNT(GCubeIndices);
		Indices.AddUninitialized(NumIndices);
		FMemory::Memcpy(Indices.GetData(), GCubeIndices, NumIndices * sizeof(uint16));

		const uint32 Size = Indices.GetResourceDataSize();
		const uint32 Stride = sizeof(uint16);

		// Create index buffer. Fill buffer with initial data upon creation
		FRHIResourceCreateInfo CreateInfo(TEXT("FUnitCubeIndexBuffer"), &Indices);
		IndexBufferRHI = RHICreateIndexBuffer(Stride, Size, BUF_Static, CreateInfo);
	}
};

#if RHI_RAYTRACING

class FUnitCubeAABBVertexBuffer : public FVertexBuffer
{
public:
	/**
	* Initialize the RHI for this rendering resource
	*/
	void InitRHI() override
	{
		const int32 NumVerts = 2;
		TResourceArray<FVector3f, VERTEXBUFFER_ALIGNMENT> Verts;
		Verts.SetNumUninitialized(NumVerts);
		Verts[0] = FVector3f(-0.5f, -0.5f, -0.5f);
		Verts[1] = FVector3f(0.5f, 0.5f, 0.5f);

		uint32 Size = Verts.GetResourceDataSize();

		// Create vertex buffer. Fill buffer with initial data upon creation
		FRHIResourceCreateInfo CreateInfo(TEXT("FUnitCubeAABBVertexBuffer"), &Verts);
		VertexBufferRHI = RHICreateVertexBuffer(Size, BUF_Static, CreateInfo);
	}
};
#endif // RHI_RAYTRACING

static TGlobalResource<FUnitCubeVertexBuffer> GUnitCubeVertexBuffer;
static TGlobalResource<FUnitCubeIndexBuffer> GUnitCubeIndexBuffer;
#if RHI_RAYTRACING
static TGlobalResource<FUnitCubeAABBVertexBuffer> GUnitCubeAABBVertexBuffer;
#endif // RHI_RAYTRACING

RENDERCORE_API FBufferRHIRef& GetUnitCubeVertexBuffer()
{
	return GUnitCubeVertexBuffer.VertexBufferRHI;
}

RENDERCORE_API FBufferRHIRef& GetUnitCubeIndexBuffer()
{
	return GUnitCubeIndexBuffer.IndexBufferRHI;
}

#if RHI_RAYTRACING
RENDERCORE_API FBufferRHIRef& GetUnitCubeAABBVertexBuffer()
{
	return GUnitCubeAABBVertexBuffer.VertexBufferRHI;
}
#endif // RHI_RAYTRACING

RENDERCORE_API void QuantizeSceneBufferSize(const FIntPoint& InBufferSize, FIntPoint& OutBufferSize)
{
	// Ensure sizes are dividable by STRATA_TILE_SIZE (==8) 2d tiles to make it more convenient.
	const uint32 StrataDividableBy = 8;
	static_assert(StrataDividableBy % 8 == 0, "A lot of graphic algorithms where previously assuming DividableBy >= 4");

	// Ensure sizes are dividable by the ideal group size for 2d tiles to make it more convenient.
	const uint32 LegacyDividableBy = 4;
	static_assert(LegacyDividableBy % 4 == 0, "A lot of graphic algorithms where previously assuming DividableBy == 4");

	const uint32 DividableBy = Strata::IsStrataEnabled() ? StrataDividableBy : LegacyDividableBy;

	const uint32 Mask = ~(DividableBy - 1);
	OutBufferSize.X = (InBufferSize.X + DividableBy - 1) & Mask;
	OutBufferSize.Y = (InBufferSize.Y + DividableBy - 1) & Mask;
}

bool UseVirtualTexturing(bool bIsMobilePlatform, const ITargetPlatform* TargetPlatform)
{
#if PLATFORM_SUPPORTS_VIRTUAL_TEXTURE_STREAMING
	if (!FPlatformProperties::SupportsVirtualTextureStreaming())
	{
		return false;
	}

	// does the project has it enabled ?
	static const auto CVarVirtualTexture = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.VirtualTextures"));
	check(CVarVirtualTexture);
	if (CVarVirtualTexture->GetValueOnAnyThread() == 0)
	{
		return false;
	}		

	// mobile needs an additional switch to enable VT		
	static const auto CVarMobileVirtualTexture = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.VirtualTextures"));
	if (bIsMobilePlatform && CVarMobileVirtualTexture->GetValueOnAnyThread() == 0)
	{
		return false;
	}

	return true;
#else
	return false;
#endif
}

RENDERCORE_API bool UseVirtualTexturing(const EShaderPlatform InShaderPlatform, const ITargetPlatform* TargetPlatform)
{
	return UseVirtualTexturing(IsMobilePlatform(InShaderPlatform), TargetPlatform);
}

RENDERCORE_API bool UseVirtualTexturing(const FStaticFeatureLevel InFeatureLevel, const ITargetPlatform* TargetPlatform)
{
	return UseVirtualTexturing(InFeatureLevel == ERHIFeatureLevel::ES3_1, TargetPlatform);
}

RENDERCORE_API bool UseVirtualTextureLightmap(const FStaticFeatureLevel InFeatureLevel, const ITargetPlatform* TargetPlatform)
{
	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.VirtualTexturedLightmaps"));
	const bool bUseVirtualTextureLightmap = (CVar->GetValueOnAnyThread() != 0) && UseVirtualTexturing(InFeatureLevel, TargetPlatform);
	return bUseVirtualTextureLightmap;
}

RENDERCORE_API bool UseNaniteLandscapeMesh(EShaderPlatform ShaderPlatform)
{
	return DoesPlatformSupportNanite(ShaderPlatform);
}

RENDERCORE_API bool ExcludeNonPipelinedShaderTypes(EShaderPlatform ShaderPlatform)
{
	if (RHISupportsShaderPipelines(ShaderPlatform))
	{
		static const TConsoleVariableData<int32>* CVarShaderPipelines = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.ShaderPipelines"));
		bool bShaderPipelinesAreEnabled = CVarShaderPipelines && CVarShaderPipelines->GetValueOnAnyThread(IsInGameThread()) != 0;
		if (bShaderPipelinesAreEnabled)
		{
			static const IConsoleVariable* CVarExcludeNonPipelinedShaders = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Material.ExcludeNonPipelinedShaders"));
			bool bExcludeNonPipelinedShaders = CVarExcludeNonPipelinedShaders && CVarExcludeNonPipelinedShaders->GetInt() != 0;

			return bExcludeNonPipelinedShaders;
		}
	}

	return false;
}

RENDERCORE_API bool PlatformSupportsVelocityRendering(const FStaticShaderPlatform Platform)
{
	if (IsMobilePlatform(Platform))
	{
		// Enable velocity rendering if desktop Gen4 TAA is supported on mobile.
		return SupportsGen4TAA(Platform);
	}

	return true;
}

bool DoesPlatformSupportNanite(EShaderPlatform Platform, bool bCheckForProjectSetting)
{
	// Nanite allowed for this project
	if (bCheckForProjectSetting)
	{
		const bool bNaniteSupported = GNaniteProjectEnabled != 0;
		if (UNLIKELY(!bNaniteSupported))
		{
			return false;
		}
	}

	// Make sure the current platform has DDPI definitions.
	const bool bValidPlatform = FDataDrivenShaderPlatformInfo::IsValid(Platform);

	// GPUScene is required for Nanite
	const bool bSupportGPUScene = FDataDrivenShaderPlatformInfo::GetSupportsGPUScene(Platform);

	// Nanite specific check
	const bool bSupportNanite = FDataDrivenShaderPlatformInfo::GetSupportsNanite(Platform);

	const bool bFullCheck = bValidPlatform && bSupportGPUScene && bSupportNanite;
	return bFullCheck;
}

bool NaniteAtomicsSupported()
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

bool NaniteComputeMaterialsSupported()
{
	static const auto AllowComputeMaterials = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Nanite.AllowComputeMaterials"));
	static const bool bAllowComputeMaterials = (AllowComputeMaterials && AllowComputeMaterials->GetValueOnAnyThread() != 0);
	return bAllowComputeMaterials;
}

bool DoesRuntimeSupportNanite(EShaderPlatform ShaderPlatform, bool bCheckForAtomicSupport, bool bCheckForProjectSetting)
{
	// Does the platform support Nanite?
	const bool bSupportedPlatform = DoesPlatformSupportNanite(ShaderPlatform, bCheckForProjectSetting);

	// Nanite is not supported with forward shading at this time.
	const bool bForwardShadingEnabled = IsForwardShadingEnabled(ShaderPlatform);

	return bSupportedPlatform && (!bCheckForAtomicSupport || NaniteAtomicsSupported()) && !bForwardShadingEnabled;
}

bool UseNanite(EShaderPlatform ShaderPlatform, bool bCheckForAtomicSupport /*= true*/, bool bCheckForProjectSetting /*= true*/)
{
	static const auto EnableNaniteCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Nanite"));
	const bool bNaniteEnabled = (EnableNaniteCVar != nullptr) ? (EnableNaniteCVar->GetInt() != 0) : true;
	return bNaniteEnabled && DoesRuntimeSupportNanite(ShaderPlatform, bCheckForAtomicSupport, bCheckForProjectSetting);
}

bool UseVirtualShadowMaps(EShaderPlatform ShaderPlatform, const FStaticFeatureLevel FeatureLevel)
{
	static const auto EnableVirtualSMCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shadow.Virtual.Enable"));
	const bool bVirtualShadowMapsEnabled = EnableVirtualSMCVar ? (EnableVirtualSMCVar->GetInt() != 0) : false;
	return bVirtualShadowMapsEnabled && DoesRuntimeSupportNanite(ShaderPlatform, true /* check for atomics */, false /* check project setting */);
}

bool DoesPlatformSupportVirtualShadowMaps(EShaderPlatform Platform)
{
	return DoesPlatformSupportNanite(Platform, false /* check project setting */);
}

bool DoesPlatformSupportNonNaniteVirtualShadowMaps(EShaderPlatform ShaderPlatform)
{
	static const auto EnableCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shadow.Virtual.NonNaniteVSM"));
	return EnableCVar->GetInt() != 0 && DoesPlatformSupportNanite(ShaderPlatform, false /* check project setting */);
}

bool UseNonNaniteVirtualShadowMaps(EShaderPlatform ShaderPlatform, const FStaticFeatureLevel FeatureLevel)
{
	static const auto EnableCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shadow.Virtual.NonNaniteVSM"));
	return EnableCVar->GetInt() != 0 && UseVirtualShadowMaps(ShaderPlatform, FeatureLevel);
}

bool IsWaterVirtualShadowMapFilteringEnabled(const FStaticShaderPlatform Platform)
{
	static const auto CVarWaterSingleLayerShaderSupportVSMFiltering = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Water.SingleLayer.ShadersSupportVSMFiltering"));
	const bool bWaterVSMFilteringSupported = CVarWaterSingleLayerShaderSupportVSMFiltering && (CVarWaterSingleLayerShaderSupportVSMFiltering->GetInt() > 0);

	const bool bVirtualShadowMapsSupported = DoesPlatformSupportVirtualShadowMaps(Platform);

	return !IsForwardShadingEnabled(Platform) && bWaterVSMFilteringSupported && bVirtualShadowMapsSupported;
}

bool IsSingleLayerWaterDepthPrepassEnabled(const FStaticShaderPlatform Platform, const FStaticFeatureLevel FeatureLevel)
{
	static const auto CVarWaterSingleLayerDepthPrepass = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Water.SingleLayer.DepthPrepass"));
	const bool bPrepassEnabled = CVarWaterSingleLayerDepthPrepass && CVarWaterSingleLayerDepthPrepass->GetInt() > 0;
	// Currently VSM is the only feature dependent on the depth prepass which is why we only enable it if VSM could also be enabled.
	// VSM can be toggled at runtime, but we need a compile time value here, so we fall back to DoesPlatformSupportVirtualShadowMaps() to check if
	// VSM *could* be enabled.
	const bool bVSMSupported = DoesPlatformSupportVirtualShadowMaps(Platform);

	return bPrepassEnabled && bVSMSupported;

}

RENDERCORE_API bool IsUsingDBuffers(const FStaticShaderPlatform Platform)
{
	return (GDBufferPlatformMask[(int)Platform]);
}

RENDERCORE_API bool AreSkinCacheShadersEnabled(EShaderPlatform Platform)
{
	static FShaderPlatformCachedIniValue<bool> PerPlatformCVar(TEXT("r.SkinCache.CompileShaders"));
	return (PerPlatformCVar.Get(Platform) != 0);
}

RENDERCORE_API bool DoesRuntimeSupportOnePassPointLightShadows(EShaderPlatform Platform)
{
	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Shadow.DetectVertexShaderLayerAtRuntime"));

	return RHISupportsVertexShaderLayer(Platform)
		|| (CVar->GetValueOnAnyThread() != 0 && GRHISupportsArrayIndexFromAnyShader != 0);
}

bool IsForwardShadingEnabled(const FStaticShaderPlatform Platform)
{
	return (GForwardShadingPlatformMask[(int)Platform])
		// Culling uses compute shader
		&& GetMaxSupportedFeatureLevel(Platform) >= ERHIFeatureLevel::SM5;
}

bool IsUsingGBuffers(const FStaticShaderPlatform Platform)
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

bool IsUsingBasePassVelocity(const FStaticShaderPlatform Platform)
{
	static FShaderPlatformCachedIniValue<int32> PerPlatformCVar(TEXT("r.VelocityOutputPass"));
	// Writing velocity in base pass is disabled for desktop forward because it may use MSAA (runtime-settable setting) and Velocity isn't a multisample texture.
	// TSR or vr.AllowMotionBlurInVR=1 case aren't recommended for Desktop Forward renderer, and if enabled, cause a separate Velocity pass.
	if (IsMobilePlatform(Platform) || IsForwardShadingEnabled(Platform))
	{
		return false;
	}
	else
	{
		return (PerPlatformCVar.Get(Platform) == 1);
	}
}

bool IsUsingSelectiveBasePassOutputs(const FStaticShaderPlatform Platform)
{
	return (GSelectiveBasePassOutputsPlatformMask[(int)Platform]);
}

bool IsUsingDistanceFields(const FStaticShaderPlatform Platform)
{
	return (GDistanceFieldsPlatformMask[(int)Platform]);
}

bool IsWaterDistanceFieldShadowEnabled(const FStaticShaderPlatform Platform)
{
	// Only deferred support such a feature. It is not possible to do that for water without a water depth pre-pass.
	static const auto CVarWaterSingleLayerShaderSupportDistanceFieldShadow = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Water.SingleLayer.ShadersSupportDistanceFieldShadow"));
	const bool bWaterSingleLayerShaderSupportDistanceFieldShadow = CVarWaterSingleLayerShaderSupportDistanceFieldShadow && (CVarWaterSingleLayerShaderSupportDistanceFieldShadow->GetInt() > 0);
	return !IsForwardShadingEnabled(Platform) && IsUsingDistanceFields(Platform) && bWaterSingleLayerShaderSupportDistanceFieldShadow;
}

bool UseGPUScene(const FStaticShaderPlatform Platform, const FStaticFeatureLevel FeatureLevel)
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

bool UseGPUScene(const FStaticShaderPlatform Platform)
{
	return UseGPUScene(Platform, GetMaxSupportedFeatureLevel(Platform));
}

bool ForceSimpleSkyDiffuse(const FStaticShaderPlatform Platform)
{
	return (GSimpleSkyDiffusePlatformMask[(int)Platform]);
}

bool VelocityEncodeDepth(const FStaticShaderPlatform Platform)
{
	return (GVelocityEncodeDepthPlatformMask[(int)Platform]);
}

RENDERCORE_API bool AllowTranslucencyPerObjectShadows(const FStaticShaderPlatform Platform)
{
	return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && GAllowTranslucencyShadowsInProject != 0;
}

bool PlatformRequires128bitRT(EPixelFormat PixelFormat)
{
	switch (PixelFormat)
	{
	case PF_R32_FLOAT:
	case PF_G32R32F:
	case PF_A32B32G32R32F:
		return FDataDrivenShaderPlatformInfo::GetRequiresExplicit128bitRT(GMaxRHIShaderPlatform);
	default:
		return false;
	}
}

bool IsRayTracingEnabledForProject(EShaderPlatform ShaderPlatform)
{
	if (RHISupportsRayTracing(ShaderPlatform))
	{
		return (GRayTracingPlatformMask[(int)ShaderPlatform]);
	}
	else
	{
		return false;
	}
}

bool ShouldCompileRayTracingShadersForProject(EShaderPlatform ShaderPlatform)
{
	if (RHISupportsRayTracingShaders(ShaderPlatform))
	{
		return IsRayTracingEnabledForProject(ShaderPlatform);
	}
	else
	{
		return false;
	}
}

bool ShouldCompileRayTracingCallableShadersForProject(EShaderPlatform ShaderPlatform)
{
	return RHISupportsRayTracingCallableShaders(ShaderPlatform) && ShouldCompileRayTracingShadersForProject(ShaderPlatform);
}


bool IsRayTracingEnabled()
{
	bool bRayTracingEnabled = true;
	static const auto RayTracingEnableCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Raytracing.Enable"));
	if (GRayTracingMode == ERayTracingMode::Dynamic && RayTracingEnableCVar)
	{
		bRayTracingEnabled = RayTracingEnableCVar->GetValueOnAnyThread() != 0;
	}

	return IsRayTracingAllowed() && bRayTracingEnabled;
}

bool IsRayTracingAllowed()
{
	checkf(GIsRHIInitialized, TEXT("IsRayTracingAllowed() may only be called once RHI is initialized."));

#if DO_CHECK && WITH_EDITOR
	{
		// This function must not be called while cooking
		if (IsRunningCookCommandlet())
		{
			return false;
		}
	}
#endif // DO_CHECK && WITH_EDITOR

	return (int32)GRayTracingMode >= (int32)ERayTracingMode::Enabled;
}

bool IsRayTracingEnabled(EShaderPlatform ShaderPlatform)
{
	return IsRayTracingEnabled() && RHISupportsRayTracing(ShaderPlatform);
}

ERayTracingMode GetRayTracingMode()
{
	return IsRayTracingAllowed() ? GRayTracingMode : ERayTracingMode::Disabled;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Strata settings interface

static TAutoConsoleVariable<int32> CVarSubstrate(
	TEXT("r.Substrate"),
	0,
	TEXT("Enable Substrate materials (Beta)."),
	ECVF_ReadOnly | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarSubstrateBytePerPixel(
	TEXT("r.Substrate.BytesPerPixel"),
	80,
	TEXT("Substrate allocated byte per pixel to store materials data. Higher value means more complex material can be represented."),
	ECVF_ReadOnly | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarSubstrateBackCompatibility(
	TEXT("r.SubstrateBackCompatibility"),
	0,
	TEXT("Disables Substrate multiple scattering and replaces Chan diffuse by Lambert."),
	ECVF_ReadOnly | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarSubstrateOpaqueMaterialRoughRefraction(
	TEXT("r.Substrate.OpaqueMaterialRoughRefraction"),
	0,
	TEXT("Enable Substrate opaque material rough refractions effect from top layers over layers below."),
	ECVF_ReadOnly | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarSubstrateShadingQuality(
	TEXT("r.Substrate.ShadingQuality"),
	1,
	TEXT("Define Substrate shading quality (1: accurate lighting, 2: approximate lighting). This variable is read-only."),
	ECVF_ReadOnly | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarSubstrateDBufferPass(
	TEXT("r.Substrate.DBufferPass"),
	0,
	TEXT("Apply DBuffer after the base-pass as a separate pass. Read only because when this is changed, it will require the recompilation of all shaders."),
	ECVF_ReadOnly | ECVF_RenderThreadSafe);

// Transition render settings that will disappear when Substrate gets enabled
static TAutoConsoleVariable<int32> CVarMaterialRoughDiffuse(
	TEXT("r.Material.RoughDiffuse"),
	0,
	TEXT("Enable rough diffuse material."),
	ECVF_ReadOnly | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarSubstrateRoughDiffuse(
	TEXT("r.Substrate.RoughDiffuse"),
	1,
	TEXT("Enable Substrate rough diffuse model (works only if r.Material.RoughDiffuse is enabled in the project settings). Togglable at runtime"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarSubstrateDebugAdvancedVisualizationShaders(
	TEXT("r.Substrate.Debug.AdvancedVisualizationShaders"),
	0,
	TEXT("Enable advanced Substrate material debug visualization shaders. Base pass shaders can output such advanced data."),
	ECVF_ReadOnly | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarSubstrateTileCoord8Bits(
	TEXT("r.Substrate.TileCoord8bits"),
	0,
	TEXT("Format of tile coord. This variable is read-only."),
	ECVF_ReadOnly | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarSubstrateAccurateSRGB(
	TEXT("r.Substrate.AccurateSRGB"),
	0,
	TEXT("Enable accurate sRGB encoding for pixel data encoding/decoding during rendering passes (base pass, lighting, ...). Otherwise use a simple 'Gamma2' approximation."),
	ECVF_ReadOnly | ECVF_RenderThreadSafe);

namespace Strata
{
	bool IsStrataEnabled()
	{
		return CVarSubstrate.GetValueOnAnyThread() > 0;
	}

	static uint32 InternalGetBytePerPixel(uint32 InBytePerPixel)
	{	
		// We enforce at least 20 bytes per pixel because this is the minimal Strata GBuffer footprint of the simplest material.
		const uint32 MinStrataBytePerPixel = 20u;
		const uint32 MaxStrataBytePerPixel = /*IsMobilePlatform(GMaxRHI InPlatform) ? 24u : */256u; // STRATA_TODO
		return FMath::Clamp(InBytePerPixel, MinStrataBytePerPixel, MaxStrataBytePerPixel);
	}

	uint32 GetBytePerPixel()
	{
		return InternalGetBytePerPixel(CVarSubstrateBytePerPixel.GetValueOnAnyThread());
	}

	uint32 GetBytePerPixel(EShaderPlatform InPlatform)
	{
		// Variant for shader compilation per platform
		static FShaderPlatformCachedIniValue<int32> CVarBudget(TEXT("r.Substrate.BytesPerPixel"));
		return InternalGetBytePerPixel(CVarBudget.Get(InPlatform));
	}

	static uint32 InternalGetRayTracingMaterialPayloadSizeInBytes(uint32 InBytePerPixels, uint32 InNormalQuality)
	{
		// If Strata is enabled, resize the payload accordingly to the byte per pixel request
		const uint32 HeaderPayload = sizeof(uint32) * (6u); // See FPackedMaterialClosestHitPayload in RayTracingCommon.ush
		const uint32 TopNormalPayload = sizeof(uint32) * (InNormalQuality == 1 ? 2u : 1u);

		return HeaderPayload + TopNormalPayload + InBytePerPixels;
	}

	uint32 GetNormalQuality()
	{
		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.GBufferFormat"));
		return CVar && CVar->GetValueOnAnyThread() > 1 ? 1 : 0;
	}

	uint32 GetRayTracingMaterialPayloadSizeInBytes()
	{
		return InternalGetRayTracingMaterialPayloadSizeInBytes(GetBytePerPixel(), GetNormalQuality());
	}

	uint32 GetRayTracingMaterialPayloadSizeInBytes(EShaderPlatform InPlatform)
	{
		return InternalGetRayTracingMaterialPayloadSizeInBytes(GetBytePerPixel(InPlatform), GetNormalQuality());
	}

	bool IsBackCompatibilityEnabled()
	{
		return CVarSubstrateBackCompatibility.GetValueOnAnyThread() > 0 ? 1 : 0;
	}
	
	bool IsRoughDiffuseEnabled()
	{
		return CVarSubstrateRoughDiffuse.GetValueOnAnyThread() > 0;
	}

	bool Is8bitTileCoordEnabled()
	{
		return CVarSubstrateTileCoord8Bits.GetValueOnAnyThread() > 0 ? 1 : 0;
	}

	bool IsAccurateSRGBEnabled()
	{
		return CVarSubstrateAccurateSRGB.GetValueOnAnyThread() > 0 ? 1 : 0;
	}

	uint32 GetShadingQuality()
	{
		return CVarSubstrateShadingQuality.GetValueOnAnyThread();
	}

	uint32 GetShadingQuality(EShaderPlatform InPlatform)
	{
		static FShaderPlatformCachedIniValue<int32> CVarSubstrateShadingQualityPlatform(TEXT("r.Substrate.ShadingQuality"));
		return CVarSubstrateShadingQualityPlatform.Get(InPlatform);
	}
	
	bool IsDBufferPassEnabled(EShaderPlatform InPlatform)
	{
		// DBuffer pass is only available if high quality normal is disabled, or if we are on a console.
		// That is because in high quality normals requires a uint2 UAV which is only supported on some graphic cards on PC 
		// and this is an unknown when compiling shaders at this stage.
		// !!! If this is changed, please update all sites reading r.Substrate.DBufferPass !!!
		const uint32 NormalQuality = GetNormalQuality();
		const bool bDBufferPassSupported = NormalQuality == 0 || (NormalQuality > 0 && IsConsolePlatform(InPlatform));
		
		static FShaderPlatformCachedIniValue<int32> CVarSubstrateDBufferPassPlatform(TEXT("r.Substrate.DBufferPass"));
		return IsStrataEnabled() && IsUsingDBuffers(InPlatform) && bDBufferPassSupported && CVarSubstrateDBufferPassPlatform.Get(InPlatform) > 0;
	}

	bool IsOpaqueRoughRefractionEnabled()
	{
		return IsStrataEnabled() && CVarSubstrateOpaqueMaterialRoughRefraction.GetValueOnAnyThread() > 0;
	}

	bool IsAdvancedVisualizationEnabled()
	{
		return CVarSubstrateDebugAdvancedVisualizationShaders.GetValueOnAnyThread() > 0;
	}
}