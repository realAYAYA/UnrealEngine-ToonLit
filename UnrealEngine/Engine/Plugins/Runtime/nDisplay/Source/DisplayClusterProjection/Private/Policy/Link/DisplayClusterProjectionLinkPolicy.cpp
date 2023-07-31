// Copyright Epic Games, Inc. All Rights Reserved.

#include "Policy/Link/DisplayClusterProjectionLinkPolicy.h"

#include "Misc/DisplayClusterHelpers.h"

#include "DisplayClusterProjectionLog.h"
#include "DisplayClusterProjectionStrings.h"

#include "Render/Viewport/IDisplayClusterViewportManager.h"
#include "Render/Viewport/IDisplayClusterViewport.h"
#include "Render/Viewport/IDisplayClusterViewportProxy.h"


FDisplayClusterProjectionLinkPolicy::FDisplayClusterProjectionLinkPolicy(const FString& ProjectionPolicyId, const FDisplayClusterConfigurationProjection* InConfigurationProjectionPolicy)
	: FDisplayClusterProjectionPolicyBase(ProjectionPolicyId, InConfigurationProjectionPolicy)
{
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterProjectionPolicy
//////////////////////////////////////////////////////////////////////////////////////////////
const FString& FDisplayClusterProjectionLinkPolicy::GetType() const
{
	static const FString Type(DisplayClusterProjectionStrings::projection::Link);
	return Type;
}

bool FDisplayClusterProjectionLinkPolicy::CalculateView(IDisplayClusterViewport* InViewport, const uint32 InContextNum, FVector& InOutViewLocation, FRotator& InOutViewRotation, const FVector& ViewOffset, const float WorldToMeters, const float NCP, const float FCP)
{
	check(IsInGameThread());
	check(InViewport);

	IDisplayClusterViewport* ParentViewport = InViewport->GetOwner().FindViewport(InViewport->GetRenderSettings().GetParentViewportId());
	if (ParentViewport != nullptr)
	{
		const TArray<FDisplayClusterViewport_Context>& ParentContexts = ParentViewport->GetContexts();
		if (InContextNum < (uint32)ParentContexts.Num())
		{
			if (ParentContexts[InContextNum].bDisableRender && ParentViewport->GetProjectionPolicy().IsValid())
			{
				return ParentViewport->GetProjectionPolicy()->CalculateView(ParentViewport, InContextNum, InOutViewLocation, InOutViewRotation, ViewOffset, WorldToMeters, NCP, FCP);
			}
			else
			{
				const FDisplayClusterViewport_Context& ParentContext = ParentContexts[InContextNum];
				InOutViewLocation = ParentContext.ViewLocation;
				InOutViewRotation = ParentContext.ViewRotation;

				return true;
			}
		}
	}

	return false;
}

bool FDisplayClusterProjectionLinkPolicy::GetProjectionMatrix(IDisplayClusterViewport* InViewport, const uint32 InContextNum, FMatrix& OutPrjMatrix)
{
	check(IsInGameThread());
	check(InViewport);

	IDisplayClusterViewport* ParentViewport = InViewport->GetOwner().FindViewport(InViewport->GetRenderSettings().GetParentViewportId());
	if (ParentViewport != nullptr)
	{
		const TArray<FDisplayClusterViewport_Context>& ParentContexts = ParentViewport->GetContexts();
		if (InContextNum < (uint32)ParentContexts.Num())
		{
			if (ParentContexts[InContextNum].bDisableRender && ParentViewport->GetProjectionPolicy().IsValid())
			{
				return ParentViewport->GetProjectionPolicy()->GetProjectionMatrix(ParentViewport, InContextNum, OutPrjMatrix);
			}
			else
			{
				const FDisplayClusterViewport_Context& ParentContext = ParentContexts[InContextNum];
				OutPrjMatrix = ParentContext.ProjectionMatrix;

				return true;
			}
		}
	}

	return false;
}
