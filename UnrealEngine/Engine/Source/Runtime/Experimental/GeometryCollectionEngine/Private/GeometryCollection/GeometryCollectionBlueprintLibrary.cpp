// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryCollection/GeometryCollectionBlueprintLibrary.h"

#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionExternalRenderInterface.h"

void UGeometryCollectionBlueprintLibrary::SetISMPoolCustomInstanceData(UGeometryCollectionComponent* GeometryCollectionComponent, int32 CustomFloatIndex, float CustomFloatValue)
{
	if (GeometryCollectionComponent == nullptr)
	{
		return;
	}
	IGeometryCollectionExternalRenderInterface* CustomRenderer = GeometryCollectionComponent->GetCustomRenderer();
	if (CustomRenderer == nullptr)
	{
		return;
	}
	CustomRenderer->SetCustomInstanceData(CustomFloatIndex, CustomFloatValue);
}
