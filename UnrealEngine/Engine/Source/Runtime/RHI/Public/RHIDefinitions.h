// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RHIDefinitions.h: Render Hardware Interface definitions
		(that don't require linking).
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "PixelFormat.h"
#include "HAL/IConsoleManager.h"
#include "Serialization/MemoryLayout.h"

#ifndef USE_STATIC_SHADER_PLATFORM_ENUMS
#define USE_STATIC_SHADER_PLATFORM_ENUMS 0
#endif

#ifndef USE_STATIC_SHADER_PLATFORM_INFO
#define USE_STATIC_SHADER_PLATFORM_INFO 0
#endif

#ifndef RHI_RAYTRACING
#define RHI_RAYTRACING 0
#endif

enum class ERHIInterfaceType
{
	Hidden,
	Null,
	D3D11,
	D3D12,
	Vulkan,
	Metal,
	Agx,
	OpenGL,
};

enum class ERHIFeatureSupport : uint8
{
	// The RHI feature is completely unavailable at runtime
	Unsupported,

	// The RHI feature can be available at runtime based on hardware or driver
	RuntimeDependent,

	// The RHI feature is guaranteed to be available at runtime.
	RuntimeGuaranteed,

	Num,
	NumBits = 2,
};

enum EShaderFrequency : uint8
{
	SF_Vertex			= 0,
	SF_Mesh				= 1,
	SF_Amplification	= 2,
	SF_Pixel			= 3,
	SF_Geometry			= 4,
	SF_Compute			= 5,
	SF_RayGen			= 6,
	SF_RayMiss			= 7,
	SF_RayHitGroup		= 8,
	SF_RayCallable		= 9,

	SF_NumFrequencies	= 10,

	// Number of standard shader frequencies for graphics pipeline (excluding compute)
	SF_NumGraphicsFrequencies = 5,

	// Number of standard shader frequencies (including compute)
	SF_NumStandardFrequencies = 6,

	SF_NumBits			= 4,
};
static_assert(SF_NumFrequencies <= (1 << SF_NumBits), "SF_NumFrequencies will not fit on SF_NumBits");

/** @warning: update *LegacyShaderPlatform* when the below changes */
enum EShaderPlatform
{
	SP_PCD3D_SM5					= 0,
	SP_METAL						= 11,
	SP_METAL_MRT					= 12,
	SP_PCD3D_ES3_1					= 14,
	SP_OPENGL_PCES3_1				= 15,
	SP_METAL_SM5					= 16,
	SP_VULKAN_PCES3_1				= 17,
	SP_METAL_SM5_NOTESS_REMOVED		UE_DEPRECATED(5.0, "ShaderPlatform is removed; please don't use.") = 18,
	SP_VULKAN_SM5					= 20,
	SP_VULKAN_ES3_1_ANDROID			= 21,
	SP_METAL_MACES3_1 				= 22,
	SP_OPENGL_ES3_1_ANDROID			= 24,
	SP_METAL_MRT_MAC				= 27,
	SP_VULKAN_SM5_LUMIN_REMOVED		UE_DEPRECATED(5.0, "ShaderPlatform is removed; please don't use.") = 28,
	SP_VULKAN_ES3_1_LUMIN_REMOVED	UE_DEPRECATED(5.0, "ShaderPlatform is removed; please don't use.") = 29,
	SP_METAL_TVOS					= 30,
	SP_METAL_MRT_TVOS				= 31,
	/**********************************************************************************/
	/* !! Do not add any new platforms here. Add them below SP_StaticPlatform_Last !! */
	/**********************************************************************************/

	//---------------------------------------------------------------------------------
	/** Pre-allocated block of shader platform enum values for platform extensions */
#define DDPI_NUM_STATIC_SHADER_PLATFORMS 16
	SP_StaticPlatform_First = 32,

	// Pull in the extra shader platform definitions from platform extensions.
	// @todo - when we remove EShaderPlatform, fix up the shader platforms defined in UEBuild[Platform].cs files.
#ifdef DDPI_EXTRA_SHADERPLATFORMS
	DDPI_EXTRA_SHADERPLATFORMS
#endif

	SP_StaticPlatform_Last  = (SP_StaticPlatform_First + DDPI_NUM_STATIC_SHADER_PLATFORMS - 1),

	//  Add new platforms below this line, starting from (SP_StaticPlatform_Last + 1)
	//---------------------------------------------------------------------------------
	SP_VULKAN_SM5_ANDROID			= SP_StaticPlatform_Last+1,
	SP_PCD3D_SM6,
	SP_D3D_ES3_1_HOLOLENS,

	SP_CUSTOM_PLATFORM_FIRST,
	SP_CUSTOM_PLATFORM_LAST = (SP_CUSTOM_PLATFORM_FIRST + 100),

	SP_NumPlatforms,
	SP_NumBits						= 16,
};
static_assert(SP_NumPlatforms <= (1 << SP_NumBits), "SP_NumPlatforms will not fit on SP_NumBits");

struct FGenericStaticShaderPlatform final
{
	inline FGenericStaticShaderPlatform(const EShaderPlatform InPlatform) : Platform(InPlatform) {}
	inline operator EShaderPlatform() const
	{
		return Platform;
	}

	inline bool operator == (const EShaderPlatform Other) const
	{
		return Other == Platform;
	}
	inline bool operator != (const EShaderPlatform Other) const
	{
		return Other != Platform;
	}
private:
	const EShaderPlatform Platform;
};

#if USE_STATIC_SHADER_PLATFORM_ENUMS
#include COMPILED_PLATFORM_HEADER(StaticShaderPlatform.inl)
#else
using FStaticShaderPlatform = FGenericStaticShaderPlatform;
#endif

class FStaticShaderPlatformNames
{
private:
	static const uint32 NumPlatforms = DDPI_NUM_STATIC_SHADER_PLATFORMS;

	struct FPlatform
	{
		FName Name;
		FName ShaderPlatform;
		FName ShaderFormat;
	} Platforms[NumPlatforms];

	FStaticShaderPlatformNames()
	{
#ifdef DDPI_SHADER_PLATFORM_NAME_MAP
		struct FStaticNameMapEntry
		{
			FName Name;
			FName PlatformName;
			int32 Index;
		} NameMap[] =
		{
			DDPI_SHADER_PLATFORM_NAME_MAP
		};

		for (int32 MapIndex = 0; MapIndex < UE_ARRAY_COUNT(NameMap); ++MapIndex)
		{
			FStaticNameMapEntry const& Entry = NameMap[MapIndex];
			check(IsStaticPlatform(EShaderPlatform(Entry.Index)));
			uint32 PlatformIndex = Entry.Index - SP_StaticPlatform_First;

			FPlatform& Platform = Platforms[PlatformIndex];
			check(Platform.Name == NAME_None); // Check we've not already seen this platform

			Platform.Name = Entry.PlatformName;
			Platform.ShaderPlatform = FName(*FString::Printf(TEXT("SP_%s"), *Entry.Name.ToString()), FNAME_Add);
			Platform.ShaderFormat = FName(*FString::Printf(TEXT("SF_%s"), *Entry.Name.ToString()), FNAME_Add);
		}
#endif
	}

public:
	static inline FStaticShaderPlatformNames const& Get()
	{
		static FStaticShaderPlatformNames Names;
		return Names;
	}

	static inline bool IsStaticPlatform(EShaderPlatform Platform)
	{
		return Platform >= SP_StaticPlatform_First && Platform <= SP_StaticPlatform_Last;
	}

	inline const FName& GetShaderPlatform(EShaderPlatform Platform) const
	{
		return Platforms[GetStaticPlatformIndex(Platform)].ShaderPlatform;
	}

	inline const FName& GetShaderFormat(EShaderPlatform Platform) const
	{
		return Platforms[GetStaticPlatformIndex(Platform)].ShaderFormat;
	}

	inline const FName& GetPlatformName(EShaderPlatform Platform) const
	{
		return Platforms[GetStaticPlatformIndex(Platform)].Name;
	}

private:
	static inline uint32 GetStaticPlatformIndex(EShaderPlatform Platform)
	{
		check(IsStaticPlatform(Platform));
		return uint32(Platform) - SP_StaticPlatform_First;
	}
};

/**
 * The RHI's feature level indicates what level of support can be relied upon.
 * Note: these are named after graphics API's like ES3 but a feature level can be used with a different API (eg ERHIFeatureLevel::ES3.1 on D3D11)
 * As long as the graphics API supports all the features of the feature level (eg no ERHIFeatureLevel::SM5 on OpenGL ES3.1)
 */
namespace ERHIFeatureLevel
{
	enum Type
	{
		/** Feature level defined by the core capabilities of OpenGL ES2. Deprecated */
		ES2_REMOVED,

		/** Feature level defined by the core capabilities of OpenGL ES3.1 & Metal/Vulkan. */
		ES3_1,

		/**
		 * Feature level defined by the capabilities of DX10 Shader Model 4.
		 * SUPPORT FOR THIS FEATURE LEVEL HAS BEEN ENTIRELY REMOVED.
		 */
		SM4_REMOVED,

		/**
		 * Feature level defined by the capabilities of DX11 Shader Model 5.
		 *   Compute shaders with shared memory, group sync, UAV writes, integer atomics
		 *   Indirect drawing
		 *   Pixel shaders with UAV writes
		 *   Cubemap arrays
		 *   Read-only depth or stencil views (eg read depth buffer as SRV while depth test and stencil write)
		 * Tessellation is not considered part of Feature Level SM5 and has a separate capability flag.
		 */
		SM5,

		/**
		 * Feature level defined by the capabilities of DirectX 12 hardware feature level 12_2 with Shader Model 6.5
		 *   Raytracing Tier 1.1
		 *   Mesh and Amplification shaders
		 *   Variable rate shading
		 *   Sampler feedback
		 *   Resource binding tier 3
		 */
		SM6,

		Num
	};
};
DECLARE_INTRINSIC_TYPE_LAYOUT(ERHIFeatureLevel::Type);

struct FGenericStaticFeatureLevel
{
	inline FGenericStaticFeatureLevel(const ERHIFeatureLevel::Type InFeatureLevel) : FeatureLevel(InFeatureLevel) {}
	inline FGenericStaticFeatureLevel(const TEnumAsByte<ERHIFeatureLevel::Type> InFeatureLevel) : FeatureLevel(InFeatureLevel) {}

	inline operator ERHIFeatureLevel::Type() const
	{
		return FeatureLevel;
	}

	inline bool operator == (const ERHIFeatureLevel::Type Other) const
	{
		return Other == FeatureLevel;
	}

	inline bool operator != (const ERHIFeatureLevel::Type Other) const
	{
		return Other != FeatureLevel;
	}

	inline bool operator <= (const ERHIFeatureLevel::Type Other) const
	{
		return FeatureLevel <= Other;
	}

	inline bool operator < (const ERHIFeatureLevel::Type Other) const
	{
		return FeatureLevel < Other;
	}

	inline bool operator >= (const ERHIFeatureLevel::Type Other) const
	{
		return FeatureLevel >= Other;
	}

	inline bool operator > (const ERHIFeatureLevel::Type Other) const
	{
		return FeatureLevel > Other;
	}

private:
	ERHIFeatureLevel::Type FeatureLevel;
};

#if USE_STATIC_SHADER_PLATFORM_ENUMS
#include COMPILED_PLATFORM_HEADER(StaticFeatureLevel.inl)
#else
using FStaticFeatureLevel = FGenericStaticFeatureLevel;
#endif

extern RHI_API const FName LANGUAGE_D3D;
extern RHI_API const FName LANGUAGE_Metal;
extern RHI_API const FName LANGUAGE_OpenGL;
extern RHI_API const FName LANGUAGE_Vulkan;
extern RHI_API const FName LANGUAGE_Sony;
extern RHI_API const FName LANGUAGE_Nintendo;

class RHI_API FGenericDataDrivenShaderPlatformInfo
{
	FName Name;
	FName Language;
	ERHIFeatureLevel::Type MaxFeatureLevel;
	FName ShaderFormat;
	EShaderPlatform PreviewShaderPlatformParent;
	uint32 ShaderPropertiesHash;
	uint32 bIsMobile: 1;
	uint32 bIsMetalMRT: 1;
	uint32 bIsPC: 1;
	uint32 bIsConsole: 1;
	uint32 bIsAndroidOpenGLES: 1;

	uint32 bSupportsDebugViewShaders : 1;
	uint32 bSupportsMobileMultiView: 1;
	uint32 bSupportsArrayTextureCompression : 1;
	uint32 bSupportsDistanceFields: 1; // used for DFShadows and DFAO - since they had the same checks
	uint32 bSupportsDiaphragmDOF: 1;
	uint32 bSupportsRGBColorBuffer: 1;
	uint32 bSupportsCapsuleShadows: 1;
	uint32 bSupportsPercentageCloserShadows : 1;
	uint32 bSupportsVolumetricFog: 1; // also used for FVVoxelization
	uint32 bSupportsIndexBufferUAVs: 1;
	uint32 bSupportsInstancedStereo: 1;
	uint32 SupportsMultiViewport: int32(ERHIFeatureSupport::NumBits);
	uint32 bSupportsMSAA: 1;
	uint32 bSupports4ComponentUAVReadWrite: 1;
	uint32 bSupportsRenderTargetWriteMask: 1;
	uint32 bSupportsRayTracing: 1;
	uint32 bSupportsRayTracingCallableShaders : 1;
	uint32 bSupportsRayTracingProceduralPrimitive : 1;
	uint32 bSupportsRayTracingTraversalStatistics : 1;
	uint32 bSupportsRayTracingIndirectInstanceData : 1; // Whether instance transforms can be copied from the GPU to the TLAS instances buffer
	uint32 bSupportsHighEndRayTracingReflections : 1; // Whether fully-featured RT reflections can be used on the platform (with multi-bounce, translucency, etc.)
	uint32 bSupportsPathTracing : 1; // Whether real-time path tracer is supported on this platform (avoids compiling unnecessary shaders)
	uint32 bSupportsGPUScene : 1;
	uint32 bSupportsByteBufferComputeShaders : 1;
	uint32 bSupportsPrimitiveShaders : 1;
	uint32 bSupportsUInt64ImageAtomics : 1;
	uint32 bRequiresVendorExtensionsForAtomics : 1;
	uint32 bSupportsNanite : 1;
	uint32 bSupportsLumenGI : 1;
	uint32 bSupportsSSDIndirect : 1;
	uint32 bSupportsTemporalHistoryUpscale : 1;
	uint32 bSupportsRTIndexFromVS : 1;
	uint32 bSupportsWaveOperations : int32(ERHIFeatureSupport::NumBits);
	uint32 MinimumWaveSize : 8;
	uint32 MaximumWaveSize : 8;
	uint32 bSupportsIntrinsicWaveOnce : 1;
	uint32 bSupportsConservativeRasterization : 1;
	uint32 bRequiresExplicit128bitRT : 1;
	uint32 bSupportsGen5TemporalAA : 1;
	uint32 bTargetsTiledGPU: 1;
	uint32 bNeedsOfflineCompiler: 1;
	uint32 bSupportsComputeFramework : 1;
	uint32 bSupportsAnisotropicMaterials : 1;
	uint32 bSupportsDualSourceBlending : 1;
	uint32 bRequiresGeneratePrevTransformBuffer : 1;
	uint32 bRequiresRenderTargetDuringRaster : 1;
	uint32 bRequiresDisableForwardLocalLights : 1;
	uint32 bCompileSignalProcessingPipeline : 1;
	uint32 bSupportsMeshShadersTier0 : 1;
	uint32 bSupportsMeshShadersTier1 : 1;
	uint32 MaxMeshShaderThreadGroupSize : 10;
	uint32 bSupportsPerPixelDBufferMask : 1;
	uint32 bIsHlslcc : 1;
	uint32 bSupportsDxc : 1; // Whether DirectXShaderCompiler (DXC) is supported
	uint32 bIsSPIRV : 1;
	uint32 bSupportsVariableRateShading : 1;
	uint32 NumberOfComputeThreads : 10;
	uint32 bWaterUsesSimpleForwardShading : 1;
	uint32 bSupportsHairStrandGeometry : 1;
	uint32 bSupportsDOFHybridScattering : 1;
	uint32 bNeedsExtraMobileFrames : 1;
	uint32 bSupportsHZBOcclusion : 1;
	uint32 bSupportsWaterIndirectDraw : 1;
	uint32 bSupportsAsyncPipelineCompilation : 1;
	uint32 bSupportsManualVertexFetch : 1;
	uint32 bRequiresReverseCullingOnMobile : 1;
	uint32 bOverrideFMaterial_NeedsGBufferEnabled : 1;
	uint32 bSupportsMobileDistanceField : 1;
	uint32 bSupportsFFTBloom : 1;
	uint32 bSupportsInlineRayTracing : 1;
	uint32 bSupportsRayTracingShaders : 1;
	uint32 bSupportsVertexShaderLayer : 1;
	uint32 bSupportsBindless : 1;
	uint32 bSupportsVolumeTextureAtomics : 1;
	uint32 bSupportsROV : 1;
	uint32 bSupportsOIT : 1;
	uint32 bSupportsRealTypes : int32(ERHIFeatureSupport::NumBits);
	uint32 EnablesHLSL2021ByDefault : 2; // 0: disabled, 1: global shaders only, 2: all shaders
	uint32 bSupportsSceneDataCompressedTransforms : 1;
	uint32 bIsPreviewPlatform : 1;
	uint32 bSupportsSwapchainUAVs : 1;
		
#if WITH_EDITOR
	FText FriendlyName;
#endif

	// NOTE: When adding fields, you must also add to ParseDataDrivenShaderInfo!
	uint32 bContainsValidPlatformInfo : 1;

	FGenericDataDrivenShaderPlatformInfo()
	{
		FMemory::Memzero(this, sizeof(*this));

		SetDefaultValues();
	}

	void SetDefaultValues();

public:
	static void Initialize();
	static void UpdatePreviewPlatforms();
	static void ParseDataDrivenShaderInfo(const FConfigSection& Section, FGenericDataDrivenShaderPlatformInfo& Info);
	static const EShaderPlatform GetShaderPlatformFromName(const FName ShaderPlatformName);

	static FORCEINLINE_DEBUGGABLE const FName GetName(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].Name;
	}

	static FORCEINLINE_DEBUGGABLE const FName GetShaderFormat(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].ShaderFormat;
	}

	static FORCEINLINE_DEBUGGABLE const EShaderPlatform GetPreviewShaderPlatformParent(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].PreviewShaderPlatformParent;
	}

	static FORCEINLINE_DEBUGGABLE uint32 GetShaderPlatformPropertiesHash(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].ShaderPropertiesHash;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetIsLanguageD3D(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].Language == LANGUAGE_D3D;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetIsLanguageMetal(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].Language == LANGUAGE_Metal;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetIsLanguageOpenGL(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].Language == LANGUAGE_OpenGL;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetIsLanguageVulkan(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].Language == LANGUAGE_Vulkan;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetIsLanguageSony(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].Language == LANGUAGE_Sony;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetIsLanguageNintendo(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].Language == LANGUAGE_Nintendo;
	}

	static FORCEINLINE_DEBUGGABLE const ERHIFeatureLevel::Type GetMaxFeatureLevel(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].MaxFeatureLevel;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetIsMobile(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bIsMobile;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetIsMetalMRT(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bIsMetalMRT;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetIsPC(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bIsPC;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetIsConsole(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bIsConsole;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetIsAndroidOpenGLES(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bIsAndroidOpenGLES;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsDebugViewShaders(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsDebugViewShaders;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsMobileMultiView(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsMobileMultiView;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsArrayTextureCompression(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsArrayTextureCompression;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsDistanceFields(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsDistanceFields;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsDiaphragmDOF(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsDiaphragmDOF;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsRGBColorBuffer(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsRGBColorBuffer;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsCapsuleShadows(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsCapsuleShadows;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsPercentageCloserShadows(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsPercentageCloserShadows;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsVolumetricFog(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsVolumetricFog;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsIndexBufferUAVs(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsIndexBufferUAVs;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsInstancedStereo(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsInstancedStereo;
	}

	UE_DEPRECATED(5.1, "bSupportsMultiView shader platform property has been deprecated. Use SupportsMultiViewport")
	static FORCEINLINE_DEBUGGABLE const bool GetSupportsMultiView(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return ERHIFeatureSupport(Infos[Platform].SupportsMultiViewport) != ERHIFeatureSupport::Unsupported;
	}

	static FORCEINLINE_DEBUGGABLE const ERHIFeatureSupport GetSupportsMultiViewport(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return ERHIFeatureSupport(Infos[Platform].SupportsMultiViewport);
	}	

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsMSAA(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsMSAA;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupports4ComponentUAVReadWrite(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupports4ComponentUAVReadWrite;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsSwapchainUAVs(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsSwapchainUAVs;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsRenderTargetWriteMask(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsRenderTargetWriteMask;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportSceneDataCompressedTransforms(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsSceneDataCompressedTransforms;
	}
	
	static FORCEINLINE_DEBUGGABLE const bool GetSupportsRayTracing(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsRayTracing;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsRayTracingShaders(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsRayTracing && Infos[Platform].bSupportsRayTracingShaders;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsInlineRayTracing(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsRayTracing && Infos[Platform].bSupportsInlineRayTracing;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsRayTracingCallableShaders(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsRayTracing && Infos[Platform].bSupportsRayTracingCallableShaders;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsRayTracingProceduralPrimitive(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsRayTracing && Infos[Platform].bSupportsRayTracingProceduralPrimitive;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsRayTracingTraversalStatistics(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsRayTracing && Infos[Platform].bSupportsRayTracingTraversalStatistics;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsRayTracingIndirectInstanceData(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsRayTracing && Infos[Platform].bSupportsRayTracingIndirectInstanceData;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsPathTracing(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsRayTracing && Infos[Platform].bSupportsPathTracing;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsHighEndRayTracingReflections(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsRayTracing && Infos[Platform].bSupportsHighEndRayTracingReflections;
	}

	UE_DEPRECATED(5.1, "This function is no longer in use and will be removed.")
	static FORCEINLINE_DEBUGGABLE const bool GetSupportsGPUSkinCache(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return true;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsComputeFramework(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsComputeFramework;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsAnisotropicMaterials(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsAnisotropicMaterials;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetTargetsTiledGPU(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bTargetsTiledGPU;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetNeedsOfflineCompiler(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bNeedsOfflineCompiler;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsByteBufferComputeShaders(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsByteBufferComputeShaders;
	}

	static FORCEINLINE_DEBUGGABLE const ERHIFeatureSupport GetSupportsWaveOperations(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return ERHIFeatureSupport(Infos[Platform].bSupportsWaveOperations);
	}

	static FORCEINLINE_DEBUGGABLE const uint32 GetMinimumWaveSize(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].MinimumWaveSize;
	}

	static FORCEINLINE_DEBUGGABLE const uint32 GetMaximumWaveSize(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].MaximumWaveSize;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsTemporalHistoryUpscale(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsTemporalHistoryUpscale;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsGPUScene(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsGPUScene;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetRequiresExplicit128bitRT(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bRequiresExplicit128bitRT;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsPrimitiveShaders(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsPrimitiveShaders;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsUInt64ImageAtomics(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsUInt64ImageAtomics;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetRequiresVendorExtensionsForAtomics(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bRequiresVendorExtensionsForAtomics;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsNanite(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsNanite;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsLumenGI(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsLumenGI;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsSSDIndirect(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsSSDIndirect;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsRTIndexFromVS(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsRTIndexFromVS;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsIntrinsicWaveOnce(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsIntrinsicWaveOnce;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsConservativeRasterization(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsConservativeRasterization;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsGen5TemporalAA(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsGen5TemporalAA;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsDualSourceBlending(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsDualSourceBlending;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetRequiresGeneratePrevTransformBuffer(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bRequiresGeneratePrevTransformBuffer;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetRequiresRenderTargetDuringRaster(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bRequiresRenderTargetDuringRaster;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetRequiresDisableForwardLocalLights(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bRequiresDisableForwardLocalLights;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetCompileSignalProcessingPipeline(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bCompileSignalProcessingPipeline;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsMeshShadersTier0(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsMeshShadersTier0;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsMeshShadersTier1(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsMeshShadersTier1;
	}

	static FORCEINLINE_DEBUGGABLE const uint32 GetMaxMeshShaderThreadGroupSize(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].MaxMeshShaderThreadGroupSize;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsPerPixelDBufferMask(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsPerPixelDBufferMask;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetIsHlslcc(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bIsHlslcc;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsDxc(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsDxc;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetIsSPIRV(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bIsSPIRV;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsVariableRateShading(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsVariableRateShading;
	}

	static FORCEINLINE_DEBUGGABLE const uint32 GetNumberOfComputeThreads(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].NumberOfComputeThreads;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetWaterUsesSimpleForwardShading(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bWaterUsesSimpleForwardShading;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsHairStrandGeometry(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsHairStrandGeometry;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsDOFHybridScattering(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsDOFHybridScattering;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetNeedsExtraMobileFrames(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bNeedsExtraMobileFrames;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsHZBOcclusion(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsHZBOcclusion;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsWaterIndirectDraw(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsWaterIndirectDraw;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsAsyncPipelineCompilation(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsAsyncPipelineCompilation;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsManualVertexFetch(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsManualVertexFetch;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetRequiresReverseCullingOnMobile(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bRequiresReverseCullingOnMobile;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetOverrideFMaterial_NeedsGBufferEnabled(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bOverrideFMaterial_NeedsGBufferEnabled;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsMobileDistanceField(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsMobileDistanceField;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsFFTBloom(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsFFTBloom;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsVertexShaderLayer(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsVertexShaderLayer;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsBindless(const FStaticShaderPlatform Platform)
	{
		return Infos[Platform].bSupportsBindless;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsVolumeTextureAtomics(const FStaticShaderPlatform Platform)
	{
		return Infos[Platform].bSupportsVolumeTextureAtomics;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsROV(const FStaticShaderPlatform Platform)
	{
		return Infos[Platform].bSupportsROV;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsOIT(const FStaticShaderPlatform Platform)
	{
		return Infos[Platform].bSupportsOIT;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetIsPreviewPlatform(const FStaticShaderPlatform Platform)
	{
		return Infos[Platform].bIsPreviewPlatform;
	}

	static FORCEINLINE_DEBUGGABLE const ERHIFeatureSupport GetSupportsRealTypes(const FStaticShaderPlatform Platform)
	{
		return ERHIFeatureSupport(Infos[Platform].bSupportsRealTypes);
	}

	static FORCEINLINE_DEBUGGABLE const uint32 GetEnablesHLSL2021ByDefault(const FStaticShaderPlatform Platform)
	{
		return Infos[Platform].EnablesHLSL2021ByDefault;
	}

#if WITH_EDITOR
	static FText GetFriendlyName(const FStaticShaderPlatform Platform);
#endif

private:
	static FGenericDataDrivenShaderPlatformInfo Infos[SP_NumPlatforms];

public:
	static bool IsValid(const FStaticShaderPlatform Platform)
	{
		return Infos[Platform].bContainsValidPlatformInfo;
	}
};

#if USE_STATIC_SHADER_PLATFORM_ENUMS || USE_STATIC_SHADER_PLATFORM_INFO

#define IMPLEMENT_DDPSPI_SETTING_WITH_RETURN_TYPE(ReturnType, Function, Value) \
	static FORCEINLINE_DEBUGGABLE const ReturnType Function(const FStaticShaderPlatform Platform) \
	{ \
		checkSlow(!FGenericDataDrivenShaderPlatformInfo::IsValid(Platform) || FGenericDataDrivenShaderPlatformInfo::Function(Platform) == Value); \
		return Value; \
	}
#define IMPLEMENT_DDPSPI_SETTING(Function, Value) IMPLEMENT_DDPSPI_SETTING_WITH_RETURN_TYPE(bool, Function, Value)

#include COMPILED_PLATFORM_HEADER(DataDrivenShaderPlatformInfo.inl)

#else
using FDataDrivenShaderPlatformInfo = FGenericDataDrivenShaderPlatformInfo;
#endif

enum ERenderQueryType
{
	// e.g. WaitForFrameEventCompletion()
	RQT_Undefined,
	// Result is the number of samples that are not culled (divide by MSAACount to get pixels)
	RQT_Occlusion,
	// Result is current time in micro seconds = 1/1000 ms = 1/1000000 sec (not a duration).
	RQT_AbsoluteTime,
};

/** Maximum number of miplevels in a texture. */
enum { MAX_TEXTURE_MIP_COUNT = 15 };

/** Maximum number of static/skeletal mesh LODs */
enum { MAX_MESH_LOD_COUNT = 8 };

/** Maximum number of immutable samplers in a PSO. */
enum
{
	MaxImmutableSamplers = 2
};

/** The maximum number of vertex elements which can be used by a vertex declaration. */
enum
{
	MaxVertexElementCount = 17,
	MaxVertexElementCount_NumBits = 5,
};
static_assert(MaxVertexElementCount <= (1 << MaxVertexElementCount_NumBits), "MaxVertexElementCount will not fit on MaxVertexElementCount_NumBits");

/** The alignment in bytes between elements of array shader parameters. */
enum { ShaderArrayElementAlignBytes = 16 };

/** The number of render-targets that may be simultaneously written to. */
enum
{
	MaxSimultaneousRenderTargets = 8,
	MaxSimultaneousRenderTargets_NumBits = 3,
};
static_assert(MaxSimultaneousRenderTargets <= (1 << MaxSimultaneousRenderTargets_NumBits), "MaxSimultaneousRenderTargets will not fit on MaxSimultaneousRenderTargets_NumBits");

/** The number of UAVs that may be simultaneously bound to a shader. */
enum { MaxSimultaneousUAVs = 8 };

enum class ERHIZBuffer
{
	// Before changing this, make sure all math & shader assumptions are correct! Also wrap your C++ assumptions with
	//		static_assert(ERHIZBuffer::IsInvertedZBuffer(), ...);
	// Shader-wise, make sure to update Definitions.usf, HAS_INVERTED_Z_BUFFER
	FarPlane = 0,
	NearPlane = 1,

	// 'bool' for knowing if the API is using Inverted Z buffer
	IsInverted = (int32)((int32)ERHIZBuffer::FarPlane < (int32)ERHIZBuffer::NearPlane),
};


/**
* The RHI's currently enabled shading path.
*/
namespace ERHIShadingPath
{
	enum Type
	{
		Deferred,
		Forward,
		Mobile,
		Num
	};
}

enum ESamplerFilter
{
	SF_Point,
	SF_Bilinear,
	SF_Trilinear,
	SF_AnisotropicPoint,
	SF_AnisotropicLinear,

	ESamplerFilter_Num,
	ESamplerFilter_NumBits = 3,
};
static_assert(ESamplerFilter_Num <= (1 << ESamplerFilter_NumBits), "ESamplerFilter_Num will not fit on ESamplerFilter_NumBits");

enum ESamplerAddressMode
{
	AM_Wrap,
	AM_Clamp,
	AM_Mirror,
	/** Not supported on all platforms */
	AM_Border,

	ESamplerAddressMode_Num,
	ESamplerAddressMode_NumBits = 2,
};
static_assert(ESamplerAddressMode_Num <= (1 << ESamplerAddressMode_NumBits), "ESamplerAddressMode_Num will not fit on ESamplerAddressMode_NumBits");

enum ESamplerCompareFunction
{
	SCF_Never,
	SCF_Less
};

enum ERasterizerFillMode
{
	FM_Point,
	FM_Wireframe,
	FM_Solid,

	ERasterizerFillMode_Num,
	ERasterizerFillMode_NumBits = 2,
};
static_assert(ERasterizerFillMode_Num <= (1 << ERasterizerFillMode_NumBits), "ERasterizerFillMode_Num will not fit on ERasterizerFillMode_NumBits");

enum ERasterizerCullMode
{
	CM_None,
	CM_CW,
	CM_CCW,

	ERasterizerCullMode_Num,
	ERasterizerCullMode_NumBits = 2,
};
static_assert(ERasterizerCullMode_Num <= (1 << ERasterizerCullMode_NumBits), "ERasterizerCullMode_Num will not fit on ERasterizerCullMode_NumBits");

enum class ERasterizerDepthClipMode : uint8
{
	DepthClip,
	DepthClamp,

	Num,
	NumBits = 1,
};
static_assert(uint32(ERasterizerDepthClipMode::Num) <= (1U << uint32(ERasterizerDepthClipMode::NumBits)), "ERasterizerDepthClipMode::Num will not fit on ERasterizerDepthClipMode::NumBits");

enum EColorWriteMask
{
	CW_RED   = 0x01,
	CW_GREEN = 0x02,
	CW_BLUE  = 0x04,
	CW_ALPHA = 0x08,

	CW_NONE  = 0,
	CW_RGB   = CW_RED | CW_GREEN | CW_BLUE,
	CW_RGBA  = CW_RED | CW_GREEN | CW_BLUE | CW_ALPHA,
	CW_RG    = CW_RED | CW_GREEN,
	CW_BA    = CW_BLUE | CW_ALPHA,

	EColorWriteMask_NumBits = 4,
};

enum ECompareFunction
{
	CF_Less,
	CF_LessEqual,
	CF_Greater,
	CF_GreaterEqual,
	CF_Equal,
	CF_NotEqual,
	CF_Never,
	CF_Always,

	ECompareFunction_Num,
	ECompareFunction_NumBits = 3,

	// Utility enumerations
	CF_DepthNearOrEqual		= (((int32)ERHIZBuffer::IsInverted != 0) ? CF_GreaterEqual : CF_LessEqual),
	CF_DepthNear			= (((int32)ERHIZBuffer::IsInverted != 0) ? CF_Greater : CF_Less),
	CF_DepthFartherOrEqual	= (((int32)ERHIZBuffer::IsInverted != 0) ? CF_LessEqual : CF_GreaterEqual),
	CF_DepthFarther			= (((int32)ERHIZBuffer::IsInverted != 0) ? CF_Less : CF_Greater),
};
static_assert(ECompareFunction_Num <= (1 << ECompareFunction_NumBits), "ECompareFunction_Num will not fit on ECompareFunction_NumBits");

enum EStencilMask
{
	SM_Default,
	SM_255,
	SM_1,
	SM_2,
	SM_4,
	SM_8,
	SM_16,
	SM_32,
	SM_64,
	SM_128,
	SM_Count
};

enum EStencilOp
{
	SO_Keep,
	SO_Zero,
	SO_Replace,
	SO_SaturatedIncrement,
	SO_SaturatedDecrement,
	SO_Invert,
	SO_Increment,
	SO_Decrement,

	EStencilOp_Num,
	EStencilOp_NumBits = 3,
};
static_assert(EStencilOp_Num <= (1 << EStencilOp_NumBits), "EStencilOp_Num will not fit on EStencilOp_NumBits");

enum EBlendOperation
{
	BO_Add,
	BO_Subtract,
	BO_Min,
	BO_Max,
	BO_ReverseSubtract,

	EBlendOperation_Num,
	EBlendOperation_NumBits = 3,
};
static_assert(EBlendOperation_Num <= (1 << EBlendOperation_NumBits), "EBlendOperation_Num will not fit on EBlendOperation_NumBits");

enum EBlendFactor
{
	BF_Zero,
	BF_One,
	BF_SourceColor,
	BF_InverseSourceColor,
	BF_SourceAlpha,
	BF_InverseSourceAlpha,
	BF_DestAlpha,
	BF_InverseDestAlpha,
	BF_DestColor,
	BF_InverseDestColor,
	BF_ConstantBlendFactor,
	BF_InverseConstantBlendFactor,
	BF_Source1Color,
	BF_InverseSource1Color,
	BF_Source1Alpha,
	BF_InverseSource1Alpha,

	EBlendFactor_Num,
	EBlendFactor_NumBits = 4,
};
static_assert(EBlendFactor_Num <= (1 << EBlendFactor_NumBits), "EBlendFactor_Num will not fit on EBlendFactor_NumBits");

enum EVertexElementType
{
	VET_None,
	VET_Float1,
	VET_Float2,
	VET_Float3,
	VET_Float4,
	VET_PackedNormal,	// FPackedNormal
	VET_UByte4,
	VET_UByte4N,
	VET_Color,
	VET_Short2,
	VET_Short4,
	VET_Short2N,		// 16 bit word normalized to (value/32767.0,value/32767.0,0,0,1)
	VET_Half2,			// 16 bit float using 1 bit sign, 5 bit exponent, 10 bit mantissa 
	VET_Half4,
	VET_Short4N,		// 4 X 16 bit word, normalized 
	VET_UShort2,
	VET_UShort4,
	VET_UShort2N,		// 16 bit word normalized to (value/65535.0,value/65535.0,0,0,1)
	VET_UShort4N,		// 4 X 16 bit word unsigned, normalized 
	VET_URGB10A2N,		// 10 bit r, g, b and 2 bit a normalized to (value/1023.0f, value/1023.0f, value/1023.0f, value/3.0f)
	VET_UInt,
	VET_MAX,

	VET_NumBits = 5,
};
static_assert(VET_MAX <= (1 << VET_NumBits), "VET_MAX will not fit on VET_NumBits");
DECLARE_INTRINSIC_TYPE_LAYOUT(EVertexElementType);

enum ECubeFace
{
	CubeFace_PosX = 0,
	CubeFace_NegX,
	CubeFace_PosY,
	CubeFace_NegY,
	CubeFace_PosZ,
	CubeFace_NegZ,
	CubeFace_MAX
};

enum EUniformBufferUsage
{
	// the uniform buffer is temporary, used for a single draw call then discarded
	UniformBuffer_SingleDraw = 0,
	// the uniform buffer is used for multiple draw calls but only for the current frame
	UniformBuffer_SingleFrame,
	// the uniform buffer is used for multiple draw calls, possibly across multiple frames
	UniformBuffer_MultiFrame,
};

enum class EUniformBufferValidation
{
	None,
	ValidateResources
};

/** The USF binding type for a resource in a shader. */
enum class EShaderCodeResourceBindingType : uint8
{
	Invalid,

	SamplerState,

	// Texture1D: not used in the renderer.
	// Texture1DArray: not used in the renderer.
	Texture2D,
	Texture2DArray,
	Texture2DMS,
	Texture3D,
	// Texture3DArray: not used in the renderer.
	TextureCube,
	TextureCubeArray,
	TextureMetadata,

	Buffer,
	StructuredBuffer,
	ByteAddressBuffer,
	RaytracingAccelerationStructure,

	// RWTexture1D: not used in the renderer.
	// RWTexture1DArray: not used in the renderer.
	RWTexture2D,
	RWTexture2DArray,
	RWTexture3D,
	// RWTexture3DArray: not used in the renderer.
	RWTextureCube,
	// RWTextureCubeArray: not used in the renderer.
	RWTextureMetadata,

	RWBuffer,
	RWStructuredBuffer,
	RWByteAddressBuffer,

	RasterizerOrderedTexture2D,

	MAX
};

/** The base type of a value in a shader parameter structure. */
enum EUniformBufferBaseType : uint8
{
	UBMT_INVALID,

	// Invalid type when trying to use bool, to have explicit error message to programmer on why
	// they shouldn't use bool in shader parameter structures.
	UBMT_BOOL,

	// Parameter types.
	UBMT_INT32,
	UBMT_UINT32,
	UBMT_FLOAT32,

	// RHI resources not tracked by render graph.
	UBMT_TEXTURE,
	UBMT_SRV,
	UBMT_UAV,
	UBMT_SAMPLER,

	// Resources tracked by render graph.
	UBMT_RDG_TEXTURE,
	UBMT_RDG_TEXTURE_ACCESS,
	UBMT_RDG_TEXTURE_ACCESS_ARRAY,
	UBMT_RDG_TEXTURE_SRV,
	UBMT_RDG_TEXTURE_UAV,
	UBMT_RDG_BUFFER_ACCESS,
	UBMT_RDG_BUFFER_ACCESS_ARRAY,
	UBMT_RDG_BUFFER_SRV,
	UBMT_RDG_BUFFER_UAV,
	UBMT_RDG_UNIFORM_BUFFER,

	// Nested structure.
	UBMT_NESTED_STRUCT,

	// Structure that is nested on C++ side, but included on shader side.
	UBMT_INCLUDED_STRUCT,

	// GPU Indirection reference of struct, like is currently named Uniform buffer.
	UBMT_REFERENCED_STRUCT,

	// Structure dedicated to setup render targets for a rasterizer pass.
	UBMT_RENDER_TARGET_BINDING_SLOTS,

	EUniformBufferBaseType_Num,
	EUniformBufferBaseType_NumBits = 5,
};
static_assert(EUniformBufferBaseType_Num <= (1 << EUniformBufferBaseType_NumBits), "EUniformBufferBaseType_Num will not fit on EUniformBufferBaseType_NumBits");
DECLARE_INTRINSIC_TYPE_LAYOUT(EUniformBufferBaseType);

/** The list of flags declaring which binding models are allowed for a uniform buffer layout. */
enum class EUniformBufferBindingFlags : uint8
{
	/** If set, the uniform buffer can be bound as an RHI shader parameter on an RHI shader (i.e. RHISetShaderUniformBuffer). */
	Shader = 1 << 0,

	/** If set, the uniform buffer can be bound globally through a static slot (i.e. RHISetStaticUniformBuffers). */
	Static = 1 << 1,

	/** If set, the uniform buffer can be bound globally or per-shader, depending on the use case. Only one binding model should be
	 *  used at a time, and RHI validation will emit an error if both are used for a particular uniform buffer at the same time. This
	 *  is designed for difficult cases where a fixed single binding model would produce an unnecessary maintenance burden. Using this
	 *  disables some RHI validation errors for global bindings, so use with care.
	 */
	StaticAndShader = Static | Shader
};
ENUM_CLASS_FLAGS(EUniformBufferBindingFlags);
DECLARE_INTRINSIC_TYPE_LAYOUT(EUniformBufferBindingFlags);

/** Numerical type used to store the static slot indices. */
using FUniformBufferStaticSlot = uint8;

enum
{
	/** The maximum number of static slots allowed. */
	MAX_UNIFORM_BUFFER_STATIC_SLOTS = 255
};

/** Returns whether a static uniform buffer slot index is valid. */
inline bool IsUniformBufferStaticSlotValid(const FUniformBufferStaticSlot Slot)
{
	return Slot < MAX_UNIFORM_BUFFER_STATIC_SLOTS;
}

struct FRHIResourceTableEntry
{
public:
	static CONSTEXPR uint32 GetEndOfStreamToken()
	{
		return 0xffffffff;
	}

	static uint32 Create(uint16 UniformBufferIndex, uint16 ResourceIndex, uint16 BindIndex)
	{
		return ((UniformBufferIndex & RTD_Mask_UniformBufferIndex) << RTD_Shift_UniformBufferIndex) |
			((ResourceIndex & RTD_Mask_ResourceIndex) << RTD_Shift_ResourceIndex) |
			((BindIndex & RTD_Mask_BindIndex) << RTD_Shift_BindIndex);
	}

	static inline uint16 GetUniformBufferIndex(uint32 Data)
	{
		return (Data >> RTD_Shift_UniformBufferIndex) & RTD_Mask_UniformBufferIndex;
	}

	static inline uint16 GetResourceIndex(uint32 Data)
	{
		return (Data >> RTD_Shift_ResourceIndex) & RTD_Mask_ResourceIndex;
	}

	static inline uint16 GetBindIndex(uint32 Data)
	{
		return (Data >> RTD_Shift_BindIndex) & RTD_Mask_BindIndex;
	}

private:
	enum EResourceTableDefinitions
	{
		RTD_NumBits_UniformBufferIndex	= 8,
		RTD_NumBits_ResourceIndex		= 16,
		RTD_NumBits_BindIndex			= 8,

		RTD_Mask_UniformBufferIndex		= (1 << RTD_NumBits_UniformBufferIndex) - 1,
		RTD_Mask_ResourceIndex			= (1 << RTD_NumBits_ResourceIndex) - 1,
		RTD_Mask_BindIndex				= (1 << RTD_NumBits_BindIndex) - 1,

		RTD_Shift_BindIndex				= 0,
		RTD_Shift_ResourceIndex			= RTD_Shift_BindIndex + RTD_NumBits_BindIndex,
		RTD_Shift_UniformBufferIndex	= RTD_Shift_ResourceIndex + RTD_NumBits_ResourceIndex,
	};
	static_assert(RTD_NumBits_UniformBufferIndex + RTD_NumBits_ResourceIndex + RTD_NumBits_BindIndex <= sizeof(uint32)* 8, "RTD_* values must fit in 32 bits");
};

enum EResourceLockMode
{
	RLM_ReadOnly,
	RLM_WriteOnly,
	RLM_WriteOnly_NoOverwrite,
	RLM_Num
};

/** limited to 8 types in FReadSurfaceDataFlags */
enum ERangeCompressionMode
{
	// 0 .. 1
	RCM_UNorm,
	// -1 .. 1
	RCM_SNorm,
	// 0 .. 1 unless there are smaller values than 0 or bigger values than 1, then the range is extended to the minimum or the maximum of the values
	RCM_MinMaxNorm,
	// minimum .. maximum (each channel independent)
	RCM_MinMax,
};

enum class EPrimitiveTopologyType : uint8
{
	Triangle,
	Patch,
	Line,
	Point,
	//Quad,

	Num,
	NumBits = 2,
};
static_assert((uint32)EPrimitiveTopologyType::Num <= (1 << (uint32)EPrimitiveTopologyType::NumBits), "EPrimitiveTopologyType::Num will not fit on EPrimitiveTopologyType::NumBits");

enum EPrimitiveType
{
	// Topology that defines a triangle N with 3 vertex extremities: 3*N+0, 3*N+1, 3*N+2.
	PT_TriangleList,

	// Topology that defines a triangle N with 3 vertex extremities: N+0, N+1, N+2.
	PT_TriangleStrip,

	// Topology that defines a line with 2 vertex extremities: 2*N+0, 2*N+1.
	PT_LineList,

	// Topology that defines a quad N with 4 vertex extremities: 4*N+0, 4*N+1, 4*N+2, 4*N+3.
	// Supported only if GRHISupportsQuadTopology == true.
	PT_QuadList,

	// Topology that defines a point N with a single vertex N.
	PT_PointList,

	// Topology that defines a screen aligned rectangle N with only 3 vertex corners:
	//    3*N + 0 is upper-left corner,
	//    3*N + 1 is upper-right corner,
	//    3*N + 2 is the lower-left corner.
	// Supported only if GRHISupportsRectTopology == true.
	PT_RectList,

	// Tesselation patch list. Supported only if tesselation is supported.
	PT_1_ControlPointPatchList,
	PT_2_ControlPointPatchList,
	PT_3_ControlPointPatchList,
	PT_4_ControlPointPatchList,
	PT_5_ControlPointPatchList,
	PT_6_ControlPointPatchList,
	PT_7_ControlPointPatchList,
	PT_8_ControlPointPatchList,
	PT_9_ControlPointPatchList,
	PT_10_ControlPointPatchList,
	PT_11_ControlPointPatchList,
	PT_12_ControlPointPatchList,
	PT_13_ControlPointPatchList,
	PT_14_ControlPointPatchList,
	PT_15_ControlPointPatchList,
	PT_16_ControlPointPatchList,
	PT_17_ControlPointPatchList,
	PT_18_ControlPointPatchList,
	PT_19_ControlPointPatchList,
	PT_20_ControlPointPatchList,
	PT_21_ControlPointPatchList,
	PT_22_ControlPointPatchList,
	PT_23_ControlPointPatchList,
	PT_24_ControlPointPatchList,
	PT_25_ControlPointPatchList,
	PT_26_ControlPointPatchList,
	PT_27_ControlPointPatchList,
	PT_28_ControlPointPatchList,
	PT_29_ControlPointPatchList,
	PT_30_ControlPointPatchList,
	PT_31_ControlPointPatchList,
	PT_32_ControlPointPatchList,
	PT_Num,
	PT_NumBits = 6
};
static_assert(PT_Num <= (1 << 8), "EPrimitiveType doesn't fit in a byte");
static_assert(PT_Num <= (1 << PT_NumBits), "PT_NumBits is too small");

enum EVRSAxisShadingRate : uint8
{
	VRSASR_1X = 0x0,
	VRSASR_2X = 0x1,
	VRSASR_4X = 0x2,
};

enum EVRSShadingRate : uint8
{
	VRSSR_1x1  = (VRSASR_1X << 2) + VRSASR_1X,
	VRSSR_1x2  = (VRSASR_1X << 2) + VRSASR_2X,
	VRSSR_2x1  = (VRSASR_2X << 2) + VRSASR_1X,
	VRSSR_2x2  = (VRSASR_2X << 2) + VRSASR_2X,
	VRSSR_2x4  = (VRSASR_2X << 2) + VRSASR_4X,
	VRSSR_4x2  = (VRSASR_4X << 2) + VRSASR_2X,
	VRSSR_4x4  = (VRSASR_4X << 2) + VRSASR_4X,
};

enum EVRSRateCombiner : uint8
{
	VRSRB_Passthrough,
	VRSRB_Override,
	VRSRB_Min,
	VRSRB_Max,
	VRSRB_Sum,
};

enum EVRSImageDataType : uint8
{
	VRSImage_NotSupported,		// Image-based Variable Rate Shading is not supported on the current device/platform.
	VRSImage_Palette,			// Image-based VRS uses a palette of discrete, enumerated values to describe shading rate per tile.
	VRSImage_Fractional,		// Image-based VRS uses a floating point value to describe shading rate in X/Y (e.g. 1.0f is full rate, 0.5f is half-rate, 0.25f is 1/4 rate, etc).
};

/**
 *	Resource usage flags - for vertex and index buffers.
 */
enum class EBufferUsageFlags : uint32
{
	None                    = 0,

	/** The buffer will be written to once. */
	Static                  = 1 << 0,

	/** The buffer will be written to occasionally, GPU read only, CPU write only.  The data lifetime is until the next update, or the buffer is destroyed. */
	Dynamic                 = 1 << 1,

	/** The buffer's data will have a lifetime of one frame.  It MUST be written to each frame, or a new one created each frame. */
	Volatile                = 1 << 2,

	/** Allows an unordered access view to be created for the buffer. */
	UnorderedAccess         = 1 << 3,

	/** Create a byte address buffer, which is basically a structured buffer with a uint32 type. */
	ByteAddressBuffer       = 1 << 4,

	/** Buffer that the GPU will use as a source for a copy. */
	SourceCopy              = 1 << 5,

	/** Create a buffer that can be bound as a stream output target. */
	StreamOutput            = 1 << 6,

	/** Create a buffer which contains the arguments used by DispatchIndirect or DrawIndirect. */
	DrawIndirect            = 1 << 7,

	/** 
	 * Create a buffer that can be bound as a shader resource. 
	 * This is only needed for buffer types which wouldn't ordinarily be used as a shader resource, like a vertex buffer.
	 */
	ShaderResource          = 1 << 8,

	/** Request that this buffer is directly CPU accessible. */
	KeepCPUAccessible       = 1 << 9,

	/** Buffer should go in fast vram (hint only). Requires BUF_Transient */
	FastVRAM                = 1 << 10,

	/** Buffer should be allocated from transient memory. */
	Transient UE_DEPRECATED(5.0, "EBufferUsageFlags::Transient flag is no longer used.") = None,

	/** Create a buffer that can be shared with an external RHI or process. */
	Shared                  = 1 << 12,

	/**
	 * Buffer contains opaque ray tracing acceleration structure data.
	 * Resources with this flag can't be bound directly to any shader stage and only can be used with ray tracing APIs.
	 * This flag is mutually exclusive with all other buffer flags except BUF_Static.
	*/
	AccelerationStructure   = 1 << 13,

	VertexBuffer            = 1 << 14,
	IndexBuffer             = 1 << 15,
	StructuredBuffer        = 1 << 16,

	/** Buffer memory is allocated independently for multiple GPUs, rather than shared via driver aliasing */
	MultiGPUAllocate		= 1 << 17,

	/**
	 * Tells the render graph to not bother transferring across GPUs in multi-GPU scenarios.  Useful for cases where
	 * a buffer is read back to the CPU (such as streaming request buffers), or written to each frame by CPU (such
	 * as indirect arg buffers), and the other GPU doesn't actually care about the data.
	*/
	MultiGPUGraphIgnore		= 1 << 18,
	
	/** Allows buffer to be used as a scratch buffer for building ray tracing acceleration structure,
	 * which implies unordered access. Only changes the buffer alignment and can be combined with other flags.
	**/
	RayTracingScratch = (1 << 19) | UnorderedAccess,

	// Helper bit-masks
	AnyDynamic = (Dynamic | Volatile),
};
ENUM_CLASS_FLAGS(EBufferUsageFlags);

#define BUF_None                   EBufferUsageFlags::None
#define BUF_Static                 EBufferUsageFlags::Static
#define BUF_Dynamic                EBufferUsageFlags::Dynamic
#define BUF_Volatile               EBufferUsageFlags::Volatile
#define BUF_UnorderedAccess        EBufferUsageFlags::UnorderedAccess
#define BUF_ByteAddressBuffer      EBufferUsageFlags::ByteAddressBuffer
#define BUF_SourceCopy             EBufferUsageFlags::SourceCopy
#define BUF_StreamOutput           EBufferUsageFlags::StreamOutput
#define BUF_DrawIndirect           EBufferUsageFlags::DrawIndirect
#define BUF_ShaderResource         EBufferUsageFlags::ShaderResource
#define BUF_KeepCPUAccessible      EBufferUsageFlags::KeepCPUAccessible
#define BUF_FastVRAM               EBufferUsageFlags::FastVRAM
#define BUF_Transient              EBufferUsageFlags::Transient
#define BUF_Shared                 EBufferUsageFlags::Shared
#define BUF_AccelerationStructure  EBufferUsageFlags::AccelerationStructure
#define BUF_RayTracingScratch	   EBufferUsageFlags::RayTracingScratch
#define BUF_VertexBuffer           EBufferUsageFlags::VertexBuffer
#define BUF_IndexBuffer            EBufferUsageFlags::IndexBuffer
#define BUF_StructuredBuffer       EBufferUsageFlags::StructuredBuffer
#define BUF_AnyDynamic             EBufferUsageFlags::AnyDynamic
#define BUF_MultiGPUAllocate       EBufferUsageFlags::MultiGPUAllocate
#define BUF_MultiGPUGraphIgnore    EBufferUsageFlags::MultiGPUGraphIgnore

enum class EGpuVendorId
{
	Unknown		= -1,
	NotQueried	= 0,

	Amd			= 0x1002,
	ImgTec		= 0x1010,
	Nvidia		= 0x10DE, 
	Arm			= 0x13B5, 
	Broadcom	= 0x14E4,
	Qualcomm	= 0x5143,
	Intel		= 0x8086,
	Apple		= 0x106B,
	Vivante		= 0x7a05,
	VeriSilicon	= 0x1EB1,

	Kazan		= 0x10003,	// VkVendorId
	Codeplay	= 0x10004,	// VkVendorId
	Mesa		= 0x10005,	// VkVendorId
};

/** An enumeration of the different RHI reference types. */
enum ERHIResourceType : uint8
{
	RRT_None,

	RRT_SamplerState,
	RRT_RasterizerState,
	RRT_DepthStencilState,
	RRT_BlendState,
	RRT_VertexDeclaration,
	RRT_VertexShader,
	RRT_MeshShader,
	RRT_AmplificationShader,
	RRT_PixelShader,
	RRT_GeometryShader,
	RRT_RayTracingShader,
	RRT_ComputeShader,
	RRT_GraphicsPipelineState,
	RRT_ComputePipelineState,
	RRT_RayTracingPipelineState,
	RRT_BoundShaderState,
	RRT_UniformBufferLayout,
	RRT_UniformBuffer,
	RRT_Buffer,
	RRT_Texture,
	// @todo: texture type unification - remove these
	RRT_Texture2D,
	RRT_Texture2DArray,
	RRT_Texture3D,
	RRT_TextureCube,
	// @todo: texture type unification - remove these
	RRT_TextureReference,
	RRT_TimestampCalibrationQuery,
	RRT_GPUFence,
	RRT_RenderQuery,
	RRT_RenderQueryPool,
	RRT_ComputeFence,
	RRT_Viewport,
	RRT_UnorderedAccessView,
	RRT_ShaderResourceView,
	RRT_RayTracingAccelerationStructure,
	RRT_StagingBuffer,
	RRT_CustomPresent,
	RRT_ShaderLibrary,
	RRT_PipelineBinaryLibrary,

	RRT_Num
};

/** Describes the dimension of a texture. */
enum class ETextureDimension : uint8
{
	Texture2D,
	Texture2DArray,
	Texture3D,
	TextureCube,
	TextureCubeArray
};

/** Flags used for texture creation */
enum class ETextureCreateFlags : uint64
{
    None                              = 0,

    // Texture can be used as a render target
    RenderTargetable                  = 1ull << 0,
    // Texture can be used as a resolve target
    ResolveTargetable                 = 1ull << 1,
    // Texture can be used as a depth-stencil target.
    DepthStencilTargetable            = 1ull << 2,
    // Texture can be used as a shader resource.
    ShaderResource                    = 1ull << 3,
    // Texture is encoded in sRGB gamma space
    SRGB                              = 1ull << 4,
    // Texture data is writable by the CPU
    CPUWritable                       = 1ull << 5,
    // Texture will be created with an un-tiled format
    NoTiling                          = 1ull << 6,
    // Texture will be used for video decode
    VideoDecode                       = 1ull << 7,
    // Texture that may be updated every frame
    Dynamic                           = 1ull << 8,
    // Texture will be used as a render pass attachment that will be read from
    InputAttachmentRead               = 1ull << 9,
    /** Texture represents a foveation attachment */
    Foveation                         = 1ull << 10,
    // Prefer 3D internal surface tiling mode for volume textures when possible
    Tiling3D                          = 1ull << 11,
    // This texture has no GPU or CPU backing. It only exists in tile memory on TBDR GPUs (i.e., mobile).
    Memoryless                        = 1ull << 12,
    // Create the texture with the flag that allows mip generation later, only applicable to D3D11
    GenerateMipCapable                = 1ull << 13,
    // The texture can be partially allocated in fastvram
    FastVRAMPartialAlloc              = 1ull << 14,
    // Do not create associated shader resource view, only applicable to D3D11 and D3D12
    DisableSRVCreation                = 1ull << 15,
    // Do not allow Delta Color Compression (DCC) to be used with this texture
    DisableDCC                        = 1ull << 16,
    // UnorderedAccessView (DX11 only)
    // Warning: Causes additional synchronization between draw calls when using a render target allocated with this flag, use sparingly
    // See: GCNPerformanceTweets.pdf Tip 37
    UAV                               = 1ull << 17,
    // Render target texture that will be displayed on screen (back buffer)
    Presentable                       = 1ull << 18,
    // Texture data is accessible by the CPU
    CPUReadback                       = 1ull << 19,
    // Texture was processed offline (via a texture conversion process for the current platform)
    OfflineProcessed                  = 1ull << 20,
    // Texture needs to go in fast VRAM if available (HINT only)
    FastVRAM                          = 1ull << 21,
    // by default the texture is not showing up in the list - this is to reduce clutter, using the FULL option this can be ignored
    HideInVisualizeTexture            = 1ull << 22,
    // Texture should be created in virtual memory, with no physical memory allocation made
    // You must make further calls to RHIVirtualTextureSetFirstMipInMemory to allocate physical memory
    // and RHIVirtualTextureSetFirstMipVisible to map the first mip visible to the GPU
    Virtual                           = 1ull << 23,
    // Creates a RenderTargetView for each array slice of the texture
    // Warning: if this was specified when the resource was created, you can't use SV_RenderTargetArrayIndex to route to other slices!
    TargetArraySlicesIndependently    = 1ull << 24,
    // Texture that may be shared with DX9 or other devices
    Shared                            = 1ull << 25,
    // RenderTarget will not use full-texture fast clear functionality.
    NoFastClear                       = 1ull << 26,
    // Texture is a depth stencil resolve target
    DepthStencilResolveTarget         = 1ull << 27,
    // Flag used to indicted this texture is a streamable 2D texture, and should be counted towards the texture streaming pool budget.
    Streamable                        = 1ull << 28,
    // Render target will not FinalizeFastClear; Caches and meta data will be flushed, but clearing will be skipped (avoids potentially trashing metadata)
    NoFastClearFinalize               = 1ull << 29,
    // Hint to the driver that this resource is managed properly by the engine for Alternate-Frame-Rendering in mGPU usage.
    AFRManual                         = 1ull << 30,
    // Workaround for 128^3 volume textures getting bloated 4x due to tiling mode on some platforms.
    ReduceMemoryWithTilingMode        = 1ull << 31,
    /** Texture should be allocated from transient memory. */
    Transient UE_DEPRECATED(5.0, "ETextureCreateFlags::Transient flag is no longer used.") = None,
    /** Texture needs to support atomic operations */
    AtomicCompatible                  = 1ull << 33,
	/** Texture should be allocated for external access. Vulkan only */
	External                		  = 1ull << 34,
	/** Don't automatically transfer across GPUs in multi-GPU scenarios.  For example, if you are transferring it yourself manually. */
	MultiGPUGraphIgnore				  = 1ull << 35,
	/** Texture needs to support atomic operations */
    Atomic64Compatible                = 1ull << 36,
};
ENUM_CLASS_FLAGS(ETextureCreateFlags);

// Compatibility defines
#define TexCreate_None                           ETextureCreateFlags::None
#define TexCreate_RenderTargetable               ETextureCreateFlags::RenderTargetable
#define TexCreate_ResolveTargetable              ETextureCreateFlags::ResolveTargetable
#define TexCreate_DepthStencilTargetable         ETextureCreateFlags::DepthStencilTargetable
#define TexCreate_ShaderResource                 ETextureCreateFlags::ShaderResource
#define TexCreate_SRGB                           ETextureCreateFlags::SRGB
#define TexCreate_CPUWritable                    ETextureCreateFlags::CPUWritable
#define TexCreate_NoTiling                       ETextureCreateFlags::NoTiling
#define TexCreate_VideoDecode                    ETextureCreateFlags::VideoDecode
#define TexCreate_Dynamic                        ETextureCreateFlags::Dynamic
#define TexCreate_InputAttachmentRead            ETextureCreateFlags::InputAttachmentRead
#define TexCreate_Foveation                      ETextureCreateFlags::Foveation
#define TexCreate_3DTiling                       ETextureCreateFlags::Tiling3D
#define TexCreate_Memoryless                     ETextureCreateFlags::Memoryless
#define TexCreate_GenerateMipCapable             ETextureCreateFlags::GenerateMipCapable
#define TexCreate_FastVRAMPartialAlloc           ETextureCreateFlags::FastVRAMPartialAlloc
#define TexCreate_DisableSRVCreation             ETextureCreateFlags::DisableSRVCreation
#define TexCreate_DisableDCC                     ETextureCreateFlags::DisableDCC
#define TexCreate_UAV                            ETextureCreateFlags::UAV
#define TexCreate_Presentable                    ETextureCreateFlags::Presentable
#define TexCreate_CPUReadback                    ETextureCreateFlags::CPUReadback
#define TexCreate_OfflineProcessed               ETextureCreateFlags::OfflineProcessed
#define TexCreate_FastVRAM                       ETextureCreateFlags::FastVRAM
#define TexCreate_HideInVisualizeTexture         ETextureCreateFlags::HideInVisualizeTexture
#define TexCreate_Virtual                        ETextureCreateFlags::Virtual
#define TexCreate_TargetArraySlicesIndependently ETextureCreateFlags::TargetArraySlicesIndependently
#define TexCreate_Shared                         ETextureCreateFlags::Shared
#define TexCreate_NoFastClear                    ETextureCreateFlags::NoFastClear
#define TexCreate_DepthStencilResolveTarget      ETextureCreateFlags::DepthStencilResolveTarget
#define TexCreate_Streamable                     ETextureCreateFlags::Streamable
#define TexCreate_NoFastClearFinalize            ETextureCreateFlags::NoFastClearFinalize
#define TexCreate_AFRManual                      ETextureCreateFlags::AFRManual
#define TexCreate_ReduceMemoryWithTilingMode     ETextureCreateFlags::ReduceMemoryWithTilingMode
#define TexCreate_Transient                      ETextureCreateFlags::Transient
#define TexCreate_AtomicCompatible               ETextureCreateFlags::AtomicCompatible
#define TexCreate_External               		 ETextureCreateFlags::External
#define TexCreate_MultiGPUGraphIgnore            ETextureCreateFlags::MultiGPUGraphIgnore

enum EAsyncComputePriority
{
	AsyncComputePriority_Default = 0,
	AsyncComputePriority_High,
};

/**
 * Async texture reallocation status, returned by RHIGetReallocateTexture2DStatus().
 */
enum ETextureReallocationStatus
{
	TexRealloc_Succeeded = 0,
	TexRealloc_Failed,
	TexRealloc_InProgress,
};

/**
 * Action to take when a render target is set.
 */
enum class ERenderTargetLoadAction : uint8
{
	// Untouched contents of the render target are undefined. Any existing content is not preserved.
	ENoAction,

	// Existing contents are preserved.
	ELoad,

	// The render target is cleared to the fast clear value specified on the resource.
	EClear,

	Num,
	NumBits = 2,
};
static_assert((uint32)ERenderTargetLoadAction::Num <= (1 << (uint32)ERenderTargetLoadAction::NumBits), "ERenderTargetLoadAction::Num will not fit on ERenderTargetLoadAction::NumBits");

/**
 * Action to take when a render target is unset or at the end of a pass. 
 */
enum class ERenderTargetStoreAction : uint8
{
	// Contents of the render target emitted during the pass are not stored back to memory.
	ENoAction,

	// Contents of the render target emitted during the pass are stored back to memory.
	EStore,

	// Contents of the render target emitted during the pass are resolved using a box filter and stored back to memory.
	EMultisampleResolve,

	Num,
	NumBits = 2,
};
static_assert((uint32)ERenderTargetStoreAction::Num <= (1 << (uint32)ERenderTargetStoreAction::NumBits), "ERenderTargetStoreAction::Num will not fit on ERenderTargetStoreAction::NumBits");

/**
 * Common render target use cases
 */
enum class ESimpleRenderTargetMode
{
	// These will all store out color and depth
	EExistingColorAndDepth,							// Color = Existing, Depth = Existing
	EUninitializedColorAndDepth,					// Color = ????, Depth = ????
	EUninitializedColorExistingDepth,				// Color = ????, Depth = Existing
	EUninitializedColorClearDepth,					// Color = ????, Depth = Default
	EClearColorExistingDepth,						// Clear Color = whatever was bound to the rendertarget at creation time. Depth = Existing
	EClearColorAndDepth,							// Clear color and depth to bound clear values.
	EExistingContents_NoDepthStore,					// Load existing contents, but don't store depth out.  depth can be written.
	EExistingColorAndClearDepth,					// Color = Existing, Depth = clear value
	EExistingColorAndDepthAndClearStencil,			// Color = Existing, Depth = Existing, Stencil = clear

	// If you add an item here, make sure to add it to DecodeRenderTargetMode() as well!
};

enum class EClearDepthStencil
{
	Depth,
	Stencil,
	DepthStencil,
};

/**
 * Hint to the driver on how to load balance async compute work.  On some platforms this may be a priority, on others actually masking out parts of the GPU for types of work.
 */
enum class EAsyncComputeBudget
{
	ELeast_0,			//Least amount of GPU allocated to AsyncCompute that still gets 'some' done.
	EGfxHeavy_1,		//Gfx gets most of the GPU.
	EBalanced_2,		//Async compute and Gfx share GPU equally.
	EComputeHeavy_3,	//Async compute can use most of the GPU
	EAll_4,				//Async compute can use the entire GPU.
};

enum class ERHIDescriptorHeapType : uint8
{
	Standard,
	Sampler,
	RenderTarget,
	DepthStencil,
	count
};

struct FRHIDescriptorHandle
{
	FRHIDescriptorHandle() = default;
	FRHIDescriptorHandle(ERHIDescriptorHeapType InType, uint32 InIndex)
		: Index(InIndex)
		, Type(InType)
	{
	}

	inline uint32                GetIndex() const { return Index; }
	inline ERHIDescriptorHeapType GetType() const { return Type; }

	inline bool IsValid() const { return Index != UINT_MAX && Type != ERHIDescriptorHeapType::count; }

private:
	uint32                 Index{ UINT_MAX };
	ERHIDescriptorHeapType Type{ ERHIDescriptorHeapType::count };
};

using FDisplayInformationArray = TArray<struct FDisplayInformation>;

inline bool IsPCPlatform(const FStaticShaderPlatform Platform)
{
	return FDataDrivenShaderPlatformInfo::GetIsPC(Platform);
}

/** Whether the shader platform corresponds to the ES3.1/Metal/Vulkan feature level. */
inline bool IsMobilePlatform(const FStaticShaderPlatform Platform)
{
	return FDataDrivenShaderPlatformInfo::GetMaxFeatureLevel(Platform) == ERHIFeatureLevel::ES3_1;
}

inline bool IsCustomPlatform(const FStaticShaderPlatform Platform)
{
	return (Platform >= EShaderPlatform::SP_CUSTOM_PLATFORM_FIRST) && (Platform < EShaderPlatform::SP_CUSTOM_PLATFORM_LAST);
}

inline bool IsOpenGLPlatform(const FStaticShaderPlatform Platform)
{
	return FDataDrivenShaderPlatformInfo::GetIsLanguageOpenGL(Platform);
}

inline bool IsMetalPlatform(const FStaticShaderPlatform Platform)
{
	return FDataDrivenShaderPlatformInfo::GetIsLanguageMetal(Platform);
}

inline bool IsMetalMobilePlatform(const FStaticShaderPlatform Platform)
{
	return FDataDrivenShaderPlatformInfo::GetIsLanguageMetal(Platform)
		&& FDataDrivenShaderPlatformInfo::GetIsMobile(Platform);
}

inline bool IsMetalMRTPlatform(const FStaticShaderPlatform Platform)
{
	return FDataDrivenShaderPlatformInfo::GetIsMetalMRT(Platform);
}

inline bool IsMetalSM5Platform(const FStaticShaderPlatform Platform)
{
	return FDataDrivenShaderPlatformInfo::GetIsLanguageMetal(Platform)
		&& FDataDrivenShaderPlatformInfo::GetMaxFeatureLevel(Platform) == ERHIFeatureLevel::SM5;
}

inline bool IsConsolePlatform(const FStaticShaderPlatform Platform)
{
	return FDataDrivenShaderPlatformInfo::GetIsConsole(Platform);
}

// @todo: data drive uses of this function
inline bool IsAndroidPlatform(const FStaticShaderPlatform Platform)
{
	return (Platform == SP_VULKAN_ES3_1_ANDROID) || (Platform == SP_VULKAN_SM5_ANDROID) || (Platform == SP_OPENGL_ES3_1_ANDROID);
}

inline bool IsVulkanPlatform(const FStaticShaderPlatform Platform)
{
	return FDataDrivenShaderPlatformInfo::GetIsLanguageVulkan(Platform);
}

inline bool IsVulkanSM5Platform(const FStaticShaderPlatform Platform)
{
	return FDataDrivenShaderPlatformInfo::GetIsLanguageVulkan(Platform)
		&& FDataDrivenShaderPlatformInfo::GetMaxFeatureLevel(Platform) == ERHIFeatureLevel::SM5;
}

// @todo: data drive uses of this function
inline bool IsVulkanMobileSM5Platform(const FStaticShaderPlatform Platform)
{
	return Platform == SP_VULKAN_SM5_ANDROID;
// 	return FDataDrivenShaderPlatformInfo::GetIsLanguageVulkan(Platform)
// 		&& FDataDrivenShaderPlatformInfo::GetMaxFeatureLevel(Platform) == ERHIFeatureLevel::SM5
// 		&& FDataDrivenShaderPlatformInfo::GetIsMobile(Platform);
}

// @todo: data drive uses of this function
inline bool IsMetalMobileSM5Platform(const FStaticShaderPlatform Platform)
{
	return Platform == SP_METAL_MRT;
// 	return FDataDrivenShaderPlatformInfo::GetIsLanguageMetal(Platform)
// 		&& FDataDrivenShaderPlatformInfo::GetMaxFeatureLevel(Platform) == ERHIFeatureLevel::SM5
// 		&& FDataDrivenShaderPlatformInfo::GetIsMobile(Platform);
}

inline bool IsAndroidOpenGLESPlatform(const FStaticShaderPlatform Platform)
{
	return FDataDrivenShaderPlatformInfo::GetIsAndroidOpenGLES(Platform);
}

inline bool IsVulkanMobilePlatform(const FStaticShaderPlatform Platform)
{
	return FDataDrivenShaderPlatformInfo::GetIsLanguageVulkan(Platform)
		//&& FDataDrivenShaderPlatformInfo::GetIsMobile(Platform)
		// This was limited to the ES3_1 platforms when hard coded
		&& FDataDrivenShaderPlatformInfo::GetMaxFeatureLevel(Platform) == ERHIFeatureLevel::ES3_1;
}

inline bool IsD3DPlatform(const FStaticShaderPlatform Platform)
{
	return FDataDrivenShaderPlatformInfo::GetIsLanguageD3D(Platform);
}

inline bool IsHlslccShaderPlatform(const FStaticShaderPlatform Platform)
{
	return IsOpenGLPlatform(Platform) || FDataDrivenShaderPlatformInfo::GetIsHlslcc(Platform);
}

inline FStaticFeatureLevel GetMaxSupportedFeatureLevel(const FStaticShaderPlatform InShaderPlatform)
{
	return FDataDrivenShaderPlatformInfo::GetMaxFeatureLevel(InShaderPlatform);
}

/* Returns true if the shader platform Platform is used to simulate a mobile feature level on a PC platform. */
inline bool IsSimulatedPlatform(const FStaticShaderPlatform Platform)
{
	return FDataDrivenShaderPlatformInfo::GetIsPreviewPlatform(Platform);
}

inline EShaderPlatform GetSimulatedPlatform(FStaticShaderPlatform Platform)
{
	if (IsSimulatedPlatform(Platform))
	{
		return FDataDrivenShaderPlatformInfo::GetPreviewShaderPlatformParent(Platform);
	}

	return Platform;
}

/** Returns true if the feature level is supported by the shader platform. */
inline bool IsFeatureLevelSupported(const FStaticShaderPlatform InShaderPlatform, ERHIFeatureLevel::Type InFeatureLevel)
{
	return InFeatureLevel <= GetMaxSupportedFeatureLevel(InShaderPlatform);
}

inline bool RHISupportsSeparateMSAAAndResolveTextures(const FStaticShaderPlatform Platform)
{
	// Metal mobile devices and Android ES3.1 need to handle MSAA and resolve textures internally (unless RHICreateTexture2D was changed to take an optional resolve target)
	return !IsMetalMobilePlatform(Platform);
}

UE_DEPRECATED(5.1, "This function is no longer in use and will be removed.")
inline bool RHISupportsComputeShaders(const FStaticShaderPlatform Platform)
{
	return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5)
		|| (GetMaxSupportedFeatureLevel(Platform) == ERHIFeatureLevel::ES3_1);
}

inline bool RHISupportsGeometryShaders(const FStaticShaderPlatform Platform)
{
	return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5)
		&& !IsMetalPlatform(Platform)
		&& !IsVulkanMobilePlatform(Platform)
		&& !IsVulkanMobileSM5Platform(Platform);
}

inline bool RHIHasTiledGPU(const FStaticShaderPlatform Platform)
{
	return FDataDrivenShaderPlatformInfo::GetTargetsTiledGPU(Platform);
}

inline bool RHISupportsMobileMultiView(const FStaticShaderPlatform Platform)
{
	return FDataDrivenShaderPlatformInfo::GetSupportsMobileMultiView(Platform);
}

inline bool RHISupportsNativeShaderLibraries(const FStaticShaderPlatform Platform)
{
	return IsMetalPlatform(Platform);
}

inline bool RHISupportsShaderPipelines(const FStaticShaderPlatform Platform)
{
	return !IsMobilePlatform(Platform);
}

inline bool RHISupportsDualSourceBlending(const FStaticShaderPlatform Platform)
{
	// For now only enable support for SM5
	// Metal RHI doesn't support dual source blending properly at the moment.
	return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && (IsD3DPlatform(Platform) || FDataDrivenShaderPlatformInfo::GetSupportsDualSourceBlending(Platform) || IsVulkanPlatform(Platform));
}

// Return what the expected number of samplers will be supported by a feature level
// Note that since the Feature Level is pretty orthogonal to the RHI/HW, this is not going to be perfect
// If should only be used for a guess at the limit, the real limit will not be known until runtime
inline uint32 GetExpectedFeatureLevelMaxTextureSamplers(const FStaticFeatureLevel FeatureLevel)
{
	return 16;
}

/** Returns whether the shader parameter type references an RDG texture. */
inline bool IsRDGTextureReferenceShaderParameterType(EUniformBufferBaseType BaseType)
{
	return
		BaseType == UBMT_RDG_TEXTURE ||
		BaseType == UBMT_RDG_TEXTURE_SRV ||
		BaseType == UBMT_RDG_TEXTURE_UAV ||
		BaseType == UBMT_RDG_TEXTURE_ACCESS ||
		BaseType == UBMT_RDG_TEXTURE_ACCESS_ARRAY;
}

/** Returns whether the shader parameter type references an RDG buffer. */
inline bool IsRDGBufferReferenceShaderParameterType(EUniformBufferBaseType BaseType)
{
	return
		BaseType == UBMT_RDG_BUFFER_SRV ||
		BaseType == UBMT_RDG_BUFFER_UAV ||
		BaseType == UBMT_RDG_BUFFER_ACCESS ||
		BaseType == UBMT_RDG_BUFFER_ACCESS_ARRAY;
}

/** Returns whether the shader parameter type is for RDG access and not actually for shaders. */
inline bool IsRDGResourceAccessType(EUniformBufferBaseType BaseType)
{
	return
		BaseType == UBMT_RDG_TEXTURE_ACCESS ||
		BaseType == UBMT_RDG_TEXTURE_ACCESS_ARRAY ||
		BaseType == UBMT_RDG_BUFFER_ACCESS ||
		BaseType == UBMT_RDG_BUFFER_ACCESS_ARRAY;
}

/** Returns whether the shader parameter type is a reference onto a RDG resource. */
inline bool IsRDGResourceReferenceShaderParameterType(EUniformBufferBaseType BaseType)
{
	return IsRDGTextureReferenceShaderParameterType(BaseType) || IsRDGBufferReferenceShaderParameterType(BaseType) || BaseType == UBMT_RDG_UNIFORM_BUFFER;
}

/** Returns whether the shader parameter type needs to be passdown to RHI through FRHIUniformBufferLayout when creating an uniform buffer. */
inline bool IsShaderParameterTypeForUniformBufferLayout(EUniformBufferBaseType BaseType)
{
	return
		// RHI resource referenced in shader parameter structures.
		BaseType == UBMT_TEXTURE ||
		BaseType == UBMT_SRV ||
		BaseType == UBMT_SAMPLER ||
		BaseType == UBMT_UAV ||

		// RHI is able to access RHI resources from RDG.
		IsRDGResourceReferenceShaderParameterType(BaseType) ||

		// Render graph uses FRHIUniformBufferLayout to walk pass' parameters.
		BaseType == UBMT_REFERENCED_STRUCT ||
		BaseType == UBMT_RENDER_TARGET_BINDING_SLOTS;
}

/** Returns whether the shader parameter type in FRHIUniformBufferLayout is actually ignored by the RHI. */
inline bool IsShaderParameterTypeIgnoredByRHI(EUniformBufferBaseType BaseType)
{
	return
		// Render targets bindings slots needs to be in FRHIUniformBufferLayout for render graph, but the RHI does not actually need to know about it.
		BaseType == UBMT_RENDER_TARGET_BINDING_SLOTS ||

		// Custom access states are used by the render graph.
		IsRDGResourceAccessType(BaseType) ||

		// #yuriy_todo: RHI is able to dereference uniform buffer in root shader parameter structures
		BaseType == UBMT_REFERENCED_STRUCT ||
		BaseType == UBMT_RDG_UNIFORM_BUFFER;
}

inline EGpuVendorId RHIConvertToGpuVendorId(uint32 VendorId)
{
	switch ((EGpuVendorId)VendorId)
	{
	case EGpuVendorId::NotQueried:
		return EGpuVendorId::NotQueried;

	case EGpuVendorId::Amd:
	case EGpuVendorId::Mesa:
	case EGpuVendorId::ImgTec:
	case EGpuVendorId::Nvidia:
	case EGpuVendorId::Arm:
	case EGpuVendorId::Broadcom:
	case EGpuVendorId::Qualcomm:
	case EGpuVendorId::Intel:
		return (EGpuVendorId)VendorId;

	default:
		break;
	}

	return EGpuVendorId::Unknown;
}

inline const TCHAR* GetShaderFrequencyString(EShaderFrequency Frequency, bool bIncludePrefix = true)
{
	const TCHAR* String = TEXT("SF_NumFrequencies");
	switch (Frequency)
	{
	case SF_Vertex:			String = TEXT("SF_Vertex"); break;
	case SF_Mesh:			String = TEXT("SF_Mesh"); break;
	case SF_Amplification:	String = TEXT("SF_Amplification"); break;
	case SF_Geometry:		String = TEXT("SF_Geometry"); break;
	case SF_Pixel:			String = TEXT("SF_Pixel"); break;
	case SF_Compute:		String = TEXT("SF_Compute"); break;
	case SF_RayGen:			String = TEXT("SF_RayGen"); break;
	case SF_RayMiss:		String = TEXT("SF_RayMiss"); break;
	case SF_RayHitGroup:	String = TEXT("SF_RayHitGroup"); break;
	case SF_RayCallable:	String = TEXT("SF_RayCallable"); break;

	default:
		checkf(0, TEXT("Unknown ShaderFrequency %d"), (int32)Frequency);
		break;
	}

	// Skip SF_
	int32 Index = bIncludePrefix ? 0 : 3;
	String += Index;
	return String;
};

inline const TCHAR* GetTextureDimensionString(ETextureDimension Dimension)
{
	switch (Dimension)
	{
	case ETextureDimension::Texture2D:
		return TEXT("Texture2D");
	case ETextureDimension::Texture2DArray:
		return TEXT("Texture2DArray");
	case ETextureDimension::Texture3D:
		return TEXT("Texture3D");
	case ETextureDimension::TextureCube:
		return TEXT("TextureCube");
	case ETextureDimension::TextureCubeArray:
		return TEXT("TextureCubeArray");
	}
	return TEXT("");
}

inline const TCHAR* GetTextureCreateFlagString(ETextureCreateFlags TextureCreateFlag)
{
	switch (TextureCreateFlag)
	{
	case ETextureCreateFlags::None:
		return TEXT("None");
	case ETextureCreateFlags::RenderTargetable:
		return TEXT("RenderTargetable");
	case ETextureCreateFlags::ResolveTargetable:
		return TEXT("ResolveTargetable");
	case ETextureCreateFlags::DepthStencilTargetable:
		return TEXT("DepthStencilTargetable");
	case ETextureCreateFlags::ShaderResource:
		return TEXT("ShaderResource");
	case ETextureCreateFlags::SRGB:
		return TEXT("SRGB");
	case ETextureCreateFlags::CPUWritable:
		return TEXT("CPUWritable");
	case ETextureCreateFlags::NoTiling:
		return TEXT("NoTiling");
	case ETextureCreateFlags::VideoDecode:
		return TEXT("VideoDecode");
	case ETextureCreateFlags::Dynamic:
		return TEXT("Dynamic");
	case ETextureCreateFlags::InputAttachmentRead:
		return TEXT("InputAttachmentRead");
	case ETextureCreateFlags::Foveation:
		return TEXT("Foveation");
	case ETextureCreateFlags::Tiling3D:
		return TEXT("Tiling3D");
	case ETextureCreateFlags::Memoryless:
		return TEXT("Memoryless");
	case ETextureCreateFlags::GenerateMipCapable:
		return TEXT("GenerateMipCapable");
	case ETextureCreateFlags::FastVRAMPartialAlloc:
		return TEXT("FastVRAMPartialAlloc");
	case ETextureCreateFlags::DisableSRVCreation:
		return TEXT("DisableSRVCreation");
	case ETextureCreateFlags::DisableDCC:
		return TEXT("DisableDCC");
	case ETextureCreateFlags::UAV:
		return TEXT("UAV");
	case ETextureCreateFlags::Presentable:
		return TEXT("Presentable");
	case ETextureCreateFlags::CPUReadback:
		return TEXT("CPUReadback");
	case ETextureCreateFlags::OfflineProcessed:
		return TEXT("OfflineProcessed");
	case ETextureCreateFlags::FastVRAM:
		return TEXT("FastVRAM");
	case ETextureCreateFlags::HideInVisualizeTexture:
		return TEXT("HideInVisualizeTexture");
	case ETextureCreateFlags::Virtual:
		return TEXT("Virtual");
	case ETextureCreateFlags::TargetArraySlicesIndependently:
		return TEXT("TargetArraySlicesIndependently");
	case ETextureCreateFlags::Shared:
		return TEXT("Shared");
	case ETextureCreateFlags::NoFastClear:
		return TEXT("NoFastClear");
	case ETextureCreateFlags::DepthStencilResolveTarget:
		return TEXT("DepthStencilResolveTarget");
	case ETextureCreateFlags::Streamable:
		return TEXT("Streamable");
	case ETextureCreateFlags::NoFastClearFinalize:
		return TEXT("NoFastClearFinalize");
	case ETextureCreateFlags::AFRManual:
		return TEXT("AFRManual");
	case ETextureCreateFlags::ReduceMemoryWithTilingMode:
		return TEXT("ReduceMemoryWithTilingMode");
	}
	return TEXT("");
}

inline const TCHAR* GetBufferUsageFlagString(EBufferUsageFlags BufferUsage)
{
	switch (BufferUsage)
	{
	case EBufferUsageFlags::None:
		return TEXT("None");
	case EBufferUsageFlags::Static:
		return TEXT("Static");
	case EBufferUsageFlags::Dynamic:
		return TEXT("Dynamic");
	case EBufferUsageFlags::Volatile:
		return TEXT("Volatile");
	case EBufferUsageFlags::UnorderedAccess:
		return TEXT("UnorderedAccess");
	case EBufferUsageFlags::ByteAddressBuffer:
		return TEXT("ByteAddressBuffer");
	case EBufferUsageFlags::SourceCopy:
		return TEXT("SourceCopy");
	case EBufferUsageFlags::StreamOutput:
		return TEXT("StreamOutput");
	case EBufferUsageFlags::DrawIndirect:
		return TEXT("DrawIndirect");
	case EBufferUsageFlags::ShaderResource:
		return TEXT("ShaderResource");
	case EBufferUsageFlags::KeepCPUAccessible:
		return TEXT("KeepCPUAccessible");
	case EBufferUsageFlags::FastVRAM:
		return TEXT("FastVRAM");
	case EBufferUsageFlags::Shared:
		return TEXT("Shared");
	case EBufferUsageFlags::AccelerationStructure:
		return TEXT("AccelerationStructure");
	case EBufferUsageFlags::VertexBuffer:
		return TEXT("VertexBuffer");
	case EBufferUsageFlags::IndexBuffer:
		return TEXT("IndexBuffer");
	case EBufferUsageFlags::StructuredBuffer:
		return TEXT("StructuredBuffer");
	}
	return TEXT("");
}

inline const TCHAR* GetUniformBufferBaseTypeString(EUniformBufferBaseType BaseType)
{
	switch (BaseType)
	{
	case UBMT_INVALID:
		return TEXT("UBMT_INVALID");
	case UBMT_BOOL:
		return TEXT("UBMT_BOOL");
	case UBMT_INT32:
		return TEXT("UBMT_INT32");
	case UBMT_UINT32:
		return TEXT("UBMT_UINT32");
	case UBMT_FLOAT32:
		return TEXT("UBMT_FLOAT32");
	case UBMT_TEXTURE:
		return TEXT("UBMT_TEXTURE");
	case UBMT_SRV:
		return TEXT("UBMT_SRV");
	case UBMT_UAV:
		return TEXT("UBMT_UAV");
	case UBMT_SAMPLER:
		return TEXT("UBMT_SAMPLER");
	case UBMT_RDG_TEXTURE:
		return TEXT("UBMT_RDG_TEXTURE");
	case UBMT_RDG_TEXTURE_ACCESS:
		return TEXT("UBMT_RDG_TEXTURE_ACCESS");
	case UBMT_RDG_TEXTURE_ACCESS_ARRAY:
		return TEXT("UBMT_RDG_TEXTURE_ACCESS_ARRAY");
	case UBMT_RDG_TEXTURE_SRV:
		return TEXT("UBMT_RDG_TEXTURE_SRV");
	case UBMT_RDG_TEXTURE_UAV:
		return TEXT("UBMT_RDG_TEXTURE_UAV");
	case UBMT_RDG_BUFFER_ACCESS:
		return TEXT("UBMT_RDG_BUFFER_ACCESS");
	case UBMT_RDG_BUFFER_ACCESS_ARRAY:
		return TEXT("UBMT_RDG_BUFFER_ACCESS_ARRAY");
	case UBMT_RDG_BUFFER_SRV:
		return TEXT("UBMT_RDG_BUFFER_SRV");
	case UBMT_RDG_BUFFER_UAV:
		return TEXT("UBMT_RDG_BUFFER_UAV");
	case UBMT_RDG_UNIFORM_BUFFER:
		return TEXT("UBMT_RDG_UNIFORM_BUFFER");
	case UBMT_NESTED_STRUCT:
		return TEXT("UBMT_NESTED_STRUCT");
	case UBMT_INCLUDED_STRUCT:
		return TEXT("UBMT_INCLUDED_STRUCT");
	case UBMT_REFERENCED_STRUCT:
		return TEXT("UBMT_REFERENCED_STRUCT");
	case UBMT_RENDER_TARGET_BINDING_SLOTS:
		return TEXT("UBMT_RENDER_TARGET_BINDING_SLOTS");
	}
	return TEXT("");
}


inline bool IsGeometryPipelineShaderFrequency(EShaderFrequency Frequency)
{
	return Frequency == SF_Mesh || Frequency == SF_Amplification;
}

inline bool IsRayTracingShaderFrequency(EShaderFrequency Frequency)
{
	switch (Frequency)
	{
	case SF_RayGen:
	case SF_RayMiss:
	case SF_RayHitGroup:
	case SF_RayCallable:
		return true;
	default:
		return false;
	}
}

inline ERHIResourceType GetRHIResourceType(ETextureDimension Dimension)
{
	switch (Dimension)
	{
	case ETextureDimension::Texture2D:
		return ERHIResourceType::RRT_Texture2D;
	case ETextureDimension::Texture2DArray:
		return ERHIResourceType::RRT_Texture2DArray;
	case ETextureDimension::Texture3D:
		return ERHIResourceType::RRT_Texture3D;
	case ETextureDimension::TextureCube:
	case ETextureDimension::TextureCubeArray:
		return ERHIResourceType::RRT_TextureCube;
	}
	checkNoEntry();
	return ERHIResourceType::RRT_None;
}

enum class ERHIBindlessConfiguration
{
	Disabled,
	AllShaders,
	RayTracingShaders,
};

RHI_API ERHIBindlessConfiguration RHIGetBindlessResourcesConfiguration(EShaderPlatform Platform);
RHI_API ERHIBindlessConfiguration RHIGetBindlessSamplersConfiguration(EShaderPlatform Platform);

#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
	#define GEOMETRY_SHADER(GeometryShader)	(GeometryShader)
#else
	#define GEOMETRY_SHADER(GeometryShader)	nullptr
#endif
