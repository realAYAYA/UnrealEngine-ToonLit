// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RHIDefinitions.h"
#include "RHIShaderPlatform.h"
#include "RHIFeatureLevel.h"
#include "UObject/NameTypes.h"

class FText;

extern RHI_API const FName LANGUAGE_D3D;
extern RHI_API const FName LANGUAGE_Metal;
extern RHI_API const FName LANGUAGE_OpenGL;
extern RHI_API const FName LANGUAGE_Vulkan;
extern RHI_API const FName LANGUAGE_Sony;
extern RHI_API const FName LANGUAGE_Nintendo;

class FGenericDataDrivenShaderPlatformInfo
{
	FName Name;
	FName Language;
	ERHIFeatureLevel::Type MaxFeatureLevel;
	FName ShaderFormat;
	uint32 ShaderPropertiesHash;
	uint32 bIsMobile : 1;
	uint32 bIsMetalMRT : 1;
	uint32 bIsPC : 1;
	uint32 bIsConsole : 1;
	uint32 bIsAndroidOpenGLES : 1;

	uint32 bSupportsDebugViewShaders : 1;
	uint32 bSupportsMobileMultiView : 1;
	uint32 bSupportsArrayTextureCompression : 1;
	uint32 bSupportsDistanceFields : 1; // used for DFShadows and DFAO - since they had the same checks
	uint32 bSupportsDiaphragmDOF : 1;
	uint32 bSupportsRGBColorBuffer : 1;
	uint32 bSupportsCapsuleShadows : 1;
	uint32 bSupportsPercentageCloserShadows : 1;
	uint32 bSupportsIndexBufferUAVs : 1;
	uint32 bSupportsInstancedStereo : 1;
	uint32 SupportsMultiViewport : int32(ERHIFeatureSupport::NumBits);
	uint32 bSupportsMSAA : 1;
	uint32 bSupports4ComponentUAVReadWrite : 1;
	uint32 bSupportsShaderRootConstants : 1;
	uint32 bSupportsShaderBundleDispatch : 1;
	uint32 bSupportsRenderTargetWriteMask : 1;
	uint32 bSupportsRayTracing : 1;
	uint32 bSupportsRayTracingCallableShaders : 1;
	uint32 bSupportsRayTracingProceduralPrimitive : 1;
	uint32 bSupportsRayTracingTraversalStatistics : 1;
	uint32 bSupportsRayTracingIndirectInstanceData : 1; // Whether instance transforms can be copied from the GPU to the TLAS instances buffer
	uint32 bSupportsHighEndRayTracingEffects : 1; // Whether fully-featured RT effects can be used on the platform (with translucent shadow, etc.)
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
	uint32 bSupportsWavePermute : 1;
	uint32 MinimumWaveSize : 8;
	uint32 MaximumWaveSize : 8;
	uint32 bSupportsIntrinsicWaveOnce : 1;
	uint32 bSupportsConservativeRasterization : 1;
	uint32 bRequiresExplicit128bitRT : 1;
	uint32 bSupportsGen5TemporalAA : 1;
	uint32 bTargetsTiledGPU : 1;
	uint32 bNeedsOfflineCompiler : 1;
	uint32 bSupportsComputeFramework : 1;
	uint32 bSupportsAnisotropicMaterials : 1;
	uint32 bSupportsDualSourceBlending : 1;
	uint32 bRequiresGeneratePrevTransformBuffer : 1;
	uint32 bRequiresRenderTargetDuringRaster : 1;
	uint32 bRequiresDisableForwardLocalLights : 1;
	uint32 bCompileSignalProcessingPipeline : 1;
	uint32 bSupportsMeshShadersTier0 : 1;
	uint32 bSupportsMeshShadersTier1 : 1;
	uint32 bSupportsMeshShadersWithClipDistance : 1;
	uint32 MaxMeshShaderThreadGroupSize : 10;
	uint32 bRequiresUnwrappedMeshShaderArgs : 1;
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
	uint32 bSupportsVertexShaderSRVs : 1; // Whether SRVs can be bound to vertex shaders (may be independent from ManualVertexFetch)
	uint32 bSupportsManualVertexFetch : 1;
	uint32 bRequiresReverseCullingOnMobile : 1;
	uint32 bOverrideFMaterial_NeedsGBufferEnabled : 1;
	uint32 bSupportsFFTBloom : 1;
	uint32 bSupportsInlineRayTracing : 1;
	uint32 bSupportsRayTracingShaders : 1;
	uint32 bSupportsVertexShaderLayer : 1;
	uint32 BindlessSupport : int32(ERHIBindlessSupport::NumBits);
	uint32 bSupportsVolumeTextureAtomics : 1;
	uint32 bSupportsROV : 1;
	uint32 bSupportsOIT : 1;
	uint32 bSupportsRealTypes : int32(ERHIFeatureSupport::NumBits);
	uint32 EnablesHLSL2021ByDefault : 2; // 0: disabled, 1: global shaders only, 2: all shaders
	uint32 bSupportsSceneDataCompressedTransforms : 1;
	uint32 bIsPreviewPlatform : 1;
	uint32 bSupportsSwapchainUAVs : 1;
	uint32 bSupportsClipDistance : 1;
	uint32 bSupportsNNEShaders: 1;
	uint32 bSupportsShaderPipelines : 1;
	uint32 bSupportsUniformBufferObjects : 1;
	uint32 bRequiresBindfulUtilityShaders : 1;
	uint32 MaxSamplers : 8;
	uint32 SupportsBarycentricsIntrinsics : 1;
	uint32 SupportsBarycentricsSemantic : int32(ERHIFeatureSupport::NumBits);
	uint32 bSupportsWave64 : 1;

