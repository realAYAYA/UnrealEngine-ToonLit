// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "HAL/Platform.h"
#include "MuT/NodeModifierMeshClipWithMesh.h"
#include "MuT/NodeSurfaceNew.h"

class FString;
class UCustomizableObjectNodeMaterial;
class UCustomizableObjectNodeMaterialBase;
class UCustomizableObjectNodeObjectGroup;
class UEdGraphPin;
class UTexture2D;
struct FMutableGraphGenerationContext;

mu::NodeImagePtr GenerateMutableGroupProjection(const int32 NodeLOD, const int32 ImageIndex, mu::NodeMeshPtr MeshNode, FMutableGraphGenerationContext& GenerationContext,
	const UCustomizableObjectNodeMaterialBase* NodeMaterialBase, bool& bShareProjectionTexturesBetweenLODs, bool& bIsGroupProjectorImage,
	UTexture2D*& GroupProjectionReferenceTexture, TMap<FString, float>& TextureNameToProjectionResFactor, FString& AlternateResStateName,
	UCustomizableObjectNodeMaterial* ParentMaterial);


/** Convert a CustomizableObject Source Graph into a mutable source graph. */
bool GenerateMutableSourceGroupProjector(const UEdGraphPin* Pin, FMutableGraphGenerationContext& GenerationContext, const UCustomizableObjectNodeObjectGroup* originalGroup);
