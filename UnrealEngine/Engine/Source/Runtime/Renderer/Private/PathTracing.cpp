// Copyright Epic Games, Inc. All Rights Reserved.

#include "PathTracing.h"
#include "HAL/IConsoleManager.h"
#include "RHI.h"
#include "PathTracingDenoiser.h"

TAutoConsoleVariable<int32> CVarPathTracing(
	TEXT("r.PathTracing"),
	1,
	TEXT("Enables the path tracing renderer (to guard the compilation of path tracer specific material permutations)"),
	ECVF_RenderThreadSafe | ECVF_ReadOnly
);

#if RHI_RAYTRACING

#include "BasePassRendering.h"
#include "RendererPrivate.h"
#include "GlobalShader.h"
#include "DeferredShadingRenderer.h"
#include "HAL/PlatformApplicationMisc.h"
#include "RayTracingTypes.h"
#include "RayTracingDefinitions.h"
#include "RayTracingPayloadType.h"
#include "PathTracingDefinitions.h"
#include "RayTracing/RayTracingMaterialHitShaders.h"
#include "RayTracing/RayTracingDecals.h"
#include "DecalRenderingCommon.h"
#include "FogRendering.h"
#include "GenerateMips.h"
#include "HairStrands/HairStrandsData.h"
#include "HeterogeneousVolumes/HeterogeneousVolumes.h"
#include "Modules/ModuleManager.h"
#include "SkyAtmosphereRendering.h"
#include <limits>
#include "PathTracingSpatialTemporalDenoising.h"
#include "PostProcess/DiaphragmDOF.h"
#include "EnvironmentComponentsFlags.h"

TAutoConsoleVariable<int32> CVarPathTracingExperimental(
	TEXT("r.PathTracing.Experimental"),
	0,
	TEXT("Enables some experimental features of the path tracing renderer that require compiling additional permutations of the path tracer."),
	ECVF_RenderThreadSafe | ECVF_ReadOnly
);