	// NOTE: When adding fields, you must also add to ParseDataDrivenShaderInfo!
	uint32 bContainsValidPlatformInfo : 1;

	FGenericDataDrivenShaderPlatformInfo()
	{
		FMemory::Memzero(this, sizeof(*this));

		SetDefaultValues();
	}

	RHI_API void SetDefaultValues();

public:
	RHI_API static void Initialize();
	RHI_API static const EShaderPlatform GetShaderPlatformFromName(const FName ShaderPlatformName);

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

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsShaderRootConstants(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsShaderRootConstants;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsShaderBundleDispatch(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsShaderBundleDispatch;
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

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsHighEndRayTracingEffects(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsRayTracing && Infos[Platform].bSupportsHighEndRayTracingEffects;
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

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsWavePermute(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsWavePermute;
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

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsMeshShadersWithClipDistance(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsMeshShadersWithClipDistance;
	}

	static FORCEINLINE_DEBUGGABLE const uint32 GetMaxMeshShaderThreadGroupSize(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].MaxMeshShaderThreadGroupSize;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetRequiresUnwrappedMeshShaderArgs(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bRequiresUnwrappedMeshShaderArgs;
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

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsVertexShaderSRVs(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsVertexShaderSRVs;
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

	UE_DEPRECATED(5.3, "This function is no longer in use and will be removed. Please use GetSupportsDistanceFields instead")
	static FORCEINLINE_DEBUGGABLE const bool GetSupportsMobileDistanceField(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return false;
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

	static FORCEINLINE_DEBUGGABLE const ERHIBindlessSupport GetBindlessSupport(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return static_cast<ERHIBindlessSupport>(Infos[Platform].BindlessSupport);
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsVolumeTextureAtomics(const FStaticShaderPlatform Platform)
	{
		return Infos[Platform].bSupportsVolumeTextureAtomics;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsPipelineShaders(const FStaticShaderPlatform Platform)
	{
		return Infos[Platform].bSupportsShaderPipelines;
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
	
	static FORCEINLINE_DEBUGGABLE const bool GetSupportsClipDistance(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsClipDistance;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsNNEShaders(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsNNEShaders;
	}
	
	static FORCEINLINE_DEBUGGABLE const bool GetSupportsUniformBufferObjects(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsUniformBufferObjects;
	}
	
	static FORCEINLINE_DEBUGGABLE const bool GetRequiresBindfulUtilityShaders(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bRequiresBindfulUtilityShaders;
	}
	
	static FORCEINLINE_DEBUGGABLE const uint32 GetMaxSamplers(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].MaxSamplers;
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsBarycentricsIntrinsics(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].SupportsBarycentricsIntrinsics;
	}

	static FORCEINLINE_DEBUGGABLE const ERHIFeatureSupport GetSupportsBarycentricsSemantic(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return ERHIFeatureSupport(Infos[Platform].SupportsBarycentricsSemantic);
	}

	static FORCEINLINE_DEBUGGABLE const bool GetSupportsWave64(const FStaticShaderPlatform Platform)
	{
		check(IsValid(Platform));
		return Infos[Platform].bSupportsWave64;
	}

	static FORCEINLINE_DEBUGGABLE const bool IsValid(const FStaticShaderPlatform Platform)
	{
		return Infos[Platform].bContainsValidPlatformInfo;
	}

#if WITH_EDITOR
	RHI_API static void UpdatePreviewPlatforms();
	RHI_API static FText GetFriendlyName(const FStaticShaderPlatform Platform);
	RHI_API static const EShaderPlatform GetPreviewShaderPlatformParent(const FStaticShaderPlatform Platform);

	RHI_API static TMap<FString, TFunction<bool(const FStaticShaderPlatform Platform)>> PropertyToShaderPlatformFunctionMap;
#endif

private:
	RHI_API static void ParseDataDrivenShaderInfo(const FConfigSection& Section, uint32 Index);

	RHI_API static FGenericDataDrivenShaderPlatformInfo Infos[SP_NumPlatforms];
};

#if USE_STATIC_SHADER_PLATFORM_ENUMS || USE_STATIC_FEATURE_LEVEL_ENUMS || USE_STATIC_SHADER_PLATFORM_INFO

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

inline bool IsPCPlatform(const FStaticShaderPlatform Platform)
{
	return FDataDrivenShaderPlatformInfo::GetIsPC(Platform);
}

/** Whether the shader platform corresponds to the ES3.1/Metal/Vulkan feature level. */
inline bool IsMobilePlatform(const FStaticShaderPlatform Platform)
{
	return FDataDrivenShaderPlatformInfo::GetMaxFeatureLevel(Platform) == ERHIFeatureLevel::ES3_1;
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

inline bool IsMetalSM6Platform(const FStaticShaderPlatform Platform)
{
    return FDataDrivenShaderPlatformInfo::GetIsLanguageMetal(Platform)
        && FDataDrivenShaderPlatformInfo::GetMaxFeatureLevel(Platform) == ERHIFeatureLevel::SM6;
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

UE_DEPRECATED(5.3, "Use a combination of IsVulkanPlatform and IsFeatureLevelSupported (or GetMaxFeatureLevel) instead.")
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

#if WITH_EDITOR
inline EShaderPlatform GetSimulatedPlatform(FStaticShaderPlatform Platform)
{
	if (IsSimulatedPlatform(Platform))
	{
		return FDataDrivenShaderPlatformInfo::GetPreviewShaderPlatformParent(Platform);
	}

	return Platform;
}
#endif // WITH_EDITOR

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
	return FDataDrivenShaderPlatformInfo::GetSupportsPipelineShaders(Platform);
}

inline bool RHISupportsDualSourceBlending(const FStaticShaderPlatform Platform)
{
	// Check if the platform supports dual src blending from DDPI
	return FDataDrivenShaderPlatformInfo::GetSupportsDualSourceBlending(Platform) && !FDataDrivenShaderPlatformInfo::GetIsHlslcc(Platform);
}

// helper to check that the shader platform supports creating a UAV off an index buffer.
inline bool RHISupportsIndexBufferUAVs(const FStaticShaderPlatform Platform)
{
	return FDataDrivenShaderPlatformInfo::GetSupportsIndexBufferUAVs(Platform);
}



inline bool RHISupportsInstancedStereo(const FStaticShaderPlatform Platform)
{
	return FDataDrivenShaderPlatformInfo::GetSupportsInstancedStereo(Platform);
}

/**
 * Can this platform implement instanced stereo rendering by rendering to multiple viewports.
 * Note: run-time users should always check GRHISupportsArrayIndexFromAnyShader as well, since for some SPs (particularly PCD3D_SM5) minspec does not guarantee that feature.
 **/
inline bool RHISupportsMultiViewport(const FStaticShaderPlatform Platform)
{
	return FDataDrivenShaderPlatformInfo::GetSupportsMultiViewport(Platform) != ERHIFeatureSupport::Unsupported;
}

inline bool RHISupportsMSAA(const FStaticShaderPlatform Platform)
{
	// @todo platplug: Maybe this should become bDisallowMSAA to default of 0 is a better default (since now MSAA is opt-out more than opt-in) 
	return FDataDrivenShaderPlatformInfo::GetSupportsMSAA(Platform);
}

inline bool RHISupportsBufferLoadTypeConversion(const FStaticShaderPlatform Platform)
{
	return !IsMetalPlatform(Platform) && !IsOpenGLPlatform(Platform);
}

/** Whether the platform supports reading from volume textures (does not cover rendering to volume textures). */
inline bool RHISupportsVolumeTextures(const FStaticFeatureLevel FeatureLevel)
{
	return FeatureLevel >= ERHIFeatureLevel::SM5;
}

inline bool RHISupportsVertexShaderLayer(const FStaticShaderPlatform Platform)
{
	return FDataDrivenShaderPlatformInfo::GetSupportsVertexShaderLayer(Platform);
}

/** Return true if and only if the GPU support rendering to volume textures (2D Array, 3D) is guaranteed supported for a target platform.
	if PipelineVolumeTextureLUTSupportGuaranteedAtRuntime is true then it is guaranteed that GSupportsVolumeTextureRendering is true at runtime.
*/
inline bool RHIVolumeTextureRenderingSupportGuaranteed(const FStaticShaderPlatform Platform)
{
	return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5)
		&& (!IsMetalPlatform(Platform) || RHISupportsVertexShaderLayer(Platform)) // For Metal only shader platforms & versions that support vertex-shader-layer can render to volume textures - this is a compile/cook time check.
		&& !IsOpenGLPlatform(Platform);		// Apparently, some OpenGL 3.3 cards support SM4 but can't render to volume textures
}

inline bool RHISupports4ComponentUAVReadWrite(const FStaticShaderPlatform Platform)
{
	// Must match usf PLATFORM_SUPPORTS_4COMPONENT_UAV_READ_WRITE
	// D3D11 does not support multi-component loads from a UAV: "error X3676: typed UAV loads are only allowed for single-component 32-bit element types"
	return FDataDrivenShaderPlatformInfo::GetSupports4ComponentUAVReadWrite(Platform);
}

/** Whether Manual Vertex Fetch is supported for the specified shader platform.
	Shader Platform must not use the mobile renderer, and for Metal, the shader language must be at least 2. */
inline bool RHISupportsManualVertexFetch(const FStaticShaderPlatform InShaderPlatform)
{
	return FDataDrivenShaderPlatformInfo::GetSupportsManualVertexFetch(InShaderPlatform);
}

inline bool RHISupportsSwapchainUAVs(const FStaticShaderPlatform Platform)
{
	return FDataDrivenShaderPlatformInfo::GetSupportsSwapchainUAVs(Platform);
}

/**
 * Returns true if SV_VertexID contains BaseVertexIndex passed to the draw call, false if shaders must manually construct an absolute VertexID.
 */
inline bool RHISupportsAbsoluteVertexID(const FStaticShaderPlatform InShaderPlatform)
{
	return IsVulkanPlatform(InShaderPlatform) || IsVulkanMobilePlatform(InShaderPlatform);
}

/** Whether this platform can build acceleration structures and use full ray tracing pipelines or inline ray tracing (ray queries).
 *  To use at runtime, also check GRHISupportsRayTracing and r.RayTracing CVar (see IsRayTracingEnabled() helper).
 *  Check GRHISupportsRayTracingShaders before using full ray tracing pipeline state objects.
 *  Check GRHISupportsInlineRayTracing before using inline ray tracing features in compute and other shaders.
 **/
inline bool RHISupportsRayTracing(const FStaticShaderPlatform Platform)
{
	return FDataDrivenShaderPlatformInfo::GetSupportsRayTracing(Platform);
}

/** Whether this platform can compile ray tracing shaders (regardless of project settings).
 *  To use at runtime, also check GRHISupportsRayTracing and r.RayTracing CVar (see IsRayTracingEnabled() helper).
 **/
inline bool RHISupportsRayTracingShaders(const FStaticShaderPlatform Platform)
{
	return FDataDrivenShaderPlatformInfo::GetSupportsRayTracingShaders(Platform);
}

/** Whether this platform can compile shaders with inline ray tracing features.
 *  To use at runtime, also check GRHISupportsRayTracing and r.RayTracing CVar (see IsRayTracingEnabled() helper).
 **/
inline bool RHISupportsInlineRayTracing(const FStaticShaderPlatform Platform)
{
	return FDataDrivenShaderPlatformInfo::GetSupportsInlineRayTracing(Platform);
}

/** Whether this platform can compile ray tracing callable shaders.
 *  To use at runtime, also check GRHISupportsRayTracing and r.RayTracing CVar (see IsRayTracingEnabled() helper).
 **/
inline bool RHISupportsRayTracingCallableShaders(const FStaticShaderPlatform Platform)
{
	return FDataDrivenShaderPlatformInfo::GetSupportsRayTracingCallableShaders(Platform);
}

/** Can this platform compile mesh shaders with tier0 capability.
 *  To use at runtime, also check GRHISupportsMeshShadersTier0.
 **/
inline bool RHISupportsMeshShadersTier0(const FStaticShaderPlatform Platform)
{
	return FDataDrivenShaderPlatformInfo::GetSupportsMeshShadersTier0(Platform);
}

/** Can this platform compile mesh shaders with tier1 capability.
 *  To use at runtime, also check GRHISupportsMeshShadersTier1.
 **/
inline bool RHISupportsMeshShadersTier1(const FStaticShaderPlatform Platform)
{
	return FDataDrivenShaderPlatformInfo::GetSupportsMeshShadersTier1(Platform);
}

inline uint32 RHIMaxMeshShaderThreadGroupSize(const FStaticShaderPlatform Platform)
{
	return FDataDrivenShaderPlatformInfo::GetMaxMeshShaderThreadGroupSize(Platform);
}

/** Can this platform compile shaders that use shader model 6.0 wave intrinsics.
 *  To use such shaders at runtime, also check GRHISupportsWaveOperations.
 **/
inline bool RHISupportsWaveOperations(const FStaticShaderPlatform Platform)
{
	return FDataDrivenShaderPlatformInfo::GetSupportsWaveOperations(Platform) != ERHIFeatureSupport::Unsupported;
}

/** True if the given shader platform supports shader root constants */
inline bool RHISupportsShaderRootConstants(const FStaticShaderPlatform Platform)
{
	return FDataDrivenShaderPlatformInfo::GetSupportsShaderRootConstants(Platform);
}

/** True if the given shader platform supports shader bundle dispatch */
inline bool RHISupportsShaderBundleDispatch(const FStaticShaderPlatform Platform)
{
	return FDataDrivenShaderPlatformInfo::GetSupportsShaderBundleDispatch(Platform);
}

/** True if the given shader platform supports a render target write mask */
inline bool RHISupportsRenderTargetWriteMask(const FStaticShaderPlatform Platform)
{
	return FDataDrivenShaderPlatformInfo::GetSupportsRenderTargetWriteMask(Platform);
}

/** True if the given shader platform supports overestimated conservative rasterization */
inline bool RHISupportsConservativeRasterization(const FStaticShaderPlatform Platform)
{
	return FDataDrivenShaderPlatformInfo::GetSupportsConservativeRasterization(Platform);
}

/** True if the given shader platform supports bindless resources/views. */
inline ERHIBindlessSupport RHIGetBindlessSupport(const FStaticShaderPlatform Platform)
{
	return FDataDrivenShaderPlatformInfo::GetBindlessSupport(Platform);
}

inline bool RHISupportsVolumeTextureAtomics(EShaderPlatform Platform)
{
	return FDataDrivenShaderPlatformInfo::GetSupportsVolumeTextureAtomics(Platform);
}

/** True if the platform supports wave size of 64 */
inline bool RHISupportsWaveSize64(const FStaticShaderPlatform Platform)
{
	return FDataDrivenShaderPlatformInfo::GetSupportsWave64(Platform);
}

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_3
#include "Internationalization/Text.h"
#endif
