// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/PlacementPlaceSingleTool.h"

#include "AssetPlacementSettings.h"
#include "Editor.h"
#include "InteractiveToolManager.h"
#include "ScopedTransaction.h"
#include "ToolDataVisualizer.h"
#include "BaseBehaviors/ClickDragBehavior.h"
#include "BaseGizmos/BrushStampIndicator.h"
#include "BaseGizmos/GizmoRenderingUtil.h"
#include "Editor/EditorEngine.h"
#include "Elements/Framework/TypedElementHandle.h"
#include "Elements/Framework/TypedElementRegistry.h"
#include "Elements/Framework/TypedElementSelectionSet.h"
#include "Elements/Interfaces/TypedElementWorldInterface.h"
#include "Elements/SMInstance/SMInstanceElementData.h" // For SMInstanceElementDataUtil::SMInstanceElementsEnabled
#include "Modes/PlacementModeSubsystem.h"
#include "Subsystems/PlacementSubsystem.h"
#include "UObject/Object.h"
#include "Tools/AssetEditorContextInterface.h"
#include "EditorModeManager.h"
#include "Toolkits/IToolkitHost.h"
#include "ContextObjectStore.h"
#include "PlacementPaletteItem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PlacementPlaceSingleTool)

#if !UE_IS_COOKED_EDITOR
#include "AssetPlacementEdModeModule.h"
#endif

constexpr TCHAR UPlacementModePlaceSingleTool::ToolName[];

UPlacementBrushToolBase* UPlacementModePlaceSingleToolBuilder::FactoryToolInstance(UObject* Outer) const
{
	return NewObject<UPlacementModePlaceSingleTool>(Outer);
}

UPlacementModePlaceSingleTool::UPlacementModePlaceSingleTool() = default;
UPlacementModePlaceSingleTool::~UPlacementModePlaceSingleTool() = default;
UPlacementModePlaceSingleTool::UPlacementModePlaceSingleTool(FVTableHelper& Helper)
	: Super(Helper)
{
}

void UPlacementModePlaceSingleTool::Setup()
{
	Super::Setup();

	SetupRightClickMouseBehavior();

	bIsTweaking = false;

	SinglePlaceSettings = NewObject<UPlacementModePlaceSingleToolSettings>(this);
	SinglePlaceSettings->LoadConfig();
	AddToolPropertySource(SinglePlaceSettings);
}

void UPlacementModePlaceSingleTool::Shutdown(EToolShutdownType ShutdownType)
{
	DestroyPreviewElements();

	// Preserve the selection on exiting the tool, so that a users' state persists as they use the mode.
	constexpr bool bClearSelectionSet = false;
	ExitTweakState(bClearSelectionSet);

	Super::Shutdown(ShutdownType);

	SinglePlaceSettings->SaveConfig();
	SinglePlaceSettings = nullptr;
}

void UPlacementModePlaceSingleTool::OnClickPress(const FInputDeviceRay& PressPos)
{
	Super::OnClickPress(PressPos);

	if (!PlacementInfo)
	{
		return;
	}

	DestroyPreviewElements();
	LastBrushStamp.Radius = 100.0f * LastBrushStampWorldToPixelScale;

	UPlacementModeSubsystem* PlacementModeSubsystem = GEditor->GetEditorSubsystem<UPlacementModeSubsystem>();
	const UAssetPlacementSettings* PlacementSettings = PlacementModeSubsystem ? PlacementModeSubsystem->GetModeSettingsObject() : nullptr;
	UPlacementSubsystem* PlacementSubsystem = GEditor->GetEditorSubsystem<UPlacementSubsystem>();
	if (PlacementSubsystem && PlacementSettings)
	{
		// Place the Preview data if we managed to get to a valid handled click.
		FPlacementOptions PlacementOptions;
		PlacementOptions.bPreferBatchPlacement = true;
		
#if !UE_IS_COOKED_EDITOR
		if (AssetPlacementEdModeUtil::AreInstanceWorkflowsEnabled())
		{
			PlacementOptions.InstancedPlacementGridGuid = PlacementSettings->GetActivePaletteGuid();
		}
		else
#endif
		{
			PlacementOptions.InstancedPlacementGridGuid = FGuid();
		}

		FAssetPlacementInfo FinalizedPlacementInfo = *PlacementInfo;
		FinalizedPlacementInfo.FinalizedTransform = FinalizeTransform(
			FTransform(PlacementInfo->FinalizedTransform.GetRotation(), LastBrushStamp.WorldPosition, PlacementInfo->FinalizedTransform.GetScale3D()),
			LastBrushStamp.WorldNormal,
			GEditor->GetEditorSubsystem<UPlacementModeSubsystem>()->GetModeSettingsObject());
		FinalizedPlacementInfo.PreferredLevel = GEditor->GetEditorWorldContext().World()->GetCurrentLevel();

		GetToolManager()->BeginUndoTransaction(NSLOCTEXT("PlacementMode", "SinglePlaceAsset", "Place Single Asset"));
		PlacedElements = PlacementSubsystem->PlaceAsset(FinalizedPlacementInfo, PlacementOptions);
		NotifyMovementStarted(PlacedElements);
	}
}

