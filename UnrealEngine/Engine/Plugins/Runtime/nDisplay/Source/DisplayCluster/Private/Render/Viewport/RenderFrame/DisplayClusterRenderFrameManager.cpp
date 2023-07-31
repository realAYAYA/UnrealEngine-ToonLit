// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrameManager.h"

#include "Render/Viewport/DisplayClusterViewport.h"

#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrame.h"
#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrameSettings.h"


///////////////////////////////////////////////////////////////
// FDisplayClusterRenderTargetFrame
///////////////////////////////////////////////////////////////
bool FDisplayClusterRenderFrameManager::BuildRenderFrame(FViewport* InViewport, const FDisplayClusterRenderFrameSettings& InRenderFrameSettings, const TArray<FDisplayClusterViewport*>& InViewports, FDisplayClusterRenderFrame& OutRenderFrame)
{

	switch (InRenderFrameSettings.RenderMode)
	{
	case EDisplayClusterRenderFrameMode::PreviewInScene:
		// Dont use render frame for preview
		break;
	default:
		if (!FindFrameTargetRect(InViewport, InViewports, InRenderFrameSettings, OutRenderFrame.FrameRect))
		{
			return false;
		}
		break;
	}

	// @todo add atlasing, merge multiple viewports in single viewfamily, etc
	// now prototype, just simple frame: use separate RTT for each viewport eye

	// Sort viewports, childs after parents
	//@todo save this order inside logic
	TArray<FDisplayClusterViewport*> SortedViewports;
	TArray<FDisplayClusterViewport*> ChildsViewports;

	// First add root viewports
	for (FDisplayClusterViewport* Viewport : InViewports)
	{
		if (Viewport->RenderSettings.GetParentViewportId().IsEmpty())
		{
			SortedViewports.Add(Viewport);
		}
		else
		{
			ChildsViewports.Add(Viewport);
		}
	}

	SortedViewports.Append(ChildsViewports);
	
	// At this point, we have the list of the viewports sorted in a way that
	// child (dependent) viewports follow after the parent ones. Here we do
	// some re-ordering so all the viewports that have a media capture
	// device assigned, will be first in the list. Child viewports can't
	// have media capture assigned, so there won't be any problems.
	{
		// Find all viewports being captured
		TArray<FDisplayClusterViewport*> ViewportsBeingCaptured = SortedViewports.FilterByPredicate([](const FDisplayClusterViewport* Viewport)
		{
			return Viewport->GetRenderSettings().bIsBeingCaptured;
		});

		// Put them all in the beginning of the list
		const int32 CapturingViewportsAmount = ViewportsBeingCaptured.Num();
		if (CapturingViewportsAmount > 0 && CapturingViewportsAmount != SortedViewports.Num())
		{
			for (FDisplayClusterViewport* ViewportBeingCaptured : ViewportsBeingCaptured)
			{
				SortedViewports.Remove(ViewportBeingCaptured);
				SortedViewports.Insert(ViewportBeingCaptured, 0);
			}
		}
	}

	bool bResult = false;

	if (InRenderFrameSettings.bAllowRenderTargetAtlasing)
	{
// Not implemented yet
#if 0
		switch (InRenderFrameSettings.ViewFamilyMode)
		{
		case EDisplayClusterRenderFamilyMode::AllowMergeForGroups:
			bResult = false;
			break;

		case EDisplayClusterRenderFamilyMode::AllowMergeForGroupsAndStereo:
			//NOT_IMPLEMENTED
			bResult = false;
			break;

		case EDisplayClusterRenderFamilyMode::MergeAnyPossible:
			//NOT_IMPLEMENTED
			bResult = false;
			break;

		default:
			bResult = false;
			break;
		}
#endif
	}
	else
	{
		bResult = BuildSimpleFrame(InViewport, InRenderFrameSettings, SortedViewports, OutRenderFrame);
	}

	return bResult;
}

bool FDisplayClusterRenderFrameManager::BuildSimpleFrame(FViewport* InViewport, const FDisplayClusterRenderFrameSettings& InRenderFrameSettings, const TArray<FDisplayClusterViewport*>& InViewports, FDisplayClusterRenderFrame& OutRenderFrame)
{
	for (FDisplayClusterViewport* ViewportIt : InViewports)
	{
		if (ViewportIt)
		{
			for (const FDisplayClusterViewport_Context& ContextIt : ViewportIt->GetContexts())
			{
				FDisplayClusterRenderFrame::FFrameView FrameView;
				{
					FrameView.ContextNum = ContextIt.ContextNum;
					FrameView.Viewport = ViewportIt;
					FrameView.bDisableRender = ContextIt.bDisableRender || ViewportIt->RenderSettings.bSkipSceneRenderingButLeaveResourcesAvailable;
					FrameView.bFreezeRendering = ViewportIt->RenderSettings.bFreezeRendering;
				}

				FDisplayClusterRenderFrame::FFrameViewFamily FrameViewFamily;
				{
					FrameViewFamily.Views.Add(FrameView);
					FrameViewFamily.CustomBufferRatio = ContextIt.CustomBufferRatio;
					FrameViewFamily.ViewExtensions = ViewportIt->GatherActiveExtensions(InViewport);
				}

				FDisplayClusterRenderFrame::FFrameRenderTarget FrameRenderTarget;
				{
					// Simple frame use unique RTT  for each viewport, so disable RTT when viewport rendering disabled
					FrameRenderTarget.bShouldUseRenderTarget = ViewportIt->RenderTargets.Num() > 0;

					FrameRenderTarget.ViewFamilies.Add(FrameViewFamily);

					FrameRenderTarget.RenderTargetSize = ContextIt.RenderTargetRect.Max;
					FrameRenderTarget.CaptureMode = ViewportIt->RenderSettings.CaptureMode;
				}

				OutRenderFrame.RenderTargets.Add(FrameRenderTarget);
			}
		}
	}

	return true;
}

bool FDisplayClusterRenderFrameManager::FindFrameTargetRect(FViewport* InViewport, const TArray<FDisplayClusterViewport*>& InOutViewports, const FDisplayClusterRenderFrameSettings& InRenderFrameSettings, FIntRect& OutFrameTargetRect) const
{
	// Calculate Backbuffer frame
	bool bIsUsed = false;

	if (InViewport && InRenderFrameSettings.bShouldUseFullSizeFrameTargetableResource)
	{
		// Use full-size frame RTT
		OutFrameTargetRect = FIntRect(FIntPoint(0, 0), InViewport->GetSizeXY());
		bIsUsed = true;
	}

	// Optimize frame target RTT
	for (const FDisplayClusterViewport* ViewportIt : InOutViewports)
	{
		if (ViewportIt && ViewportIt->RenderSettings.bEnable && ViewportIt->RenderSettings.bVisible)
		{
			for (const auto& ContextIt : ViewportIt->GetContexts())
			{
				if (!bIsUsed)
				{
					OutFrameTargetRect = ContextIt.FrameTargetRect;
					bIsUsed = true;
				}
				else
				{
					OutFrameTargetRect.Include(ContextIt.FrameTargetRect.Min);
					OutFrameTargetRect.Include(ContextIt.FrameTargetRect.Max);
				}
			}
		}
	}

	if (OutFrameTargetRect.Width() <= 0 || OutFrameTargetRect.Height() <= 0)
	{
		return false;
	}

	return bIsUsed;
}
