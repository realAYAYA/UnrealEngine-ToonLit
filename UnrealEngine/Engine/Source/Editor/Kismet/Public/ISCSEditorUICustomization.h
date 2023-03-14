// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ChildActorComponent.h"
#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"

class UActorComponent;

/** SCSEditor UI customization */
class ISCSEditorUICustomization
{
public:
	virtual ~ISCSEditorUICustomization() {}

	/** @return Whether to hide the components tree */
	virtual bool HideComponentsTree() const { return false; }

	/** @return Whether to hide the components filter box */
	virtual bool HideComponentsFilterBox() const { return false; }

	/** @return Whether to hide the "Add Component" combo button */
	virtual bool HideAddComponentButton() const { return false; }

	/** @return Whether to hide the "Edit Blueprint" and "Blueprint/Add Script" buttons */
	virtual bool HideBlueprintButtons() const { return false; }

	/** 
	 * @return An override for the default ChildActorComponentTreeViewVisualizationMode from the project settings, if different from UseDefault.
	 * @note Setting an override also forces child actor tree view expansion to be enabled.
	 */
	virtual EChildActorComponentTreeViewVisualizationMode GetChildActorVisualizationMode() const { return EChildActorComponentTreeViewVisualizationMode::UseDefault; }

	/** @return A component type that limits visible nodes when filtering the tree view */
	virtual TSubclassOf<UActorComponent> GetComponentTypeFilter() const { return nullptr; }
};
