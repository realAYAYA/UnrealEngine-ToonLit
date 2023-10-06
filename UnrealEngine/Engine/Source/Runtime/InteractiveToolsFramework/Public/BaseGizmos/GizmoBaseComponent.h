// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/PrimitiveComponent.h"
#include "GizmoBaseComponent.generated.h"

class UGizmoViewContext;

/**
 * Base class for simple Components intended to be used as part of 3D Gizmos.
 * Contains common properties and utility functions.
 * This class does nothing by itself, use subclasses like UGizmoCircleComponent
 */
UCLASS(ClassGroup = Utility, HideCategories = (Physics, Collision, Mobile), MinimalAPI)
class UGizmoBaseComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	UGizmoBaseComponent()
	{
		bUseEditorCompositing = false;
	}

	/**
	 * Currently this must be called if you change UProps on Base or subclass,
	 * to recreate render proxy which has a local copy of those settings
	 */
	void NotifyExternalPropertyUpdates()
	{
		MarkRenderStateDirty();
		UpdateBounds();
	}

public:
	UPROPERTY(EditAnywhere, Category = Options)
	FLinearColor Color = FLinearColor::Red;


	UPROPERTY(EditAnywhere, Category = Options)
	float HoverSizeMultiplier = 2.0f;


	UPROPERTY(EditAnywhere, Category = Options)
	float PixelHitDistanceThreshold = 7.0f;

	void SetGizmoViewContext(UGizmoViewContext* GizmoViewContextIn)
	{
		GizmoViewContext = GizmoViewContextIn;
		bIsViewDependent = (GizmoViewContext != nullptr);
	}

public:
	UFUNCTION()
	void UpdateHoverState(bool bHoveringIn)
	{
		if (bHoveringIn != bHovering)
		{
			bHovering = bHoveringIn;
		}
	}

	UFUNCTION()
	void UpdateWorldLocalState(bool bWorldIn)
	{
		if (bWorldIn != bWorld)
		{
			bWorld = bWorldIn;
		}
	}


protected:

	// hover state
	bool bHovering = false;

	// world/local coordinates state
	bool bWorld = false;

	UPROPERTY()
	TObjectPtr<UGizmoViewContext> GizmoViewContext = nullptr;

	// True when GizmoViewContext is not null. Here as a boolean so it
	// can be pointed to by the proxy to see what it should do.
	bool bIsViewDependent = false;
};
