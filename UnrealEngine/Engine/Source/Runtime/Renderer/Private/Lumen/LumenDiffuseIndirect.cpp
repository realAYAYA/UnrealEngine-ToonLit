// Copyright Epic Games, Inc. All Rights Reserved.

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneUtils.h"
#include "PipelineStateCache.h"
#include "ShaderParameterStruct.h"
#include "PixelShaderUtils.h"
#include "ReflectionEnvironment.h"
#include "SceneTextureParameters.h"
#include "IndirectLightRendering.h"
#include "LumenRadianceCache.h"
#include "GlobalDistanceField.h"
#include "LumenTracingUtils.h"
#include "ComponentRecreateRenderStateContext.h"

extern int32 GLumenSceneGlobalSDFSimpleCoverageBasedExpand;

FLumenGatherCvarState GLumenGatherCvars;

FLumenGatherCvarState::FLumenGatherCvarState()
{
	TraceMeshSDFs = 1;
	MeshSDFTraceDistance = 180.0f;
	SurfaceBias = 5.0f;
	VoxelTracingMode = 0;
	DirectLighting = 0;
}

static TAutoConsoleVariable<int> CVarLumenGlobalIllumination(
	TEXT("r.Lumen.DiffuseIndirect.Allow"),
	1,
	TEXT("Whether to allow Lumen Global Illumination.  Lumen GI is enabled in the project settings, this cvar can only disable it."), 
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
	{
		FGlobalComponentRecreateRenderStateContext Context;
	}),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GDiffuseTraceStepFactor = 1;
