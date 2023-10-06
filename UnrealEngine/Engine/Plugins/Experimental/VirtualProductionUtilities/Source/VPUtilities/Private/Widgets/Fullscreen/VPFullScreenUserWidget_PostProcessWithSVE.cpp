// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/Fullscreen/VPFullScreenUserWidget_PostProcessWithSVE.h"

#include "Widgets/Fullscreen/PostProcessSceneViewExtension.h"
#include "Widgets/VPFullScreenUserWidget.h"

#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "SceneViewExtension.h"

bool FVPFullScreenUserWidget_PostProcessWithSVE::Display(UWorld* World, UUserWidget* Widget, TAttribute<float> InDPIScale)
{
	bool bOk = CreateRenderer(World, Widget, MoveTemp(InDPIScale));
	if (bOk && ensureMsgf(WidgetRenderTarget, TEXT("CreateRenderer returned true even though it failed.")))
	{
		SceneViewExtension = FSceneViewExtensions::NewExtension<UE::VirtualProductionUtilities::Private::FPostProcessSceneViewExtension>(
			*WidgetRenderTarget
			);
		SceneViewExtension->IsActiveThisFrameFunctions = MoveTemp(IsActiveFunctorsToRegister);
	}
	return bOk;
}

void FVPFullScreenUserWidget_PostProcessWithSVE::Hide(UWorld* World)
{
	SceneViewExtension.Reset();
	FVPFullScreenUserWidget_PostProcessBase::Hide(World);
}

void FVPFullScreenUserWidget_PostProcessWithSVE::Tick(UWorld* World, float DeltaSeconds)
{
	TickRenderer(World, DeltaSeconds);
}

void FVPFullScreenUserWidget_PostProcessWithSVE::RegisterIsActiveFunctor(FSceneViewExtensionIsActiveFunctor IsActiveFunctor)
{
	if (SceneViewExtension)
	{
		SceneViewExtension->IsActiveThisFrameFunctions.Emplace(MoveTemp(IsActiveFunctor));
	}
	else
	{
		IsActiveFunctorsToRegister.Emplace(MoveTemp(IsActiveFunctor));
	}
}
