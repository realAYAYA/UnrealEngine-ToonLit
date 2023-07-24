// Copyright Epic Games, Inc. All Rights Reserved.

#include "SmartObjectAssetEditorViewportClient.h"
#include "Components/StaticMeshComponent.h"
#include "SmartObjectAssetToolkit.h"
#include "SmartObjectAssetEditorSettings.h"
#include "SmartObjectComponentVisualizer.h"
#include "ScopedTransaction.h"
#include "SmartObjectDefinition.h"
#include "SmartObjectAnnotation.h"


#define LOCTEXT_NAMESPACE "SmartObjectAssetToolkit"

FSmartObjectAssetEditorViewportClient::FSmartObjectAssetEditorViewportClient(const TSharedRef<const FSmartObjectAssetToolkit>& InAssetEditorToolkit, FPreviewScene* InPreviewScene, const TWeakPtr<SEditorViewport>& InEditorViewportWidget)
	: FEditorViewportClient(&InAssetEditorToolkit->GetEditorModeManager(), InPreviewScene, InEditorViewportWidget)
	, AssetEditorToolkit(InAssetEditorToolkit)
{
	EngineShowFlags.DisableAdvancedFeatures();
	bUsingOrbitCamera = true;

	// Set if the grid will be drawn
	DrawHelper.bDrawGrid = GetDefault<USmartObjectAssetEditorSettings>()->bShowGridByDefault;
}

FSmartObjectAssetEditorViewportClient::~FSmartObjectAssetEditorViewportClient()
{
	if (ScopedTransaction != nullptr)
	{
		delete ScopedTransaction;
		ScopedTransaction = nullptr;
	}
}

void FSmartObjectAssetEditorViewportClient::Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	FEditorViewportClient::Draw(View, PDI);

	if (View != nullptr && PDI != nullptr)
	{
		// Draw slots and annotations.
		if (const USmartObjectDefinition* Definition = SmartObjectDefinition.Get())
		{
			UE::SmartObjects::Editor::Draw(*Definition, Selection, FTransform::Identity, *View, *PDI);
		}

		// Draw the object origin.
		DrawCoordinateSystem(PDI, FVector::ZeroVector, FRotator::ZeroRotator, 20.f, SDPG_World, 1.f);
	}
}


void FSmartObjectAssetEditorViewportClient::DrawCanvas(FViewport& InViewport, FSceneView& View, FCanvas& Canvas)
{
	FEditorViewportClient::DrawCanvas(InViewport, View, Canvas);

	// Draw slots and annotations.
	if (const USmartObjectDefinition* Definition = SmartObjectDefinition.Get())
	{
		UE::SmartObjects::Editor::DrawCanvas(*Definition, Selection, FTransform::Identity, View, Canvas);
	}
}

void FSmartObjectAssetEditorViewportClient::ProcessClick(FSceneView& View, HHitProxy* HitProxy, FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY)
{
	FEditorViewportClient::ProcessClick(View, HitProxy, Key, Event, HitX, HitY);

	if (Key == EKeys::LeftMouseButton)
	{
		bool bClickHandled = false;
		if (HitProxy)
		{
			const FViewportClick Click(&View, this, Key, Event, HitX, HitY);

			if (HitProxy->IsA(HSmartObjectSlotProxy::StaticGetType()))
			{
				const HSmartObjectSlotProxy* SlotProxy = static_cast<HSmartObjectSlotProxy*>(HitProxy);
				const UE::SmartObjects::Editor::FSelectedItem HitItem(SlotProxy->SlotID, SlotProxy->AnnotationIndex);

				if (IsCtrlPressed())
				{
					// Toggle selection
					if (Selection.Contains(HitItem))
					{
						Selection.Remove(HitItem);
					}
					else
					{
						Selection.AddUnique(HitItem);
					}
				}
				else
				{
					// Set selection
					Selection.Reset();
					Selection.Add(HitItem);
				}

				bClickHandled = true;
			}
		}

		if (!bClickHandled)
		{
			Selection.Reset();
		}
	}
}

