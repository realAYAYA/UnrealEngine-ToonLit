// Copyright Epic Games, Inc. All Rights Reserved.

#include "CorrectivesEditMode.h"

#include "ControlRig.h"
#include "ControlRigGizmoActor.h"
#include "EditorViewportClient.h"
#include "EngineUtils.h"
#include "Rigs/RigHierarchyController.h"


FName FCorrectivesEditMode::ModeName("EditMode.CorrectivesControlRig");

/* Workaround for edit mode not clearing selection due to not being in level editor mode or using blueprint hierachy controller*/

bool FCorrectivesEditMode::HandleClick(FEditorViewportClient* InViewportClient, HHitProxy *HitProxy, const FViewportClick &Click)
{
	if(HActor* ActorHitProxy = HitProxyCast<HActor>(HitProxy))
	{
		if(ActorHitProxy->Actor && ActorHitProxy->Actor->IsA<AControlRigShapeActor>())
		{
			if (!Click.IsShiftDown() && !Click.IsControlDown())
			{
				ClearSelection();
			}
		}
	}

	return FControlRigEditMode::HandleClick(InViewportClient, HitProxy, Click);	
}

void FCorrectivesEditMode::ClearSelection()
{
	for(TWeakObjectPtr<UControlRig> ControlRig : GetControlRigs())
	{
		if (URigHierarchyController* Controller = ControlRig.Get()->GetHierarchy()->GetController())
		{
			Controller->ClearSelection();
		}
	}				
}
