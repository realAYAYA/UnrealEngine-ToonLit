// Copyright Epic Games, Inc. All Rights Reserved.

#include "VREditorWidgetComponent.h"
#include "IVREditorModule.h"
#include "VREditorAssetContainer.h"
#include "VREditorMode.h"

UVREditorWidgetComponent::UVREditorWidgetComponent()
	: Super(),
	DrawingPolicy(EVREditorWidgetDrawingPolicy::Always),
	bIsHovering(false),
	bHasEverDrawn(false)
{
	bSelectable = false;
	SetCastShadow(false);

	if (UNLIKELY(IsRunningDedicatedServer()) || HasAnyFlags(RF_ClassDefaultObject))   // @todo vreditor: early out to avoid loading font assets in the cooker on Linux
	{
		return;
	}

	if (!IsRunningCommandlet())
	{
		if (IVREditorModule::IsAvailable())
		{
			IVREditorModule& VRModule = IVREditorModule::Get();

			if (UVREditorMode* VRMode = VRModule.GetVRMode())
			{
				const UVREditorAssetContainer& VRAssetContainer = VRMode->GetAssetContainer();
				{
					TranslucentMaterial = VRAssetContainer.WidgetMaterial;
					TranslucentMaterial_OneSided = VRAssetContainer.WidgetMaterial;
				}
			}
		}

	}
}


bool UVREditorWidgetComponent::ShouldDrawWidget() const
{
	if ( DrawingPolicy == EVREditorWidgetDrawingPolicy::Always ||
		(DrawingPolicy == EVREditorWidgetDrawingPolicy::Hovering && bIsHovering) ||
		!bHasEverDrawn )
	{
		return Super::ShouldDrawWidget();
	}

	return false;
}

void UVREditorWidgetComponent::DrawWidgetToRenderTarget(float DeltaTime)
{
	bHasEverDrawn = true;

	Super::DrawWidgetToRenderTarget(DeltaTime);
}
