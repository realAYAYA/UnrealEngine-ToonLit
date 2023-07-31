// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BlueprintEditor.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "Engine/Blueprint.h"
#include "GraphEditorDragDropAction.h"
#include "Input/DragAndDrop.h"
#include "Input/Reply.h"
#include "Templates/SharedPointer.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

class FText;
class UBlueprint;
class UEdGraph;
struct FEdGraphSchemaAction;

/** DragDropAction class for drag and dropping an item from the My Blueprints tree (e.g., variable or function) */
class KISMET_API FMyBlueprintItemDragDropAction : public FGraphSchemaActionDragDropAction
{
public:
	DRAG_DROP_OPERATOR_TYPE(FMyBlueprintItemDragDropAction, FGraphSchemaActionDragDropAction)

	// FGraphEditorDragDropAction interface
	virtual FReply DroppedOnAction(TSharedRef<FEdGraphSchemaAction> Action) override;
	virtual FReply DroppedOnCategory(FText Category) override;
	virtual void HoverTargetChanged() override;
	// End of FGraphEditorDragDropAction

	/** Set if operation is modified by alt */
	void SetAltDrag(bool InIsAltDrag) {	bAltDrag = InIsAltDrag; }

	/** Set if operation is modified by the ctrl key */
	void SetCtrlDrag(bool InIsCtrlDrag) { bControlDrag = InIsCtrlDrag; }

protected:
	 /** Constructor */
	FMyBlueprintItemDragDropAction();

	virtual UBlueprint* GetSourceBlueprint() const
	{
		return nullptr;
	}

	/** Helper method to see if we're dragging in the same blueprint */
	bool IsFromBlueprint(UBlueprint* InBlueprint) const
	{
		return GetSourceBlueprint() == InBlueprint;
	}

	void SetFeedbackMessageError(const FText& Message);
	void SetFeedbackMessageOK(const FText& Message);

protected:
	/** Was ctrl held down at start of drag */
	bool bControlDrag;
	/** Was alt held down at the start of drag */
	bool bAltDrag;
	/** Analytic delegate to track node creation */
	FNodeCreationAnalytic AnalyticCallback;
};
