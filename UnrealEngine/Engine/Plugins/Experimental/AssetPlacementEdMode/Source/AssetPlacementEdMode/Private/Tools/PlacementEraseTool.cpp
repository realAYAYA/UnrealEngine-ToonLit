// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/PlacementEraseTool.h"
#include "UObject/Object.h"
#include "ToolContextInterfaces.h"
#include "InteractiveToolManager.h"
#include "Elements/Framework/EngineElementsLibrary.h"
#include "Elements/Framework/TypedElementCommonActions.h"
#include "Elements/Framework/TypedElementHandle.h"
#include "Elements/Framework/TypedElementRegistry.h"
#include "Elements/Interfaces/TypedElementObjectInterface.h"
#include "Tools/AssetEditorContextInterface.h"
#include "Editor.h"
#include "AssetPlacementEdMode.h"
#include "AssetPlacementSettings.h"
#include "Modes/PlacementModeSubsystem.h"
#include "Toolkits/IToolkitHost.h"
#include "UObject/GCObjectScopeGuard.h"
#include "EditorModeManager.h"
#include "ContextObjectStore.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PlacementEraseTool)

constexpr TCHAR UPlacementModeEraseTool::ToolName[];

UPlacementBrushToolBase* UPlacementModeEraseToolBuilder::FactoryToolInstance(UObject* Outer) const
{
	return NewObject<UPlacementModeEraseTool>(Outer);
}

void UPlacementModeEraseTool::OnBeginDrag(const FRay& Ray)
{
	Super::OnBeginDrag(Ray);

	GetToolManager()->BeginUndoTransaction(NSLOCTEXT("AssetPlacementEdMode", "BrushErase", "Erase Painted Elements"));
}

void UPlacementModeEraseTool::OnEndDrag(const FRay& Ray)
{
	GetToolManager()->EndUndoTransaction();

	Super::OnEndDrag(Ray);
}

void UPlacementModeEraseTool::OnTick(float DeltaTime)
{
	if (!bInBrushStroke)
	{
		return;
	}

	IAssetEditorContextInterface* AssetEditorContext = GetToolManager()->GetContextObjectStore()->FindContext<IAssetEditorContextInterface>();
	if (!AssetEditorContext)
	{
		return;
	}

	UTypedElementCommonActions* ElementCommonActions = AssetEditorContext->GetCommonActions();
	if (!ElementCommonActions)
	{
		return;
	}

	UTypedElementSelectionSet* SelectionSet = AssetEditorContext->GetMutableSelectionSet();
	if (!SelectionSet)
	{
		return;
	}

	FTypedElementListRef ElementsToDelete = GetElementsInBrushRadius(LastDeviceInputRay);
	if (ElementsToDelete->HasElements())
	{
		FTypedElementListRef NormalizedElementsToDelete = SelectionSet->GetNormalizedElementList(ElementsToDelete, FTypedElementSelectionNormalizationOptions());
		ElementCommonActions->DeleteNormalizedElements(NormalizedElementsToDelete, AssetEditorContext->GetEditingWorld(), SelectionSet, FTypedElementDeletionOptions());
	}
}