void UPlacementModePlaceSingleTool::OnClickDrag(const FInputDeviceRay& DragPos)
{
	FVector TraceStartLocation = DragPos.WorldRay.Origin;
	FVector TraceDirection = DragPos.WorldRay.Direction;
	FVector TraceEndLocation = TraceStartLocation + (TraceDirection * HALF_WORLD_MAX);
	FVector TraceIntersectionXY = FMath::LinePlaneIntersection(TraceStartLocation, TraceEndLocation, FPlane(LastBrushStamp.WorldPosition, LastBrushStamp.WorldNormal));

	FVector CursorDirection;
	float CursorDistance;
	FVector MouseDelta = LastBrushStamp.WorldPosition - TraceIntersectionXY;
	MouseDelta.ToDirectionAndLength(CursorDirection, CursorDistance);
	if (CursorDirection.IsNearlyZero())
	{
		return;
	}

	const UAssetPlacementSettings* PlacementSettings = GEditor->GetEditorSubsystem<UPlacementModeSubsystem>()->GetModeSettingsObject();

	// Update rotation based on mouse position
	FTransform UpdatedTransform = FinalizeTransform(
		FTransform(FRotationMatrix::MakeFromXZ(CursorDirection, PlacementInfo->FinalizedTransform.GetRotation().GetUpVector()).ToQuat(), LastBrushStamp.WorldPosition, PlacementInfo->FinalizedTransform.GetScale3D()),
		LastBrushStamp.WorldNormal,
		PlacementSettings);

	// Update scale based on mouse position
	FVector UpdatedScale = UpdatedTransform.GetScale3D();
	if (PlacementSettings && (SinglePlaceSettings->ScalingType != EPlacementScaleToCursorType::None))
	{
		auto UpdateComponent = [PlacementSettings, CursorDistance, this](float InComponent) -> float
		{
			float Sign = 1.0f;
			if (InComponent < 0.0f)
			{
				Sign = -1.0f;
			}

			FFloatInterval ScaleRange(FMath::Abs(InComponent), PlacementSettings->ScaleRange.Max);
			float NewComponent = ScaleRange.Interpolate(FMath::Min(1.0f, (CursorDistance / BrushStampIndicator->BrushRadius)));
			return NewComponent * Sign;
		};

		switch (PlacementSettings->ScalingType)
		{
			case EFoliageScaling::LockXY:
			{
				UpdatedScale.Z = UpdateComponent(UpdatedScale.Z);
				break;
			}
			case EFoliageScaling::LockYZ:
			{
				UpdatedScale.X = UpdateComponent(UpdatedScale.X);
				break;
			}
			case EFoliageScaling::LockXZ:
			{
				UpdatedScale.Y = UpdateComponent(UpdatedScale.Y);
				break;
			}
			default:
			{
				UpdatedScale.X = UpdateComponent(UpdatedScale.X);
				UpdatedScale.Y = UpdateComponent(UpdatedScale.Y);
				UpdatedScale.Z = UpdateComponent(UpdatedScale.Z);
				break;
			}
		}
	}
	UpdatedTransform.SetScale3D(UpdatedScale);

	// Use the drag position and settings to update the scale and rotation of the placed elements.
	UpdateElementTransforms(PlacedElements, UpdatedTransform, false);
}

void UPlacementModePlaceSingleTool::OnClickRelease(const FInputDeviceRay& ReleasePos)
{
	Super::OnClickRelease(ReleasePos);

	NotifyMovementEnded(PlacedElements);
	EnterTweakState(PlacedElements);
	GetToolManager()->EndUndoTransaction();

	ShutdownBrushStampIndicator();
	PlacementInfo.Reset();
	PlacedElements.Empty();
}

void UPlacementModePlaceSingleTool::OnBeginHover(const FInputDeviceRay& DevicePos)
{
	Super::OnBeginHover(DevicePos);

	// Always regenerate the placement data when a hover sequence begins
	PlacementInfo.Reset();
	CreatePreviewElements(DevicePos);
}

bool UPlacementModePlaceSingleTool::OnUpdateHover(const FInputDeviceRay& DevicePos)
{
	if (!Super::OnUpdateHover(DevicePos))
	{
		return false;
	}

	// Update the preview elements
	UpdatePreviewElements(DevicePos);
	return true;
}

void UPlacementModePlaceSingleTool::OnEndHover()
{
	Super::OnEndHover();

	// Destroy the preview elements we created.
	DestroyPreviewElements();
}

