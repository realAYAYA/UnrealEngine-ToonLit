// Copyright Epic Games, Inc. All Rights Reserved.

#include "Policy/InFrustumFit/DisplayClusterWarpInFrustumFitPolicy.h"

#include "DisplayClusterRootActor.h"
#include "Containers/DisplayClusterWarpEye.h"

#include "Render/Viewport/IDisplayClusterViewportPreview.h"
#include "Render/Viewport/IDisplayClusterViewport.h"
#include "Render/Projection/IDisplayClusterProjectionPolicy.h"

#include "IDisplayClusterWarpBlend.h"
#include "PDisplayClusterWarpStrings.h"

#include "Components/DisplayClusterCameraComponent.h"
#include "Components/DisplayClusterInFrustumFitCameraComponent.h"
#include "Components/MeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

// Experimental: Enable static view direction for mpcdi 2d
int32 GDisplayClusterWarpInFrustumFitPolicyUseStaticViewDirectionForMPCDIProfile2D = 1;
static FAutoConsoleVariableRef CVarDisplayClusterWarpInFrustumFitPolicyUseStaticViewDirectionForMPCDIProfile2D(
	TEXT("nDisplay.warp.InFrustumFit.UseStaticViewDirectionForMPCDIProfile2D"),
	GDisplayClusterWarpInFrustumFitPolicyUseStaticViewDirectionForMPCDIProfile2D,
	TEXT("Experimental: Enable static view direction for mpcdi 2d (0 - disable)\n"),
	ECVF_Default
);

// Experimental: Enable single view target for group of viewports
int32 GDisplayClusterWarpInFrustumFitPolicyUseGroupViewTarget = 1;
static FAutoConsoleVariableRef CVarDisplayClusterWarpInFrustumFitPolicyUseGroupViewTarget(
	TEXT("nDisplay.warp.InFrustumFit.UseGroupViewTarget"),
	GDisplayClusterWarpInFrustumFitPolicyUseGroupViewTarget,
	TEXT("Experimental: Enable single view target for group of viewports (0 - disable)\n"),
	ECVF_Default
);

int32 GDisplayClusterWarpInFrustumFitPolicyDrawFrustum = 0;
static FAutoConsoleVariableRef CVarDisplayClusterWarpInFrustumFitPolicyDrawFrustum(
	TEXT("nDisplay.warp.InFrustumFit.DrawFrustum"),
	GDisplayClusterWarpInFrustumFitPolicyDrawFrustum,
	TEXT("Toggles drawing the stage geometry frustum and bounding box\n"),
	ECVF_Default
);

//-------------------------------------------------------------------
// FDisplayClusterWarpInFrustumFitPolicy
//-------------------------------------------------------------------
FDisplayClusterWarpInFrustumFitPolicy::FDisplayClusterWarpInFrustumFitPolicy(const FString& InWarpPolicyName)
	: FDisplayClusterWarpPolicyBase(GetType(), InWarpPolicyName)
{ }

const FString& FDisplayClusterWarpInFrustumFitPolicy::GetType() const
{
	static const FString Type(UE::DisplayClusterWarpStrings::warp::InFrustumFit);

	return Type;
}

