// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ComponentVisualizer.h"

class USmartObjectComponent;
class USmartObjectDefinition;
class USmartObjectAssetEditorTool;

/**
 * Hit proxy for Smart Object slots.
 */
struct SMARTOBJECTSEDITORMODULE_API HSmartObjectSlotProxy : public HComponentVisProxy
{
	DECLARE_HIT_PROXY();

	HSmartObjectSlotProxy(const UActorComponent* InComponent, const FGuid InSlotID, const int32 InAnnotationIndex = INDEX_NONE)
		: HComponentVisProxy(InComponent, HPP_Foreground)
		, SlotID(InSlotID)
		, AnnotationIndex(InAnnotationIndex)
	{}

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::CardinalCross;
	}

	FGuid SlotID;
	int32 AnnotationIndex = INDEX_NONE;
};

/**
 * Helper functions to draw Smart Object definition visualization.
 */
namespace UE::SmartObjects::Editor
{
	// @todo: move this to more suitable header.
	struct FSelectedItem
	{
		FSelectedItem() = default;

		FSelectedItem(const FGuid InSlotID, const int32 InAnnotationIndex = INDEX_NONE)
			: SlotID(InSlotID)
			, AnnotationIndex(InAnnotationIndex)
		{
		}

		bool operator==(const FSelectedItem& Other) const
		{
			return SlotID == Other.SlotID && AnnotationIndex == Other.AnnotationIndex;
		}
			
		FGuid SlotID;
		int32 AnnotationIndex = INDEX_NONE; // @todo: consider guid for this too.
	};

	void Draw(const USmartObjectDefinition& Definition, TConstArrayView<FSelectedItem> Selection, const FTransform& OwnerLocalToWorld, const FSceneView& View, FPrimitiveDrawInterface& PDI);
	void DrawCanvas(const USmartObjectDefinition& Definition, TConstArrayView<FSelectedItem> Selection, const FTransform& OwnerLocalToWorld, const FSceneView& View, FCanvas& Canvas);
}; // UE::SmartObjects::Editor


/**
 * Visualizer for SmartObjectComponent
 */
class SMARTOBJECTSEDITORMODULE_API FSmartObjectComponentVisualizer : public FComponentVisualizer
{
protected:
	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	virtual void DrawVisualizationHUD(const UActorComponent* Component, const FViewport* Viewport, const FSceneView* View, FCanvas* Canvas) override;
};