FInputRayHit UPlacementModePlaceSingleTool::CanBeginClickDragSequence(const FInputDeviceRay& PressPos)
{
	if (!bIsTweaking)
	{
		return Super::CanBeginClickDragSequence(PressPos);
	}
	
	return FInputRayHit();
}

FInputRayHit UPlacementModePlaceSingleTool::BeginHoverSequenceHitTest(const FInputDeviceRay& DevicePos)
{
	if (!bIsTweaking)
	{
		return Super::BeginHoverSequenceHitTest(DevicePos);
	}

	return FInputRayHit();
}

void UPlacementModePlaceSingleTool::GeneratePlacementData(const FInputDeviceRay& DevicePos)
{
	UPlacementModeSubsystem* PlacementSubsystem = GEditor->GetEditorSubsystem<UPlacementModeSubsystem>();
	const UAssetPlacementSettings* PlacementSettings = PlacementSubsystem ? PlacementSubsystem->GetModeSettingsObject() : nullptr;
	if (PlacementSettings)
	{
		TArrayView<const TObjectPtr<UPlacementPaletteClient>> CurrentPaletteItems = PlacementSettings->GetActivePaletteItems();
		if (CurrentPaletteItems.Num())
		{
			int32 ItemIndex = FMath::RandHelper(CurrentPaletteItems.Num());
			const UPlacementPaletteClient* ItemToPlace = CurrentPaletteItems[ItemIndex];
			if (ItemToPlace)
			{
				FTransform TransformToUpdate(GenerateRandomRotation(PlacementSettings), LastBrushStamp.WorldPosition, GenerateRandomScale(PlacementSettings));

				PlacementInfo = MakeUnique<FAssetPlacementInfo>();
				PlacementInfo->AssetToPlace = ItemToPlace->AssetPath.TryLoad();
				PlacementInfo->FactoryOverride = ItemToPlace->FactoryInterface;
				PlacementInfo->ItemGuid = ItemToPlace->ClientGuid;
				PlacementInfo->FinalizedTransform = FinalizeTransform(TransformToUpdate, LastBrushStamp.WorldNormal, PlacementSettings);
				PlacementInfo->PreferredLevel = GEditor->GetEditorWorldContext().World()->GetCurrentLevel();
				PlacementInfo->SettingsObject = ItemToPlace->SettingsObject;
			}
		}
	}
}

void UPlacementModePlaceSingleTool::CreatePreviewElements(const FInputDeviceRay& DevicePos)
{
	SetupBrushStampIndicator();

	// Place the preview elements from our stored info
	if (!PlacementInfo)
	{
		GeneratePlacementData(DevicePos);
	}

	if (!PlacementInfo)
	{
		return;
	}

	if (UPlacementSubsystem* PlacementSubsystem = GEditor->GetEditorSubsystem<UPlacementSubsystem>())
	{
		FPlacementOptions PlacementOptions;
		PlacementOptions.bIsCreatingPreviewElements = true;
		FAssetPlacementInfo InfoToPlace = *PlacementInfo;
		InfoToPlace.FinalizedTransform = FinalizeTransform(
			FTransform(PlacementInfo->FinalizedTransform.GetRotation(), LastBrushStamp.WorldPosition, PlacementInfo->FinalizedTransform.GetScale3D()),
			LastBrushStamp.WorldNormal,
			GEditor->GetEditorSubsystem<UPlacementModeSubsystem>()->GetModeSettingsObject());

		PreviewElements = PlacementSubsystem->PlaceAsset(InfoToPlace, PlacementOptions);
	}

	NotifyMovementStarted(PreviewElements);
}

void UPlacementModePlaceSingleTool::UpdatePreviewElements(const FInputDeviceRay& DevicePos)
{
	// If we should have preview elements, but do not currently, go ahead and create them.
	if ((PreviewElements.Num() == 0) && !BrushStampIndicator)
	{
		CreatePreviewElements(DevicePos);
	}

	// If we don't actually have any preview handles created, we don't need to update them, so go ahead and bail.
	if (PreviewElements.Num() == 0)
	{
		return;
	}

	// Update the brush radius to be stable screen size based on the world to pixel scale at the last render update.
	LastBrushStamp.Radius = 100.0f * LastBrushStampWorldToPixelScale;

	FTransform UpdatedTransform(PlacementInfo->FinalizedTransform.GetRotation(), LastBrushStamp.WorldPosition, PlacementInfo->FinalizedTransform.GetScale3D());
	UpdatedTransform = FinalizeTransform(UpdatedTransform, LastBrushStamp.WorldNormal, GEditor->GetEditorSubsystem<UPlacementModeSubsystem>()->GetModeSettingsObject());

	UpdateElementTransforms(PreviewElements, UpdatedTransform);
}

