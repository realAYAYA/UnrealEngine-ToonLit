// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "BaseGizmos/GizmoElementShared.h"
#include "Materials/MaterialInterface.h"
#include "GizmoElementRenderState.generated.h"


//
// Structs used to store gizmo element render state.
//

//
// Color state attribute struct 
//
// Stores color state value, whether state is set and whether it overrides child state. 
//
USTRUCT()
struct INTERACTIVETOOLSFRAMEWORK_API FGizmoElementColorAttribute
{
	GENERATED_BODY()

	static constexpr FLinearColor DefaultColor = FLinearColor(1.0f, 1.0f, 1.0f);

	// Color value
	UPROPERTY()
	FLinearColor Value = DefaultColor;

	// True if value has been set.
	UPROPERTY()
	bool bHasValue = false;

	// True if this attribute overrides child state attributes
	UPROPERTY()
	bool bOverridesChildState = false;

	// Return the color value. This returns the default color value if no value has been set.
	FLinearColor GetColor() const;

	// Set color value and override flag
	void SetColor(FLinearColor InColor, bool InOverridesChildState = false);

	// Reset attribute to default values.
	void Reset();

	// Update this state based on a child state attribute.
	void UpdateState(const FGizmoElementColorAttribute& InChildColorAttribute);
};

//
// Material state attribute struct
//
// Stores material state value, whether state is set and whether it overrides child state.
//
USTRUCT()
struct INTERACTIVETOOLSFRAMEWORK_API FGizmoElementMaterialAttribute 
{
	GENERATED_BODY()

	// Material value
	UPROPERTY()
	TWeakObjectPtr<UMaterialInterface> Value = nullptr;

	// True if this attribute overrides child state attributes 
	UPROPERTY()
	bool bOverridesChildState = false;

	// Return the material value.
	const UMaterialInterface* GetMaterial() const;

	// Set material value and override flag
	void SetMaterial(TWeakObjectPtr<UMaterialInterface> InColor, bool InOverridesChildState = false);

	// Reset attribute to default values.
	void Reset();

	// Update this state based on a child state attribute.
	void UpdateState(const FGizmoElementMaterialAttribute& InChildMaterialAttribute);
};

//
// Mesh render state structure.
//
USTRUCT()
struct INTERACTIVETOOLSFRAMEWORK_API FGizmoElementMeshRenderStateAttributes
{
	GENERATED_BODY()

	// Default material
	UPROPERTY()
	FGizmoElementMaterialAttribute Material;

	// Hover material
	UPROPERTY()
	FGizmoElementMaterialAttribute HoverMaterial;

	// Interact material
	UPROPERTY()
	FGizmoElementMaterialAttribute InteractMaterial;

	// Mesh vertex color
	UPROPERTY()
	FGizmoElementColorAttribute VertexColor;

	// Hover mesh vertex color
	UPROPERTY()
	FGizmoElementColorAttribute HoverVertexColor;

	// Interact mesh vertex color
	UPROPERTY()
	FGizmoElementColorAttribute InteractVertexColor;

	// Returns the material corresponding to the input interaction state
	const UMaterialInterface* GetMaterial(EGizmoElementInteractionState InteractionState);

	// Returns mesh vertex color
	FLinearColor GetVertexColor(EGizmoElementInteractionState InteractionState);

	// Update this mesh render state based on a child mesh render state attribute.
	void Update(FGizmoElementMeshRenderStateAttributes& InChildAttributes);
};

//
// Line render state structure.
//
USTRUCT()
struct INTERACTIVETOOLSFRAMEWORK_API FGizmoElementLineRenderStateAttributes
{
	GENERATED_BODY()

	// Line color
	UPROPERTY()
	FGizmoElementColorAttribute LineColor;

	// Hover line color
	UPROPERTY()
	FGizmoElementColorAttribute HoverLineColor;

	// Interact line color
	UPROPERTY()
	FGizmoElementColorAttribute InteractLineColor;

	// Returns the line color corresponding to the input interaction state
	FLinearColor GetLineColor(EGizmoElementInteractionState InteractionState);

	// Update this line render state based on a child mesh render state attribute.
	void Update(FGizmoElementLineRenderStateAttributes& InChildAttributes);
};

