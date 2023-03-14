// Copyright Epic Games, Inc. All Rights Reserved.

#include "ComposureViewExtension.h"
#include "ComposurePipelineBaseActor.h"
#include "SceneView.h"

//------------------------------------------------------------------------------
FComposureViewExtension::FComposureViewExtension(const FAutoRegister& AutoRegister, AComposurePipelineBaseActor* Owner)
	: FSceneViewExtensionBase(AutoRegister)
	, AssociatedPipelineObj(Owner)
{}

//------------------------------------------------------------------------------
void FComposureViewExtension::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
	if (AssociatedPipelineObj.IsValid())
	{
		bool bCameraCutThisFrame = false;
		for (const FSceneView* SceneView : InViewFamily.Views)
		{
			if (SceneView && SceneView->bCameraCut)
			{
				bCameraCutThisFrame = true;
				break;
			}
		}

		AComposurePipelineBaseActor* Owner = AssociatedPipelineObj.Get();
		Owner->EnqueueRendering(bCameraCutThisFrame);
	}
}

//------------------------------------------------------------------------------
int32 FComposureViewExtension::GetPriority() const
{
	if (AssociatedPipelineObj.IsValid())
	{
		return AssociatedPipelineObj->GetRenderPriority();
	}
	return FSceneViewExtensionBase::GetPriority();
}

//------------------------------------------------------------------------------
bool FComposureViewExtension::IsActiveThisFrame_Internal(const FSceneViewExtensionContext&) const
{
	bool bActive = false;
	if (AssociatedPipelineObj.IsValid())
	{
		AComposurePipelineBaseActor* Owner = AssociatedPipelineObj.Get();
		bActive = Owner->IsActivelyRunning();
	}
	return bActive;
}