FAutoConsoleVariableRef CVarDiffuseTraceStepFactor(
	TEXT("r.Lumen.DiffuseIndirect.TraceStepFactor"),
	GDiffuseTraceStepFactor,
	TEXT("."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

float GLumenDiffuseMinSampleRadius = 10;
FAutoConsoleVariableRef CVarLumenDiffuseMinSampleRadius(
	TEXT("r.Lumen.DiffuseIndirect.MinSampleRadius"),
	GLumenDiffuseMinSampleRadius,
	TEXT("."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

float GLumenDiffuseMinTraceDistance = 0;
FAutoConsoleVariableRef CVarLumenDiffuseMinTraceDistance(
	TEXT("r.Lumen.DiffuseIndirect.MinTraceDistance"),
	GLumenDiffuseMinTraceDistance,
	TEXT("."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

FAutoConsoleVariableRef CVarLumenDiffuseSurfaceBias(
	TEXT("r.Lumen.DiffuseIndirect.SurfaceBias"),
	GLumenGatherCvars.SurfaceBias,
	TEXT("."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

float GLumenDiffuseCardInterpolateInfluenceRadius = 10;
FAutoConsoleVariableRef CVarDiffuseCardInterpolateInfluenceRadius(
	TEXT("r.Lumen.DiffuseIndirect.CardInterpolateInfluenceRadius"),
	GLumenDiffuseCardInterpolateInfluenceRadius,
	TEXT("."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

float GDiffuseCardTraceEndDistanceFromCamera = 4000.0f;
FAutoConsoleVariableRef CVarDiffuseCardTraceEndDistanceFromCamera(
	TEXT("r.Lumen.DiffuseIndirect.CardTraceEndDistanceFromCamera"),
	GDiffuseCardTraceEndDistanceFromCamera,
	TEXT(""),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

float GLumenTraceDistanceScale = 1.0f;
FAutoConsoleVariableRef CVarTraceDistanceScale(
	TEXT("r.Lumen.TraceDistanceScale"),
	GLumenTraceDistanceScale,
	TEXT("Scales the tracing distance for all tracing methods and Lumen features, used by scalability."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

// Project setting driven by RendererSettings
int32 GLumenTraceMeshSDFs = 1;
FAutoConsoleVariableRef CVarLumenTraceMeshSDFs(
	TEXT("r.Lumen.TraceMeshSDFs"),
	GLumenTraceMeshSDFs,
	TEXT("Whether Lumen should trace against Mesh Signed Distance fields.  When enabled, Lumen's Software Tracing will be more accurate, but scenes with high instance density (overlapping meshes) will have high tracing costs.  When disabled, lower resolution Global Signed Distance Field will be used instead."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

// Scalability setting driven by scalability ini
int32 GLumenAllowTracingMeshSDFs = 1;
FAutoConsoleVariableRef CVarLumenAllowTraceMeshSDFs(
	TEXT("r.Lumen.TraceMeshSDFs.Allow"),
	GLumenAllowTracingMeshSDFs,
	TEXT("Whether Lumen should trace against Mesh Signed Distance fields.  When enabled, Lumen's Software Tracing will be more accurate, but scenes with high instance density (overlapping meshes) will have high tracing costs.  When disabled, lower resolution Global Signed Distance Field will be used instead."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

FAutoConsoleVariableRef GVarLumenDiffuseMaxMeshSDFTraceDistance(
	TEXT("r.Lumen.TraceMeshSDFs.TraceDistance"),
	GLumenGatherCvars.MeshSDFTraceDistance,
	TEXT("Max trace distance against Mesh Distance Fields and Heightfields."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GCardFroxelGridPixelSize = 64;
FAutoConsoleVariableRef CVarLumenDiffuseFroxelGridPixelSize(
	TEXT("r.Lumen.DiffuseIndirect.CullGridPixelSize"),
	GCardFroxelGridPixelSize,
	TEXT("Size of a cell in the card grid, in pixels."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GCardGridDistributionLogZScale = .01f;
FAutoConsoleVariableRef CCardGridDistributionLogZScale(
	TEXT("r.Lumen.DiffuseIndirect.CullGridDistributionLogZScale"),
	GCardGridDistributionLogZScale,
	TEXT(""),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GCardGridDistributionLogZOffset = 1.0f;
FAutoConsoleVariableRef CCardGridDistributionLogZOffset(
	TEXT("r.Lumen.DiffuseIndirect.CullGridDistributionLogZOffset"),
	GCardGridDistributionLogZOffset,
	TEXT(""),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GCardGridDistributionZScale = 4.0f;
FAutoConsoleVariableRef CVarCardGridDistributionZScale(
	TEXT("r.Lumen.DiffuseIndirect.CullGridDistributionZScale"),
	GCardGridDistributionZScale,
	TEXT(""),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLumenDiffuseIndirectAsyncCompute(
	TEXT("r.Lumen.DiffuseIndirect.AsyncCompute"),
	1,
	TEXT("Whether to run Lumen diffuse indirect passes on the compute pipe if possible."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenDiffuseIndirectApplySSAO = 0;
FAutoConsoleVariableRef CVarLumenDiffuseIndirectApplySSAO(
	TEXT("r.Lumen.DiffuseIndirect.SSAO"),
	GLumenDiffuseIndirectApplySSAO,
	TEXT("Whether to render and apply SSAO to Lumen GI, only when r.Lumen.ScreenProbeGather.ShortRangeAO is disabled.  This is useful for providing short range occlusion when Lumen's Screen Bent Normal is disabled due to scalability, however SSAO settings like screen radius come from the user's post process settings."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenShouldUseStereoOptimizations = 1;
FAutoConsoleVariableRef CVarLumenShouldUseStereoOptimizations(
	TEXT("r.Lumen.StereoOptimizations"),
	GLumenShouldUseStereoOptimizations,
	TEXT("Whether to to share certain Lumen state between views during the instanced stereo rendering."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarOrthoOverrideMeshDFTraceDistances(
	TEXT("r.Lumen.Ortho.OverrideMeshDFTraceDistances"),
	1,
	TEXT("Use the full screen view rect size in Ortho views to determing the SDF trace distances instead of setting the value manually."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

bool LumenDiffuseIndirect::IsAllowed()
{
	return CVarLumenGlobalIllumination.GetValueOnAnyThread() != 0;
}

bool LumenDiffuseIndirect::UseAsyncCompute(const FViewFamilyInfo& ViewFamily)
{
	return Lumen::UseAsyncCompute(ViewFamily) && CVarLumenDiffuseIndirectAsyncCompute.GetValueOnRenderThread() != 0;
}

bool Lumen::UseMeshSDFTracing(const FSceneViewFamily& ViewFamily)
{
	return GLumenTraceMeshSDFs != 0 
		&& GLumenAllowTracingMeshSDFs != 0
		&& ViewFamily.EngineShowFlags.LumenDetailTraces;
}

bool Lumen::UseGlobalSDFTracing(const FSceneViewFamily& ViewFamily)
{
	return ViewFamily.EngineShowFlags.LumenGlobalTraces;
}

bool Lumen::UseGlobalSDFSimpleCoverageBasedExpand()
{
	return GLumenSceneGlobalSDFSimpleCoverageBasedExpand != 0;
}

float Lumen::GetMaxTraceDistance(const FViewInfo& View)
{
	return FMath::Clamp(View.FinalPostProcessSettings.LumenMaxTraceDistance * GLumenTraceDistanceScale, .01f, Lumen::MaxTraceDistance);
}

bool Lumen::ShouldPrecachePSOs(EShaderPlatform Platform)
{
	return DoesPlatformSupportLumenGI(Platform) && CVarLumenGlobalIllumination.GetValueOnAnyThread();
}

void FHemisphereDirectionSampleGenerator::GenerateSamples(int32 TargetNumSamples, int32 InPowerOfTwoDivisor, int32 InSeed, bool bInFullSphere, bool bInCosineDistribution)
{
	int32 NumThetaSteps = FMath::Max(FMath::TruncToInt(FMath::Sqrt(TargetNumSamples / ((float)PI))), 1);
	//int32 NumPhiSteps = FMath::TruncToInt(NumThetaSteps * (float)PI);
	int32 NumPhiSteps = FMath::DivideAndRoundDown(TargetNumSamples, NumThetaSteps);
	NumPhiSteps = FMath::Max(FMath::DivideAndRoundDown(NumPhiSteps, InPowerOfTwoDivisor), 1) * InPowerOfTwoDivisor;

	if (SampleDirections.Num() != NumThetaSteps * NumPhiSteps || PowerOfTwoDivisor != InPowerOfTwoDivisor || Seed != InSeed || bInFullSphere != bFullSphere)
	{
		SampleDirections.Empty(NumThetaSteps * NumPhiSteps);
		FRandomStream RandomStream(InSeed);

		for (int32 ThetaIndex = 0; ThetaIndex < NumThetaSteps; ThetaIndex++)
		{
			for (int32 PhiIndex = 0; PhiIndex < NumPhiSteps; PhiIndex++)
			{
				const float U1 = RandomStream.GetFraction();
				const float U2 = RandomStream.GetFraction();

				float Fraction1 = (ThetaIndex + U1) / (float)NumThetaSteps;

				if (bInFullSphere)
				{
					Fraction1 = Fraction1 * 2 - 1;
				}

				const float Fraction2 = (PhiIndex + U2) / (float)NumPhiSteps;
				const float Phi = 2.0f * (float)PI * Fraction2;

				if (bInCosineDistribution)
				{
					const float CosTheta = FMath::Sqrt(Fraction1);
					const float SinTheta = FMath::Sqrt(1.0f - CosTheta * CosTheta);
					SampleDirections.Add(FVector4f(FMath::Cos(Phi) * SinTheta, FMath::Sin(Phi) * SinTheta, CosTheta));
				}
				else
				{
					const float CosTheta = Fraction1;
					const float SinTheta = FMath::Sqrt(1.0f - CosTheta * CosTheta);
					SampleDirections.Add(FVector4f(FMath::Cos(Phi) * SinTheta, FMath::Sin(Phi) * SinTheta, CosTheta));
				}
			}
		}

		ConeHalfAngle = FMath::Acos(1 - 1.0f / (float)SampleDirections.Num());
		Seed = InSeed;
		PowerOfTwoDivisor = InPowerOfTwoDivisor;
		bFullSphere = bInFullSphere;
		bCosineDistribution = bInCosineDistribution;
	}
}

bool ShouldRenderLumenDiffuseGI(const FScene* Scene, const FSceneView& View, bool bSkipTracingDataCheck, bool bSkipProjectCheck) 
{
	return Lumen::IsLumenFeatureAllowedForView(Scene, View, bSkipTracingDataCheck, bSkipProjectCheck)
		&& View.FinalPostProcessSettings.DynamicGlobalIlluminationMethod == EDynamicGlobalIlluminationMethod::Lumen
		&& CVarLumenGlobalIllumination.GetValueOnAnyThread()
		&& View.Family->EngineShowFlags.GlobalIllumination 
		&& View.Family->EngineShowFlags.LumenGlobalIllumination
		&& !View.Family->EngineShowFlags.PathTracing
		&& (bSkipTracingDataCheck || Lumen::UseHardwareRayTracedScreenProbeGather(*View.Family) || Lumen::IsSoftwareRayTracingSupported());
}

bool ShouldRenderLumenDirectLighting(const FScene* Scene, const FSceneView& View)
{
	return ShouldRenderLumenDiffuseGI(Scene, View)
		&& GLumenGatherCvars.DirectLighting
		&& !GLumenIrradianceFieldGather;
}

bool ShouldRenderAOWithLumenGI()
{
	extern int32 GLumenShortRangeAmbientOcclusion;
	return GLumenDiffuseIndirectApplySSAO != 0 && GLumenShortRangeAmbientOcclusion == 0;
}

bool ShouldUseStereoLumenOptimizations()
{
	return GLumenShouldUseStereoOptimizations != 0;
}

void SetupLumenDiffuseTracingParameters(const FViewInfo& View, FLumenIndirectTracingParameters& OutParameters)
{
	OutParameters.StepFactor = FMath::Clamp(GDiffuseTraceStepFactor, .1f, 10.0f);
	
	OutParameters.MinSampleRadius = FMath::Clamp(GLumenDiffuseMinSampleRadius, .01f, 100.0f);
	OutParameters.MinTraceDistance = FMath::Clamp(GLumenDiffuseMinTraceDistance, .01f, 1000.0f);
	OutParameters.MaxTraceDistance = Lumen::GetMaxTraceDistance(View);
	if (!View.IsPerspectiveProjection() && CVarOrthoOverrideMeshDFTraceDistances.GetValueOnAnyThread())
	{
		float TraceSDFDistance = FMath::Clamp(View.ViewMatrices.GetOrthoDimensions().GetMax(), OutParameters.MinTraceDistance, OutParameters.MaxTraceDistance);
		OutParameters.MaxMeshSDFTraceDistance = TraceSDFDistance;
		OutParameters.CardTraceEndDistanceFromCamera = FMath::Max(GDiffuseCardTraceEndDistanceFromCamera, TraceSDFDistance);
	}
	else
	{
		OutParameters.MaxMeshSDFTraceDistance = FMath::Clamp(GLumenGatherCvars.MeshSDFTraceDistance, OutParameters.MinTraceDistance, OutParameters.MaxTraceDistance);
		OutParameters.CardTraceEndDistanceFromCamera = GDiffuseCardTraceEndDistanceFromCamera;

	}
	OutParameters.SurfaceBias = FMath::Clamp(GLumenGatherCvars.SurfaceBias, .01f, 100.0f);
	OutParameters.CardInterpolateInfluenceRadius = FMath::Clamp(GLumenDiffuseCardInterpolateInfluenceRadius, .01f, 1000.0f);
	OutParameters.HeightfieldMaxTracingSteps = Lumen::GetHeightfieldMaxTracingSteps();
	//@todo - remove
	OutParameters.DiffuseConeHalfAngle = 0.1f;
	OutParameters.TanDiffuseConeHalfAngle = FMath::Tan(OutParameters.DiffuseConeHalfAngle);
	OutParameters.SpecularFromDiffuseRoughnessStart = 0.0f;
	OutParameters.SpecularFromDiffuseRoughnessEnd = 0.0f;
}

void SetupLumenDiffuseTracingParametersForProbe(const FViewInfo& View, FLumenIndirectTracingParameters& OutParameters, float DiffuseConeHalfAngle)
{
	SetupLumenDiffuseTracingParameters(View, OutParameters);

	// Probe tracing doesn't have surface bias, but should bias MinTraceDistance due to the mesh SDF world space error
	OutParameters.SurfaceBias = 0.0f;
	OutParameters.MinTraceDistance = FMath::Clamp(FMath::Max(GLumenGatherCvars.SurfaceBias, GLumenDiffuseMinTraceDistance), .01f, 1000.0f);

	if (DiffuseConeHalfAngle >= 0.0f)
	{
		OutParameters.DiffuseConeHalfAngle = DiffuseConeHalfAngle;
		OutParameters.TanDiffuseConeHalfAngle = FMath::Tan(DiffuseConeHalfAngle);
	}
}

void GetCardGridZParams(float InNearPlane, float InFarPlane, FVector& OutZParams, int32& OutGridSizeZ)
{
	float NearPlane = FMath::Min(InFarPlane, InNearPlane);
	float FarPlane = FMath::Max(InFarPlane, InNearPlane);
	OutGridSizeZ = FMath::Max(FMath::TruncToInt(FMath::Log2((FarPlane - NearPlane) * GCardGridDistributionLogZScale) * GCardGridDistributionZScale), 0) + 1;
	OutZParams = FVector(GCardGridDistributionLogZScale, GCardGridDistributionLogZOffset, GCardGridDistributionZScale);
}

void CullForCardTracing(
	FRDGBuilder& GraphBuilder,
	const FScene* Scene,
	const FViewInfo& View,
	const FLumenSceneFrameTemporaries& FrameTemporaries,
	const FLumenIndirectTracingParameters& IndirectTracingParameters,
	FLumenMeshSDFGridParameters& MeshSDFGridParameters,
	ERDGPassFlags ComputePassFlags)
{
	LLM_SCOPE_BYTAG(Lumen);

	FVector ZParams;
	int32 CardGridSizeZ;
	GetCardGridZParams(View.NearClippingDistance, IndirectTracingParameters.CardTraceEndDistanceFromCamera, ZParams, CardGridSizeZ);

	MeshSDFGridParameters.CardGridPixelSizeShift = FMath::FloorLog2(GCardFroxelGridPixelSize);
	MeshSDFGridParameters.CardGridZParams = (FVector3f)ZParams; // LWC_TODO: Precision Loss

	const FIntPoint CardGridSizeXY = FIntPoint::DivideAndRoundUp(View.ViewRect.Size(), GCardFroxelGridPixelSize);
	const FIntVector CullGridSize(CardGridSizeXY.X, CardGridSizeXY.Y, CardGridSizeZ);
	MeshSDFGridParameters.CullGridSize = CullGridSize;

	CullMeshObjectsToViewGrid(
		View,
		Scene,
		FrameTemporaries,
		IndirectTracingParameters.MaxMeshSDFTraceDistance,
		IndirectTracingParameters.CardTraceEndDistanceFromCamera,
		GCardFroxelGridPixelSize,
		CardGridSizeZ,
		ZParams,
		GraphBuilder,
		MeshSDFGridParameters,
		ComputePassFlags);
}