TAutoConsoleVariable<int32> CVarPathTracingCompaction(
	TEXT("r.PathTracing.Compaction"),
	1,
	TEXT("Enables path compaction to improve GPU occupancy for the path tracer (default: 1 (enabled))"),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarPathTracingIndirectDispatch(
	TEXT("r.PathTracing.IndirectDispatch"),
	0,
	TEXT("Enables indirect dispatch (if supported by the hardware) for compacted path tracing (default: 0 (disabled))"),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarPathTracingFlushDispatch(
	TEXT("r.PathTracing.FlushDispatch"),
	2,
	TEXT("Enables flushing of the command list after dispatch to reduce the likelyhood of TDRs on Windows (default: 2)\n")
	TEXT("0: off\n")
	TEXT("1: flush after each dispatch\n")
	TEXT("2: flush after each tile\n"),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarPathTracingDispatchSize(
	TEXT("r.PathTracing.DispatchSize"),
	2048,
	TEXT("Controls the tile size used when rendering the image. Reducing this value may prevent GPU timeouts for heavy renders. (default = 2048)"),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarPathTracingMaxBounces(
	TEXT("r.PathTracing.MaxBounces"),
	-1,
	TEXT("Sets the maximum number of path tracing bounces (default = -1 (driven by postprocesing volume))"),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarPathTracingSamplesPerPixel(
	TEXT("r.PathTracing.SamplesPerPixel"),
	-1,
	TEXT("Sets the maximum number of samples per pixel (default = -1 (driven by postprocesing volume))"),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<float> CVarPathTracingFilterWidth(
	TEXT("r.PathTracing.FilterWidth"),
	3.0,
	TEXT("Sets the anti-aliasing filter width (default = 3.0 which corresponds to a gaussian with standard deviation of a 1/2 pixel)"),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarPathTracingMISMode(
	TEXT("r.PathTracing.MISMode"),
	2,
	TEXT("Selects the sampling technique for light integration (default = 2 (MIS enabled))\n")
	TEXT("0: Material sampling\n")
	TEXT("1: Light sampling\n")
	TEXT("2: MIS betwen material and light sampling (default)\n"),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarPathTracingVolumeMISMode(
	TEXT("r.PathTracing.VolumeMISMode"),
	1,
	TEXT("Selects the sampling technique for volumetric integration of local lighting (default = 1)\n")
	TEXT("0: Density sampling\n")
	TEXT("1: Light sampling (default)\n"),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarPathTracingMaxRaymarchSteps(
	TEXT("r.PathTracing.MaxRaymarchSteps"),
	256,
	TEXT("Upper limit on the number of ray marching steps in volumes. This limit should not be hit in most cases, but raising it can reduce bias in case it is. (default = 256)."),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarPathTracingMISCompensation(
	TEXT("r.PathTracing.MISCompensation"),
	1,
	TEXT("Activates MIS compensation for skylight importance sampling. (default = 1 (enabled))\n")
	TEXT("This option only takes effect when r.PathTracing.MISMode = 2\n"),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarPathTracingSkylightCaching(
	TEXT("r.PathTracing.SkylightCaching"),
	1,
	TEXT("Attempts to re-use skylight data between frames. (default = 1 (enabled))\n")
	TEXT("When set to 0, the skylight texture and importance samping data will be regenerated every frame. This is mainly intended as a benchmarking and debugging aid\n"),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarPathTracingVisibleLights(
	TEXT("r.PathTracing.VisibleLights"),
	0,
	TEXT("Should light sources be visible to camera rays? (default = 0 (off))\n")
	TEXT("0: Hide lights from camera rays (default)\n")
	TEXT("1: Make all lights visible to camera\n")
	TEXT("2: Make skydome only visible to camera\n"),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarPathTracingMaxSSSBounces(
	TEXT("r.PathTracing.MaxSSSBounces"),
	256,
	TEXT("Sets the maximum number of bounces inside subsurface materials. Lowering this value can make subsurface scattering render too dim, while setting it too high can cause long render times.  (default = 256)"),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<float> CVarPathTracingSSSGuidingRatio(
	TEXT("r.PathTracing.SSSGuidingRatio"),
	0.5f,
	TEXT("Sets the ratio between classical random walks and walks guided towards the surface. A value of 0.0 corresponds to a purely classical random walk, while a value of 1.0 is fully guided towards the surface (at the expense of fireflies in non-flat regions of the model. (default = 0.5)"),
	ECVF_RenderThreadSafe
);


TAutoConsoleVariable<float> CVarPathTracingMaxPathIntensity(
	TEXT("r.PathTracing.MaxPathIntensity"),
	-1,
	TEXT("When positive, light paths greater that this amount are clamped to prevent fireflies (default = -1 (driven by postprocesing volume))"),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarPathTracingApproximateCaustics(
	TEXT("r.PathTracing.ApproximateCaustics"),
	1,
	TEXT("When non-zero, the path tracer will approximate caustic paths to reduce noise. This reduces speckles and noise from low-roughness glass and metals. (default = 1 (enabled))"),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarPathTracingEnableEmissive(
	TEXT("r.PathTracing.EnableEmissive"),
	-1,
	TEXT("Indicates if emissive materials should contribute to scene lighting (default = -1 (driven by postprocesing volume)"),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarPathTracingEnableCameraBackfaceCulling(
	TEXT("r.PathTracing.EnableCameraBackfaceCulling"),
	1,
	TEXT("When non-zero, the path tracer will skip over backfacing triangles when tracing primary rays from the camera. (default = 1 (enabled))"),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarPathTracingAtmosphereOpticalDepthLutResolution(
	TEXT("r.PathTracing.AtmosphereOpticalDepthLUTResolution"),
	512,
	TEXT("Size of the square lookup texture used for transmittance calculations by the path tracer in reference atmosphere mode.  (default = 512)"),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarPathTracingAtmosphereOpticalDepthLutNumSamples(
	TEXT("r.PathTracing.AtmosphereOpticalDepthLUTNumSamples"),
	16384,
	TEXT("Number of ray marching samples used when building the transmittance lookup texture used for transmittance calculations by the path tracer in reference atmosphere mode.  (default = 16384)"),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarPathTracingFrameIndependentTemporalSeed(
	TEXT("r.PathTracing.FrameIndependentTemporalSeed"),
	1,
	TEXT("Indicates to use different temporal seed for each sample across frames rather than resetting the sequence at the start of each frame\n")
	TEXT("0: off\n")
	TEXT("1: on (default)\n"),
	ECVF_RenderThreadSafe
);

// See PATHTRACER_SAMPLER_* defines
TAutoConsoleVariable<int32> CVarPathTracingSamplerType(
	TEXT("r.PathTracing.SamplerType"),
	PATHTRACER_SAMPLER_DEFAULT,
	TEXT("Controls the way the path tracer generates its random numbers\n")
	TEXT("0: use a different high quality random sequence per pixel (default)\n")
	TEXT("1: optimize the random sequence across pixels to reduce visible error at the target sample count\n"),
	ECVF_RenderThreadSafe
);

#if WITH_MGPU
TAutoConsoleVariable<int32> CVarPathTracingMultiGPU(
	TEXT("r.PathTracing.MultiGPU"),
	0,
	TEXT("Run the path tracer using all available GPUs when enabled (default = 0)\n")
	TEXT("Using this functionality in the editor requires -MaxGPUCount=N setting on the command line"),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<bool> CVarPathTracingAdjustMultiGPUPasses(
	TEXT("r.PathTracing.AdjustMultiGPUPasses"),
	true,
	TEXT("Run extra passes per frame when multiple GPUs are active, to improve perf scaling as GPUs are added (default = true)\n"),
	ECVF_RenderThreadSafe
);
#endif  // WITH_MGPU

TAutoConsoleVariable<int32> CVarPathTracingWiperMode(
	TEXT("r.PathTracing.WiperMode"),
	0,
	TEXT("Enables wiper mode to render using the path tracer only in a region of the screen for debugging purposes (default = 0, wiper mode disabled)"),
	ECVF_RenderThreadSafe 
);

TAutoConsoleVariable<int32> CVarPathTracingProgressDisplay(
	TEXT("r.PathTracing.ProgressDisplay"),
	1,
	TEXT("Enables an in-frame display of progress towards the defined sample per pixel limit. The indicator dissapears when the maximum is reached and sample accumulation has stopped (default = 1)\n")
	TEXT("0: off\n")
	TEXT("1: on (default)\n"),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarPathTracingLightGridResolution(
	TEXT("r.PathTracing.LightGridResolution"),
	256,
	TEXT("Controls the resolution of the 2D light grid used to cull irrelevant lights from lighting calculations (default = 256)\n"),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarPathTracingLightGridMaxCount(
	TEXT("r.PathTracing.LightGridMaxCount"),
	128,
	TEXT("Controls the maximum number of lights per cell in the 2D light grid. The minimum of this value and the number of lights in the scene is used. (default = 128)\n"),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarPathTracingLightGridVisualize(
	TEXT("r.PathTracing.LightGridVisualize"),
	0,
	TEXT("Enables a visualization mode of the light grid density where red indicates the maximum light count has been reached (default = 0)\n")
	TEXT("0: off (default)\n")
	TEXT("1: light count heatmap (red - close to overflow, increase r.PathTracing.LightGridMaxCount)\n")
	TEXT("2: unique light lists (colors are a function of which lights occupy each cell)\n")
	TEXT("3: area light visualization (green: point light sources only, blue: some area light sources)\n"),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarPathTracingDecalGridVisualize(
	TEXT("r.PathTracing.DecalGrid.Visualize"),
	0,
	TEXT("Enables a visualization mode of the decal grid density where red indicates the maximum decal count has been reached (default = 0)\n")
	TEXT("0: off (default)\n")
	TEXT("1: decal count heatmap (red - close to overflow, increase r.RayTracing.DecalGrid.MaxCount)\n")
	TEXT("2: unique decal lists (colors are a function of which decals occupy each cell)\n"),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarPathTracingUseDBuffer(
	TEXT("r.PathTracing.UseDBuffer"),
	1,
	TEXT("Whether to support DBuffer functionality (default=1)"),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<float> CVarPathTracingDecalRoughnessCutoff(
	TEXT("r.PathTracing.DecalRoughnessCutoff"),
	0.15f,
	TEXT("Do not evaluate decals beyond this roughness level to improve performance (default=0.15)"),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<float> CVarPathTracingMeshDecalRoughnessCutoff(
	TEXT("r.PathTracing.MeshDecalRoughnessCutoff"),
	0.15f,
	TEXT("Do not evaluate mesh decals beyond this roughness level to improve performance (default=0.15)"),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<float> CVarPathTracingMeshDecalBias(
	TEXT("r.PathTracing.MeshDecalBias"),
	1.0f,
	TEXT("Bias applied to mesh decal rays to avoid intersection with geometry (default = 1.0f)"),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarPathTracingLightFunctionColor(
	TEXT("r.PathTracing.LightFunctionColor"),
	0,
	TEXT("Enables light functions to be colored instead of greyscale (default = 0)\n")
	TEXT("0: off (default)\n")
	TEXT("1: on (light function material output is used directly instead of converting to greyscale)\n"),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarPathTracingHeterogeneousVolumesRebuildEveryFrame(
	TEXT("r.PathTracing.HeterogeneousVolumes.RebuildEveryFrame"),
	1,
	TEXT("Rebuilds volumetric acceleration structures every frame (default = 1)\n"),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarPathTracingCameraMediumTracking(
	TEXT("r.PathTracing.CameraMediumTracking"),
	1,
	TEXT("Enables automatic camera medium tracking to detect when a camera starts inside water or solid glass automatically (default = 1)\n")
	TEXT("0: off\n")
	TEXT("1: on (default)\n"),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarPathTracingOutputPostProcessResources(
	TEXT("r.PathTracing.OutputPostProcessResources"),
	1,
	TEXT("Output the pathtracing resources to the postprocess passes\n")
	TEXT("0: off\n")
	TEXT("1: on (Buffers including, raw/denoised radiance, albedo, normal, and variance)\n"),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarPathTracingSubstrateUseSimplifiedMaterial(
	TEXT("r.PathTracing.Substrate.UseSimplifiedMaterials"),
	0,
	TEXT("Instead of evaluating all layers, use an optimized material in which all slabs have been merged. This is mainly intended for debugging and requires r.PathTracing.Substrate.CompileSimplifiedMaterials to be true.\n")
	TEXT("0: off (default)\n")
	TEXT("1: on\n"),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarPathTracingSubstrateCompileSimplifiedMaterial(
	TEXT("r.PathTracing.Substrate.CompileSimplifiedMaterials"),
	0,
	TEXT("Compile a simplified representation of Substrate materials which merges all slabs into one. This is mainly intended for debugging purposes. Enabling this double the number of path tracing shader permutations.\n")
	TEXT("0: off (default)\n")
	TEXT("1: on\n"),
	ECVF_RenderThreadSafe | ECVF_ReadOnly
);

TAutoConsoleVariable<int32> CVarpathTracingOverrideDepth(
	TEXT("r.PathTracing.Override.Depth"),
	1,
	TEXT("Override the scene depth z by the path tracing depth z")
	TEXT("0: off\n")
	TEXT("1: On (Default, translucent materials have better DOF with the post-process DOF.)\n"),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarPathTracingUseAnalyticTransmittance(
	TEXT("r.PathTracing.UseAnalyticTransmittance"),
	-1,
	TEXT("Determines use of analytical or null-tracking estimation when evaluating transmittance\n")
	TEXT("-1: uses null-tracking estimation if heterogeneous volumes are present, or analytical estimation otherwise (default)\n")
	TEXT("0: off (uses null-tracking estimation, instead)\n")
	TEXT("1: on (uses analytical estimation)\n"),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarPathTracingAdaptiveSampling(
	TEXT("r.PathTracing.AdaptiveSampling"),
	0,
	TEXT("Determines if adaptive sampling is enabled. When non-zero, the path tracer will try to skip calculation of pixels below the specified error threshold.\n")
	TEXT("0: off (uniform sampling - default)\n")
	TEXT("1: on (adaptive sampling)\n"),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<float> CVarPathTracingAdaptiveSamplingErrorThreshold(
	TEXT("r.PathTracing.AdaptiveSampling.ErrorThreshold"),
	0.001f,
	TEXT("This is the target perceptual error threshold. Once a pixel's error falls below this value, it will not be sampled again (default: 0.001)\n"),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarPathTracingAdaptiveSamplingVisualize(
	TEXT("r.PathTracing.AdaptiveSampling.Visualize"),
	0,
	TEXT("Select a visualization mode to help understand how adaptive sampling is working.\n")
	TEXT("0: off\n")
	TEXT("1: Visualize active pixels with heatmap (converged pixels are displayed as is)\n")
	TEXT("2: Visualize sample count heatmap (against current max samples)\n")
	TEXT("3-7: Visualize variance mip levels\n"),
	ECVF_RenderThreadSafe
);

BEGIN_SHADER_PARAMETER_STRUCT(FPathTracingData, )
	SHADER_PARAMETER(float, BlendFactor)
	SHADER_PARAMETER(uint32, Iteration)
	SHADER_PARAMETER(uint32, TemporalSeed)
	SHADER_PARAMETER(uint32, MaxSamples)
	SHADER_PARAMETER(uint32, MaxBounces)
	SHADER_PARAMETER(uint32, MaxSSSBounces)
	SHADER_PARAMETER(float , SSSGuidingRatio)
	SHADER_PARAMETER(uint32, MISMode)
	SHADER_PARAMETER(uint32, VolumeMISMode)
	SHADER_PARAMETER(uint32, ApproximateCaustics)
	SHADER_PARAMETER(uint32, EnableCameraBackfaceCulling)
	SHADER_PARAMETER(uint32, SamplerType)
	SHADER_PARAMETER(uint32, VisualizeLightGrid)
	SHADER_PARAMETER(uint32, VisualizeDecalGrid)
	SHADER_PARAMETER(uint32, EnableDBuffer)
	SHADER_PARAMETER(uint32, VolumeFlags)
	SHADER_PARAMETER(uint32, EnabledDirectLightingContributions)   // PATHTRACER_CONTRIBUTION_*
	SHADER_PARAMETER(uint32, EnabledIndirectLightingContributions) // PATHTRACER_CONTRIBUTION_*
	SHADER_PARAMETER(uint32, ApplyDiffuseSpecularOverrides)
	SHADER_PARAMETER(int32, MaxRaymarchSteps)
	SHADER_PARAMETER(float, MaxPathIntensity)
	SHADER_PARAMETER(float, MaxNormalBias)
	SHADER_PARAMETER(float, FilterWidth)
	SHADER_PARAMETER(float, DecalRoughnessCutoff)
	SHADER_PARAMETER(float, MeshDecalRoughnessCutoff)
	SHADER_PARAMETER(float, MeshDecalBias)
	SHADER_PARAMETER(float, CameraFocusDistance)
	SHADER_PARAMETER(FVector2f, CameraLensRadius)
END_SHADER_PARAMETER_STRUCT()


// Store the rendering options used on the previous frame so we can correctly invalidate when things change
struct FPathTracingConfig
{
	FPathTracingData PathTracingData;
	FIntRect ViewRect;
	int LightShowFlags;
	int LightGridResolution;
	int LightGridMaxCount;
	bool VisibleLights;
	bool UseMISCompensation;
	bool LockedSamplingPattern;
	bool UseCameraMediumTracking;
	bool UseAdaptiveSampling;
	bool UseMultiGPU; // NOTE: Requires invalidation because the buffer layout changes
	int DenoiserMode; // NOTE: does not require path tracing invalidation
	float AdaptiveSamplingThreshold;

	bool IsDifferent(const FPathTracingConfig& Other) const
	{
		// If any of these parameters if different, we will need to restart path tracing accuulation
		return
			PathTracingData.MaxSamples != Other.PathTracingData.MaxSamples ||
			PathTracingData.MaxBounces != Other.PathTracingData.MaxBounces ||
			PathTracingData.MaxSSSBounces != Other.PathTracingData.MaxSSSBounces ||
			PathTracingData.SSSGuidingRatio != Other.PathTracingData.SSSGuidingRatio ||
			PathTracingData.MISMode != Other.PathTracingData.MISMode ||
			PathTracingData.VolumeMISMode != Other.PathTracingData.VolumeMISMode ||
			PathTracingData.SamplerType != Other.PathTracingData.SamplerType ||
			PathTracingData.ApproximateCaustics != Other.PathTracingData.ApproximateCaustics ||
			PathTracingData.EnableCameraBackfaceCulling != Other.PathTracingData.EnableCameraBackfaceCulling ||
			PathTracingData.VisualizeLightGrid != Other.PathTracingData.VisualizeLightGrid ||
			PathTracingData.VisualizeDecalGrid != Other.PathTracingData.VisualizeDecalGrid ||
			PathTracingData.EnableDBuffer != Other.PathTracingData.EnableDBuffer ||
			PathTracingData.MaxPathIntensity != Other.PathTracingData.MaxPathIntensity ||
			PathTracingData.FilterWidth != Other.PathTracingData.FilterWidth ||
			PathTracingData.VolumeFlags != Other.PathTracingData.VolumeFlags ||
			PathTracingData.ApplyDiffuseSpecularOverrides != Other.PathTracingData.ApplyDiffuseSpecularOverrides ||
			PathTracingData.EnabledDirectLightingContributions != Other.PathTracingData.EnabledDirectLightingContributions ||
			PathTracingData.EnabledIndirectLightingContributions != Other.PathTracingData.EnabledIndirectLightingContributions ||
			PathTracingData.DecalRoughnessCutoff != Other.PathTracingData.DecalRoughnessCutoff ||
			PathTracingData.MeshDecalRoughnessCutoff != Other.PathTracingData.MeshDecalRoughnessCutoff ||
			PathTracingData.MeshDecalBias != Other.PathTracingData.MeshDecalBias ||
			PathTracingData.MaxRaymarchSteps != Other.PathTracingData.MaxRaymarchSteps ||
			ViewRect != Other.ViewRect ||
			LightShowFlags != Other.LightShowFlags ||
			LightGridResolution != Other.LightGridResolution ||
			LightGridMaxCount != Other.LightGridMaxCount ||
			VisibleLights != Other.VisibleLights ||
			UseMISCompensation != Other.UseMISCompensation ||
			LockedSamplingPattern != Other.LockedSamplingPattern ||
			UseCameraMediumTracking != Other.UseCameraMediumTracking ||
			UseAdaptiveSampling != Other.UseAdaptiveSampling ||
			AdaptiveSamplingThreshold != Other.AdaptiveSamplingThreshold ||
			UseMultiGPU != Other.UseMultiGPU;
	}

	bool IsDOFDifferent(const FPathTracingConfig& Other) const
	{
		return PathTracingData.CameraFocusDistance != Other.PathTracingData.CameraFocusDistance ||
			   PathTracingData.CameraLensRadius != Other.PathTracingData.CameraLensRadius;

	}
};

struct FAtmosphereConfig
{
	FAtmosphereConfig() = default;
	FAtmosphereConfig(const FAtmosphereUniformShaderParameters& Parameters) :
		AtmoParameters(Parameters),
		NumSamples(CVarPathTracingAtmosphereOpticalDepthLutNumSamples.GetValueOnRenderThread()),
		Resolution(CVarPathTracingAtmosphereOpticalDepthLutResolution.GetValueOnRenderThread()) {}

	bool IsDifferent(const FAtmosphereConfig& Other) const
	{
		// Compare only those parameters which impact the LUT construction
		return
			AtmoParameters.BottomRadiusKm != Other.AtmoParameters.BottomRadiusKm ||
			AtmoParameters.TopRadiusKm != Other.AtmoParameters.TopRadiusKm ||
			AtmoParameters.RayleighDensityExpScale != Other.AtmoParameters.RayleighDensityExpScale ||
			AtmoParameters.RayleighScattering != Other.AtmoParameters.RayleighScattering ||
			AtmoParameters.MieScattering != Other.AtmoParameters.MieScattering ||
			AtmoParameters.MieDensityExpScale != Other.AtmoParameters.MieDensityExpScale ||
			AtmoParameters.MieExtinction != Other.AtmoParameters.MieExtinction ||
			AtmoParameters.MieAbsorption != Other.AtmoParameters.MieAbsorption ||
			AtmoParameters.AbsorptionDensity0LayerWidth != Other.AtmoParameters.AbsorptionDensity0LayerWidth ||
			AtmoParameters.AbsorptionDensity0ConstantTerm != Other.AtmoParameters.AbsorptionDensity0ConstantTerm ||
			AtmoParameters.AbsorptionDensity0LinearTerm != Other.AtmoParameters.AbsorptionDensity0LinearTerm ||
			AtmoParameters.AbsorptionDensity1ConstantTerm != Other.AtmoParameters.AbsorptionDensity1ConstantTerm ||
			AtmoParameters.AbsorptionDensity1LinearTerm != Other.AtmoParameters.AbsorptionDensity1LinearTerm ||
			AtmoParameters.AbsorptionExtinction != Other.AtmoParameters.AbsorptionExtinction ||
			NumSamples != Other.NumSamples ||
			Resolution != Other.Resolution;
	}

	// hold a copy of the parameters that influence LUT construction so we can detect when they change
	FAtmosphereUniformShaderParameters AtmoParameters;

	// parameters for the LUT itself
	uint32 NumSamples;
	uint32 Resolution;
};

struct FPathTracingState {
	FPathTracingConfig LastConfig;
	// Textures holding onto the accumulated frame data
	TRefCountPtr<IPooledRenderTarget> RadianceRT;
	TRefCountPtr<IPooledRenderTarget> VarianceRT;
	TRefCountPtr<IPooledRenderTarget> AlbedoRT;
	TRefCountPtr<IPooledRenderTarget> NormalRT;
	TRefCountPtr<FRDGPooledBuffer> VarianceBuffer;

	// Cache to improve the stability when frame denoising (SPP=r.pathtracing.SamplesPerPixel) is used in animation rendering
	TRefCountPtr<IPooledRenderTarget> LastDenoisedRadianceRT;
	TRefCountPtr<IPooledRenderTarget> LastRadianceRT;
	TRefCountPtr<IPooledRenderTarget> LastNormalRT;
	TRefCountPtr<IPooledRenderTarget> LastAlbedoRT;
	TRefCountPtr<FRDGPooledBuffer> LastVarianceBuffer;

	// Volume acceleration structures
	FAdaptiveOrthoGridParameterCache AdaptiveOrthoGridParameterCache;
	FAdaptiveFrustumGridParameterCache AdaptiveFrustumGridParameterCache;

	// Texture holding onto the precomputed atmosphere data
	TRefCountPtr<IPooledRenderTarget> AtmosphereOpticalDepthLUT;
	FAtmosphereConfig LastAtmosphereConfig;

	// Buffer containing the starting medium extinction
	TRefCountPtr<FRDGPooledBuffer>    StartingExtinctionCoefficient;

	// Custom path tracing spacial temporal denoiser result, used by plugins
	TRefCountPtr<UE::Renderer::Private::IPathTracingSpatialTemporalDenoiser::IHistory> SpatialTemporalDenoiserHistory;

	// Current sample index to be rendered by the path tracer - this gets incremented each time the path tracer accumulates a frame of samples
	uint32 SampleIndex = 0;

	// Path tracer frame index, not reset on invalidation unlike SampleIndex to avoid
	// the "screen door" effect and reduce temporal aliasing
	uint32_t FrameIndex = 0;
};

namespace PathTracing
{
	bool UsesDecals(const FSceneViewFamily& ViewFamily)
	{
		return ViewFamily.EngineShowFlags.Decals;
	}
}

static bool EvalUseAnalyticTransmittance(const FViewInfo& View)
{
	int32 UseAnalyticTransmittance = CVarPathTracingUseAnalyticTransmittance.GetValueOnRenderThread();
	if (UseAnalyticTransmittance < 0)
	{
		UseAnalyticTransmittance = !ShouldRenderHeterogeneousVolumesForView(View);
	}

	return UseAnalyticTransmittance != 0;
}

// This function prepares the portion of shader arguments that may involve invalidating the path traced state
static void PreparePathTracingData(const FScene* Scene, const FViewInfo& View, FPathTracingData& PathTracingData)
{
	const FFinalPostProcessSettings& PPV = View.FinalPostProcessSettings;
	const FEngineShowFlags& ShowFlags = View.Family->EngineShowFlags;

	int32 MaxBounces = CVarPathTracingMaxBounces.GetValueOnRenderThread();
	if (MaxBounces < 0)
	{
		MaxBounces = PPV.PathTracingMaxBounces;
	}

	PathTracingData.MaxBounces = MaxBounces;
	PathTracingData.MaxSSSBounces = ShowFlags.SubsurfaceScattering ? CVarPathTracingMaxSSSBounces.GetValueOnRenderThread() : 0;
	PathTracingData.SSSGuidingRatio = FMath::Clamp(CVarPathTracingSSSGuidingRatio.GetValueOnRenderThread(), 0.0f, 1.0f);
	PathTracingData.MaxNormalBias = GetRaytracingMaxNormalBias();
	PathTracingData.MISMode = CVarPathTracingMISMode.GetValueOnRenderThread();
	PathTracingData.VolumeMISMode = CVarPathTracingVolumeMISMode.GetValueOnRenderThread();
	PathTracingData.MaxPathIntensity = CVarPathTracingMaxPathIntensity.GetValueOnRenderThread();
	if (PathTracingData.MaxPathIntensity <= 0)
	{
		// cvar clamp disabled, use PPV exposure value instad
		PathTracingData.MaxPathIntensity = FMath::Pow(2.0f, PPV.PathTracingMaxPathExposure);
	}
	PathTracingData.ApproximateCaustics = CVarPathTracingApproximateCaustics.GetValueOnRenderThread();
	PathTracingData.EnableCameraBackfaceCulling = CVarPathTracingEnableCameraBackfaceCulling.GetValueOnRenderThread();
	PathTracingData.SamplerType = CVarPathTracingSamplerType.GetValueOnRenderThread();
	PathTracingData.VisualizeLightGrid = CVarPathTracingLightGridVisualize.GetValueOnRenderThread();
	PathTracingData.VisualizeDecalGrid = CVarPathTracingDecalGridVisualize.GetValueOnRenderThread();
	PathTracingData.FilterWidth = CVarPathTracingFilterWidth.GetValueOnRenderThread();
	PathTracingData.CameraFocusDistance = 0;
	PathTracingData.CameraLensRadius = FVector2f::ZeroVector;
	if (ShowFlags.DepthOfField &&
		PPV.PathTracingEnableReferenceDOF &&
		PPV.DepthOfFieldFocalDistance > 0 &&
		PPV.DepthOfFieldFstop > 0)
	{
		DiaphragmDOF::FPhysicalCocModel CocModel;
		CocModel.Compile(View);
		PathTracingData.CameraFocusDistance = CocModel.FocusDistance;
		PathTracingData.CameraLensRadius    = CocModel.GetLensRadius();
	}

	// Merge all volume flags into one uint
	PathTracingData.VolumeFlags = 0;
	PathTracingData.VolumeFlags |=
		ShouldRenderSkyAtmosphere(Scene, ShowFlags) && 
		View.SkyAtmosphereUniformShaderParameters != nullptr &&
		PPV.PathTracingEnableReferenceAtmosphere != 0 ? PATH_TRACER_VOLUME_ENABLE_ATMOSPHERE : 0;
	PathTracingData.VolumeFlags |= ShouldRenderFog(*View.Family)
		&& Scene->ExponentialFogs.Num() > 0
		&& Scene->ExponentialFogs[0].bEnableVolumetricFog
		&& Scene->ExponentialFogs[0].VolumetricFogDistance > 0
		&& Scene->ExponentialFogs[0].VolumetricFogExtinctionScale > 0
		&& (Scene->ExponentialFogs[0].FogData[0].Density > 0 ||
			Scene->ExponentialFogs[0].FogData[1].Density > 0) ? PATH_TRACER_VOLUME_ENABLE_FOG : 0;
	PathTracingData.VolumeFlags |= ShouldRenderHeterogeneousVolumesForView(View) ? PATH_TRACER_VOLUME_ENABLE_HETEROGENEOUS_VOLUMES : 0;
	PathTracingData.VolumeFlags |= View.SkyAtmosphereUniformShaderParameters != nullptr && IsSkyAtmosphereHoldout(View.CachedViewUniformShaderParameters->EnvironmentComponentsFlags) ? PATH_TRACER_VOLUME_HOLDOUT_ATMOSPHERE : 0;
	PathTracingData.VolumeFlags |= Scene->ExponentialFogs.Num() > 0 && IsExponentialFogHoldout(View.CachedViewUniformShaderParameters->EnvironmentComponentsFlags) ? PATH_TRACER_VOLUME_HOLDOUT_FOG : 0;
	PathTracingData.VolumeFlags |= EvalUseAnalyticTransmittance(View) ? PATH_TRACER_VOLUME_USE_ANALYTIC_TRANSMITTANCE : 0;

	PathTracingData.EnableDBuffer = CVarPathTracingUseDBuffer.GetValueOnRenderThread();

	PathTracingData.DecalRoughnessCutoff = PathTracing::UsesDecals(*View.Family) && View.bHasRayTracingDecals ? CVarPathTracingDecalRoughnessCutoff.GetValueOnRenderThread() : -1.0f;

	PathTracingData.MeshDecalRoughnessCutoff = PathTracing::UsesDecals(*View.Family) && Scene->RayTracingScene.GetRHIRayTracingScene()->GetInitializer().NumNativeInstancesPerLayer[(uint32)ERayTracingSceneLayer::Decals] > 0 ? CVarPathTracingMeshDecalRoughnessCutoff.GetValueOnRenderThread() : -1.0f;
	PathTracingData.MeshDecalBias = CVarPathTracingMeshDecalBias.GetValueOnRenderThread();

	PathTracingData.MaxRaymarchSteps = CVarPathTracingMaxRaymarchSteps.GetValueOnRenderThread();

	// NOTE: Diffuse and Specular show flags also modify the override colors, but we prefer to tie those to the lighting contribution mechanism below which is more principled
	PathTracingData.ApplyDiffuseSpecularOverrides =
		ShowFlags.LightingOnlyOverride       != 0 ||
		ShowFlags.OverrideDiffuseAndSpecular != 0 ||
		ShowFlags.ReflectionOverride         != 0;

	PathTracingData.EnabledDirectLightingContributions = 0;
	if (ShowFlags.DirectLighting != 0)
	{
		PathTracingData.EnabledDirectLightingContributions |= (PPV.PathTracingIncludeEmissive != 0                           ) ? PATHTRACER_CONTRIBUTION_EMISSIVE : 0;
		PathTracingData.EnabledDirectLightingContributions |= (PPV.PathTracingIncludeDiffuse  != 0 && ShowFlags.Diffuse  != 0) ? PATHTRACER_CONTRIBUTION_DIFFUSE  : 0;
		PathTracingData.EnabledDirectLightingContributions |= (PPV.PathTracingIncludeSpecular != 0 && ShowFlags.Specular != 0) ? PATHTRACER_CONTRIBUTION_SPECULAR : 0;
		PathTracingData.EnabledDirectLightingContributions |= (PPV.PathTracingIncludeVolume   != 0                           ) ? PATHTRACER_CONTRIBUTION_VOLUME   : 0;
	}
	PathTracingData.EnabledIndirectLightingContributions = 0;
	if (ShowFlags.GlobalIllumination != 0)
	{
		const int EnableEmissiveCVar = CVarPathTracingEnableEmissive.GetValueOnRenderThread();
		const bool bEnableEmissive = EnableEmissiveCVar < 0 ? PPV.PathTracingEnableEmissiveMaterials : EnableEmissiveCVar != 0;
		PathTracingData.EnabledIndirectLightingContributions |= (bEnableEmissive                                                       ) ? PATHTRACER_CONTRIBUTION_EMISSIVE : 0;
		PathTracingData.EnabledIndirectLightingContributions |= (PPV.PathTracingIncludeIndirectDiffuse  != 0 && ShowFlags.Diffuse  != 0) ? PATHTRACER_CONTRIBUTION_DIFFUSE  : 0;
		PathTracingData.EnabledIndirectLightingContributions |= (PPV.PathTracingIncludeIndirectSpecular != 0 && ShowFlags.Specular != 0) ? PATHTRACER_CONTRIBUTION_SPECULAR : 0;
		PathTracingData.EnabledIndirectLightingContributions |= (PPV.PathTracingIncludeIndirectVolume   != 0                           ) ? PATHTRACER_CONTRIBUTION_VOLUME   : 0;
	}
}

static bool ShouldCompilePathTracingShadersForProject(EShaderPlatform ShaderPlatform)
{
	return ShouldCompileRayTracingShadersForProject(ShaderPlatform) &&
			FDataDrivenShaderPlatformInfo::GetSupportsPathTracing(ShaderPlatform) &&
			CVarPathTracing.GetValueOnAnyThread() != 0;
}

static bool ShouldCompileGPULightmassShadersForProject(EShaderPlatform ShaderPlatform)
{
#if WITH_EDITOR
	if (!ShouldCompileRayTracingShadersForProject(ShaderPlatform))
	{
		return false;
	}
	// NOTE: cache on first use as this won't change
	static const bool bIsGPULightmassLoaded = FModuleManager::Get().IsModuleLoaded(TEXT("GPULightmass"));
	return bIsGPULightmassLoaded;
#else
	// GPULightmass is an editor only plugin, so don't compile any of its permutations otherwise
	return false;
#endif
}

static bool ShouldCompileGPULightmassShadersForProject(const FMeshMaterialShaderPermutationParameters& Parameters)
{
	return ShouldCompileGPULightmassShadersForProject(Parameters.Platform) &&
		EnumHasAllFlags(Parameters.Flags, EShaderPermutationFlags::HasEditorOnlyData) &&
		Parameters.VertexFactoryType->SupportsLightmapBaking();
}

class FPathTracingSkylightPrepareCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FPathTracingSkylightPrepareCS)
	SHADER_USE_PARAMETER_STRUCT(FPathTracingSkylightPrepareCS, FGlobalShader)

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		// NOTE: skylight code is shared with RT passes
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		//OutEnvironment.CompilerFlags.Add(CFLAG_WarningsAsErrors);
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_X"), FComputeShaderUtils::kGolden2DGroupSize);
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_Y"), FComputeShaderUtils::kGolden2DGroupSize);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(TextureCube, SkyLightCubemap0)
		SHADER_PARAMETER_TEXTURE(TextureCube, SkyLightCubemap1)
		SHADER_PARAMETER_SAMPLER(SamplerState, SkyLightCubemapSampler0)
		SHADER_PARAMETER_SAMPLER(SamplerState, SkyLightCubemapSampler1)
		SHADER_PARAMETER(float, SkylightBlendFactor)
		SHADER_PARAMETER(float, SkylightInvResolution)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, SkylightTextureOutput)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, SkylightTexturePdf)
		SHADER_PARAMETER(FVector3f, SkyColor)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_SHADER_TYPE(, FPathTracingSkylightPrepareCS, TEXT("/Engine/Private/PathTracing/PathTracingSkylightPrepare.usf"), TEXT("PathTracingSkylightPrepareCS"), SF_Compute);

class FPathTracingSkylightMISCompensationCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FPathTracingSkylightMISCompensationCS)
	SHADER_USE_PARAMETER_STRUCT(FPathTracingSkylightMISCompensationCS, FGlobalShader)

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		// NOTE: skylight code is shared with RT passes
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		//OutEnvironment.CompilerFlags.Add(CFLAG_WarningsAsErrors);
		OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_X"), FComputeShaderUtils::kGolden2DGroupSize);
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_Y"), FComputeShaderUtils::kGolden2DGroupSize);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, SkylightTexturePdfAverage)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, SkylightTextureOutput)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, SkylightTexturePdf)
		SHADER_PARAMETER(FVector3f, SkyColor)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_SHADER_TYPE(, FPathTracingSkylightMISCompensationCS, TEXT("/Engine/Private/PathTracing/PathTracingSkylightMISCompensation.usf"), TEXT("PathTracingSkylightMISCompensationCS"), SF_Compute);

// this struct holds a light grid for both building or rendering
BEGIN_SHADER_PARAMETER_STRUCT(FPathTracingLightGrid, RENDERER_API)
	SHADER_PARAMETER(uint32, SceneInfiniteLightCount)
	SHADER_PARAMETER(FVector3f, SceneLightsTranslatedBoundMin)
	SHADER_PARAMETER(FVector3f, SceneLightsTranslatedBoundMax)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, LightGrid)
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, LightGridData)
	SHADER_PARAMETER(unsigned, LightGridResolution)
	SHADER_PARAMETER(unsigned, LightGridMaxCount)
	SHADER_PARAMETER(int, LightGridAxis)
END_SHADER_PARAMETER_STRUCT()

class FPathTracingBuildLightGridCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FPathTracingBuildLightGridCS)
	SHADER_USE_PARAMETER_STRUCT(FPathTracingBuildLightGridCS, FGlobalShader)

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompilePathTracingShadersForProject(Parameters.Platform) ||
			   ShouldCompileGPULightmassShadersForProject(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.CompilerFlags.Add(CFLAG_WarningsAsErrors);
		OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_X"), FComputeShaderUtils::kGolden2DGroupSize);
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_Y"), FComputeShaderUtils::kGolden2DGroupSize);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FPathTracingLight>, SceneLights)
		SHADER_PARAMETER(uint32, SceneLightCount)
		SHADER_PARAMETER_STRUCT_INCLUDE(FPathTracingLightGrid, LightGridParameters)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, RWLightGrid)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWLightGridData)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_SHADER_TYPE(, FPathTracingBuildLightGridCS, TEXT("/Engine/Private/PathTracing/PathTracingBuildLightGrid.usf"), TEXT("PathTracingBuildLightGridCS"), SF_Compute);

// make a small custom struct to represent fog, because we need a more physical approach than the rest of the engine
BEGIN_SHADER_PARAMETER_STRUCT(FPathTracingFogParameters, )
	SHADER_PARAMETER(FVector2f, FogDensity)
	SHADER_PARAMETER(FVector2f, FogHeight)
	SHADER_PARAMETER(FVector2f, FogFalloff)
	SHADER_PARAMETER(FLinearColor, FogAlbedo)
	SHADER_PARAMETER(float, FogPhaseG)
	SHADER_PARAMETER(FVector2f, FogCenter)
	SHADER_PARAMETER(float, FogMinZ)
	SHADER_PARAMETER(float, FogMaxZ)
	SHADER_PARAMETER(float, FogRadius)
END_SHADER_PARAMETER_STRUCT()

static FPathTracingFogParameters PrepareFogParameters(const FViewInfo& View, const FExponentialHeightFogSceneInfo& FogInfo)
{
	static_assert(FExponentialHeightFogSceneInfo::NumFogs == 2, "Path tracing code assumes a fixed number of fogs");
	FPathTracingFogParameters Parameters = {};

	const FVector PreViewTranslation = View.ViewMatrices.GetPreViewTranslation();

	Parameters.FogDensity.X = FogInfo.FogData[0].Density * FogInfo.VolumetricFogExtinctionScale;
	Parameters.FogDensity.Y = FogInfo.FogData[1].Density * FogInfo.VolumetricFogExtinctionScale;
	Parameters.FogHeight.X = FogInfo.FogData[0].Height + PreViewTranslation.Z;
	Parameters.FogHeight.Y = FogInfo.FogData[1].Height + PreViewTranslation.Z;
	Parameters.FogFalloff.X = FMath::Max(FogInfo.FogData[0].HeightFalloff, 0.001f);
	Parameters.FogFalloff.Y = FMath::Max(FogInfo.FogData[1].HeightFalloff, 0.001f);
	Parameters.FogAlbedo = FogInfo.VolumetricFogAlbedo;
	Parameters.FogPhaseG = FogInfo.VolumetricFogScatteringDistribution;

	const float DensityEpsilon = 1e-6f;
	const float Radius = FogInfo.VolumetricFogDistance;
	// compute the value of Z at which the density becomes negligible (but don't go beyond the radius)
	const float ZMax0 = Parameters.FogHeight.X + FMath::Min(Radius, FMath::Log2(FMath::Max(Parameters.FogDensity.X, DensityEpsilon) / DensityEpsilon) / Parameters.FogFalloff.X);
	const float ZMax1 = Parameters.FogHeight.Y + FMath::Min(Radius, FMath::Log2(FMath::Max(Parameters.FogDensity.Y, DensityEpsilon) / DensityEpsilon) / Parameters.FogFalloff.Y);
	// lowest point is just defined by the radius (fog is homogeneous below the height)
	const float ZMin0 = Parameters.FogHeight.X - Radius;
	const float ZMin1 = Parameters.FogHeight.Y - Radius;

	// center X,Y around the current view point
	// NOTE: this can lead to "sliding" when the view distance is low, would it be better to just use the component center instead?
	// NOTE: the component position is not available here, would need to be added to FogInfo ...
	const FVector O = View.ViewMatrices.GetViewOrigin() + PreViewTranslation;
	Parameters.FogCenter = FVector2f(O.X, O.Y);
	Parameters.FogMinZ = FMath::Min(ZMin0, ZMin1);
	Parameters.FogMaxZ = FMath::Max(ZMax0, ZMax1);
	Parameters.FogRadius = Radius;
	return Parameters;
}

static uint32 GetPathtracingMaterialPayloadSize()
{
	// Substrate uses a slightly bigger payload as the basic slab contains more information
	return Substrate::IsSubstrateEnabled() ? 76u : 64u;
}

IMPLEMENT_RT_PAYLOAD_TYPE_FUNCTION(ERayTracingPayloadType::PathTracingMaterial, GetPathtracingMaterialPayloadSize);
IMPLEMENT_RT_PAYLOAD_TYPE(ERayTracingPayloadType::GPULightmass, 32);

class FPathTracingRG : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FPathTracingRG)
	SHADER_USE_ROOT_PARAMETER_STRUCT(FPathTracingRG, FGlobalShader)

	class FCompactionType : SHADER_PERMUTATION_BOOL("PATH_TRACER_USE_COMPACTION");
	class FAdaptiveSampling : SHADER_PERMUTATION_BOOL("PATH_TRACER_USE_ADAPTIVE_SAMPLING");
	class FSubstrateComplexSpecialMaterial : SHADER_PERMUTATION_BOOL("PATH_TRACER_USE_SUBSTRATE_SPECIAL_COMPLEX_MATERIAL");
	using FPermutationDomain = TShaderPermutationDomain<FCompactionType, FAdaptiveSampling, FSubstrateComplexSpecialMaterial>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		const bool bUseExperimental = CVarPathTracingExperimental.GetValueOnAnyThread() != 0;
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		if (bUseExperimental == false)
		{
			
			if (PermutationVector.Get<FCompactionType>() == 0)
			{
				// non-compaction tracing is considered experimental
				return false;
			}
			if (PermutationVector.Get<FAdaptiveSampling>())
			{
				// adaptive sampling is experimental
				return false;
			}
		}
		if (!Substrate::IsSubstrateEnabled())
		{
			// If we aren't using Substrate, no need to compile the complex material path
			if (PermutationVector.Get<FSubstrateComplexSpecialMaterial>())
			{
				return false;
			}
		}
		return ShouldCompilePathTracingShadersForProject(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("USE_RECT_LIGHT_TEXTURES"), 1);
		OutEnvironment.CompilerFlags.Add(CFLAG_WarningsAsErrors);
	}

	static ERayTracingPayloadType GetRayTracingPayloadType(const int32 PermutationId)
	{
		return ERayTracingPayloadType::PathTracingMaterial | ERayTracingPayloadType::Decals;
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RadianceTexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float2>, VarianceTexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, AlbedoTexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, NormalTexture)
		SHADER_PARAMETER_RDG_BUFFER_SRV(RaytracingAccelerationStructure, TLAS)
		SHADER_PARAMETER_RDG_BUFFER_SRV(RaytracingAccelerationStructure, DecalTLAS)

		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER_STRUCT_INCLUDE(FPathTracingData, PathTracingData)

		// scene lights
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FPathTracingLight>, SceneLights)
		SHADER_PARAMETER(uint32, SceneLightCount)
		SHADER_PARAMETER(uint32, SceneVisibleLightCount)
		SHADER_PARAMETER_STRUCT_INCLUDE(FPathTracingLightGrid, LightGridParameters)

		// Skylight
		SHADER_PARAMETER_STRUCT_INCLUDE(FPathTracingSkylight, SkylightParameters)

		// sky atmosphere
		SHADER_PARAMETER_STRUCT_REF(FAtmosphereUniformShaderParameters, Atmosphere)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, AtmosphereOpticalDepthLUT)
		SHADER_PARAMETER_SAMPLER(SamplerState, AtmosphereOpticalDepthLUTSampler)
		SHADER_PARAMETER(FVector3f, PlanetCenterTranslatedWorldHi)
		SHADER_PARAMETER(FVector3f, PlanetCenterTranslatedWorldLo)

		// exponential height fog
		SHADER_PARAMETER_STRUCT_INCLUDE(FPathTracingFogParameters, FogParameters)

		// Heterogeneous volumes adaptive voxel grid
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FOrthoVoxelGridUniformBufferParameters, OrthoGridUniformBuffer)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FFrustumVoxelGridUniformBufferParameters, FrustumGridUniformBuffer)

		// scene decals
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FRayTracingDecals, DecalParameters)

		// camera ray starting extinction coefficient
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float>, StartingExtinctionCoefficient)

		// Used by multi-GPU rendering and TDR-avoidance tiling
		SHADER_PARAMETER(FIntPoint, TilePixelOffset)
		SHADER_PARAMETER(FIntPoint, TileTextureOffset)
		SHADER_PARAMETER(int32, ScanlineStride)
		SHADER_PARAMETER(int32, ScanlineWidth)

		// extra parameters required for path compacting kernel
		SHADER_PARAMETER(int, Bounce)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FPathTracingPackedPathState>, PathStateData)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<int>, ActivePaths)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<int>, NextActivePaths)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<int>, NumPathStates)

		RDG_BUFFER_ACCESS(PathTracingIndirectArgs, ERHIAccess::IndirectArgs | ERHIAccess::SRVCompute)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FPathTracingRG, "/Engine/Private/PathTracing/PathTracing.usf", "PathTracingMainRG", SF_RayGen);

class FPathTracingInitExtinctionCoefficientRG : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FPathTracingInitExtinctionCoefficientRG)
	SHADER_USE_ROOT_PARAMETER_STRUCT(FPathTracingInitExtinctionCoefficientRG, FGlobalShader)

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompilePathTracingShadersForProject(Parameters.Platform);
	}

	static ERayTracingPayloadType GetRayTracingPayloadType(const int32 PermutationId)
	{
		return ERayTracingPayloadType::PathTracingMaterial;
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(RaytracingAccelerationStructure, TLAS)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float>, RWStartingExtinctionCoefficient)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FPathTracingInitExtinctionCoefficientRG, "/Engine/Private/PathTracing/PathTracingInitExtinctionCoefficient.usf", "PathTracingInitExtinctionCoefficientRG", SF_RayGen);

class FPathTracingSwizzleScanlinesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FPathTracingSwizzleScanlinesCS)
	SHADER_USE_PARAMETER_STRUCT(FPathTracingSwizzleScanlinesCS, FGlobalShader)

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompilePathTracingShadersForProject(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.CompilerFlags.Add(CFLAG_WarningsAsErrors);
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_X"), FComputeShaderUtils::kGolden2DGroupSize);
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_Y"), FComputeShaderUtils::kGolden2DGroupSize);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FIntPoint, DispatchDim)
		SHADER_PARAMETER(FIntPoint, TileSize)
		SHADER_PARAMETER(int32, ScanlineStride)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, InputTexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, OutputTexture)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_SHADER_TYPE(, FPathTracingSwizzleScanlinesCS, TEXT("/Engine/Private/PathTracing/PathTracingSwizzleScanlines.usf"), TEXT("PathTracingSwizzleScanlinesCS"), SF_Compute);


class FPathTracingBuildAtmosphereOpticalDepthLUTCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FPathTracingBuildAtmosphereOpticalDepthLUTCS)
	SHADER_USE_PARAMETER_STRUCT(FPathTracingBuildAtmosphereOpticalDepthLUTCS, FGlobalShader)

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompilePathTracingShadersForProject(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.CompilerFlags.Add(CFLAG_WarningsAsErrors);
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_X"), FComputeShaderUtils::kGolden2DGroupSize);
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_Y"), FComputeShaderUtils::kGolden2DGroupSize);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, NumSamples)
		SHADER_PARAMETER(uint32, Resolution)
		SHADER_PARAMETER_STRUCT_REF(FAtmosphereUniformShaderParameters, Atmosphere)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, AtmosphereOpticalDepthLUT)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_SHADER_TYPE(, FPathTracingBuildAtmosphereOpticalDepthLUTCS, TEXT("/Engine/Private/PathTracing/PathTracingBuildAtmosphereLUT.usf"), TEXT("PathTracingBuildAtmosphereOpticalDepthLUTCS"), SF_Compute);


class FPathTracingBuildAdaptiveErrorTextureCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FPathTracingBuildAdaptiveErrorTextureCS)
	SHADER_USE_PARAMETER_STRUCT(FPathTracingBuildAdaptiveErrorTextureCS, FGlobalShader)

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompilePathTracingShadersForProject(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.CompilerFlags.Add(CFLAG_WarningsAsErrors);
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_X"), FComputeShaderUtils::kGolden2DGroupSize);
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_Y"), FComputeShaderUtils::kGolden2DGroupSize);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FIntPoint, InputResolution)
		SHADER_PARAMETER(FIntPoint, OutputResolution)
		SHADER_PARAMETER_SAMPLER(SamplerState, InputMipSampler)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, InputMip)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, OutputMip)

	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_SHADER_TYPE(, FPathTracingBuildAdaptiveErrorTextureCS, TEXT("/Engine/Private/PathTracing/PathTracingBuildAdaptiveError.usf"), TEXT("PathTracingBuildAdaptiveErrorTextureCS"), SF_Compute);

class FPathTracingAdaptiveStartCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FPathTracingAdaptiveStartCS)
	SHADER_USE_PARAMETER_STRUCT(FPathTracingAdaptiveStartCS, FGlobalShader)

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompilePathTracingShadersForProject(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.CompilerFlags.Add(CFLAG_WarningsAsErrors);
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_X"), FComputeShaderUtils::kGolden2DGroupSize);
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_Y"), FComputeShaderUtils::kGolden2DGroupSize);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, VarianceTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, VarianceSampler)
		SHADER_PARAMETER(FIntVector, VarianceTextureDims)
		SHADER_PARAMETER(float, AdaptiveSamplingErrorThreshold)
		SHADER_PARAMETER(FIntPoint, TileTextureOffset)
		SHADER_PARAMETER(FIntPoint, DispatchDim)
		SHADER_PARAMETER(float, ViewPreExposure)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<int>, NextActivePaths)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<int>, NumPathStates)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_SHADER_TYPE(, FPathTracingAdaptiveStartCS, TEXT("/Engine/Private/PathTracing/PathTracingAdaptiveStart.usf"), TEXT("PathTracingAdaptiveStartCS"), SF_Compute);



// Default miss shader (using the path tracing payload)
template <bool IsGPULightmass>
class TPathTracingDefaultMS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TPathTracingDefaultMS, Global, );
public:

	TPathTracingDefaultMS() = default;
	TPathTracingDefaultMS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		if (IsGPULightmass)
		{
			return ShouldCompileGPULightmassShadersForProject(Parameters.Platform);
		}
		else
		{
			return ShouldCompilePathTracingShadersForProject(Parameters.Platform);
		}
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
	}

	static ERayTracingPayloadType GetRayTracingPayloadType(const int32 PermutationId)
	{
		if (IsGPULightmass)
		{
			return ERayTracingPayloadType::GPULightmass;
		}
		else
		{
			return ERayTracingPayloadType::PathTracingMaterial;
		}
	}
};

using FPathTracingDefaultMS  = TPathTracingDefaultMS<false>;
using FGPULightmassDefaultMS = TPathTracingDefaultMS<true>;
IMPLEMENT_SHADER_TYPE(template<>, FPathTracingDefaultMS , TEXT("/Engine/Private/PathTracing/PathTracingMissShader.usf"), TEXT("PathTracingDefaultMS"), SF_RayMiss);
IMPLEMENT_SHADER_TYPE(template<>, FGPULightmassDefaultMS, TEXT("/Engine/Private/PathTracing/PathTracingMissShader.usf"), TEXT("PathTracingDefaultMS"), SF_RayMiss);

FRHIRayTracingShader* GetPathTracingDefaultMissShader(const FGlobalShaderMap* ShaderMap)
{
	return ShaderMap->GetShader<FPathTracingDefaultMS>().GetRayTracingShader();
}

FRHIRayTracingShader* GetGPULightmassDefaultMissShader(const FGlobalShaderMap* ShaderMap)
{
	return ShaderMap->GetShader<FGPULightmassDefaultMS>().GetRayTracingShader();
}

void FDeferredShadingSceneRenderer::SetupPathTracingDefaultMissShader(FRHICommandListImmediate& RHICmdList, const FViewInfo& View)
{
	int32 MissShaderPipelineIndex = FindRayTracingMissShaderIndex(View.RayTracingMaterialPipeline, GetPathTracingDefaultMissShader(View.ShaderMap), true);

	RHICmdList.SetRayTracingMissShader(View.GetRayTracingSceneChecked(),
		RAY_TRACING_MISS_SHADER_SLOT_DEFAULT,
		View.RayTracingMaterialPipeline,
		MissShaderPipelineIndex,
		0, nullptr, 0);
}


BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FLightFunctionParametersPathTracing, )
	SHADER_PARAMETER(FMatrix44f, LightFunctionTranslatedWorldToLight)
	SHADER_PARAMETER(FVector4f, LightFunctionParameters)
	SHADER_PARAMETER(FVector3f, LightFunctionParameters2)
	SHADER_PARAMETER(int32    , EnableColoredLightFunctions)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FLightFunctionParametersPathTracing, "PathTracingLightFunctionParameters");

static TUniformBufferRef<FLightFunctionParametersPathTracing> CreateLightFunctionParametersBufferPT(
	const FLightSceneInfo* LightSceneInfo,
	const FSceneView& View,
	EUniformBufferUsage Usage)
{
	FLightFunctionParametersPathTracing LightFunctionParameters;

	const FVector Scale = LightSceneInfo->Proxy->GetLightFunctionScale();
	// Switch x and z so that z of the user specified scale affects the distance along the light direction
	const FVector InverseScale = FVector(1.f / Scale.Z, 1.f / Scale.Y, 1.f / Scale.X);
	const FMatrix WorldToLight = LightSceneInfo->Proxy->GetWorldToLight() * FScaleMatrix(FVector(InverseScale));

	LightFunctionParameters.LightFunctionTranslatedWorldToLight = FMatrix44f(FTranslationMatrix(-View.ViewMatrices.GetPreViewTranslation()) * WorldToLight);

	const bool bIsSpotLight = LightSceneInfo->Proxy->GetLightType() == LightType_Spot;
	const bool bIsPointLight = LightSceneInfo->Proxy->GetLightType() == LightType_Point;
	const float TanOuterAngle = bIsSpotLight ? FMath::Tan(LightSceneInfo->Proxy->GetOuterConeAngle()) : 1.0f;

	const float ShadowFadeFraction = 1.0f;

	LightFunctionParameters.LightFunctionParameters = FVector4f(TanOuterAngle, ShadowFadeFraction, bIsSpotLight ? 1.0f : 0.0f, bIsPointLight ? 1.0f : 0.0f);

	const bool bRenderingPreviewShadowIndicator = false;

	LightFunctionParameters.LightFunctionParameters2 = FVector3f(
		LightSceneInfo->Proxy->GetLightFunctionFadeDistance(),
		LightSceneInfo->Proxy->GetLightFunctionDisabledBrightness(),
		bRenderingPreviewShadowIndicator ? 1.0f : 0.0f);

	LightFunctionParameters.EnableColoredLightFunctions = CVarPathTracingLightFunctionColor.GetValueOnRenderThread();

	return CreateUniformBufferImmediate(LightFunctionParameters, Usage);
}

// Miss Shader implementing light functions
class FPathTracingLightingMS : public FMaterialShader
{
	DECLARE_SHADER_TYPE(FPathTracingLightingMS, Material);
	LAYOUT_FIELD(FShaderUniformBufferParameter, LightMaterialsParameter);

public:
	static bool ShouldCompilePermutation(const FMaterialShaderPermutationParameters& Parameters)
	{
		return Parameters.MaterialParameters.MaterialDomain == MD_LightFunction && ShouldCompilePathTracingShadersForProject(Parameters.Platform);
	}

	FPathTracingLightingMS() {}
	FPathTracingLightingMS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMaterialShader(Initializer)
	{
		LightMaterialsParameter.Bind(Initializer.ParameterMap, TEXT("PathTracingLightFunctionParameters"));
	}

	void GetShaderBindings(
		const FScene* Scene,
		ERHIFeatureLevel::Type FeatureLevel,
		const FMaterialRenderProxy& MaterialRenderProxy,
		const FMaterial& Material,
		const FViewInfo& View,
		const TUniformBufferRef<FLightFunctionParametersPathTracing>& LightFunctionParameters,
		FMeshDrawSingleShaderBindings& ShaderBindings) const
	{
		FMaterialShader::GetShaderBindings(Scene, FeatureLevel, MaterialRenderProxy, Material, ShaderBindings);
		ShaderBindings.Add(GetUniformBufferParameter<FViewUniformShaderParameters>(), View.ViewUniformBuffer);
		ShaderBindings.Add(LightMaterialsParameter, LightFunctionParameters);
	}

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}

	static ERayTracingPayloadType GetRayTracingPayloadType(const int32 PermutationId)
	{
		return ERayTracingPayloadType::PathTracingMaterial;
	}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(, FPathTracingLightingMS, TEXT("/Engine/Private/PathTracing/PathTracingLightingMissShader.usf"), TEXT("PathTracingLightingMS"), SF_RayMiss);


static void BindLightFunction(
	FRHICommandList& RHICmdList,
	const FScene* Scene,
	const FViewInfo& View,
	const FMaterial& Material,
	const FMaterialRenderProxy& MaterialRenderProxy,
	const TUniformBufferRef<FLightFunctionParametersPathTracing>& LightFunctionParameters,
	int32 Index
)
{
	FRHIRayTracingScene* RTScene = View.GetRayTracingSceneChecked();
	FRayTracingPipelineState* Pipeline = View.RayTracingMaterialPipeline;
	const FMaterialShaderMap* MaterialShaderMap = Material.GetRenderingThreadShaderMap();

	TShaderRef<FPathTracingLightingMS> Shader = MaterialShaderMap->GetShader<FPathTracingLightingMS>();

	FMeshDrawShaderBindings ShaderBindings;
	ShaderBindings.Initialize(Shader);

	FMeshDrawSingleShaderBindings SingleShaderBindings = ShaderBindings.GetSingleShaderBindings(SF_RayMiss);

	Shader->GetShaderBindings(Scene, Scene->GetFeatureLevel(), MaterialRenderProxy, Material, View, LightFunctionParameters, SingleShaderBindings);

	int32 MissShaderPipelineIndex = FindRayTracingMissShaderIndex(View.RayTracingMaterialPipeline, Shader.GetRayTracingShader(), true);

	ShaderBindings.SetRayTracingShaderBindingsForMissShader(RHICmdList, RTScene, Pipeline, MissShaderPipelineIndex, Index);
}

void BindLightFunctionShadersPathTracing(
	FRHICommandList& RHICmdList,
	const FScene* Scene,
	const FRayTracingLightFunctionMap* RayTracingLightFunctionMap,
	const class FViewInfo& View)
{
	if (RayTracingLightFunctionMap == nullptr)
	{
		return;
	}
	for (const FRayTracingLightFunctionMap::ElementType& LightAndIndex : *RayTracingLightFunctionMap)
	{
		const FLightSceneInfo* LightSceneInfo = LightAndIndex.Key;

		const FMaterialRenderProxy* MaterialProxy = LightSceneInfo->Proxy->GetLightFunctionMaterial();
		check(MaterialProxy != nullptr);
		// Catch the fallback material case
		const FMaterialRenderProxy* FallbackMaterialRenderProxyPtr = nullptr;
		const FMaterial& Material = MaterialProxy->GetMaterialWithFallback(Scene->GetFeatureLevel(), FallbackMaterialRenderProxyPtr);

		check(Material.IsLightFunction());

		const FMaterialRenderProxy& MaterialRenderProxy = FallbackMaterialRenderProxyPtr ? *FallbackMaterialRenderProxyPtr : *MaterialProxy;

		TUniformBufferRef<FLightFunctionParametersPathTracing> LightFunctionParameters = CreateLightFunctionParametersBufferPT(LightSceneInfo, View, EUniformBufferUsage::UniformBuffer_SingleFrame);

		int32 MissIndex = LightAndIndex.Value;
		BindLightFunction(RHICmdList, Scene, View, Material, MaterialRenderProxy, LightFunctionParameters, MissIndex);
	}
}


FRayTracingLightFunctionMap GatherLightFunctionLightsPathTracing(FScene* Scene, const FEngineShowFlags EngineShowFlags, ERHIFeatureLevel::Type InFeatureLevel)
{
	checkf(EngineShowFlags.LightFunctions, TEXT("This function should not be called if light functions are disabled"));
	FRayTracingLightFunctionMap RayTracingLightFunctionMap;
	for (const FLightSceneInfoCompact& Light : Scene->Lights)
	{
		FLightSceneInfo* LightSceneInfo = Light.LightSceneInfo;
		auto MaterialProxy = LightSceneInfo->Proxy->GetLightFunctionMaterial();
		if (MaterialProxy)
		{
			const FMaterialRenderProxy* FallbackMaterialRenderProxyPtr = nullptr;
			const FMaterial& Material = MaterialProxy->GetMaterialWithFallback(InFeatureLevel, FallbackMaterialRenderProxyPtr);
			if (Material.IsLightFunction())
			{
				const FMaterialShaderMap* MaterialShaderMap = Material.GetRenderingThreadShaderMap();
				// Getting the shader here has the side-effect of populating the raytracing miss shader library which is used when building the raytracing pipeline
				MaterialShaderMap->GetShader<FPathTracingLightingMS>().GetRayTracingShader();

				int32 Index = Scene->RayTracingScene.NumMissShaderSlots;
				Scene->RayTracingScene.NumMissShaderSlots++;
				RayTracingLightFunctionMap.Add(LightSceneInfo, Index);
			}
		}
	}
	return RayTracingLightFunctionMap;
}

static bool NeedsAnyHitShader(EBlendMode BlendMode)
{
	switch (BlendMode)
	{
		case BLEND_Opaque: 							return false; // always hit
		case BLEND_Masked: 							return true;  // runs shader (NOTE: dithered masking gets turned into translucent for the path tracer)
		case BLEND_Translucent: 					return true;  // casts transparent (colored) shadows depending on the shading model setup (fake caustics or transparent shadows)
		case BLEND_Additive: 						return false; // never hit for shadows, goes through the default shader instead, so no need to use AHS for primary rays
		case BLEND_Modulate: 						return true;  // casts colored shadows
		case BLEND_AlphaComposite: 					return true;
		case BLEND_AlphaHoldout: 					return false; // treat as opaque for shadows
		case BLEND_TranslucentColoredTransmittance: return true;  // NOTE: Substrate only
		default: checkf(false, TEXT("Unhandled blend mode %d"), int(BlendMode)); return false;
	}

}

static bool NeedsAnyHitShader(const FMaterial& RESTRICT MaterialResource)
{
	return NeedsAnyHitShader(MaterialResource.GetBlendMode());
}

template<bool UseAnyHitShader, bool UseIntersectionShader, bool IsGPULightmass, bool SimplifySubstrate>
class TPathTracingMaterial : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(TPathTracingMaterial, MeshMaterial);
public:
	TPathTracingMaterial() = default;

	TPathTracingMaterial(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer)
		: FMeshMaterialShader(Initializer)
	{}

	static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
	{
		if (!ShouldCompileRayTracingShadersForProject(Parameters.Platform))
		{
			// is raytracing enabled at all?
			return false;
		}
		if (!Parameters.VertexFactoryType->SupportsRayTracing())
		{
			// does the VF support ray tracing at all?
			return false;
		}
		if (Parameters.MaterialParameters.MaterialDomain != MD_Surface)
		{
			// This material is only for surfaces at the moment
			return false;
		}
		if (NeedsAnyHitShader(Parameters.MaterialParameters.BlendMode) != UseAnyHitShader)
		{
			return false;
		}
		const bool bUseProceduralPrimitive = Parameters.VertexFactoryType->SupportsRayTracingProceduralPrimitive() && FDataDrivenShaderPlatformInfo::GetSupportsRayTracingProceduralPrimitive(Parameters.Platform);
		if (UseIntersectionShader != bUseProceduralPrimitive)
		{
			// only need to compile the intersection shader permutation if the VF actually requires it
			return false;
		}		
		if (IsGPULightmass)
		{
			return ShouldCompileGPULightmassShadersForProject(Parameters);
		}
		else
		{
			if (SimplifySubstrate && (!Substrate::IsSubstrateEnabled() || CVarPathTracingSubstrateCompileSimplifiedMaterial.GetValueOnAnyThread() == 0))
			{
				// don't compile the extra Substrate permutation if:
				//    Substrate is not enabled on this project
				// or the user did not request the extra permutations to be compiled (default)
				return false;
			}
			return ShouldCompilePathTracingShadersForProject(Parameters.Platform);
		}
	}

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("USE_MATERIAL_CLOSEST_HIT_SHADER"), 1);
		OutEnvironment.SetDefine(TEXT("USE_MATERIAL_ANY_HIT_SHADER"), UseAnyHitShader ? 1 : 0);
		OutEnvironment.SetDefine(TEXT("USE_MATERIAL_INTERSECTION_SHADER"), UseIntersectionShader ? 1 : 0);
		OutEnvironment.SetDefine(TEXT("USE_RAYTRACED_TEXTURE_RAYCONE_LOD"), 0);
		OutEnvironment.SetDefine(TEXT("SCENE_TEXTURES_DISABLED"), 1);
		OutEnvironment.SetDefine(TEXT("SIMPLIFIED_MATERIAL_SHADER"), IsGPULightmass);
		OutEnvironment.SetDefine(TEXT("SUBSTRATE_USE_FULLYSIMPLIFIED_MATERIAL"), IsGPULightmass || SimplifySubstrate);
		OutEnvironment.CompilerFlags.Add(CFLAG_ForceDXC);
		OutEnvironment.CompilerFlags.Add(CFLAG_HLSL2021);
		FMeshMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}

	static bool ValidateCompiledResult(EShaderPlatform Platform, const FShaderParameterMap& ParameterMap, TArray<FString>& OutError)
	{
		if (ParameterMap.ContainsParameterAllocation(FSceneTextureUniformParameters::FTypeInfo::GetStructMetadata()->GetShaderVariableName()))
		{
			OutError.Add(TEXT("Ray tracing closest hit shaders cannot read from the SceneTexturesStruct."));
			return false;
		}

		for (const auto& It : ParameterMap.GetParameterMap())
		{
			const FParameterAllocation& ParamAllocation = It.Value;
			if (ParamAllocation.Type != EShaderParameterType::UniformBuffer
				&& ParamAllocation.Type != EShaderParameterType::LooseData)
			{
				OutError.Add(FString::Printf(TEXT("Invalid ray tracing shader parameter '%s'. Only uniform buffers and loose data parameters are supported."), *(It.Key)));
				return false;
			}
		}

		return true;
	}

	static ERayTracingPayloadType GetRayTracingPayloadType(const int32 PermutationId)
	{
		if (IsGPULightmass)
		{
			return ERayTracingPayloadType::GPULightmass;
		}
		else
		{
			return ERayTracingPayloadType::PathTracingMaterial;
		}
	}
};


// TODO: It would be nice to avoid this template boilerplate and just use ordinary permutations. This would require allowing the FunctionName for the material to be dependent on the permutation somehow
using FPathTracingMaterialCHS        = TPathTracingMaterial<false, false, false, false>;
using FPathTracingMaterialCHS_AHS    = TPathTracingMaterial<true , false, false, false>;
using FPathTracingMaterialCHS_IS     = TPathTracingMaterial<false, true , false, false>;
using FPathTracingMaterialCHS_AHS_IS = TPathTracingMaterial<true , true , false, false>;
using FPathTracingMaterialSimplifiedCHS        = TPathTracingMaterial<false, false, false, true>;
using FPathTracingMaterialSimplifiedCHS_AHS    = TPathTracingMaterial<true , false, false, true>;
using FPathTracingMaterialSimplifiedCHS_IS     = TPathTracingMaterial<false, true , false, true>;
using FPathTracingMaterialSimplifiedCHS_AHS_IS = TPathTracingMaterial<true , true , false, true>;


// NOTE: lightmass doesn't work with intersection shader VFs at the moment, so avoid instantiating permutations that will never generate any shaders
// Also lightmass is always using simplified Substrate mode.
using FGPULightmassCHS               = TPathTracingMaterial<false, false, true, true>;
using FGPULightmassCHS_AHS           = TPathTracingMaterial<true , false, true, true>;

IMPLEMENT_MATERIAL_SHADER_TYPE(template <>, FPathTracingMaterialCHS       , TEXT("/Engine/Private/PathTracing/PathTracingMaterialHitShader.usf"), TEXT("closesthit=PathTracingMaterialCHS"), SF_RayHitGroup);
IMPLEMENT_MATERIAL_SHADER_TYPE(template <>, FPathTracingMaterialCHS_AHS   , TEXT("/Engine/Private/PathTracing/PathTracingMaterialHitShader.usf"), TEXT("closesthit=PathTracingMaterialCHS anyhit=PathTracingMaterialAHS"), SF_RayHitGroup);
IMPLEMENT_MATERIAL_SHADER_TYPE(template <>, FPathTracingMaterialCHS_IS    , TEXT("/Engine/Private/PathTracing/PathTracingMaterialHitShader.usf"), TEXT("closesthit=PathTracingMaterialCHS intersection=MaterialIS"), SF_RayHitGroup);
IMPLEMENT_MATERIAL_SHADER_TYPE(template <>, FPathTracingMaterialCHS_AHS_IS, TEXT("/Engine/Private/PathTracing/PathTracingMaterialHitShader.usf"), TEXT("closesthit=PathTracingMaterialCHS anyhit=PathTracingMaterialAHS intersection=MaterialIS"), SF_RayHitGroup);
IMPLEMENT_MATERIAL_SHADER_TYPE(template <>, FPathTracingMaterialSimplifiedCHS       , TEXT("/Engine/Private/PathTracing/PathTracingMaterialHitShader.usf"), TEXT("closesthit=PathTracingMaterialCHS"), SF_RayHitGroup);
IMPLEMENT_MATERIAL_SHADER_TYPE(template <>, FPathTracingMaterialSimplifiedCHS_AHS   , TEXT("/Engine/Private/PathTracing/PathTracingMaterialHitShader.usf"), TEXT("closesthit=PathTracingMaterialCHS anyhit=PathTracingMaterialAHS"), SF_RayHitGroup);
IMPLEMENT_MATERIAL_SHADER_TYPE(template <>, FPathTracingMaterialSimplifiedCHS_IS    , TEXT("/Engine/Private/PathTracing/PathTracingMaterialHitShader.usf"), TEXT("closesthit=PathTracingMaterialCHS intersection=MaterialIS"), SF_RayHitGroup);
IMPLEMENT_MATERIAL_SHADER_TYPE(template <>, FPathTracingMaterialSimplifiedCHS_AHS_IS, TEXT("/Engine/Private/PathTracing/PathTracingMaterialHitShader.usf"), TEXT("closesthit=PathTracingMaterialCHS anyhit=PathTracingMaterialAHS intersection=MaterialIS"), SF_RayHitGroup);

IMPLEMENT_MATERIAL_SHADER_TYPE(template <>, FGPULightmassCHS              , TEXT("/Engine/Private/PathTracing/PathTracingGPULightmassMaterialHitShader.usf"), TEXT("closesthit=GPULightmassMaterialCHS"), SF_RayHitGroup);
IMPLEMENT_MATERIAL_SHADER_TYPE(template <>, FGPULightmassCHS_AHS          , TEXT("/Engine/Private/PathTracing/PathTracingGPULightmassMaterialHitShader.usf"), TEXT("closesthit=GPULightmassMaterialCHS anyhit=GPULightmassMaterialAHS"), SF_RayHitGroup);

template <bool IsGPULightmass, bool IsOpaque>
class TPathTracingDefaultHitGroup : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(TPathTracingDefaultHitGroup)
	SHADER_USE_ROOT_PARAMETER_STRUCT(TPathTracingDefaultHitGroup, FGlobalShader)

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		if (IsGPULightmass)
		{
			return ShouldCompileGPULightmassShadersForProject(Parameters.Platform);
		}
		else
		{
			return ShouldCompilePathTracingShadersForProject(Parameters.Platform);
		}
	}

	static ERayTracingPayloadType GetRayTracingPayloadType(const int32 PermutationId)
	{
		if (IsGPULightmass)
		{
			return ERayTracingPayloadType::GPULightmass;
		}
		else
		{
			return ERayTracingPayloadType::PathTracingMaterial;
		}
	}

	using FParameters = FEmptyShaderParameters;
};

