// Copyright Epic Games, Inc. All Rights Reserved.

#include "Policy/DisplayClusterProjectionPolicyBase.h"

#include "DisplayClusterProjectionLog.h"

#include "IDisplayCluster.h"
#include "Game/IDisplayClusterGameManager.h"

#include "DisplayClusterRootActor.h"

#include "Render/Viewport/IDisplayClusterViewportManager.h"
#include "Render/Viewport/IDisplayClusterViewport.h"


//////////////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterProjectionPolicyBase
//////////////////////////////////////////////////////////////////////////////////////////////
FDisplayClusterProjectionPolicyBase::FDisplayClusterProjectionPolicyBase(const FString& InProjectionPolicyId, const FDisplayClusterConfigurationProjection* InConfigurationProjectionPolicy)
	: PolicyInstanceId(InProjectionPolicyId)
{
	Parameters.Append(InConfigurationProjectionPolicy->Parameters);
}


bool FDisplayClusterProjectionPolicyBase::IsEditorOperationMode_RenderThread(const IDisplayClusterViewportProxy* InViewportProxy)
{
	check(IsInRenderingThread());

	if (const bool bIsClusterOperationMode = IDisplayCluster::Get().GetOperationMode() == EDisplayClusterOperationMode::Cluster)
	{
		return false;
	}

	return true;
}

bool FDisplayClusterProjectionPolicyBase::IsEditorOperationMode(IDisplayClusterViewport* InViewport)
{
	check(IsInGameThread());

	// Hide spam in logs when configuring VP in editor
	if (const bool bIsClusterOperationMode = IDisplayCluster::Get().GetOperationMode() == EDisplayClusterOperationMode::Cluster)
	{
		return false;
	}

	// Get state from viewport world (UE-114493)
	if (InViewport)
	{
		if (UWorld* CurrentWorld = InViewport->GetOwner().GetCurrentWorld())
		{
			if (CurrentWorld)
			{
				switch (CurrentWorld->WorldType)
				{
				case EWorldType::Editor:
				case EWorldType::EditorPreview:
					return true;

				default:
					break;
				}
			}
		}
	}

	return false;
}

bool FDisplayClusterProjectionPolicyBase::IsConfigurationChanged(const FDisplayClusterConfigurationProjection* InConfigurationProjectionPolicy) const
{
	if (InConfigurationProjectionPolicy->Parameters.Num() != Parameters.Num()) {

		return true;
	}

	for (const TPair<FString, FString>& NewParamIt : InConfigurationProjectionPolicy->Parameters) {

		const FString* CurrentValue = Parameters.Find(NewParamIt.Key);
		if (CurrentValue==nullptr || CurrentValue->Compare(NewParamIt.Value, ESearchCase::IgnoreCase) != 0) {
			return true;
		}
	}

	// Parameters not changed
	return false;
}

void FDisplayClusterProjectionPolicyBase::InitializeOriginComponent(IDisplayClusterViewport* InViewport, const FString& OriginCompId)
{
	// Reset previous one
	PolicyOriginComponentRef.ResetSceneComponent();

	if (InViewport)
	{
		USceneComponent* PolicyOriginComp = nullptr;
		ADisplayClusterRootActor* RootActor = InViewport->GetOwner().GetRootActor();
		if (RootActor)
		{
			// default use root actor as Origin
			PolicyOriginComp = RootActor->GetRootComponent();

			// Try to get a node specified in the config file
			if (!OriginCompId.IsEmpty())
			{
				UE_LOG(LogDisplayClusterProjection, Verbose, TEXT("Looking for an origin component '%s'..."), *OriginCompId);
				PolicyOriginComp = RootActor->GetComponentByName<USceneComponent>(OriginCompId);

				if (PolicyOriginComp == nullptr)
				{
					if (!IsEditorOperationMode(InViewport))
					{
						UE_LOG(LogDisplayClusterProjection, Error, TEXT("No custom origin set or component '%s' not found for policy '%s'. VR root will be used."), *OriginCompId, *GetId());
					}

					PolicyOriginComp = RootActor->GetRootComponent();
				}
			}
		}

		if (!PolicyOriginComp)
		{
			if (!IsEditorOperationMode(InViewport))
			{
				UE_LOG(LogDisplayClusterProjection, Error, TEXT("Couldn't set origin component"));
			}
			return;
		}

		PolicyOriginComponentRef.SetSceneComponent(PolicyOriginComp);
	}
}

void FDisplayClusterProjectionPolicyBase::ReleaseOriginComponent()
{
	PolicyOriginComponentRef.ResetSceneComponent();
}
