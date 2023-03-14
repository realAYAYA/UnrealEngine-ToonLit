// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LumenDistantScene.cpp
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneUtils.h"
#include "PipelineStateCache.h"
#include "ShaderParameterStruct.h"
#include "PixelShaderUtils.h"
#include "ReflectionEnvironment.h"

int32 GLumenDistantScene = 0;
FAutoConsoleVariableRef CVarLumenDistantScene(
	TEXT("r.LumenScene.DistantScene"),
	GLumenDistantScene,
	TEXT("0: off, 1: on, 2: only on if r.LumenScene.FastCameraMode is enabled"),
	ECVF_RenderThreadSafe
	);

int32 GLumenUpdateDistantSceneCaptures = 1;
FAutoConsoleVariableRef CVarLumenLumenUpdateDistantSceneCaptures(
	TEXT("r.LumenScene.DistantScene.UpdateCaptures"),
	GLumenUpdateDistantSceneCaptures,
	TEXT(""),
	ECVF_RenderThreadSafe
	);

int32 GLumenUpdateDistantScenePlacement = 1;
FAutoConsoleVariableRef CVarLumenLumenUpdateDistantScenePlacement(
	TEXT("r.LumenScene.DistantScene.UpdatePlacement"),
	GLumenUpdateDistantScenePlacement,
	TEXT(""),
	ECVF_RenderThreadSafe
	);

int32 GLumenDistantSceneSnapOrigin = 1;
FAutoConsoleVariableRef CVarLumenDistantSceneSnapOrigin(
	TEXT("r.LumenScene.DistantScene.SnapOrigin"),
	GLumenDistantSceneSnapOrigin,
	TEXT(""),
	ECVF_RenderThreadSafe
	);

int32 GLumenDistantSceneCardResolution = 256;
FAutoConsoleVariableRef CVarLumenDistantSceneCardResolution(
	TEXT("r.LumenScene.DistantScene.CardResolution"),
	GLumenDistantSceneCardResolution,
	TEXT(""),
	ECVF_RenderThreadSafe
	);

int32 GLumenNumDistantCascades = 1;
FAutoConsoleVariableRef CVarLumenNumDistantCascades(
	TEXT("r.LumenScene.DistantScene.NumCascades"),
	GLumenNumDistantCascades,
	TEXT("Todo - shader only supports 1 cascade"),
	ECVF_RenderThreadSafe
	);

int32 GLumenDrawCascadeBounds = 0;
FAutoConsoleVariableRef CVarLumenDrawCascadeBounds(
	TEXT("r.LumenScene.DistantScene.DrawCascadeBounds"),
	GLumenDrawCascadeBounds,
	TEXT(""),
	ECVF_RenderThreadSafe
	);

float GLumenDistantSceneStartDistanceFromCamera = 20000.0f;
FAutoConsoleVariableRef CVarLumenDistantSceneStartDistanceFromCamera(
	TEXT("r.LumenScene.DistantScene.StartDistanceFromCamera"),
	GLumenDistantSceneStartDistanceFromCamera,
	TEXT(""),
	ECVF_RenderThreadSafe
	);

float GLumenDistantSceneEndDistanceFromCamera = 100000.0f;
FAutoConsoleVariableRef CVarLumenDistantSceneEndDistanceFromCamera(
	TEXT("r.LumenScene.DistantScene.EndDistanceFromCamera"),
	GLumenDistantSceneEndDistanceFromCamera,
	TEXT(""),
	ECVF_RenderThreadSafe
	);

float GLumenDistantSceneMaxTraceDistance = 100000.0f;
FAutoConsoleVariableRef CVarLumenDistantSceneMaxTraceDistance(
	TEXT("r.LumenScene.DistantScene.MaxTraceDistance"),
	GLumenDistantSceneMaxTraceDistance,
	TEXT(""),
	ECVF_RenderThreadSafe
	);

float GLumenDistantSceneMinInstanceBoundsRadius = 200.0f;
FAutoConsoleVariableRef CVarLumenDistantSceneMinInstanceBoundsRadius(
	TEXT("r.LumenScene.DistantScene.MinInstanceBoundsRadius"),
	GLumenDistantSceneMinInstanceBoundsRadius,
	TEXT(""),
	ECVF_RenderThreadSafe
	);