using FPathTracingDefaultOpaqueHitGroup  = TPathTracingDefaultHitGroup<false, true >;
using FPathTracingDefaultHiddenHitGroup  = TPathTracingDefaultHitGroup<false, false>;
using FGPULightmassDefaultOpaqueHitGroup = TPathTracingDefaultHitGroup<true , true >;
using FGPULightmassDefaultHiddenHitGroup = TPathTracingDefaultHitGroup<true , false>;

IMPLEMENT_SHADER_TYPE(template<>, FPathTracingDefaultOpaqueHitGroup , TEXT("/Engine/Private/PathTracing/PathTracingDefaultHitShader.usf"), TEXT("closesthit=PathTracingDefaultOpaqueCHS"), SF_RayHitGroup);
IMPLEMENT_SHADER_TYPE(template<>, FGPULightmassDefaultOpaqueHitGroup, TEXT("/Engine/Private/PathTracing/PathTracingDefaultHitShader.usf"), TEXT("closesthit=PathTracingDefaultOpaqueCHS"), SF_RayHitGroup);
IMPLEMENT_SHADER_TYPE(template<>, FPathTracingDefaultHiddenHitGroup , TEXT("/Engine/Private/PathTracing/PathTracingDefaultHitShader.usf"), TEXT("closesthit=PathTracingDefaultHiddenCHS anyhit=PathTracingDefaultHiddenAHS"), SF_RayHitGroup);
IMPLEMENT_SHADER_TYPE(template<>, FGPULightmassDefaultHiddenHitGroup, TEXT("/Engine/Private/PathTracing/PathTracingDefaultHitShader.usf"), TEXT("closesthit=PathTracingDefaultHiddenCHS anyhit=PathTracingDefaultHiddenAHS"), SF_RayHitGroup);