void FDisplayClusterWarpInFrustumFitPolicy::HandleNewFrame(const TArray<TSharedPtr<IDisplayClusterViewport, ESPMode::ThreadSafe>>& InViewports)
{
	// Todo: get real scale
	const float WorldToMeters = 100.0f;

	const float WorldScale = WorldToMeters / 100.f;
	
	bGeometryContextsUpdated = false;
	if (GDisplayClusterWarpInFrustumFitPolicyUseGroupViewTarget)
	{
		// Calculate AABB for group of viewports:
		GroupAABBox = FDisplayClusterWarpAABB();
		for (const TSharedPtr<IDisplayClusterViewport, ESPMode::ThreadSafe>& Viewport : InViewports)
		{
			if (Viewport.IsValid() && Viewport->GetProjectionPolicy().IsValid())
			{
				TSharedPtr<IDisplayClusterWarpBlend, ESPMode::ThreadSafe> WarpBlend;
				if (Viewport->GetProjectionPolicy()->GetWarpBlendInterface(WarpBlend))
				{
					if (WarpBlend->UpdateGeometryContext(WorldScale))
					{
						GroupAABBox.UpdateAABB(WarpBlend->GetGeometryContext().AABBox);
					}
				}
			}
		}

		bGeometryContextsUpdated = true;
	}

	const uint32 ContextNum = 0; // calculate for a single context

	bValidGroupFrustum = false;

	// Recalculate warp projection angles
	GroupGeometryWarpProjection.ResetProjectionAngles();
	SymmetricForwardCorrection.Reset();

	bool bCanCalcFrustumContext = true;
	bool bHasFixedViewDirection = false;

	// Use center of GroupAABB as target viewpoint, and setup to use same viewprojection plane for all viewports:
	for (const TSharedPtr<IDisplayClusterViewport, ESPMode::ThreadSafe>& Viewport : InViewports)
	{
		if (bCanCalcFrustumContext && Viewport.IsValid() && Viewport->GetProjectionPolicy().IsValid())
		{
			// Note: This code is partially copied from FDisplayClusterProjectionMPCDIPolicy::CalculateView().

			// Override viewpoint
			// MPCDI always expects the location of the viewpoint component (eye location from the real world)
			FVector ViewOffset = FVector::ZeroVector;
			FVector InOutViewLocation;
			FRotator InOutViewRotation;
			if (!Viewport->GetViewPointCameraEye(ContextNum, InOutViewLocation, InOutViewRotation, ViewOffset))
			{
				continue;
			}

			if (UDisplayClusterInFrustumFitCameraComponent* ConfigurationCameraComponent = Cast<UDisplayClusterInFrustumFitCameraComponent>(Viewport->GetViewPointCameraComponent(EDisplayClusterRootActorType::Configuration)))
			{
				bHasFixedViewDirection = ConfigurationCameraComponent->CameraViewTarget == EDisplayClusterWarpCameraViewTarget::MatchViewOrigin;
			}

			TSharedPtr<IDisplayClusterWarpBlend, ESPMode::ThreadSafe> WarpBlend;
			if (Viewport->GetProjectionPolicy()->GetWarpBlendInterface(WarpBlend))
			{
				const USceneComponent* const OriginComp = Viewport->GetProjectionPolicy()->GetOriginComponent();

				TSharedPtr<FDisplayClusterWarpEye, ESPMode::ThreadSafe> WarpEye = MakeShared<FDisplayClusterWarpEye, ESPMode::ThreadSafe>(Viewport, 0);

				WarpEye->World2LocalTransform = (OriginComp ? OriginComp->GetComponentTransform() : FTransform::Identity);

				// Get our base camera location and view offset in local space (MPCDI space)
				WarpEye->ViewPoint.Location = WarpEye->World2LocalTransform.InverseTransformPosition(InOutViewLocation - ViewOffset);
				WarpEye->ViewPoint.EyeOffset = WarpEye->World2LocalTransform.InverseTransformPosition(InOutViewLocation) - WarpEye->ViewPoint.Location;
				WarpEye->ViewPoint.Rotation = WarpEye->World2LocalTransform.InverseTransformRotation(InOutViewRotation.Quaternion()).Rotator();

				WarpEye->WorldScale = WorldScale;

				WarpEye->WarpPolicy = SharedThis(this);

				if (!WarpBlend->CalcFrustumContext(WarpEye))
				{
					bCanCalcFrustumContext = false;
				}
				else
				{
					const FDisplayClusterWarpData& WarpData = WarpBlend->GetWarpData(ContextNum);
					GroupGeometryWarpProjection.ExpandProjectionAngles(WarpData.GeometryWarpProjection);
				}
			}
		}
	}

	bValidGroupFrustum = bCanCalcFrustumContext;

	// Recalculate the group frustum so that it is symmetric
	MakeGroupFrustumSymmetrical(bHasFixedViewDirection);
}