FVector FSmartObjectAssetEditorViewportClient::GetWidgetLocation() const
{
	if (bIsManipulating)
	{
		// Return cached location during manipulation to avoid feedback effects.
		return CachedWidgetLocation;
	}
	
	FVector Result = FVector::ZeroVector; 
	
	const USmartObjectDefinition* Definition = SmartObjectDefinition.Get();
	if (Definition != nullptr)
	{
		int32 NumSlots = 0;
		FVector AccumulatedSlotLocation = FVector::ZeroVector;
		const FTransform OwnerLocalToWorld = FTransform::Identity;
		const TConstArrayView<FSmartObjectSlotDefinition> Slots = Definition->GetSlots();
		
		for (int32 Index = 0; Index < Slots.Num(); ++Index)
		{
			TOptional<FTransform> SlotTransform = Definition->GetSlotTransform(OwnerLocalToWorld, FSmartObjectSlotIndex(Index));
			if (!SlotTransform.IsSet())
			{
				continue;
			}

			const FSmartObjectSlotDefinition& Slot = Slots[Index];

			if (Selection.Contains(UE::SmartObjects::Editor::FSelectedItem(Slot.ID)))
			{
				AccumulatedSlotLocation += SlotTransform->GetLocation();
				NumSlots++;
			}

			for (int32 AnnotationIndex = 0; AnnotationIndex < Slot.Data.Num(); AnnotationIndex++)
			{
				const FInstancedStruct& Data = Slot.Data[AnnotationIndex];
				if (const FSmartObjectSlotAnnotation* Annotation = Data.GetPtr<FSmartObjectSlotAnnotation>())
				{
					if (Selection.Contains(UE::SmartObjects::Editor::FSelectedItem(Slot.ID, AnnotationIndex)))
					{
						const TOptional<FTransform> AnnotationTransform = Annotation->GetWorldTransform(*SlotTransform);
						if (AnnotationTransform.IsSet())
						{
							AccumulatedSlotLocation += AnnotationTransform->GetLocation();
							NumSlots++;
						}
					}
				}
			}
				
		}

		if (NumSlots > 0)
		{
			Result = AccumulatedSlotLocation / NumSlots;
		}
	}

	CachedWidgetLocation = Result;
	
	return Result;
}

FMatrix FSmartObjectAssetEditorViewportClient::GetWidgetCoordSystem() const
{
	return FMatrix::Identity;
}

ECoordSystem FSmartObjectAssetEditorViewportClient::GetWidgetCoordSystemSpace() const
{
	return WidgetCoordSystemSpace;
}

UE::Widget::EWidgetMode FSmartObjectAssetEditorViewportClient::GetWidgetMode() const
{
	bool bIsWidgetValid = false;

	const USmartObjectDefinition* Definition = SmartObjectDefinition.Get();
	if (Definition != nullptr)
	{
		const FTransform OwnerLocalToWorld = FTransform::Identity;
		const TConstArrayView<FSmartObjectSlotDefinition> Slots = Definition->GetSlots();

		for (int32 Index = 0; Index < Slots.Num() && !bIsWidgetValid; ++Index)
		{
			const FSmartObjectSlotDefinition& Slot = Slots[Index];

			TOptional<FTransform> SlotTransform = Definition->GetSlotTransform(OwnerLocalToWorld, FSmartObjectSlotIndex(Index));
			if (SlotTransform.IsSet())
			{
				if (Selection.Contains(UE::SmartObjects::Editor::FSelectedItem(Slot.ID)))
				{
					bIsWidgetValid = true;
					break;
				}
			}

			for (int32 AnnotationIndex = 0; AnnotationIndex < Slot.Data.Num(); AnnotationIndex++)
			{
				const FInstancedStruct& Data = Slot.Data[AnnotationIndex];
				if (const FSmartObjectSlotAnnotation* Annotation = Data.GetPtr<FSmartObjectSlotAnnotation>())
				{
					if (Selection.Contains(UE::SmartObjects::Editor::FSelectedItem(Slot.ID, AnnotationIndex)))
					{
						const TOptional<FTransform> AnnotationTransform = Annotation->GetWorldTransform(*SlotTransform);
						if (AnnotationTransform.IsSet())
						{
							bIsWidgetValid = true;
							break;
						}
					}
				}
			}
		}
	}
	
	return bIsWidgetValid ? WidgetMode : UE::Widget::EWidgetMode::WM_None;
}