FRHIRayTracingShader* GetPathTracingDefaultOpaqueHitShader(const FGlobalShaderMap* ShaderMap)
{
	return ShaderMap->GetShader<FPathTracingDefaultOpaqueHitGroup>().GetRayTracingShader();
}

FRHIRayTracingShader* GetGPULightmassDefaultOpaqueHitShader(const FGlobalShaderMap* ShaderMap)
{
	return ShaderMap->GetShader<FGPULightmassDefaultOpaqueHitGroup>().GetRayTracingShader();
}

FRHIRayTracingShader* GetPathTracingDefaultHiddenHitShader(const FGlobalShaderMap* ShaderMap)
{
	return ShaderMap->GetShader<FPathTracingDefaultHiddenHitGroup>().GetRayTracingShader();
}

FRHIRayTracingShader* GetGPULightmassDefaultHiddenHitShader(const FGlobalShaderMap* ShaderMap)
{
	return ShaderMap->GetShader<FGPULightmassDefaultHiddenHitGroup>().GetRayTracingShader();
}

bool FRayTracingMeshProcessor::ProcessPathTracing(
	const FMeshBatch& RESTRICT MeshBatch,
	uint64 BatchElementMask,
	const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
	const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
	const FMaterial& RESTRICT MaterialResource)
{
	FMaterialShaderTypes ShaderTypes;

	if (MaterialResource.GetMaterialDomain() == MD_DeferredDecal)
	{
		ShaderTypes.AddShaderType(GetRayTracingDecalMaterialShaderType(MaterialResource.GetBlendMode()));
	}
	else
	{
		const bool bUseProceduralPrimitive = MeshBatch.VertexFactory->GetType()->SupportsRayTracingProceduralPrimitive() &&
			FDataDrivenShaderPlatformInfo::GetSupportsRayTracingProceduralPrimitive(GMaxRHIShaderPlatform);
		switch (RayTracingMeshCommandsMode)
		{
			case ERayTracingMeshCommandsMode::PATH_TRACING:
			{
				// In order to use Substrate simplified materials, Substrate has to be enabled, we have to have _compiled_ the extra permutations _and_ the runtime toggle must be true
				const bool bUseSimplifiedMaterial = Substrate::IsSubstrateEnabled() &&
					CVarPathTracingSubstrateCompileSimplifiedMaterial.GetValueOnRenderThread() != 0 &&
					CVarPathTracingSubstrateUseSimplifiedMaterial.GetValueOnRenderThread() != 0;
				if (NeedsAnyHitShader(MaterialResource))
				{
					if (bUseSimplifiedMaterial)
					{
						if (bUseProceduralPrimitive)
							ShaderTypes.AddShaderType<FPathTracingMaterialSimplifiedCHS_AHS_IS>();
						else
							ShaderTypes.AddShaderType<FPathTracingMaterialSimplifiedCHS_AHS>();
					}
					else
					{
						if (bUseProceduralPrimitive)
							ShaderTypes.AddShaderType<FPathTracingMaterialCHS_AHS_IS>();
						else
							ShaderTypes.AddShaderType<FPathTracingMaterialCHS_AHS>();
					}
				}
				else
				{
					if (bUseSimplifiedMaterial)
					{
						if (bUseProceduralPrimitive)
							ShaderTypes.AddShaderType<FPathTracingMaterialSimplifiedCHS_IS>();
						else
							ShaderTypes.AddShaderType<FPathTracingMaterialSimplifiedCHS>();
					}
					else
					{
						if (bUseProceduralPrimitive)
							ShaderTypes.AddShaderType<FPathTracingMaterialCHS_IS>();
						else
							ShaderTypes.AddShaderType<FPathTracingMaterialCHS>();
					}
				}
				break;
			}
			case ERayTracingMeshCommandsMode::LIGHTMAP_TRACING:
			{
				if (NeedsAnyHitShader(MaterialResource))
				{
					ShaderTypes.AddShaderType<FGPULightmassCHS_AHS>();
				}
				else
				{
					ShaderTypes.AddShaderType<FGPULightmassCHS>();
				}
				break;
			}
			default:
			{
				return false;
			}
		}
	}

	FMaterialShaders Shaders;
	if (!MaterialResource.TryGetShaders(ShaderTypes, MeshBatch.VertexFactory->GetType(), Shaders))
	{
		return false;
	}

	TShaderRef<FMeshMaterialShader> RayTracingShader;
	if (!Shaders.TryGetShader(SF_RayHitGroup, RayTracingShader))
	{
		return false;
	}

	TBasePassShaderElementData<FUniformLightMapPolicy> ShaderElementData(MeshBatch.LCI);
	ShaderElementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, PrimitiveSceneProxy, MeshBatch, -1, true);

	BuildRayTracingMeshCommands(
		MeshBatch,
		BatchElementMask,
		PrimitiveSceneProxy,
		MaterialRenderProxy,
		MaterialResource,
		RayTracingShader,
		ShaderElementData,
		ERayTracingViewMaskMode::PathTracing);

	return true;
}

RENDERER_API void PrepareSkyTexture_Internal(
	FRDGBuilder& GraphBuilder,
	ERHIFeatureLevel::Type FeatureLevel,
	FReflectionUniformParameters& Parameters,
	uint32 Size,
	FLinearColor SkyColor,
	bool UseMISCompensation,

	// Out
	FRDGTextureRef& SkylightTexture,
	FRDGTextureRef& SkylightPdf,
	float& SkylightInvResolution,
	int32& SkylightMipCount
)
{
	FRDGTextureDesc SkylightTextureDesc = FRDGTextureDesc::Create2D(
		FIntPoint(Size, Size),
		PF_A32B32G32R32F, // half precision might be ok?
		FClearValueBinding::None,
		TexCreate_ShaderResource | TexCreate_UAV);

	SkylightTexture = GraphBuilder.CreateTexture(SkylightTextureDesc, TEXT("PathTracer.Skylight"), ERDGTextureFlags::None);

	FRDGTextureDesc SkylightPdfDesc = FRDGTextureDesc::Create2D(
		FIntPoint(Size, Size),
		PF_R32_FLOAT, // half precision might be ok?
		FClearValueBinding::None,
		TexCreate_ShaderResource | TexCreate_UAV,
		FMath::CeilLogTwo(Size) + 1);

	SkylightPdf = GraphBuilder.CreateTexture(SkylightPdfDesc, TEXT("PathTracer.SkylightPdf"), ERDGTextureFlags::None);

	SkylightInvResolution = 1.0f / Size;
	SkylightMipCount = SkylightPdfDesc.NumMips;

	// run a simple compute shader to sample the cubemap and prep the top level of the mipmap hierarchy
	{
		TShaderMapRef<FPathTracingSkylightPrepareCS> ComputeShader(GetGlobalShaderMap(FeatureLevel));
		FPathTracingSkylightPrepareCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FPathTracingSkylightPrepareCS::FParameters>();
		PassParameters->SkyColor = FVector3f(SkyColor.R, SkyColor.G, SkyColor.B);
		PassParameters->SkyLightCubemap0 = Parameters.SkyLightCubemap;
		PassParameters->SkyLightCubemap1 = Parameters.SkyLightBlendDestinationCubemap;
		PassParameters->SkyLightCubemapSampler0 = Parameters.SkyLightCubemapSampler;
		PassParameters->SkyLightCubemapSampler1 = Parameters.SkyLightBlendDestinationCubemapSampler;
		PassParameters->SkylightBlendFactor = Parameters.SkyLightParameters.W;
		PassParameters->SkylightInvResolution = SkylightInvResolution;
		PassParameters->SkylightTextureOutput = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(SkylightTexture, 0));
		PassParameters->SkylightTexturePdf = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(SkylightPdf, 0));
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("SkylightPrepare"),
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(FIntPoint(Size, Size), FComputeShaderUtils::kGolden2DGroupSize));
	}
	FGenerateMips::ExecuteCompute(GraphBuilder, FeatureLevel, SkylightPdf, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());

	if (UseMISCompensation)
	{
		TShaderMapRef<FPathTracingSkylightMISCompensationCS> ComputeShader(GetGlobalShaderMap(FeatureLevel));
		FPathTracingSkylightMISCompensationCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FPathTracingSkylightMISCompensationCS::FParameters>();
		PassParameters->SkylightTexturePdfAverage = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForMipLevel(SkylightPdf, SkylightMipCount - 1));
		PassParameters->SkylightTextureOutput = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(SkylightTexture, 0));
		PassParameters->SkylightTexturePdf = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(SkylightPdf, 0));
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("SkylightMISCompensation"),
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(FIntPoint(Size, Size), FComputeShaderUtils::kGolden2DGroupSize));
		FGenerateMips::ExecuteCompute(GraphBuilder, FeatureLevel, SkylightPdf, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());
	}
}

RDG_REGISTER_BLACKBOARD_STRUCT(FPathTracingSkylight)

