// Copyright Epic Games, Inc. All Rights Reserved.

#include "Policy/Domeprojection/DisplayClusterProjectionDomeprojectionPolicyBase.h"

#include "DisplayClusterProjectionLog.h"
#include "DisplayClusterProjectionStrings.h"

#include "Misc/DisplayClusterHelpers.h"

#include "IDisplayCluster.h"
#include "Config/IDisplayClusterConfigManager.h"
#include "Game/IDisplayClusterGameManager.h"

#include "Render/Viewport/IDisplayClusterViewport.h"
#include "Render/Viewport/IDisplayClusterViewportProxy.h"

int32 GDisplayClusterProjectionDomeprojectionPolicyEnableNearClippingPlane = 0;
static FAutoConsoleVariableRef CVarDisplayClusterProjectionDomeprojectionPolicyEnableNearClippingPlane(
	TEXT("nDisplay.render.projection.EnableNearClippingPlane.Dome"),
	GDisplayClusterProjectionDomeprojectionPolicyEnableNearClippingPlane,
	TEXT("Enable NearClippingPlane for DomeProjection (0 - disable).\n"),
	ECVF_RenderThreadSafe
);

//////////////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterProjectionDomeprojectionPolicyBase
//////////////////////////////////////////////////////////////////////////////////////////////
FDisplayClusterProjectionDomeprojectionPolicyBase::FDisplayClusterProjectionDomeprojectionPolicyBase(const FString& ProjectionPolicyId, const FDisplayClusterConfigurationProjection* InConfigurationProjectionPolicy)
	: FDisplayClusterProjectionPolicyBase(ProjectionPolicyId, InConfigurationProjectionPolicy)
{
}

const FString& FDisplayClusterProjectionDomeprojectionPolicyBase::GetType() const
{
	static const FString& Type(DisplayClusterProjectionStrings::projection::Domeprojection);
	return Type;
}

bool FDisplayClusterProjectionDomeprojectionPolicyBase::HandleStartScene(IDisplayClusterViewport* InViewport)
{
	check(IsInGameThread());

	// The game side of the nDisplay has been initialized by the nDisplay Game Manager already
	// so we can extend it by our projection related functionality/components/etc.	

	// Read domeprojection config data from nDisplay config file
	FString File;
	if (!ReadConfigData(InViewport, File, OriginCompId, DomeprojectionChannel))
	{
		if (!IsEditorOperationMode(InViewport))
		{
			UE_LOG(LogDisplayClusterProjectionDomeprojection, Error, TEXT("Couldn't read Domeprojection configuration from the config file"));
		}

		return false;
	}

	if (File.IsEmpty())
	{
		if (!IsEditorOperationMode(InViewport))
		{
			UE_LOG(LogDisplayClusterProjectionDomeprojection, Error, TEXT("Domeprojection configuration file is empty"));
		}

		return false;
	}

	// Find origin component if it exists
	InitializeOriginComponent(InViewport, OriginCompId);

	const FString FullFilePath = DisplayClusterHelpers::filesystem::GetFullPathForConfigResource(File);
	if (!FPaths::FileExists(FullFilePath))
	{
		UE_LOG(LogDisplayClusterProjectionDomeprojection, Error, TEXT("Couldn't read Domeprojection configuration file: %s"), *FullFilePath);
		return false;
	}

	// Create and store nDisplay-to-Domeprojection viewport adapter
	const int32 MaxViewsAmount = 2; // always init contexts for stereo

	ViewAdapter = CreateViewAdapter(FDisplayClusterProjectionDomeprojectionViewAdapterBase::FInitParams{ MaxViewsAmount });
	if (!(ViewAdapter && ViewAdapter->Initialize(InViewport, FullFilePath)))
	{
		if (!IsEditorOperationMode(InViewport))
		{
			UE_LOG(LogDisplayClusterProjectionDomeprojection, Error, TEXT("An error occurred during Domeprojection viewport adapter initialization"));
		}
		return false;
	}

	UE_LOG(LogDisplayClusterProjectionDomeprojection, Verbose, TEXT("A Domeprojection viewport adapter has been initialized"));

	return true;
}

void FDisplayClusterProjectionDomeprojectionPolicyBase::HandleEndScene(IDisplayClusterViewport* InViewport)
{
	check(IsInGameThread());

	ReleaseOriginComponent();
}


