// Copyright Epic Games, Inc. All Rights Reserved.

#include "Policy/Simple/DisplayClusterProjectionSimplePolicy.h"

#include "IDisplayCluster.h"
#include "Game/IDisplayClusterGameManager.h"
#include "Misc/DisplayClusterHelpers.h"

#include "DisplayClusterRootActor.h"
#include "Components/DisplayClusterCameraComponent.h"
#include "Components/DisplayClusterScreenComponent.h"
#include "Components/MeshComponent.h"

#include "DisplayClusterProjectionLog.h"
#include "DisplayClusterProjectionStrings.h"

#include "Render/Viewport/IDisplayClusterViewportManager.h"
#include "Render/Viewport/IDisplayClusterViewport.h"
#include "Render/Viewport/IDisplayClusterViewportProxy.h"


FDisplayClusterProjectionSimplePolicy::FDisplayClusterProjectionSimplePolicy(const FString& ProjectionPolicyId, const FDisplayClusterConfigurationProjection* InConfigurationProjectionPolicy)
	: FDisplayClusterProjectionPolicyBase(ProjectionPolicyId, InConfigurationProjectionPolicy)
{
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterProjectionPolicy
//////////////////////////////////////////////////////////////////////////////////////////////
const FString& FDisplayClusterProjectionSimplePolicy::GetType() const
{
	static const FString Type(DisplayClusterProjectionStrings::projection::Simple);
	return Type;
}

bool FDisplayClusterProjectionSimplePolicy::HandleStartScene(IDisplayClusterViewport* InViewport)
{
	check(IsInGameThread());

	UE_LOG(LogDisplayClusterProjectionSimple, Verbose, TEXT("Initializing internals for the viewport '%s'"), *InViewport->GetId());

	// Get assigned screen ID
	if (!DisplayClusterHelpers::map::template ExtractValue(GetParameters(), DisplayClusterProjectionStrings::cfg::simple::Screen, ScreenId))
	{
		if (!IsEditorOperationMode(InViewport))
		{
			UE_LOG(LogDisplayClusterProjectionSimple, Error, TEXT("No screen ID specified for projection policy of viewport '%s'"), *InViewport->GetId());
		}

		return false;
	}

	if (InitializeMeshData(InViewport))
	{
		ViewData.Empty();
		ViewData.AddDefaulted(2);

		return true;
	}

	return false;
}

void FDisplayClusterProjectionSimplePolicy::HandleEndScene(IDisplayClusterViewport* InViewport)
{
	check(IsInGameThread());

	// Forget about screen. It will be released by the Engine.
	ScreenCompRef.ResetSceneComponent();
}

bool FDisplayClusterProjectionSimplePolicy::CalculateView(IDisplayClusterViewport* InViewport, const uint32 InContextNum, FVector& InOutViewLocation, FRotator& InOutViewRotation, const FVector& ViewOffset, const float WorldToMeters, const float NCP, const float FCP)
{
	check(IsInGameThread());

	if (InContextNum >= (uint32)ViewData.Num())
	{
		UE_LOG(LogDisplayClusterProjectionSimple, Verbose, TEXT("viewport '%s' not initialized for render"), *InViewport->GetId());
		return false;
	}

	USceneComponent* SceneComponent = ScreenCompRef.GetOrFindSceneComponent();

	// Just store the data. We will compute the frustum later on request
	ViewData[InContextNum].ViewLoc = InOutViewLocation;
	ViewData[InContextNum].NCP = NCP;
	ViewData[InContextNum].FCP = FCP;
	ViewData[InContextNum].WorldToMeters = WorldToMeters;

	// View rotation is the same as screen's rotation (orthogonal to the screen plane)
	InOutViewRotation = (SceneComponent ? SceneComponent->GetComponentRotation() : FRotator::ZeroRotator);

	return true;
}

bool FDisplayClusterProjectionSimplePolicy::GetProjectionMatrix(IDisplayClusterViewport* InViewport, const uint32 InContextNum, FMatrix& OutPrjMatrix)
{
	check(IsInGameThread());

	UE_LOG(LogDisplayClusterProjectionSimple, Verbose, TEXT("Calculating projection matrix for viewport '%s'..."), *InViewport->GetId());

	if (InContextNum >= (uint32)ViewData.Num())
	{
		UE_LOG(LogDisplayClusterProjectionSimple, Verbose, TEXT("viewport '%s' not initialized for render"), *InViewport->GetId());
		return false;
	}

	USceneComponent* SceneComponent = ScreenCompRef.GetOrFindSceneComponent();
	UDisplayClusterScreenComponent* ScreenComp = (SceneComponent != nullptr)?StaticCast<UDisplayClusterScreenComponent*>(SceneComponent) : nullptr;

	if (ScreenComp == nullptr)
	{
		OutPrjMatrix = FMatrix::Identity;
		return false;
	}

	const double n = ViewData[InContextNum].NCP;
	const double f = ViewData[InContextNum].FCP;

	// Half-size
	//@note The original code that takes the world scale into account needs
	//      a) to be removed completely from nDisplay rendering pipeline
	//      b) to be implemented for every projection policy, and properly tested
	// Currently it's not working properly so I commented it out. Once we decide what to
	// do with the world scale, I will get it back or remove completely.
#if 0
	const double hw = ScreenComp->GetScreenSize().X / 2.f * ViewData[InContextNum].WorldToMeters;
	const double hh = ScreenComp->GetScreenSize().Y / 2.f * ViewData[InContextNum].WorldToMeters;
#else
	const double hw = ScreenComp->GetScreenSize().X / 2.f;
	const double hh = ScreenComp->GetScreenSize().Y / 2.f;
#endif

	// Screen data
	const AActor* const Owner   = ScreenComp->GetOwner();
	const FTransform LocalSpace = (Owner ? Owner->GetActorTransform() : FTransform::Identity);
	const FVector    ScreenLoc  = LocalSpace.InverseTransformPositionNoScale( ScreenComp->GetComponentLocation() );
	const FRotator   ScreenRot  = LocalSpace.InverseTransformRotation( ScreenComp->GetComponentRotation().Quaternion() ).Rotator();

	// Screen corners
	const FVector pa = ScreenLoc + ScreenRot.Quaternion().RotateVector(GetProjectionScreenGeometryLBC(hw, hh)); // left bottom corner
	const FVector pb = ScreenLoc + ScreenRot.Quaternion().RotateVector(GetProjectionScreenGeometryRBC(hw, hh)); // right bottom corner
	const FVector pc = ScreenLoc + ScreenRot.Quaternion().RotateVector(GetProjectionScreenGeometryLTC(hw, hh)); // left top corner

	// Screen vectors
	FVector vr = pb - pa; // lb->rb normalized vector, right axis of projection screen
	vr.Normalize();
	FVector vu = pc - pa; // lb->lt normalized vector, up axis of projection screen
	vu.Normalize();
	FVector vn = -FVector::CrossProduct(vr, vu); // Projection plane normal. Use minus because of left-handed coordinate system
	vn.Normalize();

	const FVector pe = LocalSpace.InverseTransformPositionNoScale(ViewData[InContextNum].ViewLoc); // ViewLocation;

	const FVector va = pa - pe; // camera -> lb
	const FVector vb = pb - pe; // camera -> rb
	const FVector vc = pc - pe; // camera -> lt

	const double d = -FVector::DotProduct(va, vn); // distance from eye to screen

	static const double minScreenDistance = 10; //Minimal distance from eye to screen
	const double SafeDistance = (fabs(d) < minScreenDistance) ? minScreenDistance : d;

	const double ndifd = n / SafeDistance;

	const double l = FVector::DotProduct(vr, va) * ndifd; // distance to left screen edge
	const double r = FVector::DotProduct(vr, vb) * ndifd; // distance to right screen edge
	const double b = FVector::DotProduct(vu, va) * ndifd; // distance to bottom screen edge
	const double t = FVector::DotProduct(vu, vc) * ndifd; // distance to top screen edge

	InViewport->CalculateProjectionMatrix(InContextNum, l, r, t, b, n, f, false);
	OutPrjMatrix = InViewport->GetContexts()[InContextNum].ProjectionMatrix;

	return true;
}


//////////////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterProjectionSimplePolicy
//////////////////////////////////////////////////////////////////////////////////////////////
bool FDisplayClusterProjectionSimplePolicy::InitializeMeshData(IDisplayClusterViewport* InViewport)
{
	UE_LOG(LogDisplayClusterProjectionSimple, Verbose, TEXT("Initializing screen geometry for viewport policy  %s"), *GetId());

	// Get our VR root
	ADisplayClusterRootActor* const Root = InViewport->GetOwner().GetRootActor();
	if (!Root)
	{
		if (!IsEditorOperationMode(InViewport))
		{
			UE_LOG(LogDisplayClusterProjectionSimple, Error, TEXT("Couldn't get a VR root object"));
		}

		return false;
	}

	// Get screen component
	UDisplayClusterScreenComponent* ScreenComp = Root->GetComponentByName<UDisplayClusterScreenComponent>(ScreenId);
	if (!ScreenComp)
	{
		if (!IsEditorOperationMode(InViewport))
		{
			UE_LOG(LogDisplayClusterProjectionSimple, Warning, TEXT("Couldn't initialize screen component"));
		}

		return false;
	}

	return ScreenCompRef.SetSceneComponent(ScreenComp);
}

void FDisplayClusterProjectionSimplePolicy::ReleaseMeshData(IDisplayClusterViewport* InViewport)
{
}

#if WITH_EDITOR
UMeshComponent* FDisplayClusterProjectionSimplePolicy::GetOrCreatePreviewMeshComponent(IDisplayClusterViewport* InViewport, bool& bOutIsRootActorComponent)
{
	bOutIsRootActorComponent = true;

	USceneComponent* SceneComponent = ScreenCompRef.GetOrFindSceneComponent();

	if (SceneComponent)
	{
		UDisplayClusterScreenComponent* ScreenComp = StaticCast<UDisplayClusterScreenComponent*>(SceneComponent);
		return (ScreenComp == nullptr) ? nullptr : ScreenComp;
	}

	return nullptr;
}
#endif