bool PrepareSkyTexture(FRDGBuilder& GraphBuilder, FScene* Scene, const FViewInfo& View, bool SkylightEnabled, bool UseMISCompensation, FPathTracingSkylight* SkylightParameters)
{
	SkylightParameters->SkylightTextureSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	FReflectionUniformParameters Parameters;
	SetupReflectionUniformParameters(GraphBuilder, View, Parameters);
	if (!SkylightEnabled || !(Parameters.SkyLightParameters.Y > 0))
	{
		// textures not ready, or skylight not active
		// just put in a placeholder
		SkylightParameters->SkylightTexture = GraphBuilder.RegisterExternalTexture(GSystemTextures.BlackDummy);
		SkylightParameters->SkylightPdf = GraphBuilder.RegisterExternalTexture(GSystemTextures.BlackDummy);
		SkylightParameters->SkylightInvResolution = 0;
		SkylightParameters->SkylightMipCount = 0;
		return false;
	}

	// the sky is actually enabled, lets see if someone already made use of it for this frame
	const FPathTracingSkylight* PreviousSkylightParameters = GraphBuilder.Blackboard.Get<FPathTracingSkylight>();
	if (PreviousSkylightParameters != nullptr)
	{
		*SkylightParameters = *PreviousSkylightParameters;
		return true;
	}

	// should we remember the skylight prep for the next frame?
	const bool IsSkylightCachingEnabled = CVarPathTracingSkylightCaching.GetValueOnAnyThread() != 0;
	FLinearColor SkyColor = Scene->SkyLight->GetEffectiveLightColor();
	const bool bSkylightColorChanged = SkyColor != Scene->PathTracingSkylightColor;
	if (!IsSkylightCachingEnabled || bSkylightColorChanged)
	{
		// we don't want any caching (or the light color changed)
		// release what we might have been holding onto so we get the right texture for this frame
		Scene->PathTracingSkylightTexture.SafeRelease();
		Scene->PathTracingSkylightPdf.SafeRelease();
	}

	if (Scene->PathTracingSkylightTexture.IsValid() &&
		Scene->PathTracingSkylightPdf.IsValid())
	{
		// we already have a valid texture and pdf, just re-use them!
		// it is the responsability of code that may invalidate the contents to reset these pointers
		SkylightParameters->SkylightTexture = GraphBuilder.RegisterExternalTexture(Scene->PathTracingSkylightTexture, TEXT("PathTracer.Skylight"));
		SkylightParameters->SkylightPdf = GraphBuilder.RegisterExternalTexture(Scene->PathTracingSkylightPdf, TEXT("PathTracer.SkylightPdf"));
		SkylightParameters->SkylightInvResolution = 1.0f / SkylightParameters->SkylightTexture->Desc.GetSize().X;
		SkylightParameters->SkylightMipCount = SkylightParameters->SkylightPdf->Desc.NumMips;
		return true;
	}
	RDG_EVENT_SCOPE(GraphBuilder, "Path Tracing SkylightPrepare");
	Scene->PathTracingSkylightColor = SkyColor;
	// since we are resampled into an octahedral layout, we multiply the cubemap resolution by 2 to get roughly the same number of texels
	uint32 Size = FMath::RoundUpToPowerOfTwo(2 * Scene->SkyLight->CaptureCubeMapResolution);
	
	RDG_GPU_MASK_SCOPE(GraphBuilder, 
		IsSkylightCachingEnabled ? FRHIGPUMask::All() : GraphBuilder.RHICmdList.GetGPUMask());

	PrepareSkyTexture_Internal(
		GraphBuilder,
		View.FeatureLevel,
		Parameters,
		Size,
		SkyColor,
		UseMISCompensation,
		// Out
		SkylightParameters->SkylightTexture,
		SkylightParameters->SkylightPdf,
		SkylightParameters->SkylightInvResolution,
		SkylightParameters->SkylightMipCount
	);

	// hang onto these for next time (if caching is enabled)
	if (IsSkylightCachingEnabled)
	{
		GraphBuilder.QueueTextureExtraction(SkylightParameters->SkylightTexture, &Scene->PathTracingSkylightTexture);
		GraphBuilder.QueueTextureExtraction(SkylightParameters->SkylightPdf, &Scene->PathTracingSkylightPdf);
	}

	// remember the skylight parameters for future passes within this frame
	GraphBuilder.Blackboard.Create<FPathTracingSkylight>() = *SkylightParameters;

	return true;
}

RENDERER_API void PrepareLightGrid(FRDGBuilder& GraphBuilder, ERHIFeatureLevel::Type FeatureLevel, FPathTracingLightGrid* LightGridParameters, const FPathTracingLight* Lights, uint32 NumLights, uint32 NumInfiniteLights, FRDGBufferSRV* LightsSRV)
{
	const float Inf = std::numeric_limits<float>::infinity();
	LightGridParameters->SceneInfiniteLightCount = NumInfiniteLights;
	LightGridParameters->SceneLightsTranslatedBoundMin = FVector3f(+Inf, +Inf, +Inf);
	LightGridParameters->SceneLightsTranslatedBoundMax = FVector3f(-Inf, -Inf, -Inf);
	LightGridParameters->LightGrid = nullptr;
	LightGridParameters->LightGridData = nullptr;

	int NumFiniteLights = NumLights - NumInfiniteLights;
	// if we have some finite lights -- build a light grid
	if (NumFiniteLights > 0)
	{
		// get bounding box of all finite lights
		const FPathTracingLight* FiniteLights = Lights + NumInfiniteLights;
		for (int Index = 0; Index < NumFiniteLights; Index++)
		{
			const FPathTracingLight& Light = FiniteLights[Index];
			FVector3f Lo = FVector3f(-Inf, -Inf, -Inf);
			FVector3f Hi = FVector3f( Inf,  Inf,  Inf);

			const float Radius = 1.0f / Light.Attenuation;
			const FVector3f Center = Light.TranslatedWorldPosition;
			const FVector3f Normal = Light.Normal;
			const FVector3f Disc = FVector3f(
				FMath::Sqrt(FMath::Clamp(1 - Normal.X * Normal.X, 0.0f, 1.0f)),
				FMath::Sqrt(FMath::Clamp(1 - Normal.Y * Normal.Y, 0.0f, 1.0f)),
				FMath::Sqrt(FMath::Clamp(1 - Normal.Z * Normal.Z, 0.0f, 1.0f))
			);
			switch (Light.Flags & PATHTRACER_FLAG_TYPE_MASK)
			{
				case PATHTRACING_LIGHT_POINT:
				{
					// simple sphere of influence
					Lo = Center - FVector3f(Radius, Radius, Radius);
					Hi = Center + FVector3f(Radius, Radius, Radius);
					break;
				}
				case PATHTRACING_LIGHT_SPOT:
				{
					// box around ray from light center to tip of the cone
					const FVector3f Tip = Center + Normal * Radius;
					Lo = FVector3f::Min(Center, Tip);
					Hi = FVector3f::Max(Center, Tip);

					// expand by disc around the farthest part of the cone
					const float CosOuter = Light.Shaping.X;
					const float SinOuter = FMath::Sqrt(1.0f - CosOuter * CosOuter);

					Lo = FVector3f::Min(Lo, Center + Radius * (Normal * CosOuter - Disc * SinOuter));
					Hi = FVector3f::Max(Hi, Center + Radius * (Normal * CosOuter + Disc * SinOuter));
					break;
				}
				case PATHTRACING_LIGHT_RECT:
				{
					// quad bbox is the bbox of the disc +  the tip of the hemisphere
					// TODO: is it worth trying to account for barndoors? seems unlikely to cut much empty space since the volume _inside_ the barndoor receives light
					const FVector3f Tip = Center + Normal * Radius;
					Lo = FVector3f::Min(Tip, Center - Radius * Disc);
					Hi = FVector3f::Max(Tip, Center + Radius * Disc);
					break;
				}
				default:
				{
					// non-finite lights should not appear in this case
					checkSlow(false);
					break;
				}
			}
			LightGridParameters->SceneLightsTranslatedBoundMin = FVector3f::Min(LightGridParameters->SceneLightsTranslatedBoundMin, Lo);
			LightGridParameters->SceneLightsTranslatedBoundMax = FVector3f::Max(LightGridParameters->SceneLightsTranslatedBoundMax, Hi);
		}

		const uint32 Resolution = FMath::RoundUpToPowerOfTwo(CVarPathTracingLightGridResolution.GetValueOnRenderThread());
		const uint32 MaxCount = FMath::Clamp(
			CVarPathTracingLightGridMaxCount.GetValueOnRenderThread(),
			1,
			FMath::Min(NumFiniteLights, RAY_TRACING_LIGHT_COUNT_MAXIMUM)
		);
		LightGridParameters->LightGridResolution = Resolution;
		LightGridParameters->LightGridMaxCount = MaxCount;

		// pick the shortest axis
		FVector3f Diag = LightGridParameters->SceneLightsTranslatedBoundMax - LightGridParameters->SceneLightsTranslatedBoundMin;
		if (Diag.X < Diag.Y && Diag.X < Diag.Z)
		{
			LightGridParameters->LightGridAxis = 0;
		}
		else if (Diag.Y < Diag.Z)
		{
			LightGridParameters->LightGridAxis = 1;
		}
		else
		{
			LightGridParameters->LightGridAxis = 2;
		}

		FPathTracingBuildLightGridCS::FParameters* LightGridPassParameters = GraphBuilder.AllocParameters< FPathTracingBuildLightGridCS::FParameters>();

		FRDGTextureDesc LightGridDesc = FRDGTextureDesc::Create2D(
			FIntPoint(Resolution, Resolution),
			PF_R32_UINT,
			FClearValueBinding::None,
			TexCreate_ShaderResource | TexCreate_UAV);

		// jhoerner TODO 9/30/2022: Hack to work around MGPU resource transition architectural bug in RDG.  Mask PathTracer.LightGrid texture
		// to only be present on current GPU.  The bug is that RDG batches transitions, but the execution of batched transitions uses the
		// GPU Mask of the current Pass that's executing, not the GPU Mask that's relevant to the Passes where a given resource is used.  This
		// causes an assert due to a mismatch in the expected transition state on a specific GPU, when an intermediate transition was skipped
		// on that GPU, due to the arbitrary nature of the GPU mask when a transition batch is flushed.  The hack works by removing the
		// resource from GPUs it's not actually used on, where the intermediate transition gets skipped.
		LightGridDesc.GPUMask = GraphBuilder.RHICmdList.GetGPUMask();

		FRDGTexture* LightGridTexture = GraphBuilder.CreateTexture(LightGridDesc, TEXT("PathTracer.LightGrid"), ERDGTextureFlags::None);
		LightGridPassParameters->RWLightGrid = GraphBuilder.CreateUAV(LightGridTexture);

		EPixelFormat LightGridDataFormat = PF_R32_UINT;
		size_t LightGridDataNumBytes = sizeof(uint32);
		if (NumLights <= (MAX_uint8 + 1))
		{
			LightGridDataFormat = PF_R8_UINT;
			LightGridDataNumBytes = sizeof(uint8);
		}
		else if (NumLights <= (MAX_uint16 + 1))
		{
			LightGridDataFormat = PF_R16_UINT;
			LightGridDataNumBytes = sizeof(uint16);
		}
		FRDGBufferDesc LightGridDataDesc = FRDGBufferDesc::CreateBufferDesc(LightGridDataNumBytes, MaxCount * Resolution * Resolution);
		FRDGBuffer* LightGridData = GraphBuilder.CreateBuffer(LightGridDataDesc, TEXT("PathTracer.LightGridData"));
		LightGridPassParameters->RWLightGridData = GraphBuilder.CreateUAV(LightGridData, LightGridDataFormat);
		LightGridPassParameters->LightGridParameters = *LightGridParameters;
		LightGridPassParameters->SceneLights = LightsSRV;
		LightGridPassParameters->SceneLightCount = NumLights;

		TShaderMapRef<FPathTracingBuildLightGridCS> ComputeShader(GetGlobalShaderMap(FeatureLevel));
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("Light Grid Create (%u lights)", NumFiniteLights),
			ComputeShader,
			LightGridPassParameters,
			FComputeShaderUtils::GetGroupCount(FIntPoint(Resolution, Resolution), FComputeShaderUtils::kGolden2DGroupSize));

		// hookup to the actual rendering pass
		LightGridParameters->LightGrid = LightGridTexture;
		LightGridParameters->LightGridData = GraphBuilder.CreateSRV(LightGridData, LightGridDataFormat);


	}
	else
	{
		// light grid is not needed - just hookup dummy data
		LightGridParameters->LightGridResolution = 0;
		LightGridParameters->LightGridMaxCount = 0;
		LightGridParameters->LightGridAxis = 0;
		LightGridParameters->LightGrid = GraphBuilder.RegisterExternalTexture(GSystemTextures.BlackDummy);
		FRDGBufferDesc LightGridDataDesc = FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), 1);
		FRDGBuffer* LightGridData = GraphBuilder.CreateBuffer(LightGridDataDesc, TEXT("PathTracer.LightGridData"));
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(LightGridData, PF_R32_UINT), 0);
		LightGridParameters->LightGridData = GraphBuilder.CreateSRV(LightGridData, PF_R32_UINT);
	}
}

void SetLightParameters(FRDGBuilder& GraphBuilder, FPathTracingRG::FParameters* PassParameters, FScene* Scene, const FViewInfo& View, bool UseMISCompensation)
{
	PassParameters->SceneVisibleLightCount = 0;

	// Lights
	uint32 MaxNumLights = 1 + Scene->Lights.Num(); // upper bound
	// Allocate from the graph builder so that we don't need to copy the data again when queuing the upload
	FPathTracingLight* Lights = (FPathTracingLight*) GraphBuilder.Alloc(sizeof(FPathTracingLight) * MaxNumLights, 16);
	uint32 NumLights = 0;

	// Prepend SkyLight to light buffer since it is not part of the regular light list
	// skylight should be excluded if we are using the reference atmosphere calculation (don't bother checking again if an atmosphere is present)
	const bool bUseAtmosphere = (PassParameters->PathTracingData.VolumeFlags & PATH_TRACER_VOLUME_ENABLE_ATMOSPHERE) != 0;
	const bool bEnableSkydome = !bUseAtmosphere;
	if (PrepareSkyTexture(GraphBuilder, Scene, View, bEnableSkydome, UseMISCompensation, &PassParameters->SkylightParameters))
	{
		check(Scene->SkyLight != nullptr);
		FPathTracingLight& DestLight = Lights[NumLights++];
		DestLight.Color = FVector3f(1, 1, 1); // not used (it is folded into the importance table directly)
		DestLight.Flags = Scene->SkyLight->bTransmission ? PATHTRACER_FLAG_TRANSMISSION_MASK : 0;
		DestLight.Flags |= PATHTRACER_FLAG_LIGHTING_CHANNEL_MASK;
		DestLight.Flags |= PATHTRACING_LIGHT_SKY;
		DestLight.Flags |= Scene->SkyLight->bCastShadows ? PATHTRACER_FLAG_CAST_SHADOW_MASK : 0;
		DestLight.Flags |= Scene->SkyLight->bCastVolumetricShadow ? PATHTRACER_FLAG_CAST_VOL_SHADOW_MASK : 0;
		DestLight.SpecularScale = 1.0f;
		DestLight.VolumetricScatteringIntensity = Scene->SkyLight->VolumetricScatteringIntensity;
		DestLight.IESAtlasIndex = INDEX_NONE;
		DestLight.MissShaderIndex = 0;
		if ((Scene->SkyLight->bRealTimeCaptureEnabled && (View.SkyAtmosphereUniformShaderParameters == nullptr ||  !IsSkyAtmosphereHoldout(View.CachedViewUniformShaderParameters->EnvironmentComponentsFlags))) || CVarPathTracingVisibleLights.GetValueOnRenderThread() == 2)
		{
			// When using the realtime capture system, always make the skylight visible
			// because this is our only way of "seeing" the atmo/clouds at the moment
			// The one exception to this case is if the sky atmo has been marked as holdout.

			// Also allow seeing just the sky via a cvar for debugging purposes
			PassParameters->SceneVisibleLightCount = 1;
		}
	}

	const FRayTracingLightFunctionMap* RayTracingLightFunctionMap = GraphBuilder.Blackboard.Get<FRayTracingLightFunctionMap>();

	// Add directional lights next (all lights with infinite bounds should come first)
	if (View.Family->EngineShowFlags.DirectionalLights)
	{
		for (const FLightSceneInfoCompact& Light : Scene->Lights)
		{
			ELightComponentType LightComponentType = (ELightComponentType)Light.LightSceneInfo->Proxy->GetLightType();

			if (LightComponentType != LightType_Directional)
			{
				continue;
			}

			FLightRenderParameters LightParameters;
			Light.LightSceneInfo->Proxy->GetLightShaderParameters(LightParameters);

			if (FVector3f(LightParameters.Color).IsZero())
			{
				continue;
			}

			FPathTracingLight& DestLight = Lights[NumLights++];
			uint32 Transmission = Light.LightSceneInfo->Proxy->Transmission();
			uint8 LightingChannelMask = Light.LightSceneInfo->Proxy->GetLightingChannelMask();

			DestLight.Flags = Transmission ? PATHTRACER_FLAG_TRANSMISSION_MASK : 0;
			DestLight.Flags |= LightingChannelMask & PATHTRACER_FLAG_LIGHTING_CHANNEL_MASK;
			DestLight.Flags |= Light.LightSceneInfo->Proxy->CastsDynamicShadow() ? PATHTRACER_FLAG_CAST_SHADOW_MASK : 0;
			DestLight.Flags |= Light.LightSceneInfo->Proxy->CastsVolumetricShadow() ? PATHTRACER_FLAG_CAST_VOL_SHADOW_MASK : 0;
			DestLight.IESAtlasIndex = INDEX_NONE;
			DestLight.MissShaderIndex = 0;

			if (RayTracingLightFunctionMap)
			{
				const int32* LightFunctionIndex = RayTracingLightFunctionMap->Find(Light.LightSceneInfo);
				if (LightFunctionIndex)
				{
					DestLight.MissShaderIndex = *LightFunctionIndex;
				}
			}

			// these mean roughly the same thing across all light types
			DestLight.Color = FVector3f(LightParameters.Color) * LightParameters.GetLightExposureScale(View.GetLastEyeAdaptationExposure());
			DestLight.TranslatedWorldPosition = FVector3f(LightParameters.WorldPosition + View.ViewMatrices.GetPreViewTranslation());
			DestLight.Normal = -LightParameters.Direction;
			DestLight.dPdu = FVector3f::CrossProduct(LightParameters.Tangent, LightParameters.Direction);
			DestLight.dPdv = LightParameters.Tangent;
			DestLight.Shaping = FVector2f(0.0f, 0.0f);
			DestLight.SpecularScale = LightParameters.SpecularScale;
			DestLight.Attenuation = LightParameters.InvRadius;
			DestLight.FalloffExponent = 0;
			DestLight.VolumetricScatteringIntensity = Light.LightSceneInfo->Proxy->GetVolumetricScatteringIntensity();
			DestLight.RectLightAtlasUVOffset = FVector2f(0.0f, 0.0f);
			DestLight.RectLightAtlasUVScale = FVector2f(0.0f, 0.0f);

			DestLight.Normal = LightParameters.Direction;
			DestLight.Dimensions = FVector2f(LightParameters.SourceRadius, 0.0f);
			DestLight.Flags |= PATHTRACING_LIGHT_DIRECTIONAL;
		}
	}

	if (bUseAtmosphere)
	{
		// show directional lights when atmosphere is enabled
		// NOTE: there cannot be any skydome in this case
		PassParameters->SceneVisibleLightCount = NumLights;
	}

	uint32 NumInfiniteLights = NumLights;

	int32 NextRectTextureIndex = 0;

	for (const FLightSceneInfoCompact& Light : Scene->Lights)
	{
		ELightComponentType LightComponentType = (ELightComponentType)Light.LightSceneInfo->Proxy->GetLightType();

		if ( (LightComponentType == LightType_Directional) /* already handled by the loop above */  ||
			((LightComponentType == LightType_Rect       ) && !View.Family->EngineShowFlags.RectLights       ) ||
			((LightComponentType == LightType_Spot       ) && !View.Family->EngineShowFlags.SpotLights       ) ||
			((LightComponentType == LightType_Point      ) && !View.Family->EngineShowFlags.PointLights      ))
		{
			// This light type is not currently enabled
			continue;
		}

		FLightRenderParameters LightParameters;
		Light.LightSceneInfo->Proxy->GetLightShaderParameters(LightParameters);

		if (FVector3f(LightParameters.Color).IsZero())
		{
			continue;
		}

		FPathTracingLight& DestLight = Lights[NumLights++];

		uint32 Transmission = Light.LightSceneInfo->Proxy->Transmission();
		uint8 LightingChannelMask = Light.LightSceneInfo->Proxy->GetLightingChannelMask();

		DestLight.Flags = Transmission ? PATHTRACER_FLAG_TRANSMISSION_MASK : 0;
		DestLight.Flags |= LightingChannelMask & PATHTRACER_FLAG_LIGHTING_CHANNEL_MASK;
		DestLight.Flags |= Light.LightSceneInfo->Proxy->CastsDynamicShadow() ? PATHTRACER_FLAG_CAST_SHADOW_MASK : 0;
		DestLight.Flags |= Light.LightSceneInfo->Proxy->CastsVolumetricShadow() ? PATHTRACER_FLAG_CAST_VOL_SHADOW_MASK : 0;
		DestLight.IESAtlasIndex = LightParameters.IESAtlasIndex;
		DestLight.MissShaderIndex = 0;

		// these mean roughly the same thing across all light types
		DestLight.Color = FVector3f(LightParameters.Color) * LightParameters.GetLightExposureScale(View.GetLastEyeAdaptationExposure());
		DestLight.TranslatedWorldPosition = FVector3f(LightParameters.WorldPosition + View.ViewMatrices.GetPreViewTranslation());
		DestLight.Normal = -LightParameters.Direction;
		DestLight.dPdu = FVector3f::CrossProduct(LightParameters.Tangent, LightParameters.Direction);
		DestLight.dPdv = LightParameters.Tangent;
		DestLight.Shaping = FVector2f(0.0f, 0.0f);
		DestLight.SpecularScale = LightParameters.SpecularScale;
		DestLight.Attenuation = LightParameters.InvRadius;
		DestLight.FalloffExponent = 0;
		DestLight.VolumetricScatteringIntensity = Light.LightSceneInfo->Proxy->GetVolumetricScatteringIntensity();
		DestLight.RectLightAtlasUVOffset = FVector2f(0.0f, 0.0f);
		DestLight.RectLightAtlasUVScale = FVector2f(0.0f, 0.0f);

		if (RayTracingLightFunctionMap)
		{
			const int32* LightFunctionIndex = RayTracingLightFunctionMap->Find(Light.LightSceneInfo);
			if (LightFunctionIndex)
			{
				DestLight.MissShaderIndex = *LightFunctionIndex;
			}
		}

		switch (LightComponentType)
		{
			case LightType_Rect:
			{
				DestLight.Dimensions = FVector2f(2.0f * LightParameters.SourceRadius, 2.0f * LightParameters.SourceLength);
				DestLight.Shaping = FVector2f(LightParameters.RectLightBarnCosAngle, LightParameters.RectLightBarnLength);
				DestLight.FalloffExponent = LightParameters.FalloffExponent;
				DestLight.Flags |= Light.LightSceneInfo->Proxy->IsInverseSquared() ? 0 : PATHTRACER_FLAG_NON_INVERSE_SQUARE_FALLOFF_MASK;
				DestLight.Flags |= PATHTRACING_LIGHT_RECT;


				// Rect light atlas UV transformation
				DestLight.RectLightAtlasUVOffset = LightParameters.RectLightAtlasUVOffset;
				DestLight.RectLightAtlasUVScale  = LightParameters.RectLightAtlasUVScale;
				if (LightParameters.RectLightAtlasMaxLevel < 16)
				{
					DestLight.Flags |= PATHTRACER_FLAG_HAS_RECT_TEXTURE_MASK;
				}
				break;
			}
			case LightType_Spot:
			{
				DestLight.Dimensions = FVector2f(LightParameters.SourceRadius, LightParameters.SourceLength);
				DestLight.Shaping = LightParameters.SpotAngles;
				DestLight.FalloffExponent = LightParameters.FalloffExponent;
				DestLight.Flags |= Light.LightSceneInfo->Proxy->IsInverseSquared() ? 0 : PATHTRACER_FLAG_NON_INVERSE_SQUARE_FALLOFF_MASK;
				DestLight.Flags |= PATHTRACING_LIGHT_SPOT;
				break;
			}
			case LightType_Point:
			{
				DestLight.Dimensions = FVector2f(LightParameters.SourceRadius, LightParameters.SourceLength);
				DestLight.FalloffExponent = LightParameters.FalloffExponent;
				DestLight.Flags |= Light.LightSceneInfo->Proxy->IsInverseSquared() ? 0 : PATHTRACER_FLAG_NON_INVERSE_SQUARE_FALLOFF_MASK;
				DestLight.Flags |= PATHTRACING_LIGHT_POINT;
				break;
			}
			default:
			{
				// Just in case someone adds a new light type one day ...
				checkNoEntry();
				break;
			}
		}
	}

	PassParameters->SceneLightCount = NumLights;
	{
		// Upload the buffer of lights to the GPU
		uint32 NumCopyLights = FMath::Max(1u, NumLights); // need at least one since zero-sized buffers are not allowed
		size_t DataSize = sizeof(FPathTracingLight) * NumCopyLights;
		PassParameters->SceneLights = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(CreateStructuredBuffer(GraphBuilder, TEXT("PathTracer.LightsBuffer"), sizeof(FPathTracingLight), NumCopyLights, Lights, DataSize, ERDGInitialDataFlags::NoCopy)));
	}

	if (CVarPathTracingVisibleLights.GetValueOnRenderThread() == 1)
	{
		// make all lights in the scene visible
		PassParameters->SceneVisibleLightCount = PassParameters->SceneLightCount;
	}

	PrepareLightGrid(GraphBuilder, View.FeatureLevel, &PassParameters->LightGridParameters, Lights, NumLights, NumInfiniteLights, PassParameters->SceneLights);
}

class FPathTracingCompositorPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FPathTracingCompositorPS)
	SHADER_USE_PARAMETER_STRUCT(FPathTracingCompositorPS, FGlobalShader)

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.CompilerFlags.Add(CFLAG_WarningsAsErrors);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_SAMPLER(SamplerState, VarianceSampler)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float4>, RadianceTexture)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float2>, VarianceTexture)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float4>, NormalDepthTexture)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER(uint32, Iteration)
		SHADER_PARAMETER(uint32, MaxSamples)
		SHADER_PARAMETER(int, ProgressDisplayEnabled)
		SHADER_PARAMETER(float, AdaptiveSamplingErrorThreshold)
		SHADER_PARAMETER(int, AdaptiveSamplingVisualize)
		SHADER_PARAMETER(FIntVector, VarianceTextureDims)

		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_SHADER_TYPE(, FPathTracingCompositorPS, TEXT("/Engine/Private/PathTracing/PathTracingCompositingPixelShader.usf"), TEXT("CompositeMain"), SF_Pixel);

static FPathTracingRG::FPermutationDomain GetPathTracingRGPermutation(const FScene& Scene)
{
	const bool bUseExperimental = CVarPathTracingExperimental.GetValueOnRenderThread() != 0;
	const bool bUseCompaction = (bUseExperimental == false) || CVarPathTracingCompaction.GetValueOnRenderThread() != 0;
	const bool bUseAdaptiveSampling = bUseExperimental && CVarPathTracingAdaptiveSampling.GetValueOnRenderThread() != 0;
	const bool bHasComplexSpecialRenderPath = Substrate::IsSubstrateEnabled() && Scene.SubstrateSceneData.bUsesComplexSpecialRenderPath;

	FPathTracingRG::FPermutationDomain Out;
	Out.Set<FPathTracingRG::FCompactionType>(bUseCompaction);
	Out.Set<FPathTracingRG::FAdaptiveSampling>(bUseAdaptiveSampling);
	Out.Set<FPathTracingRG::FSubstrateComplexSpecialMaterial>(bHasComplexSpecialRenderPath);
	return Out;
}

void FDeferredShadingSceneRenderer::PreparePathTracing(const FSceneViewFamily& ViewFamily, const FScene& Scene, TArray<FRHIRayTracingShader*>& OutRayGenShaders)
{
	if (ViewFamily.EngineShowFlags.PathTracing
		&& ShouldCompilePathTracingShadersForProject(ViewFamily.GetShaderPlatform()))
	{
		// Declare all RayGen shaders that require material closest hit shaders to be bound
		FPathTracingRG::FPermutationDomain PermutationVector = GetPathTracingRGPermutation(Scene);
		{
			auto RayGenShader = GetGlobalShaderMap(ViewFamily.GetShaderPlatform())->GetShader<FPathTracingRG>(PermutationVector);
			OutRayGenShaders.Add(RayGenShader.GetRayTracingShader());
		}

		{
			auto RayGenShader = GetGlobalShaderMap(ViewFamily.GetShaderPlatform())->GetShader<FPathTracingInitExtinctionCoefficientRG>();
			OutRayGenShaders.Add(RayGenShader.GetRayTracingShader());
		}		
	}
}

void PreparePathTracingRTPSO()
{
	if (!IsRayTracingEnabled())
	{
		return;
	}
	ENQUEUE_RENDER_COMMAND(PreparePathTracingRTPSO)([](FRHICommandListImmediate& RHICmdList)
		{
			int NumValidPermutations = 0;
			for (int PermutationId = 0; PermutationId < FPathTracingRG::FPermutationDomain::PermutationCount; PermutationId++)
			{
				FGlobalShaderPermutationParameters Parameters(FPathTracingRG::GetStaticType().GetFName(), GMaxRHIShaderPlatform, PermutationId);
				if (!FPathTracingRG::ShouldCompilePermutation(Parameters))
				{
					// Permutation is not enabled, nothing to pre-compile
					continue;
				}
				FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);

				FPathTracingRG::FPermutationDomain PermutationVector(PermutationId);
				
				FRHIRayTracingShader* RayGenShaderTable[] = {
					ShaderMap->GetShader<FPathTracingRG>(PermutationVector).GetRayTracingShader(),
					ShaderMap->GetShader<FPathTracingInitExtinctionCoefficientRG>().GetRayTracingShader(),
				};
				FRHIRayTracingShader* MissShaderTable[] = {
					GetPathTracingDefaultMissShader(ShaderMap),
				};
				FRHIRayTracingShader* HitGroupTable[] = {
					GetPathTracingDefaultOpaqueHitShader(ShaderMap),
				};
				FRayTracingPipelineStateInitializer Initializer;
				Initializer.bPartial = true; // TODO: getting a crash in nvidia driver when false
				Initializer.SetRayGenShaderTable(RayGenShaderTable);
				Initializer.SetMissShaderTable(MissShaderTable);
				Initializer.SetHitGroupTable(HitGroupTable);
				Initializer.MaxPayloadSizeInBytes = RayGenShaderTable[0]->RayTracingPayloadSize;
				FRayTracingPipelineState* PipelineState = PipelineStateCache::GetAndOrCreateRayTracingPipelineState(RHICmdList, Initializer);
				NumValidPermutations++;
			}
			UE_LOG(LogRenderer, Log, TEXT("Requested compilation of Path Tracing RTPSOs (%d permutations)."), NumValidPermutations);
		}
	);
}

void FSceneViewState::PathTracingInvalidate(bool InvalidateAnimationStates)
{
	FPathTracingState* State = PathTracingState.Get();
	if (State)
	{
		
		if(InvalidateAnimationStates)
		{
			State->LastDenoisedRadianceRT.SafeRelease();
			State->LastRadianceRT.SafeRelease();
			State->LastNormalRT.SafeRelease();
			State->LastAlbedoRT.SafeRelease();
			State->LastVarianceBuffer.SafeRelease();

			State->SpatialTemporalDenoiserHistory.SafeRelease();
		}

		State->RadianceRT.SafeRelease();
		State->VarianceRT.SafeRelease();
		State->AlbedoRT.SafeRelease();
		State->NormalRT.SafeRelease();
		State->VarianceBuffer.SafeRelease();
		State->SampleIndex = 0;

		State->AdaptiveFrustumGridParameterCache.TopLevelGridBuffer.SafeRelease();
	}
}

uint32 FSceneViewState::GetPathTracingSampleIndex() const {
	const FPathTracingState* State = PathTracingState.Get();
	return State ? State->SampleIndex : 0;
}

uint32 FSceneViewState::GetPathTracingSampleCount() const {
	const FPathTracingState* State = PathTracingState.Get();
	return State ? State->LastConfig.PathTracingData.MaxSamples : 0;
}

static void SplitDouble(double x, float* hi, float* lo)
{
	const double SPLIT = 134217729.0; // 2^27+1
	double temp = SPLIT * x;
	*hi = static_cast<float>(temp - (temp - x));
	*lo = static_cast<float>(x - *hi);
}

#if WITH_MGPU
BEGIN_SHADER_PARAMETER_STRUCT(FMGPUTransferParameters, )
	RDG_TEXTURE_ACCESS(InputTexture, ERHIAccess::CopySrc)
	RDG_TEXTURE_ACCESS(InputAlbedo, ERHIAccess::CopySrc)
	RDG_TEXTURE_ACCESS(InputNormal, ERHIAccess::CopySrc)
END_SHADER_PARAMETER_STRUCT()
#endif

DECLARE_GPU_STAT_NAMED(PathTracing, TEXT("Path Tracing"));
DECLARE_GPU_STAT_NAMED(PathTracingPost, TEXT("Path Tracing Post"));
#if WITH_MGPU
DECLARE_GPU_STAT_NAMED(PathTracingCopy, TEXT("Path Tracing Copy"));
#endif

void FDeferredShadingSceneRenderer::RenderPathTracing(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTexturesUniformBuffer,
	FRDGTextureRef SceneColorOutputTexture,
	FRDGTextureRef SceneDepthOutputTexture,
	FPathTracingResources& PathTracingResources)
{
	RDG_EVENT_SCOPE(GraphBuilder, "Path Tracing");

	// To make the GPU profiler work for path tracing with multi-GPU, we need the root GPU profiling scope (marked as "Unaccounted") to be on all GPUs,
	// as the profiler discards events where any event in the hierarchy wasn't on a given GPU.  So in the parent scene render code, we set the GPU mask
	// to "All" when path tracing is enabled, instead of "AllViewsGPUMask".  Then we'll enable that scope inside the path tracer instead.  We also
	// subdivide the profiling scopes inside the path tracer, so the multi-GPU rendering and single-GPU post processing are separate scopes, instead of
	// a scope for the whole path tracer (which would create the same problem).
	RDG_GPU_MASK_SCOPE(GraphBuilder, AllViewsGPUMask);

	if (!ensureMsgf(FDataDrivenShaderPlatformInfo::GetSupportsPathTracing(View.GetShaderPlatform()),
		TEXT("Attempting to use path tracing on unsupported platform.")))
	{
		return;
	}

	if (CVarPathTracing.GetValueOnRenderThread() == 0)
	{
		// Path tracing is not enabled on this project (should not be seen by end-users since the menu entry to pick path tracing should be hidden)
		// If they reach this code through ShowFlag manipulation, they may observe an incomplete image. Is there a way to inform the user here?
		return;
	}

	FPathTracingConfig Config = {};

	// Get current value of MaxSPP and reset render if it has changed
	// NOTE: we ignore the CVar when using offline rendering
	int32 SamplesPerPixelCVar = View.bIsOfflineRender ? -1 : CVarPathTracingSamplesPerPixel.GetValueOnRenderThread();
	uint32 MaxSPP = SamplesPerPixelCVar > -1 ? SamplesPerPixelCVar : View.FinalPostProcessSettings.PathTracingSamplesPerPixel;
	MaxSPP = FMath::Max(MaxSPP, 1u);

	const bool bUseExperimental = CVarPathTracingExperimental.GetValueOnRenderThread() != 0;

	Config.LockedSamplingPattern = CVarPathTracingFrameIndependentTemporalSeed.GetValueOnRenderThread() == 0;
	Config.UseCameraMediumTracking = CVarPathTracingCameraMediumTracking.GetValueOnRenderThread() != 0;
	Config.UseAdaptiveSampling = bUseExperimental && CVarPathTracingAdaptiveSampling.GetValueOnAnyThread() != 0;
	Config.AdaptiveSamplingThreshold = CVarPathTracingAdaptiveSamplingErrorThreshold.GetValueOnRenderThread();

	// compute an integer code of what show flags and booleans related to lights are currently enabled so we can detect changes
	Config.LightShowFlags = 0;
	Config.LightShowFlags |= View.Family->EngineShowFlags.SkyLighting           ? 1 << 0 : 0;
	Config.LightShowFlags |= View.Family->EngineShowFlags.DirectionalLights     ? 1 << 1 : 0;
	Config.LightShowFlags |= View.Family->EngineShowFlags.RectLights            ? 1 << 2 : 0;
	Config.LightShowFlags |= View.Family->EngineShowFlags.SpotLights            ? 1 << 3 : 0;
	Config.LightShowFlags |= View.Family->EngineShowFlags.PointLights           ? 1 << 4 : 0;
	Config.LightShowFlags |= View.Family->EngineShowFlags.TexturedLightProfiles ? 1 << 5 : 0;
	Config.LightShowFlags |= View.Family->EngineShowFlags.LightFunctions        ? 1 << 6 : 0;
	Config.LightShowFlags |= CVarPathTracingLightFunctionColor.GetValueOnRenderThread() ? 1 << 7 : 0;
	// the following flags all mess with diffuse/spec overrides and therefore change the image
	Config.LightShowFlags |= View.Family->EngineShowFlags.Diffuse                    ? 1 << 8 : 0;
	Config.LightShowFlags |= View.Family->EngineShowFlags.Specular                   ? 1 << 9 : 0;
	Config.LightShowFlags |= View.Family->EngineShowFlags.OverrideDiffuseAndSpecular ? 1 << 10 : 0;
	Config.LightShowFlags |= View.Family->EngineShowFlags.LightingOnlyOverride       ? 1 << 11 : 0;
	Config.LightShowFlags |= View.Family->EngineShowFlags.ReflectionOverride         ? 1 << 12 : 0;
	Config.LightShowFlags |= View.Family->EngineShowFlags.SubsurfaceScattering       ? 1 << 13 : 0;
	// the following affects which material shaders get used and therefore change the image
	if (Substrate::IsSubstrateEnabled() && CVarPathTracingSubstrateCompileSimplifiedMaterial.GetValueOnRenderThread() != 0)
	{
		Config.LightShowFlags |= CVarPathTracingSubstrateUseSimplifiedMaterial.GetValueOnRenderThread() != 0 ? 1 << 14 : 0;
	}

	PreparePathTracingData(Scene, View, Config.PathTracingData);

	Config.VisibleLights = CVarPathTracingVisibleLights.GetValueOnRenderThread() != 0;
	Config.UseMISCompensation = Config.PathTracingData.MISMode == 2 && CVarPathTracingMISCompensation.GetValueOnRenderThread() != 0;

	Config.ViewRect = View.ViewRect;

	Config.LightGridResolution = FMath::RoundUpToPowerOfTwo(CVarPathTracingLightGridResolution.GetValueOnRenderThread());
	Config.LightGridMaxCount = FMath::Clamp(CVarPathTracingLightGridMaxCount.GetValueOnRenderThread(), 1, RAY_TRACING_LIGHT_COUNT_MAXIMUM);

	Config.PathTracingData.MaxSamples = MaxSPP;

	bool FirstTime = false;
	if (!View.ViewState->PathTracingState.IsValid())
	{
		View.ViewState->PathTracingState = MakePimpl<FPathTracingState>();
		FirstTime = true; // we just initialized the option state for this view -- don't bother comparing in this case
	}
	check(View.ViewState->PathTracingState.IsValid());
	FPathTracingState* PathTracingState = View.ViewState->PathTracingState.Get();

	if (FirstTime || Config.UseMISCompensation != PathTracingState->LastConfig.UseMISCompensation)
	{
		// if the mode changes we need to rebuild the importance table
		Scene->PathTracingSkylightTexture.SafeRelease();
		Scene->PathTracingSkylightPdf.SafeRelease();
	}

	// if the skylight has changed colors, reset both the path tracer and the importance tables
	if (Scene->SkyLight && Scene->SkyLight->GetEffectiveLightColor() != Scene->PathTracingSkylightColor)
	{
		Scene->PathTracingSkylightTexture.SafeRelease();
		Scene->PathTracingSkylightPdf.SafeRelease();
		// reset last color here as well in case we don't reach PrepareSkyLightTexture
		Scene->PathTracingSkylightColor = Scene->SkyLight->GetEffectiveLightColor();
		if (!View.bIsOfflineRender)
		{
			// reset accumulation, unless this is an offline render, in which case it is ok for the color to evolve
			// across temporal samples
			View.ViewState->PathTracingInvalidate();
		}
		
	}


	// If this is the first sample, recompute the initial medium
	// In this case of an offline render, do this every frame so that motion blur through a boundary is properly accounted for
	FRDGBufferRef StartingExtinctionCoefficient = nullptr;
	if (!Config.UseCameraMediumTracking)
	{
		PathTracingState->StartingExtinctionCoefficient.SafeRelease();
		// camera medium tracking is not enabled - just make a temp buffer and set it to 0
		StartingExtinctionCoefficient = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(float), 3), TEXT("PathTracer.StartingExtinctionCoefficient"));
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(StartingExtinctionCoefficient, PF_R32_FLOAT), 0);
		
	}
	else if (!PathTracingState->StartingExtinctionCoefficient.IsValid() || PathTracingState->SampleIndex == 0 || View.bIsOfflineRender)
	{
		auto RayGenShader = GetGlobalShaderMap(View.FeatureLevel)->GetShader<FPathTracingInitExtinctionCoefficientRG>();

		// prepare extinction coefficient for camera rays
		StartingExtinctionCoefficient = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(float), 3), TEXT("PathTracer.StartingExtinctionCoefficient"));

		FPathTracingInitExtinctionCoefficientRG::FParameters* PassParameters = GraphBuilder.AllocParameters<FPathTracingInitExtinctionCoefficientRG::FParameters>();
		PassParameters->TLAS = Scene->RayTracingScene.GetLayerView(ERayTracingSceneLayer::Base);
		PassParameters->ViewUniformBuffer = View.ViewUniformBuffer;
		PassParameters->RWStartingExtinctionCoefficient = GraphBuilder.CreateUAV(StartingExtinctionCoefficient, PF_R32_FLOAT);

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("Path Tracer Init Sigma"),
			PassParameters,
			ERDGPassFlags::Compute,
			[PassParameters, RayGenShader, &View](FRHIRayTracingCommandList& RHICmdList)
			{
				FRHIRayTracingScene* RayTracingSceneRHI = View.GetRayTracingSceneChecked();

				FRayTracingShaderBindingsWriter GlobalResources;
				SetShaderParameters(GlobalResources, RayGenShader, *PassParameters);
				
				RHICmdList.RayTraceDispatch(
					View.RayTracingMaterialPipeline,
					RayGenShader.GetRayTracingShader(),
					RayTracingSceneRHI, GlobalResources,
					1, 1
				);
			});
		GraphBuilder.QueueBufferExtraction(StartingExtinctionCoefficient, &PathTracingState->StartingExtinctionCoefficient);
	}
	else
	{
		check(PathTracingState->StartingExtinctionCoefficient.IsValid());
		StartingExtinctionCoefficient = GraphBuilder.RegisterExternalBuffer(PathTracingState->StartingExtinctionCoefficient, TEXT("PathTracer.StartingExtinctionCoefficient"));
	}

	// prepare atmosphere optical depth lookup texture (if needed)
	FRDGTexture* AtmosphereOpticalDepthLUT = nullptr;
	if ((Config.PathTracingData.VolumeFlags & PATH_TRACER_VOLUME_ENABLE_ATMOSPHERE) != 0)
	{
		check(Scene->GetSkyAtmosphereSceneInfo() != nullptr);
		check(Scene->GetSkyAtmosphereSceneInfo()->GetAtmosphereShaderParameters() != nullptr);
		FAtmosphereConfig AtmoConfig(*Scene->GetSkyAtmosphereSceneInfo()->GetAtmosphereShaderParameters());
		if (!PathTracingState->AtmosphereOpticalDepthLUT.IsValid() || PathTracingState->LastAtmosphereConfig.IsDifferent(AtmoConfig))
		{
			RDG_GPU_MASK_SCOPE(GraphBuilder, FRHIGPUMask::All());
			PathTracingState->LastAtmosphereConfig = AtmoConfig;
			// need to create a new LUT
			FRDGTextureDesc Desc = FRDGTextureDesc::Create2D(
				FIntPoint(AtmoConfig.Resolution, AtmoConfig.Resolution),
				PF_A32B32G32R32F,
				FClearValueBinding::None,
				TexCreate_ShaderResource | TexCreate_UAV);
			AtmosphereOpticalDepthLUT = GraphBuilder.CreateTexture(Desc, TEXT("PathTracer.AtmosphereOpticalDepthLUT"), ERDGTextureFlags::MultiFrame);
			FPathTracingBuildAtmosphereOpticalDepthLUTCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FPathTracingBuildAtmosphereOpticalDepthLUTCS::FParameters>();
			PassParameters->NumSamples = AtmoConfig.NumSamples;
			PassParameters->Resolution = AtmoConfig.Resolution;
			PassParameters->Atmosphere = Scene->GetSkyAtmosphereSceneInfo()->GetAtmosphereUniformBuffer();
			PassParameters->AtmosphereOpticalDepthLUT = GraphBuilder.CreateUAV(AtmosphereOpticalDepthLUT);
			TShaderMapRef<FPathTracingBuildAtmosphereOpticalDepthLUTCS> ComputeShader(GetGlobalShaderMap(View.FeatureLevel));
			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("Path Tracing Atmosphere Optical Depth LUT (Resolution=%u, NumSamples=%u)", AtmoConfig.Resolution, AtmoConfig.NumSamples),
				ComputeShader,
				PassParameters,
				FComputeShaderUtils::GetGroupCount(
					FIntPoint(AtmoConfig.Resolution, AtmoConfig.Resolution),
					FIntPoint(FComputeShaderUtils::kGolden2DGroupSize, FComputeShaderUtils::kGolden2DGroupSize))
			);
			GraphBuilder.QueueTextureExtraction(AtmosphereOpticalDepthLUT, &PathTracingState->AtmosphereOpticalDepthLUT);
		}
		else
		{
			AtmosphereOpticalDepthLUT = GraphBuilder.RegisterExternalTexture(PathTracingState->AtmosphereOpticalDepthLUT, TEXT("PathTracer.AtmosphereOpticalDepthLUT"));
		}
	}
	else
	{
		AtmosphereOpticalDepthLUT = GraphBuilder.RegisterExternalTexture(GSystemTextures.BlackDummy);
	}
