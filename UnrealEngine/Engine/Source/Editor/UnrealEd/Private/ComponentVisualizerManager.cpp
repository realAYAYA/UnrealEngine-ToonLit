// Copyright Epic Games, Inc. All Rights Reserved.

#include "ComponentVisualizerManager.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/MenuStack.h"
#include "Framework/Application/SlateApplication.h"
#include "Editor/UnrealEdEngine.h"
#include "UnrealEdGlobals.h"
#include "SEditorViewport.h"
#include "HAL/IConsoleManager.h"
#include "EditorModeManager.h"
#include "Elements/Framework/TypedElementSelectionSet.h"

namespace ComponentVisualizers
{
	static bool GAutoSelectComponentWithPointSelection = true;
	static FAutoConsoleVariableRef CVarAutoSelectComponentWithPointSelection(
		TEXT("Editor.ComponentVisualizer.AutoSelectComponent"),
		GAutoSelectComponentWithPointSelection,
		TEXT("Automatically adds the spline component to the selection set if avaialable when a point is selected on the spline")
	);

}; // namespace ComponentVisualizers


FComponentVisualizerManager::FComponentVisualizerManager()
	: EditedVisualizerViewportClient(nullptr)
{
}

/** Handle a click on the specified editor viewport client */
bool FComponentVisualizerManager::HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click)
{
	TSharedPtr<FComponentVisualizer> EditedVisualizer = EditedVisualizerPtr.Pin();

	// Give current visualizer a chance to handle click if it has a modifier.
	if (EditedVisualizer.IsValid() && (Click.IsControlDown() || Click.IsAltDown() || Click.IsShiftDown()))
	{
		if (EditedVisualizer->HandleModifiedClick(InViewportClient, HitProxy, Click))
		{ 
			return true;
		}
	}
		
	bool bHandled = HandleProxyForComponentVis(InViewportClient, HitProxy, Click);
	if (bHandled && Click.GetKey() == EKeys::RightMouseButton)
	{
		TSharedPtr<SWidget> MenuWidget = GenerateContextMenuForComponentVis();
		if (MenuWidget.IsValid())
		{
			TSharedPtr<SEditorViewport> ViewportWidget = InViewportClient->GetEditorViewportWidget();
			if (ViewportWidget.IsValid())
			{
				FSlateApplication::Get().PushMenu(
					ViewportWidget.ToSharedRef(),
					FWidgetPath(),
					MenuWidget.ToSharedRef(),
					FSlateApplication::Get().GetCursorPos(),
					FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));

				return true;
			}
		}
	}

	return bHandled;
}

bool FComponentVisualizerManager::HandleProxyForComponentVis(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click)
{
	if (HitProxy && HitProxy->IsA(HComponentVisProxy::StaticGetType()))
	{
		HComponentVisProxy* VisProxy = (HComponentVisProxy*)HitProxy;
		const UActorComponent* ClickedComponent = VisProxy->Component.Get();
		if (ClickedComponent != NULL)
		{
			TSharedPtr<FComponentVisualizer> Visualizer = GUnrealEd->FindComponentVisualizer(ClickedComponent->GetClass());
			if (Visualizer.IsValid())
			{
				FTypedElementHandle SelectedComponentHandle = VisProxy->GetElementHandle();
				UTypedElementSelectionSet* ElementSelectionSet = (InViewportClient && InViewportClient->GetModeTools()) ? InViewportClient->GetModeTools()->GetEditorSelectionSet() : nullptr;

				bool bIsActive = Visualizer->VisProxyHandleClick(InViewportClient, VisProxy, Click);
				if (bIsActive)
				{
					// Set the selection set to the spline component, so clicking the component doesn't reselect and clear the current editing state.
					if (ComponentVisualizers::GAutoSelectComponentWithPointSelection && ElementSelectionSet)
					{
						if (!ElementSelectionSet->IsElementSelected(SelectedComponentHandle, FTypedElementIsSelectedOptions()))
						{
							TArray<FTypedElementHandle> TmpArray = { SelectedComponentHandle };
							ElementSelectionSet->SetSelection(TmpArray, FTypedElementSelectionOptions());
						}
					}

					SetActiveComponentVis(InViewportClient, Visualizer);

					if (ComponentVisualizers::GAutoSelectComponentWithPointSelection && ElementSelectionSet)
					{
						ElementSelectionSet->NotifyPendingChanges();
					}

					return true;
				}
			}
		}
	}

	// DO NOT call ClearActiveComponentVis() here. If a new actor is being selected, ClearActiveComponentVis() 
	// will eventually be called by UUnrealEdEngine::NoteSelectionChange().  If it were called here, 
	// it would be prior to the selection transaction and thus the previous state of the component visualizer 
	// would not be captured for undo/redo.

	return false;
}

TSharedPtr<FComponentVisualizer> FComponentVisualizerManager::GetActiveComponentVis()
{
	return EditedVisualizerPtr.Pin();
}

bool FComponentVisualizerManager::SetActiveComponentVis(FEditorViewportClient* InViewportClient, TSharedPtr<FComponentVisualizer>& InVisualizer)
{
	if (InViewportClient && InVisualizer.IsValid())
	{
		// call EndEditing on any currently edited visualizer, if we are going to change it
		TSharedPtr<FComponentVisualizer> EditedVisualizer = EditedVisualizerPtr.Pin();
		if (EditedVisualizer.IsValid() && InVisualizer.Get() != EditedVisualizer.Get())
		{
			EditedVisualizer->EndEditing();
		}

		EditedVisualizerPtr = InVisualizer;
		EditedVisualizerViewportClient = InViewportClient;
		return true;
	}

	return false;
}

