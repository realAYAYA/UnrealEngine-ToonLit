// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AdvancedPreviewScene.h"

class UStaticMeshComponent;

class FCustomizableObjectPreviewScene : public FAdvancedPreviewScene
{
public:
	/** Parameter constructor */
	FCustomizableObjectPreviewScene(ConstructionValues CVS, float InFloorOffset = 0.0f);

	/** Destructor */
	virtual ~FCustomizableObjectPreviewScene();

	/** Getter of SkyComponent */
	UStaticMeshComponent* GetSkyComponent();
};