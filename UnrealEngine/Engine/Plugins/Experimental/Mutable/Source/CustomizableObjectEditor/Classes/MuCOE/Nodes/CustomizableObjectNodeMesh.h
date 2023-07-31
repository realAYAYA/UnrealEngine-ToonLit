// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "CoreTypes.h"
#include "Math/Vector2D.h"
#include "Misc/AssertionMacros.h"
#include "MuCOE/Nodes/CustomizableObjectNode.h"
#include "UObject/UObjectGlobals.h"

#include "CustomizableObjectNodeMesh.generated.h"

class UCustomizableObjectLayout;
class UEdGraphPin;
class UObject;
class UTexture2D;


UCLASS(abstract)
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeMesh : public UCustomizableObjectNode
{
public:
	GENERATED_BODY()

	// Own interface
	/** Returns the multiple Layouts of a given Mesh pin. A pin can have multiple Layouts since it can have multiple UVs. Override. */
	virtual TArray<UCustomizableObjectLayout*> GetLayouts(const UEdGraphPin* OutPin) const PURE_VIRTUAL(UCustomizableObjectNodeMesh::GetLayouts, return {};);

	virtual UTexture2D* FindTextureForPin(const UEdGraphPin* Pin) const PURE_VIRTUAL(UCustomizableObjectNodeMesh::FindTextureForPin, return {};);

	virtual void GetUVChannelForPin(const UEdGraphPin* Pin, TArray<FVector2f>& OutSegments, int32 UVIndex) const PURE_VIRTUAL(UCustomizableObjectNodeMesh::GetUVChannelForPin, );

	/** Returns the Unreal mesh (e.g., USkeletalMesh, UStaticMesh...). */
	virtual UObject* GetMesh() const PURE_VIRTUAL(UCustomizableObjectNodeMesh::GetMesh, return {};);;

	/** Returns the out mesh pin associated to the given LOD and Material. Override. */
	virtual UEdGraphPin* GetMeshPin(int32 LOD, int MaterialIndex) const PURE_VIRTUAL(UCustomizableObjectNodeMesh::GetMeshPin, return {};);

	/** Given a pin, return the Section and Layout index.
	 *
	 * Will always return a valid result. The result can be valid but out of sync with respect the Unreal mesh asset.
	 * In other words, the pin still represents a LOD 3 but the asset no longer has this third LOD.
	 * 
	 * @param Pin In. Node's owning pin.
	 * @param LODIndex Out.
	 * @param SectionIndex Out.
	 * @param LayoutIndex Out. */
	virtual void GetPinSection(const UEdGraphPin& Pin, int32& OutLODIndex, int32& OutSectionIndex, int32& OutLayoutIndex) const PURE_VIRTUAL(UCustomizableObjectNodeMesh::GetPinSection, );
};

