// Copyright Epic Games, Inc. All Rights Reserved.
 
#include "BaseGizmos/GizmoElementRenderState.h"
#include "Materials/MaterialInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GizmoElementRenderState)

FLinearColor FGizmoElementColorAttribute::GetColor() const
{
	if (bHasValue)
	{
		return Value;
	}
	return DefaultColor;
}

void FGizmoElementColorAttribute::SetColor(FLinearColor InColor, bool InOverridesChildState)
{
	Value = InColor;
	bHasValue = true;
	bOverridesChildState = InOverridesChildState;
}

void FGizmoElementColorAttribute::Reset()
{
	Value = DefaultColor;
	bHasValue = false;
	bOverridesChildState = false;
}

void  FGizmoElementColorAttribute::UpdateState(const FGizmoElementColorAttribute& InChildColorAttribute)
{
	if (InChildColorAttribute.bHasValue && !(bOverridesChildState && bHasValue))
	{
		Value = InChildColorAttribute.Value;
		bHasValue = true;
		bOverridesChildState = InChildColorAttribute.bOverridesChildState;
	}
}

const UMaterialInterface* FGizmoElementMaterialAttribute::GetMaterial() const
{
	if (Value.IsValid())
	{
		return Value.Get();
	}
	return nullptr;
}

void FGizmoElementMaterialAttribute::SetMaterial(TWeakObjectPtr<UMaterialInterface> InMaterial, bool InOverridesChildState)
{
	Value = InMaterial;
	bOverridesChildState = InOverridesChildState;
}

void FGizmoElementMaterialAttribute::Reset()
{
	Value = nullptr;
	bOverridesChildState = false;
}

void FGizmoElementMaterialAttribute::UpdateState(const FGizmoElementMaterialAttribute& InChildMaterialAttribute)
{
	if (InChildMaterialAttribute.Value != nullptr && !(bOverridesChildState && Value != nullptr))
	{
		Value = InChildMaterialAttribute.Value;
		bOverridesChildState = InChildMaterialAttribute.bOverridesChildState;
	}
}

const UMaterialInterface* FGizmoElementMeshRenderStateAttributes::GetMaterial(EGizmoElementInteractionState InteractionState)
{
	if (InteractionState == EGizmoElementInteractionState::Hovering)
	{
		return HoverMaterial.GetMaterial();
	}
	else if (InteractionState == EGizmoElementInteractionState::Interacting)
	{
		return InteractMaterial.GetMaterial();
	}

	return Material.GetMaterial();
}

FLinearColor FGizmoElementMeshRenderStateAttributes::GetVertexColor(EGizmoElementInteractionState InteractionState)
{
	if (InteractionState == EGizmoElementInteractionState::Hovering)
	{
		return HoverVertexColor.GetColor();
	}
	else if (InteractionState == EGizmoElementInteractionState::Interacting)
	{
		return InteractVertexColor.GetColor();
	}
	return VertexColor.GetColor();
}

void FGizmoElementMeshRenderStateAttributes::Update(FGizmoElementMeshRenderStateAttributes& InChildAttributes)
{
	Material.UpdateState(InChildAttributes.Material);
	HoverMaterial.UpdateState(InChildAttributes.HoverMaterial);
	InteractMaterial.UpdateState(InChildAttributes.InteractMaterial);
	VertexColor.UpdateState(InChildAttributes.VertexColor);
	HoverVertexColor.UpdateState(InChildAttributes.HoverVertexColor);
	InteractVertexColor.UpdateState(InChildAttributes.InteractVertexColor);
}

FLinearColor FGizmoElementLineRenderStateAttributes::GetLineColor(EGizmoElementInteractionState InteractionState)
{
	if (InteractionState == EGizmoElementInteractionState::Hovering)
	{
		return HoverLineColor.GetColor();
	}
	else if (InteractionState == EGizmoElementInteractionState::Interacting)
	{
		return InteractLineColor.GetColor();
	}

	return LineColor.GetColor();
}

void FGizmoElementLineRenderStateAttributes::Update(FGizmoElementLineRenderStateAttributes& InChildAttributes)
{
	LineColor.UpdateState(InChildAttributes.LineColor);
	HoverLineColor.UpdateState(InChildAttributes.HoverLineColor);
	InteractLineColor.UpdateState(InChildAttributes.InteractLineColor);
}