bool FSmartObjectAssetEditorViewportClient::CanSetWidgetMode(UE::Widget::EWidgetMode NewMode) const
{
	return	NewMode == UE::Widget::EWidgetMode::WM_Translate
			|| NewMode == UE::Widget::EWidgetMode::WM_TranslateRotateZ
			|| NewMode == UE::Widget::EWidgetMode::WM_Rotate;
}

void FSmartObjectAssetEditorViewportClient::SetWidgetMode(UE::Widget::EWidgetMode NewMode)
{
	FEditorViewportClient::SetWidgetMode(NewMode);
	WidgetMode = NewMode;
}

void FSmartObjectAssetEditorViewportClient::SetWidgetCoordSystemSpace(ECoordSystem NewCoordSystem)
{
	// Empty, we support only world for the time being.
}

void FSmartObjectAssetEditorViewportClient::BeginTransaction(FText Text)
{
	if (ScopedTransaction)
	{
		ScopedTransaction->Cancel();
		delete ScopedTransaction;
		ScopedTransaction = nullptr;
	}

	ScopedTransaction = new FScopedTransaction(Text);
	check(ScopedTransaction);
}

void FSmartObjectAssetEditorViewportClient::EndTransaction()
{
	if (ScopedTransaction)
	{
		delete ScopedTransaction;
		ScopedTransaction = nullptr;
	}
}

void FSmartObjectAssetEditorViewportClient::TrackingStarted(const struct FInputEventState& InInputState, bool bIsDraggingWidget, bool bNudge)
{
	if (!bIsManipulating && bIsDraggingWidget)
	{
		bIsManipulating = true;

		// Begin transaction
		BeginTransaction(LOCTEXT("ModifySlots", "Modify Slots(s)"));
		bIsManipulating = true;

	}
}

void FSmartObjectAssetEditorViewportClient::TrackingStopped()
{
	if (bIsManipulating)
	{
		// End transaction
		bIsManipulating = false;
		EndTransaction();
	}
}

bool FSmartObjectAssetEditorViewportClient::InputWidgetDelta(FViewport* InViewport, EAxisList::Type CurrentAxis, FVector& Drag, FRotator& Rot, FVector& Scale)
{
	USmartObjectDefinition* Definition = SmartObjectDefinition.Get();
	if (Definition == nullptr)
	{
		return false;
	}

	Definition->SetFlags(RF_Transactional);
	Definition->Modify();

	bool bResult = false;

	// Update the cached location so that the widget moves during drag.
	CachedWidgetLocation += Drag;
	
	if (bIsManipulating && CurrentAxis != EAxisList::None)
	{
		const FTransform OwnerLocalToWorld = FTransform::Identity;
		const TArrayView<FSmartObjectSlotDefinition> Slots = Definition->GetMutableSlots();
		
		for (int32 Index = 0; Index < Definition->GetSlots().Num(); ++Index)
		{
			FSmartObjectSlotDefinition& Slot = Slots[Index];

			TOptional<FTransform> SlotTransform = Definition->GetSlotTransform(OwnerLocalToWorld, FSmartObjectSlotIndex(Index));
			if (SlotTransform.IsSet())
			{
				// Do not move annotations if slot is selected, or else, the annotation will get the adjustment in double (assumes annotations are generally relative to slot).
				if (Selection.Contains(UE::SmartObjects::Editor::FSelectedItem(Slot.ID)))
				{
					FVector SlotDrag = Drag;
					if (!Rot.IsZero())
					{
						// Rotate around gizmo pivot
						const FVector SlotOffset = SlotTransform->GetTranslation() - CachedWidgetLocation;
						if (!SlotOffset.IsNearlyZero())
						{
							const FVector RotatedSlotOffset = Rot.RotateVector(SlotOffset);
							SlotDrag += RotatedSlotOffset - SlotOffset;
						}
					}

					check(OwnerLocalToWorld.EqualsNoScale(FTransform::Identity));
					if (!SlotDrag.IsZero())
					{
						Slot.Offset += SlotDrag;
					}

					if (!Rot.IsZero())
					{
						Slot.Rotation += Rot;
						Slot.Rotation.Normalize();
					}
				}
				else
				{
					for (int32 AnnotationIndex = 0; AnnotationIndex < Slot.Data.Num(); AnnotationIndex++)
					{
						FInstancedStruct& Data = Slot.Data[AnnotationIndex];
						if (FSmartObjectSlotAnnotation* Annotation = Data.GetMutablePtr<FSmartObjectSlotAnnotation>())
						{
							if (Selection.Contains(UE::SmartObjects::Editor::FSelectedItem(Slot.ID, AnnotationIndex)))
							{
								const TOptional<FTransform> AnnotationTransform = Annotation->GetWorldTransform(*SlotTransform);
								if (AnnotationTransform.IsSet())
								{
									FVector AnnotationDrag = Drag;
									if (!Rot.IsZero())
									{
										// Rotate around gizmo pivot
										const FVector AnnotationOffset = AnnotationTransform->GetTranslation() - CachedWidgetLocation;
										if (!AnnotationOffset.IsNearlyZero())
										{
											const FVector RotatedSlotOffset = Rot.RotateVector(AnnotationOffset);
											AnnotationDrag += RotatedSlotOffset - AnnotationOffset;
										}
									}

									Annotation->AdjustWorldTransform(*SlotTransform, AnnotationDrag, Rot);
								}
							}
						}
					}
				}
			}

			bResult = true;
		}
	}
	
	return bResult;
}