void FDisplayClusterWarpInFrustumFitPolicy::Tick(IDisplayClusterViewportManager* InViewportManager, float DeltaSeconds)
{
#if WITH_EDITOR
	if (GDisplayClusterWarpInFrustumFitPolicyDrawFrustum)
	{
		for (const TSharedPtr<IDisplayClusterViewport, ESPMode::ThreadSafe>& Viewport : InViewportManager->GetEntireClusterViewportsForWarpPolicy(SharedThis(this)))
		{
			// Getting data from the first viewport, since all viewports use the same ViewPoint component
			if (UDisplayClusterInFrustumFitCameraComponent* SceneCameraComponent = Cast<UDisplayClusterInFrustumFitCameraComponent>(Viewport->GetViewPointCameraComponent(EDisplayClusterRootActorType::Scene)))
			{
				if (ADisplayClusterRootActor* SceneRootActor = InViewportManager->GetConfiguration().GetRootActor(EDisplayClusterRootActorType::Scene))
				{
					if (SceneCameraComponent)
					{
						DrawDebugGroupFrustum(SceneRootActor, SceneCameraComponent, FColor::Blue);
					}

					DrawDebugGroupBoundingBox(SceneRootActor, FColor::Red);
				}
			}

			break;
		}
	}
#endif
}

bool FDisplayClusterWarpInFrustumFitPolicy::HasPreviewEditableMesh(IDisplayClusterViewport* InViewport)
{
	// This warp policy is based on IDisplayClusterWarpBlend only.
	// Process only viewports with a projection policy based on the warpblend interface.
	TSharedPtr<IDisplayClusterWarpBlend, ESPMode::ThreadSafe> WarpBlend;
	if (!InViewport || !InViewport->GetProjectionPolicy().IsValid() || !InViewport->GetProjectionPolicy()->GetWarpBlendInterface(WarpBlend))
	{
		return false;
	}

	// If the preview is not used in this configuration
	if (!InViewport->GetConfiguration().IsPreviewRendering() || InViewport->GetConfiguration().GetRootActor(EDisplayClusterRootActorType::Preview) == nullptr)
	{
		return false;
	}

	// If owner DCRA world is EditorPreview dont show editable mesh (Configurator, ICVFX Panel, etc)
	if (InViewport->GetConfiguration().IsRootActorWorldHasAnyType(EDisplayClusterRootActorType::Preview, EWorldType::EditorPreview))
	{
		return false;
	}

	// The editable mesh is an option for the UDisplayClusterInFrustumFitCameraComponent.
	if (UDisplayClusterInFrustumFitCameraComponent* ConfigurationCameraComponent = Cast<UDisplayClusterInFrustumFitCameraComponent>(InViewport->GetViewPointCameraComponent(EDisplayClusterRootActorType::Configuration)))
	{
		if (ConfigurationCameraComponent->bShowPreviewFrustumFit)
		{
			return true;
		}
	}

	return false;
}

void FDisplayClusterWarpInFrustumFitPolicy::BeginCalcFrustum(IDisplayClusterViewport* InViewport, const uint32 ContextNum)
{
	if (InViewport && InViewport->GetProjectionPolicy().IsValid())
	{
		TSharedPtr<IDisplayClusterWarpBlend, ESPMode::ThreadSafe> WarpBlend;
		if (InViewport->GetProjectionPolicy()->GetWarpBlendInterface(WarpBlend))
		{
			if (UDisplayClusterInFrustumFitCameraComponent* ConfigurationCameraComponent = Cast<UDisplayClusterInFrustumFitCameraComponent>(InViewport->GetViewPointCameraComponent(EDisplayClusterRootActorType::Configuration)))
			{
				FDisplayClusterWarpData& WarpData = WarpBlend->GetWarpData(ContextNum);
				if (WarpData.WarpEye.IsValid())
				{
					// geometry context already updated.
					WarpData.WarpEye->bUpdateGeometryContext = !bGeometryContextsUpdated;

					if (ConfigurationCameraComponent->CameraViewTarget == EDisplayClusterWarpCameraViewTarget::MatchViewOrigin)
					{
						WarpData.WarpEye->OverrideViewDirection = WarpData.WarpEye->ViewPoint.Rotation.RotateVector(FVector::XAxisVector);
					}
					else
					{
						if (GDisplayClusterWarpInFrustumFitPolicyUseGroupViewTarget)
						{
							// If we have a correction to the view forward vector to make the frustum symmetric, apply it
							if (SymmetricForwardCorrection.IsSet())
							{
								const FVector AABBForward = (GroupAABBox.GetCenter() - WarpData.WarpEye->ViewPoint.GetEyeLocation()).GetSafeNormal();
								WarpData.WarpEye->OverrideViewDirection = SymmetricForwardCorrection->RotateVector(AABBForward);
							}
							else
							{
								// Use the same view target for all viewports
								WarpData.WarpEye->OverrideViewTarget = GroupAABBox.GetCenter();
							}
						}
					}

					if (GDisplayClusterWarpInFrustumFitPolicyUseStaticViewDirectionForMPCDIProfile2D)
					{
						// [experimental] use static view direction for MPCDI profile 2D
						if (WarpBlend->GetWarpProfileType() == EDisplayClusterWarpProfileType::warp_2D)
						{
							WarpData.WarpEye->OverrideViewDirection = FVector(1, 0, 0);
						}
					}

					// Todo: This feature now not supported here. need to be fixed.
					WarpData.bEnabledRotateFrustumToFitContextSize = false;
				}
			}
		}
	}
}