bool FDisplayClusterProjectionDomeprojectionPolicyBase::CalculateView(IDisplayClusterViewport* InViewport, const uint32 InContextNum, FVector& InOutViewLocation, FRotator& InOutViewRotation, const FVector& ViewOffset, const float WorldToMeters, const float InNCP, const float InFCP)
{
	check(IsInGameThread());

	if (!ViewAdapter.IsValid())
	{
		return false;
	}

	const float NCP = GDisplayClusterProjectionDomeprojectionPolicyEnableNearClippingPlane ? InNCP : 1;
	const float FCP = GDisplayClusterProjectionDomeprojectionPolicyEnableNearClippingPlane ? InFCP : 1;

	// Get origin component
	const USceneComponent* const OriginComp = GetOriginComp();
	check(OriginComp);

	// Get world-origin matrix
	const FTransform& World2LocalTransform = (OriginComp != nullptr ? OriginComp->GetComponentTransform() : FTransform::Identity);

	// Calculate view location in origin space
	FVector OriginSpaceViewLocation = World2LocalTransform.InverseTransformPosition(InOutViewLocation);

	// Forward data to the RHI dependend Domeprojection implementation
	FRotator OriginSpaceViewRotation = FRotator::ZeroRotator;
	if (!ViewAdapter->CalculateView(InViewport, InContextNum, DomeprojectionChannel, OriginSpaceViewLocation, OriginSpaceViewRotation, ViewOffset, WorldToMeters, NCP, FCP))
	{
		if (!IsEditorOperationMode(InViewport))
		{
			UE_LOG(LogDisplayClusterProjectionDomeprojection, Warning, TEXT("Couldn't compute view info for <%s> viewport"), *InViewport->GetId());
		}

		return false;
	}

	// Convert rotation back from origin to world space
	InOutViewRotation = World2LocalTransform.TransformRotation(OriginSpaceViewRotation.Quaternion()).Rotator();

	// Convert location back from origin to world space
	InOutViewLocation = World2LocalTransform.TransformPosition(OriginSpaceViewLocation);

	return true;
}

bool FDisplayClusterProjectionDomeprojectionPolicyBase::GetProjectionMatrix(IDisplayClusterViewport* InViewport, const uint32 InContextNum, FMatrix& OutPrjMatrix)
{
	check(IsInGameThread());

	if (!ViewAdapter.IsValid())
	{
		return false;
	}

	// Pass request to the adapter
	return ViewAdapter->GetProjectionMatrix(InViewport, InContextNum, DomeprojectionChannel, OutPrjMatrix);
}

bool FDisplayClusterProjectionDomeprojectionPolicyBase::IsWarpBlendSupported()
{
	return true;
}

void FDisplayClusterProjectionDomeprojectionPolicyBase::ApplyWarpBlend_RenderThread(FRHICommandListImmediate& RHICmdList, const IDisplayClusterViewportProxy* InViewportProxy)
{
	check(IsInRenderingThread());

	if (ViewAdapter.IsValid())
	{
		ViewAdapter->ApplyWarpBlend_RenderThread(RHICmdList, InViewportProxy, DomeprojectionChannel);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterProjectionDomeprojectionPolicy
//////////////////////////////////////////////////////////////////////////////////////////////
bool FDisplayClusterProjectionDomeprojectionPolicyBase::ReadConfigData(IDisplayClusterViewport* InViewport, FString& OutFile, FString& OutOrigin, uint32& OutChannel)
{
	check(InViewport);

	const FString InViewportId = InViewport->GetId();

	// Domeprojection file (mandatory)
	if (DisplayClusterHelpers::map::ExtractValue(GetParameters(), DisplayClusterProjectionStrings::cfg::domeprojection::File, OutFile))
	{
		UE_LOG(LogDisplayClusterProjectionDomeprojection, Verbose, TEXT("Viewport <%s>: Projection parameter '%s' - '%s'"), *InViewportId, DisplayClusterProjectionStrings::cfg::domeprojection::File, *OutFile);
	}
	else
	{
		if (!IsEditorOperationMode(InViewport))
		{
			UE_LOG(LogDisplayClusterProjectionDomeprojection, Error, TEXT("Viewport <%s>: Projection parameter '%s' not found"), *InViewportId, DisplayClusterProjectionStrings::cfg::domeprojection::File);
		}

		return false;
	}

	// Channel (mandatory)
	if(DisplayClusterHelpers::map::ExtractValueFromString(GetParameters(), DisplayClusterProjectionStrings::cfg::domeprojection::Channel, OutChannel))
	{
		UE_LOG(LogDisplayClusterProjectionDomeprojection, Verbose, TEXT("Viewport <%s>: Projection parameter '%s' - '%d'"), *InViewportId, DisplayClusterProjectionStrings::cfg::domeprojection::Channel, OutChannel);
	}
	else
	{
		if (!IsEditorOperationMode(InViewport))
		{
			UE_LOG(LogDisplayClusterProjectionDomeprojection, Error, TEXT("Viewport <%s>: Parameter <%s> not found in the config file"), *InViewportId, DisplayClusterProjectionStrings::cfg::domeprojection::Channel);
		}

		return false;
	}

	// Origin node (optional)
	if (DisplayClusterHelpers::map::ExtractValue(GetParameters(), DisplayClusterProjectionStrings::cfg::domeprojection::Origin, OutOrigin))
	{
		UE_LOG(LogDisplayClusterProjectionDomeprojection, Verbose, TEXT("Viewport <%s>: Projection parameter '%s' - '%s'"), *InViewportId, DisplayClusterProjectionStrings::cfg::domeprojection::Origin, *OutOrigin);
	}
	else
	{
		if (!IsEditorOperationMode(InViewport))
		{
			UE_LOG(LogDisplayClusterProjectionDomeprojection, Log, TEXT("Viewport <%s>: No <%s> parameter found for projection %s"), *InViewportId, DisplayClusterProjectionStrings::cfg::domeprojection::Origin, *OutOrigin);
		}
	}

	return true;
}