void FSmartObjectAssetEditorViewportClient::SetSmartObjectDefinition(USmartObjectDefinition& InDefinition)
{
	SmartObjectDefinition = &InDefinition;
	if (Viewport)
	{
		FocusViewportOnBox(GetPreviewBounds());
	}
}

void FSmartObjectAssetEditorViewportClient::SetPreviewMesh(UStaticMesh* InStaticMesh)
{
	if (PreviewMeshComponent == nullptr)
	{
		PreviewMeshComponent = NewObject<UStaticMeshComponent>();
		ON_SCOPE_EXIT { PreviewScene->AddComponent(PreviewMeshComponent.Get(),FTransform::Identity); };
	}

	PreviewMeshComponent->SetStaticMesh(InStaticMesh);
	FocusViewportOnBox(GetPreviewBounds());
}

void FSmartObjectAssetEditorViewportClient::SetPreviewActor(AActor* InActor)
{
	if (AActor* Actor = PreviewActor.Get())
	{
		PreviewScene->GetWorld()->DestroyActor(Actor);
		PreviewActor.Reset();
	}

	if (InActor != nullptr)
	{
		PreviewActor = PreviewScene->GetWorld()->SpawnActor(InActor->GetClass());
	}

	FocusViewportOnBox(GetPreviewBounds());
}

void FSmartObjectAssetEditorViewportClient::SetPreviewActorClass(const UClass* ActorClass)
{
	if (AActor* Actor = PreviewActorFromClass.Get())
	{
		PreviewScene->GetWorld()->DestroyActor(Actor);
		PreviewActorFromClass.Reset();
	}

	if (ActorClass != nullptr)
	{
		PreviewActorFromClass = PreviewScene->GetWorld()->SpawnActor(const_cast<UClass*>(ActorClass));
	}

	FocusViewportOnBox(GetPreviewBounds());
}

FBox FSmartObjectAssetEditorViewportClient::GetPreviewBounds() const
{
	FBoxSphereBounds Bounds(FSphere(FVector::ZeroVector, 100.f));
	if (const AActor* Actor = PreviewActor.Get())
	{
		Bounds = Bounds+ Actor->GetComponentsBoundingBox();
	}

	if (const AActor* Actor = PreviewActorFromClass.Get())
	{
		Bounds = Bounds+ Actor->GetComponentsBoundingBox();
	}

	if (const UStaticMeshComponent* Component = PreviewMeshComponent.Get())
	{
		Bounds = Bounds + Component->CalcBounds(FTransform::Identity);
	}

	const TSharedRef<const FSmartObjectAssetToolkit> Toolkit = AssetEditorToolkit.Pin().ToSharedRef();
	const TArray< UObject* >* EditedObjects = Toolkit->GetObjectsCurrentlyBeingEdited();
	if (EditedObjects != nullptr)
	{
		for (const UObject* EditedObject : *EditedObjects)
		{
			const USmartObjectDefinition* Definition = Cast<USmartObjectDefinition>(EditedObject);
			if (IsValid(Definition))
			{
				Bounds = Bounds + Definition->GetBounds();
			}
		}
	}

	return Bounds.GetBox();
}

#undef LOCTEXT_NAMESPACE