void FDisplayClusterWarpInFrustumFitPolicy::EndCalcFrustum(IDisplayClusterViewport* InViewport, const uint32 ContextNum)
{
	if (bValidGroupFrustum && InViewport && InViewport->GetProjectionPolicy().IsValid())
	{
		TSharedPtr<IDisplayClusterWarpBlend, ESPMode::ThreadSafe> WarpBlend;
		if (InViewport->GetProjectionPolicy()->GetWarpBlendInterface(WarpBlend))
		{
			// Change warp settings:
			FDisplayClusterWarpData& WarpData = WarpBlend->GetWarpData(ContextNum);

			// Apply camera frustum fitting:
			FDisplayClusterWarpProjection NewWarpProjection = ApplyInFrustumFit(InViewport, WarpData.WarpEye->World2LocalTransform, WarpData.WarpProjection);
			if (NewWarpProjection.IsValidProjection())
			{
				WarpData.WarpProjection = NewWarpProjection;

				// The warp policy Tick() function uses warp data, and it must be sure that this data is updated in the previous frame.
				//.This value must be set to true from the EndCalcFrustum() warp policy function when changes are made to this structure.
				WarpData.bHasWarpPolicyChanges = true;
			}
		}
	}
}

void FDisplayClusterWarpInFrustumFitPolicy::OnUpdateDisplayDeviceMeshAndMaterialInstance(IDisplayClusterViewportPreview& InViewportPreview, const EDisplayClusterDisplayDeviceMeshType InMeshType, const EDisplayClusterDisplayDeviceMaterialType InMaterialType, UMeshComponent* InMeshComponent, UMaterialInstanceDynamic* InMeshMaterialInstance) const
{
	// The preview material used for editable meshes requires a set of unique parameters that are set from the warp policy.
	check(InMeshComponent && InMeshMaterialInstance);

	if (InMeshType != EDisplayClusterDisplayDeviceMeshType::PreviewEditableMesh)
	{
		// Only for editable mesh
		return;
	}

	// Process only viewports with a projection policy based on the warpblend interface.
	TSharedPtr<IDisplayClusterWarpBlend, ESPMode::ThreadSafe> WarpBlend;
	IDisplayClusterViewport* InViewport = InViewportPreview.GetViewport();
	if (!InViewport || !InViewport->GetProjectionPolicy().IsValid() || !InViewport->GetProjectionPolicy()->GetWarpBlendInterface(WarpBlend))
	{
		return;
	}

	// Not all projection policies support a editable mesh.
	const FDisplayClusterWarpData& WarpData = WarpBlend->GetWarpData(0);
	if (WarpData.bValid && WarpData.bHasWarpPolicyChanges)
	{
		const FTransform CameraTransform(WarpData.WarpProjection.CameraRotation.Quaternion(), WarpData.WarpProjection.CameraLocation);

		const float HScale = (WarpData.WarpProjection.Left - WarpData.WarpProjection.Right) / (WarpData.GeometryWarpProjection.Left - WarpData.GeometryWarpProjection.Right);
		const float VScale = (WarpData.WarpProjection.Top - WarpData.WarpProjection.Bottom) / (WarpData.GeometryWarpProjection.Top - WarpData.GeometryWarpProjection.Bottom);

		checkf(FMath::IsNearlyEqual(HScale, VScale), TEXT("Streching the stage geometry to fit a different aspect ratio is not supported!"));
		const FVector Scale(1, HScale, HScale);

		const FMatrix CameraBasis = FRotationMatrix::Make(WarpData.WarpProjection.CameraRotation).Inverse();

		// Compute the relative transform from the view origin to the geometry
		FTransform RelativeTransform = FTransform(WarpData.WarpContext.MeshToStageMatrix * WarpData.Local2World.Inverse());
		RelativeTransform.ScaleTranslation(Scale);

		// Final transform is computed from the relative transform of the geometry to the view point, the frustum fit transform
		// which will scale and position the geometry based on the fitted frustum, and the camera transform
		const FTransform FinalTransform = RelativeTransform * CameraTransform;
		InMeshComponent->SetRelativeTransform(FinalTransform);

		// Since the mesh needs to be skewed to scale appropriately, and since Unreal Engine does not support a skew transform
		// through FTransform, the mesh needs to be skewed through the vertex shader using WorldPositionOffset,
		// so pass in the "global" scale to the preview mesh's material instance
		InMeshMaterialInstance->SetVectorParameterValue(UE::DisplayClusterWarpStrings::InFrustumFit::material::attr::GlobalScale, Scale);
		InMeshMaterialInstance->SetVectorParameterValue(UE::DisplayClusterWarpStrings::InFrustumFit::material::attr::GlobalForward, CameraBasis.GetUnitAxis(EAxis::X));
		InMeshMaterialInstance->SetVectorParameterValue(UE::DisplayClusterWarpStrings::InFrustumFit::material::attr::GlobalRight, CameraBasis.GetUnitAxis(EAxis::Y));
		InMeshMaterialInstance->SetVectorParameterValue(UE::DisplayClusterWarpStrings::InFrustumFit::material::attr::GlobalUp, CameraBasis.GetUnitAxis(EAxis::Z));
	}
}

