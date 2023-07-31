// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessPixelProjectedReflectionMobile.h
=============================================================================*/

#pragma once
#include "CoreMinimal.h"
#include "RendererInterface.h"

FRDGTextureRef CreateMobilePixelProjectedReflectionTexture(FRDGBuilder& GraphBuilder, FIntPoint Extent);