void UPlacementModePlaceSingleTool::DestroyPreviewElements()
{
	if (IAssetEditorContextInterface* AssetEditorContext = GetToolManager()->GetContextObjectStore()->FindContext<IAssetEditorContextInterface>())
	{
		if (UTypedElementSelectionSet* SelectionSet = AssetEditorContext->GetMutableSelectionSet())
		{
			for (const FTypedElementHandle& PreviewElement : PreviewElements)
			{
				if (TTypedElement<ITypedElementWorldInterface> WorldInterfaceElement = UTypedElementRegistry::GetInstance()->GetElement<ITypedElementWorldInterface>(PreviewElement))
				{
					WorldInterfaceElement.NotifyMovementEnded();
					WorldInterfaceElement.DeleteElement(WorldInterfaceElement.GetOwnerWorld(), SelectionSet, FTypedElementDeletionOptions());
				}
			}
		}
	}

	PreviewElements.Empty();
}

void UPlacementModePlaceSingleTool::EnterTweakState(TArrayView<const FTypedElementHandle> InElementHandles)
{
	if (InElementHandles.Num() == 0 || !SinglePlaceSettings->bSelectAfterPlacing)
	{
		return;
	}

	if (IAssetEditorContextInterface* AssetEditorContext = GetToolManager()->GetContextObjectStore()->FindContext<IAssetEditorContextInterface>())
	{
		if (UTypedElementSelectionSet* SelectionSet = AssetEditorContext->GetMutableSelectionSet())
		{
			SelectionSet->SetSelection(InElementHandles, FTypedElementSelectionOptions());
			bIsTweaking = true;
		}
	}
}

void UPlacementModePlaceSingleTool::ExitTweakState(bool bClearSelectionSet)
{
	if (bClearSelectionSet)
	{
		if (IAssetEditorContextInterface* AssetEditorContext = GetToolManager()->GetContextObjectStore()->FindContext<IAssetEditorContextInterface>())
		{
			if (UTypedElementSelectionSet* SelectionSet = AssetEditorContext->GetMutableSelectionSet())
			{
				SelectionSet->ClearSelection(FTypedElementSelectionOptions());
			}
		}
	}

	bIsTweaking = false;
}

void UPlacementModePlaceSingleTool::UpdateElementTransforms(TArrayView<const FTypedElementHandle> InElements, const FTransform& InTransform, bool bLocalTransform)
{
	// Update the transform positions for the preview elements.
	for (const FTypedElementHandle& ElementHandle : InElements)
	{
		if (TTypedElement<ITypedElementWorldInterface> WorldInterfaceElement = UTypedElementRegistry::GetInstance()->GetElement<ITypedElementWorldInterface>(ElementHandle))
		{
			bLocalTransform ? WorldInterfaceElement.SetRelativeTransform(InTransform) : WorldInterfaceElement.SetWorldTransform(InTransform);
			WorldInterfaceElement.NotifyMovementOngoing();
		}
	}
}

void UPlacementModePlaceSingleTool::NotifyMovementStarted(TArrayView<const FTypedElementHandle> InElements)
{
	// Notify Movement started
	for (const FTypedElementHandle& ElementToNotify : InElements)
	{
		if (TTypedElement<ITypedElementWorldInterface> WorldInterfaceElement = UTypedElementRegistry::GetInstance()->GetElement<ITypedElementWorldInterface>(ElementToNotify))
		{
			WorldInterfaceElement.NotifyMovementStarted();
		}
	}
}

void UPlacementModePlaceSingleTool::NotifyMovementEnded(TArrayView<const FTypedElementHandle> InElements)
{
	for (const FTypedElementHandle& ElementToNotify : InElements)
	{
		if (TTypedElement<ITypedElementWorldInterface> WorldInterfaceElement = UTypedElementRegistry::GetInstance()->GetElement<ITypedElementWorldInterface>(ElementToNotify))
		{
			WorldInterfaceElement.NotifyMovementEnded();
		}
	}
}

void UPlacementModePlaceSingleTool::SetupRightClickMouseBehavior()
{
	ULocalClickDragInputBehavior* RightMouseBehavior = NewObject<ULocalClickDragInputBehavior>(this);
	RightMouseBehavior->CanBeginClickDragFunc = [this](const FInputDeviceRay&) { return bShiftToggle ? FInputRayHit(1.0f) : FInputRayHit(); };
	RightMouseBehavior->OnClickReleaseFunc = [this](const FInputDeviceRay&)
	{
		constexpr bool bClearSelection = true;
		ExitTweakState(bClearSelection);
		PlacementInfo.Reset();
	};
	RightMouseBehavior->SetDefaultPriority(FInputCapturePriority(-1));
	RightMouseBehavior->SetUseRightMouseButton();
	RightMouseBehavior->Initialize();
	AddInputBehavior(RightMouseBehavior);
}