float GLumenDistantSceneCascadeDistributionExponent = 2.0f;
FAutoConsoleVariableRef CVarLumenDistantSceneCascadeDistributionExponent(
	TEXT("r.LumenScene.DistantScene.CascadeDistributionExponent"),
	GLumenDistantSceneCascadeDistributionExponent,
	TEXT(""),
	ECVF_RenderThreadSafe
	);

static TAutoConsoleVariable<float> CVarLumenDistantSceneNaniteLODBias(
	TEXT("r.LumenScene.DistantScene.NaniteLODBias"),
	4.0f,
	TEXT("LOD bias for Nanite geometry in Lumen distant scene representation. 0 - full detail. > 0 - reduced detail."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarLumenFarField(
	TEXT("r.LumenScene.FarField"), 0,
	TEXT("Enable/Disable Lumen far-field ray tracing."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarLumenFarFieldMaxTraceDistance(
	TEXT("r.LumenScene.FarField.MaxTraceDistance"), 1.0e6f,
	TEXT("Maximum hit-distance for Lumen far-field ray tracing (Default = 1.0e6)."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarLumenFarFieldDitheredStartDistanceFactor(
	TEXT("r.LumenScene.FarField.DitheredStartDistanceFactor"), 0.66f,
	TEXT("Starting distance for far-field dithered t-min, as a percentage of near-field t-max (Default = 0.66f)."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarLumenFarFieldReferencePosZ(
	TEXT("r.LumenScene.FarField.ReferencePos.Z"),
	100000.0f,
	TEXT("Far-field reference position in Z (default = 100000.0)"),
	ECVF_RenderThreadSafe
);

namespace Lumen
{
	bool UseFarField(const FSceneViewFamily& ViewFamily)
	{
		return CVarLumenFarField.GetValueOnRenderThread() != 0 
			&& ViewFamily.EngineShowFlags.LumenFarFieldTraces;
	}

	float GetFarFieldMaxTraceDistance()
	{
		return CVarLumenFarFieldMaxTraceDistance.GetValueOnRenderThread();
	}

	float GetFarFieldDitheredStartDistanceFactor()
	{
		return FMath::Clamp(CVarLumenFarFieldDitheredStartDistanceFactor.GetValueOnRenderThread(), 0.0f, 1.0f);
	}

	FVector GetFarFieldReferencePos()
	{
		return FVector(0.0f, 0.0f, CVarLumenFarFieldReferencePosZ.GetValueOnRenderThread());
	}
}

bool ShouldEnableDistantScene()
{
	if (GLumenDistantScene == 1)
	{
		return true;
	}
	else if (GLumenDistantScene == 2)
	{
		return GLumenFastCameraMode != 0;
	}
	
	return false;
}

float Lumen::GetDistanceSceneNaniteLODScaleFactor()
{
	return FMath::Pow(2.0f, -FMath::Max(CVarLumenDistantSceneNaniteLODBias.GetValueOnRenderThread(), 0.0f));
}

FSphere GetCascadeBounds(const FViewInfo& View, float SplitNear, float SplitFar)
{
	const FMatrix& ViewMatrix = View.ViewMatrices.GetViewMatrix();
	const FMatrix& ProjectionMatrix = View.ViewMatrices.GetProjectionMatrix();
	const FVector ViewOrigin = View.ViewMatrices.GetViewOrigin();
	const FVector CameraDirection = ViewMatrix.GetColumn(2);

	// Support asymmetric projection
	// Get FOV and AspectRatio from the view's projection matrix.
	float AspectRatio = ProjectionMatrix.M[1][1] / ProjectionMatrix.M[0][0];
	bool bIsPerspectiveProjection = View.ViewMatrices.IsPerspectiveProjection();

	// Build the camera frustum for this cascade
	float HalfHorizontalFOV = bIsPerspectiveProjection ? FMath::Atan(1.0f / ProjectionMatrix.M[0][0]) : PI / 4.0f;
	float HalfVerticalFOV = bIsPerspectiveProjection ? FMath::Atan(1.0f / ProjectionMatrix.M[1][1]) : FMath::Atan((FMath::Tan(PI / 4.0f) / AspectRatio));
	float AsymmetricFOVScaleX = ProjectionMatrix.M[2][0];
	float AsymmetricFOVScaleY = ProjectionMatrix.M[2][1];

	// Near plane
	const float StartHorizontalTotalLength = SplitNear * FMath::Tan(HalfHorizontalFOV);
	const float StartVerticalTotalLength = SplitNear * FMath::Tan(HalfVerticalFOV);
	const FVector StartCameraLeftOffset = ViewMatrix.GetColumn(0) * -StartHorizontalTotalLength * (1 + AsymmetricFOVScaleX);
	const FVector StartCameraRightOffset = ViewMatrix.GetColumn(0) *  StartHorizontalTotalLength * (1 - AsymmetricFOVScaleX);
	const FVector StartCameraBottomOffset = ViewMatrix.GetColumn(1) * -StartVerticalTotalLength * (1 + AsymmetricFOVScaleY);
	const FVector StartCameraTopOffset = ViewMatrix.GetColumn(1) *  StartVerticalTotalLength * (1 - AsymmetricFOVScaleY);
	// Far plane
	const float EndHorizontalTotalLength = SplitFar * FMath::Tan(HalfHorizontalFOV);
	const float EndVerticalTotalLength = SplitFar * FMath::Tan(HalfVerticalFOV);
	const FVector EndCameraLeftOffset = ViewMatrix.GetColumn(0) * -EndHorizontalTotalLength * (1 + AsymmetricFOVScaleX);
	const FVector EndCameraRightOffset = ViewMatrix.GetColumn(0) *  EndHorizontalTotalLength * (1 - AsymmetricFOVScaleX);
	const FVector EndCameraBottomOffset = ViewMatrix.GetColumn(1) * -EndVerticalTotalLength * (1 + AsymmetricFOVScaleY);
	const FVector EndCameraTopOffset = ViewMatrix.GetColumn(1) *  EndVerticalTotalLength * (1 - AsymmetricFOVScaleY);

	// Get the 8 corners of the cascade's camera frustum, in world space
	FVector CascadeFrustumVerts[8];
	CascadeFrustumVerts[0] = ViewOrigin + CameraDirection * SplitNear + StartCameraRightOffset + StartCameraTopOffset;    // 0 Near Top    Right
	CascadeFrustumVerts[1] = ViewOrigin + CameraDirection * SplitNear + StartCameraRightOffset + StartCameraBottomOffset; // 1 Near Bottom Right
	CascadeFrustumVerts[2] = ViewOrigin + CameraDirection * SplitNear + StartCameraLeftOffset + StartCameraTopOffset;     // 2 Near Top    Left
	CascadeFrustumVerts[3] = ViewOrigin + CameraDirection * SplitNear + StartCameraLeftOffset + StartCameraBottomOffset;  // 3 Near Bottom Left
	CascadeFrustumVerts[4] = ViewOrigin + CameraDirection * SplitFar + EndCameraRightOffset + EndCameraTopOffset;       // 4 Far  Top    Right
	CascadeFrustumVerts[5] = ViewOrigin + CameraDirection * SplitFar + EndCameraRightOffset + EndCameraBottomOffset;    // 5 Far  Bottom Right
	CascadeFrustumVerts[6] = ViewOrigin + CameraDirection * SplitFar + EndCameraLeftOffset + EndCameraTopOffset;       // 6 Far  Top    Left
	CascadeFrustumVerts[7] = ViewOrigin + CameraDirection * SplitFar + EndCameraLeftOffset + EndCameraBottomOffset;    // 7 Far  Bottom Left

	// Fit a bounding sphere around the world space camera cascade frustum.
	// Compute the sphere ideal centre point given the FOV and near/far.
	float TanHalfFOVx = FMath::Tan(HalfHorizontalFOV);
	float TanHalfFOVy = FMath::Tan(HalfVerticalFOV);
	float FrustumLength = SplitFar - SplitNear;

	float FarX = TanHalfFOVx * SplitFar;
	float FarY = TanHalfFOVy * SplitFar;
	float DiagonalASq = FarX * FarX + FarY * FarY;

	float NearX = TanHalfFOVx * SplitNear;
	float NearY = TanHalfFOVy * SplitNear;
	float DiagonalBSq = NearX * NearX + NearY * NearY;

	// Calculate the ideal bounding sphere for the subfrustum.
	// Fx  = (Db^2 - da^2) / 2Fl + Fl / 2 
	// (where Da is the far diagonal, and Db is the near, and Fl is the frustum length)
	float OptimalOffset = (DiagonalBSq - DiagonalASq) / (2.0f * FrustumLength) + FrustumLength * 0.5f;
	float CentreZ = SplitFar - OptimalOffset;
	CentreZ = FMath::Clamp( CentreZ, SplitNear, SplitFar );
	FSphere CascadeSphere(ViewOrigin + CameraDirection * CentreZ, 0);
	for (int32 Index = 0; Index < 8; Index++)
	{
		CascadeSphere.W = FMath::Max(CascadeSphere.W, FVector::DistSquared(CascadeFrustumVerts[Index], CascadeSphere.Center));
	}

	// Don't allow the bounds to reach 0 (INF)
	CascadeSphere.W = FMath::Max(FMath::Sqrt(CascadeSphere.W), 1.0f); 

	return CascadeSphere;
}

float GetCascadeStartDistance(int32 CascadeIndex, int32 NumCascades)
{
	const float DistantSceneDepthRange = FMath::Max(GLumenDistantSceneEndDistanceFromCamera - GLumenDistantSceneStartDistanceFromCamera, 0.0f);
	
	float Total = 0.0f;
	float CascadeDistance = 0.0f;
	float Scale = 1.0f;

	for (int32 i = 0; i < NumCascades; i++)
	{
		Total += Scale;

		if (i < CascadeIndex)
		{
			CascadeDistance += Scale;
		}

		Scale *= GLumenDistantSceneCascadeDistributionExponent;
	}

	return GLumenDistantSceneStartDistanceFromCamera + DistantSceneDepthRange * CascadeDistance / Total;
}

/** Generates valid X and Y axes of a coordinate system, given the Z axis. */
void GenerateCoordinateSystem(const FVector& ZAxis, FVector& XAxis, FVector& YAxis)
{
	if (FMath::Abs(ZAxis.X) > FMath::Abs(ZAxis.Y))
 	{
		const float InverseLength = FMath::InvSqrt(ZAxis.X * ZAxis.X + ZAxis.Z * ZAxis.Z);
		XAxis = FVector4(-ZAxis.Z * InverseLength, 0.0f, ZAxis.X * InverseLength);
 	}
 	else
 	{
		const float InverseLength = FMath::InvSqrt(ZAxis.Y * ZAxis.Y + ZAxis.Z * ZAxis.Z);
		XAxis = FVector4(0.0f, ZAxis.Z * InverseLength, -ZAxis.Y * InverseLength);
 	}

	YAxis = ZAxis ^ XAxis;
}

void UpdateDistantScene(FScene* Scene, FViewInfo& View)
{
	// #lumen_todo: distance scene is disabled as it was colliding with the virtual surface cache work
	// and anyway requires a rewrite to reach required quality level

#if 0
	LLM_SCOPE_BYTAG(Lumen);
	TRACE_CPUPROFILER_EVENT_SCOPE(UpdateDistantScene);
	QUICK_SCOPE_CYCLE_COUNTER(UpdateDistantScene);

	FLumenSceneData& LumenSceneData = *Scene->LumenSceneData;

	int32 NumDesiredCascades = 0;

	if (ShouldEnableDistantScene() && Scene->DirectionalLights.Num() > 0)
	{
		NumDesiredCascades = FMath::Clamp<int32>(GLumenNumDistantCascades, 0, MaxDistantCards);
	}

	if (LumenSceneData.DistantCardIndices.Num() != NumDesiredCascades)
	{
		for (int32 DistantCardIndex : LumenSceneData.DistantCardIndices)
		{
			FLumenCard& DistantCard = LumenSceneData.Cards[DistantCardIndex];
			DistantCard.RemoveFromAtlas(LumenSceneData);
			LumenSceneData.CardIndicesToUpdateInBuffer.Add(DistantCardIndex);
			LumenSceneData.Cards.RemoveSpan(DistantCardIndex, 1);
		}

		LumenSceneData.DistantCardIndices.Reset();
	}

	if (NumDesiredCascades > 0)
	{
		bool bDistantCardsNeedFirstTransform = false;

		if (LumenSceneData.DistantCardIndices.Num() != NumDesiredCascades)
		{
			const int32 FirstCardIndex = LumenSceneData.Cards.AddSpan(NumDesiredCascades);
			LumenSceneData.DistantCardIndices.Reserve(NumDesiredCascades);

			for (int32 CascadeIndex = 0; CascadeIndex < NumDesiredCascades; CascadeIndex++)
			{
				const int32 CardIndex = FirstCardIndex + CascadeIndex;
				LumenSceneData.DistantCardIndices.Add(CardIndex);
				LumenSceneData.Cards[CardIndex].bDistantScene = true;
			}

			bDistantCardsNeedFirstTransform = true;
		}

		if (GLumenUpdateDistantScenePlacement || bDistantCardsNeedFirstTransform)
		{
			const FLightSceneInfo* DirectionalLight = Scene->DirectionalLights[0];
			const FVector LightDirection = -DirectionalLight->Proxy->GetDirection();
			const FVector CardToLocalRotationZ = LightDirection;

			FVector CardToLocalRotationX;
			FVector CardToLocalRotationY;
			GenerateCoordinateSystem(CardToLocalRotationZ, CardToLocalRotationX, CardToLocalRotationY);

			for (int32 CascadeIndex = 0; CascadeIndex < NumDesiredCascades; CascadeIndex++)
			{
				const float SplitNear = GetCascadeStartDistance(CascadeIndex, NumDesiredCascades);
				const float SplitFar = GetCascadeStartDistance(CascadeIndex + 1, NumDesiredCascades);
				const FSphere CascadeBounds = GetCascadeBounds(View, SplitNear, SplitFar);
				const int32 DistantCardIndex = LumenSceneData.DistantCardIndices[CascadeIndex];

				// Adding MaxTraceDistance is conservative but not necessary since there's so much padding around the cascade (AABB of sphere of frustum slice)
				const FVector LocalExtent = FVector(CascadeBounds.W/* + GLumenDistantSceneMaxTraceDistance*/);
			
				FVector CascadeCenter = CascadeBounds.Center;

				if (GLumenDistantSceneSnapOrigin)
				{
					FVector LocalCenter(CascadeBounds.Center | CardToLocalRotationX, CascadeBounds.Center | CardToLocalRotationY, CascadeBounds.Center | CardToLocalRotationZ);
					float SnapX = FMath::Fmod(LocalCenter.X, 2.0f * LocalExtent.X / GLumenDistantSceneCardResolution);
					float SnapY = FMath::Fmod(LocalCenter.Y, 2.0f * LocalExtent.Y / GLumenDistantSceneCardResolution);
					LocalCenter.X -= SnapX;
					LocalCenter.Y -= SnapY;
					CascadeCenter = LocalCenter.X * CardToLocalRotationX + LocalCenter.Y * CardToLocalRotationY + LocalCenter.Z * CardToLocalRotationZ;
				}

				LumenSceneData.Cards[DistantCardIndex].SetTransform(FMatrix::Identity, CascadeCenter, CardToLocalRotationX, CardToLocalRotationY, CardToLocalRotationZ, LocalExtent);
			}
		}
		
		if (GLumenDrawCascadeBounds)
		{
			FViewElementPDI ViewPDI(&View, nullptr, &View.DynamicPrimitiveCollector);

			for (int32 CascadeIndex = 0; CascadeIndex < NumDesiredCascades; CascadeIndex++)
			{
				const int32 DistantCardIndex = LumenSceneData.DistantCardIndices[CascadeIndex];
				const FLumenCard& DistantCard = LumenSceneData.Cards[DistantCardIndex];

				const uint8 DepthPriority = SDPG_World;
				const uint8 CardHue = (CascadeIndex * 10) & 0xFF;
				const uint8 CardSaturation = 0xFF;
				const uint8 CardValue = 0xFF;

				FLinearColor CardColor = FLinearColor::MakeFromHSV8(CardHue, CardSaturation, CardValue);
				CardColor.A = 0.5f;

				FMatrix CardToWorld = FMatrix::Identity;
				CardToWorld.SetAxes(&DistantCard.LocalToWorldRotationX, &DistantCard.LocalToWorldRotationY, &DistantCard.LocalToWorldRotationZ, &DistantCard.Origin);

				const FBox LocalBounds(-DistantCard.LocalExtent, DistantCard.LocalExtent);
				DrawWireBox(&ViewPDI, CardToWorld, LocalBounds, CardColor, DepthPriority);
			}
		}
	}
#endif
}