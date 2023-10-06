// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandmassBPEditorExtension.h"

#include "LevelEditorViewport.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LandmassBPEditorExtension)

ULandmassBlueprintFunctionLibrary::ULandmassBlueprintFunctionLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

bool ULandmassBlueprintFunctionLibrary::GetCursorWorldRay(FVector& CameraLocation, FVector& RayOrigin, FVector& RayDirection)
{
	if (GCurrentLevelEditingViewportClient == nullptr)
	{
		return false;
	}

	FViewportCursorLocation CursorLocation = GCurrentLevelEditingViewportClient->GetCursorWorldLocationFromMousePos();

	CameraLocation = CursorLocation.GetOrigin();
	RayOrigin = CursorLocation.GetOrigin();
	RayDirection = CursorLocation.GetDirection();

	return true;
}
