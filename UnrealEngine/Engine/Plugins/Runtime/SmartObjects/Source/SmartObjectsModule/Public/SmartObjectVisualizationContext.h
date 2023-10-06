// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EngineDefines.h"
#include "Math/Transform.h"
#include "Templates/SubclassOf.h"

class FCanvas;
class FSceneView;
class FPrimitiveDrawInterface;
class USmartObjectDefinition;
class UFont;
class USmartObjectSubsystem;
class AActor;
class USmartObjectSlotValidationFilter;

#if WITH_EDITOR

/**
 * Helper class used for Smart Object Annotation rendering.
 */
struct SMARTOBJECTSMODULE_API FSmartObjectVisualizationContext
{
	explicit FSmartObjectVisualizationContext(const USmartObjectDefinition& InDefinition, const UWorld& InWorld);

	/** @return true of the context is property set up for 2D drawing. */
	bool IsValidForDraw() const;

	/** @return true of the context is property set up for HUD (2D) drawing. */
	bool IsValidForDrawHUD() const;

	/** Draws string on canvas at specified canvas location. */
	void DrawString(const float StartX, const float StartY, const TCHAR* Text, const FLinearColor& Color, const FLinearColor& ShadowColor = FLinearColor::Black) const;
	
	/** Draws string on canvas centered at specified world location. */
	void DrawString(const FVector& Location, const TCHAR* Text, const FLinearColor& Color, const FLinearColor& ShadowColor = FLinearColor::Black) const;

	/**
	 * Draws arrow from Start to End.
	 * @param Start arrow start location
	 * @param End arrow end location
	 * @param Color color of the arrow
	 * @param ArrowHeadLength length of the arrow head
	 * @param EndLocationInset how much the ends of the line are inset
	 */
	void DrawArrow(const FVector& Start, const FVector& End, const FLinearColor& Color, const float ArrowHeadLength = 15.0f, const float EndLocationInset = 0.0f, const uint8 DepthPriorityGroup = 0, const float Thickness = 1.0f, const float DepthBias = 0.0f, bool bScreenSpace = false) const;
	
	/** Projects world location into canvas screen space. */
	FVector2D Project(const FVector& Location) const;

	/** @return true of the location is in view frustum. */
	bool IsLocationVisible(const FVector& Location) const;

	/** @return distance from location in world space, to the camera. */
	FVector::FReal GetDistanceToCamera(const FVector& Location) const;
	
	/** Pointer to the visualized Smart Object definition. */
	const USmartObjectDefinition& Definition;

	/** World associated with the drawing. */
	const UWorld& World;

	/** Actor used for previewing the Smart Object. */
	const AActor* PreviewActor = nullptr;

	/** Validation filter class used for previewing. */
	TSubclassOf<USmartObjectSlotValidationFilter> PreviewValidationFilterClass;
	
	/** Index of the visualized slot, or invalid of the annotation is on the object.  */
	int32 SlotIndex = INDEX_NONE;

	/** Index of the visualized annotation. */
	int32 AnnotationIndex = INDEX_NONE;

	/** Transform of the owner object. */
	FTransform OwnerLocalToWorld;

	/** True, if the slot is currently selected */
	bool bIsSlotSelected = false;

	/** True, if the annotation is currently selected */
	bool bIsAnnotationSelected = false;
	
	/** Current view to draw to. */
	const FSceneView* View = nullptr;

	/** Primitive draw interface to draw with during DrawVisualization(). */
	FPrimitiveDrawInterface* PDI = nullptr;

	/** Canvas to render to during DrawVisualizationHUD(). */
	FCanvas* Canvas = nullptr;

	/** Pointer to valid engine small font. */
	const UFont* Font = nullptr;

	/** Color for a selection, cached from UEditorStyleSettings. */
	FLinearColor SelectedColor = FLinearColor::White;
};

#endif // WITH_EDITOR