#if WITH_EDITOR
#include "Components/LineBatchComponent.h"

void FDisplayClusterWarpInFrustumFitPolicy::DrawDebugGroupBoundingBox(ADisplayClusterRootActor* SceneRootActor, const FLinearColor& Color)
{
	// DCRA uses its own LineBatcher
	ULineBatchComponent* LineBatcher = SceneRootActor ? SceneRootActor->GetLineBatchComponent() : nullptr;
	UWorld* World = SceneRootActor ? SceneRootActor->GetWorld() : nullptr;
	if (LineBatcher && World)
	{
		const float Thickness = 1.f;
		const float PointSize = 5.f;
		const FBox WorldBox = GroupAABBox.TransformBy(SceneRootActor->GetActorTransform());

		LineBatcher->DrawBox(WorldBox.GetCenter(), WorldBox.GetExtent(), Color, 0, SDPG_World, Thickness);
		LineBatcher->DrawPoint(WorldBox.GetCenter(), Color, PointSize, SDPG_World);
	}
}

void FDisplayClusterWarpInFrustumFitPolicy::DrawDebugGroupFrustum(ADisplayClusterRootActor* SceneRootActor, UDisplayClusterInFrustumFitCameraComponent* CameraComponent, const FLinearColor& Color)
{
	// DCRA uses its own LineBatcher
	ULineBatchComponent* LineBatcher = SceneRootActor ? SceneRootActor->GetLineBatchComponent() : nullptr;
	if (LineBatcher && CameraComponent)
	{
		UWorld* World = SceneRootActor->GetWorld();
		IDisplayClusterViewportConfiguration* ViewportConfiguration = SceneRootActor->GetViewportConfiguration();
		if (ViewportConfiguration && World)
		{
			const float Thickness = 1.0f;

			// Get the configuration in use
			const UDisplayClusterInFrustumFitCameraComponent& ConfigurationCameraComponent = CameraComponent->GetConfigurationInFrustumFitCameraComponent(*ViewportConfiguration);

			const float NearPlane = 10;
			const float FarPlane = 1000;

			const FVector CameraLoc = CameraComponent->GetComponentLocation();
			FVector ViewDirection;

			if (ConfigurationCameraComponent.CameraViewTarget == EDisplayClusterWarpCameraViewTarget::MatchViewOrigin)
			{
				ViewDirection = CameraComponent->GetComponentRotation().RotateVector(FVector::XAxisVector);
			}
			else
			{
				const FBox WorldBox = GroupAABBox.TransformBy(SceneRootActor->GetActorTransform());
				ViewDirection = (WorldBox.GetCenter() - CameraComponent->GetComponentLocation()).GetSafeNormal();

				if (SymmetricForwardCorrection.IsSet())
				{
					ViewDirection = SymmetricForwardCorrection->RotateVector(ViewDirection);
				}
			}


			const FRotator ViewRotator = ViewDirection.ToOrientationRotator();
			const FVector FrustumTopLeft = ViewRotator.RotateVector(FVector(GroupGeometryWarpProjection.ZNear, GroupGeometryWarpProjection.Left, GroupGeometryWarpProjection.Top) / GroupGeometryWarpProjection.ZNear);
			const FVector FrustumTopRight = ViewRotator.RotateVector(FVector(GroupGeometryWarpProjection.ZNear, GroupGeometryWarpProjection.Right, GroupGeometryWarpProjection.Top) / GroupGeometryWarpProjection.ZNear);
			const FVector FrustumBottomLeft = ViewRotator.RotateVector(FVector(GroupGeometryWarpProjection.ZNear, GroupGeometryWarpProjection.Left, GroupGeometryWarpProjection.Bottom) / GroupGeometryWarpProjection.ZNear);
			const FVector FrustumBottomRight = ViewRotator.RotateVector(FVector(GroupGeometryWarpProjection.ZNear, GroupGeometryWarpProjection.Right, GroupGeometryWarpProjection.Bottom) / GroupGeometryWarpProjection.ZNear);

			const FVector FrustumVertices[8] =
			{
				CameraLoc + FrustumTopLeft * NearPlane,
				CameraLoc + FrustumTopRight * NearPlane,
				CameraLoc + FrustumBottomRight * NearPlane,
				CameraLoc + FrustumBottomLeft * NearPlane,

				CameraLoc + FrustumTopLeft * FarPlane,
				CameraLoc + FrustumTopRight * FarPlane,
				CameraLoc + FrustumBottomRight * FarPlane,
				CameraLoc + FrustumBottomLeft * FarPlane,
			};

			LineBatcher->DrawLine(CameraLoc, CameraLoc + ViewDirection * 50, Color, SDPG_World, Thickness, 0.f);

			// Near plane rectangle
			LineBatcher->DrawLine(FrustumVertices[0], FrustumVertices[1], Color, SDPG_World, Thickness, 0.f);
			LineBatcher->DrawLine(FrustumVertices[1], FrustumVertices[2], Color, SDPG_World, Thickness, 0.f);
			LineBatcher->DrawLine(FrustumVertices[2], FrustumVertices[3], Color, SDPG_World, Thickness, 0.f);
			LineBatcher->DrawLine(FrustumVertices[3], FrustumVertices[0], Color, SDPG_World, Thickness, 0.f);

			// Frustum
			LineBatcher->DrawLine(FrustumVertices[0], FrustumVertices[4], Color, SDPG_World, Thickness, 0.f);
			LineBatcher->DrawLine(FrustumVertices[1], FrustumVertices[5], Color, SDPG_World, Thickness, 0.f);
			LineBatcher->DrawLine(FrustumVertices[2], FrustumVertices[6], Color, SDPG_World, Thickness, 0.f);
			LineBatcher->DrawLine(FrustumVertices[3], FrustumVertices[7], Color, SDPG_World, Thickness, 0.f);

			// Far plane rectangle
			LineBatcher->DrawLine(FrustumVertices[4], FrustumVertices[5], Color, SDPG_World, Thickness, 0.f);
			LineBatcher->DrawLine(FrustumVertices[5], FrustumVertices[6], Color, SDPG_World, Thickness, 0.f);
			LineBatcher->DrawLine(FrustumVertices[6], FrustumVertices[7], Color, SDPG_World, Thickness, 0.f);
			LineBatcher->DrawLine(FrustumVertices[7], FrustumVertices[4], Color, SDPG_World, Thickness, 0.f);
		}
	}
}
#endif