void FComponentVisualizerManager::ClearActiveComponentVis()
{
	TSharedPtr<FComponentVisualizer> EditedVisualizer = EditedVisualizerPtr.Pin();

	if (EditedVisualizer.IsValid())
	{
		EditedVisualizer->EndEditing();
	}

	EditedVisualizerPtr.Reset();
	EditedVisualizerViewportClient = nullptr;
}

bool FComponentVisualizerManager::HandleInputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event) const
{
	TSharedPtr<FComponentVisualizer> EditedVisualizer = EditedVisualizerPtr.Pin();

	if (EditedVisualizer.IsValid())
	{
		return EditedVisualizer->HandleInputKey(ViewportClient, Viewport, Key, Event);
	}

	return false;
}

bool FComponentVisualizerManager::HandleInputDelta(FEditorViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale) const
{
	TSharedPtr<FComponentVisualizer> EditedVisualizer = EditedVisualizerPtr.Pin();

	if (EditedVisualizer.IsValid() && InViewportClient && InViewportClient->GetCurrentWidgetAxis() != EAxisList::None)
	{
		return EditedVisualizer->HandleInputDelta(InViewportClient, InViewport, InDrag, InRot, InScale);
	}

	return false;
}

bool FComponentVisualizerManager::HandleFrustumSelect(const FConvexVolume& InFrustum, FEditorViewportClient* InViewportClient, FViewport* InViewport) const
{
	TSharedPtr<FComponentVisualizer> EditedVisualizer = EditedVisualizerPtr.Pin();

	if (EditedVisualizer.IsValid())
	{
		return EditedVisualizer->HandleFrustumSelect(InFrustum, InViewportClient, InViewport);
	}

	return false;
}

bool FComponentVisualizerManager::HandleBoxSelect(const FBox& InBox, FEditorViewportClient* InViewportClient, FViewport* InViewport) const
{
	TSharedPtr<FComponentVisualizer> EditedVisualizer = EditedVisualizerPtr.Pin();

	if (EditedVisualizer.IsValid())
	{
		return EditedVisualizer->HandleBoxSelect(InBox, InViewportClient, InViewport);
	}

	return false;
}

bool FComponentVisualizerManager::HasFocusOnSelectionBoundingBox(FBox& OutBoundingBox) const
{
	TSharedPtr<FComponentVisualizer> EditedVisualizer = EditedVisualizerPtr.Pin();

	if (EditedVisualizer.IsValid())
	{
		return EditedVisualizer->HasFocusOnSelectionBoundingBox(OutBoundingBox);
	}

	return false;
}

bool FComponentVisualizerManager::HandleSnapTo(const bool bInAlign, const bool bInUseLineTrace, const bool bInUseBounds, const bool bInUsePivot, AActor* InDestination)
{
	TSharedPtr<FComponentVisualizer> EditedVisualizer = EditedVisualizerPtr.Pin();

	if (EditedVisualizer.IsValid())
	{
		return EditedVisualizer->HandleSnapTo(bInAlign, bInUseLineTrace, bInUseBounds, bInUsePivot, InDestination);
	}

	return false;
}

bool FComponentVisualizerManager::GetWidgetLocation(const FEditorViewportClient* ViewportClient, FVector& OutLocation) const
{
	TSharedPtr<FComponentVisualizer> EditedVisualizer = EditedVisualizerPtr.Pin();

	if (EditedVisualizer.IsValid())
	{
		return EditedVisualizer->GetWidgetLocation(ViewportClient, OutLocation);
	}

	return false;
}


bool FComponentVisualizerManager::GetCustomInputCoordinateSystem(const FEditorViewportClient* ViewportClient, FMatrix& OutMatrix) const
{
	TSharedPtr<FComponentVisualizer> EditedVisualizer = EditedVisualizerPtr.Pin();

	if (EditedVisualizer.IsValid())
	{
		return EditedVisualizer->GetCustomInputCoordinateSystem(ViewportClient, OutMatrix);
	}

	return false;
}

void FComponentVisualizerManager::TrackingStarted(FEditorViewportClient* InViewportClient)
{
	TSharedPtr<FComponentVisualizer> EditedVisualizer = EditedVisualizerPtr.Pin();

	if (EditedVisualizer.IsValid())
	{
		return EditedVisualizer->TrackingStarted(InViewportClient);
	}
}

void FComponentVisualizerManager::TrackingStopped(FEditorViewportClient* InViewportClient, bool bInDidMove)
{
	TSharedPtr<FComponentVisualizer> EditedVisualizer = EditedVisualizerPtr.Pin();

	if (EditedVisualizer.IsValid() && EditedVisualizer->GetEditedComponent() != nullptr)
	{
		return EditedVisualizer->TrackingStopped(InViewportClient, bInDidMove);
	}
}

TSharedPtr<SWidget> FComponentVisualizerManager::GenerateContextMenuForComponentVis() const
{
	TSharedPtr<FComponentVisualizer> EditedVisualizer = EditedVisualizerPtr.Pin();

	if (EditedVisualizer.IsValid())
	{
		return EditedVisualizer->GenerateContextMenu();
	}

	return TSharedPtr<SWidget>();
}


bool FComponentVisualizerManager::IsActive() const
{
	TSharedPtr<FComponentVisualizer> EditedVisualizer = EditedVisualizerPtr.Pin();
	return EditedVisualizer.IsValid();
}


bool FComponentVisualizerManager::IsVisualizingArchetype() const
{
	TSharedPtr<FComponentVisualizer> EditedVisualizer = EditedVisualizerPtr.Pin();
	return EditedVisualizer.IsValid() && EditedVisualizer->IsVisualizingArchetype();
}
