// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/PlacementLassoSelectTool.h"

#include "AssetPlacementEdMode.h"
#include "AssetPlacementSettings.h"
#include "Editor.h"
#include "Elements/Framework/EngineElementsLibrary.h"
#include "Elements/Framework/TypedElementHandle.h"
#include "Elements/Framework/TypedElementRegistry.h"
#include "Elements/Interfaces/TypedElementObjectInterface.h"
#include "InteractiveToolManager.h"
#include "ToolContextInterfaces.h"
#include "UObject/Object.h"
#include "BaseBehaviors/KeyAsModifierInputBehavior.h"
#include "Elements/Framework/TypedElementSelectionSet.h"
#include "Modes/PlacementModeSubsystem.h"
#include "Tools/AssetEditorContextInterface.h"
#include "EditorModeManager.h"
#include "ContextObjectStore.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PlacementLassoSelectTool)

constexpr TCHAR UPlacementModeLassoSelectTool::ToolName[];

namespace PlacementModeLassoToolInternal
{
	FTypedElementSelectionOptions SelectionOptions {};
}

UPlacementBrushToolBase* UPlacementModeLassoSelectToolBuilder::FactoryToolInstance(UObject* Outer) const
{
	return NewObject<UPlacementModeLassoSelectTool>(Outer);
}

void UPlacementModeLassoSelectTool::OnBeginDrag(const FRay& Ray)
{
	Super::OnBeginDrag(Ray);
	GetToolManager()->BeginUndoTransaction(NSLOCTEXT("AssetPlacementEdMode", "BrushSelect", "Select Elements"));
}

void UPlacementModeLassoSelectTool::OnEndDrag(const FRay& Ray)
{
	GetToolManager()->EndUndoTransaction();
	Super::OnEndDrag(Ray);
}

void UPlacementModeLassoSelectTool::OnTick(float DeltaTime)
{
	if (!bInBrushStroke)
	{
		return;
	}

	if (IAssetEditorContextInterface* AssetEditorContext = GetToolManager()->GetContextObjectStore()->FindContext<IAssetEditorContextInterface>())
	{
		UTypedElementSelectionSet* SelectionSet = AssetEditorContext->GetMutableSelectionSet();
		if (SelectionSet)
		{
			FTypedElementListRef HitElements = GetElementsInBrushRadius(LastDeviceInputRay);
			const bool bSelectElements = !bCtrlToggle;
			if (bSelectElements)
			{
				SelectionSet->SelectElements(HitElements, PlacementModeLassoToolInternal::SelectionOptions);
			}
			else
			{
				SelectionSet->DeselectElements(HitElements, PlacementModeLassoToolInternal::SelectionOptions);
			}
		}
	}
}