#if WITH_MGPU
	Config.UseMultiGPU = CVarPathTracingMultiGPU.GetValueOnRenderThread() != 0;
	// TODO: Figure out how to support adaptive sampling in multi-gpu cases (this is complicated due to the swizzled layout of the variance texture)
	Config.UseMultiGPU &= !Config.UseAdaptiveSampling;
#else
	Config.UseMultiGPU = false;
#endif

	// If the scene has changed in some way (camera move, object movement, etc ...)
	// we must invalidate the ViewState to start over from scratch
	// NOTE: only check things like hair position changes for interactive viewports, for offline renders we don't want any chance of mid-render invalidation
	// NOTE: same for DOF changes, these parameters could be animated which should not automatically invalidate a render in progress
	if (FirstTime ||
		Config.IsDifferent(PathTracingState->LastConfig) ||
		(!View.bIsOfflineRender && Config.IsDOFDifferent(PathTracingState->LastConfig)) ||
		(!View.bIsOfflineRender && HairStrands::HasPositionsChanged(GraphBuilder, *Scene, View)))
	{
		// remember the options we used for next time
		PathTracingState->LastConfig = Config;
		View.ViewState->PathTracingInvalidate();
	}

	// Declare heterogeneous volume buffers
	TRDGUniformBufferRef<FOrthoVoxelGridUniformBufferParameters> OrthoGridUniformBuffer;
	TRDGUniformBufferRef<FFrustumVoxelGridUniformBufferParameters> FrustumGridUniformBuffer;
	bool bCreateVolumeGrids = false;

	// Prepare radiance buffer (will be shared with display pass)
	FRDGTexture* RadianceTexture = nullptr;
	FRDGTexture* VarianceTexture = nullptr;
	FRDGTexture* AlbedoTexture = nullptr;
	FRDGTexture* NormalTexture = nullptr;
	const int NumVarianceMips = FMath::Min(5u, 1 + FMath::FloorLog2(uint32(View.ViewRect.Size().GetMin())));
	if (PathTracingState->RadianceRT)
	{
		// we already have a valid radiance texture, re-use it
		RadianceTexture = GraphBuilder.RegisterExternalTexture(PathTracingState->RadianceRT, TEXT("PathTracer.Radiance"));
		AlbedoTexture   = GraphBuilder.RegisterExternalTexture(PathTracingState->AlbedoRT, TEXT("PathTracer.Albedo"));
		NormalTexture   = GraphBuilder.RegisterExternalTexture(PathTracingState->NormalRT, TEXT("PathTracer.Normal"));
	}
	else
	{
		// First time through, need to make a new texture
		FRDGTextureDesc Desc = FRDGTextureDesc::Create2D(
			View.ViewRect.Size(),
			PF_A32B32G32R32F,
			FClearValueBinding::None,
			TexCreate_ShaderResource | TexCreate_UAV | GetExtraTextureCreateFlagsForDenoiser());
		RadianceTexture = GraphBuilder.CreateTexture(Desc, TEXT("PathTracer.Radiance"), ERDGTextureFlags::MultiFrame);
		AlbedoTexture   = GraphBuilder.CreateTexture(Desc, TEXT("PathTracer.Albedo")  , ERDGTextureFlags::MultiFrame);
		NormalTexture   = GraphBuilder.CreateTexture(Desc, TEXT("PathTracer.Normal")  , ERDGTextureFlags::MultiFrame);
	}
	if (Config.UseAdaptiveSampling)
	{
		if (PathTracingState->VarianceRT)
		{
			VarianceTexture = GraphBuilder.RegisterExternalTexture(PathTracingState->VarianceRT, TEXT("PathTracer.Variance"));
		}
		else
		{
			// format stores Luminance,Luminance^2,NumSamples which can be used for error estimation
			FRDGTextureDesc Desc = FRDGTextureDesc::Create2D(
				View.ViewRect.Size(),
				PF_A32B32G32R32F,
				FClearValueBinding::None,
				TexCreate_ShaderResource | TexCreate_UAV);
			Desc.NumMips = NumVarianceMips;
			VarianceTexture = GraphBuilder.CreateTexture(Desc, TEXT("PathTracer.Variance"), ERDGTextureFlags::MultiFrame);
		}
	}
	else
	{
		// If we are not using adaptive, make sure the old variance buffer doesn't stick around
		PathTracingState->VarianceRT.SafeRelease();
	}


	// should we use multiple GPUs to render the image?
	const FRHIGPUMask GPUMask = Config.UseMultiGPU ? FRHIGPUMask::All() : View.GPUMask;
	const int32 NumGPUs = GPUMask.GetNumActive();
	const int32 DispatchResX = View.ViewRect.Size().X;
	const int32 DispatchResY = View.ViewRect.Size().Y;
	const int32 DispatchSize = FMath::Max(CVarPathTracingDispatchSize.GetValueOnRenderThread(), 64);

	// When running with multiple GPUs, do that number of passes per frame, to keep the GPU work done per frame consistent
	// (given that each GPU processes a fraction of the pixels), but get the job done in fewer frames.
#if WITH_MGPU
	const int32 FramePassCount = !View.bIsOfflineRender && CVarPathTracingAdjustMultiGPUPasses.GetValueOnRenderThread() ? NumGPUs : 1;
#else
	const int32 FramePassCount = 1;
#endif

	bool bNeedsMoreRays = false;
	bool bNeedsTextureExtract = false;

	for (int32 FramePassIndex = 0; FramePassIndex < FramePassCount; FramePassIndex++)
	{
		// Setup temporal seed _after_ invalidation in case we got reset
		if (Config.LockedSamplingPattern)
		{
			// Count samples from 0 for deterministic results
			Config.PathTracingData.TemporalSeed = PathTracingState->SampleIndex;
		}
		else
		{
			// Count samples from an ever-increasing counter to avoid screen-door effect
			Config.PathTracingData.TemporalSeed = PathTracingState->FrameIndex;
		}
		Config.PathTracingData.Iteration = PathTracingState->SampleIndex;
		Config.PathTracingData.BlendFactor = 1.0f / (Config.PathTracingData.Iteration + 1);

		bNeedsMoreRays = Config.PathTracingData.Iteration < MaxSPP;

		if (bNeedsMoreRays)
		{
			RDG_EVENT_SCOPE(GraphBuilder, "Path Tracing Compute (%d x %d)", DispatchResX, DispatchResY);
			bool bForceRebuild = CVarPathTracingHeterogeneousVolumesRebuildEveryFrame.GetValueOnRenderThread() != 0;
			bCreateVolumeGrids = bForceRebuild ||
				!PathTracingState->AdaptiveFrustumGridParameterCache.TopLevelGridBuffer ||
				!PathTracingState->AdaptiveOrthoGridParameterCache.TopLevelGridBuffer;
			if (bCreateVolumeGrids)
			{
				FVoxelGridBuildOptions BuildOptions;
				BuildOrthoVoxelGrid(GraphBuilder, Scene, Views, VisibleLightInfos, BuildOptions, OrthoGridUniformBuffer);
				BuildFrustumVoxelGrid(GraphBuilder, Scene, Views[0], BuildOptions, FrustumGridUniformBuffer);
			}
			else
			{
				RegisterExternalOrthoVoxelGridUniformBuffer(GraphBuilder,
					PathTracingState->AdaptiveOrthoGridParameterCache,
					OrthoGridUniformBuffer
				);

				RegisterExternalFrustumVoxelGridUniformBuffer(GraphBuilder,
					PathTracingState->AdaptiveFrustumGridParameterCache,
					FrustumGridUniformBuffer
				);
			}

			// We are writing to the texture, we'll need to extract it...
			bNeedsTextureExtract = true;

			// should we use path compaction?
			const bool bUseCompaction = (bUseExperimental == false) || CVarPathTracingCompaction.GetValueOnRenderThread() != 0;
			const bool bUseIndirectDispatch = GRHISupportsRayTracingDispatchIndirect && CVarPathTracingIndirectDispatch.GetValueOnRenderThread() != 0;
			const int FlushRenderingCommands = CVarPathTracingFlushDispatch.GetValueOnRenderThread();

			FRDGBuffer* ActivePaths[2] = {};
			FRDGBuffer* NumActivePaths[2] = {};
			FRDGBuffer* PathStateData = nullptr;
			if (bUseCompaction || Config.UseAdaptiveSampling)
			{
				const int32 NumPaths = FMath::Min(
					DispatchSize * FMath::DivideAndRoundUp(DispatchSize, NumGPUs),
					DispatchResX * FMath::DivideAndRoundUp(DispatchResY, NumGPUs)
				);
				ActivePaths[0] = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(int32), NumPaths), TEXT("PathTracer.ActivePaths0"));
				ActivePaths[1] = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(int32), NumPaths), TEXT("PathTracer.ActivePaths1"));
				if (bUseIndirectDispatch)
				{
					NumActivePaths[0] = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<int32>(3), TEXT("PathTracer.NumActivePaths0"));
					NumActivePaths[1] = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<int32>(3), TEXT("PathTracer.NumActivePaths1"));
				}
				else
				{
					NumActivePaths[0] = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(int32), 3), TEXT("PathTracer.NumActivePaths"));
				}
				PathStateData = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(FPathTracingPackedPathState), NumPaths), TEXT("PathTracer.PathStateData"));
			}

			TShaderMapRef<FPathTracingRG> RayGenShader(View.ShaderMap, GetPathTracingRGPermutation(*Scene));
			FPathTracingRG::FParameters* PreviousPassParameters = nullptr;
			// Divide each tile among all the active GPUs (interleaving scanlines)
			// The assumption is that the tiles are as big as possible, hopefully covering the entire screen
			// so rather than dividing tiles among GPUs, we divide each tile among all GPUs
			int32 CurrentGPU = 0; // keep our own counter so that we don't assume the assigned GPUs in the view mask are sequential
			for (int32 GPUIndex : GPUMask)
			{
				RDG_GPU_MASK_SCOPE(GraphBuilder, FRHIGPUMask::FromIndex(GPUIndex));
				RDG_EVENT_SCOPE_CONDITIONAL(GraphBuilder, NumGPUs > 1, "Path Tracing GPU%d", GPUIndex);
#if WITH_MGPU
				RDG_GPU_STAT_SCOPE(GraphBuilder, PathTracing);
#endif
				for (int32 TileY = 0; TileY < DispatchResY; TileY += DispatchSize)
				{
					for (int32 TileX = 0; TileX < DispatchResX; TileX += DispatchSize)
					{
						const int32 DispatchSizeX = FMath::Min(DispatchSize, DispatchResX - TileX);
						const int32 DispatchSizeY = FMath::Min(DispatchSize, DispatchResY - TileY);

						const int32 DispatchSizeYSplit = FMath::DivideAndRoundUp(DispatchSizeY, NumGPUs);

						// Compute the dispatch size for just this set of scanlines
						const int32 DispatchSizeYLocal = FMath::Min(DispatchSizeYSplit, DispatchSizeY - CurrentGPU * DispatchSizeYSplit);

						RDG_EVENT_SCOPE_CONDITIONAL(GraphBuilder, DispatchResX > DispatchSize || DispatchResY > DispatchSize, "Tile=(%d,%d - %dx%d)", TileX, TileY, DispatchSizeX, DispatchSizeYLocal);

						if (Config.UseAdaptiveSampling && Config.PathTracingData.Iteration > 0)
						{
							// If we are using adaptive sampling, build a smaller list of active paths after the first iteration
							TShaderMapRef<FPathTracingAdaptiveStartCS> ComputeShader(GetGlobalShaderMap(View.FeatureLevel));

							FPathTracingAdaptiveStartCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FPathTracingAdaptiveStartCS::FParameters>();

							PassParameters->VarianceTexture = GraphBuilder.CreateSRV(VarianceTexture);
							PassParameters->VarianceSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
							PassParameters->VarianceTextureDims = FIntVector(DispatchResX, DispatchResY, NumVarianceMips);
							PassParameters->AdaptiveSamplingErrorThreshold = Config.AdaptiveSamplingThreshold;
							PassParameters->ViewPreExposure = View.PreExposure;

							PassParameters->NextActivePaths = GraphBuilder.CreateUAV(ActivePaths[0], PF_R32_SINT);
							PassParameters->NumPathStates = GraphBuilder.CreateUAV(NumActivePaths[0], PF_R32_UINT);
							AddClearUAVPass(GraphBuilder, PassParameters->NextActivePaths, -1); // make sure everything is initialized to -1 since paths that go inactive don't write anything
							AddClearUAVPass(GraphBuilder, PassParameters->NumPathStates, 0);

							PassParameters->TileTextureOffset.X = TileX;
							PassParameters->TileTextureOffset.Y = TileY + CurrentGPU * DispatchSizeYSplit;
							PassParameters->DispatchDim = FIntPoint(DispatchSizeX, DispatchSizeYLocal);

							FComputeShaderUtils::AddPass(
								GraphBuilder,
								RDG_EVENT_NAME("Prepare Adaptive Sampling Mask"),
								ComputeShader,
								PassParameters,
								FComputeShaderUtils::GetGroupCount(PassParameters->DispatchDim, FComputeShaderUtils::kGolden2DGroupSize));
						}
						else if (bUseCompaction)
						{
							AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(ActivePaths[0], PF_R32_UINT), 0);
						}

						// When using path compaction, we need to run the path tracer once per bounce
						// otherwise, the path tracer is the one doing the bounces
						for (int Bounce = 0, MaxBounces = bUseCompaction ? Config.PathTracingData.MaxBounces : 0; Bounce <= MaxBounces; Bounce++)
						{
							FPathTracingRG::FParameters* PassParameters = GraphBuilder.AllocParameters<FPathTracingRG::FParameters>();
							PassParameters->TLAS = Scene->RayTracingScene.GetLayerView(ERayTracingSceneLayer::Base);
							PassParameters->DecalTLAS = Scene->RayTracingScene.GetLayerView(ERayTracingSceneLayer::Decals);
							PassParameters->ViewUniformBuffer = View.ViewUniformBuffer;
							PassParameters->PathTracingData = Config.PathTracingData;
							PassParameters->StartingExtinctionCoefficient = GraphBuilder.CreateSRV(StartingExtinctionCoefficient, PF_R32_FLOAT);
							if (PreviousPassParameters == nullptr)
							{
								// upload sky/lights data
								RDG_GPU_MASK_SCOPE(GraphBuilder, GPUMask); // make sure this happens on all GPUs we will be rendering on
								SetLightParameters(GraphBuilder, PassParameters, Scene, View, Config.UseMISCompensation);
							}
							else
							{
								// re-use from last iteration
								PassParameters->LightGridParameters = PreviousPassParameters->LightGridParameters;
								PassParameters->SceneLightCount = PreviousPassParameters->SceneLightCount;
								PassParameters->SceneVisibleLightCount = PreviousPassParameters->SceneVisibleLightCount;
								PassParameters->SceneLights = PreviousPassParameters->SceneLights;
								PassParameters->SkylightParameters = PreviousPassParameters->SkylightParameters;
							}
							PassParameters->DecalParameters = View.RayTracingDecalUniformBuffer;

							PassParameters->RadianceTexture = GraphBuilder.CreateUAV(RadianceTexture);
							PassParameters->AlbedoTexture = GraphBuilder.CreateUAV(AlbedoTexture);
							PassParameters->NormalTexture = GraphBuilder.CreateUAV(NormalTexture);

							if (Config.UseAdaptiveSampling)
							{
								PassParameters->VarianceTexture = GraphBuilder.CreateUAV(VarianceTexture);
							}
							else
							{
								// this texture is not used in this case
								PassParameters->VarianceTexture = nullptr;
							}
							

							if (PreviousPassParameters != nullptr)
							{
								PassParameters->Atmosphere = PreviousPassParameters->Atmosphere;
								PassParameters->PlanetCenterTranslatedWorldHi = PreviousPassParameters->PlanetCenterTranslatedWorldHi;
								PassParameters->PlanetCenterTranslatedWorldLo = PreviousPassParameters->PlanetCenterTranslatedWorldLo;
							}
							else if ((Config.PathTracingData.VolumeFlags & PATH_TRACER_VOLUME_ENABLE_ATMOSPHERE) != 0)
							{
								PassParameters->Atmosphere = Scene->GetSkyAtmosphereSceneInfo()->GetAtmosphereUniformBuffer();
								FVector PlanetCenterTranslatedWorld = Scene->GetSkyAtmosphereSceneInfo()->GetSkyAtmosphereSceneProxy().GetAtmosphereSetup().PlanetCenterKm * double(FAtmosphereSetup::SkyUnitToCm) + View.ViewMatrices.GetPreViewTranslation();
								SplitDouble(PlanetCenterTranslatedWorld.X, &PassParameters->PlanetCenterTranslatedWorldHi.X, &PassParameters->PlanetCenterTranslatedWorldLo.X);
								SplitDouble(PlanetCenterTranslatedWorld.Y, &PassParameters->PlanetCenterTranslatedWorldHi.Y, &PassParameters->PlanetCenterTranslatedWorldLo.Y);
								SplitDouble(PlanetCenterTranslatedWorld.Z, &PassParameters->PlanetCenterTranslatedWorldHi.Z, &PassParameters->PlanetCenterTranslatedWorldLo.Z);
							}
							else
							{
								FAtmosphereUniformShaderParameters AtmosphereParams = {};
								PassParameters->Atmosphere = CreateUniformBufferImmediate(AtmosphereParams, EUniformBufferUsage::UniformBuffer_SingleFrame);
								PassParameters->PlanetCenterTranslatedWorldHi = FVector3f(0);
								PassParameters->PlanetCenterTranslatedWorldLo = FVector3f(0);
							}
							PassParameters->AtmosphereOpticalDepthLUT = AtmosphereOpticalDepthLUT;
							PassParameters->AtmosphereOpticalDepthLUTSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

							if ((Config.PathTracingData.VolumeFlags & PATH_TRACER_VOLUME_ENABLE_FOG) != 0)
							{
								PassParameters->FogParameters = PrepareFogParameters(View, Scene->ExponentialFogs[0]);
							}
							else
							{
								PassParameters->FogParameters = {};
							}

							// Heterogeneous volume bindings
							PassParameters->OrthoGridUniformBuffer = OrthoGridUniformBuffer;
							PassParameters->FrustumGridUniformBuffer = FrustumGridUniformBuffer;

							PassParameters->TilePixelOffset.X = TileX;
							PassParameters->TilePixelOffset.Y = TileY + CurrentGPU;
							PassParameters->TileTextureOffset.X = TileX;
							PassParameters->TileTextureOffset.Y = TileY + CurrentGPU * DispatchSizeYSplit;
							PassParameters->ScanlineStride = NumGPUs;
							PassParameters->ScanlineWidth = DispatchSizeX;

							PassParameters->Bounce = Bounce;
							if (bUseCompaction)
							{
								PassParameters->ActivePaths = GraphBuilder.CreateSRV(ActivePaths[Bounce & 1], PF_R32_SINT);
								PassParameters->NextActivePaths = GraphBuilder.CreateUAV(ActivePaths[(Bounce & 1) ^ 1], PF_R32_SINT);
								PassParameters->PathStateData = GraphBuilder.CreateUAV(PathStateData);
								if (bUseIndirectDispatch)
								{
									PassParameters->NumPathStates = GraphBuilder.CreateUAV(NumActivePaths[Bounce & 1], PF_R32_UINT);
									PassParameters->PathTracingIndirectArgs = NumActivePaths[(Bounce & 1) ^ 1];
								}
								else
								{
									PassParameters->NumPathStates = GraphBuilder.CreateUAV(NumActivePaths[0], PF_R32_UINT);
									AddClearUAVPass(GraphBuilder, PassParameters->NextActivePaths, -1); // make sure everything is initialized to -1 since paths that go inactive don't write anything
								}
								AddClearUAVPass(GraphBuilder, PassParameters->NumPathStates, 0);
							}
							ClearUnusedGraphResources(RayGenShader, PassParameters);
							const bool bFlushRenderingCommands = FlushRenderingCommands == 1 || (FlushRenderingCommands == 2 && Bounce == MaxBounces);
							GraphBuilder.AddPass(
								bUseCompaction
								? RDG_EVENT_NAME("Path Tracer Sample=%d/%d NumLights=%d (Bounce=%d%s)", PathTracingState->SampleIndex, MaxSPP, PassParameters->SceneLightCount, Bounce, bUseIndirectDispatch && Bounce > 0 ? TEXT(" indirect") : TEXT(""))
								: RDG_EVENT_NAME("Path Tracer Sample=%d/%d NumLights=%d"              , PathTracingState->SampleIndex, MaxSPP, PassParameters->SceneLightCount),
								PassParameters,
								ERDGPassFlags::Compute,
								[PassParameters, RayGenShader, DispatchSizeX, DispatchSizeYLocal, bUseIndirectDispatch, bFlushRenderingCommands, GPUIndex, &View](FRHIRayTracingCommandList& RHICmdList)
								{
									FRHIRayTracingScene* RayTracingSceneRHI = View.GetRayTracingSceneChecked();

									FRayTracingShaderBindingsWriter GlobalResources;
									SetShaderParameters(GlobalResources, RayGenShader, *PassParameters);
									if (bUseIndirectDispatch && PassParameters->Bounce > 0)
									{
										PassParameters->PathTracingIndirectArgs->MarkResourceAsUsed();
										RHICmdList.RayTraceDispatchIndirect(
											View.RayTracingMaterialPipeline,
											RayGenShader.GetRayTracingShader(),
											RayTracingSceneRHI, GlobalResources,
											PassParameters->PathTracingIndirectArgs->GetIndirectRHICallBuffer(), 0
										);
									}
									else
									{
										RHICmdList.RayTraceDispatch(
											View.RayTracingMaterialPipeline,
											RayGenShader.GetRayTracingShader(),
											RayTracingSceneRHI, GlobalResources,
											DispatchSizeX, DispatchSizeYLocal
										);
									}
									if (bFlushRenderingCommands)
									{
										RHICmdList.SubmitCommandsHint();
									}
								});
							if (PreviousPassParameters == nullptr)
							{
								PreviousPassParameters = PassParameters;
							}
						}
					}
				}
				++CurrentGPU;
			}

			// Bump counters for next frame pass
			++PathTracingState->SampleIndex;
			++PathTracingState->FrameIndex;
		}
	}

	if (bNeedsTextureExtract)
	{
#if WITH_MGPU
		if (NumGPUs > 1)
		{
			// Need fences to prevent cross GPU copies from overlapping with rendering to the same buffers
			TArray<FTransferResourceFenceData*> CopyFenceDatas;
			CopyFenceDatas.AddUninitialized(NumGPUs - 1);
			for (int32 FenceIndex = 0; FenceIndex < NumGPUs - 1; FenceIndex++)
			{
				CopyFenceDatas[FenceIndex] = RHICreateTransferResourceFenceData();
			}

			{
				// Signal that the first GPU is done rendering, and other GPUs can copy to the buffer now.  Get all the GPUs
				// besides the first GPU into a mask.  These are the source GPUs for copies to the first GPU.
				FRHIGPUMask SrcGPUMask = FRHIGPUMask::FromIndex(GPUMask.GetLastIndex());
				for (uint32 SrcGPUIndex : GPUMask)
				{
					if (SrcGPUIndex != GPUMask.GetFirstIndex())
					{
						SrcGPUMask |= FRHIGPUMask::FromIndex(SrcGPUIndex);
					}
				}

				// Signal goes from first GPU (destination of copy), to remaining GPUs (sources of copy).
				RDG_GPU_MASK_SCOPE(GraphBuilder, FRHIGPUMask::FromIndex(GPUMask.GetFirstIndex()));
				GraphBuilder.AddPass(
					RDG_EVENT_NAME("Path Tracer Cross-GPU Signal (%d GPUs)", NumGPUs),
					ERDGPassFlags::None,
					[this, LocalCopyFenceDatas = CopyTemp(CopyFenceDatas), SrcGPUMask](FRHICommandListImmediate& RHICmdList)
				{
					RHICmdList.TransferResourceSignal(LocalCopyFenceDatas, SrcGPUMask);
				});
			}

			// Treat the cross GPU copy as occurring on all GPUs, for profiling purposes.  Internally, the cross GPU transfer doesn't
			// pay attention to the mask, so it has no effect on behavior.  Technically the work of the copy is done on the second GPU,
			// and the first GPU stalls waiting on that, so it's useful to show this interval on both GPUs.
			RDG_GPU_MASK_SCOPE(GraphBuilder, GPUMask);
			RDG_GPU_STAT_SCOPE(GraphBuilder, PathTracingCopy);

			FMGPUTransferParameters* Parameters = GraphBuilder.AllocParameters<FMGPUTransferParameters>();
			Parameters->InputTexture = RadianceTexture;
			Parameters->InputAlbedo = AlbedoTexture;
			Parameters->InputNormal = NormalTexture;
			GraphBuilder.AddPass(RDG_EVENT_NAME("Path Tracer Cross-GPU Transfer (%d GPUs)", NumGPUs), Parameters, ERDGPassFlags::Readback,
				[Parameters, DispatchResX, DispatchResY, DispatchSize, GPUMask, MainGPUMask = View.GPUMask, LocalCopyFenceDatas = MoveTemp(CopyFenceDatas)](FRHICommandListImmediate& RHICmdList)
				{
					const int32 FirstGPUIndex = MainGPUMask.GetFirstIndex();
					const int32 NumGPUs = GPUMask.GetNumActive();
					TArray<FTransferResourceParams> TransferParams;
					for (int32 TileY = 0; TileY < DispatchResY; TileY += DispatchSize)
					{
						for (int32 TileX = 0; TileX < DispatchResX; TileX += DispatchSize)
						{
							const int32 DispatchSizeX = FMath::Min(DispatchSize, DispatchResX - TileX);
							const int32 DispatchSizeY = FMath::Min(DispatchSize, DispatchResY - TileY);

							const int32 DispatchSizeYSplit = FMath::DivideAndRoundUp(DispatchSizeY, NumGPUs);

							// Divide each tile among all the active GPUs (interleaving scanlines)
							// The assumption is that the tiles are as big as possible, hopefully covering the entire screen
							// so rather than dividing tiles among GPUs, we divide each tile among all GPUs
							int32 CurrentGPU = 0; // keep our own counter so that we don't assume the assigned GPUs in the view mask are sequential
							for (int32 GPUIndex : GPUMask)
							{
								// Compute the dispatch size for just this set of scanlines
								const int32 DispatchSizeYLocal = FMath::Min(DispatchSizeYSplit, DispatchSizeY - CurrentGPU * DispatchSizeYSplit);
								// If this portion of the texture was not rendered by GPU0, transfer the rendered pixels there
								if (GPUIndex != FirstGPUIndex)
								{
									FIntRect TileToCopy;
									TileToCopy.Min.X = TileX;
									TileToCopy.Min.Y = TileY + CurrentGPU * DispatchSizeYSplit;
									TileToCopy.Max.X = TileX + DispatchSizeX;
									TileToCopy.Max.Y = TileToCopy.Min.Y + DispatchSizeYLocal;
									TransferParams.Emplace(Parameters->InputTexture->GetRHI(), TileToCopy, GPUIndex, FirstGPUIndex, false, false);
									TransferParams.Emplace(Parameters->InputAlbedo->GetRHI(), TileToCopy, GPUIndex, FirstGPUIndex, false, false);
									TransferParams.Emplace(Parameters->InputNormal->GetRHI(), TileToCopy, GPUIndex, FirstGPUIndex, false, false);
								}
								++CurrentGPU;
							}
						}
					}

					// Include the fences we need to wait on in our list of transfers
					check(TransferParams.Num() >= LocalCopyFenceDatas.Num());
					for (int32 FenceIndex = 0; FenceIndex < LocalCopyFenceDatas.Num(); FenceIndex++)
					{
						TransferParams[FenceIndex].PreTransferFence = LocalCopyFenceDatas[FenceIndex];
					}

					RHICmdList.TransferResources(TransferParams);
				}
			);
		}
#endif
		// After we are done, make sure we remember our texture for next time so that we can accumulate samples across frames
		GraphBuilder.QueueTextureExtraction(RadianceTexture, &PathTracingState->RadianceRT);
		GraphBuilder.QueueTextureExtraction(AlbedoTexture, &PathTracingState->AlbedoRT);
		GraphBuilder.QueueTextureExtraction(NormalTexture, &PathTracingState->NormalRT);
		if (Config.UseAdaptiveSampling)
		{
			check(VarianceTexture != nullptr);
			GraphBuilder.QueueTextureExtraction(VarianceTexture, &PathTracingState->VarianceRT);
		}
	}

	if (bCreateVolumeGrids)
	{
		ExtractOrthoVoxelGridUniformBuffer(GraphBuilder, OrthoGridUniformBuffer, PathTracingState->AdaptiveOrthoGridParameterCache);
		ExtractFrustumVoxelGridUniformBuffer(GraphBuilder, FrustumGridUniformBuffer, PathTracingState->AdaptiveFrustumGridParameterCache);
	}

	RDG_GPU_MASK_SCOPE(GraphBuilder, View.GPUMask);
	RDG_GPU_STAT_SCOPE(GraphBuilder, PathTracingPost);

	// Figure out if the denoiser is enabled and needs to run
	FRDGTexture* DenoisedRadianceTexture = nullptr;
	bool IsDenoiserEnabled = IsPathTracingDenoiserEnabled(View);
	int DenoiserMode = GetPathTracingDenoiserMode(View);

	// Request denoise if this is the last sample OR allow turning on the denoiser after the image has stopped accumulating samples
	const bool NeedsDenoise = IsDenoiserEnabled &&
		(((Config.PathTracingData.Iteration + 1) == MaxSPP) ||
		 (!bNeedsMoreRays && DenoiserMode != PathTracingState->LastConfig.DenoiserMode));

#if WITH_MGPU
	if (NumGPUs > 1)
	{
		// mGPU renders blocks of pixels that need to be mapped back into alternating scanlines
		// perform this swizzling now with a simple compute shader

		TShaderMapRef<FPathTracingSwizzleScanlinesCS> ComputeShader(GetGlobalShaderMap(View.FeatureLevel));
		FRDGTextureDesc Desc = FRDGTextureDesc::Create2D(
			View.ViewRect.Size(),
			PF_A32B32G32R32F,
			FClearValueBinding::None,
			TexCreate_ShaderResource | TexCreate_UAV);
		FRDGTexture* NewRadianceTexture = GraphBuilder.CreateTexture(Desc, TEXT("PathTracer.RadianceUnswizzled"));
		FRDGTexture* NewNormalTexture = GraphBuilder.CreateTexture(Desc, TEXT("PathTracer.NormalUnswizzled"));
		FRDGTexture* NewAlbedoTexture = NeedsDenoise ? GraphBuilder.CreateTexture(Desc, TEXT("PathTracer.AlbedoUnswizzled")) : nullptr;

		FRDGTexture* InputTextures[3] = { RadianceTexture, NormalTexture, AlbedoTexture};
		FRDGTexture* OutputTextures[3] = { NewRadianceTexture, NewNormalTexture, NewAlbedoTexture};
		for (int Index = 0, Num = NeedsDenoise ? 3 : 2; Index < Num; Index++)
		{
			FPathTracingSwizzleScanlinesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FPathTracingSwizzleScanlinesCS::FParameters>();
			PassParameters->DispatchDim.X = DispatchResX;
			PassParameters->DispatchDim.Y = DispatchResY;
			PassParameters->TileSize.X = DispatchSize;
			PassParameters->TileSize.Y = DispatchSize;
			PassParameters->ScanlineStride = NumGPUs;
			PassParameters->InputTexture = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(InputTextures[Index]));
			PassParameters->OutputTexture = GraphBuilder.CreateUAV(OutputTextures[Index]);
			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("UnswizzleScanlines(%d)", Index),
				ComputeShader,
				PassParameters,
				FComputeShaderUtils::GetGroupCount(FIntPoint(DispatchResX, DispatchResY), FComputeShaderUtils::kGolden2DGroupSize));
		}

		// let the remaining code operate on the unswizzled textures
		RadianceTexture = NewRadianceTexture;
		NormalTexture = NewNormalTexture;
		AlbedoTexture = NewAlbedoTexture;
	}
#endif

	// build adaptive sampling error map if we traced some rays
	if (Config.UseAdaptiveSampling && bNeedsMoreRays)
	{
		RDG_EVENT_SCOPE(GraphBuilder, "Adaptive Sampling");
		FIntPoint BufferSize = View.ViewRect.Size();
		TShaderMapRef<FPathTracingBuildAdaptiveErrorTextureCS> ComputeShader(GetGlobalShaderMap(View.FeatureLevel));
		for (int MipLevel = 0; MipLevel < NumVarianceMips - 1; MipLevel++)
		{
			FPathTracingBuildAdaptiveErrorTextureCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FPathTracingBuildAdaptiveErrorTextureCS::FParameters>();

			PassParameters->InputMipSampler = TStaticSamplerState<ESamplerFilter::SF_Bilinear>::CreateRHI();
			PassParameters->InputMip = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForMipLevel(VarianceTexture, MipLevel));
			PassParameters->OutputMip = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(VarianceTexture, MipLevel + 1));
			PassParameters->InputResolution = FIntPoint(
				FMath::Max(BufferSize.X >> MipLevel, 1),
				FMath::Max(BufferSize.Y >> MipLevel, 1));
			PassParameters->OutputResolution = FIntPoint(
				FMath::Max(BufferSize.X >> (MipLevel + 1), 1),
				FMath::Max(BufferSize.Y >> (MipLevel + 1), 1));
			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("Build Error Estimation Mips (%d)", MipLevel),
				ComputeShader,
				PassParameters,
				FComputeShaderUtils::GetGroupCount(PassParameters->OutputResolution, FComputeShaderUtils::kGolden2DGroupSize));
		}
	}

	FPathTracingSpatialTemporalDenoisingContext DenoisingContext = {};
	DenoisingContext.SpatialTemporalDenoiserHistory = PathTracingState->SpatialTemporalDenoiserHistory;
	const bool EnablePathTracingDenoiserRealtimeDebug = ShouldEnablePathTracingDenoiserRealtimeDebug();

	if (IsDenoiserEnabled)
	{	
		if (PathTracingState->LastDenoisedRadianceRT)
		{
			// we already have a texture for this
			DenoisedRadianceTexture = GraphBuilder.RegisterExternalTexture(PathTracingState->LastDenoisedRadianceRT, TEXT("PathTracer.DenoisedRadiance"));
		}

		// 1. Prepass to estimate pixel variance
		FRDGBuffer* CurrentVarianceBufer = nullptr;
		{
			DenoisingContext.RadianceTexture = RadianceTexture;
			DenoisingContext.AlbedoTexture = AlbedoTexture;
			DenoisingContext.NormalTexture = NormalTexture;

			if (PathTracingState->VarianceBuffer)
			{
				DenoisingContext.VarianceBuffer = GraphBuilder.RegisterExternalBuffer(PathTracingState->VarianceBuffer, TEXT("PathTracing.VarianceBuffer"));
			}

			PathTracingSpatialTemporalDenoisingPrePass(GraphBuilder, View, Config.PathTracingData.Iteration, DenoisingContext);

			CurrentVarianceBufer = DenoisingContext.VarianceBuffer;
		}

		// 2. Denoising pass
		if (NeedsDenoise || EnablePathTracingDenoiserRealtimeDebug)
		{
			DenoisingContext.RadianceTexture = RadianceTexture;
			DenoisingContext.FrameIndex = PathTracingState->FrameIndex;
			DenoisingContext.VarianceBuffer = CurrentVarianceBufer;

			if (PathTracingState->LastDenoisedRadianceRT)
			{
				DenoisingContext.LastDenoisedRadianceTexture =
					GraphBuilder.RegisterExternalTexture(PathTracingState->LastDenoisedRadianceRT, TEXT("PathTracing.LastPreDenoisedRadiance"));
				DenoisingContext.LastRadianceTexture =
					GraphBuilder.RegisterExternalTexture(PathTracingState->LastRadianceRT, TEXT("PathTracing.LastRadianceTexture"));
				DenoisingContext.LastNormalTexture =
					GraphBuilder.RegisterExternalTexture(PathTracingState->LastNormalRT, TEXT("PathTracing.LastNormalTexture"));
				DenoisingContext.LastAlbedoTexture =
					GraphBuilder.RegisterExternalTexture(PathTracingState->LastAlbedoRT, TEXT("PathTracing.LastAlbedoTexture"));

				DenoisingContext.LastVarianceBuffer = PathTracingState->LastVarianceBuffer?
					GraphBuilder.RegisterExternalBuffer(PathTracingState->LastVarianceBuffer, TEXT("PathTracing.LastVarianceBuffer")) : nullptr;
			}

			PathTracingSpatialTemporalDenoising(GraphBuilder,
				View,
				DenoiserMode,
				DenoisedRadianceTexture,
				DenoisingContext);

			GraphBuilder.QueueTextureExtraction(DenoisedRadianceTexture, &PathTracingState->LastDenoisedRadianceRT);
			GraphBuilder.QueueTextureExtraction(NormalTexture, &PathTracingState->LastNormalRT);
			GraphBuilder.QueueTextureExtraction(AlbedoTexture, &PathTracingState->LastAlbedoRT);
			GraphBuilder.QueueTextureExtraction(RadianceTexture, &PathTracingState->LastRadianceRT);

			PathTracingState->SpatialTemporalDenoiserHistory = DenoisingContext.SpatialTemporalDenoiserHistory;
		}

		// 3. Update pixel variance
		if (CurrentVarianceBufer)
		{
			GraphBuilder.QueueBufferExtraction(CurrentVarianceBufer, 
				(NeedsDenoise || EnablePathTracingDenoiserRealtimeDebug) ?
				&PathTracingState->LastVarianceBuffer:
				&PathTracingState->VarianceBuffer);

			if (NeedsDenoise || EnablePathTracingDenoiserRealtimeDebug)
			{
				PathTracingState->VarianceBuffer = nullptr;
			}
		}
		
	}
	PathTracingState->LastConfig.DenoiserMode = DenoiserMode;

	// now add a pixel shader pass to display our Radiance buffer and write to the depth buffer

	FPathTracingCompositorPS::FParameters* DisplayParameters = GraphBuilder.AllocParameters<FPathTracingCompositorPS::FParameters>();
	DisplayParameters->Iteration = Config.PathTracingData.Iteration;
	DisplayParameters->MaxSamples = MaxSPP;
	DisplayParameters->ProgressDisplayEnabled = CVarPathTracingProgressDisplay.GetValueOnRenderThread();
	DisplayParameters->AdaptiveSamplingErrorThreshold = Config.AdaptiveSamplingThreshold;
	DisplayParameters->AdaptiveSamplingVisualize = CVarPathTracingAdaptiveSamplingVisualize.GetValueOnRenderThread();
	DisplayParameters->VarianceTextureDims = FIntVector(DispatchResX, DispatchResY, NumVarianceMips);
	DisplayParameters->ViewUniformBuffer = View.ViewUniformBuffer;
	DisplayParameters->RadianceTexture = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(DenoisedRadianceTexture ? DenoisedRadianceTexture : RadianceTexture));
	DisplayParameters->VarianceTexture = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(VarianceTexture ? VarianceTexture : GSystemTextures.GetBlackDummy(GraphBuilder)));
	DisplayParameters->NormalDepthTexture = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(NormalTexture));
	DisplayParameters->RenderTargets[0] = FRenderTargetBinding(SceneColorOutputTexture, ERenderTargetLoadAction::ELoad);
	DisplayParameters->RenderTargets.DepthStencil = FDepthStencilBinding(SceneDepthOutputTexture,  ERenderTargetLoadAction::ELoad, ERenderTargetLoadAction::ENoAction, FExclusiveDepthStencil::DepthWrite_StencilNop);
	DisplayParameters->VarianceSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();

	FScreenPassTextureViewport Viewport(SceneColorOutputTexture, View.ViewRect);

	const bool IsCursorInsideView = View.CursorPos.X != -1 || View.CursorPos.Y != -1;
	// wiper mode - reveals the render below the path tracing display
	// NOTE: we still path trace the full resolution even while wiping the cursor so that rendering does not get out of sync
	if (CVarPathTracingWiperMode.GetValueOnRenderThread() != 0)
	{
		float DPIScale = FPlatformApplicationMisc::GetDPIScaleFactorAtPoint(View.CursorPos.X, View.CursorPos.Y);
		
		if (IsCursorInsideView)
		{
			Viewport.Rect.Min.X = View.CursorPos.X / DPIScale;
		}
		else
		{
			Viewport.Rect.Min.X = 0.5 * View.ViewRect.Min.X + 0.5 * View.ViewRect.Max.X;
		}
	}

	TShaderMapRef<FPathTracingCompositorPS> PixelShader(View.ShaderMap);
	TShaderMapRef<FScreenPassVS> VertexShader(View.ShaderMap);
	FRHIBlendState* BlendState = FScreenPassPipelineState::FDefaultBlendState::GetRHI();
	FRHIDepthStencilState* DepthStencilState = nullptr;

	if (CVarpathTracingOverrideDepth.GetValueOnRenderThread() != 0)
	{
		DepthStencilState = TStaticDepthStencilState<true, CF_Always>::GetRHI();
	}
	else
	{
		DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
	}

	AddDrawScreenPass(
		GraphBuilder,
		RDG_EVENT_NAME("Path Tracer Display (%d x %d)", View.ViewRect.Size().X, View.ViewRect.Size().Y),
		View,
		Viewport,
		Viewport,
		VertexShader,
		PixelShader,
		BlendState,
		DepthStencilState,
		DisplayParameters);

	// Setup the path tracing resources to be used by post process pass.
	if (CVarPathTracingOutputPostProcessResources.GetValueOnRenderThread() != 0)
	{
		PathTracingResources.bPostProcessEnabled = true;
		PathTracingResources.DenoisedRadiance = DenoisedRadianceTexture ? DenoisedRadianceTexture : RadianceTexture;
		PathTracingResources.Radiance = RadianceTexture;
		PathTracingResources.Albedo = AlbedoTexture;
		PathTracingResources.Normal = NormalTexture;
		PathTracingResources.Variance = DenoisingContext.VarianceTexture;
	}

	// Add a visualization path for denoising
	if (NeedsDenoise || EnablePathTracingDenoiserRealtimeDebug)
	{
		FVisualizePathTracingDenoisingInputs Inputs;
		Inputs.SceneColor =SceneColorOutputTexture;

		FScreenPassTextureViewport MotionVectorViewport(SceneColorOutputTexture, View.ViewRect);
		if (CVarPathTracingWiperMode.GetValueOnRenderThread() != 0)
		{
			float DPIScale = FPlatformApplicationMisc::GetDPIScaleFactorAtPoint(View.CursorPos.X, View.CursorPos.Y);
			if (IsCursorInsideView)
			{
				MotionVectorViewport.Rect.Max.X = View.CursorPos.X / DPIScale;
			}
			else
			{
				MotionVectorViewport.Rect.Max.X = 0.5 * View.ViewRect.Min.X + 0.5 * View.ViewRect.Max.X;
			}
		}

		Inputs.Viewport = MotionVectorViewport;

		Inputs.DenoisingContext = DenoisingContext;
		Inputs.SceneTexturesUniformBuffer = SceneTexturesUniformBuffer;
		Inputs.DenoisedTexture = DenoisedRadianceTexture;

		AddVisualizePathTracingDenoisingPass(GraphBuilder, View, Inputs);
	}
}

#endif
